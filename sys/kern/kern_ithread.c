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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ithread.h>
#include <sys/kthread.h>
#include <sys/queue.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/intr.h>	/* XXX */

//#define ITHREAD_DEBUG
#ifdef ITHREAD_DEBUG
int ithread_debug = 10;
#define DPRINTF(l, x...)	do { if ((l) <= ithread_debug) printf(x); } while (0)
#else
#define DPRINTF(l, x...)
#endif	/* ITHREAD_DEBUG */

TAILQ_HEAD(, intrsource) ithreads = TAILQ_HEAD_INITIALIZER(ithreads);

void
ithread(void *v_is)
{
	struct intrsource *is = v_is;
	struct pic *pic = is->is_pic;
	struct intrhand *ih;
	int rc, s;

	KASSERT(curproc == is->is_proc);
	sched_peg_curproc(&cpu_info_primary);
	KERNEL_UNLOCK();

	DPRINTF(1, "ithread %u pin %d started\n",
	    curproc->p_pid, is->is_pin);

	for (; ;) {
		rc = 0;

		/* XXX */
		KASSERT(CRIT_DEPTH == 0);
		crit_enter();
		
		if (is->is_maxlevel > IPL_CRIT)
			s = splraise(is->is_maxlevel);

		for (ih = is->is_handlers; ih != NULL; ih = ih->ih_next) {
			is->is_scheduled = 0; /* protected by is->is_maxlevel */

			if ((ih->ih_flags & IPL_MPSAFE) == 0)
				KERNEL_LOCK();

			KASSERT(ih->ih_level <= is->is_maxlevel);
			rc |= (*ih->ih_fun)(ih->ih_arg);

			if ((ih->ih_flags & IPL_MPSAFE) == 0)
				KERNEL_UNLOCK();
		}

		/* XXX */
		if (is->is_maxlevel > IPL_CRIT)
			splx(s);
			
		crit_leave();
		KASSERT(CRIT_DEPTH == 0);

		if (!rc)
			printf("stray interrupt pin %d ?\n", is->is_pin);

		pic->pic_hwunmask(pic, is->is_pin);

		/*
		 * Since we do splx, we might have just run the handler which
		 * wakes us up, therefore, is_scheduled might be set now, in the
		 * future we should have 3 values, so that the handler could
		 * save the wakeup() but still set is_scheduled. We need to
		 * raise the IPL again so that the check for is_scheduled and
		 * the tsleep call is atomic.
		 */
		if (!is->is_scheduled) {
			ithread_sleep(is);
			DPRINTF(20, "ithread %u woke up\n", curproc->p_pid);
		}
		splx(s);
	}
}


/*
 * We're called with interrupts disabled, so no need to protect anything. This
 * is the code that gets called when get a real interrupt, it's sole purpose is
 * to schedule the interrupt source to run.
 * XXX intr_shared_edge is not being used, and we're being pessimistic.
 */
int
ithread_run(struct intrsource *is)
{
#ifdef ITHREAD_DEBUG
	struct cpu_info *ci = curcpu();
#endif
	struct proc *p = is->is_proc;
	int s;

	crit_enter();
	if (p == NULL) {
		is->is_scheduled = 1;
		DPRINTF(1, "received interrupt pin %d before ithread is ready",
		    is->is_pin);
		crit_leave();
		return (0);
	}

	splassert(is->is_maxlevel);

	DPRINTF(10, "ithread accepted interrupt pin %d "
	    "(ilevel = %d, maxlevel = %d)\n",
	    is->is_pin, ci->ci_ilevel, is->is_maxlevel);

	is->is_scheduled = 1;

	SCHED_LOCK(s);
	switch (p->p_stat) {
	case SRUN:
	case SONPROC:
		break;
	case SSLEEP:
		unsleep(p);
		p->p_stat = SRUN;
		p->p_slptime = 0;
		/*
		 * Setting the thread to runnable is cheaper than a normal
		 * process since the process state can be protected by blocking
		 * interrupts. There is also no need to choose a cpu since we're
		 * pinned. XXX we're not there yet and still rely on normal
		 * SCHED_LOCK crap.
		 */
		setrunqueue(p);
		break;
	default:
		SCHED_UNLOCK(s);
		panic("ithread_handler: unexpected thread state %d\n", p->p_stat);
	}
	SCHED_UNLOCK(s);
	crit_leave();

	return (0);
}

void
ithread_register(struct intrsource *is)
{
	struct intrsource *is2;

	/* Prevent multiple inclusion */
	TAILQ_FOREACH(is2, &ithreads, entry) {
		if (is2 == is)
			return;
	}

	DPRINTF(1, "ithread_register intrsource pin %d\n", is->is_pin);

	TAILQ_INSERT_TAIL(&ithreads, is, entry);
}

void
ithread_deregister(struct intrsource *is)
{
	DPRINTF(1, "ithread_deregister intrsource pin %d\n", is->is_pin);

	TAILQ_REMOVE(&ithreads, is, entry);
}

void
ithread_forkall(void)
{
	struct intrsource *is;
	static int softs;

	TAILQ_FOREACH(is, &ithreads, entry) {
		DPRINTF(1, "ithread forking intrsource pin %d\n", is->is_pin);

		if (is->is_pic == &softintr_pic) {
			if (kthread_create(ithread, is, &is->is_proc,
			    "ithread soft %d", softs++))
				panic("ithread_forkall");
		} else {
			if (kthread_create(ithread, is, &is->is_proc,
			    "ithread pin %d", is->is_pin))
				panic("ithread_forkall");
		}
	}
}

/*
 * Generic painfully slow soft interrupts, this is a temporary implementation to
 * allow us to kill the IPL subsystem and remove all "interrupts" from the
 * system. In the future we'll have real message passing for remote scheduling
 * and one softint per cpu when applicable. These soft threads interlock through
 * SCHED_LOCK, which is totally unacceptable in the future.
 */
struct intrsource *
ithread_softregister(int level, int (*handler)(void *), void *arg, int flags)
{
	struct intrsource *is;
	struct intrhand *ih;

	is = malloc(sizeof(*is), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (is == NULL)
		panic("ithread_softregister");

	is->is_type = IST_LEVEL; /* XXX more like level than EDGE */
	is->is_pic = &softintr_pic;
	is->is_minlevel = IPL_HIGH;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ih == NULL)
		panic("ithread_softregister");

	ih->ih_fun = handler;
	ih->ih_arg = arg;
	ih->ih_level = level;
	ih->ih_flags = flags;
	ih->ih_pin = 0;
	ih->ih_cpu = &cpu_info_primary;

	/* Just prepend it */
	ih->ih_next = is->is_handlers;
	is->is_handlers = ih;

	if (ih->ih_level > is->is_maxlevel)
		is->is_maxlevel = ih->ih_level;
	if (ih->ih_level < is->is_minlevel) /* XXX minlevel will be gone */
		is->is_minlevel = ih->ih_level;

	ithread_register(is);

	return (is);
}

void
ithread_softderegister(struct intrsource *is)
{
	if (!cold)
		panic("ithread_softderegister only works on cold case");

	ithread_deregister(is);
	free(is, M_DEVBUF);
}

/* XXX kern_synch.c, temporary */
#define TABLESIZE	128
#define LOOKUP(x)	(((long)(x) >> 8) & (TABLESIZE - 1))
extern TAILQ_HEAD(slpque,proc) slpque[TABLESIZE];

void
ithread_sleep(struct intrsource *is)
{
	struct proc *p = is->is_proc;
	int s;

	KASSERT(curproc == p);
	KASSERT(p->p_stat == SONPROC);

	SCHED_LOCK(s);
	if (!is->is_scheduled) {
		p->p_wchan = p;
		p->p_wmesg = "softintr";
		p->p_slptime = 0;
		p->p_priority = PVM & PRIMASK;
		TAILQ_INSERT_TAIL(&slpque[LOOKUP(p)], p, p_runq);
		p->p_stat = SSLEEP;
		mi_switch();
	}
	SCHED_UNLOCK(s);
}
