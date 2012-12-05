/* $OpenBSD: i915_drv.c,v 1.123 2012/09/25 10:19:46 jsg Exp $ */
/*
 * Copyright (c) 2008-2009 Owain G. Ainsworth <oga@openbsd.org>
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
/*-
 * Copyright © 2008 Intel Corporation
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "intel_drv.h"

#include <machine/pmap.h>

#include <sys/queue.h>
#include <sys/workq.h>
#if 0
#	define INTELDRM_WATCH_COHERENCY
#	define WATCH_INACTIVE
#endif

const struct intel_device_info *
	i915_get_device_id(int);
int	inteldrm_probe(struct device *, void *, void *);
void	inteldrm_attach(struct device *, struct device *, void *);
int	inteldrm_detach(struct device *, int);
int	inteldrm_activate(struct device *, int);
int	inteldrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	inteldrm_doioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	inteldrm_intr(void *);
void	inteldrm_error(struct inteldrm_softc *);
int	inteldrm_ironlake_intr(void *);
void	inteldrm_lastclose(struct drm_device *);

void	inteldrm_wrap_ring(struct inteldrm_softc *);
int	inteldrm_gmch_match(struct pci_attach_args *);
void	inteldrm_timeout(void *);
void	inteldrm_hangcheck(void *);
void	inteldrm_hung(void *, void *);
void	inteldrm_965_reset(struct inteldrm_softc *, u_int8_t);
int	inteldrm_fault(struct drm_obj *, struct uvm_faultinfo *, off_t,
	    vaddr_t, vm_page_t *, int, int, vm_prot_t, int );
void	inteldrm_quiesce(struct inteldrm_softc *);

/* For reset and suspend */

void	i915_alloc_ifp(struct inteldrm_softc *, struct pci_attach_args *);
void	i965_alloc_ifp(struct inteldrm_softc *, struct pci_attach_args *);

void	inteldrm_detect_bit_6_swizzle(struct inteldrm_softc *,
	    struct pci_attach_args *);

int	inteldrm_setup_mchbar(struct inteldrm_softc *,
	    struct pci_attach_args *);
void	inteldrm_teardown_mchbar(struct inteldrm_softc *,
	    struct pci_attach_args *, int);

static const struct intel_device_info intel_i830_info = {
	.gen = 2, .is_mobile = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_845g_info = {
	.gen = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i85x_info = {
	.gen = 2, .is_i85x = 1, .is_mobile = 1,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i865g_info = {
	.gen = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i915g_info = {
	.gen = 3, .is_i915g = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};
static const struct intel_device_info intel_i915gm_info = {
	.gen = 3, .is_mobile = 1,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
};
static const struct intel_device_info intel_i945g_info = {
	.gen = 3, .has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};
static const struct intel_device_info intel_i945gm_info = {
	.gen = 3, .is_i945gm = 1, .is_mobile = 1,
	.has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
};

static const struct intel_device_info intel_i965g_info = {
	.gen = 4, .is_broadwater = 1,
	.has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_i965gm_info = {
	.gen = 4, .is_crestline = 1,
	.is_mobile = 1, .has_fbc = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.supports_tv = 1,
};

static const struct intel_device_info intel_g33_info = {
	.gen = 3, .is_g33 = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_g45_info = {
	.gen = 4, .is_g4x = 1, .need_gfx_hws = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_gm45_info = {
	.gen = 4, .is_g4x = 1,
	.is_mobile = 1, .need_gfx_hws = 1, .has_fbc = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.supports_tv = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_pineview_info = {
	.gen = 3, .is_g33 = 1, .is_pineview = 1, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_ironlake_d_info = {
	.gen = 5,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_ironlake_m_info = {
	.gen = 5, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0, /* disabled due to buggy hardware */
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_sandybridge_d_info = {
	.gen = 6,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
};

static const struct intel_device_info intel_sandybridge_m_info = {
	.gen = 6, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
};

static const struct intel_device_info intel_ivybridge_d_info = {
	.is_ivybridge = 1, .gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
};

static const struct intel_device_info intel_ivybridge_m_info = {
	.is_ivybridge = 1, .gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,	/* FBC is not enabled on Ivybridge mobile yet */
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
};

const static struct drm_pcidev inteldrm_pciidlist[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830M_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82855GM_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865G_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7221_IGD,		0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GME_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G35_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G965_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM965_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GME965_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_IGD_1,	0 },
	{PCI_VENDOR_INTEL, 0x2E02,				0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G45_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G41_IGD_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_IGC_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_M_IGC_1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CLARKDALE_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ARRANDALE_IGD,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT2_PLUS,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT2_PLUS,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_D_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_M_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_S_GT1,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_D_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_M_GT2,	0 },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_S_GT2,	0 },
	{0, 0, 0}
};

static const struct intel_gfx_device_id {
	int vendor;
        int device;
        const struct intel_device_info *info;
} inteldrm_pciidlist_info[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82830M_IGD,
	    &intel_i830_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82845G_IGD,
	    &intel_845g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82855GM_IGD,
	    &intel_i85x_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82865G_IGD,
	    &intel_i865g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915G_IGD_1,
	    &intel_i915g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7221_IGD,
	    &intel_i915g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82915GM_IGD_1,
	    &intel_i915gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945G_IGD_1,
	    &intel_i945g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GM_IGD_1,
	    &intel_i945gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82945GME_IGD_1,
	    &intel_i945gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82946GZ_IGD_1,
	    &intel_i965g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G35_IGD_1,
	    &intel_i965g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q965_IGD_1,
	    &intel_i965g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G965_IGD_1,
	    &intel_i965g_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM965_IGD_1,
	    &intel_i965gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GME965_IGD_1,
	    &intel_i965gm_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G33_IGD_1,
	    &intel_g33_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q35_IGD_1,
	    &intel_g33_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q33_IGD_1,
	    &intel_g33_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82GM45_IGD_1,
	    &intel_gm45_info },
	{PCI_VENDOR_INTEL, 0x2E02,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82Q45_IGD_1,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G45_IGD_1,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82G41_IGD_1,
	    &intel_g45_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_IGC_1,
	    &intel_pineview_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PINEVIEW_M_IGC_1,
	    &intel_pineview_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CLARKDALE_IGD,
	    &intel_ironlake_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ARRANDALE_IGD,
	    &intel_ironlake_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT1,
	    &intel_sandybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT1,
	    &intel_sandybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT2,
	    &intel_sandybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT2,
	    &intel_sandybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_GT2_PLUS,
	    &intel_sandybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE2G_M_GT2_PLUS,
	    &intel_sandybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_D_GT1,
	    &intel_ivybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_M_GT1,
	    &intel_ivybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_S_GT1,
	    &intel_ivybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_D_GT2,
	    &intel_ivybridge_d_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_M_GT2,
	    &intel_ivybridge_m_info },
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CORE3G_S_GT2,
	    &intel_ivybridge_d_info },
	{0, 0, NULL}
};

static const struct drm_driver_info inteldrm_driver = {
	.buf_priv_size		= 1,	/* No dev_priv */
	.file_priv_size		= sizeof(struct inteldrm_file),
	.ioctl			= inteldrm_ioctl,
	.lastclose		= inteldrm_lastclose,
	.vblank_pipes		= 2,
	.get_vblank_counter	= i915_get_vblank_counter,
	.enable_vblank		= i915_enable_vblank,
	.disable_vblank		= i915_disable_vblank,
	.irq_install		= i915_driver_irq_install,
	.irq_uninstall		= i915_driver_irq_uninstall,

	.gem_init_object	= i915_gem_init_object,
	.gem_free_object	= i915_gem_free_object,
	.gem_fault		= inteldrm_fault,
	.gem_size		= sizeof(struct inteldrm_obj),

	.dumb_create		= i915_gem_dumb_create,
	.dumb_map_offset	= i915_gem_mmap_gtt,
	.dumb_destroy		= i915_gem_dumb_destroy,

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.patchlevel		= DRIVER_PATCHLEVEL,

	.flags			= DRIVER_AGP | DRIVER_AGP_REQUIRE |
				    DRIVER_MTRR | DRIVER_IRQ | DRIVER_GEM |
				    DRIVER_MODESET,
};

const struct intel_device_info *
i915_get_device_id(int device)
{
	const struct intel_gfx_device_id *did;

	for (did = &inteldrm_pciidlist_info[0]; did->device != 0; did++) {
		if (did->device != device)
			continue;
		return (did->info);
	}
	return (NULL);
}

int
inteldrm_probe(struct device *parent, void *match, void *aux)
{
	return (drm_pciprobe((struct pci_attach_args *)aux,
	    inteldrm_pciidlist));
}

/*
 * We're intel IGD, bus 0 function 0 dev 0 should be the GMCH, so it should
 * be Intel
 */
int
inteldrm_gmch_match(struct pci_attach_args *pa)
{
	if (pa->pa_bus == 0 && pa->pa_device == 0 && pa->pa_function == 0 &&
	    PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_HOST)
		return (1);
	return (0);
}

void
inteldrm_attach(struct device *parent, struct device *self, void *aux)
{
	struct inteldrm_softc	*dev_priv = (struct inteldrm_softc *)self;
	struct pci_attach_args	*pa = aux, bpa;
	struct vga_pci_bar	*bar;
	struct drm_device	*dev;
	const struct drm_pcidev	*id_entry;
	int			 i;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), inteldrm_pciidlist);
	dev_priv->pci_device = PCI_PRODUCT(pa->pa_id);
	dev_priv->info = i915_get_device_id(dev_priv->pci_device);
	KASSERT(dev_priv->info->gen != 0);

	dev_priv->pc = pa->pa_pc;
	dev_priv->tag = pa->pa_tag;
	dev_priv->dmat = pa->pa_dmat;
	dev_priv->bst = pa->pa_memt;

	/* we need to use this api for now due to sharing with intagp */
	bar = vga_pci_bar_info((struct vga_pci_softc *)parent,
	    (IS_I9XX(dev_priv) ? 0 : 1));
	if (bar == NULL) {
		printf(": can't get BAR info\n");
		return;
	}

	dev_priv->regs = vga_pci_bar_map((struct vga_pci_softc *)parent,
	    bar->addr, 0, 0);
	if (dev_priv->regs == NULL) {
		printf(": can't map mmio space\n");
		return;
	}

	/*
	 * i945G/GM report MSI capability despite not actually supporting it.
	 * so explicitly disable it.
	 */
	if (IS_I945G(dev_priv) || IS_I945GM(dev_priv))
		pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;

	if (pci_intr_map(pa, &dev_priv->ih) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}

	/*
	 * set up interrupt handler, note that we don't switch the interrupt
	 * on until the X server talks to us, kms will change this.
	 */
	dev_priv->irqh = pci_intr_establish(dev_priv->pc, dev_priv->ih, IPL_TTY,
	    (HAS_PCH_SPLIT(dev_priv) ? inteldrm_ironlake_intr : inteldrm_intr),
	    dev_priv, dev_priv->dev.dv_xname);
	if (dev_priv->irqh == NULL) {
		printf(": couldn't  establish interrupt\n");
		return;
	}

	/* Unmask the interrupts that we always want on. */
	if (HAS_PCH_SPLIT(dev_priv)) {
		dev_priv->irq_mask_reg = ~PCH_SPLIT_DISPLAY_INTR_FIX;
		/* masked for now, turned on on demand */
		dev_priv->gt_irq_mask_reg = ~PCH_SPLIT_RENDER_INTR_FIX;
		dev_priv->pch_irq_mask_reg = ~PCH_SPLIT_HOTPLUG_INTR_FIX;
	} else {
		dev_priv->irq_mask_reg = ~I915_INTERRUPT_ENABLE_FIX;
	}

	dev_priv->workq = workq_create("intelrel", 1, IPL_TTY);
	if (dev_priv->workq == NULL) {
		printf("couldn't create workq\n");
		return;
	}

	/* GEM init */
	TAILQ_INIT(&dev_priv->mm.active_list);
	TAILQ_INIT(&dev_priv->mm.flushing_list);
	TAILQ_INIT(&dev_priv->mm.inactive_list);
	TAILQ_INIT(&dev_priv->mm.gpu_write_list);
	TAILQ_INIT(&dev_priv->mm.request_list);
	TAILQ_INIT(&dev_priv->mm.fence_list);
	timeout_set(&dev_priv->mm.retire_timer, inteldrm_timeout, dev_priv);
	timeout_set(&dev_priv->mm.hang_timer, inteldrm_hangcheck, dev_priv);
	dev_priv->mm.next_gem_seqno = 1;
	dev_priv->mm.suspended = 1;

	/* On GEN3 we really need to make sure the ARB C3 LP bit is set */
	if (IS_GEN3(dev_priv)) {
		u_int32_t tmp = I915_READ(MI_ARB_STATE);
		if (!(tmp & MI_ARB_C3_LP_WRITE_ENABLE)) {
			/*
			 * arb state is a masked write, so set bit + bit
			 * in mask
			 */
			tmp = MI_ARB_C3_LP_WRITE_ENABLE |
			    (MI_ARB_C3_LP_WRITE_ENABLE << MI_ARB_MASK_SHIFT);
			I915_WRITE(MI_ARB_STATE, tmp);
		}
	}

	/* For the X server, in kms mode this will not be needed */
	dev_priv->fence_reg_start = 3;

	if (IS_I965G(dev_priv) || IS_I945G(dev_priv) || IS_I945GM(dev_priv) ||
	    IS_G33(dev_priv))
		dev_priv->num_fence_regs = 16;
	else
		dev_priv->num_fence_regs = 8;

	/* Initialise fences to zero, else on some macs we'll get corruption */
	if (IS_GEN6(dev_priv) || IS_GEN7(dev_priv)) {
		for (i = 0; i < 16; i++)
			I915_WRITE64(FENCE_REG_SANDYBRIDGE_0 + (i * 8), 0);
	} else if (IS_I965G(dev_priv)) {
		for (i = 0; i < 16; i++)
			I915_WRITE64(FENCE_REG_965_0 + (i * 8), 0);
	} else {
		for (i = 0; i < 8; i++)
			I915_WRITE(FENCE_REG_830_0 + (i * 4), 0);
		if (IS_I945G(dev_priv) || IS_I945GM(dev_priv) ||
		    IS_G33(dev_priv))
			for (i = 0; i < 8; i++)
				I915_WRITE(FENCE_REG_945_8 + (i * 4), 0);
	}

	if (pci_find_device(&bpa, inteldrm_gmch_match) == 0) {
		printf(": can't find GMCH\n");
		return;
	}

	/* Set up the IFP for chipset flushing */
	if (IS_I915G(dev_priv) || IS_I915GM(dev_priv) || IS_I945G(dev_priv) ||
	    IS_I945GM(dev_priv)) {
		i915_alloc_ifp(dev_priv, &bpa);
	} else if (IS_I965G(dev_priv) || IS_G33(dev_priv)) {
		i965_alloc_ifp(dev_priv, &bpa);
	} else {
		int nsegs;
		/*
		 * I8XX has no flush page mechanism, we fake it by writing until
		 * the cache is empty. allocate a page to scribble on
		 */
		dev_priv->ifp.i8xx.kva = NULL;
		if (bus_dmamem_alloc(pa->pa_dmat, PAGE_SIZE, 0, 0,
		    &dev_priv->ifp.i8xx.seg, 1, &nsegs, BUS_DMA_WAITOK) == 0) {
			if (bus_dmamem_map(pa->pa_dmat, &dev_priv->ifp.i8xx.seg,
			    1, PAGE_SIZE, &dev_priv->ifp.i8xx.kva, 0) != 0) {
				bus_dmamem_free(pa->pa_dmat,
				    &dev_priv->ifp.i8xx.seg, nsegs);
				dev_priv->ifp.i8xx.kva = NULL;
			}
		}
	}

	inteldrm_detect_bit_6_swizzle(dev_priv, &bpa);
	/* Init HWS */
	if (!I915_NEED_GFX_HWS(dev_priv)) {
		if (i915_init_phys_hws(dev_priv, pa->pa_dmat) != 0) {
			printf(": couldn't alloc HWS page\n");
			return;
		}
	}

	printf(": %s\n", pci_intr_string(pa->pa_pc, dev_priv->ih));

	mtx_init(&dev_priv->user_irq_lock, IPL_TTY);
	mtx_init(&dev_priv->list_lock, IPL_NONE);
	mtx_init(&dev_priv->request_lock, IPL_NONE);
	mtx_init(&dev_priv->fence_lock, IPL_NONE);
	mtx_init(&dev_priv->gt_lock, IPL_NONE);

	if (IS_IVYBRIDGE(dev_priv))
		dev_priv->num_pipe = 3;
	else if (IS_MOBILE(dev_priv) || !IS_GEN2(dev_priv))
		dev_priv->num_pipe = 2;
	else
		dev_priv->num_pipe = 1;

	intel_detect_pch(dev_priv);

	/* All intel chipsets need to be treated as agp, so just pass one */
	dev_priv->drmdev = drm_attach_pci(&inteldrm_driver, pa, 1, self);

	dev = (struct drm_device *)dev_priv->drmdev;

	intel_opregion_setup(dev);
	intel_setup_bios(dev);
	intel_setup_gmbus(dev_priv);

	/* XXX would be a lot nicer to get agp info before now */
	uvm_page_physload(atop(dev->agp->base), atop(dev->agp->base +
	    dev->agp->info.ai_aperture_size), atop(dev->agp->base),
	    atop(dev->agp->base + dev->agp->info.ai_aperture_size),
	    PHYSLOAD_DEVICE);
	/* array of vm pages that physload introduced. */
	dev_priv->pgs = PHYS_TO_VM_PAGE(dev->agp->base);
	KASSERT(dev_priv->pgs != NULL);
	/*
	 * XXX mark all pages write combining so user mmaps get the right
	 * bits. We really need a proper MI api for doing this, but for now
	 * this allows us to use PAT where available.
	 */
	for (i = 0; i < atop(dev->agp->info.ai_aperture_size); i++)
		atomic_setbits_int(&(dev_priv->pgs[i].pg_flags), PG_PMAP_WC);
	if (agp_init_map(dev_priv->bst, dev->agp->base,
	    dev->agp->info.ai_aperture_size, BUS_SPACE_MAP_LINEAR |
	    BUS_SPACE_MAP_PREFETCHABLE, &dev_priv->agph))
		panic("can't map aperture");

	/* XXX */
	i915_load_modeset_init(dev);
	intel_opregion_init(dev);
}

int
inteldrm_detach(struct device *self, int flags)
{
	struct inteldrm_softc *dev_priv = (struct inteldrm_softc *)self;

	/* this will quiesce any dma that's going on and kill the timeouts. */
	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

	if (!I915_NEED_GFX_HWS(dev_priv) && dev_priv->hws_dmamem) {
		drm_dmamem_free(dev_priv->dmat, dev_priv->hws_dmamem);
		dev_priv->hws_dmamem = NULL;
		/* Need to rewrite hardware status page */
		I915_WRITE(HWS_PGA, 0x1ffff000);
		dev_priv->hw_status_page = NULL;
	}

	if (IS_I9XX(dev_priv) && dev_priv->ifp.i9xx.bsh != 0) {
		bus_space_unmap(dev_priv->ifp.i9xx.bst, dev_priv->ifp.i9xx.bsh,
		    PAGE_SIZE);
	} else if ((IS_I830(dev_priv) || IS_845G(dev_priv) || IS_I85X(dev_priv) ||
	    IS_I865G(dev_priv)) && dev_priv->ifp.i8xx.kva != NULL) {
		bus_dmamem_unmap(dev_priv->dmat, dev_priv->ifp.i8xx.kva,
		     PAGE_SIZE);
		bus_dmamem_free(dev_priv->dmat, &dev_priv->ifp.i8xx.seg, 1);
	}

	pci_intr_disestablish(dev_priv->pc, dev_priv->irqh);

	if (dev_priv->regs != NULL)
		vga_pci_bar_unmap(dev_priv->regs);

	return (0);
}

int
inteldrm_activate(struct device *arg, int act)
{
	struct inteldrm_softc	*dev_priv = (struct inteldrm_softc *)arg;

	switch (act) {
	case DVACT_QUIESCE:
		inteldrm_quiesce(dev_priv);
		break;
	case DVACT_SUSPEND:
		i915_save_state(dev_priv);
		break;
	case DVACT_RESUME:
		i915_restore_state(dev_priv);
		/* entrypoints can stop sleeping now */
		atomic_clearbits_int(&dev_priv->sc_flags, INTELDRM_QUIET);
		wakeup(&dev_priv->flags);
		break;
	}

	return (0);
}

struct cfattach inteldrm_ca = {
	sizeof(struct inteldrm_softc), inteldrm_probe, inteldrm_attach,
	inteldrm_detach, inteldrm_activate
};

struct cfdriver inteldrm_cd = {
	0, "inteldrm", DV_DULL
};

int
inteldrm_ioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int			 error = 0;

	while ((dev_priv->sc_flags & INTELDRM_QUIET) && error == 0)
		error = tsleep(&dev_priv->flags, PCATCH, "intelioc", 0);
	if (error)
		return (error);
	dev_priv->entries++;

	error = inteldrm_doioctl(dev, cmd, data, file_priv);

	dev_priv->entries--;
	if (dev_priv->sc_flags & INTELDRM_QUIET)
		wakeup(&dev_priv->entries);
	return (error);
}

int
inteldrm_doioctl(struct drm_device *dev, u_long cmd, caddr_t data,
    struct drm_file *file_priv)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;

	if (file_priv->authenticated == 1) {
		switch (cmd) {
		case DRM_IOCTL_I915_GETPARAM:
			return (inteldrm_getparam(dev_priv, data));
		case DRM_IOCTL_I915_GEM_EXECBUFFER2:
			return (i915_gem_execbuffer2(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_BUSY:
			return (i915_gem_busy_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_THROTTLE:
			return (i915_gem_ring_throttle(dev, file_priv));
		case DRM_IOCTL_I915_GEM_MMAP:
			return (i915_gem_gtt_map_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_MMAP_GTT:
			return (i915_gem_mmap_gtt_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_CREATE:
			return (i915_gem_create_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PREAD:
			return (i915_gem_pread_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PWRITE:
			return (i915_gem_pwrite_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_SET_DOMAIN:
			return (i915_gem_set_domain_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GEM_SET_TILING:
			return (i915_gem_set_tiling(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_GET_TILING:
			return (i915_gem_get_tiling(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_GET_APERTURE:
			return (i915_gem_get_aperture_ioctl(dev, data,
			    file_priv));
		case DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID:
			return (intel_get_pipe_from_crtc_id(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_MADVISE:
			return (i915_gem_madvise_ioctl(dev, data, file_priv));
		default:
			break;
		}
	}

	if (file_priv->master == 1) {
		switch (cmd) {
		case DRM_IOCTL_I915_SETPARAM:
			return (inteldrm_setparam(dev_priv, data));
		case DRM_IOCTL_I915_GEM_INIT:
			return (i915_gem_init_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_ENTERVT:
			return (i915_gem_entervt_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_LEAVEVT:
			return (i915_gem_leavevt_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_PIN:
			return (i915_gem_pin_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_GEM_UNPIN:
			return (i915_gem_unpin_ioctl(dev, data, file_priv));
		case DRM_IOCTL_I915_OVERLAY_PUT_IMAGE:
			return (intel_overlay_put_image(dev, data, file_priv));
		case DRM_IOCTL_I915_OVERLAY_ATTRS:
			return (intel_overlay_attrs(dev, data, file_priv));
		case DRM_IOCTL_I915_GET_SPRITE_COLORKEY:
			return (intel_sprite_get_colorkey(dev, data, file_priv));
		case DRM_IOCTL_I915_SET_SPRITE_COLORKEY:
			return (intel_sprite_set_colorkey(dev, data, file_priv));
		}
	}
	return (EINVAL);
}

int
inteldrm_ironlake_intr(void *arg)
{
	struct inteldrm_softc	*dev_priv = arg;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	u_int32_t		 de_iir, gt_iir, de_ier, pch_iir;
	int			 ret = 0, i;

	de_ier = I915_READ(DEIER);
	I915_WRITE(DEIER, de_ier & ~DE_MASTER_IRQ_CONTROL);
	(void)I915_READ(DEIER);

	de_iir = I915_READ(DEIIR);
	gt_iir = I915_READ(GTIIR);
	pch_iir = I915_READ(SDEIIR);

	if (de_iir == 0 && gt_iir == 0 && pch_iir == 0)
		goto done;
	ret = 1;

	if (gt_iir & GT_USER_INTERRUPT) {
		wakeup(dev_priv);
		dev_priv->mm.hang_cnt = 0;
		timeout_add_msec(&dev_priv->mm.hang_timer, 750);
	}
	if (gt_iir & GT_MASTER_ERROR)
		inteldrm_error(dev_priv);

	if (IS_GEN7(dev_priv)) {
		for (i = 0; i < 3; i++) {
			if (de_iir & (DE_PIPEA_VBLANK_IVB << (5 * i)))
				drm_handle_vblank(dev, i);
		}
	} else {
		if (de_iir & DE_PIPEA_VBLANK)
			drm_handle_vblank(dev, 0);
		if (de_iir & DE_PIPEB_VBLANK)
			drm_handle_vblank(dev, 1);
	}

	/* should clear PCH hotplug event before clearing CPU irq */
	I915_WRITE(SDEIIR, pch_iir);
	I915_WRITE(GTIIR, gt_iir);
	I915_WRITE(DEIIR, de_iir);

done:
	I915_WRITE(DEIER, de_ier);
	(void)I915_READ(DEIER);

	return (ret);
}

int
inteldrm_intr(void *arg)
{
	struct inteldrm_softc	*dev_priv = arg;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	u_int32_t		 iir, pipea_stats = 0, pipeb_stats = 0;

	/*
	 * we're not set up, don't poke the hw  and if we're vt switched
	 * then nothing will be enabled
	 */
	if (dev_priv->hw_status_page == NULL || dev_priv->mm.suspended)
		return (0);

	iir = I915_READ(IIR);
	if (iir == 0)
		return (0);

	/*
	 * lock is to protect from writes to PIPESTAT and IMR from other cores.
	 */
	mtx_enter(&dev_priv->user_irq_lock);
	/*
	 * Clear the PIPE(A|B)STAT regs before the IIR
	 */
	if (iir & I915_DISPLAY_PIPE_A_EVENT_INTERRUPT) {
		pipea_stats = I915_READ(_PIPEASTAT);
		I915_WRITE(_PIPEASTAT, pipea_stats);
	}
	if (iir & I915_DISPLAY_PIPE_B_EVENT_INTERRUPT) {
		pipeb_stats = I915_READ(_PIPEBSTAT);
		I915_WRITE(_PIPEBSTAT, pipeb_stats);
	}
	if (iir & I915_RENDER_COMMAND_PARSER_ERROR_INTERRUPT)
		inteldrm_error(dev_priv);

	I915_WRITE(IIR, iir);
	(void)I915_READ(IIR); /* Flush posted writes */

	if (iir & I915_USER_INTERRUPT) {
		wakeup(dev_priv);
		dev_priv->mm.hang_cnt = 0;
		timeout_add_msec(&dev_priv->mm.hang_timer, 750);
	}

	mtx_leave(&dev_priv->user_irq_lock);

	if (pipea_stats & PIPE_VBLANK_INTERRUPT_STATUS)
		drm_handle_vblank(dev, 0);

	if (pipeb_stats & PIPE_VBLANK_INTERRUPT_STATUS)
		drm_handle_vblank(dev, 1);

	return (1);
}

u_int32_t
inteldrm_read_hws(struct inteldrm_softc *dev_priv, int reg)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	struct inteldrm_obj	*obj_priv;
	bus_dma_tag_t		 tag;
	bus_dmamap_t		 map;
	u_int32_t		 val;

	if (I915_NEED_GFX_HWS(dev_priv)) {
		obj_priv = (struct inteldrm_obj *)dev_priv->hws_obj;
		map = obj_priv->dmamap;
		tag = dev_priv->agpdmat;
	} else {
		map = dev_priv->hws_dmamem->map;
		tag = dev->dmat;
	}

	bus_dmamap_sync(tag, map, 0, PAGE_SIZE, BUS_DMASYNC_POSTREAD);

	val = ((volatile u_int32_t *)(dev_priv->hw_status_page))[reg];
	bus_dmamap_sync(tag, map, 0, PAGE_SIZE, BUS_DMASYNC_PREREAD);

	return (val);
}

/*
 * These five ring manipulation functions are protected by dev->dev_lock.
 */
int
inteldrm_wait_ring(struct inteldrm_softc *dev_priv, int n)
{
	struct inteldrm_ring	*ring = &dev_priv->ring;
	u_int32_t		 acthd_reg, acthd, last_acthd, last_head;
	int			 i;

	acthd_reg = IS_I965G(dev_priv) ? ACTHD_I965 : ACTHD;
	last_head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	last_acthd = I915_READ(acthd_reg);

	/* ugh. Could really do with a proper, resettable timer here. */
	for (i = 0; i < 100000; i++) {
		ring->head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
		acthd = I915_READ(acthd_reg);
		ring->space = ring->head - (ring->tail + 8);

		INTELDRM_VPRINTF("%s: head: %x tail: %x space: %x\n", __func__,
			ring->head, ring->tail, ring->space);
		if (ring->space < 0)
			ring->space += ring->size;
		if (ring->space >= n)
			return (0);

		/* Only timeout if the ring isn't chewing away on something */
		if (ring->head != last_head || acthd != last_acthd)
			i = 0;

		last_head = ring->head;
		last_acthd = acthd;
		delay(10);
	}

	return (EBUSY);
}

void
inteldrm_wrap_ring(struct inteldrm_softc *dev_priv)
{
	u_int32_t	rem;;

	rem = dev_priv->ring.size - dev_priv->ring.tail;
	if (dev_priv->ring.space < rem &&
	    inteldrm_wait_ring(dev_priv, rem) != 0)
			return; /* XXX */

	dev_priv->ring.space -= rem;

	bus_space_set_region_4(dev_priv->bst, dev_priv->ring.bsh,
	    dev_priv->ring.woffset, MI_NOOP, rem / 4);

	dev_priv->ring.tail = 0;
}

void
inteldrm_begin_ring(struct inteldrm_softc *dev_priv, int ncmd)
{
	int	bytes = 4 * ncmd;

	INTELDRM_VPRINTF("%s: %d\n", __func__, ncmd);
	if (dev_priv->ring.tail + bytes > dev_priv->ring.size)
		inteldrm_wrap_ring(dev_priv);
	if (dev_priv->ring.space < bytes)
		inteldrm_wait_ring(dev_priv, bytes);
	dev_priv->ring.woffset = dev_priv->ring.tail;
	dev_priv->ring.tail += bytes;
	dev_priv->ring.tail &= dev_priv->ring.size - 1;
	dev_priv->ring.space -= bytes;
}

void
inteldrm_out_ring(struct inteldrm_softc *dev_priv, u_int32_t cmd)
{
	INTELDRM_VPRINTF("%s: %x\n", __func__, cmd);
	bus_space_write_4(dev_priv->bst, dev_priv->ring.bsh,
	    dev_priv->ring.woffset, cmd);
	/*
	 * don't need to deal with wrap here because we padded
	 * the ring out if we would wrap
	 */
	dev_priv->ring.woffset += 4;
}

void
inteldrm_advance_ring(struct inteldrm_softc *dev_priv)
{
	INTELDRM_VPRINTF("%s: %x, %x\n", __func__, dev_priv->ring.wspace,
	    dev_priv->ring.woffset);
	DRM_MEMORYBARRIER();
	I915_WRITE(PRB0_TAIL, dev_priv->ring.tail);
}

void
inteldrm_update_ring(struct inteldrm_softc *dev_priv)
{
	struct inteldrm_ring	*ring = &dev_priv->ring;

	ring->head = (I915_READ(PRB0_HEAD) & HEAD_ADDR);
	ring->tail = (I915_READ(PRB0_TAIL) & TAIL_ADDR);
	ring->space = ring->head - (ring->tail + 8);
	if (ring->space < 0)
		ring->space += ring->size;
	INTELDRM_VPRINTF("%s: head: %x tail: %x space: %x\n", __func__,
		ring->head, ring->tail, ring->space);
}

/*
 * Sets up the hardware status page for devices that need a physical address
 * in the register.
 */
int
i915_init_phys_hws(struct inteldrm_softc *dev_priv, bus_dma_tag_t dmat)
{
	/* Program Hardware Status Page */
	if ((dev_priv->hws_dmamem = drm_dmamem_alloc(dmat, PAGE_SIZE,
	    PAGE_SIZE, 1, PAGE_SIZE, 0, BUS_DMA_READ)) == NULL) {
		return (ENOMEM);
	}

	dev_priv->hw_status_page = dev_priv->hws_dmamem->kva;

	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);

	bus_dmamap_sync(dmat, dev_priv->hws_dmamem->map, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);
	I915_WRITE(HWS_PGA, dev_priv->hws_dmamem->map->dm_segs[0].ds_addr);
	DRM_DEBUG("Enabled hardware status page\n");
	return (0);
}

void
i915_alloc_ifp(struct inteldrm_softc *dev_priv, struct pci_attach_args *bpa)
{
	bus_addr_t	addr;
	u_int32_t	reg;

	dev_priv->ifp.i9xx.bst = bpa->pa_memt;

	reg = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR);
	if (reg & 0x1) {
		addr = (bus_addr_t)reg;
		addr &= ~0x1;
		/* XXX extents ... need data on whether bioses alloc or not. */
		if (bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
		    &dev_priv->ifp.i9xx.bsh) != 0)
			goto nope;
		return;
	} else if (bpa->pa_memex == NULL || extent_alloc(bpa->pa_memex,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) || bus_space_map(bpa->pa_memt,
	    addr, PAGE_SIZE, 0, &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I915_IFPADDR, addr | 0x1);

	return;

nope:
	dev_priv->ifp.i9xx.bsh = 0;
	printf(": no ifp ");
}

void
i965_alloc_ifp(struct inteldrm_softc *dev_priv, struct pci_attach_args *bpa)
{
	bus_addr_t	addr;
	u_int32_t	lo, hi;

	dev_priv->ifp.i9xx.bst = bpa->pa_memt;

	hi = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4);
	lo = pci_conf_read(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR);
	if (lo & 0x1) {
		addr = (((u_int64_t)hi << 32) | lo);
		addr &= ~0x1;
		/* XXX extents ... need data on whether bioses alloc or not. */
		if (bus_space_map(bpa->pa_memt, addr, PAGE_SIZE, 0,
		    &dev_priv->ifp.i9xx.bsh) != 0)
			goto nope;
		return;
	} else if (bpa->pa_memex == NULL || extent_alloc(bpa->pa_memex,
	    PAGE_SIZE, PAGE_SIZE, 0, 0, 0, &addr) || bus_space_map(bpa->pa_memt,
	    addr, PAGE_SIZE, 0, &dev_priv->ifp.i9xx.bsh))
		goto nope;

	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR + 4,
	    upper_32_bits(addr));
	pci_conf_write(bpa->pa_pc, bpa->pa_tag, I965_IFPADDR,
	    (addr & 0xffffffff) | 0x1);

	return;

nope:
	dev_priv->ifp.i9xx.bsh = 0;
	printf(": no ifp ");
}

void
inteldrm_chipset_flush(struct inteldrm_softc *dev_priv)
{
	/*
	 * Write to this flush page flushes the chipset write cache.
	 * The write will return when it is done.
	 */
	if (IS_I9XX(dev_priv)) {
	    if (dev_priv->ifp.i9xx.bsh != 0)
		bus_space_write_4(dev_priv->ifp.i9xx.bst,
		    dev_priv->ifp.i9xx.bsh, 0, 1);
	} else {
		int i;

		wbinvd();

#define I830_HIC        0x70

		I915_WRITE(I830_HIC, (I915_READ(I830_HIC) | (1<<31)));
		for (i = 1000; i; i--) {
			if (!(I915_READ(I830_HIC) & (1<<31)))
				break;
			delay(100);
		}

	}
}

void
inteldrm_lastclose(struct drm_device *dev)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct vm_page		*p;
	int			 ret;

	ret = i915_gem_idle(dev_priv);
	if (ret)
		DRM_ERROR("failed to idle hardware: %d\n", ret);

	if (dev_priv->agpdmat != NULL) {
		/*
		 * make sure we nuke everything, we may have mappings that we've
		 * unrefed, but uvm has a reference to them for maps. Make sure
		 * they get unbound and any accesses will segfault.
		 * XXX only do ones in GEM.
		 */
		for (p = dev_priv->pgs; p < dev_priv->pgs +
		    (dev->agp->info.ai_aperture_size / PAGE_SIZE); p++)
			pmap_page_protect(p, VM_PROT_NONE);
		agp_bus_dma_destroy((struct agp_softc *)dev->agp->agpdev,
		    dev_priv->agpdmat);
	}
	dev_priv->agpdmat = NULL;
}

int
inteldrm_getparam(struct inteldrm_softc *dev_priv, void *data)
{
	drm_i915_getparam_t	*param = data;
	int			 value;

	switch (param->param) {
	case I915_PARAM_CHIPSET_ID:
		value = dev_priv->pci_device;
		break;
	case I915_PARAM_HAS_GEM:
		value = 1;
		break;
	case I915_PARAM_NUM_FENCES_AVAIL:
		value = dev_priv->num_fence_regs - dev_priv->fence_reg_start;
		break;
	case I915_PARAM_HAS_EXECBUF2:
		value = 1;
		break;
	case I915_PARAM_HAS_BSD:
		value = HAS_BSD(dev_priv);
		break;
	case I915_PARAM_HAS_BLT:
		value = HAS_BLT(dev_priv);
		break;
	case I915_PARAM_HAS_RELAXED_FENCING:
		value = 1;
		break;
	default:
		DRM_DEBUG("Unknown parameter %d\n", param->param);
		return (EINVAL);
	}
	return (copyout(&value, param->value, sizeof(int)));
}

int
inteldrm_setparam(struct inteldrm_softc *dev_priv, void *data)
{
	drm_i915_setparam_t	*param = data;

	switch (param->param) {
	case I915_SETPARAM_NUM_USED_FENCES:
		if (param->value > dev_priv->num_fence_regs ||
		    param->value < 0)
			return EINVAL;
		/* Userspace can use first N regs */
		dev_priv->fence_reg_start = param->value;
		break;
	default:
		DRM_DEBUG("unknown parameter %d\n", param->param);
		return (EINVAL);
	}

	return 0;
}

void
inteldrm_set_max_obj_size(struct inteldrm_softc *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	/*
	 * Allow max obj size up to the size where ony 2 would fit the
	 * aperture, but some slop exists due to alignment etc
	 */
	dev_priv->max_gem_obj_size = (dev->gtt_total -
	    atomic_read(&dev->pin_memory)) * 3 / 4 / 2;

}

int
i915_gem_gtt_map_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_gem_mmap	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	vaddr_t				 addr;
	voff_t				 offset;
	vsize_t				 end, nsize;
	int				 ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);

	/* Since we are doing purely uvm-related operations here we do
	 * not need to hold the object, a reference alone is sufficient
	 */
	obj_priv = (struct inteldrm_obj *)obj;

	/* Check size. Also ensure that the object is not purgeable */
	if (args->size == 0 || args->offset > obj->size || args->size >
	    obj->size || (args->offset + args->size) > obj->size ||
	    i915_obj_purgeable(obj_priv)) {
		ret = EINVAL;
		goto done;
	}

	end = round_page(args->offset + args->size);
	offset = trunc_page(args->offset);
	nsize = end - offset;

	/*
	 * We give our reference from object_lookup to the mmap, so only
	 * must free it in the case that the map fails.
	 */
	addr = 0;
	ret = uvm_map(&curproc->p_vmspace->vm_map, &addr, nsize, &obj->uobj,
	    offset, 0, UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW,
	    UVM_INH_SHARE, UVM_ADV_RANDOM, 0));

done:
	if (ret == 0)
		args->addr_ptr = (uint64_t) addr + (args->offset & PAGE_MASK);
	else
		drm_unref(&obj->uobj);

	return (ret);
}

void
inteldrm_purge_obj(struct drm_obj *obj)
{
	DRM_ASSERT_HELD(obj);
	/*
	 * may sleep. We free here instead of deactivate (which
	 * the madvise() syscall would do) because in this case
	 * (userland bo cache and GL_APPLE_object_purgeable objects in
	 * OpenGL) the pages are defined to be freed if they were cleared
	 * so kill them and free up the memory
	 */
	simple_lock(&obj->uao->vmobjlock);
	obj->uao->pgops->pgo_flush(obj->uao, 0, obj->size,
	    PGO_ALLPAGES | PGO_FREE);
	simple_unlock(&obj->uao->vmobjlock);

	/*
	 * If flush failed, it may have halfway through, so just
	 * always mark as purged
	 */
	atomic_setbits_int(&obj->do_flags, I915_PURGED);
}

void
inteldrm_process_flushing(struct inteldrm_softc *dev_priv,
    u_int32_t flush_domains)
{
	struct inteldrm_obj		*obj_priv, *next;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	mtx_enter(&dev_priv->list_lock);
	for (obj_priv = TAILQ_FIRST(&dev_priv->mm.gpu_write_list);
	    obj_priv != TAILQ_END(&dev_priv->mm.gpu_write_list);
	    obj_priv = next) {
		struct drm_obj *obj = &(obj_priv->obj);

		next = TAILQ_NEXT(obj_priv, write_list);

		if ((obj->write_domain & flush_domains)) {
			TAILQ_REMOVE(&dev_priv->mm.gpu_write_list,
			    obj_priv, write_list);
			atomic_clearbits_int(&obj->do_flags,
			     I915_GPU_WRITE);
			i915_gem_object_move_to_active(obj);
			obj->write_domain = 0;
			/* if we still need the fence, update LRU */
			if (inteldrm_needs_fence(obj_priv)) {
				KASSERT(obj_priv->fence_reg !=
				    I915_FENCE_REG_NONE);
				/* we have a fence, won't sleep, can't fail
				 * since we have the fence we no not need
				 * to have the object held
				 */
				i915_gem_get_fence_reg(obj, 1);
			}

		}
	}
	mtx_leave(&dev_priv->list_lock);
}

/**
 * Moves buffers associated only with the given active seqno from the active
 * to inactive list, potentially freeing them.
 *
 * called with and sleeps with the drm_lock.
 */
void
i915_gem_retire_request(struct inteldrm_softc *dev_priv,
    struct inteldrm_request *request)
{
	struct inteldrm_obj	*obj_priv;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	mtx_enter(&dev_priv->list_lock);
	/* Move any buffers on the active list that are no longer referenced
	 * by the ringbuffer to the flushing/inactive lists as appropriate.  */
	while ((obj_priv  = TAILQ_FIRST(&dev_priv->mm.active_list)) != NULL) {
		struct drm_obj *obj = &obj_priv->obj;

		/* If the seqno being retired doesn't match the oldest in the
		 * list, then the oldest in the list must still be newer than
		 * this seqno.
		 */
		if (obj_priv->last_rendering_seqno != request->seqno)
			break;

		drm_lock_obj(obj);
		/*
		 * If we're now clean and can be read from, move inactive,
		 * else put on the flushing list to signify that we're not
		 * available quite yet.
		 */
		if (obj->write_domain != 0) {
			KASSERT(inteldrm_is_active(obj_priv));
			i915_move_to_tail(obj_priv,
			    &dev_priv->mm.flushing_list);
			i915_gem_object_move_off_active(obj);
			drm_unlock_obj(obj);
		} else {
			/* unlocks object for us and drops ref */
			i915_gem_object_move_to_inactive_locked(obj);
			mtx_enter(&dev_priv->list_lock);
		}
	}
	mtx_leave(&dev_priv->list_lock);
}

void
i915_gem_retire_work_handler(void *arg1, void *unused)
{
	struct inteldrm_softc	*dev_priv = arg1;

	i915_gem_retire_requests(dev_priv);
	if (!TAILQ_EMPTY(&dev_priv->mm.request_list))
		timeout_add_sec(&dev_priv->mm.retire_timer, 1);
}

/*
 * flush and invalidate the provided domains
 * if we have successfully queued a gpu flush, then we return a seqno from
 * the request. else (failed or just cpu flushed)  we return 0.
 */
u_int32_t
i915_gem_flush(struct inteldrm_softc *dev_priv, uint32_t invalidate_domains,
    uint32_t flush_domains)
{
	uint32_t	cmd;
	int		ret = 0;

	if (flush_domains & I915_GEM_DOMAIN_CPU)
		inteldrm_chipset_flush(dev_priv);
	if (((invalidate_domains | flush_domains) & I915_GEM_GPU_DOMAINS) == 0) 
		return (0);
	/*
	 * read/write caches:
	 *
	 * I915_GEM_DOMAIN_RENDER is always invalidated, but is
	 * only flushed if MI_NO_WRITE_FLUSH is unset.  On 965, it is
	 * also flushed at 2d versus 3d pipeline switches.
	 *
	 * read-only caches:
	 *
	 * I915_GEM_DOMAIN_SAMPLER is flushed on pre-965 if
	 * MI_READ_FLUSH is set, and is always flushed on 965.
	 *
	 * I915_GEM_DOMAIN_COMMAND may not exist?
	 *
	 * I915_GEM_DOMAIN_INSTRUCTION, which exists on 965, is
	 * invalidated when MI_EXE_FLUSH is set.
	 *
	 * I915_GEM_DOMAIN_VERTEX, which exists on 965, is
	 * invalidated with every MI_FLUSH.
	 *
	 * TLBs:
	 *
	 * On 965, TLBs associated with I915_GEM_DOMAIN_COMMAND
	 * and I915_GEM_DOMAIN_CPU in are invalidated at PTE write and
	 * I915_GEM_DOMAIN_RENDER and I915_GEM_DOMAIN_SAMPLER
	 * are flushed at any MI_FLUSH.
	 */

	cmd = MI_FLUSH | MI_NO_WRITE_FLUSH;
	if ((invalidate_domains | flush_domains) &
	    I915_GEM_DOMAIN_RENDER)
		cmd &= ~MI_NO_WRITE_FLUSH;
	/*
	 * On the 965, the sampler cache always gets flushed
	 * and this bit is reserved.
	 */
	if (!IS_I965G(dev_priv) &&
	    invalidate_domains & I915_GEM_DOMAIN_SAMPLER)
		cmd |= MI_READ_FLUSH;
	if (invalidate_domains & I915_GEM_DOMAIN_INSTRUCTION)
		cmd |= MI_EXE_FLUSH;

	mtx_enter(&dev_priv->request_lock);
	BEGIN_LP_RING(2);
	OUT_RING(cmd);
	OUT_RING(MI_NOOP);
	ADVANCE_LP_RING();

	/* if this is a gpu flush, process the results */
	if (flush_domains & I915_GEM_GPU_DOMAINS) {
		inteldrm_process_flushing(dev_priv, flush_domains);
		ret = i915_add_request(dev_priv);
	}
	mtx_leave(&dev_priv->request_lock);

	return (ret);
}

struct drm_obj *
i915_gem_find_inactive_object(struct inteldrm_softc *dev_priv,
    size_t min_size)
{
	struct drm_obj		*obj, *best = NULL, *first = NULL;
	struct inteldrm_obj	*obj_priv;

	/*
	 * We don't need references to the object as long as we hold the list
	 * lock, they won't disappear until we release the lock.
	 */
	mtx_enter(&dev_priv->list_lock);
	TAILQ_FOREACH(obj_priv, &dev_priv->mm.inactive_list, list) {
		obj = &obj_priv->obj;
		if (obj->size >= min_size) {
			if ((!inteldrm_is_dirty(obj_priv) ||
			    i915_obj_purgeable(obj_priv)) &&
			    (best == NULL || obj->size < best->size)) {
				best = obj;
				if (best->size == min_size)
					break;
			}
		}
		if (first == NULL)
			first = obj;
	}
	if (best == NULL)
		best = first;
	if (best) {
		drm_ref(&best->uobj);
		/*
		 * if we couldn't grab it, we may as well fail and go
		 * onto the next step for the sake of simplicity.
		 */
		if (drm_try_hold_object(best) == 0) {
			drm_unref(&best->uobj);
			best = NULL;
		}
	}
	mtx_leave(&dev_priv->list_lock);
	return (best);
}

/*
 * i915_gem_get_fence_reg - set up a fence reg for an object
 *
 * When mapping objects through the GTT, userspace wants to be able to write
 * to them without having to worry about swizzling if the object is tiled.
 *
 * This function walks the fence regs looking for a free one, stealing one
 * if it can't find any.
 *
 * It then sets up the reg based on the object's properties: address, pitch
 * and tiling format.
 */
int
i915_gem_get_fence_reg(struct drm_obj *obj, int interruptible)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct inteldrm_obj	*old_obj_priv = NULL;
	struct drm_obj		*old_obj = NULL;
	struct inteldrm_fence	*reg = NULL;
	int			 i, ret, avail;

	/* If our fence is getting used, just update our place in the LRU */
	if (obj_priv->fence_reg != I915_FENCE_REG_NONE) {
		mtx_enter(&dev_priv->fence_lock);
		reg = &dev_priv->fence_regs[obj_priv->fence_reg];

		TAILQ_REMOVE(&dev_priv->mm.fence_list, reg, list);
		TAILQ_INSERT_TAIL(&dev_priv->mm.fence_list, reg, list);
		mtx_leave(&dev_priv->fence_lock);
		return (0);
	}

	DRM_ASSERT_HELD(obj);
	switch (obj_priv->tiling_mode) {
	case I915_TILING_NONE:
		DRM_ERROR("allocating a fence for non-tiled object?\n");
		break;
	case I915_TILING_X:
		if (obj_priv->stride == 0)
			return (EINVAL);
		if (obj_priv->stride & (512 - 1))
			DRM_ERROR("object 0x%08x is X tiled but has non-512B"
			    " pitch\n", obj_priv->gtt_offset);
		break;
	case I915_TILING_Y:
		if (obj_priv->stride == 0)
			return (EINVAL);
		if (obj_priv->stride & (128 - 1))
			DRM_ERROR("object 0x%08x is Y tiled but has non-128B"
			    " pitch\n", obj_priv->gtt_offset);
		break;
	}

again:
	/* First try to find a free reg */
	avail = 0;
	mtx_enter(&dev_priv->fence_lock);
	for (i = dev_priv->fence_reg_start; i < dev_priv->num_fence_regs; i++) {
		reg = &dev_priv->fence_regs[i];
		if (reg->obj == NULL)
			break;

		old_obj_priv = (struct inteldrm_obj *)reg->obj;
		if (old_obj_priv->pin_count == 0)
			avail++;
	}

	/* None available, try to steal one or wait for a user to finish */
	if (i == dev_priv->num_fence_regs) {
		if (avail == 0) {
			mtx_leave(&dev_priv->fence_lock);
			return (ENOMEM);
		}

		TAILQ_FOREACH(reg, &dev_priv->mm.fence_list,
		    list) {
			old_obj = reg->obj;
			old_obj_priv = (struct inteldrm_obj *)old_obj;

			if (old_obj_priv->pin_count)
				continue;

			/* Ref it so that wait_rendering doesn't free it under
			 * us. if we can't hold it, it may change state soon
			 * so grab the next one.
			 */
			drm_ref(&old_obj->uobj);
			if (drm_try_hold_object(old_obj) == 0) {
				drm_unref(&old_obj->uobj);
				continue;
			}

			break;
		}
		mtx_leave(&dev_priv->fence_lock);

		/* if we tried all of them, give it another whirl. we failed to
		 * get a hold this go round.
		 */
		if (reg == NULL)
			goto again;

		ret = i915_gem_object_put_fence_reg(old_obj, interruptible);
		drm_unhold_and_unref(old_obj);
		if (ret != 0)
			return (ret);
		/* we should have freed one up now, so relock and re-search */
		goto again;
	}

	/*
	 * Here we will either have found a register in the first
	 * loop, or we will have waited for one and in the second case
	 * and thus have grabbed the object in question, freed the register
	 * then redone the second loop (having relocked the fence list).
	 * Therefore at this point it is impossible to have a null value
	 * in reg.
	 */
	KASSERT(reg != NULL);

	obj_priv->fence_reg = i;
	reg->obj = obj;
	TAILQ_INSERT_TAIL(&dev_priv->mm.fence_list, reg, list);

	if (IS_GEN6(dev_priv) || IS_GEN7(dev_priv))
		sandybridge_write_fence_reg(reg);
	else if (IS_I965G(dev_priv))
		i965_write_fence_reg(reg);
	else if (IS_I9XX(dev_priv))
		i915_write_fence_reg(reg);
	else
		i830_write_fence_reg(reg);
	mtx_leave(&dev_priv->fence_lock);

	return 0;
}

int
inteldrm_fault(struct drm_obj *obj, struct uvm_faultinfo *ufi, off_t offset,
    vaddr_t vaddr, vm_page_t *pps, int npages, int centeridx,
    vm_prot_t access_type, int flags)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	paddr_t			 paddr;
	int			 lcv, ret;
	int			 write = !!(access_type & VM_PROT_WRITE);
	vm_prot_t		 mapprot;
	boolean_t		 locked = TRUE;

	/* Are we about to suspend?, if so wait until we're done */
	if (dev_priv->sc_flags & INTELDRM_QUIET) {
		/* we're about to sleep, unlock the map etc */
		uvmfault_unlockall(ufi, NULL, &obj->uobj, NULL);
		while (dev_priv->sc_flags & INTELDRM_QUIET)
			tsleep(&dev_priv->flags, 0, "intelflt", 0);
		dev_priv->entries++;
		/*
		 * relock so we're in the same state we would be in if we
		 * were not quiesced before
		 */
		locked = uvmfault_relock(ufi);
		if (locked) {
			drm_lock_obj(obj);
		} else {
			dev_priv->entries--;
			if (dev_priv->sc_flags & INTELDRM_QUIET)
				wakeup(&dev_priv->entries);
			return (VM_PAGER_REFAULT);
		}
	} else {
		dev_priv->entries++;
	}

	if (rw_enter(&dev->dev_lock, RW_NOSLEEP | RW_READ) != 0) {
		uvmfault_unlockall(ufi, NULL, &obj->uobj, NULL);
		DRM_READLOCK();
		locked = uvmfault_relock(ufi);
		if (locked)
			drm_lock_obj(obj);
	}
	if (locked)
		drm_hold_object_locked(obj);
	else { /* obj already unlocked */
		dev_priv->entries--;
		if (dev_priv->sc_flags & INTELDRM_QUIET)
			wakeup(&dev_priv->entries);
		return (VM_PAGER_REFAULT);
	}

	/* we have a hold set on the object now, we can unlock so that we can
	 * sleep in binding and flushing.
	 */
	drm_unlock_obj(obj);

	if (obj_priv->dmamap != NULL &&
	    (obj_priv->gtt_offset & (i915_gem_get_gtt_alignment(obj) - 1) ||
	    (!i915_gem_object_fence_offset_ok(obj, obj_priv->tiling_mode)))) {
		/*
		 * pinned objects are defined to have a sane alignment which can
		 * not change.
		 */
		KASSERT(obj_priv->pin_count == 0);
		if ((ret = i915_gem_object_unbind(obj, 0)))
			goto error;
	}

	if (obj_priv->dmamap == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj, 0, 0);
		if (ret) {
			printf("%s: failed to bind\n", __func__);
			goto error;
		}
		i915_gem_object_move_to_inactive(obj);
	}

	/*
	 * We could only do this on bind so allow for map_buffer_range
	 * unsynchronised objects (where buffer suballocation
	 * is done by the GL application), however it gives coherency problems
	 * normally.
	 */
	ret = i915_gem_object_set_to_gtt_domain(obj, write, 0);
	if (ret) {
		printf("%s: failed to set to gtt (%d)\n",
		    __func__, ret);
		goto error;
	}

	mapprot = ufi->entry->protection;
	/*
	 * if it's only a read fault, we only put ourselves into the gtt
	 * read domain, so make sure we fault again and set ourselves to write.
	 * this prevents us needing userland to do domain management and get
	 * it wrong, and makes us fully coherent with the gpu re mmap.
	 */
	if (write == 0)
		mapprot &= ~VM_PROT_WRITE;
	/* XXX try and  be more efficient when we do this */
	for (lcv = 0 ; lcv < npages ; lcv++, offset += PAGE_SIZE,
	    vaddr += PAGE_SIZE) {
		if ((flags & PGO_ALLPAGES) == 0 && lcv != centeridx)
			continue;

		if (pps[lcv] == PGO_DONTCARE)
			continue;

		paddr = dev->agp->base + obj_priv->gtt_offset + offset;

		if (pmap_enter(ufi->orig_map->pmap, vaddr, paddr,
		    mapprot, PMAP_CANFAIL | mapprot) != 0) {
			drm_unhold_object(obj);
			uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap,
			    NULL, NULL);
			DRM_READUNLOCK();
			dev_priv->entries--;
			if (dev_priv->sc_flags & INTELDRM_QUIET)
				wakeup(&dev_priv->entries);
			uvm_wait("intelflt");
			return (VM_PAGER_REFAULT);
		}
	}
error:
	drm_unhold_object(obj);
	uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, NULL, NULL);
	DRM_READUNLOCK();
	dev_priv->entries--;
	if (dev_priv->sc_flags & INTELDRM_QUIET)
		wakeup(&dev_priv->entries);
	pmap_update(ufi->orig_map->pmap);
	if (ret == EIO) {
		/*
		 * EIO means we're wedged, so upon resetting the gpu we'll
		 * be alright and can refault. XXX only on resettable chips.
		 */
		ret = VM_PAGER_REFAULT;
	} else if (ret) {
		ret = VM_PAGER_ERROR;
	} else {
		ret = VM_PAGER_OK;
	}
	return (ret);
}

void
inteldrm_wipe_mappings(struct drm_obj *obj)
{
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct vm_page		*pg;

	DRM_ASSERT_HELD(obj);
	/* make sure any writes hit the bus before we do whatever change
	 * that prompted us to kill the mappings.
	 */
	DRM_MEMORYBARRIER();
	/* nuke all our mappings. XXX optimise. */
	for (pg = &dev_priv->pgs[atop(obj_priv->gtt_offset)]; pg !=
	    &dev_priv->pgs[atop(obj_priv->gtt_offset + obj->size)]; pg++)
		pmap_page_protect(pg, VM_PROT_NONE);
}

/**
 * Pin an object to the GTT and evaluate the relocations landing in it.
 */
int
i915_gem_object_pin_and_relocate(struct drm_obj *obj,
    struct drm_file *file_priv, struct drm_i915_gem_exec_object2 *entry,
    struct drm_i915_gem_relocation_entry *relocs)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_obj		*target_obj;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	bus_space_handle_t	 bsh;
	int			 i, ret, needs_fence;

	DRM_ASSERT_HELD(obj);
	needs_fence = ((entry->flags & EXEC_OBJECT_NEEDS_FENCE) &&
	    obj_priv->tiling_mode != I915_TILING_NONE);
	if (needs_fence)
		atomic_setbits_int(&obj->do_flags, I915_EXEC_NEEDS_FENCE);

	/* Choose the GTT offset for our buffer and put it there. */
	ret = i915_gem_object_pin(obj, (u_int32_t)entry->alignment,
	    needs_fence);
	if (ret)
		return ret;

	entry->offset = obj_priv->gtt_offset;

	/* Apply the relocations, using the GTT aperture to avoid cache
	 * flushing requirements.
	 */
	for (i = 0; i < entry->relocation_count; i++) {
		struct drm_i915_gem_relocation_entry *reloc = &relocs[i];
		struct inteldrm_obj *target_obj_priv;
		uint32_t reloc_val, reloc_offset;

		target_obj = drm_gem_object_lookup(obj->dev, file_priv,
		    reloc->target_handle);
		/* object must have come before us in the list */
		if (target_obj == NULL) {
			i915_gem_object_unpin(obj);
			return (EBADF);
		}
		if ((target_obj->do_flags & I915_IN_EXEC) == 0) {
			printf("%s: object not already in execbuffer\n",
			__func__);
			ret = EBADF;
			goto err;
		}

		target_obj_priv = (struct inteldrm_obj *)target_obj;

		/* The target buffer should have appeared before us in the
		 * exec_object list, so it should have a GTT space bound by now.
		 */
		if (target_obj_priv->dmamap == 0) {
			DRM_ERROR("No GTT space found for object %d\n",
				  reloc->target_handle);
			ret = EINVAL;
			goto err;
		}

		/* must be in one write domain and one only */
		if (reloc->write_domain & (reloc->write_domain - 1)) {
			ret = EINVAL;
			goto err;
		}
		if (reloc->read_domains & I915_GEM_DOMAIN_CPU ||
		    reloc->write_domain & I915_GEM_DOMAIN_CPU) {
			DRM_ERROR("relocation with read/write CPU domains: "
			    "obj %p target %d offset %d "
			    "read %08x write %08x", obj,
			    reloc->target_handle, (int)reloc->offset,
			    reloc->read_domains, reloc->write_domain);
			ret = EINVAL;
			goto err;
		}

		if (reloc->write_domain && target_obj->pending_write_domain &&
		    reloc->write_domain != target_obj->pending_write_domain) {
			DRM_ERROR("Write domain conflict: "
				  "obj %p target %d offset %d "
				  "new %08x old %08x\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset,
				  reloc->write_domain,
				  target_obj->pending_write_domain);
			ret = EINVAL;
			goto err;
		}

		target_obj->pending_read_domains |= reloc->read_domains;
		target_obj->pending_write_domain |= reloc->write_domain;


		if (reloc->offset > obj->size - 4) {
			DRM_ERROR("Relocation beyond object bounds: "
				  "obj %p target %d offset %d size %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset, (int) obj->size);
			ret = EINVAL;
			goto err;
		}
		if (reloc->offset & 3) {
			DRM_ERROR("Relocation not 4-byte aligned: "
				  "obj %p target %d offset %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset);
			ret = EINVAL;
			goto err;
		}

		if (reloc->delta > target_obj->size) {
			DRM_ERROR("reloc larger than target\n");
			ret = EINVAL;
			goto err;
		}

		/* Map the page containing the relocation we're going to
		 * perform.
		 */
		reloc_offset = obj_priv->gtt_offset + reloc->offset;
		reloc_val = target_obj_priv->gtt_offset + reloc->delta;

		if (target_obj_priv->gtt_offset == reloc->presumed_offset) {
			drm_gem_object_unreference(target_obj);
			 continue;
		}

		ret = i915_gem_object_set_to_gtt_domain(obj, 1, 1);
		if (ret != 0)
			goto err;

		if ((ret = agp_map_subregion(dev_priv->agph,
		    trunc_page(reloc_offset), PAGE_SIZE, &bsh)) != 0) {
			DRM_ERROR("map failed...\n");
			goto err;
		}

		bus_space_write_4(dev_priv->bst, bsh, reloc_offset & PAGE_MASK,
		     reloc_val);

		reloc->presumed_offset = target_obj_priv->gtt_offset;

		agp_unmap_subregion(dev_priv->agph, bsh, PAGE_SIZE);
		drm_gem_object_unreference(target_obj);
	}

	return 0;

err:
	/* we always jump to here mid-loop */
	drm_gem_object_unreference(target_obj);
	i915_gem_object_unpin(obj);
	return (ret);
}

/** Dispatch a batchbuffer to the ring
 */
void
i915_dispatch_gem_execbuffer(struct drm_device *dev,
    struct drm_i915_gem_execbuffer2 *exec, uint64_t exec_offset)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	uint32_t		 exec_start, exec_len;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	exec_start = (uint32_t)exec_offset + exec->batch_start_offset;
	exec_len = (uint32_t)exec->batch_len;

	if (IS_I830(dev_priv) || IS_845G(dev_priv)) {
		BEGIN_LP_RING(6);
		OUT_RING(MI_BATCH_BUFFER);
		OUT_RING(exec_start | MI_BATCH_NON_SECURE);
		OUT_RING(exec_start + exec_len - 4);
		OUT_RING(MI_NOOP);
	} else {
		BEGIN_LP_RING(4);
		if (IS_I965G(dev_priv)) {
			if (IS_GEN6(dev_priv) || IS_GEN7(dev_priv))
				OUT_RING(MI_BATCH_BUFFER_START |
				    MI_BATCH_NON_SECURE_I965);
			else
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6) |
				    MI_BATCH_NON_SECURE_I965);
			OUT_RING(exec_start);
		} else {
			OUT_RING(MI_BATCH_BUFFER_START | (2 << 6));
			OUT_RING(exec_start | MI_BATCH_NON_SECURE);
		}
	}

	/*
	 * Ensure that the commands in the batch buffer are
	 * finished before the interrupt fires (from a subsequent request
	 * added). We get back a seqno representing the execution of the
	 * current buffer, which we can wait on.  We would like to mitigate
	 * these interrupts, likely by only creating seqnos occasionally
	 * (so that we have *some* interrupts representing completion of
	 * buffers that we can wait on when trying to clear up gtt space).
	 */
	OUT_RING(MI_FLUSH | MI_NO_WRITE_FLUSH);
	OUT_RING(MI_NOOP);
	ADVANCE_LP_RING();
	/*
	 * move to active associated all previous buffers with the seqno
	 * that this call will emit. so we don't need the return. If it fails
	 * then the next seqno will take care of it.
	 */
	(void)i915_add_request(dev_priv);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
}

int
i915_gem_get_relocs_from_user(struct drm_i915_gem_exec_object2 *exec_list,
    u_int32_t buffer_count, struct drm_i915_gem_relocation_entry **relocs)
{
	u_int32_t	reloc_count = 0, reloc_index = 0, i;
	int		ret;

	*relocs = NULL;
	for (i = 0; i < buffer_count; i++) {
		if (reloc_count + exec_list[i].relocation_count < reloc_count)
			return (EINVAL);
		reloc_count += exec_list[i].relocation_count;
	}

	if (reloc_count == 0)
		return (0);

	if (SIZE_MAX / reloc_count < sizeof(**relocs))
		return (EINVAL);
	*relocs = drm_alloc(reloc_count * sizeof(**relocs));
	for (i = 0; i < buffer_count; i++) {
		if ((ret = copyin((void *)(uintptr_t)exec_list[i].relocs_ptr,
		    &(*relocs)[reloc_index], exec_list[i].relocation_count *
		    sizeof(**relocs))) != 0) {
			drm_free(*relocs);
			*relocs = NULL;
			return (ret);
		}
		reloc_index += exec_list[i].relocation_count;
	}

	return (0);
}

int
i915_gem_put_relocs_to_user(struct drm_i915_gem_exec_object2 *exec_list,
    u_int32_t buffer_count, struct drm_i915_gem_relocation_entry *relocs)
{
	u_int32_t	reloc_count = 0, i;
	int		ret = 0;

	if (relocs == NULL)
		return (0);

	for (i = 0; i < buffer_count; i++) {
		if ((ret = copyout(&relocs[reloc_count],
		    (void *)(uintptr_t)exec_list[i].relocs_ptr,
		    exec_list[i].relocation_count * sizeof(*relocs))) != 0)
			break;
		reloc_count += exec_list[i].relocation_count;
	}

	drm_free(relocs);

	return (ret);
}

void
inteldrm_quiesce(struct inteldrm_softc *dev_priv)
{
	/*
	 * Right now we depend on X vt switching, so we should be
	 * already suspended, but fallbacks may fault, etc.
	 * Since we can't readback the gtt to reset what we have, make
	 * sure that everything is unbound.
	 */
	KASSERT(dev_priv->mm.suspended);
	KASSERT(dev_priv->ring.ring_obj == NULL);
	atomic_setbits_int(&dev_priv->sc_flags, INTELDRM_QUIET);
	while (dev_priv->entries)
		tsleep(&dev_priv->entries, 0, "intelquiet", 0);
	/*
	 * nothing should be dirty WRT the chip, only stuff that's bound
	 * for gtt mapping. Nothing should be pinned over vt switch, if it
	 * is then rendering corruption will occur due to api misuse, shame.
	 */
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.flushing_list));
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.active_list));
	/* Disabled because root could panic the kernel if this was enabled */
	/* KASSERT(dev->pin_count == 0); */

	/* can't fail since uninterruptible */
	(void)i915_gem_evict_inactive(dev_priv, 0);
}

int
i915_gem_init_hws(struct inteldrm_softc *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;
	int			 ret;

	/* If we need a physical address for the status page, it's already
	 * initialized at driver load time.
	 */
	if (!I915_NEED_GFX_HWS(dev_priv))
		return 0;

	obj = drm_gem_object_alloc(dev, 4096);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate status page\n");
		return (ENOMEM);
	}
	obj_priv = (struct inteldrm_obj *)obj;
	drm_hold_object(obj);
	/*
	 * snooped gtt mapping please .
	 * Normally this flag is only to dmamem_map, but it's been overloaded
	 * for the agp mapping
	 */
	obj_priv->dma_flags = BUS_DMA_COHERENT | BUS_DMA_READ;

	ret = i915_gem_object_pin(obj, 4096, 0);
	if (ret != 0) {
		drm_unhold_and_unref(obj);
		return ret;
	}

	dev_priv->hw_status_page = (void *)vm_map_min(kernel_map);
	obj->uao->pgops->pgo_reference(obj->uao);
	if ((ret = uvm_map(kernel_map, (vaddr_t *)&dev_priv->hw_status_page,
	    PAGE_SIZE, obj->uao, 0, 0, UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW,
	    UVM_INH_SHARE, UVM_ADV_RANDOM, 0))) != 0)
	if (ret != 0) {
		DRM_ERROR("Failed to map status page.\n");
		obj->uao->pgops->pgo_detach(obj->uao);
		i915_gem_object_unpin(obj);
		drm_unhold_and_unref(obj);
		return (EINVAL);
	}
	drm_unhold_object(obj);
	dev_priv->hws_obj = obj;
	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);
	I915_WRITE(HWS_PGA, obj_priv->gtt_offset);
	I915_READ(HWS_PGA); /* posting read */
	DRM_DEBUG("hws offset: 0x%08x\n", obj_priv->gtt_offset);

	return 0;
}

void
i915_gem_cleanup_hws(struct inteldrm_softc *dev_priv)
{
	struct drm_obj		*obj;

	if (!I915_NEED_GFX_HWS(dev_priv) || dev_priv->hws_obj == NULL)
		return;

	obj = dev_priv->hws_obj;

	uvm_unmap(kernel_map, (vaddr_t)dev_priv->hw_status_page,
	    (vaddr_t)dev_priv->hw_status_page + PAGE_SIZE);
	dev_priv->hw_status_page = NULL;
	drm_hold_object(obj);
	i915_gem_object_unpin(obj);
	drm_unhold_and_unref(obj);
	dev_priv->hws_obj = NULL;

	/* Write high address into HWS_PGA when disabling. */
	I915_WRITE(HWS_PGA, 0x1ffff000);
}

int
i915_gem_init_ringbuffer(struct inteldrm_softc *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;
	int			 ret;

	ret = i915_gem_init_hws(dev_priv);
	if (ret != 0)
		return ret;

	obj = drm_gem_object_alloc(dev, 128 * 1024);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate ringbuffer\n");
		ret = ENOMEM;
		goto delhws;
	}
	drm_hold_object(obj);
	obj_priv = (struct inteldrm_obj *)obj;

	ret = i915_gem_object_pin(obj, 4096, 0);
	if (ret != 0)
		goto unref;

	/* Set up the kernel mapping for the ring. */
	dev_priv->ring.size = obj->size;

	if ((ret = agp_map_subregion(dev_priv->agph, obj_priv->gtt_offset,
	    obj->size, &dev_priv->ring.bsh)) != 0) {
		DRM_INFO("can't map ringbuffer\n");
		goto unpin;
	}
	dev_priv->ring.ring_obj = obj;

	if ((ret = inteldrm_start_ring(dev_priv)) != 0)
		goto unmap;

	drm_unhold_object(obj);
	return (0);

unmap:
	agp_unmap_subregion(dev_priv->agph, dev_priv->ring.bsh, obj->size);
unpin:
	memset(&dev_priv->ring, 0, sizeof(dev_priv->ring));
	i915_gem_object_unpin(obj);
unref:
	drm_unhold_and_unref(obj);
delhws:
	i915_gem_cleanup_hws(dev_priv);
	return (ret);
}

int
inteldrm_start_ring(struct inteldrm_softc *dev_priv)
{
	struct drm_obj		*obj = dev_priv->ring.ring_obj;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	u_int32_t		 head;

	/* Stop the ring if it's running. */
	I915_WRITE(PRB0_CTL, 0);
	I915_WRITE(PRB0_TAIL, 0);
	I915_WRITE(PRB0_HEAD, 0);

	/* Initialize the ring. */
	I915_WRITE(PRB0_START, obj_priv->gtt_offset);
	head = I915_READ(PRB0_HEAD) & HEAD_ADDR;

	/* G45 ring initialisation fails to reset head to zero */
	if (head != 0) {
		I915_WRITE(PRB0_HEAD, 0);
		DRM_DEBUG("Forced ring head to zero ctl %08x head %08x"
		    "tail %08x start %08x\n", I915_READ(PRB0_CTL),
		    I915_READ(PRB0_HEAD), I915_READ(PRB0_TAIL),
		    I915_READ(PRB0_START));
	}

	I915_WRITE(PRB0_CTL, ((obj->size - 4096) & RING_NR_PAGES) |
	    RING_NO_REPORT | RING_VALID);

	head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	/* If ring head still != 0, the ring is dead */
	if (head != 0) {
		DRM_ERROR("Ring initialisation failed: ctl %08x head %08x"
		    "tail %08x start %08x\n", I915_READ(PRB0_CTL),
		    I915_READ(PRB0_HEAD), I915_READ(PRB0_TAIL),
		    I915_READ(PRB0_START));
		return (EIO);
	}

	/* Update our cache of the ring state */
	inteldrm_update_ring(dev_priv);

	if (IS_GEN6(dev_priv) || IS_GEN7(dev_priv))
		I915_WRITE(MI_MODE | MI_FLUSH_ENABLE << 16 | MI_FLUSH_ENABLE,
		    (VS_TIMER_DISPATCH) << 15 | VS_TIMER_DISPATCH);
	else if (IS_I9XX(dev_priv) && !IS_GEN3(dev_priv))
		I915_WRITE(MI_MODE, (VS_TIMER_DISPATCH) << 15 |
		    VS_TIMER_DISPATCH);

	return (0);
}

void
inteldrm_timeout(void *arg)
{
	struct inteldrm_softc	*dev_priv = arg;

	if (workq_add_task(dev_priv->workq, 0, i915_gem_retire_work_handler,
	    dev_priv, NULL) == ENOMEM)
		DRM_ERROR("failed to run retire handler\n");
}

/*
 * handle hung hardware, or error interrupts. for now print debug info.
 */
void
inteldrm_error(struct inteldrm_softc *dev_priv)
{
	u_int32_t	eir, ipeir;
	u_int8_t	reset = GRDOM_RENDER;
	char 		*errbitstr;

	eir = I915_READ(EIR);
	if (eir == 0)
		return;

	if (IS_IRONLAKE(dev_priv)) {
		errbitstr = "\20\x05PTEE\x04MPVE\x03CPVE";
	} else if (IS_G4X(dev_priv)) {
		errbitstr = "\20\x10 BCSINSTERR\x06PTEERR\x05MPVERR\x04CPVERR"
		     "\x03 BCSPTEERR\x02REFRESHERR\x01INSTERR";
	} else {
		errbitstr = "\20\x5PTEERR\x2REFRESHERR\x1INSTERR";
	}

	printf("render error detected, EIR: %b\n", eir, errbitstr);
	if (IS_IRONLAKE(dev_priv) || IS_GEN6(dev_priv) || IS_GEN7(dev_priv)) {
		if (eir & I915_ERROR_PAGE_TABLE) {
			dev_priv->mm.wedged = 1;
			reset = GRDOM_FULL;
		}
	} else {
		if (IS_G4X(dev_priv)) {
			if (eir & (GM45_ERROR_MEM_PRIV | GM45_ERROR_CP_PRIV)) {
				printf("  IPEIR: 0x%08x\n",
				    I915_READ(IPEIR_I965));
				printf("  IPEHR: 0x%08x\n",
				    I915_READ(IPEHR_I965));
				printf("  INSTDONE: 0x%08x\n",
				    I915_READ(INSTDONE_I965));
				printf("  INSTPS: 0x%08x\n",
				    I915_READ(INSTPS));
				printf("  INSTDONE1: 0x%08x\n",
				    I915_READ(INSTDONE1));
				printf("  ACTHD: 0x%08x\n",
				    I915_READ(ACTHD_I965));
			}
			if (eir & GM45_ERROR_PAGE_TABLE) {
				printf("  PGTBL_ER: 0x%08x\n",
				    I915_READ(PGTBL_ER));
				dev_priv->mm.wedged = 1;
				reset = GRDOM_FULL;

			}
		} else if (IS_I9XX(dev_priv) && eir & I915_ERROR_PAGE_TABLE) {
			printf("  PGTBL_ER: 0x%08x\n", I915_READ(PGTBL_ER));
			dev_priv->mm.wedged = 1;
			reset = GRDOM_FULL;
		}
		if (eir & I915_ERROR_MEMORY_REFRESH) {
			printf("PIPEASTAT: 0x%08x\n",
			    I915_READ(_PIPEASTAT));
			printf("PIPEBSTAT: 0x%08x\n",
			    I915_READ(_PIPEBSTAT));
		}
		if (eir & I915_ERROR_INSTRUCTION) {
			printf("  INSTPM: 0x%08x\n",
			       I915_READ(INSTPM));
			if (!IS_I965G(dev_priv)) {
				ipeir = I915_READ(IPEIR);

				printf("  IPEIR: 0x%08x\n",
				       I915_READ(IPEIR));
				printf("  IPEHR: 0x%08x\n",
					   I915_READ(IPEHR));
				printf("  INSTDONE: 0x%08x\n",
					   I915_READ(INSTDONE));
				printf("  ACTHD: 0x%08x\n",
					   I915_READ(ACTHD));
				I915_WRITE(IPEIR, ipeir);
				(void)I915_READ(IPEIR);
			} else {
				ipeir = I915_READ(IPEIR_I965);

				printf("  IPEIR: 0x%08x\n",
				       I915_READ(IPEIR_I965));
				printf("  IPEHR: 0x%08x\n",
				       I915_READ(IPEHR_I965));
				printf("  INSTDONE: 0x%08x\n",
				       I915_READ(INSTDONE_I965));
				printf("  INSTPS: 0x%08x\n",
				       I915_READ(INSTPS));
				printf("  INSTDONE1: 0x%08x\n",
				       I915_READ(INSTDONE1));
				printf("  ACTHD: 0x%08x\n",
				       I915_READ(ACTHD_I965));
				I915_WRITE(IPEIR_I965, ipeir);
				(void)I915_READ(IPEIR_I965);
			}
		}
	}

	I915_WRITE(EIR, eir);
	eir = I915_READ(EIR);
	/*
	 * nasty errors don't clear and need a reset, mask them until we reset
	 * else we'll get infinite interrupt storms.
	 */
	if (eir) {
		if (dev_priv->mm.wedged == 0)
			DRM_ERROR("EIR stuck: 0x%08x, masking\n", eir);
		I915_WRITE(EMR, I915_READ(EMR) | eir);
		if (IS_IRONLAKE(dev_priv) || IS_GEN6(dev_priv) ||
		    IS_GEN7(dev_priv)) {
			I915_WRITE(GTIIR, GT_MASTER_ERROR);
		} else {
			I915_WRITE(IIR,
			    I915_RENDER_COMMAND_PARSER_ERROR_INTERRUPT);
		}
	}
	/*
	 * if it was a pagetable error, or we were called from hangcheck, then
	 * reset the gpu.
	 */
	if (dev_priv->mm.wedged && workq_add_task(dev_priv->workq, 0,
	    inteldrm_hung, dev_priv, (void *)(uintptr_t)reset) == ENOMEM)
		DRM_INFO("failed to schedule reset task\n");

}

void
inteldrm_hung(void *arg, void *reset_type)
{
	struct inteldrm_softc	*dev_priv = arg;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	struct inteldrm_obj	*obj_priv;
	u_int8_t		 reset = (u_int8_t)(uintptr_t)reset_type;

	DRM_LOCK();
	if (HAS_RESET(dev_priv)) {
		DRM_INFO("resetting gpu: ");
		inteldrm_965_reset(dev_priv, reset);
		printf("done!\n");
	} else
		printf("no reset function for chipset.\n");

	/*
	 * Clear out all of the requests and make everything inactive.
	 */
	i915_gem_retire_requests(dev_priv);

	/*
	 * Clear the active and flushing lists to inactive. Since
	 * we've reset the hardware then they're not going to get
	 * flushed or completed otherwise. nuke the domains since
	 * they're now irrelavent.
	 */
	mtx_enter(&dev_priv->list_lock);
	while ((obj_priv = TAILQ_FIRST(&dev_priv->mm.flushing_list)) != NULL) {
		drm_lock_obj(&obj_priv->obj);
		if (obj_priv->obj.write_domain & I915_GEM_GPU_DOMAINS) {
			TAILQ_REMOVE(&dev_priv->mm.gpu_write_list,
			    obj_priv, write_list);
			atomic_clearbits_int(&obj_priv->obj.do_flags,
			    I915_GPU_WRITE);
			obj_priv->obj.write_domain &= ~I915_GEM_GPU_DOMAINS;
		}
		/* unlocks object and list */
		i915_gem_object_move_to_inactive_locked(&obj_priv->obj);
		mtx_enter(&dev_priv->list_lock);
	}
	mtx_leave(&dev_priv->list_lock);

	/* unbind everything */
	(void)i915_gem_evict_inactive(dev_priv, 0);

	if (HAS_RESET(dev_priv))
		dev_priv->mm.wedged = 0;
	DRM_UNLOCK();
}

void
inteldrm_hangcheck(void *arg)
{
	struct inteldrm_softc	*dev_priv = arg;
	u_int32_t		 acthd, instdone, instdone1;

	/* are we idle? no requests, or ring is empty */
	if (TAILQ_EMPTY(&dev_priv->mm.request_list) ||
	    (I915_READ(PRB0_HEAD) & HEAD_ADDR) ==
	    (I915_READ(PRB0_TAIL) & TAIL_ADDR)) {
		dev_priv->mm.hang_cnt = 0;
		return;
	}

	if (IS_I965G(dev_priv)) {
		acthd = I915_READ(ACTHD_I965);
		instdone = I915_READ(INSTDONE_I965);
		instdone1 = I915_READ(INSTDONE1);
	} else {
		acthd = I915_READ(ACTHD);
		instdone = I915_READ(INSTDONE);
		instdone1 = 0;
	}

	/* if we've hit ourselves before and the hardware hasn't moved, hung. */
	if (dev_priv->mm.last_acthd == acthd &&
	    dev_priv->mm.last_instdone == instdone &&
	    dev_priv->mm.last_instdone1 == instdone1) {
		/* if that's twice we didn't hit it, then we're hung */
		if (++dev_priv->mm.hang_cnt >= 2) {
			if (!IS_GEN2(dev_priv)) {
				u_int32_t tmp = I915_READ(PRB0_CTL);
				if (tmp & RING_WAIT) {
					I915_WRITE(PRB0_CTL, tmp);
					(void)I915_READ(PRB0_CTL);
					goto out;
				}
			}
			dev_priv->mm.hang_cnt = 0;
			/* XXX atomic */
			dev_priv->mm.wedged = 1; 
			DRM_INFO("gpu hung!\n");
			/* XXX locking */
			wakeup(dev_priv);
			inteldrm_error(dev_priv);
			return;
		}
	} else {
		dev_priv->mm.hang_cnt = 0;

		dev_priv->mm.last_acthd = acthd;
		dev_priv->mm.last_instdone = instdone;
		dev_priv->mm.last_instdone1 = instdone1;
	}
out:
	/* Set ourselves up again, in case we haven't added another batch */
	timeout_add_msec(&dev_priv->mm.hang_timer, 750);
}

void
i915_move_to_tail(struct inteldrm_obj *obj_priv, struct i915_gem_list *head)
{
	i915_list_remove(obj_priv);
	TAILQ_INSERT_TAIL(head, obj_priv, list);
	obj_priv->current_list = head;
}

void
i915_list_remove(struct inteldrm_obj *obj_priv)
{
	if (obj_priv->current_list != NULL)
		TAILQ_REMOVE(obj_priv->current_list, obj_priv, list);
	obj_priv->current_list = NULL;
}

/*
 *
 * Support for managing tiling state of buffer objects.
 *
 * The idea behind tiling is to increase cache hit rates by rearranging
 * pixel data so that a group of pixel accesses are in the same cacheline.
 * Performance improvement from doing this on the back/depth buffer are on
 * the order of 30%.
 *
 * Intel architectures make this somewhat more complicated, though, by
 * adjustments made to addressing of data when the memory is in interleaved
 * mode (matched pairs of DIMMS) to improve memory bandwidth.
 * For interleaved memory, the CPU sends every sequential 64 bytes
 * to an alternate memory channel so it can get the bandwidth from both.
 *
 * The GPU also rearranges its accesses for increased bandwidth to interleaved
 * memory, and it matches what the CPU does for non-tiled.  However, when tiled
 * it does it a little differently, since one walks addresses not just in the
 * X direction but also Y.  So, along with alternating channels when bit
 * 6 of the address flips, it also alternates when other bits flip --  Bits 9
 * (every 512 bytes, an X tile scanline) and 10 (every two X tile scanlines)
 * are common to both the 915 and 965-class hardware.
 *
 * The CPU also sometimes XORs in higher bits as well, to improve
 * bandwidth doing strided access like we do so frequently in graphics.  This
 * is called "Channel XOR Randomization" in the MCH documentation.  The result
 * is that the CPU is XORing in either bit 11 or bit 17 to bit 6 of its address
 * decode.
 *
 * All of this bit 6 XORing has an effect on our memory management,
 * as we need to make sure that the 3d driver can correctly address object
 * contents.
 *
 * If we don't have interleaved memory, all tiling is safe and no swizzling is
 * required.
 *
 * When bit 17 is XORed in, we simply refuse to tile at all.  Bit
 * 17 is not just a page offset, so as we page an object out and back in,
 * individual pages in it will have different bit 17 addresses, resulting in
 * each 64 bytes being swapped with its neighbor!
 *
 * Otherwise, if interleaved, we have to tell the 3d driver what the address
 * swizzling it needs to do is, since it's writing with the CPU to the pages
 * (bit 6 and potentially bit 11 XORed in), and the GPU is reading from the
 * pages (bit 6, 9, and 10 XORed in), resulting in a cumulative bit swizzling
 * required by the CPU of XORing in bit 6, 9, 10, and potentially 11, in order
 * to match what the GPU expects.
 */

#define MCHBAR_I915	0x44
#define MCHBAR_I965	0x48
#define	MCHBAR_SIZE	(4*4096)

#define	DEVEN_REG	0x54
#define	DEVEN_MCHBAR_EN	(1 << 28)


/*
 * Check the MCHBAR on the host bridge is enabled, and if not allocate it.
 * we do not need to actually map it because we access the bar through it's
 * mirror on the IGD, however, if it is disabled or not allocated then
 * the mirror does not work. *sigh*.
 *
 * we return a trinary state:
 * 0 = already enabled, or can not enable
 * 1 = enabled, needs disable
 * 2 = enabled, needs disable and free.
 */
int
inteldrm_setup_mchbar(struct inteldrm_softc *dev_priv,
    struct pci_attach_args *bpa)
{
	u_int64_t	mchbar_addr;
	pcireg_t	tmp, low, high = 0;
	u_long		addr;
	int		reg = IS_I965G(dev_priv) ? MCHBAR_I965 : MCHBAR_I915;
	int		ret = 1, enabled = 0;

	if (IS_I915G(dev_priv) || IS_I915GM(dev_priv)) {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, DEVEN_REG);
		enabled = !!(tmp & DEVEN_MCHBAR_EN);
	} else {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		enabled = tmp & 1;
	}

	if (enabled) {
		return (0);
	}

	if (IS_I965G(dev_priv))
		high = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg + 4);
	low = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
	mchbar_addr = ((u_int64_t)high << 32) | low;

	/*
	 * XXX need to check to see if it's allocated in the pci resources,
	 * right now we just check to see if there's any address there
	 *
	 * if there's no address, then we allocate one.
	 * note that we can't just use pci_mapreg_map here since some intel
	 * BARs are special in that they set bit 0 to show they're enabled,
	 * this is not handled by generic pci code.
	 */
	if (mchbar_addr == 0) {
		addr = (u_long)mchbar_addr;
		if (bpa->pa_memex == NULL || extent_alloc(bpa->pa_memex,
	            MCHBAR_SIZE, MCHBAR_SIZE, 0, 0, 0, &addr)) {
			return (0); /* just say we don't need to disable */
		} else {
			mchbar_addr = addr;
			ret = 2;
			/* We've allocated it, now fill in the BAR again */
			if (IS_I965G(dev_priv))
				pci_conf_write(bpa->pa_pc, bpa->pa_tag,
				    reg + 4, upper_32_bits(mchbar_addr));
			pci_conf_write(bpa->pa_pc, bpa->pa_tag,
			    reg, mchbar_addr & 0xffffffff);
		}
	}
	/* set the enable bit */
	if (IS_I915G(dev_priv) || IS_I915GM(dev_priv)) {
		pci_conf_write(bpa->pa_pc, bpa->pa_tag, DEVEN_REG,
		    tmp | DEVEN_MCHBAR_EN);
	} else {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		pci_conf_write(bpa->pa_pc, bpa->pa_tag, reg, tmp | 1);
	}

	return (ret);
}

/*
 * we take the trinary returned from inteldrm_setup_mchbar and clean up after
 * it.
 */
void
inteldrm_teardown_mchbar(struct inteldrm_softc *dev_priv,
    struct pci_attach_args *bpa, int disable)
{
	u_int64_t	mchbar_addr;
	pcireg_t	tmp, low, high = 0;
	int		reg = IS_I965G(dev_priv) ? MCHBAR_I965 : MCHBAR_I915;

	switch(disable) {
	case 2:
		if (IS_I965G(dev_priv))
			high = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg + 4);
		low = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		mchbar_addr = ((u_int64_t)high << 32) | low;
		if (bpa->pa_memex)
			extent_free(bpa->pa_memex, mchbar_addr, MCHBAR_SIZE, 0);
		/* FALLTHROUGH */
	case 1:
		if (IS_I915G(dev_priv) || IS_I915GM(dev_priv)) {
			tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, DEVEN_REG);
			tmp &= ~DEVEN_MCHBAR_EN;
			pci_conf_write(bpa->pa_pc, bpa->pa_tag, DEVEN_REG, tmp);
		} else {
			tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
			tmp &= ~1;
			pci_conf_write(bpa->pa_pc, bpa->pa_tag, reg, tmp);
		}
		break;
	case 0:
	default:
		break;
	};
}

/**
 * Detects bit 6 swizzling of address lookup between IGD access and CPU
 * access through main memory.
 */
void
inteldrm_detect_bit_6_swizzle(struct inteldrm_softc *dev_priv,
    struct pci_attach_args *bpa)
{
	uint32_t	swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
	uint32_t	swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;
	int		need_disable;

	if (!IS_I9XX(dev_priv)) {
		/* As far as we know, the 865 doesn't have these bit 6
		 * swizzling issues.
		 */
		swizzle_x = I915_BIT_6_SWIZZLE_NONE;
		swizzle_y = I915_BIT_6_SWIZZLE_NONE;
	} else if (HAS_PCH_SPLIT(dev_priv)) {
		/*
		 * On ironlake and sandybridge the swizzling is the same
		 * no matter what the DRAM config
		 */
		swizzle_x = I915_BIT_6_SWIZZLE_9_10;
		swizzle_y = I915_BIT_6_SWIZZLE_9;
	} else if (IS_MOBILE(dev_priv)) {
		uint32_t dcc;

		/* try to enable MCHBAR, a lot of biosen disable it */
		need_disable = inteldrm_setup_mchbar(dev_priv, bpa);

		/* On 915-945 and GM965, channel interleave by the CPU is
		 * determined by DCC.  The CPU will alternate based on bit 6
		 * in interleaved mode, and the GPU will then also alternate
		 * on bit 6, 9, and 10 for X, but the CPU may also optionally
		 * alternate based on bit 17 (XOR not disabled and XOR
		 * bit == 17).
		 */
		dcc = I915_READ(DCC);
		switch (dcc & DCC_ADDRESSING_MODE_MASK) {
		case DCC_ADDRESSING_MODE_SINGLE_CHANNEL:
		case DCC_ADDRESSING_MODE_DUAL_CHANNEL_ASYMMETRIC:
			swizzle_x = I915_BIT_6_SWIZZLE_NONE;
			swizzle_y = I915_BIT_6_SWIZZLE_NONE;
			break;
		case DCC_ADDRESSING_MODE_DUAL_CHANNEL_INTERLEAVED:
			if (dcc & DCC_CHANNEL_XOR_DISABLE) {
				/* This is the base swizzling by the GPU for
				 * tiled buffers.
				 */
				swizzle_x = I915_BIT_6_SWIZZLE_9_10;
				swizzle_y = I915_BIT_6_SWIZZLE_9;
			} else if ((dcc & DCC_CHANNEL_XOR_BIT_17) == 0) {
				/* Bit 11 swizzling by the CPU in addition. */
				swizzle_x = I915_BIT_6_SWIZZLE_9_10_11;
				swizzle_y = I915_BIT_6_SWIZZLE_9_11;
			} else {
				/* Bit 17 swizzling by the CPU in addition. */
				swizzle_x = I915_BIT_6_SWIZZLE_9_10_17;
				swizzle_y = I915_BIT_6_SWIZZLE_9_17;
			}
			break;
		}
		if (dcc == 0xffffffff) {
			DRM_ERROR("Couldn't read from MCHBAR.  "
				  "Disabling tiling.\n");
			swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
			swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;
		}

		inteldrm_teardown_mchbar(dev_priv, bpa, need_disable);
	} else {
		/* The 965, G33, and newer, have a very flexible memory
		 * configuration. It will enable dual-channel mode
		 * (interleaving) on as much memory as it can, and the GPU
		 * will additionally sometimes enable different bit 6
		 * swizzling for tiled objects from the CPU.
		 *
		 * Here's what I found on G965:
		 *
		 *    slot fill			memory size	swizzling
		 * 0A   0B	1A	1B	1-ch	2-ch
		 * 512	0	0	0	512	0	O
		 * 512	0	512	0	16	1008	X
		 * 512	0	0	512	16	1008	X
		 * 0	512	0	512	16	1008	X
		 * 1024	1024	1024	0	2048	1024	O
		 *
		 * We could probably detect this based on either the DRB
		 * matching, which was the case for the swizzling required in
		 * the table above, or from the 1-ch value being less than
		 * the minimum size of a rank.
		 */
		if (I915_READ16(C0DRB3) != I915_READ16(C1DRB3)) {
			swizzle_x = I915_BIT_6_SWIZZLE_NONE;
			swizzle_y = I915_BIT_6_SWIZZLE_NONE;
		} else {
			swizzle_x = I915_BIT_6_SWIZZLE_9_10;
			swizzle_y = I915_BIT_6_SWIZZLE_9;
		}
	}

	dev_priv->mm.bit_6_swizzle_x = swizzle_x;
	dev_priv->mm.bit_6_swizzle_y = swizzle_y;
}

int
inteldrm_swizzle_page(struct vm_page *pg)
{
	vaddr_t	 va;
	int	 i;
	u_int8_t temp[64], *vaddr;

#if defined (__HAVE_PMAP_DIRECT)
	va = pmap_map_direct(pg);
#else
	va = uvm_km_valloc(kernel_map, PAGE_SIZE);
	if (va == 0)
		return (ENOMEM);
	pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg), UVM_PROT_RW);
	pmap_update(pmap_kernel());
#endif
	vaddr = (u_int8_t *)va;

	for (i = 0; i < PAGE_SIZE; i += 128) {
		memcpy(temp, &vaddr[i], 64);
		memcpy(&vaddr[i], &vaddr[i + 64], 64);
		memcpy(&vaddr[i + 64], temp, 64);
	}

#if defined (__HAVE_PMAP_DIRECT)
	pmap_unmap_direct(va);
#else
	pmap_kremove(va, PAGE_SIZE);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, PAGE_SIZE);
#endif
	return (0);
}

void
i915_gem_bit_17_swizzle(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct vm_page		*pg;
	bus_dma_segment_t	*segp;
	int			 page_count = obj->size >> PAGE_SHIFT;
	int                      i, n, ret;

	if (dev_priv->mm.bit_6_swizzle_x != I915_BIT_6_SWIZZLE_9_10_17 ||
	    obj_priv->bit_17 == NULL)
		return;

	segp = &obj_priv->dma_segs[0];
	n = 0;
	for (i = 0; i < page_count; i++) {
		/* compare bit 17 with previous one (in case we swapped).
		 * if they don't match we'll have to swizzle the page
		 */
		if ((((segp->ds_addr + n) >> 17) & 0x1) !=
		    test_bit(i, obj_priv->bit_17)) {
			/* XXX move this to somewhere where we already have pg */
			pg = PHYS_TO_VM_PAGE(segp->ds_addr + n);
			KASSERT(pg != NULL);
			ret = inteldrm_swizzle_page(pg);
			if (ret)
				return;
			atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);
		}

		n += PAGE_SIZE;
		if (n >= segp->ds_len) {
			n = 0;
			segp++;
		}
	}

}

void
i915_gem_save_bit_17_swizzle(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	bus_dma_segment_t	*segp;
	int			 page_count = obj->size >> PAGE_SHIFT, i, n;

	if (dev_priv->mm.bit_6_swizzle_x != I915_BIT_6_SWIZZLE_9_10_17)
		return;

	if (obj_priv->bit_17 == NULL) {
		/* round up number of pages to a multiple of 32 so we know what
		 * size to make the bitmask. XXX this is wasteful with malloc
		 * and a better way should be done
		 */
		size_t nb17 = ((page_count + 31) & ~31)/32;
		obj_priv->bit_17 = drm_alloc(nb17 * sizeof(u_int32_t));
		if (obj_priv-> bit_17 == NULL) {
			return;
		}

	}

	segp = &obj_priv->dma_segs[0];
	n = 0;
	for (i = 0; i < page_count; i++) {
		if ((segp->ds_addr + n) & (1 << 17))
			set_bit(i, obj_priv->bit_17);
		else
			clear_bit(i, obj_priv->bit_17);

		n += PAGE_SIZE;
		if (n >= segp->ds_len) {
			n = 0;
			segp++;
		}
	}
}

bus_size_t
i915_get_fence_size(struct inteldrm_softc *dev_priv, bus_size_t size)
{
	bus_size_t	i, start;

	if (IS_I965G(dev_priv)) {
		/* 965 can have fences anywhere, so align to gpu-page size */
		return ((size + (4096 - 1)) & ~(4096 - 1));
	} else {
		/*
		 * Align the size to a power of two greater than the smallest
		 * fence size.
		 */
		if (IS_I9XX(dev_priv))
			start = 1024 * 1024;
		else
			start = 512 * 1024;

		for (i = start; i < size; i <<= 1)
			;

		return (i);
	}
}

int
i915_gem_object_fence_offset_ok(struct drm_obj *obj, int tiling_mode)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	if (obj_priv->dmamap == NULL || tiling_mode == I915_TILING_NONE)
		return (1);

	if (!IS_I965G(dev_priv)) {
		if (obj_priv->gtt_offset & (obj->size -1))
			return (0);
		if (IS_I9XX(dev_priv)) {
			if (obj_priv->gtt_offset & ~I915_FENCE_START_MASK)
				return (0);
		} else {
			if (obj_priv->gtt_offset & ~I830_FENCE_START_MASK)
				return (0);
		}
	}
	return (1);
}

/*
 * Reset the chip after a hang (965 only)
 *
 * The procedure that should be followed is relatively simple:
 *	- reset the chip using the reset reg
 *	- re-init context state
 *	- re-init Hardware status page
 *	- re-init ringbuffer
 *	- re-init interrupt state
 *	- re-init display
 */
void
inteldrm_965_reset(struct inteldrm_softc *dev_priv, u_int8_t flags)
{
	pcireg_t	reg;
	int		i = 0;

	/*
	 * There seems to be soemthing wrong with !full reset modes, so force
	 * the whole shebang for now.
	 */
	flags = GRDOM_FULL;

	if (flags == GRDOM_FULL)
		i915_save_display(dev_priv);

	reg = pci_conf_read(dev_priv->pc, dev_priv->tag, I965_GDRST);
	/*
	 * Set the domains we want to reset, then bit 0 (reset itself).
	 * then we wait for the hardware to clear it.
	 */
	pci_conf_write(dev_priv->pc, dev_priv->tag, I965_GDRST,
	    reg | (u_int32_t)flags | ((flags == GRDOM_FULL) ? 0x1 : 0x0));
	delay(50);
	/* don't clobber the rest of the register */
	pci_conf_write(dev_priv->pc, dev_priv->tag, I965_GDRST, reg & 0xfe);

	/* if this fails we're pretty much fucked, but don't loop forever */
	do {
		delay(100);
		reg = pci_conf_read(dev_priv->pc, dev_priv->tag, I965_GDRST);
	} while ((reg & 0x1) && ++i < 10);

	if (reg & 0x1)
		printf("bit 0 not cleared .. ");

	/* put everything back together again */

	/*
	 * GTT is already up (we didn't do a pci-level reset, thank god.
	 *
	 * We don't have to restore the contexts (we don't use them yet).
	 * So, if X is running we need to put the ringbuffer back first.
	 */
	 if (dev_priv->mm.suspended == 0) {
		struct drm_device *dev = (struct drm_device *)dev_priv->drmdev;
		if (inteldrm_start_ring(dev_priv) != 0)
			panic("can't restart ring, we're fucked");

		/* put the hardware status page back */
		if (I915_NEED_GFX_HWS(dev_priv)) {
			I915_WRITE(HWS_PGA, ((struct inteldrm_obj *)
			    dev_priv->hws_obj)->gtt_offset);
		} else {
			I915_WRITE(HWS_PGA,
			    dev_priv->hws_dmamem->map->dm_segs[0].ds_addr);
		}
		I915_READ(HWS_PGA); /* posting read */

		/* so we remove the handler and can put it back in */
		DRM_UNLOCK();
		drm_irq_uninstall(dev);
		drm_irq_install(dev);
		DRM_LOCK();
	 } else
		printf("not restarting ring...\n");


	 if (flags == GRDOM_FULL)
		i915_restore_display(dev_priv);
}

/*
 * Debug code from here.
 */
#ifdef WATCH_INACTIVE
void
inteldrm_verify_inactive(struct inteldrm_softc *dev_priv, char *file,
    int line)
{
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;

	TAILQ_FOREACH(obj_priv, &dev_priv->mm.inactive_list, list) {
		obj = (struct drm_obj *)obj_priv;
		if (obj_priv->pin_count || inteldrm_is_active(obj_priv) ||
		    obj->write_domain & I915_GEM_GPU_DOMAINS)
			DRM_ERROR("inactive %p (p $d a $d w $x) %s:%d\n",
			    obj, obj_priv->pin_count,
			    inteldrm_is_active(obj_priv),
			    obj->write_domain, file, line);
	}
}
#endif /* WATCH_INACTIVE */

#if (INTELDRM_DEBUG > 1)

static const char *get_pin_flag(struct inteldrm_obj *obj_priv)
{
	if (obj_priv->pin_count > 0)
		return "p";
	else
		return " ";
}

static const char *get_tiling_flag(struct inteldrm_obj *obj_priv)
{
    switch (obj_priv->tiling_mode) {
    default:
    case I915_TILING_NONE: return " ";
    case I915_TILING_X: return "X";
    case I915_TILING_Y: return "Y";
    }
}

void
i915_gem_seqno_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;

	if (dev_priv->hw_status_page != NULL) {
		printf("Current sequence: %d\n", i915_get_gem_seqno(dev_priv));
	} else {
		printf("Current sequence: hws uninitialized\n");
	}
}

void
i915_interrupt_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;

	printf("Interrupt enable:    %08x\n",
		   I915_READ(IER));
	printf("Interrupt identity:  %08x\n",
		   I915_READ(IIR));
	printf("Interrupt mask:      %08x\n",
		   I915_READ(IMR));
	printf("Pipe A stat:         %08x\n",
		   I915_READ(_PIPEASTAT));
	printf("Pipe B stat:         %08x\n",
		   I915_READ(_PIPEBSTAT));
	printf("Interrupts received: 0\n");
	if (dev_priv->hw_status_page != NULL) {
		printf("Current sequence:    %d\n",
			   i915_get_gem_seqno(dev_priv));
	} else {
		printf("Current sequence:    hws uninitialized\n");
	}
}

void
i915_gem_fence_regs_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int i;

	printf("Reserved fences = %d\n", dev_priv->fence_reg_start);
	printf("Total fences = %d\n", dev_priv->num_fence_regs);
	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		struct drm_obj *obj = dev_priv->fence_regs[i].obj;

		if (obj == NULL) {
			printf("Fenced object[%2d] = unused\n", i);
		} else {
			struct inteldrm_obj *obj_priv;

			obj_priv = (struct inteldrm_obj *)obj;
			printf("Fenced object[%2d] = %p: %s "
				   "%08x %08zx %08x %s %08x %08x %d",
				   i, obj, get_pin_flag(obj_priv),
				   obj_priv->gtt_offset,
				   obj->size, obj_priv->stride,
				   get_tiling_flag(obj_priv),
				   obj->read_domains, obj->write_domain,
				   obj_priv->last_rendering_seqno);
			if (obj->name)
				printf(" (name: %d)", obj->name);
			printf("\n");
		}
	}
}

void
i915_hws_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int i;
	volatile u32 *hws;

	hws = (volatile u32 *)dev_priv->hw_status_page;
	if (hws == NULL)
		return;

	for (i = 0; i < 4096 / sizeof(u32) / 4; i += 4) {
		printf("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			   i * 4,
			   hws[i], hws[i + 1], hws[i + 2], hws[i + 3]);
	}
}

static void
i915_dump_pages(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t size)
{
	bus_addr_t	offset = 0;
	int		i = 0;

	/*
	 * this is a bit odd so i don't have to play with the intel
	 * tools too much.
	 */
	for (offset = 0; offset < size; offset += 4, i += 4) {
		if (i == PAGE_SIZE)
			i = 0;
		printf("%08x :  %08x\n", i, bus_space_read_4(bst, bsh,
		    offset));
	}
}

void
i915_batchbuffer_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;
	bus_space_handle_t	 bsh;
	int			 ret;

	TAILQ_FOREACH(obj_priv, &dev_priv->mm.active_list, list) {
		obj = &obj_priv->obj;
		if (obj->read_domains & I915_GEM_DOMAIN_COMMAND) {
			if ((ret = agp_map_subregion(dev_priv->agph,
			    obj_priv->gtt_offset, obj->size, &bsh)) != 0) {
				DRM_ERROR("Failed to map pages: %d\n", ret);
				return;
			}
			printf("--- gtt_offset = 0x%08x\n",
			    obj_priv->gtt_offset);
			i915_dump_pages(dev_priv->bst, bsh, obj->size);
			agp_unmap_subregion(dev_priv->agph, dev_priv->ring.bsh,
			    obj->size);
		}
	}
}

void
i915_ringbuffer_data(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	bus_size_t		 off;

	if (!dev_priv->ring.ring_obj) {
		printf("No ringbuffer setup\n");
		return;
	}

	for (off = 0; off < dev_priv->ring.size; off += 4)
		printf("%08x :  %08x\n", off, bus_space_read_4(dev_priv->bst,
		    dev_priv->ring.bsh, off));
}

void
i915_ringbuffer_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	u_int32_t		 head, tail;

	head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	tail = I915_READ(PRB0_TAIL) & TAIL_ADDR;

	printf("RingHead :  %08x\n", head);
	printf("RingTail :  %08x\n", tail);
	printf("RingMask :  %08x\n", dev_priv->ring.size - 1);
	printf("RingSize :  %08lx\n", dev_priv->ring.size);
	printf("Acthd :  %08x\n", I915_READ(IS_I965G(dev_priv) ?
	    ACTHD_I965 : ACTHD));
}

#endif

#define PCI_VENDOR_INTEL		0x8086
#define INTEL_PCH_DEVICE_ID_MASK	0xff00
#define INTEL_PCH_IBX_DEVICE_ID_TYPE	0x3b00
#define INTEL_PCH_CPT_DEVICE_ID_TYPE	0x1c00
#define INTEL_PCH_PPT_DEVICE_ID_TYPE	0x1e00
#define INTEL_PCH_LPT_DEVICE_ID_TYPE	0x8c00

static int
intel_pch_match(struct pci_attach_args *pa)
{
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_ISA)
		return (1);
	return (0);
}

void
intel_detect_pch(struct inteldrm_softc *dev_priv)
{
	struct pci_attach_args	pa;
	if (pci_find_device(&pa, intel_pch_match) == 0) {
		DRM_DEBUG_KMS("No Intel PCI-ISA bridge found\n");
	}
	switch (PCI_PRODUCT(pa.pa_id) & INTEL_PCH_DEVICE_ID_MASK) {
	case INTEL_PCH_IBX_DEVICE_ID_TYPE:
		dev_priv->pch_type = PCH_IBX;
		dev_priv->num_pch_pll = 2;
		DRM_DEBUG_KMS("Found Ibex Peak PCH\n");
		break;
	case INTEL_PCH_CPT_DEVICE_ID_TYPE:
		dev_priv->pch_type = PCH_CPT;
		dev_priv->num_pch_pll = 2;
		DRM_DEBUG_KMS("Found CougarPoint PCH\n");
		break;
	case INTEL_PCH_PPT_DEVICE_ID_TYPE:
		/* PantherPoint is CPT compatible */
		dev_priv->pch_type = PCH_CPT;
		dev_priv->num_pch_pll = 2;
		DRM_DEBUG_KMS("Found PatherPoint PCH\n");
		break;
	case INTEL_PCH_LPT_DEVICE_ID_TYPE:
		dev_priv->pch_type = PCH_LPT;
		dev_priv->num_pch_pll = 0;
		DRM_DEBUG_KMS("Found LynxPoint PCH\n");
	default:
		DRM_DEBUG_KMS("No PCH detected\n");
	}
}

void
__gen6_gt_force_wake_get(struct inteldrm_softc *dev_priv)
{
	int count;

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_ACK) & 1))
		DELAY(10000);

	I915_WRITE_NOTRACE(FORCEWAKE, 1);
	POSTING_READ(FORCEWAKE);

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_ACK) & 1) == 0)
		DELAY(10000);
}

void
__gen6_gt_force_wake_mt_get(struct inteldrm_softc *dev_priv)
{
	int count;

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_MT_ACK) & 1))
		DELAY(10000);

	I915_WRITE_NOTRACE(FORCEWAKE_MT, (1<<16) | 1);
	POSTING_READ(FORCEWAKE_MT);

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_MT_ACK) & 1) == 0)
		DELAY(10000);
}

void
gen6_gt_force_wake_get(struct inteldrm_softc *dev_priv)
{

	mtx_enter(&dev_priv->gt_lock);
	if (dev_priv->forcewake_count++ == 0)
		dev_priv->display.force_wake_get(dev_priv);
	mtx_leave(&dev_priv->gt_lock);
}

static void
gen6_gt_check_fifodbg(struct inteldrm_softc *dev_priv)
{
	u32 gtfifodbg;

	gtfifodbg = I915_READ_NOTRACE(GTFIFODBG);
	if ((gtfifodbg & GT_FIFO_CPU_ERROR_MASK) != 0) {
		printf("MMIO read or write has been dropped %x\n", gtfifodbg);
		I915_WRITE_NOTRACE(GTFIFODBG, GT_FIFO_CPU_ERROR_MASK);
	}
}

void
__gen6_gt_force_wake_put(struct inteldrm_softc *dev_priv)
{

	I915_WRITE_NOTRACE(FORCEWAKE, 0);
	/* The below doubles as a POSTING_READ */
	gen6_gt_check_fifodbg(dev_priv);
}

void
__gen6_gt_force_wake_mt_put(struct inteldrm_softc *dev_priv)
{

	I915_WRITE_NOTRACE(FORCEWAKE_MT, (1<<16) | 0);
	/* The below doubles as a POSTING_READ */
	gen6_gt_check_fifodbg(dev_priv);
}

void
gen6_gt_force_wake_put(struct inteldrm_softc *dev_priv)
{

	mtx_enter(&dev_priv->gt_lock);
	if (--dev_priv->forcewake_count == 0)
 		dev_priv->display.force_wake_put(dev_priv);
	mtx_leave(&dev_priv->gt_lock);
}

int
__gen6_gt_wait_for_fifo(struct inteldrm_softc *dev_priv)
{
	int ret = 0;

	if (dev_priv->gt_fifo_count < GT_FIFO_NUM_RESERVED_ENTRIES) {
		int loop = 500;
		u32 fifo = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);
		while (fifo <= GT_FIFO_NUM_RESERVED_ENTRIES && loop--) {
			DELAY(10000);
			fifo = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);
		}
		if (loop < 0 && fifo <= GT_FIFO_NUM_RESERVED_ENTRIES) {
			printf("%s loop\n", __func__);
			++ret;
		}
		dev_priv->gt_fifo_count = fifo;
	}
	dev_priv->gt_fifo_count--;

	return (ret);
}

