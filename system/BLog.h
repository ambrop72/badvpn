/**
 * @file BLog.h
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
 * A global object for logging.
 */

#ifndef BADVPN_SYSTEM_BLOG_H
#define BADVPN_SYSTEM_BLOG_H

#include <stdarg.h>
#include <string.h>

#include <misc/debug.h>

// auto-generated channel numbers and number of channels
#include <generated/blog_channels_defines.h>

#define BLOG_ERROR 1
#define BLOG_WARNING 2
#define BLOG_NOTICE 3
#define BLOG_INFO 4
#define BLOG_DEBUG 5

#define BLog(...) BLog_LogToChannel(BLOG_CURRENT_CHANNEL, __VA_ARGS__)

typedef void (*_BLog_log_func) (int channel, int level, const char *msg);
typedef void (*_BLog_free_func) (void);

struct _BLog_channel {
    const char *name;
    int loglevel;
};

struct _BLog_global {
    #ifndef NDEBUG
    int initialized; // initialized statically
    #endif
    struct _BLog_channel channels[BLOG_NUM_CHANNELS];
    _BLog_log_func log_func;
    _BLog_free_func free_func;
    char logbuf[2048];
    int logbuf_pos;
};

extern struct _BLog_channel blog_channel_list[];
extern struct _BLog_global blog_global;

static int BLogGlobal_GetChannelByName (const char *channel_name);

static void BLog_Init (_BLog_log_func log_func, _BLog_free_func free_func);
static void BLog_Free (void);
static void BLog_SetChannelLoglevel (int channel, int loglevel);
static void BLog_AppendVarArg (const char *fmt, va_list vl);
static void BLog_Append (const char *fmt, ...);
static void BLog_Finish (int channel, int level);
static void BLog_LogToChannelStr (int channel, int level, const char *msg);
static void BLog_LogToChannelVarArg (int channel, int level, const char *fmt, va_list vl);
static void BLog_LogToChannel (int channel, int level, const char *fmt, ...);

void BLog_InitStdout (void);

int BLogGlobal_GetChannelByName (const char *channel_name)
{
    int i;
    for (i = 0; i < BLOG_NUM_CHANNELS; i++) {
        if (!strcmp(blog_channel_list[i].name, channel_name)) {
            return i;
        }
    }
    
    return -1;
}

void BLog_Init (_BLog_log_func log_func, _BLog_free_func free_func)
{
    ASSERT(!blog_global.initialized)
    
    #ifndef NDEBUG
    blog_global.initialized = 1;
    #endif
    
    // initialize channels
    memcpy(blog_global.channels, blog_channel_list, BLOG_NUM_CHANNELS * sizeof(struct _BLog_channel));
    
    blog_global.log_func = log_func;
    blog_global.free_func = free_func;
    blog_global.logbuf_pos = 0;
    blog_global.logbuf[0] = '\0';
}

void BLog_Free (void)
{
    ASSERT(blog_global.initialized)
    
    #ifndef NDEBUG
    blog_global.initialized = 0;
    #endif
    
    blog_global.free_func();
}

void BLog_SetChannelLoglevel (int channel, int loglevel)
{
    ASSERT(blog_global.initialized)
    ASSERT(channel >= 0 && channel < BLOG_NUM_CHANNELS)
    ASSERT(loglevel >= 0 && loglevel <= BLOG_DEBUG)
    
    blog_global.channels[channel].loglevel = loglevel;
}

void BLog_AppendVarArg (const char *fmt, va_list vl)
{
    ASSERT(blog_global.initialized)
    
    ASSERT(blog_global.logbuf_pos >= 0 && blog_global.logbuf_pos < sizeof(blog_global.logbuf))
    
    int w = vsnprintf(blog_global.logbuf + blog_global.logbuf_pos, sizeof(blog_global.logbuf) - blog_global.logbuf_pos, fmt, vl);
    
    if (w >= sizeof(blog_global.logbuf) - blog_global.logbuf_pos) {
        blog_global.logbuf_pos = sizeof(blog_global.logbuf) - 1;
    } else {
        blog_global.logbuf_pos += w;
    }
}

void BLog_Append (const char *fmt, ...)
{
    ASSERT(blog_global.initialized)
    
    va_list vl;
    va_start(vl, fmt);
    BLog_AppendVarArg(fmt, vl);
    va_end(vl);
}

void BLog_Finish (int channel, int level)
{
    ASSERT(blog_global.initialized)
    ASSERT(channel >= 0 && channel < BLOG_NUM_CHANNELS)
    ASSERT(level >= BLOG_ERROR && level <= BLOG_DEBUG)
    
    ASSERT(blog_global.logbuf_pos >= 0 && blog_global.logbuf_pos < sizeof(blog_global.logbuf))
    ASSERT(blog_global.logbuf[blog_global.logbuf_pos] == '\0')
    
    if (level <= blog_global.channels[channel].loglevel) {
        blog_global.log_func(channel, level, blog_global.logbuf);
    }
    
    blog_global.logbuf_pos = 0;
    blog_global.logbuf[0] = '\0';
}

void BLog_LogToChannelStr (int channel, int level, const char *msg)
{
    ASSERT(blog_global.initialized)
    ASSERT(channel >= 0 && channel < BLOG_NUM_CHANNELS)
    ASSERT(level >= BLOG_ERROR && level <= BLOG_DEBUG)
    
    BLog_Append("%s", msg);
    BLog_Finish(channel, level);
}

void BLog_LogToChannelVarArg (int channel, int level, const char *fmt, va_list vl)
{
    ASSERT(blog_global.initialized)
    ASSERT(channel >= 0 && channel < BLOG_NUM_CHANNELS)
    ASSERT(level >= BLOG_ERROR && level <= BLOG_DEBUG)
    
    BLog_AppendVarArg(fmt, vl);
    BLog_Finish(channel, level);
}

void BLog_LogToChannel (int channel, int level, const char *fmt, ...)
{
    ASSERT(blog_global.initialized)
    ASSERT(channel >= 0 && channel < BLOG_NUM_CHANNELS)
    ASSERT(level >= BLOG_ERROR && level <= BLOG_DEBUG)
    
    va_list vl;
    va_start(vl, fmt);
    BLog_LogToChannelVarArg(channel, level, fmt, vl);
    va_end(vl);
}

#endif
