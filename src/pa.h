/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 * Prefix and address assignment element. It contains all
 * prefix and address assignment sub components.
 *
 */


#ifndef PA_H_
#define PA_H_

#include "hnetd.h"

#include <libubox/uloop.h>
#include <stdint.h>

#include "hncp.h"

#include "pa_core.h"
#include "pa_data.h"
#include "pa_local.h"
#include "pa_store.h"
#include "pa_pd.h"

struct pa;

#include "iface.h"

#define PA_PRIORITY_MIN              0
#define PA_PRIORITY_AUTHORITY_MIN    4
#define PA_PRIORITY_AUTO_MIN         6
#define PA_PRIORITY_DEFAULT          8
#define PA_PRIORITY_SCARCITY         9
#define PA_PRIORITY_PD               10
#define PA_PRIORITY_AUTO_MAX         10
#define PA_PRIORITY_AUTHORITY_MAX    12
#define PA_PRIORITY_MAX              15

struct pa_conf {
	struct pa_data_conf data_conf;
	struct pa_local_conf local_conf;
	struct pa_pd_conf pd_conf;
};

struct pa {
	bool started;
	struct pa_core core;                  /* Algorithm core elements */
	struct pa_data data;                  /* PAA database */
	struct pa_conf conf;                  /* Configuration */
	struct pa_local local;                /* Ipv4 and ULA elements */
	struct pa_store store;                /* Stable storage interface */
	struct pa_pd pd;                      /* Prefix delegation support */
	struct iface_user ifu;
	hncp hncp;                            /* hncp instance */
};

#define pa_data(pa) (&(pa)->data)

#define pa_set_hncp(pa, hncp_o) (pa)->hncp = hncp_o

void pa_conf_set_defaults(struct pa_conf *conf);
/* Initializes the pa structure. */
void pa_init(struct pa *pa, const struct pa_conf *conf);
/* Start the pa algorithm. */
void pa_start(struct pa *pa);
/* Pause the pa alforithm (In a possibly wrong state). */
void pa_stop(struct pa *pa);
/* Reset pa to post-init state, without modifying configuration. */
void pa_term(struct pa *pa);


/* Generic functions used by different sub-part of pa */

/* Check if the proposed prefix collides with any other used prefix (local or distant).
 * In case of collision, the colliding prefix is returned. */
const struct prefix *pa_prefix_getcollision(struct pa *pa, const struct prefix *prefix);
bool pa_addr_available(struct pa *pa, struct pa_iface *iface, const struct in6_addr *addr);

/* Check if some other router's assignment is colliding with the chosen prefix.
 * It is assumed that collisions can't happen with other cps. */
bool pa_cp_isvalid(struct pa *pa, struct pa_cp *cp);

/* Check if another router's assignment is correct with respect to _other_ router's asisgnments.
 * Local cps are not checked. */
bool pa_ap_isvalid(struct pa *pa, struct pa_ap *ap);

int pa_precedence(bool auth1, uint8_t prio1, struct pa_rid *rid1,
		bool auth2, uint8_t prio2, struct pa_rid *rid2);
int pa_precedence_apap(struct pa_ap *ap1, struct pa_ap *ap2);
int pa_precedence_apcp(struct pa_ap *ap, struct pa_cp *cp);

/* Counts the number of available prefixes
 * prefix_count must be an array of length 129 */
void pa_count_available_prefixes(struct pa *pa, uint16_t *count, struct prefix *container);
void pa_count_available_decrement(uint16_t *count, uint8_t removed_plen, uint8_t container_plen);

/* Returns the smallest prefix length so that at least nmax prefixes of length plen are available.
 * If less than nmax prefixes are available, the smallest available prefix length is returned (Or plen + 1 if no prefix is available).
 * *nfound provides the number of available prefixes (at most nmax). */
uint8_t pa_count_available_subset(const uint16_t *count, uint8_t plen, uint32_t *nfound, uint32_t nmax);

#endif /* PA_H_ */
