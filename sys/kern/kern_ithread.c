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

#include <uvm/uvm_extern.h>

#include <machine/intr.h> 	/* XXX */

#define ITHREAD_DEBUG
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

	sched_peg_curproc(&cpu_info_primary);
	KERNEL_UNLOCK();

	DPRINTF(1, "ithread %p pin %d started\n",
	    curproc->p_pid, is->is_pin);

	for (; ;) {
		rc = 0;

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
		splx(s);
		
		if (!rc)
			printf("stray interrupt pin %d ?\n", is->is_pin);

		pic->pic_hwunmask(pic, is->is_pin);

		s = splraise(is->is_maxlevel);
		/*
		 * Since we do splx, we might have just run the handler which
		 * wakes us up, therefore, is_scheduled might be set now, in the
		 * future we should have 3 values, so that the handler could
		 * save the wakeup() but still set is_scheduled. We need to
		 * raise the IPL again so that the check for is_scheduled and
		 * the tsleep call is atomic.
		 */
		if (!is->is_scheduled) {
			tsleep(is->is_proc, PVM, "interrupt", 0);
			DPRINTF(20, "ithread %p woke up\n", curproc->p_pid);
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
ithread_handler(struct intrsource *is)
{
	struct cpu_info *ci = curcpu();
	struct proc *p = is->is_proc;
	int s;

	splassert(is->is_maxlevel);

	DPRINTF(10, "ithread accepted interrupt pin %d "
	    "(ilevel = %d, maxlevel = %d)\n",
	    is->is_pin, ci->ci_ilevel, is->is_maxlevel);

	is->is_scheduled = 1;

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
		SCHED_LOCK(s);
		setrunqueue(p);
		SCHED_UNLOCK(s);
		break;
	default:
		panic("ithread_handler: unexpected thread state %d\n", p->p_stat);
	}

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

	TAILQ_FOREACH(is, &ithreads, entry) {
		DPRINTF(1, "ithread forking intrsource pin %d\n", is->is_pin);

		if (kthread_create(ithread, is, &is->is_proc,
		    "ithread pin %d", is->is_pin))
			panic("ithread_forkall");
	}
}
