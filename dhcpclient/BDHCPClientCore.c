/**
 * @file BDHCPClientCore.c
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
#include <stdlib.h>

#include <misc/byteorder.h>
#include <misc/minmax.h>
#include <security/BRandom.h>
#include <system/BLog.h>

#include <dhcpclient/BDHCPClientCore.h>

#include <generated/blog_channel_BDHCPClientCore.h>

#define RESET_TIMEOUT 4000
#define REQUEST_TIMEOUT 3000
#define RENEW_REQUEST_TIMEOUT 20000
#define MAX_REQUESTS 4
#define RENEW_TIMEOUT(lease) ((btime_t)500 * (lease))
#define XID_REUSE_MAX 8

#define LEASE_TIMEOUT(lease) ((btime_t)1000 * (lease) - RENEW_TIMEOUT(lease))

#define STATE_RESETTING 1
#define STATE_SENT_DISCOVER 2
#define STATE_SENT_REQUEST 3
#define STATE_FINISHED 4
#define STATE_RENEWING 5

#define IP_UDP_HEADERS_SIZE 28

static void report_up (BDHCPClientCore *o)
{
    o->handler(o->user, BDHCPCLIENTCORE_EVENT_UP);
    return;
}

static void report_down (BDHCPClientCore *o)
{
    o->handler(o->user, BDHCPCLIENTCORE_EVENT_DOWN);
    return;
}

static void send_message (
    BDHCPClientCore *o,
    int type,
    uint32_t xid,
    int have_requested_ip_address, uint32_t requested_ip_address,
    int have_dhcp_server_identifier, uint32_t dhcp_server_identifier
)
{
    ASSERT(type == DHCP_MESSAGE_TYPE_DISCOVER || type == DHCP_MESSAGE_TYPE_REQUEST)
    
    if (o->sending) {
        BLog(BLOG_ERROR, "already sending");
        return;
    }
    
    // write header
    memset(o->send_buf, 0, sizeof(*o->send_buf));
    o->send_buf->op = hton8(DHCP_OP_BOOTREQUEST);
    o->send_buf->htype = hton8(DHCP_HARDWARE_ADDRESS_TYPE_ETHERNET);
    o->send_buf->hlen = hton8(6);
    o->send_buf->xid = xid;
    o->send_buf->secs = hton16(0);
    memcpy(o->send_buf->chaddr, o->client_mac_addr, sizeof(o->client_mac_addr));
    o->send_buf->magic = hton32(DHCP_MAGIC);
    
    // write options
    
    struct dhcp_option_header *out = (void *)(o->send_buf + 1);
    
    // DHCP message type
    out->type = hton8(DHCP_OPTION_DHCP_MESSAGE_TYPE);
    out->len = hton8(sizeof(struct dhcp_option_dhcp_message_type));
    ((struct dhcp_option_dhcp_message_type *)(out + 1))->type = hton8(type);
    out = (void *)((uint8_t *)(out + 1) + ntoh8(out->len));
    
    if (have_requested_ip_address) {
        // requested IP address
        out->type = hton8(DHCP_OPTION_REQUESTED_IP_ADDRESS);
        out->len = hton8(sizeof(struct dhcp_option_addr));
        ((struct dhcp_option_addr *)(out + 1))->addr = requested_ip_address;
        out = (void *)((uint8_t *)(out + 1) + ntoh8(out->len));
    }
    
    if (have_dhcp_server_identifier) {
        // DHCP server identifier
        out->type = hton8(DHCP_OPTION_DHCP_SERVER_IDENTIFIER);
        out->len = hton8(sizeof(struct dhcp_option_dhcp_server_identifier));
        ((struct dhcp_option_dhcp_server_identifier *)(out + 1))->id = dhcp_server_identifier;
        out = (void *)((uint8_t *)(out + 1) + ntoh8(out->len));
    }
    
    // maximum message size
    out->type = hton8(DHCP_OPTION_MAXIMUM_MESSAGE_SIZE);
    out->len = hton8(sizeof(struct dhcp_option_maximum_message_size));
    ((struct dhcp_option_maximum_message_size *)(out + 1))->size = hton16(IP_UDP_HEADERS_SIZE + PacketRecvInterface_GetMTU(o->recv_if));
    out = (void *)((uint8_t *)(out + 1) + ntoh8(out->len));
    
    // parameter request list
    out->type = hton8(DHCP_OPTION_PARAMETER_REQUEST_LIST);
    out->len = hton8(4);
    ((uint8_t *)(out + 1))[0] = DHCP_OPTION_SUBNET_MASK;
    ((uint8_t *)(out + 1))[1] = DHCP_OPTION_ROUTER;
    ((uint8_t *)(out + 1))[2] = DHCP_OPTION_DOMAIN_NAME_SERVER;
    ((uint8_t *)(out + 1))[3] = DHCP_OPTION_IP_ADDRESS_LEASE_TIME;
    out = (void *)((uint8_t *)(out + 1) + ntoh8(out->len));
    
    // end option
    *((uint8_t *)out) = 0xFF;
    out = (void *)((uint8_t *)out + 1);
    
    // send it
    PacketPassInterface_Sender_Send(o->send_if, (uint8_t *)o->send_buf, (uint8_t *)out - (uint8_t *)o->send_buf);
    o->sending = 1;
}

static void send_handler_done (BDHCPClientCore *o)
{
    ASSERT(o->sending)
    DebugObject_Access(&o->d_obj);
    
    o->sending = 0;
}

static void recv_handler_done (BDHCPClientCore *o, int data_len)
{
    ASSERT(data_len >= 0)
    DebugObject_Access(&o->d_obj);
    
    // receive more packets
    PacketRecvInterface_Receiver_Recv(o->recv_if, (uint8_t *)o->recv_buf);
    
    if (o->state == STATE_RESETTING) {
        return;
    }
    
    // check header
    
    if (data_len < sizeof(*o->recv_buf)) {
        return;
    }
    
    if (ntoh8(o->recv_buf->op) != DHCP_OP_BOOTREPLY) {
        return;
    }
    
    if (ntoh8(o->recv_buf->htype) != DHCP_HARDWARE_ADDRESS_TYPE_ETHERNET) {
        return;
    }
    
    if (ntoh8(o->recv_buf->hlen) != 6) {
        return;
    }
    
    if (o->recv_buf->xid != o->xid) {
        return;
    }
    
    if (memcmp(o->recv_buf->chaddr, o->client_mac_addr, sizeof(o->client_mac_addr))) {
        return;
    }
    
    if (ntoh32(o->recv_buf->magic) != DHCP_MAGIC) {
        return;
    }
    
    // parse and check options
    
    uint8_t *pos = (uint8_t *)o->recv_buf + sizeof(*o->recv_buf);
    int len = data_len - sizeof(*o->recv_buf);
    
    int have_end = 0;
    
    int dhcp_message_type = -1;
    
    int have_dhcp_server_identifier = 0;
    uint32_t dhcp_server_identifier;
    
    int have_ip_address_lease_time = 0;
    uint32_t ip_address_lease_time;
    
    int have_subnet_mask = 0;
    uint32_t subnet_mask;
    
    int have_router = 0;
    uint32_t router;
    
    int domain_name_servers_count = 0;
    uint32_t domain_name_servers[BDHCPCLIENTCORE_MAX_DOMAIN_NAME_SERVERS];
    
    while (len > 0) {
        // padding option ?
        if (*pos == 0) {
            pos++;
            len--;
            continue;
        }
        
        if (have_end) {
            return;
        }
        
        // end option ?
        if (*pos == 0xff) {
            pos++;
            len--;
            have_end = 1;
            continue;
        }
        
        // check option header
        if (len < sizeof(struct dhcp_option_header)) {
            return;
        }
        struct dhcp_option_header *opt = (void *)pos;
        pos += sizeof(*opt);
        len -= sizeof(*opt);
        int opt_type = ntoh8(opt->type);
        int opt_len = ntoh8(opt->len);
        
        // check option payload
        if (opt_len > len) {
            return;
        }
        void *optval = pos;
        pos += opt_len;
        len -= opt_len;
        
        switch (opt_type) {
            case DHCP_OPTION_DHCP_MESSAGE_TYPE: {
                if (opt_len != sizeof(struct dhcp_option_dhcp_message_type)) {
                    return;
                }
                struct dhcp_option_dhcp_message_type *val = optval;
                
                dhcp_message_type = ntoh8(val->type);
            } break;
            
            case DHCP_OPTION_DHCP_SERVER_IDENTIFIER: {
                if (opt_len != sizeof(struct dhcp_option_dhcp_server_identifier)) {
                    return;
                }
                struct dhcp_option_dhcp_server_identifier *val = optval;
                
                dhcp_server_identifier = val->id;
                have_dhcp_server_identifier = 1;
            } break;
            
            case DHCP_OPTION_IP_ADDRESS_LEASE_TIME: {
                if (opt_len != sizeof(struct dhcp_option_time)) {
                    return;
                }
                struct dhcp_option_time *val = optval;
                
                ip_address_lease_time = ntoh32(val->time);
                have_ip_address_lease_time = 1;
            } break;
            
            case DHCP_OPTION_SUBNET_MASK: {
                if (opt_len != sizeof(struct dhcp_option_addr)) {
                    return;
                }
                struct dhcp_option_addr *val = optval;
                
                subnet_mask = val->addr;
                have_subnet_mask = 1;
            } break;
            
            case DHCP_OPTION_ROUTER: {
                if (opt_len != sizeof(struct dhcp_option_addr)) {
                    return;
                }
                struct dhcp_option_addr *val = optval;
                
                router = val->addr;
                have_router = 1;
            } break;
            
            case DHCP_OPTION_DOMAIN_NAME_SERVER: {
                if (opt_len % 4) {
                    return;
                }
                
                int num_servers = opt_len / 4;
                
                int i;
                for (i = 0; i < num_servers && i < BDHCPCLIENTCORE_MAX_DOMAIN_NAME_SERVERS; i++) {
                    domain_name_servers[i] = ((struct dhcp_option_addr *)optval + i)->addr;
                }
                
                domain_name_servers_count = i;
            } break;
        }
    }
    
    if (!have_end) {
        return;
    }
    
    if (dhcp_message_type == -1) {
        return;
    }
    
    if (dhcp_message_type != DHCP_MESSAGE_TYPE_OFFER && dhcp_message_type != DHCP_MESSAGE_TYPE_ACK && dhcp_message_type != DHCP_MESSAGE_TYPE_NAK) {
        return;
    }
    
    if (!have_dhcp_server_identifier) {
        return;
    }
    
    if (dhcp_message_type == DHCP_MESSAGE_TYPE_NAK) {
        if (o->state != STATE_SENT_REQUEST && o->state != STATE_FINISHED && o->state != STATE_RENEWING) {
            return;
        }
        
        if (dhcp_server_identifier != o->offered.dhcp_server_identifier) {
            return;
        }
        
        if (o->state == STATE_SENT_REQUEST) {
            BLog(BLOG_INFO, "received NAK (in sent request)");
            
            // stop request timer
            BReactor_RemoveTimer(o->reactor, &o->request_timer);
            
            // start reset timer
            BReactor_SetTimer(o->reactor, &o->reset_timer);
            
            // set state
            o->state = STATE_RESETTING;
        }
        else if (o->state == STATE_FINISHED) {
            BLog(BLOG_INFO, "received NAK (in finished)");
            
            // stop renew timer
            BReactor_RemoveTimer(o->reactor, &o->renew_timer);
            
            // start reset timer
            BReactor_SetTimer(o->reactor, &o->reset_timer);
            
            // set state
            o->state = STATE_RESETTING;
            
            // report to user
            report_down(o);
            return;
        }
        else { // STATE_RENEWING
            BLog(BLOG_INFO, "received NAK (in renewing)");
            
            // stop renew request timer
            BReactor_RemoveTimer(o->reactor, &o->renew_request_timer);
            
            // stop lease timer
            BReactor_RemoveTimer(o->reactor, &o->lease_timer);
            
            // start reset timer
            BReactor_SetTimer(o->reactor, &o->reset_timer);
            
            // set state
            o->state = STATE_RESETTING;
            
            // report to user
            report_down(o);
            return;
        }
        
        return;
    }
    
    if (ntoh32(o->recv_buf->yiaddr) == 0) {
        return;
    }
    
    if (!have_ip_address_lease_time) {
        return;
    }
    
    if (!have_subnet_mask) {
        return;
    }
    
    if (o->state == STATE_SENT_DISCOVER && dhcp_message_type == DHCP_MESSAGE_TYPE_OFFER) {
        BLog(BLOG_INFO, "received OFFER");
        
        // remember offer
        o->offered.yiaddr = o->recv_buf->yiaddr;
        o->offered.dhcp_server_identifier = dhcp_server_identifier;
        
        // send request
        send_message(o, DHCP_MESSAGE_TYPE_REQUEST, o->xid, 1, o->offered.yiaddr, 1, o->offered.dhcp_server_identifier);
        
        // stop reset timer
        BReactor_RemoveTimer(o->reactor, &o->reset_timer);
        
        // start request timer
        BReactor_SetTimer(o->reactor, &o->request_timer);
        
        // set state
        o->state = STATE_SENT_REQUEST;
        
        // set request count
        o->request_count = 1;
    }
    else if (o->state == STATE_SENT_REQUEST && dhcp_message_type == DHCP_MESSAGE_TYPE_ACK) {
        if (o->recv_buf->yiaddr != o->offered.yiaddr) {
            return;
        }
        
        if (dhcp_server_identifier != o->offered.dhcp_server_identifier) {
            return;
        }
        
        BLog(BLOG_INFO, "received ACK (in sent request)");
        
        // remember stuff
        o->acked.ip_address_lease_time = ip_address_lease_time;
        o->acked.subnet_mask = subnet_mask;
        o->acked.have_router = have_router;
        if (have_router) {
            o->acked.router = router;
        }
        o->acked.domain_name_servers_count = domain_name_servers_count;
        memcpy(o->acked.domain_name_servers, domain_name_servers, domain_name_servers_count * sizeof(uint32_t));
        
        // stop request timer
        BReactor_RemoveTimer(o->reactor, &o->request_timer);
        
        // start renew timer
        BReactor_SetTimerAfter(o->reactor, &o->renew_timer, RENEW_TIMEOUT(o->acked.ip_address_lease_time));
        
        // set state
        o->state = STATE_FINISHED;
        
        // report to user
        report_up(o);
        return;
    }
    else if (o->state == STATE_RENEWING && dhcp_message_type == DHCP_MESSAGE_TYPE_ACK) {
        if (o->recv_buf->yiaddr != o->offered.yiaddr) {
            return;
        }
        
        if (dhcp_server_identifier != o->offered.dhcp_server_identifier) {
            return;
        }
        
        // TODO: check parameters?
        
        BLog(BLOG_INFO, "received ACK (in renewing)");
        
        // remember stuff
        o->acked.ip_address_lease_time = ip_address_lease_time;
        
        // stop renew request timer
        BReactor_RemoveTimer(o->reactor, &o->renew_request_timer);
        
        // stop lease timer
        BReactor_RemoveTimer(o->reactor, &o->lease_timer);
        
        // start renew timer
        BReactor_SetTimerAfter(o->reactor, &o->renew_timer, RENEW_TIMEOUT(o->acked.ip_address_lease_time));
        
        // set state
        o->state = STATE_FINISHED;
    }
}

static void start_process (BDHCPClientCore *o, int force_new_xid)
{
    if (force_new_xid || o->xid_reuse_counter == XID_REUSE_MAX) {
        // generate xid
        BRandom_randomize((uint8_t *)&o->xid, sizeof(o->xid));
        
        // reset counter
        o->xid_reuse_counter = 0;
    }
    
    // increment counter
    o->xid_reuse_counter++;
    
    // send discover
    send_message(o, DHCP_MESSAGE_TYPE_DISCOVER, o->xid, 0, 0, 0, 0);
    
    // set timer
    BReactor_SetTimer(o->reactor, &o->reset_timer);
    
    // set state
    o->state = STATE_SENT_DISCOVER;
}

static void reset_timer_handler (BDHCPClientCore *o)
{
    ASSERT(o->state == STATE_RESETTING || o->state == STATE_SENT_DISCOVER)
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_INFO, "reset timer");
    
    start_process(o, 0);
}

static void request_timer_handler (BDHCPClientCore *o)
{
    ASSERT(o->state == STATE_SENT_REQUEST)
    ASSERT(o->request_count >= 1)
    ASSERT(o->request_count <= MAX_REQUESTS)
    DebugObject_Access(&o->d_obj);
    
    // if we have sent enough requests, start again
    if (o->request_count == MAX_REQUESTS) {
        BLog(BLOG_INFO, "request timer, aborting");
        
        start_process(o, 0);
        return;
    }
    
    BLog(BLOG_INFO, "request timer, retrying");
    
    // send request
    send_message(o, DHCP_MESSAGE_TYPE_REQUEST, o->xid, 1, o->offered.yiaddr, 1, o->offered.dhcp_server_identifier);
    
    // start request timer
    BReactor_SetTimer(o->reactor, &o->request_timer);
    
    // increment request count
    o->request_count++;
}

static void renew_timer_handler (BDHCPClientCore *o)
{
    ASSERT(o->state == STATE_FINISHED)
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_INFO, "renew timer");
    
    // send request
    send_message(o, DHCP_MESSAGE_TYPE_REQUEST, o->xid, 1, o->offered.yiaddr, 0, 0);
    
    // start renew request timer
    BReactor_SetTimer(o->reactor, &o->renew_request_timer);
    
    // start lease timer
    BReactor_SetTimerAfter(o->reactor, &o->lease_timer, LEASE_TIMEOUT(o->acked.ip_address_lease_time));
    
    // set state
    o->state = STATE_RENEWING;
}

static void renew_request_timer_handler (BDHCPClientCore *o)
{
    ASSERT(o->state == STATE_RENEWING)
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_INFO, "renew request timer");
    
    // send request
    send_message(o, DHCP_MESSAGE_TYPE_REQUEST, o->xid, 1, o->offered.yiaddr, 0, 0);
    
    // start renew request timer
    BReactor_SetTimer(o->reactor, &o->renew_request_timer);
}

static void lease_timer_handler (BDHCPClientCore *o)
{
    ASSERT(o->state == STATE_RENEWING)
    DebugObject_Access(&o->d_obj);
    
    BLog(BLOG_INFO, "lease timer");
    
    // stop renew request timer
    BReactor_RemoveTimer(o->reactor, &o->renew_request_timer);
    
    // start again now
    start_process(o, 1);
    
    // report to user
    report_down(o);
    return;
}

int BDHCPClientCore_Init (BDHCPClientCore *o, PacketPassInterface *send_if, PacketRecvInterface *recv_if, uint8_t *client_mac_addr, BReactor *reactor, BDHCPClientCore_handler handler, void *user)
{
    ASSERT(PacketPassInterface_GetMTU(send_if) == PacketRecvInterface_GetMTU(recv_if))
    ASSERT(PacketPassInterface_GetMTU(send_if) >= 576 - IP_UDP_HEADERS_SIZE)
    
    // init arguments
    o->send_if = send_if;
    o->recv_if = recv_if;
    memcpy(o->client_mac_addr, client_mac_addr, sizeof(o->client_mac_addr));
    o->reactor = reactor;
    o->handler = handler;
    o->user = user;
    
    // allocate buffers
    if (!(o->send_buf = malloc(PacketPassInterface_GetMTU(send_if)))) {
        BLog(BLOG_ERROR, "malloc send buf failed");
        goto fail0;
    }
    if (!(o->recv_buf = malloc(PacketRecvInterface_GetMTU(recv_if)))) {
        BLog(BLOG_ERROR, "malloc recv buf failed");
        goto fail1;
    }
    
    // init send interface
    PacketPassInterface_Sender_Init(o->send_if, (PacketPassInterface_handler_done)send_handler_done, o);
    
    // init receive interface
    PacketRecvInterface_Receiver_Init(o->recv_if, (PacketRecvInterface_handler_done)recv_handler_done, o);
    
    // set not sending
    o->sending = 0;
    
    // init timers
    BTimer_Init(&o->reset_timer, RESET_TIMEOUT, (BTimer_handler)reset_timer_handler, o);
    BTimer_Init(&o->request_timer, REQUEST_TIMEOUT, (BTimer_handler)request_timer_handler, o);
    BTimer_Init(&o->renew_timer, 0, (BTimer_handler)renew_timer_handler, o);
    BTimer_Init(&o->renew_request_timer, RENEW_REQUEST_TIMEOUT, (BTimer_handler)renew_request_timer_handler, o);
    BTimer_Init(&o->lease_timer, 0, (BTimer_handler)lease_timer_handler, o);
    
    // start receving
    PacketRecvInterface_Receiver_Recv(o->recv_if, (uint8_t *)o->recv_buf);
    
    // start
    start_process(o, 1);
    
    DebugObject_Init(&o->d_obj);
    
    return 1;
    
fail1:
    free(o->send_buf);
fail0:
    return 0;
}

void BDHCPClientCore_Free (BDHCPClientCore *o)
{
    DebugObject_Free(&o->d_obj);
    
    // free timers
    BReactor_RemoveTimer(o->reactor, &o->lease_timer);
    BReactor_RemoveTimer(o->reactor, &o->renew_request_timer);
    BReactor_RemoveTimer(o->reactor, &o->renew_timer);
    BReactor_RemoveTimer(o->reactor, &o->request_timer);
    BReactor_RemoveTimer(o->reactor, &o->reset_timer);
    
    // free buffers
    free(o->recv_buf);
    free(o->send_buf);
}

void BDHCPClientCore_GetClientIP (BDHCPClientCore *o, uint32_t *out_ip)
{
    ASSERT(o->state == STATE_FINISHED || o->state == STATE_RENEWING)
    DebugObject_Access(&o->d_obj);
    
    *out_ip = o->offered.yiaddr;
}

void BDHCPClientCore_GetClientMask (BDHCPClientCore *o, uint32_t *out_mask)
{
    ASSERT(o->state == STATE_FINISHED || o->state == STATE_RENEWING)
    DebugObject_Access(&o->d_obj);
    
    *out_mask = o->acked.subnet_mask;
}

int BDHCPClientCore_GetRouter (BDHCPClientCore *o, uint32_t *out_router)
{
    ASSERT(o->state == STATE_FINISHED || o->state == STATE_RENEWING)
    DebugObject_Access(&o->d_obj);
    
    if (!o->acked.have_router) {
        return 0;
    }
    
    *out_router = o->acked.router;
    return 1;
}

int BDHCPClientCore_GetDNS (BDHCPClientCore *o, uint32_t *out_dns_servers, size_t max_dns_servers)
{
    ASSERT(o->state == STATE_FINISHED || o->state == STATE_RENEWING)
    DebugObject_Access(&o->d_obj);
    
    int num_return = bmin_int(o->acked.domain_name_servers_count, max_dns_servers);
    
    memcpy(out_dns_servers, o->acked.domain_name_servers, num_return * sizeof(uint32_t));
    return num_return;
}
