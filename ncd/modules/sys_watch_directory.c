/**
 * @file sys_watch_directory.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * This file is part of BadVPN.
 * 
 * BadVPN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * BadVPN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * @section DESCRIPTION
 * 
 * Directory watcher.
 * 
 * Synopsis: sys.watch_directory(string dir)
 * Description: reports directory entry events. Transitions up when an event is detected, and
 *   goes down waiting for the next event when sys.watch_directory::nextevent() is called.
 * Variables:
 *   string event_type - what happened with the file: "added", "removed" or "changed"
 *   string filename - name of the file in the directory the event refers to
 *   string filepath - "dir/filename"
 * 
 * Synopsis: sys.watch_directory::nextevent()
 * Description: makes the watch_directory module transition down in order to report the next event.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>

#include <misc/nonblocking.h>
#include <misc/concat_strings.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_sys_watch_directory.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

#define MAX_EVENTS 128

struct instance {
    NCDModuleInst *i;
    char *dir;
    int inotify_fd;
    BFileDescriptor bfd;
    int processing;
    struct inotify_event events[MAX_EVENTS];
    int events_count;
    int events_index;
};

struct nextevent_instance {
    NCDModuleInst *i;
};

static void assert_event (struct instance *o)
{
    ASSERT(o->events_index < o->events_count)
    ASSERT(o->events[o->events_index].len % sizeof(o->events[0]) == 0)
    ASSERT(o->events[o->events_index].len / sizeof(o->events[0]) <= o->events_count - (o->events_index + 1))
}

static int check_event (struct instance *o)
{
    assert_event(o);
    
    return (
        strlen(o->events[o->events_index].name) > 0 &&
        (o->events[o->events_index].mask & (IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO))
    );
}

static void next_event (struct instance *o)
{
    assert_event(o);
    
    o->events_index += 1 + o->events[o->events_index].len / sizeof(o->events[0]);
}

static void skip_bad_events (struct instance *o)
{
    while (o->events_index < o->events_count && !check_event(o)) {
        ModuleLog(o->i, BLOG_ERROR, "unknown inotify event");
        next_event(o);
    }
}

static void inotify_fd_handler (struct instance *o, int events)
{
    ASSERT(!o->processing)
    
    int res = read(o->inotify_fd, o->events, sizeof(o->events));
    if (res < 0) {
        ModuleLog(o->i, BLOG_ERROR, "read failed");
        return;
    }
    
    ASSERT(res <= sizeof(o->events))
    ASSERT(res % sizeof(o->events[0]) == 0)
    
    // setup buffer state
    o->events_count = res / sizeof(o->events[0]);
    o->events_index = 0;
    
    // skip bad events
    skip_bad_events(o);
    
    if (o->events_index == o->events_count) {
        // keep reading
        return;
    }
    
    // stop reading
    BReactor_SetFileDescriptorEvents(o->i->reactor, &o->bfd, 0);
    
    // set processing
    o->processing = 1;
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
}

static void inotify_nextevent (struct instance *o)
{
    ASSERT(o->processing)
    assert_event(o);
    
    // update index, skip bad events
    next_event(o);
    skip_bad_events(o);
    
    if (o->events_index == o->events_count) {
        // start reading
        BReactor_SetFileDescriptorEvents(o->i->reactor, &o->bfd, BREACTOR_READ);
        
        // set not processing
        o->processing = 0;
        
        // signal down
        NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
        
        return;
    }
    
    // signal down and up to process the next event
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
}

static void func_new (NCDModuleInst *i)
{
    // allocate instance
    struct instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    NCDValue *dir_arg;
    if (!NCDValue_ListRead(o->i->args, 1, &dir_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (NCDValue_Type(dir_arg) != NCDVALUE_STRING) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    o->dir = NCDValue_StringValue(dir_arg);
    
    // open inotify
    if ((o->inotify_fd = inotify_init()) < 0) {
        ModuleLog(o->i, BLOG_ERROR, "inotify_init failed");
        goto fail1;
    }
    
    // add watch
    if (inotify_add_watch(o->inotify_fd, o->dir, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO) < 0) {
        ModuleLog(o->i, BLOG_ERROR, "inotify_add_watch failed");
        goto fail2;
    }
    
    // set non-blocking
    if (!badvpn_set_nonblocking(o->inotify_fd)) {
        ModuleLog(o->i, BLOG_ERROR, "badvpn_set_nonblocking failed");
        goto fail2;
    }
    
    // init BFileDescriptor
    BFileDescriptor_Init(&o->bfd, o->inotify_fd, (BFileDescriptor_handler)inotify_fd_handler, o);
    if (!BReactor_AddFileDescriptor(o->i->reactor, &o->bfd)) {
        ModuleLog(o->i, BLOG_ERROR, "BReactor_AddFileDescriptor failed");
        goto fail2;
    }
    BReactor_SetFileDescriptorEvents(o->i->reactor, &o->bfd, BREACTOR_READ);
    
    // set not processing
    o->processing = 0;
    
    return;
    
fail2:
    ASSERT_FORCE(close(o->inotify_fd) == 0)
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free BFileDescriptor
    BReactor_RemoveFileDescriptor(o->i->reactor, &o->bfd);
    
    // close inotify
    ASSERT_FORCE(close(o->inotify_fd) == 0)
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static int func_getvar (void *vo, const char *name, NCDValue *out)
{
    struct instance *o = vo;
    ASSERT(o->processing)
    assert_event(o);
    ASSERT(check_event(o))
    
    struct inotify_event *event = &o->events[o->events_index];
    
    if (!strcmp(name, "event_type")) {
        const char *str;
        
        if ((event->mask & (IN_CREATE | IN_MOVED_TO))) {
            str = "added";
        }
        else if ((event->mask & (IN_DELETE | IN_MOVED_FROM))) {
            str = "removed";
        }
        else if ((event->mask & IN_MODIFY)) {
            str = "changed";
        } else {
            ASSERT(0);
        }
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "filename")) {
        if (!NCDValue_InitString(out, event->name)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            return 0;
        }
        
        return 1;
    }
    
    if (!strcmp(name, "filepath")) {
        char *str = concat_strings(3, o->dir, "/", event->name);
        if (!str) {
            ModuleLog(o->i, BLOG_ERROR, "concat_strings failed");
            return 0;
        }
        
        if (!NCDValue_InitString(out, str)) {
            ModuleLog(o->i, BLOG_ERROR, "NCDValue_InitString failed");
            free(str);
            return 0;
        }
        
        free(str);
        
        return 1;
    }
    
    return 0;
}

static void nextevent_func_new (NCDModuleInst *i)
{
    // allocate instance
    struct nextevent_instance *o = malloc(sizeof(*o));
    if (!o) {
        ModuleLog(i, BLOG_ERROR, "failed to allocate instance");
        goto fail0;
    }
    NCDModuleInst_Backend_SetUser(i, o);
    
    // init arguments
    o->i = i;
    
    // check arguments
    if (!NCDValue_ListRead(o->i->args, 0)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    
    // get method object
    struct instance *mo = i->method_object->inst_user;
    ASSERT(mo->processing)
    
    // signal up.
    // Do it before finishing the event so our process does not advance any further if
    // we would be killed the event provider going down.
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
    
    // wait for next event
    inotify_nextevent(mo);
    
    return;
    
fail1:
    free(o);
fail0:
    NCDModuleInst_Backend_SetError(i);
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static void nextevent_func_die (void *vo)
{
    struct nextevent_instance *o = vo;
    NCDModuleInst *i = o->i;
    
    // free instance
    free(o);
    
    NCDModuleInst_Backend_Event(i, NCDMODULE_EVENT_DEAD);
}

static const struct NCDModule modules[] = {
    {
        .type = "sys.watch_directory",
        .func_new = func_new,
        .func_die = func_die,
        .func_getvar = func_getvar
    }, {
        .type = "sys.watch_directory::nextevent",
        .func_new = nextevent_func_new,
        .func_die = nextevent_func_die
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_sys_watch_directory = {
    .modules = modules
};
