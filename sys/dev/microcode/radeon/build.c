/*	$OpenBSD$	*/

/*-
 * Copyright (c) 2006
 *	Damien Bergamini <damien.bergamini@free.fr>
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

#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/* old license */
#include "r100_cp.h"
#include "r200_cp.h"
#include "r300_cp.h"
#include "r420_cp.h"
#include "r520_cp.h"
#include "r600_me.h"
#include "r600_pfp.h"
#include "rs600_cp.h"
#include "rs690_cp.h"
#include "rs780_me.h"
#include "rs780_pfp.h"
#include "rv610_me.h"
#include "rv610_pfp.h"
#include "rv620_me.h"
#include "rv620_pfp.h"
#include "rv630_me.h"
#include "rv630_pfp.h"
#include "rv635_me.h"
#include "rv635_pfp.h"
#include "rv670_me.h"
#include "rv670_pfp.h"
#include "rv710_me.h"
#include "rv710_pfp.h"
#include "rv730_me.h"
#include "rv730_pfp.h"
#include "rv770_me.h"
#include "rv770_pfp.h"

/* new license */
#include "aruba_me.h"
#include "aruba_pfp.h"
#include "aruba_rlc.h"
#include "barts_mc.h"
#include "barts_me.h"
#include "barts_pfp.h"
#include "btc_rlc.h"
#include "caicos_mc.h"
#include "caicos_me.h"
#include "caicos_pfp.h"
#include "cayman_mc.h"
#include "cayman_me.h"
#include "cayman_pfp.h"
#include "cayman_rlc.h"
#include "cedar_me.h"
#include "cedar_pfp.h"
#include "cedar_rlc.h"
#include "cypress_me.h"
#include "cypress_pfp.h"
#include "cypress_rlc.h"
#include "cypress_uvd.h"
#include "hainan_ce.h"
#include "hainan_mc.h"
#include "hainan_me.h"
#include "hainan_pfp.h"
#include "hainan_rlc.h"
#include "juniper_me.h"
#include "juniper_pfp.h"
#include "juniper_rlc.h"
#include "oland_ce.h"
#include "oland_mc.h"
#include "oland_me.h"
#include "oland_pfp.h"
#include "oland_rlc.h"
#include "palm_me.h"
#include "palm_pfp.h"
#include "pitcairn_ce.h"
#include "pitcairn_mc.h"
#include "pitcairn_me.h"
#include "pitcairn_pfp.h"
#include "pitcairn_rlc.h"
#include "r600_rlc.h"
#include "r700_rlc.h"
#include "redwood_me.h"
#include "redwood_pfp.h"
#include "redwood_rlc.h"
#include "rv710_uvd.h"
#include "sumo2_me.h"
#include "sumo2_pfp.h"
#include "sumo_me.h"
#include "sumo_pfp.h"
#include "sumo_rlc.h"
#include "sumo_uvd.h"
#include "tahiti_ce.h"
#include "tahiti_mc.h"
#include "tahiti_me.h"
#include "tahiti_pfp.h"
#include "tahiti_rlc.h"
#include "tahiti_uvd.h"
#include "turks_mc.h"
#include "turks_me.h"
#include "turks_pfp.h"
#include "verde_ce.h"
#include "verde_mc.h"
#include "verde_me.h"
#include "verde_pfp.h"
#include "verde_rlc.h"

static void
output(const char *name, const uint8_t *ucode, int size)
{
	ssize_t rlen;
	int fd;

	printf("creating %s length %d\n", name, size);

	fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
		err(1, "%s", name);

	rlen = write(fd, ucode, size);
	if (rlen == -1)
		err(1, "%s", name);
	if (rlen != size)
		errx(1, "%s: short write", name);

	close(fd);
}

int
main(void)
{
	/* old license */
	output("radeon-r100_cp", r100_cp, sizeof r100_cp);
	output("radeon-r200_cp", r200_cp, sizeof r200_cp);
	output("radeon-r300_cp", r300_cp, sizeof r300_cp);
	output("radeon-r420_cp", r420_cp, sizeof r420_cp);
	output("radeon-r520_cp", r520_cp, sizeof r520_cp);
	output("radeon-r600_me", r600_me, sizeof r600_me);
	output("radeon-r600_pfp", r600_pfp, sizeof r600_pfp);
	output("radeon-rs600_cp", rs600_cp, sizeof rs600_cp);
	output("radeon-rs690_cp", rs690_cp, sizeof rs690_cp);
	output("radeon-rs780_me", rs780_me, sizeof rs780_me);
	output("radeon-rs780_pfp", rs780_pfp, sizeof rs780_pfp);
	output("radeon-rv610_me", rv610_me, sizeof rv610_me);
	output("radeon-rv610_pfp", rv610_pfp, sizeof rv610_pfp);
	output("radeon-rv620_me", rv620_me, sizeof rv620_me);
	output("radeon-rv620_pfp", rv620_pfp, sizeof rv620_pfp);
	output("radeon-rv630_me", rv630_me, sizeof rv630_me);
	output("radeon-rv630_pfp", rv630_pfp, sizeof rv630_pfp);
	output("radeon-rv635_me", rv635_me, sizeof rv635_me);
	output("radeon-rv635_pfp", rv635_pfp, sizeof rv635_pfp);
	output("radeon-rv670_me", rv670_me, sizeof rv670_me);
	output("radeon-rv670_pfp", rv670_pfp, sizeof rv670_pfp);
	output("radeon-rv710_me", rv710_me, sizeof rv710_me);
	output("radeon-rv710_pfp", rv710_pfp, sizeof rv710_pfp);
	output("radeon-rv730_me", rv730_me, sizeof rv730_me);
	output("radeon-rv730_pfp", rv730_pfp, sizeof rv730_pfp);
	output("radeon-rv770_me", rv770_me, sizeof rv770_me);
	output("radeon-rv770_pfp", rv770_pfp, sizeof rv770_pfp);

	/* new license */
	output("radeon-aruba_me", aruba_me, sizeof aruba_me);
	output("radeon-aruba_pfp", aruba_pfp, sizeof aruba_pfp);
	output("radeon-aruba_rlc", aruba_rlc, sizeof aruba_rlc);
	output("radeon-barts_mc", barts_mc, sizeof barts_mc);
	output("radeon-barts_me", barts_me, sizeof barts_me);
	output("radeon-barts_pfp", barts_pfp, sizeof barts_pfp);
	output("radeon-btc_rlc", btc_rlc, sizeof btc_rlc);
	output("radeon-caicos_mc", caicos_mc, sizeof caicos_mc);
	output("radeon-caicos_me", caicos_me, sizeof caicos_me);
	output("radeon-caicos_pfp", caicos_pfp, sizeof caicos_pfp);
	output("radeon-cayman_mc", cayman_mc, sizeof cayman_mc);
	output("radeon-cayman_me", cayman_me, sizeof cayman_me);
	output("radeon-cayman_pfp", cayman_pfp, sizeof cayman_pfp);
	output("radeon-cayman_rlc", cayman_rlc, sizeof cayman_rlc);
	output("radeon-cedar_me", cedar_me, sizeof cedar_me);
	output("radeon-cedar_pfp", cedar_pfp, sizeof cedar_pfp);
	output("radeon-cedar_rlc", cedar_rlc, sizeof cedar_rlc);
	output("radeon-cypress_me", cypress_me, sizeof cypress_me);
	output("radeon-cypress_pfp", cypress_pfp, sizeof cypress_pfp);
	output("radeon-cypress_rlc", cypress_rlc, sizeof cypress_rlc);
	output("radeon-cypress_uvd", cypress_uvd, sizeof cypress_uvd);
	output("radeon-hainan_ce", hainan_ce, sizeof hainan_ce);
	output("radeon-hainan_mc", hainan_mc, sizeof hainan_mc);
	output("radeon-hainan_me", hainan_me, sizeof hainan_me);
	output("radeon-hainan_pfp", hainan_pfp, sizeof hainan_pfp);
	output("radeon-hainan_rlc", hainan_rlc, sizeof hainan_rlc);
	output("radeon-juniper_me", juniper_me, sizeof juniper_me);
	output("radeon-juniper_pfp", juniper_pfp, sizeof juniper_pfp);
	output("radeon-juniper_rlc", juniper_rlc, sizeof juniper_rlc);
	output("radeon-oland_ce", oland_ce, sizeof oland_ce);
	output("radeon-oland_mc", oland_mc, sizeof oland_mc);
	output("radeon-oland_me", oland_me, sizeof oland_me);
	output("radeon-oland_pfp", oland_pfp, sizeof oland_pfp);
	output("radeon-oland_rlc", oland_rlc, sizeof oland_rlc);
	output("radeon-palm_me", palm_me, sizeof palm_me);
	output("radeon-palm_pfp", palm_pfp, sizeof palm_pfp);
	output("radeon-pitcairn_ce", pitcairn_ce, sizeof pitcairn_ce);
	output("radeon-pitcairn_mc", pitcairn_mc, sizeof pitcairn_mc);
	output("radeon-pitcairn_me", pitcairn_me, sizeof pitcairn_me);
	output("radeon-pitcairn_pfp", pitcairn_pfp, sizeof pitcairn_pfp);
	output("radeon-pitcairn_rlc", pitcairn_rlc, sizeof pitcairn_rlc);
	output("radeon-r600_rlc", r600_rlc, sizeof r600_rlc);
	output("radeon-r700_rlc", r700_rlc, sizeof r700_rlc);
	output("radeon-redwood_me", redwood_me, sizeof redwood_me);
	output("radeon-redwood_pfp", redwood_pfp, sizeof redwood_pfp);
	output("radeon-redwood_rlc", redwood_rlc, sizeof redwood_rlc);
	output("radeon-rv710_uvd", rv710_uvd, sizeof rv710_uvd);
	output("radeon-sumo2_me", sumo2_me, sizeof sumo2_me);
	output("radeon-sumo2_pfp", sumo2_pfp, sizeof sumo2_pfp);
	output("radeon-sumo_me", sumo_me, sizeof sumo_me);
	output("radeon-sumo_pfp", sumo_pfp, sizeof sumo_pfp);
	output("radeon-sumo_rlc", sumo_rlc, sizeof sumo_rlc);
	output("radeon-sumo_uvd", sumo_uvd, sizeof sumo_uvd);
	output("radeon-tahiti_ce", tahiti_ce, sizeof tahiti_ce);
	output("radeon-tahiti_mc", tahiti_mc, sizeof tahiti_mc);
	output("radeon-tahiti_me", tahiti_me, sizeof tahiti_me);
	output("radeon-tahiti_pfp", tahiti_pfp, sizeof tahiti_pfp);
	output("radeon-tahiti_rlc", tahiti_rlc, sizeof tahiti_rlc);
	output("radeon-tahiti_uvd", tahiti_uvd, sizeof tahiti_uvd);
	output("radeon-turks_mc", turks_mc, sizeof turks_mc);
	output("radeon-turks_me", turks_me, sizeof turks_me);
	output("radeon-turks_pfp", turks_pfp, sizeof turks_pfp);
	output("radeon-verde_ce", verde_ce, sizeof verde_ce);
	output("radeon-verde_mc", verde_mc, sizeof verde_mc);
	output("radeon-verde_me", verde_me, sizeof verde_me);
	output("radeon-verde_pfp", verde_pfp, sizeof verde_pfp);
	output("radeon-verde_rlc", verde_rlc, sizeof verde_rlc);

	return 0;
}
