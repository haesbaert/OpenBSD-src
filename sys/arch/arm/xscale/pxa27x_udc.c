/*	$OpenBSD: pxa27x_udc.c,v 1.9 2007/02/12 15:09:03 drahn Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbf.h>
#include <dev/usb/usbfvar.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <arm/xscale/pxa27x_udcreg.h>
#define PXAUDC_EP0MAXP	16	/* XXX */
#define PXAUDC_NEP	24	/* total number of endpoints */

#include <machine/zaurus_reg.h>	/* XXX */

#include "usbf.h"

struct pxaudc_xfer {
	struct usbf_xfer	 xfer;
	u_int16_t		 frmlen;
};

struct pxaudc_pipe {
	struct usbf_pipe	 pipe;
//	LIST_ENTRY(pxaudc_pipe)	 list;
};

struct pxaudc_softc {
	struct usbf_bus		 sc_bus;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	bus_size_t		 sc_size;
	void			*sc_ih;
	void			*sc_conn_ih;
	void 			*sc_powerhook;
	SIMPLEQ_HEAD(,usbf_xfer) sc_free_xfers;	/* recycled xfers */
	u_int32_t		 sc_icr0;	/* enabled EP interrupts */
	u_int32_t		 sc_icr1;	/* enabled EP interrupts */
	enum {
		EP0_SETUP,
		EP0_IN
	}			 sc_ep0state;
	u_int32_t		 sc_isr0;	/* XXX deferred interrupts */
	u_int32_t		 sc_isr1;	/* XXX deferred interrupts */
	u_int32_t		 sc_otgisr;	/* XXX deferred interrupts */
	struct pxaudc_pipe	*sc_pipe[PXAUDC_NEP];
	int			 sc_npipe;
};

int		 pxaudc_match(struct device *, void *, void *);
void		 pxaudc_attach(struct device *, struct device *, void *);
int		 pxaudc_detach(struct device *, int);
void		 pxaudc_power(int, void *);

int		 pxaudc_is_host(void);
int		 pxaudc_is_device(void);
void		 pxaudc_setup(struct pxaudc_softc *);
void		 pxaudc_hide(struct pxaudc_softc *);
void		 pxaudc_show(struct pxaudc_softc *);

void		 pxaudc_enable(struct pxaudc_softc *);
void		 pxaudc_disable(struct pxaudc_softc *);
void		 pxaudc_read_ep0(struct pxaudc_softc *, usbf_xfer_handle);
void		 pxaudc_write_ep0(struct pxaudc_softc *, usbf_xfer_handle);
void		 pxaudc_write(struct pxaudc_softc *, usbf_xfer_handle);

int		 pxaudc_connect_intr(void *);
int		 pxaudc_intr(void *);
void		 pxaudc_intr1(struct pxaudc_softc *);
void		 pxaudc_ep0_intr(struct pxaudc_softc *);

usbf_status	 pxaudc_open(struct usbf_pipe *);
void		 pxaudc_softintr(void *);
usbf_status	 pxaudc_allocm(struct usbf_bus *, usb_dma_t *, u_int32_t);
void		 pxaudc_freem(struct usbf_bus *, usb_dma_t *);
usbf_xfer_handle pxaudc_allocx(struct usbf_bus *);
void		 pxaudc_freex(struct usbf_bus *, usbf_xfer_handle);

usbf_status	 pxaudc_ctrl_transfer(usbf_xfer_handle);
usbf_status	 pxaudc_ctrl_start(usbf_xfer_handle);
void		 pxaudc_ctrl_abort(usbf_xfer_handle);
void		 pxaudc_ctrl_done(usbf_xfer_handle);
void		 pxaudc_ctrl_close(usbf_pipe_handle);

usbf_status	 pxaudc_bulk_transfer(usbf_xfer_handle);
usbf_status	 pxaudc_bulk_start(usbf_xfer_handle);
void		 pxaudc_bulk_abort(usbf_xfer_handle);
void		 pxaudc_bulk_done(usbf_xfer_handle);
void		 pxaudc_bulk_close(usbf_pipe_handle);

struct cfattach pxaudc_ca = {
	sizeof(struct pxaudc_softc), pxaudc_match, pxaudc_attach,
	pxaudc_detach
};

struct cfdriver pxaudc_cd = {
	NULL, "pxaudc", DV_DULL
};

#if NUSBF > 0

struct usbf_bus_methods pxaudc_bus_methods = {
	pxaudc_open,
	pxaudc_softintr,
	pxaudc_allocm,
	pxaudc_freem,
	pxaudc_allocx,
	pxaudc_freex
};

struct usbf_pipe_methods pxaudc_ctrl_methods = {
	pxaudc_ctrl_transfer,
	pxaudc_ctrl_start,
	pxaudc_ctrl_abort,
	pxaudc_ctrl_done,
	pxaudc_ctrl_close
};

struct usbf_pipe_methods pxaudc_bulk_methods = {
	pxaudc_bulk_transfer,
	pxaudc_bulk_start,
	pxaudc_bulk_abort,
	pxaudc_bulk_done,
	pxaudc_bulk_close
};

#endif /* NUSBF > 0 */

#define DEVNAME(sc)	USBDEVNAME((sc)->sc_bus.bdev)

#define CSR_READ_4(sc, reg) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define CSR_WRITE_4(sc, reg, val) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define CSR_WRITE_1(sc, reg, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define CSR_SET_4(sc, reg, val) \
	CSR_WRITE_4((sc), (reg), CSR_READ_4((sc), (reg)) | (val))
#define CSR_CLR_4(sc, reg, val) \
	CSR_WRITE_4((sc), (reg), CSR_READ_4((sc), (reg)) & ~(val))

#ifndef PXAUDC_DEBUG
#define DPRINTF(l, x)	do {} while (0)
#else
int pxaudcdebug = 0;
#define DPRINTF(l, x)	if ((l) <= pxaudcdebug) printf x; else {}
#endif

int
pxaudc_match(struct device *parent, void *match, void *aux)
{
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) != CPU_ID_PXA27X)
		return (0);

	return (1);
}

void
pxaudc_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxaudc_softc		*sc = (struct pxaudc_softc *)self;
	struct pxaip_attach_args	*pxa = aux;

	sc->sc_iot = pxa->pxa_iot;
	if (bus_space_map(sc->sc_iot, PXA2X0_USBDC_BASE, PXA2X0_USBDC_SIZE, 0,
	    &sc->sc_ioh)) {
		printf(": cannot map mem space\n");
		return;
	}
	sc->sc_size = PXA2X0_USBDC_SIZE;

	printf(": USB Device Controller\n");

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, sc->sc_size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	/* Set up GPIO pins and disable the controller. */
	pxaudc_setup(sc);
	pxaudc_disable(sc);

#if NUSBF > 0
	/* Establish USB device interrupt. */
	sc->sc_ih = pxa2x0_intr_establish(PXA2X0_INT_USB, IPL_USB,
	    pxaudc_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
		return;
	}

	/* Establish device connect interrupt. */
#if 0
	sc->sc_conn_ih = pxa2x0_gpio_intr_establish(C3000_USB_DEVICE_PIN, /* XXX */
	    IST_EDGE_BOTH, IPL_USB, pxaudc_connect_intr, sc, "usbc");
#endif
	sc->sc_conn_ih = pxa2x0_gpio_intr_establish(C3000_USB_CONNECT_PIN,
	    IST_EDGE_BOTH, IPL_USB, pxaudc_connect_intr, sc, "usbc");
	if (sc->sc_conn_ih == NULL) {
		printf(": unable to establish connect interrupt\n");
		pxa2x0_intr_disestablish(sc->sc_ih);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_ioh = NULL;
		sc->sc_size = 0;
		return;
	}

	sc->sc_powerhook = powerhook_establish(pxaudc_power, sc);

	/* Set up the bus struct. */
	sc->sc_bus.methods = &pxaudc_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct pxaudc_pipe);
	sc->sc_bus.ep0_maxp = PXAUDC_EP0MAXP;
	sc->sc_bus.usbrev = USBREV_1_1;
	sc->sc_bus.dmatag = pxa->pxa_dmat;
	sc->sc_npipe = 0;	/* ep0 is always there. */

	/* Attach logical device and function. */
	(void)config_found(self, &sc->sc_bus, NULL);

	/* Enable the controller unless we're now acting as a host. */
	if (!pxaudc_is_host())
		pxaudc_enable(sc);
#endif
}

int
pxaudc_detach(struct device *self, int flags)
{
	struct pxaudc_softc		*sc = (struct pxaudc_softc *)self;

	if (sc->sc_powerhook != NULL)
		powerhook_disestablish(sc->sc_powerhook);

	if (sc->sc_conn_ih != NULL)
		pxa2x0_gpio_intr_disestablish(sc->sc_conn_ih);

	if (sc->sc_ih != NULL)
		pxa2x0_intr_disestablish(sc->sc_ih);

	if (sc->sc_size) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
	}

	return (0);
}

void
pxaudc_power(int why, void *arg)
{
	struct pxaudc_softc		*sc = (struct pxaudc_softc *)arg;

	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		pxaudc_disable(sc);
		break;

	case PWR_RESUME:
		pxaudc_enable(sc);
		break;
	}
}

/*
 * Machine-specific functions
 */

/* XXX move to machine-specific file */

int
pxaudc_is_host(void)
{
	return (!pxa2x0_gpio_get_bit(C3000_USB_CONNECT_PIN) &&
		!pxa2x0_gpio_get_bit(C3000_USB_DEVICE_PIN));
}

int
pxaudc_is_device(void)
{
	return (pxa2x0_gpio_get_bit(C3000_USB_CONNECT_PIN) &&
		pxa2x0_gpio_get_bit(C3000_USB_DEVICE_PIN));
}

void
pxaudc_setup(struct pxaudc_softc *sc)
{
	pxa2x0_gpio_set_function(45, GPIO_OUT);
	pxa2x0_gpio_set_function(C3000_USB_CONNECT_PIN, GPIO_IN); /* 41 */
	pxa2x0_gpio_set_function(40, GPIO_OUT);
	pxa2x0_gpio_set_function(39, GPIO_IN);
	pxa2x0_gpio_set_function(38, GPIO_IN);
	pxa2x0_gpio_set_function(37, GPIO_OUT);
	pxa2x0_gpio_set_function(36, GPIO_IN);
	pxa2x0_gpio_set_function(C3000_USB_DEVICE_PIN, GPIO_IN); /* 35 */
	pxa2x0_gpio_set_function(34, GPIO_IN);
	pxa2x0_gpio_set_function(89, GPIO_OUT);
	pxa2x0_gpio_set_function(120, GPIO_OUT);
}

/* Hide us from the host. */
void
pxaudc_hide(struct pxaudc_softc *sc)
{
	pxa2x0_gpio_clear_bit(C3000_USB_PULLUP_PIN);
}

/* Show us to the host. */
void
pxaudc_show(struct pxaudc_softc *sc)
{
	pxa2x0_gpio_set_bit(C3000_USB_PULLUP_PIN);
}

/*
 * Register manipulation
 */

#if 0
static void
pxaudc_dump_regs(struct pxaudc_softc *sc)
{
	printf("UDCCR\t%b\n", CSR_READ_4(sc, USBDC_UDCCR),
	    USBDC_UDCCR_BITS);
	printf("UDCICR0\t%b\n", CSR_READ_4(sc, USBDC_UDCICR0),
	    USBDC_UDCISR0_BITS);
	printf("UDCICR1\t%b\n", CSR_READ_4(sc, USBDC_UDCICR1),
	    USBDC_UDCISR1_BITS);
	printf("OTGICR\t%b\n", CSR_READ_4(sc, USBDC_UDCOTGICR),
	    USBDC_UDCOTGISR_BITS);
}
#endif

void
pxaudc_enable(struct pxaudc_softc *sc)
{
	int i;

	DPRINTF(0,("pxaudc_enable\n"));

	/* Start the clocks. */
	pxa2x0_clkman_config(CKEN_USBDC, 1);

#if 0
	/* Configure Port 2 for USB device. */
	CSR_WRITE_4(sc, USBDC_UP2OCR, USBDC_UP2OCR_DMPUE |
	    USBDC_UP2OCR_DPPUE | USBDC_UP2OCR_HXOE);
#else
	/* Configure Port 2 for USB device. */
	CSR_WRITE_4(sc, USBDC_UP2OCR, USBDC_UP2OCR_DPPUE | USBDC_UP2OCR_HXOE);
#endif

	CSR_SET_4(sc, USBDC_UDCCR, 0);
	sc->sc_icr0 = 0;
	sc->sc_icr1 = 0;

	for (i = 1; i < PXAUDC_NEP; i++) 
		CSR_WRITE_4(sc, USBDC_UDCECR(i), 0); /* disable endpoints */

	for (i = 1; i < sc->sc_npipe; i++) {
		if (sc->sc_pipe[i] != NULL)  {
			struct usbf_endpoint *ep =
			    sc->sc_pipe[i]->pipe.endpoint;
			u_int32_t cr;
			int dir = usbf_endpoint_dir(ep);
			usb_endpoint_descriptor_t *ed = ep->edesc;

			if (i < 16)
				sc->sc_icr0 |= USBDC_UDCICR0_IE(i);
			else
				sc->sc_icr1 |= USBDC_UDCICR1_IE(i-16);

			printf("configuring pipe/ep %x %x\n", i,
			    UE_GET_ADDR(ed->bEndpointAddress));

			cr = USBDC_UDCECR_EE | USBDC_UDCECR_DE;
			cr |= USBDC_UDCECR_ENs(
			    UE_GET_ADDR(ed->bEndpointAddress));
			cr |= USBDC_UDCECR_MPSs(UGETW(ed->wMaxPacketSize));
			cr |= USBDC_UDCECR_ETs(ed->bmAttributes & UE_XFERTYPE);
			if (dir == UE_DIR_IN)
				cr |= USBDC_UDCECR_ED;

			/* XXX - until pipe has cn/in/ain */
			cr |=   USBDC_UDCECR_AISNs(0) | USBDC_UDCECR_INs(0) |
			    USBDC_UDCECR_CNs(1);

			CSR_WRITE_4(sc, USBDC_UDCECR(i), cr);
			printf("endpoint %c programed to %x\n", '@'+i, cr);

			/* clear old status */
			CSR_WRITE_4(sc, USBDC_UDCCSR(1), 
			    USBDC_UDCCSR_PC | USBDC_UDCCSR_TRN |
			    USBDC_UDCCSR_SST | USBDC_UDCCSR_FEF);
			printf("csr%d %x\n", i, CSR_READ_4(sc,  USBDC_UDCCSR(1)));
		}
	}

	CSR_WRITE_4(sc, USBDC_UDCISR0, 0xffffffff); /* clear all */
	CSR_WRITE_4(sc, USBDC_UDCISR1, 0xffffffff); /* clear all */
	CSR_SET_4(sc, USBDC_UDCCSR0, USBDC_UDCCSR0_ACM);


	/* Enable interrupts for configured endpoints. */
	CSR_WRITE_4(sc, USBDC_UDCICR0, USBDC_UDCICR0_IE(0) |
	    sc->sc_icr0);
	printf("icr0 %x\n", CSR_READ_4(sc,  USBDC_UDCICR0));
	CSR_WRITE_4(sc, USBDC_UDCICR1, USBDC_UDCICR1_IERS |
	    USBDC_UDCICR1_IESU | USBDC_UDCICR1_IERU |
	    USBDC_UDCICR1_IECC | sc->sc_icr1);
	printf("icr1 %x\n", CSR_READ_4(sc,  USBDC_UDCICR1));

	/* Enable the controller. */
	CSR_CLR_4(sc, USBDC_UDCCR, USBDC_UDCCR_EMCE);
	CSR_SET_4(sc, USBDC_UDCCR, USBDC_UDCCR_UDE);

	/* Enable USB client on port 2. */
	pxa2x0_gpio_clear_bit(37); /* USB_P2_8 */
}

void
pxaudc_disable(struct pxaudc_softc *sc)
{
	DPRINTF(0,("pxaudc_disable\n"));

	/* Disable the controller. */
	CSR_CLR_4(sc, USBDC_UDCCR, USBDC_UDCCR_UDE);

	/* Disable all interrupts. */
	CSR_WRITE_4(sc, USBDC_UDCICR0, 0);
	CSR_WRITE_4(sc, USBDC_UDCICR1, 0);
	CSR_WRITE_4(sc, USBDC_UDCOTGICR, 0);

	/* Set Port 2 output to "Non-OTG Host with Differential Port". */
	CSR_WRITE_4(sc, USBDC_UP2OCR, USBDC_UP2OCR_HXS | USBDC_UP2OCR_HXOE);

	/* Set "Host Port 2 Transceiver D� Pull Down Enable". */
	CSR_SET_4(sc, USBDC_UP2OCR, USBDC_UP2OCR_DMPDE);

	/* Stop the clocks. */
	pxa2x0_clkman_config(CKEN_USBDC, 0);

	/* Enable USB host on port 2. */
	pxa2x0_gpio_set_bit(37); /* USB_P2_8 */
}

#if NUSBF > 0

/*
 * Endpoint FIFO handling
 */

void
pxaudc_read_ep0(struct pxaudc_softc *sc, usbf_xfer_handle xfer)
{
	size_t len;
	u_int8_t *p;

	xfer->actlen = CSR_READ_4(sc, USBDC_UDCBCR(0));
	len = MIN(xfer->actlen, xfer->length);
	p = xfer->buffer;

	while (CSR_READ_4(sc, USBDC_UDCCSR0) & USBDC_UDCCSR0_RNE) {
		u_int32_t v = CSR_READ_4(sc, USBDC_UDCDR(0));

		if (len > 0) {
			if (((unsigned)p & 0x3) == 0)
				*(u_int32_t *)p = v;
			else {
				*(p+0) = v & 0xff;
				*(p+1) = (v >> 8) & 0xff;
				*(p+2) = (v >> 16) & 0xff;
				*(p+3) = (v >> 24) & 0xff;
			}
			p += 4;
			len -= 4;
		}
	}

	CSR_WRITE_4(sc, USBDC_UDCCSR0, USBDC_UDCCSR0_SA | USBDC_UDCCSR0_OPC);

	xfer->status = USBF_NORMAL_COMPLETION;
	usbf_transfer_complete(xfer);
}

void
pxaudc_write_ep0(struct pxaudc_softc *sc, usbf_xfer_handle xfer)
{
	struct pxaudc_xfer *lxfer = (struct pxaudc_xfer *)xfer;
	u_int32_t len;
	u_int8_t *p;

	if (lxfer->frmlen > 0) {
		xfer->actlen += lxfer->frmlen;
		lxfer->frmlen = 0;
	}

	DPRINTF(1,("%s: ep0 ctrl-in, xfer=%p, len=%u, actlen=%u\n",
	    DEVNAME(sc), xfer, xfer->length, xfer->actlen));

	if (xfer->actlen >= xfer->length) {
		sc->sc_ep0state = EP0_SETUP;
		usbf_transfer_complete(xfer);
		return;
	}

	sc->sc_ep0state = EP0_IN;

	p = (u_char *)xfer->buffer + xfer->actlen;
	len = xfer->length - xfer->actlen;
	len = MIN(len, PXAUDC_EP0MAXP);
	lxfer->frmlen = len;

	while (len >= 4) {
		u_int32_t v;

		if (((unsigned)p & 0x3) == 0)
			v = *(u_int32_t *)p;
		else {
			v = *(p+0);
			v |= *(p+1) << 8;
			v |= *(p+2) << 16;
			v |= *(p+3) << 24;
		}

		CSR_WRITE_4(sc, USBDC_UDCDR(0), v);
		len -= 4;
		p += 4;
	}

	while (len > 0) {
		CSR_WRITE_1(sc, USBDC_UDCDR(0), *p);
		len--;
		p++;
	}

	/* (12.6.7) Set IPR only for short packets. */
	if (lxfer->frmlen < PXAUDC_EP0MAXP)
		CSR_SET_4(sc, USBDC_UDCCSR0, USBDC_UDCCSR0_IPR);
}

void
pxaudc_write(struct pxaudc_softc *sc, usbf_xfer_handle xfer)
{
	printf("pxaudc_write: XXX\n");
}

/*
 * Interrupt handling
 */

int
pxaudc_connect_intr(void *v)
{
	struct pxaudc_softc *sc = v;

	DPRINTF(0,("pxaudc_connect_intr: connect=%d device=%d\n",
	    pxa2x0_gpio_get_bit(C3000_USB_CONNECT_PIN),
	    pxa2x0_gpio_get_bit(C3000_USB_DEVICE_PIN)));

	/* XXX only set a flag here */
	if (pxaudc_is_host()) {
		pxaudc_disable(sc);
	} else {
		pxaudc_enable(sc);
	}

	/* Claim this interrupt. */
	return 1;
}

int
pxaudc_intr(void *v)
{
	struct pxaudc_softc *sc = v;
	u_int32_t isr0, isr1, otgisr;

	isr0 = CSR_READ_4(sc, USBDC_UDCISR0);
	isr1 = CSR_READ_4(sc, USBDC_UDCISR1);
	otgisr = CSR_READ_4(sc, USBDC_UDCOTGISR);

	DPRINTF(0,("pxaudc_intr: isr0=%b, isr1=%b, otgisr=%b\n",
	    isr0, USBDC_UDCISR0_BITS, isr1, USBDC_UDCISR1_BITS,
	    otgisr, USBDC_UDCOTGISR_BITS));

	if (isr0 || isr1 || otgisr) {
		sc->sc_isr0 |= isr0;
		sc->sc_isr1 |= isr1;
		sc->sc_otgisr |= otgisr;

		//usbf_schedsoftintr(&sc->sc_bus);
		pxaudc_intr1(sc); /* XXX */
	}

	CSR_WRITE_4(sc, USBDC_UDCISR0, isr0);
	CSR_WRITE_4(sc, USBDC_UDCISR1, isr1);
	CSR_WRITE_4(sc, USBDC_UDCOTGISR, otgisr);

	/* Claim this interrupt. */
	return 1;
}
u_int32_t csr1, csr2;

void
pxaudc_intr1(struct pxaudc_softc *sc)
{
	u_int32_t isr0, isr1, otgisr;
	//int s;

	//s = splhardusb();
	isr0 = sc->sc_isr0;
	isr1 = sc->sc_isr1;
	otgisr = sc->sc_otgisr;
	sc->sc_isr0 = 0;
	sc->sc_isr1 = 0;
	sc->sc_otgisr = 0;
	//splx(s);

	sc->sc_bus.intr_context++;

	if (isr0 & USBDC_UDCISR0_IR(1)) {
		printf("interrupt pending ep[1]\n");
	}
	{
		int i;
		u_int32_t csr;
                csr = CSR_READ_4(sc, USBDC_UDCCSR(1));
		if (csr1 != csr) {
			printf("CSR1 %x\n", csr);
			csr1 = csr;
		}
                csr = CSR_READ_4(sc, USBDC_UDCCSR(2));
		if (csr2 != csr) {
			printf("CSR1 %x\n", csr);
			csr2 = csr;
		 }
		 for (i = 1; i < 23; i++) {
			int x;
			x = CSR_READ_4(sc, USBDC_UDCBCR(i));
			if( x != 0)
				printf("data present in ep %d %d\n", i, x);
		 }
	}
	if (isr0 & USBDC_UDCISR0_IR(2)) {
		printf("interrupt pending ep[2]\n");
	}
	/* Handle USB RESET condition. */
	if (isr1 & USBDC_UDCISR1_IRRS) {
		sc->sc_ep0state = EP0_SETUP;
		usbf_host_reset(&sc->sc_bus);
		/* Discard all other interrupts. */
		goto ret;
	}

	/* Service control pipe interrupts. */
	if (isr0 & USBDC_UDCISR0_IR(0))
		pxaudc_ep0_intr(sc);

	if (isr1 & USBDC_UDCISR1_IRSU) {
		/* suspend ?? */
	}
	if (isr1 & USBDC_UDCISR1_IRRU) {
		/* resume ?? */
	}
	if (isr1 & USBDC_UDCISR1_IRCC) {
                u_int32_t ccr;
                ccr = CSR_READ_4(sc, USBDC_UDCCR);
 
                printf("config change isr %x %x ccr0 %b\n acn %x ain %x aaisn %x\n",
                    isr0, isr1,
			ccr, USBDC_UDCCR_BITS,
                        (ccr >> 11)  & 7,
                        (ccr >> 8)  & 7,
                        (ccr >> 5)  & 7);
                CSR_SET_4(sc, USBDC_UDCCR, USBDC_UDCCR_SMAC);

		/* wait for reconfig to finish (SMAC auto clears */
		while (CSR_READ_4(sc, USBDC_UDCCR)  & USBDC_UDCCR_SMAC)
			delay(10);

                ccr = CSR_READ_4(sc, USBDC_UDCCR);
 
                printf("after config change isr %x %x acn %x ain %x aaisn %x ccr0 %x\n",
                    isr0, isr1,
                        (ccr >> 11)  & 7,
                        (ccr >> 8)  & 7,
                        (ccr >> 5)  & 7, ccr);
	}

ret:
	sc->sc_bus.intr_context--;
}

void
pxaudc_ep0_intr(struct pxaudc_softc *sc)
{
	struct pxaudc_pipe *ppipe;
	usbf_pipe_handle pipe = NULL;
	usbf_xfer_handle xfer = NULL;
	u_int32_t csr0;

	csr0 = CSR_READ_4(sc, USBDC_UDCCSR0);
	DPRINTF(0,("pxaudc_ep0_intr: csr0=%b\n", csr0, USBDC_UDCCSR0_BITS));

	ppipe = sc->sc_pipe[0];
	if (ppipe != NULL) {
		pipe = &ppipe->pipe;
		xfer = SIMPLEQ_FIRST(&pipe->queue);
	}

	if (sc->sc_ep0state == EP0_SETUP && (csr0 & USBDC_UDCCSR0_OPC)) {
		if (pipe == NULL) {
			DPRINTF(0,("pxaudc_ep0_intr: no control pipe\n"));
			return;
		}

		if (xfer == NULL) {
			DPRINTF(0,("pxaudc_ep0_intr: no xfer\n"));
			return;
		}

		pxaudc_read_ep0(sc, xfer);
	} else if (sc->sc_ep0state == EP0_IN &&
	    (csr0 & USBDC_UDCCSR0_IPR) == 0 && xfer) {
		pxaudc_write_ep0(sc, xfer);
	}
}

/*
 * Bus methods
 */

usbf_status
pxaudc_open(struct usbf_pipe *pipe)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;
	struct pxaudc_pipe *ppipe = (struct pxaudc_pipe *)pipe;
	int s;

	if (usbf_endpoint_index(pipe->endpoint) >= PXAUDC_NEP)
		return USBF_BAD_ADDRESS;

	DPRINTF(0,("pxaudc_open\n"));
	s = splhardusb();

	switch (usbf_endpoint_type(pipe->endpoint)) {
	case UE_CONTROL:
		pipe->methods = &pxaudc_ctrl_methods;
		break;

	case UE_BULK:
		pipe->methods = &pxaudc_bulk_methods;
		break;

	case UE_ISOCHRONOUS:
	case UE_INTERRUPT:
	default:
		/* XXX */
		splx(s);
		return USBF_BAD_ADDRESS;
	}

	sc->sc_pipe[sc->sc_npipe] = ppipe;
	sc->sc_npipe++;
	printf("adding pipe %x\n", usbf_endpoint_index(pipe->endpoint));

	splx(s);
	return USBF_NORMAL_COMPLETION;
}

void
pxaudc_softintr(void *v)
{
	struct pxaudc_softc *sc = v;

	pxaudc_intr1(sc);
}

usbf_status
pxaudc_allocm(struct usbf_bus *bus, usb_dma_t *dmap, u_int32_t size)
{
	return usbf_allocmem(bus, size, 0, dmap);
}

void
pxaudc_freem(struct usbf_bus *bus, usb_dma_t *dmap)
{
	usbf_freemem(bus, dmap);
}

usbf_xfer_handle
pxaudc_allocx(struct usbf_bus *bus)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)bus;
	usbf_xfer_handle xfer;

	xfer = SIMPLEQ_FIRST(&sc->sc_free_xfers);
	if (xfer != NULL)
		SIMPLEQ_REMOVE_HEAD(&sc->sc_free_xfers, next);
	else
		xfer = malloc(sizeof(struct pxaudc_xfer), M_USB, M_NOWAIT);
	if (xfer != NULL)
		bzero(xfer, sizeof(struct pxaudc_xfer));
	return xfer;
}

void
pxaudc_freex(struct usbf_bus *bus, usbf_xfer_handle xfer)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)bus;

	SIMPLEQ_INSERT_HEAD(&sc->sc_free_xfers, xfer, next);
}

/*
 * Control pipe methods
 */

usbf_status
pxaudc_ctrl_transfer(usbf_xfer_handle xfer)
{
	usbf_status err;

	/* Insert last in queue. */
	err = usbf_insert_transfer(xfer);
	if (err)
		return err;

	/*
	 * Pipe isn't running (otherwise err would be USBF_IN_PROGRESS),
	 * so start first.
	 */
	return pxaudc_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

usbf_status
pxaudc_ctrl_start(usbf_xfer_handle xfer)
{
	struct usbf_pipe *pipe = xfer->pipe;
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;
	int iswrite = !(xfer->rqflags & URQ_REQUEST);
	int s;

	s = splusb();
	xfer->status = USBF_IN_PROGRESS;
	if (iswrite)
		pxaudc_write_ep0(sc, xfer);
	else {
		/* XXX boring message, this case is normally reached if
		 * XXX the xfer for a device request is being queued. */
		DPRINTF(0,("%s: ep[%x] ctrl-out, xfer=%p, len=%u, "
		    "actlen=%u\n", DEVNAME(sc),
		    usbf_endpoint_address(xfer->pipe->endpoint),
		    xfer, xfer->length,
		    xfer->actlen));
	}
	splx(s);
	return USBF_IN_PROGRESS;
}

/* (also used by bulk pipes) */
void
pxaudc_ctrl_abort(usbf_xfer_handle xfer)
{
	int s;
#ifdef PXAUDC_DEBUG
	struct usbf_pipe *pipe = xfer->pipe;
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;
	int index = usbf_endpoint_index(pipe->endpoint);
	int dir = usbf_endpoint_dir(pipe->endpoint);
	int type = usbf_endpoint_type(pipe->endpoint);
#endif

	DPRINTF(0,("%s: ep%d %s-%s abort, xfer=%p\n", DEVNAME(sc), index,
	    type == UE_CONTROL ? "ctrl" : "bulk", dir == UE_DIR_IN ?
	    "in" : "out", xfer));

	/*
	 * Step 1: Make soft interrupt routine and hardware ignore the xfer.
	 */
	s = splusb();
	xfer->status = USBF_CANCELLED;
	usb_uncallout(xfer->timeout_handle, pxaudc_timeout, NULL);
	splx(s);

	/*
	 * Step 2: Make sure hardware has finished any possible use of the
	 * xfer and the soft interrupt routine has run.
	 */
	s = splusb();
	/* XXX this does not seem right, what if there
	 * XXX are two xfers in the FIFO and we only want to
	 * XXX ignore one? */
#ifdef notyet
	pxaudc_flush(sc, usbf_endpoint_address(pipe->endpoint));
#endif
	/* XXX we're not doing DMA and the soft interrupt routine does not
	   XXX need to clean up anything. */
	splx(s);

	/*
	 * Step 3: Execute callback.
	 */
	s = splusb();
	usbf_transfer_complete(xfer);
	splx(s);
}

void
pxaudc_ctrl_done(usbf_xfer_handle xfer)
{
}

void
pxaudc_ctrl_close(usbf_pipe_handle pipe)
{
	/* XXX */
}

/*
 * Bulk pipe methods
 */

usbf_status
pxaudc_bulk_transfer(usbf_xfer_handle xfer)
{
	usbf_status err;

	/* Insert last in queue. */
	err = usbf_insert_transfer(xfer);
	if (err)
		return err;

	/*
	 * Pipe isn't running (otherwise err would be USBF_IN_PROGRESS),
	 * so start first.
	 */
	return pxaudc_bulk_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

usbf_status
pxaudc_bulk_start(usbf_xfer_handle xfer)
{
	struct usbf_pipe *pipe = xfer->pipe;
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;
	int iswrite = usbf_endpoint_dir(pipe->endpoint) == UE_DIR_IN;
	int s;

	DPRINTF(0,("%s: ep%d bulk-%s start, xfer=%p, len=%u\n", DEVNAME(sc),
	    usbf_endpoint_index(pipe->endpoint), iswrite ? "in" : "out",
	    xfer, xfer->length));

	s = splusb();
	xfer->status = USBF_IN_PROGRESS;
	if (iswrite)
		pxaudc_write(sc, xfer);
	else {
		/* enable interrupt */
	}
	splx(s);
	return USBF_IN_PROGRESS;
}

void
pxaudc_bulk_abort(usbf_xfer_handle xfer)
{
	pxaudc_ctrl_abort(xfer);
}

void
pxaudc_bulk_done(usbf_xfer_handle xfer)
{
}

void
pxaudc_bulk_close(usbf_pipe_handle pipe)
{
	/* XXX */
}

#endif /* NUSBF > 0 */
