/**
 * @file fairqueue_test2.c
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

#include <string.h>

#include <misc/debug.h>
#include <system/BReactor.h>
#include <system/BLog.h>
#include <system/BTime.h>
#include <flow/PacketPassFairQueue.h>
#include <examples/FastPacketSource.h>
#include <examples/RandomPacketSink.h>

int main ()
{
    // initialize logging
    BLog_InitStdout();
    
    // init time
    BTime_Init();
    
    // initialize reactor
    BReactor reactor;
    if (!BReactor_Init(&reactor)) {
        DEBUG("BReactor_Init failed");
        return 1;
    }
    
    // initialize sink
    RandomPacketSink sink;
    RandomPacketSink_Init(&sink, &reactor, 500, 0);
    
    // initialize queue
    PacketPassFairQueue fq;
    PacketPassFairQueue_Init(&fq, RandomPacketSink_GetInput(&sink), BReactor_PendingGroup(&reactor));
    
    // initialize source 1
    PacketPassFairQueueFlow flow1;
    PacketPassFairQueueFlow_Init(&flow1, &fq);
    FastPacketSource source1;
    char data1[] = "data1";
    FastPacketSource_Init(&source1, PacketPassFairQueueFlow_GetInput(&flow1), (uint8_t *)data1, strlen(data1), BReactor_PendingGroup(&reactor));
    
    // initialize source 2
    PacketPassFairQueueFlow flow2;
    PacketPassFairQueueFlow_Init(&flow2, &fq);
    FastPacketSource source2;
    char data2[] = "data2data2";
    FastPacketSource_Init(&source2, PacketPassFairQueueFlow_GetInput(&flow2), (uint8_t *)data2, strlen(data2), BReactor_PendingGroup(&reactor));
    
    // initialize source 3
    PacketPassFairQueueFlow flow3;
    PacketPassFairQueueFlow_Init(&flow3, &fq);
    FastPacketSource source3;
    char data3[] = "data3data3data3data3data3data3data3data3data3";
    FastPacketSource_Init(&source3, PacketPassFairQueueFlow_GetInput(&flow3), (uint8_t *)data3, strlen(data3), BReactor_PendingGroup(&reactor));
    
    // run reactor
    int ret = BReactor_Exec(&reactor);
    BReactor_Free(&reactor);
    return ret;
}
