/**
 * @file FrameDecider.c
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
#include <stddef.h>
#include <inttypes.h>

#include <misc/debug.h>
#include <misc/offset.h>
#include <misc/balloc.h>
#include <misc/ethernet_proto.h>
#include <misc/ipv4_proto.h>
#include <misc/igmp_proto.h>
#include <misc/byteorder.h>
#include <system/BLog.h>

#include <client/FrameDecider.h>

#include <generated/blog_channel_FrameDecider.h>

#define DECIDE_STATE_NONE 1
#define DECIDE_STATE_UNICAST 2
#define DECIDE_STATE_FLOOD 3
#define DECIDE_STATE_MULTICAST 4

static int mac_comparator (void *user, uint8_t *mac1, uint8_t *mac2)
{
    int c = memcmp(mac1, mac2, 6);
    if (c < 0) {
        return -1;
    }
    if (c > 0) {
        return 1;
    }
    return 0;
}

static int uint32_comparator (void *user, uint32_t *v1, uint32_t *v2)
{
    if (*v1 < *v2) {
        return -1;
    }
    if (*v1 > *v2) {
        return 1;
    }
    return 0;
}

static void add_mac_to_peer (FrameDeciderPeer *o, uint8_t mac[6])
{
    FrameDecider *d = o->d;
    
    // locate entry in tree
    BAVLNode *tree_node = BAVL_LookupExact(&d->macs_tree, mac);
    if (tree_node) {
        struct _FrameDecider_mac_entry *entry = UPPER_OBJECT(tree_node, struct _FrameDecider_mac_entry, tree_node);
        
        if (entry->peer == o) {
            // this is our MAC; only move it to the end of the used list
            LinkedList2_Remove(&o->mac_entries_used, &entry->list_node);
            LinkedList2_Append(&o->mac_entries_used, &entry->list_node);
            return;
        }
        
        // some other peer has that MAC; disassociate it
        BAVL_Remove(&d->macs_tree, &entry->tree_node);
        LinkedList2_Remove(&entry->peer->mac_entries_used, &entry->list_node);
        LinkedList2_Append(&entry->peer->mac_entries_free, &entry->list_node);
    }
    
    // aquire MAC address entry, if there are no free ones reuse the oldest used one
    LinkedList2Node *list_node;
    struct _FrameDecider_mac_entry *entry;
    if (list_node = LinkedList2_GetFirst(&o->mac_entries_free)) {
        entry = UPPER_OBJECT(list_node, struct _FrameDecider_mac_entry, list_node);
        ASSERT(entry->peer == o)
        
        // remove from free
        LinkedList2_Remove(&o->mac_entries_free, &entry->list_node);
    } else {
        list_node = LinkedList2_GetFirst(&o->mac_entries_used);
        ASSERT(list_node)
        entry = UPPER_OBJECT(list_node, struct _FrameDecider_mac_entry, list_node);
        ASSERT(entry->peer == o)
        
        // remove from used
        BAVL_Remove(&d->macs_tree, &entry->tree_node);
        LinkedList2_Remove(&o->mac_entries_used, &entry->list_node);
    }
    
    BLog(BLOG_INFO, "adding MAC %02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8"", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // set MAC in entry
    memcpy(entry->mac, mac, sizeof(entry->mac));
    
    // add to used
    LinkedList2_Append(&o->mac_entries_used, &entry->list_node);
    ASSERT_EXECUTE(BAVL_Insert(&d->macs_tree, &entry->tree_node, NULL))
}

static uint32_t compute_sig_for_group (uint32_t group)
{
    return hton32(ntoh32(group)&0x7FFFFF);
}

static uint32_t compute_sig_for_mac (uint8_t *mac)
{
    uint32_t sig;
    memcpy(&sig, mac + 2, 4);
    sig = hton32(ntoh32(sig)&0x7FFFFF);
    return sig;
}

static void add_to_multicast (FrameDecider *d, struct _FrameDecider_group_entry *group_entry)
{
    // compute sig
    uint32_t sig = compute_sig_for_group(group_entry->group);
    
    BAVLNode *node;
    if (node = BAVL_LookupExact(&d->multicast_tree, &sig)) {
        // use existing master
        struct _FrameDecider_group_entry *master = UPPER_OBJECT(node, struct _FrameDecider_group_entry, master.tree_node);
        ASSERT(master->is_master)
        
        // set not master
        group_entry->is_master = 0;
        
        // insert to list
        LinkedList3Node_InitAfter(&group_entry->sig_list_node, &master->sig_list_node);
    } else {
        // make this entry master
        
        // set master
        group_entry->is_master = 1;
        
        // set sig
        group_entry->master.sig = sig;
        
        // insert to multicast tree
        ASSERT_EXECUTE(BAVL_Insert(&d->multicast_tree, &group_entry->master.tree_node, NULL))
        
        // init list node
        LinkedList3Node_InitLonely(&group_entry->sig_list_node);
    }
}

static void remove_from_multicast (FrameDecider *d, struct _FrameDecider_group_entry *group_entry)
{
    // compute sig
    uint32_t sig = compute_sig_for_group(group_entry->group);
    
    if (group_entry->is_master) {
        // remove master from multicast tree
        BAVL_Remove(&d->multicast_tree, &group_entry->master.tree_node);
        
        if (!LinkedList3Node_IsLonely(&group_entry->sig_list_node)) {
            // at least one more group entry for this sig; make another entry the master
            
            // get an entry
            LinkedList3Node *list_node = LinkedList3Node_NextOrPrev(&group_entry->sig_list_node);
            struct _FrameDecider_group_entry *newmaster = UPPER_OBJECT(list_node, struct _FrameDecider_group_entry, sig_list_node);
            ASSERT(!newmaster->is_master)
            
            // set master
            newmaster->is_master = 1;
            
            // set sig
            newmaster->master.sig = sig;
            
            // insert to multicast tree
            ASSERT_EXECUTE(BAVL_Insert(&d->multicast_tree, &newmaster->master.tree_node, NULL))
        }
    }
    
    // free linked list node
    LinkedList3Node_Free(&group_entry->sig_list_node);
}

static void add_group_to_peer (FrameDeciderPeer *o, uint32_t group)
{
    FrameDecider *d = o->d;
    
    struct _FrameDecider_group_entry *group_entry;
    
    // lookup if the peer already has that group
    BAVLNode *old_tree_node;
    if ((old_tree_node = BAVL_LookupExact(&o->groups_tree, &group))) {
        group_entry = UPPER_OBJECT(old_tree_node, struct _FrameDecider_group_entry, tree_node);
        
        // move to end of used list
        LinkedList2_Remove(&o->group_entries_used, &group_entry->list_node);
        LinkedList2_Append(&o->group_entries_used, &group_entry->list_node);
    } else {
        BLog(BLOG_INFO, "joined group %"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8"",
            ((uint8_t *)&group)[0], ((uint8_t *)&group)[1], ((uint8_t *)&group)[2], ((uint8_t *)&group)[3]
        );
        
        // aquire group entry, if there are no free ones reuse the earliest used one
        LinkedList2Node *node;
        if (node = LinkedList2_GetFirst(&o->group_entries_free)) {
            group_entry = UPPER_OBJECT(node, struct _FrameDecider_group_entry, list_node);
            
            // remove from free list
            LinkedList2_Remove(&o->group_entries_free, &group_entry->list_node);
        } else {
            node = LinkedList2_GetFirst(&o->group_entries_used);
            ASSERT(node)
            group_entry = UPPER_OBJECT(node, struct _FrameDecider_group_entry, list_node);
            
            // remove from multicast
            remove_from_multicast(d, group_entry);
            
            // remove from peer's groups tree
            BAVL_Remove(&o->groups_tree, &group_entry->tree_node);
            
            // remove from used list
            LinkedList2_Remove(&o->group_entries_used, &group_entry->list_node);
        }
        
        // add entry to used list
        LinkedList2_Append(&o->group_entries_used, &group_entry->list_node);
        
        // set group address
        group_entry->group = group;
        
        // insert to peer's groups tree
        ASSERT_EXECUTE(BAVL_Insert(&o->groups_tree, &group_entry->tree_node, NULL))
        
        // add to multicast
        add_to_multicast(d, group_entry);
    }
    
    // set timer
    group_entry->timer_endtime = btime_gettime() + d->igmp_group_membership_interval;
    BReactor_SetTimerAbsolute(d->reactor, &group_entry->timer, group_entry->timer_endtime);
}

static void remove_group_entry (struct _FrameDecider_group_entry *group_entry)
{
    FrameDeciderPeer *peer = group_entry->peer;
    FrameDecider *d = peer->d;
    
    uint32_t group = group_entry->group;
    
    BLog(BLOG_INFO, "left group %"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8"",
        ((uint8_t *)&group)[0], ((uint8_t *)&group)[1], ((uint8_t *)&group)[2], ((uint8_t *)&group)[3]
    );
    
    // remove from multicast
    remove_from_multicast(d, group_entry);
    
    // remove from peer's groups tree
    BAVL_Remove(&peer->groups_tree, &group_entry->tree_node);
    
    // remove from used list
    LinkedList2_Remove(&peer->group_entries_used, &group_entry->list_node);
    
    // add to free list
    LinkedList2_Append(&peer->group_entries_free, &group_entry->list_node);
    
    // stop timer
    BReactor_RemoveTimer(d->reactor, &group_entry->timer);
}

static void lower_group_timers_to_lmqt (FrameDecider *d, uint32_t group)
{
    // have to lower all the group timers of this group down to LMQT
    
    // compute sig
    uint32_t sig = compute_sig_for_group(group);
    
    // look up the sig in multicast tree
    BAVLNode *tree_node;
    if (!(tree_node = BAVL_LookupExact(&d->multicast_tree, &sig))) {
        return;
    }
    struct _FrameDecider_group_entry *master = UPPER_OBJECT(tree_node, struct _FrameDecider_group_entry, master.tree_node);
    ASSERT(master->is_master)
    
    // iterate all group entries with this sig
    LinkedList3Iterator it;
    LinkedList3Iterator_Init(&it, LinkedList3Node_First(&master->sig_list_node), 1);
    LinkedList3Node *sig_list_node;
    while (sig_list_node = LinkedList3Iterator_Next(&it)) {
        struct _FrameDecider_group_entry *group_entry = UPPER_OBJECT(sig_list_node, struct _FrameDecider_group_entry, sig_list_node);
        
        // skip wrong groups
        if (group_entry->group != group) {
            continue;
        }
        
        // lower timer down to LMQT
        btime_t now = btime_gettime();
        if (group_entry->timer_endtime > now + d->igmp_last_member_query_time) {
            group_entry->timer_endtime = now + d->igmp_last_member_query_time;
            BReactor_SetTimerAbsolute(d->reactor, &group_entry->timer, group_entry->timer_endtime);
        }
    }
}

static int check_ipv4_packet (uint8_t *data, int data_len, struct ipv4_header **out_header, uint8_t **out_payload, int *out_payload_len)
{
    // check base header
    if (data_len < sizeof(struct ipv4_header)) {
        BLog(BLOG_DEBUG, "check ipv4: packet too short (base header)");
        return 0;
    }
    struct ipv4_header *header = (struct ipv4_header *)data;
    
    // check version
    if (IPV4_GET_VERSION(*header) != 4) {
        BLog(BLOG_DEBUG, "check ipv4: version not 4");
        return 0;
    }
    
    // check options
    int header_len = IPV4_GET_IHL(*header) * 4;
    if (header_len < sizeof(struct ipv4_header)) {
        BLog(BLOG_DEBUG, "check ipv4: ihl too small");
        return 0;
    }
    if (header_len > data_len) {
        BLog(BLOG_DEBUG, "check ipv4: packet too short for ihl");
        return 0;
    }
    
    // check total length
    uint16_t total_length = ntoh16(header->total_length);
    if (total_length < header_len) {
        BLog(BLOG_DEBUG, "check ipv4: total length too small");
        return 0;
    }
    if (total_length > data_len) {
        BLog(BLOG_DEBUG, "check ipv4: total length too large");
        return 0;
    }
    
    *out_header = header;
    *out_payload = data + header_len;
    *out_payload_len = total_length - header_len;
    
    return 1;
}

void FrameDecider_Init (FrameDecider *o, int max_peer_macs, int max_peer_groups, btime_t igmp_group_membership_interval, btime_t igmp_last_member_query_time, BReactor *reactor)
{
    ASSERT(max_peer_macs > 0)
    ASSERT(max_peer_groups > 0)
    
    // init arguments
    o->max_peer_macs = max_peer_macs;
    o->max_peer_groups = max_peer_groups;
    o->igmp_group_membership_interval = igmp_group_membership_interval;
    o->igmp_last_member_query_time = igmp_last_member_query_time;
    o->reactor = reactor;
    
    // init peers list
    LinkedList2_Init(&o->peers_list);
    
    // init MAC tree
    BAVL_Init(&o->macs_tree, OFFSET_DIFF(struct _FrameDecider_mac_entry, mac, tree_node), (BAVL_comparator)mac_comparator, NULL);
    
    // init multicast tree
    BAVL_Init(&o->multicast_tree, OFFSET_DIFF(struct _FrameDecider_group_entry, master.sig, master.tree_node), (BAVL_comparator)uint32_comparator, NULL);
    
    // init decide state
    o->decide_state = DECIDE_STATE_NONE;
    
    DebugObject_Init(&o->d_obj);
}

void FrameDecider_Free (FrameDecider *o)
{
    ASSERT(BAVL_IsEmpty(&o->multicast_tree))
    ASSERT(BAVL_IsEmpty(&o->macs_tree))
    ASSERT(LinkedList2_IsEmpty(&o->peers_list))
    DebugObject_Free(&o->d_obj);
}

void FrameDecider_AnalyzeAndDecide (FrameDecider *o, uint8_t *frame, int frame_len)
{
    ASSERT(frame_len >= 0)
    DebugObject_Access(&o->d_obj);
    
    // reset decide state
    switch (o->decide_state) {
        case DECIDE_STATE_NONE:
            break;
        case DECIDE_STATE_UNICAST:
            break;
        case DECIDE_STATE_FLOOD:
            LinkedList2Iterator_Free(&o->decide_flood_it);
            break;
        case DECIDE_STATE_MULTICAST:
            LinkedList3Iterator_Free(&o->decide_multicast_it);
            return;
        default:
            ASSERT(0);
    }
    o->decide_state = DECIDE_STATE_NONE;
    
    // analyze frame
    
    uint8_t *pos = frame;
    int len = frame_len;
    
    if (len < sizeof(struct ethernet_header)) {
        return;
    }
    struct ethernet_header *eh = (struct ethernet_header *)pos;
    pos += sizeof(struct ethernet_header);
    len -= sizeof(struct ethernet_header);
    
    int is_igmp = 0;
    
    switch (ntoh16(eh->type)) {
        case ETHERTYPE_IPV4: {
            // check IPv4 header
            struct ipv4_header *ipv4_header;
            if (!check_ipv4_packet(pos, len, &ipv4_header, &pos, &len)) {
                BLog(BLOG_INFO, "decide: wrong IP packet");
                goto out;
            }
            
            // check if it's IGMP
            if (ntoh8(ipv4_header->protocol) != IPV4_PROTOCOL_IGMP) {
                goto out;
            }
            
            // remember that it's IGMP; we have to flood IGMP frames
            is_igmp = 1;
            
            // check IGMP header
            if (len < sizeof(struct igmp_base)) {
                BLog(BLOG_INFO, "decide: IGMP: short packet");
                goto out;
            }
            struct igmp_base *igmp_base = (struct igmp_base *)pos;
            pos += sizeof(struct igmp_base);
            len -= sizeof(struct igmp_base);
            
            switch (ntoh8(igmp_base->type)) {
                case IGMP_TYPE_MEMBERSHIP_QUERY: {
                    if (len == sizeof(struct igmp_v2_extra) && ntoh8(igmp_base->max_resp_code) != 0) {
                        // V2 query
                        struct igmp_v2_extra *query = (struct igmp_v2_extra *)pos;
                        pos += sizeof(struct igmp_v2_extra);
                        len -= sizeof(struct igmp_v2_extra);
                        
                        if (ntoh32(query->group) != 0) {
                            // got a Group-Specific Query, lower group timers to LMQT
                            lower_group_timers_to_lmqt(o, query->group);
                        }
                    }
                    else if (len >= sizeof(struct igmp_v3_query_extra)) {
                        // V3 query
                        struct igmp_v3_query_extra *query = (struct igmp_v3_query_extra *)pos;
                        pos += sizeof(struct igmp_v3_query_extra);
                        len -= sizeof(struct igmp_v3_query_extra);
                        
                        // iterate sources
                        uint16_t num_sources = ntoh16(query->number_of_sources);
                        int i;
                        for (i = 0; i < num_sources; i++) {
                            // check source
                            if (len < sizeof(struct igmp_source)) {
                                BLog(BLOG_NOTICE, "decide: IGMP: short source");
                                goto out;
                            }
                            pos += sizeof(struct igmp_source);
                            len -= sizeof(struct igmp_source);
                        }
                        
                        if (ntoh32(query->group) != 0 && num_sources == 0) {
                            // got a Group-Specific Query, lower group timers to LMQT
                            lower_group_timers_to_lmqt(o, query->group);
                        }
                    }
                } break;
            }
        } break;
    }
    
out:;
    
    const uint8_t broadcast_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    const uint8_t multicast_mac_header[] = {0x01, 0x00, 0x5e};
    
    // if it's broadcast or IGMP, flood it
    if (is_igmp || !memcmp(eh->dest, broadcast_mac, sizeof(broadcast_mac))) {
        o->decide_state = DECIDE_STATE_FLOOD;
        LinkedList2Iterator_InitForward(&o->decide_flood_it, &o->peers_list);
        return;
    }
    
    // if it's multicast, forward to all peers with the given sig
    if (!memcmp(eh->dest, multicast_mac_header, sizeof(multicast_mac_header))) {
        // extract group's sig from destination MAC
        uint32_t sig = compute_sig_for_mac(eh->dest);
        
        // look up the sig in multicast tree
        BAVLNode *node;
        if (node = BAVL_LookupExact(&o->multicast_tree, &sig)) {
            struct _FrameDecider_group_entry *master = UPPER_OBJECT(node, struct _FrameDecider_group_entry, master.tree_node);
            ASSERT(master->is_master)
            
            o->decide_state = DECIDE_STATE_MULTICAST;
            LinkedList3Iterator_Init(&o->decide_multicast_it, LinkedList3Node_First(&master->sig_list_node), 1);
        }
        
        return;
    }
    
    // look for MAC entry
    BAVLNode *tree_node;
    if (tree_node = BAVL_LookupExact(&o->macs_tree, eh->dest)) {
        struct _FrameDecider_mac_entry *entry = UPPER_OBJECT(tree_node, struct _FrameDecider_mac_entry, tree_node);
        o->decide_state = DECIDE_STATE_UNICAST;
        o->decide_unicast_peer = entry->peer;
        return;
    }
    
    // unknown destination MAC, flood
    o->decide_state = DECIDE_STATE_FLOOD;
    LinkedList2Iterator_InitForward(&o->decide_flood_it, &o->peers_list);
    return;
}

FrameDeciderPeer * FrameDecider_NextDestination (FrameDecider *o)
{
    DebugObject_Access(&o->d_obj);
    
    switch (o->decide_state) {
        case DECIDE_STATE_NONE: {
            return NULL;
        } break;
            
        case DECIDE_STATE_UNICAST: {
            o->decide_state = DECIDE_STATE_NONE;
            
            return o->decide_unicast_peer;
        } break;
        
        case DECIDE_STATE_FLOOD: {
            LinkedList2Node *list_node = LinkedList2Iterator_Next(&o->decide_flood_it);
            if (!list_node) {
                o->decide_state = DECIDE_STATE_NONE;
                return NULL;
            }
            FrameDeciderPeer *peer = UPPER_OBJECT(list_node, FrameDeciderPeer, list_node);
            
            return peer;
        } break;
        
        case DECIDE_STATE_MULTICAST: {
            LinkedList3Node *list_node = LinkedList3Iterator_Next(&o->decide_multicast_it);
            if (!list_node) {
                o->decide_state = DECIDE_STATE_NONE;
                return NULL;
            }
            struct _FrameDecider_group_entry *group_entry = UPPER_OBJECT(list_node, struct _FrameDecider_group_entry, sig_list_node);
            
            return group_entry->peer;
        } break;
        
        default:
            ASSERT(0);
    }
}

static void group_entry_timer_handler (struct _FrameDecider_group_entry *group_entry)
{
    remove_group_entry(group_entry);
}

int FrameDeciderPeer_Init (FrameDeciderPeer *o, FrameDecider *d)
{
    // init arguments
    o->d = d;
    
    // allocate MAC entries
    if (!(o->mac_entries = BAllocArray(d->max_peer_macs, sizeof(struct _FrameDecider_mac_entry)))) {
        DEBUG("failed to allocate MAC entries");
        goto fail0;
    }
    
    // allocate group entries
    if (!(o->group_entries = BAllocArray(d->max_peer_groups, sizeof(struct _FrameDecider_group_entry)))) {
        DEBUG("failed to allocate group entries");
        goto fail1;
    }
    
    // insert to peers list
    LinkedList2_Append(&d->peers_list, &o->list_node);
    
    // init MAC entry lists
    LinkedList2_Init(&o->mac_entries_free);
    LinkedList2_Init(&o->mac_entries_used);
    
    // initialize MAC entries
    for (int i = 0; i < d->max_peer_macs; i++) {
        struct _FrameDecider_mac_entry *entry = &o->mac_entries[i];
        
        // set peer
        entry->peer = o;
        
        // insert to free list
        LinkedList2_Append(&o->mac_entries_free, &entry->list_node);
    }
    
    // init group entry lists
    LinkedList2_Init(&o->group_entries_free);
    LinkedList2_Init(&o->group_entries_used);
    
    // initialize group entries
    for (int i = 0; i < d->max_peer_groups; i++) {
        struct _FrameDecider_group_entry *entry = &o->group_entries[i];
        
        // set peer
        entry->peer = o;
        
        // insert to free list
        LinkedList2_Append(&o->group_entries_free, &entry->list_node);
        
        // init timer
        BTimer_Init(&entry->timer, 0, (BTimer_handler)group_entry_timer_handler, entry);
    }
    
    // initialize groups tree
    BAVL_Init(&o->groups_tree, OFFSET_DIFF(struct _FrameDecider_group_entry, group, tree_node), (BAVL_comparator)uint32_comparator, NULL);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    BFree(o->mac_entries);
fail0:
    return 0;
}

void FrameDeciderPeer_Free (FrameDeciderPeer *o)
{
    DebugObject_Free(&o->d_obj);
    
    FrameDecider *d = o->d;
    
    // remove decide unicast reference
    if (d->decide_state == DECIDE_STATE_UNICAST && d->decide_unicast_peer == o) {
        d->decide_state = DECIDE_STATE_NONE;
    }
    
    LinkedList2Iterator it;
    LinkedList2Node *node;
    
    // free group entries
    LinkedList2Iterator_InitForward(&it, &o->group_entries_used);
    while (node = LinkedList2Iterator_Next(&it)) {
        struct _FrameDecider_group_entry *entry = UPPER_OBJECT(node, struct _FrameDecider_group_entry, list_node);
        
        // remove from multicast
        remove_from_multicast(d, entry);
        
        // stop timer
        BReactor_RemoveTimer(d->reactor, &entry->timer);
    }
    
    // remove used MAC entries from tree
    LinkedList2Iterator_InitForward(&it, &o->mac_entries_used);
    while (node = LinkedList2Iterator_Next(&it)) {
        struct _FrameDecider_mac_entry *entry = UPPER_OBJECT(node, struct _FrameDecider_mac_entry, list_node);
        
        // remove from tree
        BAVL_Remove(&d->macs_tree, &entry->tree_node);
    }
    
    // remove from peers list
    LinkedList2_Remove(&d->peers_list, &o->list_node);
    
    // free group entries
    BFree(o->group_entries);
    
    // free MAC entries
    BFree(o->mac_entries);
}

void FrameDeciderPeer_Analyze (FrameDeciderPeer *o, uint8_t *frame, int frame_len)
{
    ASSERT(frame_len >= 0)
    DebugObject_Access(&o->d_obj);
    
    uint8_t *pos = frame;
    int len = frame_len;
    
    if (len < sizeof(struct ethernet_header)) {
        goto out;
    }
    struct ethernet_header *eh = (struct ethernet_header *)pos;
    pos += sizeof(struct ethernet_header);
    len -= sizeof(struct ethernet_header);
    
    // register source MAC address with this peer
    add_mac_to_peer(o, eh->source);
    
    switch (ntoh16(eh->type)) {
        case ETHERTYPE_IPV4: {
            // check IPv4 header
            struct ipv4_header *ipv4_header;
            if (!check_ipv4_packet(pos, len, &ipv4_header, &pos, &len)) {
                BLog(BLOG_INFO, "analyze: wrong IP packet");
                goto out;
            }
            
            // check if it's IGMP
            if (ntoh8(ipv4_header->protocol) != IPV4_PROTOCOL_IGMP) {
                goto out;
            }
            
            // check IGMP header
            if (len < sizeof(struct igmp_base)) {
                BLog(BLOG_INFO, "analyze: IGMP: short packet");
                goto out;
            }
            struct igmp_base *igmp_base = (struct igmp_base *)pos;
            pos += sizeof(struct igmp_base);
            len -= sizeof(struct igmp_base);
            
            switch (ntoh8(igmp_base->type)) {
                case IGMP_TYPE_V2_MEMBERSHIP_REPORT: {
                    // check extra
                    if (len < sizeof(struct igmp_v2_extra)) {
                        BLog(BLOG_INFO, "analyze: IGMP: short v2 report");
                        goto out;
                    }
                    struct igmp_v2_extra *report = (struct igmp_v2_extra *)pos;
                    pos += sizeof(struct igmp_v2_extra);
                    len -= sizeof(struct igmp_v2_extra);
                    
                    // add to group
                    add_group_to_peer(o, report->group);
                } break;
                
                case IGMP_TYPE_V3_MEMBERSHIP_REPORT: {
                    // check extra
                    if (len < sizeof(struct igmp_v3_report_extra)) {
                        BLog(BLOG_INFO, "analyze: IGMP: short v3 report");
                        goto out;
                    }
                    struct igmp_v3_report_extra *report = (struct igmp_v3_report_extra *)pos;
                    pos += sizeof(struct igmp_v3_report_extra);
                    len -= sizeof(struct igmp_v3_report_extra);
                    
                    // iterate records
                    uint16_t num_records = ntoh16(report->number_of_group_records);
                    for (int i = 0; i < num_records; i++) {
                        // check record
                        if (len < sizeof(struct igmp_v3_report_record)) {
                            BLog(BLOG_INFO, "analyze: IGMP: short record header");
                            goto out;
                        }
                        struct igmp_v3_report_record *record = (struct igmp_v3_report_record *)pos;
                        pos += sizeof(struct igmp_v3_report_record);
                        len -= sizeof(struct igmp_v3_report_record);
                        
                        // iterate sources
                        uint16_t num_sources = ntoh16(record->number_of_sources);
                        int j;
                        for (j = 0; j < num_sources; j++) {
                            // check source
                            if (len < sizeof(struct igmp_source)) {
                                BLog(BLOG_INFO, "analyze: IGMP: short source");
                                goto out;
                            }
                            struct igmp_source *source = (struct igmp_source *)pos;
                            pos += sizeof(struct igmp_source);
                            len -= sizeof(struct igmp_source);
                        }
                        
                        // check aux data
                        uint16_t aux_len = ntoh16(record->aux_data_len);
                        if (len < aux_len) {
                            BLog(BLOG_INFO, "analyze: IGMP: short record aux data");
                            goto out;
                        }
                        pos += aux_len;
                        len -= aux_len;
                        
                        switch (record->type) {
                            case IGMP_RECORD_TYPE_MODE_IS_INCLUDE:
                            case IGMP_RECORD_TYPE_CHANGE_TO_INCLUDE_MODE:
                                if (num_sources != 0) {
                                    add_group_to_peer(o, record->group);
                                }
                                break;
                            case IGMP_RECORD_TYPE_MODE_IS_EXCLUDE:
                            case IGMP_RECORD_TYPE_CHANGE_TO_EXCLUDE_MODE:
                                add_group_to_peer(o, record->group);
                                break;
                        }
                    }
                } break;
            }
        } break;
    }
    
out:;
}
