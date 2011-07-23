/**
 * @file FrameDecider.h
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
 * Mudule which decides to which peers frames from the device are to be
 * forwarded.
 */

#ifndef BADVPN_CLIENT_FRAMEDECIDER_H
#define BADVPN_CLIENT_FRAMEDECIDER_H

#include <stdint.h>

#include <structure/BAVL.h>
#include <structure/LinkedList2.h>
#include <structure/LinkedList3.h>
#include <base/DebugObject.h>
#include <base/BLog.h>
#include <system/BReactor.h>

struct _FrameDeciderPeer;
struct _FrameDecider_mac_entry;

/**
 * Object that represents a local device.
 */
typedef struct {
    int max_peer_macs;
    int max_peer_groups;
    btime_t igmp_group_membership_interval;
    btime_t igmp_last_member_query_time;
    BReactor *reactor;
    LinkedList2 peers_list;
    BAVL macs_tree;
    BAVL multicast_tree;
    int decide_state;
    LinkedList2Iterator decide_flood_it;
    struct _FrameDeciderPeer *decide_unicast_peer;
    LinkedList3Iterator decide_multicast_it;
    DebugObject d_obj;
} FrameDecider;

/**
 * Object that represents a peer that a local device can send frames to.
 */
typedef struct _FrameDeciderPeer {
    FrameDecider *d;
    void *user;
    BLog_logfunc logfunc;
    struct _FrameDecider_mac_entry *mac_entries;
    struct _FrameDecider_group_entry *group_entries;
    LinkedList2Node list_node; // node in FrameDecider.peers_list
    LinkedList2 mac_entries_free;
    LinkedList2 mac_entries_used;
    LinkedList2 group_entries_free;
    LinkedList2 group_entries_used;
    BAVL groups_tree;
    DebugObject d_obj;
} FrameDeciderPeer;

struct _FrameDecider_mac_entry {
    FrameDeciderPeer *peer;
    LinkedList2Node list_node; // node in FrameDeciderPeer.mac_entries_free or FrameDeciderPeer.mac_entries_used
    // defined when used:
    uint8_t mac[6];
    BAVLNode tree_node; // node in FrameDecider.macs_tree, indexed by mac
};

struct _FrameDecider_group_entry {
    FrameDeciderPeer *peer;
    LinkedList2Node list_node; // node in FrameDeciderPeer.group_entries_free or FrameDeciderPeer.group_entries_used
    BTimer timer; // timer for removing the group entry, running when used
    // defined when used:
    // basic group data
    uint32_t group; // group address
    BAVLNode tree_node; // node in FrameDeciderPeer.groups_tree, indexed by group
    // all that folows is managed by add_to_multicast() and remove_from_multicast()
    LinkedList3Node sig_list_node; // node in list of group entries with the same sig
    btime_t timer_endtime;
    int is_master;
    // defined when used and we are master:
    struct {
        uint32_t sig; // last 23 bits of group address
        BAVLNode tree_node; // node in FrameDecider.multicast_tree, indexed by sig
    } master;
};

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param max_peer_macs maximum number of MAC addresses a peer may posess. Must be >0.
 * @param max_peer_groups maximum number of multicast groups a peer may belong to. Must be >0.
 * @param igmp_group_membership_interval IGMP Group Membership Interval value. When a join
 *        is detected for a peer in {@link FrameDeciderPeer_Analyze}, this is how long we wait
 *        for another join before we remove the group from the peer. Note that the group may
 *        be removed sooner if the peer fails to respond to a Group-Specific Query (see below).
 * @param igmp_last_member_query_time IGMP Last Member Query Time value. When a Group-Specific
 *        Query is detected in {@link FrameDecider_AnalyzeAndDecide}, this is how long we wait for a peer
 *        belonging to the group to send a join before we remove the group from it.
 */
void FrameDecider_Init (FrameDecider *o, int max_peer_macs, int max_peer_groups, btime_t igmp_group_membership_interval, btime_t igmp_last_member_query_time, BReactor *reactor);

/**
 * Frees the object.
 * There must be no {@link FrameDeciderPeer} objects using this decider.
 * 
 * @param o the object
 */
void FrameDecider_Free (FrameDecider *o);

/**
 * Analyzes a frame read from the local device and starts deciding which peers
 * the frame should be forwarded to.
 * 
 * @param o the object
 * @param frame frame data
 * @param frame_len frame length. Must be >=0.
 */
void FrameDecider_AnalyzeAndDecide (FrameDecider *o, const uint8_t *frame, int frame_len);

/**
 * Returns the next peer that the frame submitted to {@link FrameDecider_AnalyzeAndDecide} should be
 * forwarded to.
 * 
 * @param o the object
 * @return peer to forward the frame to, or NULL if no more
 */
FrameDeciderPeer * FrameDecider_NextDestination (FrameDecider *o);

/**
 * Initializes the object.
 * 
 * @param o the object
 * @param d decider this peer will belong to
 * @param user argument to log function
 * @param logfunc function which prepends the log prefix using {@link BLog_Append}
 * @return 1 on success, 0 on failure
 */
int FrameDeciderPeer_Init (FrameDeciderPeer *o, FrameDecider *d, void *user, BLog_logfunc logfunc) WARN_UNUSED;

/**
 * Frees the object.
 * 
 * @param o the object
 */
void FrameDeciderPeer_Free (FrameDeciderPeer *o);

/**
 * Analyzes a frame received from the peer.
 * 
 * @param o the object
 * @param frame frame data
 * @param frame_len frame length. Must be >=0.
 */
void FrameDeciderPeer_Analyze (FrameDeciderPeer *o, const uint8_t *frame, int frame_len);

#endif
