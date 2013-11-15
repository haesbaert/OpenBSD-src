/*
 * Copyright (c) 2013 Christiano F. Haesbaert <haesbaert@haesbaert.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This is heavily based on Dragonfly's objcache from Jeffrey M. Hsu.
 *
 * dcache is a lock/atomic-ops free object allocator on the fast path and
 * spinlocked on the slow path.
 *
 * Each cpu holds always 2 magazines, each magazine holds MAG_CAPACITY
 * objects. When one cpu requests an object, it looks into it's 'loaded'
 * magazine, if it's empty, it swaps its 'loaded' magazine with the 'prev'
 * magazine, if the new 'loaded' magazine is also empty, it enters the slow
 * path. In slow path the cpu exchanges its empty magazine with a full loaded
 * one from the global depot, to do it, it must acquire the depot lock. It is
 * amortized since one slow path yields another MAG_CAPACITY fast paths.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/dcache.h>

#include <uvm/uvm.h>

#define MAG_CAPACITY		256
#define MAG_EMPTY(dm)		(dm->dm_rounds == 0)
#define MAG_FULL(dm)		(dm->dm_rounds == MAG_CAPACITY)
#define MAG_POPOBJ(dm)		((dm)->dm_objs[--(dm)->dm_rounds])
#define MAG_PUTOBJ(dm, obj)	((dm)->dm_objs[(dm)->dm_rounds++] = obj)
#define	MAG_SWAP(x, y)		({ struct dcache_mag *t = (x); (x) = (y); (y) = (t); })

struct dcache_pcpu {
	struct dcache_mag *		dp_loadedmag;
	struct dcache_mag *		dp_prevmag;
	int				dp_sleeper;
};

struct dcache_mag {
	SLIST_ENTRY(dcache_mag)		dm_entry;
	int				dm_rounds;
	void *				dm_objs[MAG_CAPACITY];
};

struct dcache {
	u_int				dc_maxobjs;
	size_t				dc_objsize;
	struct mutex			dc_mtx; /* Protects emptymags and fullmags */
	int				dc_sleeper;
	SLIST_HEAD(, dcache_mag)	dc_emptymags;
	SLIST_HEAD(, dcache_mag)	dc_fullmags;
	struct dcache_pcpu		dc_pcpu[MAXCPUS];
};

void	dcache_add_mag(struct dcache *, int);
void	dcache_mag_prime(struct dcache *, struct dcache_mag *);

void
dcache_init(struct dcache *dc, size_t objsize, u_int maxobjs)
{
	struct dcache_pcpu *dp;
	int i, nemptymags, nfullmags;

#ifdef MULTIPROCESSOR
	if (ncpus <= 1)
		panic("Creating dcache before ncpu is set ?");
#endif

	bzero(dc, sizeof(*dc));

	mtx_init(&dc->dc_mtx, IPL_HIGH);
	dc->dc_objsize = objsize;
	dc->dc_maxobjs = maxobjs;

	/* We need at least these emptymags to avoid worst case */
	nemptymags = (ncpus - 1) * 2; /* may be 0 */
	while (nemptymags--)
		dcache_add_mag(dc, 1);

	/* Calculate how many full magazines we need and add them */
	nfullmags = howmany(maxobjs, MAG_CAPACITY);
	if (nfullmags < (ncpus * 2))
		nfullmags = ncpus * 2;
	while (nfullmags--)
		dcache_add_mag(dc, 0);

	/* Distribute the magazines across cpus */
	mtx_enter(&dc->dc_mtx);
	for (i = 0; i < ncpus; i++) {
		dp = &dc->dc_pcpu[i];

		dp->dp_loadedmag = SLIST_FIRST(&dc->dc_fullmags);
		SLIST_REMOVE_HEAD(&dc->dc_fullmags, dm_entry);

		dp->dp_prevmag = SLIST_FIRST(&dc->dc_emptymags);
		SLIST_REMOVE_HEAD(&dc->dc_emptymags, dm_entry);
	}
	mtx_leave(&dc->dc_mtx);
}

void
dcache_add_mag(struct dcache *dc, int empty)
{
	struct dcache_mag *dm;

	dm = malloc(sizeof(*dm), M_DEVBUF, M_WAITOK | M_ZERO);

	if (empty) {
		mtx_enter(&dc->dc_mtx);
		SLIST_INSERT_HEAD(&dc->dc_emptymags, dm, dm_entry);
		mtx_leave(&dc->dc_mtx);
		return;
	}

	dcache_mag_prime(dc, dm);
}

void
dcache_mag_prime(struct dcache *dc, struct dcache_mag *dm)
{
	char *p, *end;
	size_t sz;
	struct kmem_dyn_mode kd = KMEM_DYN_INITIALIZER;

	kd.kd_waitok = 1;

	sz = round_page(sizeof(dc->dc_objsize) * MAG_CAPACITY);
	/* KERNEL_LOCK() */
	p = km_alloc(sz, &kv_intrsafe, &kp_dma_zero, &kd);
	/* KERNEL_UNLOCK() */
	end = p + sz;

	while (p < end) {
		dm->dm_objs[dm->dm_rounds++] = p;
		p += dc->dc_objsize;

		if (dm->dm_rounds > MAG_CAPACITY)
			panic("whoops dm_rounds overflow");
	}

	mtx_enter(&dc->dc_mtx);
	SLIST_INSERT_HEAD(&dc->dc_fullmags, dm, dm_entry);
	mtx_leave(&dc->dc_mtx);
}

void *
dcache_get(struct dcache *dc, int waitok)
{
	void *obj;
	struct dcache_pcpu *dp = &dc->dc_pcpu[curcpu()->ci_cpuid];

	crit_enter();
again:

	/* 1st Fast path, we have an object in the loaded magazine */
	if (!MAG_FULL(dp->dp_loadedmag)) {
		obj = MAG_POPOBJ(dp->dp_loadedmag);
		crit_leave();
		return (obj);
	}

	/*
	 * 2nd Fast path, we have an object in the previous magazine,
	 * swap and retry
	 */
	if (!MAG_FULL(dp->dp_prevmag)) {
		MAG_SWAP(dp->dp_loadedmag, dp->dp_prevmag);
		goto again;
	}

	/*
	 * Slow path, there are no objects for us to grab, switch our empty
	 * loaded magazine with a full one from the depot
	 */

	/* Ok to race here, we recover below */
	if (!waitok && SLIST_EMPTY(&dc->dc_fullmags)) {
		crit_leave();
		return (NULL);
	}

	mtx_enter(&dc->dc_mtx);
	if (SLIST_EMPTY(&dc->dc_fullmags)) {
		dp->dp_sleeper++;
		dc->dc_sleeper++;
		msleep(dc, &dc->dc_mtx, PVM, "dacache magazine", 0);
		dc->dc_sleeper--;
		dp->dp_sleeper--;
	}
	SLIST_INSERT_HEAD(&dc->dc_emptymags, dp->dp_loadedmag, dm_entry);
	dp->dp_loadedmag = SLIST_FIRST(&dc->dc_fullmags);
	SLIST_REMOVE_HEAD(&dc->dc_fullmags, dm_entry);
	mtx_leave(&dc->dc_mtx);

	goto again;
}

void
dcache_put(struct dcache *dc, void *obj)
{
	struct dcache_pcpu *dp = &dc->dc_pcpu[curcpu()->ci_cpuid];

	crit_enter();

	/* 1st Fast path, we have a slot in the loaded mag */
	if (!MAG_FULL(dp->dp_loadedmag)) {
		MAG_PUTOBJ(dp->dp_loadedmag, obj);
		if (dp->dp_sleeper)
			wakeup(dc);
		crit_leave();
		return;
	}

	/* 2nd Fast path, we have a slot in the prev mag */
	if (!MAG_FULL(dp->dp_prevmag)) {
		MAG_PUTOBJ(dp->dp_prevmag, obj);
		MAG_SWAP(dp->dp_loadedmag, dp->dp_prevmag);
		if (dp->dp_sleeper)
			wakeup(dc);
		crit_leave();
		return;
	}

	crit_leave();

	/* Slow path, we need an empty mag from the depot */
	mtx_enter(&dc->dc_mtx);
	if (SLIST_EMPTY(&dc->dc_emptymags))
		panic("No more emptymags, this can never happen, wrong math!");
	SLIST_INSERT_HEAD(&dc->dc_fullmags, dp->dp_loadedmag, dm_entry);
	dp->dp_loadedmag = SLIST_FIRST(&dc->dc_emptymags);
	SLIST_REMOVE_HEAD(&dc->dc_emptymags, dm_entry);
	if (dc->dc_sleeper)
		wakeup(dc);
	mtx_leave(&dc->dc_mtx);
}
