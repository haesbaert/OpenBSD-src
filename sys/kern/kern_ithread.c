#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ithread.h>
#include <sys/kthread.h>

#include <uvm/uvm_extern.h>

#include <machine/intr.h> 	/* XXX */

extern int hz;			/* XXX temporary */
/*
 * ITHREADS
 */

#define ITHREAD_DEBUG
#ifdef ITHREAD_DEBUG
int ithread_debug = 10;
#define DPRINTF(l, x...)	do { if ((l) <= ithread_debug) printf(x); } while (0)
#else
#define DPRINTF(l, x...)
#endif	/* ITHREAD_DEBUG */

void
ithread(void *v_is)
{
	struct intrsource *is = v_is;
	struct pic *pic = is->is_pic;
	struct intrhand *ih;
	int rc, s;

	sched_peg_curproc(&cpu_info_primary);
	KERNEL_UNLOCK();

	DPRINTF(1, "cpu %d ithread %p pin %d started\n",
	    curcpu()->ci_cpuid, curproc->p_pid, is->is_pin);

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
			printf("stray interrupt ?\n");

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
			tsleep(is->is_proc, PVM, "interrupt", hz);
			DPRINTF(20, "ithread %p woke up\n", curproc->p_pid);
		}
		/* XXX remove hz when we're sure we have no races */
		splx(s);
	}
}


/* XXX intr_shared_edge totalmente ignorado */
/*
 * We're called with interrupts disable, so no need to protect anything. This is
 * the code that gets called when get a real interrupt, it's sole purpose is to
 * schedule the interrupt source to run. So no need to do any ci_ilevel or
 * ci_depth wankery, if we want to run normal handlers in this stub, then it is
 * different.
 */
int
ithread_handler(int num)
{
	struct cpu_info *ci = curcpu();
	struct intrsource *is = ci->ci_isources[num];
	struct pic *pic;

	DPRINTF(10, "cpu %d ithread %u got interrupt %d\n",
	    ci->ci_cpuid, curproc->p_pid, num);

	if (is == NULL) /* XXX make it fatal for now */
		panic("stray interrupt %d\n", num);

	pic = is->is_pic;

	/* See if we can take this interrupt */
	if (ci->ci_ilevel >= is->is_maxlevel) {	/* Can't take it, >= or > ? */
		DPRINTF(10, "cpu %d ithread %u can't take interrupt %d "
		    "(ilevel = %d, maxlevel = %d)\n",
		    ci->ci_cpuid, curproc->p_pid, ci->ci_ilevel, is->is_maxlevel);
		/* Set pending interrupt vector */
		ci->ci_ipending |= num;
		/* Leave ioapic masked, probably unecessary */
		pic->pic_hwmask(pic, is->is_pin);
		/* Acknowledge lapic EOI, we'll keep the ioapic pin masked  */
		lapic_ackeoi();

		return (0);
	}

	DPRINTF(10, "cpu %d ithread %u accepted interrupt %d "
	    "(ilevel = %d, maxlevel = %d)\n",
	    ci->ci_cpuid, curproc->p_pid, num, ci->ci_ilevel, is->is_maxlevel);

	/* We're taking it */
	uvmexp.intrs++;		/* XXX per cpu */

	/* Acknowledge lapic EOI, we'll keep the ioapic pin masked  */
	lapic_ackeoi();

	/* Leave ioapic masked, probably unecessary */
	pic->pic_hwmask(pic, is->is_pin);

	is->is_scheduled = 1;
	wakeup(is->is_proc);

	return (0);
}

void
ithread_create(struct intrsource *is)
{
	struct intrhand *ih;

	/* Make sure all handlers are marked as IPL_THREAD */
	for (ih = is->is_handlers; ih != NULL; ih = ih->ih_next)
		KASSERT(ih->ih_flags & IPL_THREAD);

	if (is->is_proc)
		return;

	DPRINTF(1, "ithread_create for cpu %d pin %d\n",
	    curcpu()->ci_cpuid, is->is_pin);

	kthread_create_deferred(ithread_create2, is);
}

void
ithread_create2(void *v_is)
{
	struct intrsource *is = v_is;

	DPRINTF(1, "ithread_create2 for cpu %d pin %d\n",
	    curcpu()->ci_cpuid, is->is_pin);

	if (kthread_create(ithread, is, &is->is_proc,
	    "ithread%d", is->is_idtvec))
		panic("ithread create2");
}
