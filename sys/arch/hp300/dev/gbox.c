/*	$OpenBSD: gbox.c,v 1.6 2005/01/21 16:22:34 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: grf_gb.c 1.18 93/08/13$
 *
 *	@(#)grf_gb.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics routines for the Gatorbox.
 *
 * Note: In the context of this system, "gator" and "gatorbox" both refer to
 *       HP 987x0 graphics systems.  "Gator" is not used for high res mono.
 *       (as in 9837 Gator systems)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/ioctl.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/intiovar.h>

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <hp300/dev/diofbreg.h>
#include <hp300/dev/diofbvar.h>
#include <hp300/dev/gboxreg.h>

struct	gbox_softc {
	struct device	sc_dev;
	struct diofb	*sc_fb;
	struct diofb	sc_fb_store;
	int		sc_scode;
};

int	gbox_dio_match(struct device *, void *, void *);
void	gbox_dio_attach(struct device *, struct device *, void *);
int	gbox_intio_match(struct device *, void *, void *);
void	gbox_intio_attach(struct device *, struct device *, void *);

struct cfattach gbox_dio_ca = {
	sizeof(struct gbox_softc), gbox_dio_match, gbox_dio_attach
};

struct cfattach gbox_intio_ca = {
	sizeof(struct gbox_softc), gbox_intio_match, gbox_intio_attach
};

struct cfdriver gbox_cd = {
	NULL, "gbox", DV_DULL
};

int	gbox_reset(struct diofb *, int, struct diofbreg *);
void	gbox_windowmove(struct diofb *, u_int16_t, u_int16_t,
	     u_int16_t, u_int16_t, u_int16_t, u_int16_t, int);

int	gbox_ioctl(void *, u_long, caddr_t, int, struct proc *);
void	gbox_burner(void *, u_int, u_int);

struct	wsdisplay_accessops	gbox_accessops = {
	gbox_ioctl,
	diofb_mmap,
	diofb_alloc_screen,
	diofb_free_screen,
	diofb_show_screen,
	NULL,   /* load_font */
	NULL,   /* scrollback */
	NULL,   /* getchar */
	gbox_burner
};

/*
 * Attachment glue
 */
int
gbox_intio_match(struct device *parent, void *match, void *aux)
{
	struct intio_attach_args *ia = aux;
	struct diofbreg *fbr;

	fbr = (struct diofbreg *)IIOV(GRFIADDR);

	if (badaddr((caddr_t)fbr))
		return (0);

	if (fbr->id == GRFHWID && fbr->id2 == GID_GATORBOX) {
		ia->ia_addr = (caddr_t)GRFIADDR;
		return (1);
	}

	return (0);
}

void
gbox_intio_attach(struct device *parent, struct device *self, void *aux)
{
	struct gbox_softc *sc = (struct gbox_softc *)self;
	struct diofbreg *fbr;

	fbr = (struct diofbreg *)IIOV(GRFIADDR);
	sc->sc_scode = -1;	/* XXX internal i/o */

	if (sc->sc_scode == conscode) {
		sc->sc_fb = &diofb_cn;
	} else {
		sc->sc_fb = &sc->sc_fb_store;
		gbox_reset(sc->sc_fb, sc->sc_scode, fbr);
	}

	diofb_end_attach(sc, &gbox_accessops, sc->sc_fb,
	    sc->sc_scode == conscode, 0, NULL);
}

int
gbox_dio_match(struct device *parent, void *match, void *aux)
{
	struct dio_attach_args *da = aux;

	/* We can not appear in DIO-II space */
	if (DIO_ISDIOII(da->da_scode))
		return (0);

	if (da->da_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    da->da_secid == DIO_DEVICE_SECID_GATORBOX)
		return (1);

	return (0);
}

void
gbox_dio_attach(struct device *parent, struct device *self, void *aux)
{
	struct gbox_softc *sc = (struct gbox_softc *)self;
	struct dio_attach_args *da = aux;
	struct diofbreg * fbr;

	sc->sc_scode = da->da_scode;
	if (sc->sc_scode == conscode) {
		fbr = (struct diofbreg *)conaddr;	/* already mapped */
		sc->sc_fb = &diofb_cn;
	} else {
		sc->sc_fb = &sc->sc_fb_store;
		fbr = (struct diofbreg *)
		    iomap(dio_scodetopa(sc->sc_scode), da->da_size);
		if (fbr == NULL ||
		    gbox_reset(sc->sc_fb, sc->sc_scode, fbr) != 0) {
			printf(": can't map framebuffer\n");
			return;
		}
	}

	diofb_end_attach(sc, &gbox_accessops, sc->sc_fb,
	    sc->sc_scode == conscode, 0, NULL);
}

/*
 * Initialize hardware and display routines.
 */

const u_int8_t crtc_init_data[] = {
    0x29, 0x20, 0x23, 0x04, 0x30, 0x0b, 0x30,
    0x30, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00
};

int
gbox_reset(struct diofb *fb, int scode, struct diofbreg *fbr)
{
	volatile struct gboxfb *gb = (struct gboxfb *)fbr;
	u_int8_t *fbp, save;
	int rc;
	int i;

	/* XXX don't trust hardware, force defaults */
	fb->fbwidth = 1024;
	fb->fbheight = 1024;
	fb->dwidth = 1024;
	fb->dheight = 768;
	if ((rc = diofb_fbinquire(fb, scode, fbr)) != 0)
		return (rc);

	/*
	 * The minimal info here is from the Gatorbox X driver.
	 */
	gb->write_protect = 0x0;
	gb->interrupt = 0x4;
	gb->rep_rule = RR_COPY;
	gb->blink1 = 0xff;
	gb->blink2 = 0xff;

	/*
	 * Program the 6845.
	 */
	for (i = 0; i < sizeof(crtc_init_data); i++) {
		gb->crtc_address = i;
		gb->crtc_data = crtc_init_data[i];
	}

	/*
	 * Find out how many colors are available by determining
	 * which planes are installed.  That is, write all ones to
	 * a frame buffer location, see how many ones are read back.
	 */
	fbp = (u_int8_t *)fb->fbkva;
	save = *fbp;
	*fbp = 0x0ff;
	fb->planemask = *fbp;
	*fbp = save;
	for (fb->planes = 1; fb->planemask >= (1 << fb->planes);
	    fb->planes++);

	/*
	 * Set up the color map entries. We use three entries in the
	 * color map. The first, is for black, the second is for
	 * white, and the very last entry is for the inverted cursor.
	 */
	gb->creg_select = 0x00;
	gb->cmap_red    = 0x00;
	gb->cmap_grn    = 0x00;
	gb->cmap_blu    = 0x00;
	gb->cmap_write  = 0x00;
	gbcm_waitbusy(gb);

	gb->creg_select = 0x01;
	gb->cmap_red    = 0xFF;
	gb->cmap_grn    = 0xFF;
	gb->cmap_blu    = 0xFF;
	gb->cmap_write  = 0x01;
	gbcm_waitbusy(gb);

	/* XXX is the cursors entry necessary??? */
	gb->creg_select = 0xFF;
	gb->cmap_red    = 0xFF;
	gb->cmap_grn    = 0xFF;
	gb->cmap_blu    = 0xFF;
	gb->cmap_write  = 0x01;
	gbcm_waitbusy(gb);

	fb->bmv = gbox_windowmove;
	diofb_fbsetup(fb);
	diofb_fontunpack(fb);

	tile_mover_waitbusy(gb);

	/*
	 * Enable display.
	 */
	gb->sec_interrupt = 0x01;

	return (0);
}

int
gbox_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct diofb *fb = v;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_GBOX;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = fb->dheight;
		wdf->width = fb->dwidth;
		wdf->depth = fb->planes;
		wdf->cmsize = 1 << fb->planes;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = fb->fbwidth;
		break;
	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
		/* XXX TBD */
		break;
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;
	default:
		return (-1);
	}

	return (0);
}

void
gbox_burner(void *v, u_int on, u_int flags)
{
	struct diofb *fb = v;
	volatile struct gboxfb *gb = (struct gboxfb *)fb->regkva;

	if (on)
		gb->sec_interrupt = 0x01;
	else
		gb->sec_interrupt = 0x00;
}

void
gbox_windowmove(struct diofb *fb, u_int16_t sx, u_int16_t sy,
    u_int16_t dx, u_int16_t dy, u_int16_t cx, u_int16_t cy, int rop)
{
	volatile struct gboxfb *gb = (struct gboxfb *)fb->regkva;
	int src, dest;

	src  = (sy * 1024) + sx; /* upper left corner in pixels */
	dest = (dy * 1024) + dx;

	tile_mover_waitbusy(gb);
	gb->width = -(cx / 4);
	gb->height = -(cy / 4);
	if (src < dest)
		gb->rep_rule = MOVE_DOWN_RIGHT | rop;
	else {
		gb->rep_rule = MOVE_UP_LEFT | rop;
		/*
		 * Adjust to top of lower right tile of the block.
		 */
		src = src + ((cy - 4) * 1024) + (cx - 4);
		dest= dest + ((cy - 4) * 1024) + (cx - 4);
	}
	FBBASE(fb)[dest] = FBBASE(fb)[src];
}

/*
 * Gatorbox console support
 */

int gbox_console_scan(int, caddr_t, void *);
cons_decl(gbox);

int
gbox_console_scan(int scode, caddr_t va, void *arg)
{
	struct diofbreg *fbr = (struct diofbreg *)va;
	struct consdev *cp = arg;
	int force = 0, pri;

	if (fbr->id != GRFHWID || fbr->id2 != GID_GATORBOX)
		return (0);

	pri = CN_NORMAL;

#ifdef CONSCODE
	/*
	 * Raise our priority, if appropriate.
	 */
	if (scode == CONSCODE) {
		pri = CN_REMOTE;
		force = conforced = 1;
	}
#endif

	/* Only raise priority. */
	if (pri > cp->cn_pri)
		cp->cn_pri = pri;

	/*
	 * If our priority is higher than the currently-remembered
	 * console, stash our priority.
	 */
	if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri)) || force) {
		cn_tab = cp;
		return (DIO_SIZE(scode, va));
	}
	return (0);
}

void
gboxcnprobe(struct consdev *cp)
{
	int maj;
	caddr_t va;
	struct diofbreg *fbr;
	int force = 0;

	/* Abort early if console already forced. */
	if (conforced)
		return;

	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;
	}

	if (maj == nchrdev)
		return;

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_DEAD;

	/* Look for "internal" framebuffer. */
	va = (caddr_t)IIOV(GRFIADDR);
	fbr = (struct diofbreg *)va;
	if (!badaddr(va) &&
	    fbr->id == GRFHWID && fbr->id2 == GID_GATORBOX) {
		cp->cn_pri = CN_INTERNAL;

#ifdef CONSCODE
		if (CONSCODE == -1) {
			force = conforced = 1;
		}
#endif

		/*
		 * If our priority is higher than the currently
		 * remembered console, stash our priority, and
		 * unmap whichever device might be currently mapped.
		 * Since we're internal, we set the saved size to 0
		 * so they don't attempt to unmap our fixed VA later.
		 */
		if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri))
		    || force) {
			cn_tab = cp;
			if (convasize)
				iounmap(conaddr, convasize);
			conscode = -1;
			conaddr = va;
			convasize = 0;
		}
	}

	console_scan(gbox_console_scan, cp, HP300_BUS_DIO);
}

void
gboxcninit(cp)
	struct consdev *cp;
{
	long defattr;

	gbox_reset(&diofb_cn, conscode, (struct diofbreg *)conaddr);
	diofb_alloc_attr(NULL, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&diofb_cn.wsd, &diofb_cn, 0, 0, defattr);
}
