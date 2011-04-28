/**
 * @file event_template.c
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
 */

#include <stdlib.h>

#include <misc/offset.h>
#include <misc/debug.h>
#include <misc/balloc.h>

#include <ncd/modules/event_template.h>

#define TemplateLog(o, ...) NCDModuleInst_Backend_Log((o)->i, (o)->blog_channel, __VA_ARGS__)

static void enable_event (event_template *o)
{
    ASSERT(!LinkedList1_IsEmpty(&o->events_list))
    ASSERT(!o->enabled)
    
    // get event
    struct event_template_event *e = UPPER_OBJECT(LinkedList1_GetFirst(&o->events_list), struct event_template_event, events_list_node);
    
    // remove from events list
    LinkedList1_Remove(&o->events_list, &e->events_list_node);
    
    // grab enabled map
    o->enabled_map = e->map;
    
    // append to free list
    LinkedList1_Append(&o->free_list, &e->events_list_node);
    
    // set enabled
    o->enabled = 1;
    
    // signal up
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_UP);
}

void event_template_new (event_template *o, NCDModuleInst *i, int blog_channel, int maxevents, void *user,
                         event_template_func_free func_free)
{
    ASSERT(maxevents > 0)
    
    // init arguments
    o->i = i;
    o->blog_channel = blog_channel;
    o->user = user;
    o->func_free = func_free;
    
    // allocate events array
    if (!(o->events = BAllocArray(maxevents, sizeof(o->events[0])))) {
        TemplateLog(o, BLOG_ERROR, "BAllocArray failed");
        goto fail0;
    }
    
    // init events lists
    LinkedList1_Init(&o->events_list);
    LinkedList1_Init(&o->free_list);
    for (int i = 0; i < maxevents; i++) {
        LinkedList1_Append(&o->free_list, &o->events[i].events_list_node);
    }
    
    // set not enabled
    o->enabled = 0;
    
    return;
    
fail0:
    NCDModuleInst_Backend_SetError(o->i);
    o->func_free(o->user);
    return;
}

void event_template_die (event_template *o)
{
    // free enabled map
    if (o->enabled) {
        BStringMap_Free(&o->enabled_map);
    }
    
    // free event maps
    LinkedList1Node *list_node;
    while (list_node = LinkedList1_GetFirst(&o->events_list)) {
        struct event_template_event *e = UPPER_OBJECT(list_node, struct event_template_event, events_list_node);
        LinkedList1_Remove(&o->events_list, &e->events_list_node);
        BStringMap_Free(&e->map);
    }
    
    // free events array
    BFree(o->events);
    
    o->func_free(o->user);
    return;
}

int event_template_getvar (event_template *o, const char *name, NCDValue *out)
{
    ASSERT(o->enabled)
    ASSERT(name)
    
    const char *val = BStringMap_Get(&o->enabled_map, name);
    if (!val) {
        return 0;
    }
    
    if (!NCDValue_InitString(out, val)) {
        TemplateLog(o, BLOG_ERROR, "NCDValue_InitString failed");
        return 0;
    }
    
    return 1;
}

void event_template_queue (event_template *o, BStringMap map, int *out_was_empty)
{
    ASSERT(!LinkedList1_IsEmpty(&o->free_list))
    
    // get event
    struct event_template_event *e = UPPER_OBJECT(LinkedList1_GetFirst(&o->free_list), struct event_template_event, events_list_node);
    
    // remove from free list
    LinkedList1_Remove(&o->free_list, &e->events_list_node);
    
    // set map
    e->map = map;
    
    // insert to events list
    LinkedList1_Append(&o->events_list, &e->events_list_node);
    
    // enable if not already
    if (!o->enabled) {
        enable_event(o);
        *out_was_empty = 1;
    } else {
        *out_was_empty = 0;
    }
}

void event_template_dequeue (event_template *o, int *out_is_empty)
{
    ASSERT(o->enabled)
    
    // free enabled map
    BStringMap_Free(&o->enabled_map);
    
    // set not enabled
    o->enabled = 0;
    
    // signal down
    NCDModuleInst_Backend_Event(o->i, NCDMODULE_EVENT_DOWN);
    
    if (!LinkedList1_IsEmpty(&o->events_list)) {
        // events are queued, enable
        enable_event(o);
        *out_is_empty = 0;
    } else {
        *out_is_empty = 1;
    }
}

void event_template_assert_enabled (event_template *o)
{
    ASSERT(o->enabled)
}
