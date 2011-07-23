/**
 * @file FragmentProtoAssembler.c
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
#include <string.h>

#include <misc/offset.h>
#include <misc/byteorder.h>
#include <misc/balloc.h>

#include "FragmentProtoAssembler.h"

#include <generated/blog_channel_FragmentProtoAssembler.h>

#define PeerLog(_o, ...) BLog_LogViaFunc((_o)->logfunc, (_o)->user, BLOG_CURRENT_CHANNEL, __VA_ARGS__)

static int frame_id_comparator (void *unused, fragmentproto_frameid *v1, fragmentproto_frameid *v2)
{
    if (*v1 < *v2) {
        return -1;
    }
    if (*v1 > *v2) {
        return 1;
    }
    return 0;
}

static void free_frame (FragmentProtoAssembler *o, struct FragmentProtoAssembler_frame *frame)
{
    // remove from used list
    LinkedList2_Remove(&o->frames_used, &frame->list_node);
    // remove from used tree
    BAVL_Remove(&o->frames_used_tree, &frame->tree_node);
    
    // append to free list
    LinkedList2_Append(&o->frames_free, &frame->list_node);
}

static void free_oldest_frame (FragmentProtoAssembler *o)
{
    ASSERT(!LinkedList2_IsEmpty(&o->frames_used))
    
    // obtain oldest frame (first on the list)
    LinkedList2Node *list_node = LinkedList2_GetFirst(&o->frames_used);
    ASSERT(list_node)
    struct FragmentProtoAssembler_frame *frame = UPPER_OBJECT(list_node, struct FragmentProtoAssembler_frame, list_node);
    
    // free frame
    free_frame(o, frame);
}

static struct FragmentProtoAssembler_frame * allocate_new_frame (FragmentProtoAssembler *o, fragmentproto_frameid id)
{
    ASSERT(!BAVL_LookupExact(&o->frames_used_tree, &id))
    
    // if there are no free entries, free the oldest used one
    if (LinkedList2_IsEmpty(&o->frames_free)) {
        PeerLog(o, BLOG_INFO, "freeing used frame");
        free_oldest_frame(o);
    }
    
    // obtain frame entry
    LinkedList2Node *list_node = LinkedList2_GetFirst(&o->frames_free);
    ASSERT(list_node)
    struct FragmentProtoAssembler_frame *frame = UPPER_OBJECT(list_node, struct FragmentProtoAssembler_frame, list_node);
    
    // remove from free list
    LinkedList2_Remove(&o->frames_free, &frame->list_node);
    
    // initialize values
    frame->id = id;
    frame->time = o->time;
    frame->num_chunks = 0;
    frame->sum = 0;
    frame->length = -1;
    frame->length_so_far = 0;
    
    // append to used list
    LinkedList2_Append(&o->frames_used, &frame->list_node);
    // insert to used tree
    ASSERT_EXECUTE(BAVL_Insert(&o->frames_used_tree, &frame->tree_node, NULL))
    
    return frame;
}

static int chunks_overlap (int c1_start, int c1_len, int c2_start, int c2_len)
{
    return (c1_start + c1_len > c2_start && c2_start + c2_len > c1_start);
}

static int frame_is_timed_out (FragmentProtoAssembler *o, struct FragmentProtoAssembler_frame *frame)
{
    ASSERT(frame->time <= o->time)
    
    return (o->time - frame->time > o->time_tolerance);
}

static void reduce_times (FragmentProtoAssembler *o)
{
    // find the frame with minimal time, removing timed out frames
    struct FragmentProtoAssembler_frame *minframe = NULL;
    LinkedList2Iterator it;
    LinkedList2Iterator_InitForward(&it, &o->frames_used);
    LinkedList2Node *list_node;
    while (list_node = LinkedList2Iterator_Next(&it)) {
        struct FragmentProtoAssembler_frame *frame = UPPER_OBJECT(list_node, struct FragmentProtoAssembler_frame, list_node);
        if (frame_is_timed_out(o, frame)) {
            PeerLog(o, BLOG_INFO, "freeing timed out frame (while reducing times)");
            free_frame(o, frame);
        } else {
            if (!minframe || frame->time < minframe->time) {
                minframe = frame;
            }
        }
    }
    
    if (!minframe) {
        // have no frames, set packet time to zero
        o->time = 0;
        return;
    }
    
    uint32_t min_time = minframe->time;
    
    // subtract minimal time from all frames
    LinkedList2Iterator_InitForward(&it, &o->frames_used);
    while (list_node = LinkedList2Iterator_Next(&it)) {
        struct FragmentProtoAssembler_frame *frame = UPPER_OBJECT(list_node, struct FragmentProtoAssembler_frame, list_node);
        frame->time -= min_time;
    }
    
    // subtract minimal time from packet time
    o->time -= min_time;
}

static int process_chunk (FragmentProtoAssembler *o, fragmentproto_frameid frame_id, int chunk_start, int chunk_len, int is_last, uint8_t *payload)
{
    ASSERT(chunk_start >= 0)
    ASSERT(chunk_len >= 0)
    ASSERT(is_last == 0 || is_last == 1)
    
    // verify chunk
    
    // check start
    if (chunk_start > o->output_mtu) {
        PeerLog(o, BLOG_INFO, "chunk starts outside");
        return 0;
    }
    
    // check frame size bound
    if (chunk_len > o->output_mtu - chunk_start) {
        PeerLog(o, BLOG_INFO, "chunk ends outside");
        return 0;
    }
    
    // calculate end
    int chunk_end = chunk_start + chunk_len;
    ASSERT(chunk_end >= 0)
    ASSERT(chunk_end <= o->output_mtu)
    
    // lookup frame
    struct FragmentProtoAssembler_frame *frame;
    BAVLNode *tree_node;
    if (!(tree_node = BAVL_LookupExact(&o->frames_used_tree, &frame_id))) {
        // frame not found, add a new one
        frame = allocate_new_frame(o, frame_id);
    } else {
        // have existing frame with that ID
        frame = UPPER_OBJECT(tree_node, struct FragmentProtoAssembler_frame, tree_node);
        // check frame time
        if (frame_is_timed_out(o, frame)) {
            // frame is timed out, remove it and use a new one
            PeerLog(o, BLOG_INFO, "freeing timed out frame (while processing chunk)");
            free_frame(o, frame);
            frame = allocate_new_frame(o, frame_id);
        }
    }
    
    ASSERT(frame->num_chunks < o->num_chunks)
    
    // check if the chunk overlaps with any existing chunks
    for (int i = 0; i < frame->num_chunks; i++) {
        struct FragmentProtoAssembler_chunk *chunk = &frame->chunks[i];
        if (chunks_overlap(chunk->start, chunk->len, chunk_start, chunk_len)) {
            PeerLog(o, BLOG_INFO, "chunk overlaps with existing chunk");
            goto fail_frame;
        }
    }
    
    if (is_last) {
        // this chunk is marked as last
        if (frame->length >= 0) {
            PeerLog(o, BLOG_INFO, "got last chunk, but already have one");
            goto fail_frame;
        }
        // check if frame size according to this packet is consistent
        // with existing chunks
        if (frame->length_so_far > chunk_end) {
            PeerLog(o, BLOG_INFO, "got last chunk, but already have data over its bound");
            goto fail_frame;
        }
    } else {
        // if we have length, chunk must be in its bound
        if (frame->length >= 0) {
            if (chunk_end > frame->length) {
                PeerLog(o, BLOG_INFO, "chunk out of length bound");
                goto fail_frame;
            }
        }
    }
    
    // chunk is good, add it
    
    // update frame time
    frame->time = o->time;
    
    // add chunk entry
    struct FragmentProtoAssembler_chunk *chunk = &frame->chunks[frame->num_chunks];
    chunk->start = chunk_start;
    chunk->len = chunk_len;
    frame->num_chunks++;
    
    // update sum
    frame->sum += chunk_len;
    
    // update length
    if (is_last) {
        frame->length = chunk_end;
    } else {
        if (frame->length < 0) {
            if (frame->length_so_far < chunk_end) {
                frame->length_so_far = chunk_end;
            }
        }
    }
    
    // copy chunk payload to buffer
    memcpy(frame->buffer + chunk_start, payload, chunk_len);
    
    // is frame incomplete?
    if (frame->length < 0 || frame->sum < frame->length) {
        // if all chunks are used, fail it
        if (frame->num_chunks == o->num_chunks) {
            PeerLog(o, BLOG_INFO, "all chunks used, but frame not complete");
            goto fail_frame;
        }
        
        // wait for more chunks
        return 0;
    }
    
    ASSERT(frame->sum == frame->length)
    
    PeerLog(o, BLOG_DEBUG, "frame complete");
    
    // free frame entry
    free_frame(o, frame);
    
    // send frame
    PacketPassInterface_Sender_Send(o->output, frame->buffer, frame->length);
    
    return 1;
    
fail_frame:
    free_frame(o, frame);
    return 0;
}

static void process_input (FragmentProtoAssembler *o)
{
    ASSERT(o->in_len >= 0)
    
    // read chunks
    while (o->in_pos < o->in_len) {
        // obtain header
        if (o->in_len - o->in_pos < sizeof(struct fragmentproto_chunk_header)) {
            PeerLog(o, BLOG_INFO, "too little data for chunk header");
            break;
        }
        struct fragmentproto_chunk_header *header = (struct fragmentproto_chunk_header *)(o->in + o->in_pos);
        o->in_pos += sizeof(struct fragmentproto_chunk_header);
        fragmentproto_frameid frame_id = ltoh16(header->frame_id);
        int chunk_start = ltoh16(header->chunk_start);
        int chunk_len = ltoh16(header->chunk_len);
        int is_last = ltoh8(header->is_last);
        
        // check is_last field
        if (!(is_last == 0 || is_last == 1)) {
            PeerLog(o, BLOG_INFO, "chunk is_last wrong");
            break;
        }
        
        // obtain data
        if (o->in_len - o->in_pos < chunk_len) {
            PeerLog(o, BLOG_INFO, "too little data for chunk data");
            break;
        }
        
        // process chunk
        int res = process_chunk(o, frame_id, chunk_start, chunk_len, is_last, o->in + o->in_pos);
        o->in_pos += chunk_len;
        
        if (res) {
            // sending complete frame, stop processing input
            return;
        }
    }
    
    // increment packet time
    if (o->time == FPA_MAX_TIME) {
        reduce_times(o);
        if (!LinkedList2_IsEmpty(&o->frames_used)) {
            ASSERT(o->time < FPA_MAX_TIME) // If there was a frame with zero time, it was removed because
                                           // time_tolerance < FPA_MAX_TIME. So something >0 was subtracted.
            o->time++;
        } else {
            // it was set to zero by reduce_times
            ASSERT(o->time == 0)
        }
    } else {
        o->time++;
    }
    
    // set no input packet
    o->in_len = -1;
    
    // finish input
    PacketPassInterface_Done(&o->input);
}

static void input_handler_send (FragmentProtoAssembler *o, uint8_t *data, int data_len)
{
    ASSERT(data_len >= 0)
    ASSERT(o->in_len == -1)
    DebugObject_Access(&o->d_obj);
    
    // save input packet
    o->in_len = data_len;
    o->in = data;
    o->in_pos = 0;
    
    process_input(o);
}

static void output_handler_done (FragmentProtoAssembler *o)
{
    ASSERT(o->in_len >= 0)
    DebugObject_Access(&o->d_obj);
    
    process_input(o);
}

int FragmentProtoAssembler_Init (FragmentProtoAssembler *o, int input_mtu, PacketPassInterface *output, int num_frames, int num_chunks, BPendingGroup *pg, void *user, BLog_logfunc logfunc)
{
    ASSERT(input_mtu >= 0)
    ASSERT(num_frames > 0)
    ASSERT(num_frames < FPA_MAX_TIME) // needed so we can always subtract times when packet time is maximum
    ASSERT(num_chunks > 0)
    
    // init arguments
    o->output = output;
    o->num_chunks = num_chunks;
    o->user = user;
    o->logfunc = logfunc;
    
    // init input
    PacketPassInterface_Init(&o->input, input_mtu, (PacketPassInterface_handler_send)input_handler_send, o, pg);
    
    // init output
    PacketPassInterface_Sender_Init(o->output, (PacketPassInterface_handler_done)output_handler_done, o);
    
    // remebmer output MTU
    o->output_mtu = PacketPassInterface_GetMTU(o->output);
    
    // set packet time to zero
    o->time = 0;
    
    // set time tolerance to num_frames
    o->time_tolerance = num_frames;
    
    // allocate frames
    if (!(o->frames_entries = BAllocArray(num_frames, sizeof(o->frames_entries[0])))) {
        goto fail1;
    }
    
    // allocate chunks
    if (!(o->frames_chunks = BAllocArray2(num_frames, o->num_chunks, sizeof(o->frames_chunks[0])))) {
        goto fail2;
    }
    
    // allocate buffers
    if (!(o->frames_buffer = BAllocArray(num_frames, o->output_mtu))) {
        goto fail3;
    }
    
    // init frame lists
    LinkedList2_Init(&o->frames_free);
    LinkedList2_Init(&o->frames_used);
    
    // initialize frame entries
    for (int i = 0; i < num_frames; i++) {
        struct FragmentProtoAssembler_frame *frame = &o->frames_entries[i];
        // set chunks array pointer
        frame->chunks = o->frames_chunks + (size_t)i * o->num_chunks;
        // set buffer pointer
        frame->buffer = o->frames_buffer + (size_t)i * o->output_mtu;
        // add to free list
        LinkedList2_Append(&o->frames_free, &frame->list_node);
    }
    
    // init tree
    BAVL_Init(&o->frames_used_tree, OFFSET_DIFF(struct FragmentProtoAssembler_frame, id, tree_node), (BAVL_comparator)frame_id_comparator, NULL);
    
    // have no input packet
    o->in_len = -1;
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail3:
    BFree(o->frames_chunks);
fail2:
    BFree(o->frames_entries);
fail1:
    PacketPassInterface_Free(&o->input);
    return 0;
}

void FragmentProtoAssembler_Free (FragmentProtoAssembler *o)
{
    DebugObject_Free(&o->d_obj);

    // free buffers
    BFree(o->frames_buffer);
    
    // free chunks
    BFree(o->frames_chunks);
    
    // free frames
    BFree(o->frames_entries);
    
    // free input
    PacketPassInterface_Free(&o->input);
}

PacketPassInterface * FragmentProtoAssembler_GetInput (FragmentProtoAssembler *o)
{
    DebugObject_Access(&o->d_obj);
    
    return &o->input;
}
