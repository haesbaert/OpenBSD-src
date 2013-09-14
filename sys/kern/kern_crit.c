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

#include <machine/cpufunc.h>

static void	crit_rundeferred(void);

//#define CRITCOUNTERS
#ifdef CRITCOUNTERS
uint64_t defclock, defother;
#endif

/* XXX we need to make sure we have a process context soon, for now just keep
 * this counter so that we can hunt it. */
u_int crit_escaped;

void
crit_enter(void)
{
	if (curproc == NULL) {
		crit_escaped++;
		return;
	}
	curproc->p_crit++;
}


void
crit_leave(void)
{
	if (curproc == NULL) {
		crit_escaped++;
		return;
	}
	if (curproc->p_crit == 1) {
		long rf = read_rflags(); /* XXX or psl ? */
		disable_intr();

		crit_rundeferred();

		curproc->p_crit--;
		write_rflags(rf);
	} else
		curproc->p_crit--;

	KASSERT(curproc->p_crit >= 0);
}

/*
 * Must always run with interrupts disabled, ci_ipending is protect by blocking
 * interrupts.
 */
#define IPENDING_ISSET(ci, slot) (ci->ci_ipending & (1ull << slot))
#define IPENDING_CLR(ci, slot) (ci->ci_ipending &= ~(1ull << slot))
#define IPENDING_NEXT(ci, slot) (flsq(ci->ci_ipending) - 1)
static void
crit_rundeferred(void)
{
	struct cpu_info *ci = curcpu();
	int i;

	ci->ci_idepth++;
	if (IPENDING_ISSET(ci, LIR_TIMER)) {
#ifdef CRITCOUNTERS
		defclock++;
#endif
		Xfakeclock();
		IPENDING_CLR(ci, LIR_TIMER);
	}

	/* LIR_IPI dance temporary until ipi is out of IPL */
	while (ci->ci_ipending & ~(1ull << LIR_IPI)) {
#ifdef CRITCOUNTERS
		defother++;
#endif
		i = flsq(ci->ci_ipending & ~(1ull << LIR_IPI)) - 1;
		ithread_run(ci->ci_isources[i]);
		IPENDING_CLR(ci, i);
	}
	ci->ci_idepth--;
}
