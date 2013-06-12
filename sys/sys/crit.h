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

#ifndef _SYS_CRIT_H
#define _SYS_CRIT_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

/* XXX do we need compiler barriers around static inline stuff ? */
/* XXX we need to make sure we have a process context soon, for now just keep
 * this counter so that we can hunt it. */
u_int crit_escaped;

static inline void
crit_enter(void)
{
	if (curproc == NULL) {
		crit_escaped++;
		return;
	}
	curproc->p_crit++;
}

static inline void
crit_leave(void)
{
	if (curproc == NULL) {
		crit_escaped++;
		return;
	}
	curproc->p_crit--;
	KASSERT(curproc->p_crit >= 0);
}

static inline int
crit_inside(void)
{
	if (curproc == NULL) {
		crit_escaped++;
		return (0);
	}
	KASSERT(curproc);
	return (curproc->p_crit);
}

#endif	/* _SYS_CRIT_H */
