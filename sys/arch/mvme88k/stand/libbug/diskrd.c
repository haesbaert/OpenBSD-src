/*	$OpenBSD: diskrd.c,v 1.5 2013/01/05 11:20:56 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>
#include "prom.h"

/* returns 0: success, nonzero: error */
int
mvmeprom_diskrd(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;

	asm volatile ("or %%r2,%%r0,%0": : "r" (arg));
	MVMEPROM_CALL(MVMEPROM_DSKRD);
	asm volatile ("or %0,%%r0,%%r2" :  "=r" (ret));
	return ((ret & 0x8));
}
