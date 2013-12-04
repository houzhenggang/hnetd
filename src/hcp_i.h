/*
 * $Id: hcp_i.h $
 *
 * Author: Markus Stenberg <markus stenberg@iki.fi>
 *
 * Copyright (c) 2013 cisco Systems, Inc.
 *
 * Created:       Wed Nov 20 13:56:12 2013 mstenber
 * Last modified: Wed Dec  4 11:32:38 2013 mstenber
 * Edit time:     110 min
 *
 */

#ifndef HCP_I_H
#define HCP_I_H

#include "hcp.h"

#include <libubox/uloop.h>

/* Rough approximation - should think of real figure. */
#define HCP_MAXIMUM_PAYLOAD_SIZE 65536

/* Pretty arbitrary. I wonder if all links can really guarantee MTU size
 * packets going through.. */
#define HCP_MAXIMUM_MULTICAST_SIZE 1280

#include <libubox/vlist.h>

/* in6_addr */
#include <netinet/in.h>

/* IFNAMSIZ */
#include <net/if.h>

typedef uint32_t iid_t;

struct hcp_struct {
  /* Can we assume bidirectional reachability? */
  bool assume_bidirectional_reachability;

  /* Disable pruning (should be used probably only in unit tests) */
  bool disable_prune;

  /* cached current time; if zero, should ask hcp_io for it again */
  hnetd_time_t now;

  /* nodes (as contained within the protocol, that is, raw TLV data blobs). */
  struct vlist_tree nodes;

  /* local data (TLVs API's clients want published). */
  struct vlist_tree tlvs;

  /* local links (those API's clients want active). */
  struct vlist_tree links;

  /* flag which indicates that we should re-publish links. */
  bool links_dirty;

  /* flag which indicates that we should re-publish our node in nodes. */
  bool tlvs_dirty;

  /* flag which indicates that we (or someone connected) may have
   * changed connectivity. */
  bool neighbors_dirty;
  hnetd_time_t last_prune;

  /* flag which indicates that we should re-calculate network hash
   * based on nodes' state. */
  bool network_hash_dirty;

  /* before io-init is done, we keep just prod should_schedule. */
  bool io_init_done;
  bool should_schedule;
  bool immediate_scheduled;

  /* Our own node (it should be constant, never purged) */
  hcp_node own_node;

  /* Whole network hash we consider current (based on content of 'nodes'). */
  hcp_hash_s network_hash;

  /* First free local interface identifier (we allocate them in
   * monotonically increasing fashion just to keep things simple). */
  int first_free_iid;

  /* UDP socket. */
  int udp_socket;

  /* And it's corresponding uloop_fd */
  struct uloop_fd ufd;

  /* Timeout for doing 'something' in hcp_io. */
  struct uloop_timeout timeout;

  /* Multicast address */
  struct in6_addr multicast_address;

  /* When did multicast join fail last time? */
  hnetd_time_t join_failed_time;

  /* List of subscribers to change notifications. */
  struct list_head subscribers;
};

typedef struct hcp_link_struct hcp_link_s, *hcp_link;

struct hcp_link_struct {
  struct vlist_node in_links;

  /* Backpointer to hcp */
  hcp hcp;

  /* Who are the neighbors on the link. */
  struct vlist_tree neighbors;

  /* Name of the (local) link. */
  char ifname[IFNAMSIZ];

  /* Address of the interface (_only_ used in testing) */
  struct in6_addr address;

  /* Interface identifier - these should be unique over lifetime of
   * hcp process. */
  iid_t iid;

  /* Join failed -> probably tried during DAD. Should try later again. */
  bool join_pending;

  /* Trickle state */
  int i; /* trickle interval size */
  hnetd_time_t send_time; /* when do we send if c < k*/
  hnetd_time_t interval_end_time; /* when does current interval end */
  int c; /* counter */
};

typedef struct hcp_neighbor_struct hcp_neighbor_s, *hcp_neighbor;


struct hcp_neighbor_struct {
  struct vlist_node in_neighbors;

  hcp_hash_s node_identifier_hash;
  iid_t iid;

  /* Link-level address */
  struct in6_addr last_address;

  /* When did we last hear from this one? */
  hnetd_time_t last_heard;

  /* When did they last respond to our message? */
  hnetd_time_t last_response;

  /* If proactive mode is enabled, when did we last try to ping this
   * one. */
  hnetd_time_t last_ping;
  int ping_count;
};

struct hcp_node_struct {
  /* hcp->nodes entry */
  struct vlist_node in_nodes;

  /* backpointer to hcp */
  hcp hcp;

  /* These map 1:1 to node data TLV's start */
  hcp_hash_s node_identifier_hash;
  uint32_t update_number;

  /* Node state stuff */
  hcp_hash_s node_data_hash;
  bool node_data_hash_dirty; /* Something related to hash changed */
  hnetd_time_t origination_time; /* in monotonic time */

  /* TLV data for the node. All TLV data in one binary blob, as
   * received/created. We could probably also maintain this at end of
   * the structure, but that'd mandate re-inserts whenever content
   * changes, so probably just faster to keep a pointer to it. */

  /* (We actually _do_ parse incoming TLV and create a new TLV, just
   * to make sure there's no 'bad actors' somewhere with invalid sizes
   * or whatever). */
  struct tlv_attr *tlv_container;
};

typedef struct hcp_tlv_struct hcp_tlv_s, *hcp_tlv;

struct hcp_tlv_struct {
  /* hcp->tlvs entry */
  struct vlist_node in_tlvs;

  /* Actual TLV attribute itself. */
  struct tlv_attr tlv;
};

/* Internal or testing-only way to initialize hp struct _without_
 * dynamic allocations (and some of the steps omitted too). */
bool hcp_init(hcp o, const void *node_identifier, int len);
void hcp_uninit(hcp o);

hcp_link hcp_find_link(hcp o, const char *ifname, bool create);
hcp_node hcp_find_node_by_hash(hcp o, const hcp_hash h, bool create);

/* Private utility - shouldn't be used by clients. */
bool hcp_node_set_tlvs(hcp_node n, struct tlv_attr *a);

void hcp_schedule(hcp o);

/* Flush own TLV changes to own node. */
void hcp_self_flush(hcp_node n);

/* Various hash calculation utilities. */
void hcp_calculate_hash(const void *buf, int len, hcp_hash dest);
void hcp_calculate_network_hash(hcp o);
static inline unsigned long long hcp_hash64(hcp_hash h)
{
  return *((unsigned long long *)h);
}

/* Utility functions to send frames. */
bool hcp_link_send_network_state(hcp_link l,
                                 struct in6_addr *dst,
                                 size_t maximum_size);
bool hcp_link_send_req_network_state(hcp_link l,
                                     struct in6_addr *dst);


/* Subscription stuff (hcp_notify.c) */
void hcp_notify_subscribers_tlvs_changed(hcp_node n,
                                         struct tlv_attr *a_old,
                                         struct tlv_attr *a_new);
void hcp_notify_subscribers_node_changed(hcp_node n, bool add);


/* Low-level interface module stuff. */

bool hcp_io_init(hcp o);
void hcp_io_uninit(hcp o);
bool hcp_io_set_ifname_enabled(hcp o, const char *ifname, bool enabled);
int hcp_io_get_hwaddrs(unsigned char *buf, int buf_left);
void hcp_io_schedule(hcp o, int msecs);
hnetd_time_t hcp_io_time(hcp o);

ssize_t hcp_io_recvfrom(hcp o, void *buf, size_t len,
                        char *ifname,
                        struct in6_addr *src,
                        struct in6_addr *dst);
ssize_t hcp_io_sendto(hcp o, void *buf, size_t len,
                      const char *ifname,
                      const struct in6_addr *dst);

/* Multicast rejoin utility. (in hcp.c) */
bool hcp_link_join(hcp_link l);

/* Inlined utilities. */
static inline hnetd_time_t hcp_time(hcp o)
{
  if (!o->now)
    return hcp_io_time(o);
  return o->now;
}

#define TMIN(x,y) ((x) == 0 ? (y) : (y) == 0 ? (x) : (x) < (y) ? (x) : (y))

static inline const char *hex_repr(char *buf, void *data, int len)
{
  char *r = buf;

  while (len--)
    {
      sprintf(buf, "%02X", (int) *((unsigned char *)data));
      buf += 2;
      data++;
    }
  return r;
}
#define HEX_REPR(buf, len) hex_repr(alloca((len) * 2 + 1), buf, len)

#define HCP_NODE_REPR(n) HEX_REPR(&n->node_identifier_hash, HCP_HASH_LEN)

#endif /* HCP_I_H */
