/**
 * @file btimer_example.c
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <system/BReactor.h>
#include <base/BLog.h>
#include <system/BTime.h>

// gives average firing rate 100kHz
#define TIMER_NUM 500
#define TIMER_MODULO 10

BReactor sys;

void handle_timer (BTimer *bt)
{
    #ifdef BADVPN_USE_WINAPI
    btime_t time = btime_gettime() + rand()%TIMER_MODULO;
    #else
    btime_t time = btime_gettime() + random()%TIMER_MODULO;
    #endif
    BReactor_SetTimerAbsolute(&sys, bt, time);
}

int main ()
{
    BLog_InitStdout();
    
    #ifdef BADVPN_USE_WINAPI
    srand(time(NULL));
    #else
    srandom(time(NULL));
    #endif
    
    // init time
    BTime_Init();

    if (!BReactor_Init(&sys)) {
        DEBUG("BReactor_Init failed");
        return 1;
    }
    
    BTimer timers[TIMER_NUM];

    int i;
    for (i=0; i<TIMER_NUM; i++) {
        BTimer *timer = &timers[i];
        BTimer_Init(timer, 0, (BTimer_handler)handle_timer, timer);
        BReactor_SetTimer(&sys, timer);
    }
    
    int ret = BReactor_Exec(&sys);
    BReactor_Free(&sys);
    return ret;
}
