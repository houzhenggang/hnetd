/*
 * pa_pd.h
 *
 * Author: Pierre Pfister
 *
 * Prefix delegation support in prefix assignment.
 *
 * This file provides an API for reserving delegated prefixes.
 *
 */


#ifndef PA_PD_H_
#define PA_PD_H_

#include "pa_data.h"

/* General structure for pa_pd management.
 * Put in pa structure. */
struct pa_pd {
	struct list_head leases;       /* List of known leases */
	struct uloop_timeout pd_to;    /* Used to schedule leases update */
	struct pa_data_user data_user; /* Used to receive data updates */
};

/* This structure keeps track of a given delegation lease. */
struct pa_pd_lease {
	/* Called whenever some modification is made, after a short delay elapsed (about half a second)
	 * so that if multiple modifications are made in a row, it is only called once. */
	void (*update_cb)(struct pa_pd_lease *lease);

	/* Contains a list of pa_cpds (see pa_data.h).
	 * A cpd may be delegated only if cpd->cp.valid == true.
	 * It is false initially and becomes true after some time (depending on hncp speed).
	 * When a cpd becomes invalid for other reasons, it is immediately deleted
	 * and the callback is called later.
	 * Lifetimes may be found there: cpd->cp.dp->valid_lifetime (or preferred).
	 * Sometimes cpd->cp.dp may be null, but the callback will never
	 * be called in such situation (and will be called later instead). */
	struct list_head cpds;

	/****** Private *****/
	struct list_head le;        /* Linked in pa_pd structure */
	struct uloop_timeout cb_to;
};

/* Adds a new lease request. */
void pa_pd_lease_init(struct pa_pd *, struct pa_pd_lease *, uint8_t preferred_len, uint8_t max_len);

/* Terminates an existing lease. */
void pa_pd_lease_term(struct pa_pd *, struct pa_pd_lease *);

void pa_pd_init(struct pa_pd *);
void pa_pd_start(struct pa_pd *);
void pa_pd_stop(struct pa_pd *);
void pa_pd_term(struct pa_pd *);

#endif /* PA_PD_H_ */
