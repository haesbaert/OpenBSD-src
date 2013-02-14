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

extern struct mutex mchdev_lock;

/*
 * Override lid status (0=autodetect, 1=autodetect disabled [default],
 * -1=force lid closed, -2=force lid open)
 */
int i915_panel_ignore_lid = 1;

/* Enable powersavings, fbc, downclocking, etc. (default: true) */
unsigned int i915_powersave = 1;

/*
 * Enable frame buffer compression for power savings
 * (default: -1 (use per-chip default))
 */
int i915_enable_fbc = -1;

/*
 * Enable power-saving render C-state 6.
 * Different stages can be selected via bitmask values
 * (0 = disable; 1 = enable rc6; 2 = enable deep rc6; 4 = enable deepest rc6).
 * For example, 3 would enable rc6 and deep rc6, and 7 would enable everything.
 * default: -1 (use per-chip default)
 */
int i915_enable_rc6 = -1;

const struct intel_device_info *
	i915_get_device_id(int);
int	inteldrm_probe(struct device *, void *, void *);
void	inteldrm_attach(struct device *, struct device *, void *);
int	inteldrm_detach(struct device *, int);
int	inteldrm_activate(struct device *, int);
int	inteldrm_ioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);
int	inteldrm_doioctl(struct drm_device *, u_long, caddr_t, struct drm_file *);

int	inteldrm_gmch_match(struct pci_attach_args *);
void	inteldrm_timeout(void *);
void	inteldrm_hung(void *, void *);
void	inteldrm_965_reset(struct inteldrm_softc *, u_int8_t);
void	inteldrm_quiesce(struct inteldrm_softc *);

/* For reset and suspend */

void	i915_alloc_ifp(struct inteldrm_softc *, struct pci_attach_args *);
void	i965_alloc_ifp(struct inteldrm_softc *, struct pci_attach_args *);

int	i915_drm_freeze(struct drm_device *);
int	__i915_drm_thaw(struct drm_device *);
int	i915_drm_thaw(struct drm_device *);

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
	.has_fbc = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_sandybridge_d_info = {
	.gen = 6,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_sandybridge_m_info = {
	.gen = 6, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_ivybridge_d_info = {
	.is_ivybridge = 1, .gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_ivybridge_m_info = {
	.is_ivybridge = 1, .gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,	/* FBC is not enabled on Ivybridge mobile yet */
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_valleyview_m_info = {
	.gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.is_valleyview = 1,
};

static const struct intel_device_info intel_valleyview_d_info = {
	.gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.is_valleyview = 1,
};

static const struct intel_device_info intel_haswell_d_info = {
	.is_haswell = 1, .gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
};

static const struct intel_device_info intel_haswell_m_info = {
	.is_haswell = 1, .gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_force_wake = 1,
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

static struct drm_driver_info inteldrm_driver = {
	.buf_priv_size		= 1,	/* No dev_priv */
	.file_priv_size		= sizeof(struct inteldrm_file),
	.ioctl			= inteldrm_ioctl,
	.lastclose		= i915_driver_lastclose,
	.vblank_pipes		= 2,
	.get_vblank_counter	= i915_get_vblank_counter,
	.enable_vblank		= i915_enable_vblank,
	.disable_vblank		= i915_disable_vblank,

	.gem_init_object	= i915_gem_init_object,
	.gem_free_object	= i915_gem_free_object,
	.gem_fault		= i915_gem_fault,
	.gem_size		= sizeof(struct drm_i915_gem_object),

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

int
i915_drm_freeze(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;

	drm_kms_helper_poll_disable(dev);

#if 0
	pci_save_state(dev->pdev);
#endif

	/* If KMS is active, we do the leavevt stuff here */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		int error = i915_gem_idle(dev_priv);
		if (error) {
			printf("GEM idle failed, resume might fail\n");
			return (error);
		}

#ifdef notyet
		cancel_delayed_work_sync(&dev_priv->rps.delayed_resume_work);
#endif

		intel_modeset_disable(dev);

		drm_irq_uninstall(dev);
	}

	i915_save_state(dev);

	intel_opregion_fini(dev);

	/* Modeset on resume, not lid events */
	dev_priv->modeset_on_lid = 0;

	return 0;
}

int
__i915_drm_thaw(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	int error = 0;

	i915_restore_state(dev);
	intel_opregion_setup(dev);

	/* KMS EnterVT equivalent */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		intel_init_pch_refclk(dev);

		DRM_LOCK();
		dev_priv->mm.suspended = 0;

		error = i915_gem_init_hw(dev);
		DRM_UNLOCK();

		intel_modeset_init_hw(dev);
		intel_modeset_setup_hw_state(dev, false);
		drm_irq_install(dev);
	}

	intel_opregion_init(dev);

	dev_priv->modeset_on_lid = 0;

	return error;
}

int
i915_drm_thaw(struct drm_device *dev)
{
	int error = 0;

	intel_gt_reset(dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		DRM_LOCK();
		i915_gem_restore_gtt_mappings(dev);
		DRM_UNLOCK();
	}

	__i915_drm_thaw(dev);

	return error;
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
	uint16_t		 pci_device;

	id_entry = drm_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), inteldrm_pciidlist);
	pci_device = PCI_PRODUCT(pa->pa_id);
	dev_priv->info = i915_get_device_id(pci_device);
	KASSERT(dev_priv->info->gen != 0);

	dev_priv->pc = pa->pa_pc;
	dev_priv->tag = pa->pa_tag;
	dev_priv->dmat = pa->pa_dmat;
	dev_priv->bst = pa->pa_memt;

	/* All intel chipsets need to be treated as agp, so just pass one */
	dev_priv->drmdev = drm_attach_pci(&inteldrm_driver, pa, 1, self);

	dev = (struct drm_device *)dev_priv->drmdev;

	/* we need to use this api for now due to sharing with intagp */
	bar = vga_pci_bar_info((struct vga_pci_softc *)parent,
	    (IS_I9XX(dev) ? 0 : 1));
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
	if (IS_I945G(dev) || IS_I945GM(dev))
		pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;

	if (pci_intr_map(pa, &dev_priv->ih) != 0) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intel_irq_init(dev);

	/*
	 * set up interrupt handler, note that we don't switch the interrupt
	 * on until the X server talks to us, kms will change this.
	 */
	dev_priv->irqh = pci_intr_establish(dev_priv->pc, dev_priv->ih, IPL_TTY,
	    inteldrm_driver.irq_handler,
	    dev_priv, dev_priv->dev.dv_xname);
	if (dev_priv->irqh == NULL) {
		printf(": couldn't  establish interrupt\n");
		return;
	}

	/* Unmask the interrupts that we always want on. */
	if (HAS_PCH_SPLIT(dev)) {
		dev_priv->irq_mask = ~PCH_SPLIT_DISPLAY_INTR_FIX;
		/* masked for now, turned on on demand */
		dev_priv->gt_irq_mask = ~PCH_SPLIT_RENDER_INTR_FIX;
		dev_priv->pch_irq_mask = ~PCH_SPLIT_HOTPLUG_INTR_FIX;
	} else {
		dev_priv->irq_mask = ~I915_INTERRUPT_ENABLE_FIX;
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
	TAILQ_INIT(&dev_priv->mm.fence_list);
	for (i = 0; i < I915_NUM_RINGS; i++)
		init_ring_lists(&dev_priv->rings[i]);
	timeout_set(&dev_priv->mm.retire_timer, inteldrm_timeout, dev_priv);
	timeout_set(&dev_priv->hangcheck_timer, i915_hangcheck_elapsed, dev_priv);
	dev_priv->next_seqno = 1;
	dev_priv->mm.suspended = 1;

	/* On GEN3 we really need to make sure the ARB C3 LP bit is set */
	if (IS_GEN3(dev)) {
		u_int32_t tmp = I915_READ(MI_ARB_STATE);
		if (!(tmp & MI_ARB_C3_LP_WRITE_ENABLE)) {
			/*
			 * arb state is a masked write, so set bit + bit
			 * in mask
			 */
			I915_WRITE(MI_ARB_STATE,
			           _MASKED_BIT_ENABLE(MI_ARB_C3_LP_WRITE_ENABLE));
		}
	}

	/* Old X drivers will take 0-2 for front, back, depth buffers */
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		dev_priv->fence_reg_start = 3;

	if (INTEL_INFO(dev)->gen >= 4 || IS_I945G(dev) ||
	    IS_I945GM(dev) || IS_G33(dev))
		dev_priv->num_fence_regs = 16;
	else
		dev_priv->num_fence_regs = 8;

	/* Initialise fences to zero, else on some macs we'll get corruption */
	if (IS_GEN6(dev) || IS_GEN7(dev)) {
		for (i = 0; i < 16; i++)
			I915_WRITE64(FENCE_REG_SANDYBRIDGE_0 + (i * 8), 0);
	} else if (INTEL_INFO(dev)->gen >= 4) {
		for (i = 0; i < 16; i++)
			I915_WRITE64(FENCE_REG_965_0 + (i * 8), 0);
	} else {
		for (i = 0; i < 8; i++)
			I915_WRITE(FENCE_REG_830_0 + (i * 4), 0);
		if (IS_I945G(dev) || IS_I945GM(dev) ||
		    IS_G33(dev))
			for (i = 0; i < 8; i++)
				I915_WRITE(FENCE_REG_945_8 + (i * 4), 0);
	}

	if (pci_find_device(&bpa, inteldrm_gmch_match) == 0) {
		printf(": can't find GMCH\n");
		return;
	}

	/* Set up the IFP for chipset flushing */
	if (IS_I915G(dev) || IS_I915GM(dev) || IS_I945G(dev) ||
	    IS_I945GM(dev)) {
		i915_alloc_ifp(dev_priv, &bpa);
	} else if (INTEL_INFO(dev)->gen >= 4 || IS_G33(dev)) {
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

	i915_gem_detect_bit_6_swizzle(dev_priv, &bpa);

	dev_priv->mm.interruptible = true;

	printf(": %s\n", pci_intr_string(pa->pa_pc, dev_priv->ih));

	mtx_init(&dev_priv->irq_lock, IPL_TTY);
	mtx_init(&dev_priv->list_lock, IPL_NONE);
	mtx_init(&dev_priv->request_lock, IPL_NONE);
	mtx_init(&dev_priv->fence_lock, IPL_NONE);
	mtx_init(&dev_priv->dpio_lock, IPL_NONE);
	mtx_init(&mchdev_lock, IPL_NONE);

	if (IS_IVYBRIDGE(dev) || IS_HASWELL(dev))
		dev_priv->num_pipe = 3;
	else if (IS_MOBILE(dev) || !IS_GEN2(dev))
		dev_priv->num_pipe = 2;
	else
		dev_priv->num_pipe = 1;

	intel_detect_pch(dev_priv);

	intel_gt_init(dev);

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
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		i915_load_modeset_init(dev);
	intel_opregion_init(dev);
}

int
inteldrm_detach(struct device *self, int flags)
{
	struct inteldrm_softc	*dev_priv = (struct inteldrm_softc *)self;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	/* this will quiesce any dma that's going on and kill the timeouts. */
	if (dev_priv->drmdev != NULL) {
		config_detach(dev_priv->drmdev, flags);
		dev_priv->drmdev = NULL;
	}

#if 0
	if (!I915_NEED_GFX_HWS(dev) && dev_priv->hws_dmamem) {
		drm_dmamem_free(dev_priv->dmat, dev_priv->hws_dmamem);
		dev_priv->hws_dmamem = NULL;
		/* Need to rewrite hardware status page */
		I915_WRITE(HWS_PGA, 0x1ffff000);
//		dev_priv->hw_status_page = NULL;
	}
#endif

	if (IS_I9XX(dev) && dev_priv->ifp.i9xx.bsh != 0) {
		bus_space_unmap(dev_priv->ifp.i9xx.bst, dev_priv->ifp.i9xx.bsh,
		    PAGE_SIZE);
	} else if ((IS_I830(dev) || IS_845G(dev) || IS_I85X(dev) ||
	    IS_I865G(dev)) && dev_priv->ifp.i8xx.kva != NULL) {
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
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	switch (act) {
	case DVACT_QUIESCE:
//		inteldrm_quiesce(dev_priv);
		i915_drm_freeze(dev);
		break;
	case DVACT_SUSPEND:
//		i915_save_state(dev);
		break;
	case DVACT_RESUME:
//		i915_restore_state(dev);
//		/* entrypoints can stop sleeping now */
//		atomic_clearbits_int(&dev_priv->sc_flags, INTELDRM_QUIET);
//		wakeup(&dev_priv->flags);
		i915_drm_thaw(dev);
		intel_fb_restore_mode(dev);
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
			return (i915_getparam(dev_priv, data));
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
			return (i915_setparam(dev_priv, data));
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
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;

	/*
	 * Write to this flush page flushes the chipset write cache.
	 * The write will return when it is done.
	 */
	if (IS_I9XX(dev)) {
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
	struct drm_i915_gem_object	*obj_priv;
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
	obj_priv = to_intel_bo(obj);

	/* Check size. Also ensure that the object is not purgeable */
	if (args->size == 0 || args->offset > obj->size || args->size >
	    obj->size || (args->offset + args->size) > obj->size ||
	    i915_gem_object_is_purgeable(obj_priv)) {
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
	struct drm_i915_gem_object	*obj_priv = to_intel_bo(obj);

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
	obj_priv->madv = __I915_MADV_PURGED;
}

void
inteldrm_process_flushing(struct inteldrm_softc *dev_priv,
    u_int32_t flush_domains)
{
	struct drm_i915_gem_object	*obj_priv, *next;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	mtx_enter(&dev_priv->list_lock);
	for (obj_priv = TAILQ_FIRST(&dev_priv->mm.gpu_write_list);
	    obj_priv != TAILQ_END(&dev_priv->mm.gpu_write_list);
	    obj_priv = next) {
		struct drm_obj *obj = &(obj_priv->base);

		next = TAILQ_NEXT(obj_priv, write_list);

		if ((obj->write_domain & flush_domains)) {
			TAILQ_REMOVE(&dev_priv->mm.gpu_write_list,
			    obj_priv, write_list);
			atomic_clearbits_int(&obj->do_flags,
			     I915_GPU_WRITE);
			i915_gem_object_move_to_active(obj_priv, obj_priv->ring);
			obj->write_domain = 0;
			/* if we still need the fence, update LRU */
			if (inteldrm_needs_fence(obj_priv)) {
				KASSERT(obj_priv->fence_reg !=
				    I915_FENCE_REG_NONE);
				/* we have a fence, won't sleep, can't fail
				 * since we have the fence we no not need
				 * to have the object held
				 */
				i915_gem_get_fence_reg(obj);
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
    struct drm_i915_gem_request *request)
{
	struct drm_i915_gem_object	*obj_priv;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	mtx_enter(&dev_priv->list_lock);
	/* Move any buffers on the active list that are no longer referenced
	 * by the ringbuffer to the flushing/inactive lists as appropriate.  */
	while ((obj_priv  = TAILQ_FIRST(&dev_priv->mm.active_list)) != NULL) {
		struct drm_obj *obj = &obj_priv->base;

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
			KASSERT(obj_priv->active);
			i915_move_to_tail(obj_priv,
			    &dev_priv->mm.flushing_list);
			i915_gem_object_move_off_active(obj_priv);
			drm_unlock_obj(obj);
		} else {
			/* unlocks object for us and drops ref */
			i915_gem_object_move_to_inactive_locked(obj_priv);
			mtx_enter(&dev_priv->list_lock);
		}
	}
	mtx_leave(&dev_priv->list_lock);
}

void
i915_gem_retire_work_handler(void *arg1, void *unused)
{
	struct inteldrm_softc	*dev_priv = arg1;
	struct intel_ring_buffer *ring;
	bool			 idle;
	int			 i;

	i915_gem_retire_requests(dev_priv);
	idle = true;
	for_each_ring(ring, dev_priv, i) {
		idle &= list_empty(&ring->request_list);
	}
	if (!dev_priv->mm.suspended && !idle)
		timeout_add_sec(&dev_priv->mm.retire_timer, 1);
}

/*
 * flush and invalidate the provided domains
 * if we have successfully queued a gpu flush, then we return a seqno from
 * the request. else (failed or just cpu flushed)  we return 0.
 */
u_int32_t
i915_gem_flush(struct intel_ring_buffer *ring, uint32_t invalidate_domains,
    uint32_t flush_domains)
{
	drm_i915_private_t	*dev_priv = ring->dev->dev_private;
	int			 err = 0;
	u32			 seqno;

	if (flush_domains & I915_GEM_DOMAIN_CPU)
		inteldrm_chipset_flush(dev_priv);
	if (((invalidate_domains | flush_domains) & I915_GEM_GPU_DOMAINS) == 0) 
		return (0);

	ring->flush(ring, invalidate_domains, flush_domains);

	/* if this is a gpu flush, process the results */
	if (flush_domains & I915_GEM_GPU_DOMAINS) {
		mtx_enter(&dev_priv->request_lock);
		inteldrm_process_flushing(dev_priv, flush_domains);
		mtx_leave(&dev_priv->request_lock);
		err = i915_add_request(ring, NULL, &seqno);
	}

	if (err)
		return (0);
	else
		return (seqno);
}

struct drm_obj *
i915_gem_find_inactive_object(struct inteldrm_softc *dev_priv,
    size_t min_size)
{
	struct drm_obj		*obj, *best = NULL, *first = NULL;
	struct drm_i915_gem_object *obj_priv;

	/*
	 * We don't need references to the object as long as we hold the list
	 * lock, they won't disappear until we release the lock.
	 */
	mtx_enter(&dev_priv->list_lock);
	TAILQ_FOREACH(obj_priv, &dev_priv->mm.inactive_list, list) {
		obj = &obj_priv->base;
		if (obj->size >= min_size) {
			if ((!obj_priv->dirty ||
			    i915_gem_object_is_purgeable(obj_priv)) &&
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
i915_gem_get_fence_reg(struct drm_obj *obj)
{
	struct drm_device		*dev = obj->dev;
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_object	*obj_priv = to_intel_bo(obj);
	struct drm_i915_gem_object	*old_obj_priv = NULL;
	struct drm_obj			*old_obj = NULL;
	struct drm_i915_fence_reg	*reg = NULL;
	int				 i, ret, avail;

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

		old_obj_priv = to_intel_bo(reg->obj);
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
			old_obj_priv = to_intel_bo(old_obj);

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

		ret = i915_gem_object_put_fence_reg(old_obj);
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

	switch (INTEL_INFO(dev)->gen) {
	case 7:
	case 6:
		sandybridge_write_fence_reg(reg);
		break;
	case 5:
	case 4:
		i965_write_fence_reg(reg);
		break;
	case 3:
		i915_write_fence_reg(reg);
		break;
	case 2:
		i830_write_fence_reg(reg);
		break;
	}
	mtx_leave(&dev_priv->fence_lock);

	return 0;
}

void
inteldrm_wipe_mappings(struct drm_obj *obj)
{
	struct drm_i915_gem_object *obj_priv = to_intel_bo(obj);
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
	struct drm_i915_gem_object *obj_priv = to_intel_bo(obj);
	bus_space_handle_t	 bsh;
	int			 i, ret, needs_fence;

	DRM_ASSERT_HELD(obj);
	needs_fence = ((entry->flags & EXEC_OBJECT_NEEDS_FENCE) &&
	    obj_priv->tiling_mode != I915_TILING_NONE);
	if (needs_fence)
		atomic_setbits_int(&obj->do_flags, I915_EXEC_NEEDS_FENCE);

	/* Choose the GTT offset for our buffer and put it there. */
	ret = i915_gem_object_pin(obj_priv, (u_int32_t)entry->alignment,
	    needs_fence);
	if (ret)
		return ret;

	entry->offset = obj_priv->gtt_offset;

	/* Apply the relocations, using the GTT aperture to avoid cache
	 * flushing requirements.
	 */
	for (i = 0; i < entry->relocation_count; i++) {
		struct drm_i915_gem_relocation_entry *reloc = &relocs[i];
		struct drm_i915_gem_object *target_obj_priv;
		uint32_t reloc_val, reloc_offset;

		target_obj = drm_gem_object_lookup(obj->dev, file_priv,
		    reloc->target_handle);
		/* object must have come before us in the list */
		if (target_obj == NULL) {
			i915_gem_object_unpin(obj_priv);
			return (EBADF);
		}
		if ((target_obj->do_flags & I915_IN_EXEC) == 0) {
			printf("%s: object not already in execbuffer\n",
			__func__);
			ret = EBADF;
			goto err;
		}

		target_obj_priv = to_intel_bo(target_obj);

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

		ret = i915_gem_object_set_to_gtt_domain(obj_priv, 1);
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
	i915_gem_object_unpin(obj_priv);
	return (ret);
}

/** Dispatch a batchbuffer to the ring
 */
void
i915_dispatch_gem_execbuffer(struct intel_ring_buffer *ring,
    struct drm_i915_gem_execbuffer2 *exec, uint64_t exec_offset)
{
//	struct inteldrm_softc	*dev_priv = dev->dev_private;
	uint32_t		 exec_start, exec_len;
	int			 ret;

	exec_start = (uint32_t)exec_offset + exec->batch_start_offset;
	exec_len = (uint32_t)exec->batch_len;

	ret = ring->dispatch_execbuffer(ring, exec_start, exec_len, 0);
	if (ret)
		return;

	/*
	 * Ensure that the commands in the batch buffer are
	 * finished before the interrupt fires (from a subsequent request
	 * added). We get back a seqno representing the execution of the
	 * current buffer, which we can wait on.  We would like to mitigate
	 * these interrupts, likely by only creating seqnos occasionally
	 * (so that we have *some* interrupts representing completion of
	 * buffers that we can wait on when trying to clear up gtt space).
	 */
	intel_ring_flush_all_caches(ring);
	/*
	 * move to active associated all previous buffers with the seqno
	 * that this call will emit. so we don't need the return. If it fails
	 * then the next seqno will take care of it.
	 */
	(void)i915_add_request(ring, NULL, NULL);

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
	KASSERT(dev_priv->rings[RCS].obj == NULL);
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
	(void)i915_gem_evict_inactive(dev_priv);
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
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	u_int32_t		 eir, ipeir;
	u_int8_t		 reset = GRDOM_RENDER;
	char 			*errbitstr;

	eir = I915_READ(EIR);
	if (eir == 0)
		return;

	if (IS_IRONLAKE(dev)) {
		errbitstr = "\20\x05PTEE\x04MPVE\x03CPVE";
	} else if (IS_G4X(dev)) {
		errbitstr = "\20\x10 BCSINSTERR\x06PTEERR\x05MPVERR\x04CPVERR"
		     "\x03 BCSPTEERR\x02REFRESHERR\x01INSTERR";
	} else {
		errbitstr = "\20\x5PTEERR\x2REFRESHERR\x1INSTERR";
	}

	printf("render error detected, EIR: %b\n", eir, errbitstr);
	if (IS_IRONLAKE(dev) || IS_GEN6(dev) || IS_GEN7(dev)) {
		if (eir & I915_ERROR_PAGE_TABLE) {
			dev_priv->mm.wedged = 1;
			reset = GRDOM_FULL;
		}
	} else {
		if (IS_G4X(dev)) {
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
		} else if (IS_I9XX(dev) && eir & I915_ERROR_PAGE_TABLE) {
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
			if (INTEL_INFO(dev)->gen < 4) {
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
		if (IS_IRONLAKE(dev) || IS_GEN6(dev) ||
		    IS_GEN7(dev)) {
			I915_WRITE(GTIIR, GT_RENDER_CS_ERROR_INTERRUPT);
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
	struct drm_i915_gem_object *obj_priv;
	u_int8_t		 reset = (u_int8_t)(uintptr_t)reset_type;

	DRM_LOCK();
	if (HAS_RESET(dev)) {
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
		drm_lock_obj(&obj_priv->base);
		if (obj_priv->base.write_domain & I915_GEM_GPU_DOMAINS) {
			TAILQ_REMOVE(&dev_priv->mm.gpu_write_list,
			    obj_priv, write_list);
			atomic_clearbits_int(&obj_priv->base.do_flags,
			    I915_GPU_WRITE);
			obj_priv->base.write_domain &= ~I915_GEM_GPU_DOMAINS;
		}
		/* unlocks object and list */
		i915_gem_object_move_to_inactive_locked(obj_priv);
		mtx_enter(&dev_priv->list_lock);
	}
	mtx_leave(&dev_priv->list_lock);

	/* unbind everything */
	(void)i915_gem_evict_inactive(dev_priv);

	if (HAS_RESET(dev))
		dev_priv->mm.wedged = 0;
	DRM_UNLOCK();
}

void
i915_move_to_tail(struct drm_i915_gem_object *obj_priv,
    struct i915_gem_list *head)
{
	i915_list_remove(obj_priv);
	TAILQ_INSERT_TAIL(head, obj_priv, list);
	obj_priv->current_list = head;
}

void
i915_list_remove(struct drm_i915_gem_object *obj_priv)
{
	if (obj_priv->current_list != NULL)
		TAILQ_REMOVE(obj_priv->current_list, obj_priv, list);
	obj_priv->current_list = NULL;
}

bus_size_t
i915_get_fence_size(struct inteldrm_softc *dev_priv, bus_size_t size)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	bus_size_t		 i, start;

	if (INTEL_INFO(dev)->gen >= 4) {
		/* 965 can have fences anywhere, so align to gpu-page size */
		return ((size + (4096 - 1)) & ~(4096 - 1));
	} else {
		/*
		 * Align the size to a power of two greater than the smallest
		 * fence size.
		 */
		if (IS_I9XX(dev))
			start = 1024 * 1024;
		else
			start = 512 * 1024;

		for (i = start; i < size; i <<= 1)
			;

		return (i);
	}
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
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	pcireg_t		 reg;
	int			 i = 0;

	/*
	 * There seems to be soemthing wrong with !full reset modes, so force
	 * the whole shebang for now.
	 */
	flags = GRDOM_FULL;

	if (flags == GRDOM_FULL)
		i915_save_display(dev);

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
		if (init_ring_common(&dev_priv->rings[RCS]) != 0)
			panic("can't restart ring, we're fucked");

#if 0
		/* put the hardware status page back */
		if (I915_NEED_GFX_HWS(dev)) {
			I915_WRITE(HWS_PGA, ((struct drm_i915_gem_object *)
			    dev_priv->hws_obj)->gtt_offset);
		} else {
			I915_WRITE(HWS_PGA,
			    dev_priv->hws_dmamem->map->dm_segs[0].ds_addr);
		}
		I915_READ(HWS_PGA); /* posting read */
#endif

		/* so we remove the handler and can put it back in */
		DRM_UNLOCK();
		drm_irq_uninstall(dev);
		drm_irq_install(dev);
		DRM_LOCK();
	 } else
		printf("not restarting ring...\n");


	 if (flags == GRDOM_FULL)
		i915_restore_display(dev);
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
	struct drm_i915_gem_object *obj_priv;

	TAILQ_FOREACH(obj_priv, &dev_priv->mm.inactive_list, list) {
		obj = &obj_priv->base;
		if (obj_priv->pin_count || obj_priv->active ||
		    obj->write_domain & I915_GEM_GPU_DOMAINS)
			DRM_ERROR("inactive %p (p $d a $d w $x) %s:%d\n",
			    obj, obj_priv->pin_count, obj_priv->active,
			    obj->write_domain, file, line);
	}
}
#endif /* WATCH_INACTIVE */

#if (INTELDRM_DEBUG > 1)

static const char *get_pin_flag(struct drm_i915_gem_object *obj)
{
	if (obj->user_pin_count > 0)
		return "P";
	if (obj->pin_count > 0)
		return "p";
	else
		return " ";
}

static const char *get_tiling_flag(struct drm_i915_gem_object *obj)
{
    switch (obj->tiling_mode) {
    default:
    case I915_TILING_NONE: return " ";
    case I915_TILING_X: return "X";
    case I915_TILING_Y: return "Y";
    }
}

static void i915_ring_seqno_info(struct intel_ring_buffer *ring)
{
	if (ring->get_seqno) {
		printf("Current sequence (%s): %d\n",
		       ring->name, ring->get_seqno(ring, false));
	}
}

void
i915_gem_seqno_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int			 i;

	for_each_ring(ring, dev_priv, i)
		i915_ring_seqno_info(ring);
}

void
i915_interrupt_info(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int			 i, pipe;

	if (IS_VALLEYVIEW(dev)) {
		printf("Display IER:\t%08x\n",
			   I915_READ(VLV_IER));
		printf("Display IIR:\t%08x\n",
			   I915_READ(VLV_IIR));
		printf("Display IIR_RW:\t%08x\n",
			   I915_READ(VLV_IIR_RW));
		printf("Display IMR:\t%08x\n",
			   I915_READ(VLV_IMR));
		for_each_pipe(pipe)
			printf("Pipe %c stat:\t%08x\n",
				   pipe_name(pipe),
				   I915_READ(PIPESTAT(pipe)));

		printf("Master IER:\t%08x\n",
			   I915_READ(VLV_MASTER_IER));

		printf("Render IER:\t%08x\n",
			   I915_READ(GTIER));
		printf("Render IIR:\t%08x\n",
			   I915_READ(GTIIR));
		printf("Render IMR:\t%08x\n",
			   I915_READ(GTIMR));

		printf("PM IER:\t\t%08x\n",
			   I915_READ(GEN6_PMIER));
		printf("PM IIR:\t\t%08x\n",
			   I915_READ(GEN6_PMIIR));
		printf("PM IMR:\t\t%08x\n",
			   I915_READ(GEN6_PMIMR));

		printf("Port hotplug:\t%08x\n",
			   I915_READ(PORT_HOTPLUG_EN));
		printf("DPFLIPSTAT:\t%08x\n",
			   I915_READ(VLV_DPFLIPSTAT));
		printf("DPINVGTT:\t%08x\n",
			   I915_READ(DPINVGTT));

	} else if (!HAS_PCH_SPLIT(dev)) {
		printf("Interrupt enable:    %08x\n",
			   I915_READ(IER));
		printf("Interrupt identity:  %08x\n",
			   I915_READ(IIR));
		printf("Interrupt mask:      %08x\n",
			   I915_READ(IMR));
		for_each_pipe(pipe)
			printf("Pipe %c stat:         %08x\n",
				   pipe_name(pipe),
				   I915_READ(PIPESTAT(pipe)));
	} else {
		printf("North Display Interrupt enable:		%08x\n",
			   I915_READ(DEIER));
		printf("North Display Interrupt identity:	%08x\n",
			   I915_READ(DEIIR));
		printf("North Display Interrupt mask:		%08x\n",
			   I915_READ(DEIMR));
		printf("South Display Interrupt enable:		%08x\n",
			   I915_READ(SDEIER));
		printf("South Display Interrupt identity:	%08x\n",
			   I915_READ(SDEIIR));
		printf("South Display Interrupt mask:		%08x\n",
			   I915_READ(SDEIMR));
		printf("Graphics Interrupt enable:		%08x\n",
			   I915_READ(GTIER));
		printf("Graphics Interrupt identity:		%08x\n",
			   I915_READ(GTIIR));
		printf("Graphics Interrupt mask:		%08x\n",
			   I915_READ(GTIMR));
	}
#if 0
	printf("Interrupts received: %d\n",
		   atomic_read(&dev_priv->irq_received));
#endif
	for_each_ring(ring, dev_priv, i) {
		if (IS_GEN6(dev) || IS_GEN7(dev)) {
			printf(
				   "Graphics Interrupt mask (%s):	%08x\n",
				   ring->name, I915_READ_IMR(ring));
		}
		i915_ring_seqno_info(ring);
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
			struct drm_i915_gem_object *obj_priv;

			obj_priv = to_intel_bo(obj);
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
	struct intel_ring_buffer *ring = &dev_priv->rings[RCS];
	int i;
	volatile u32 *hws;

	hws = (volatile u32 *)ring->status_page.page_addr;
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
	struct drm_i915_gem_object *obj_priv;
	bus_space_handle_t	 bsh;
	int			 ret;

	TAILQ_FOREACH(obj_priv, &dev_priv->mm.active_list, list) {
		obj = &obj_priv->base;
		if (obj->read_domains & I915_GEM_DOMAIN_COMMAND) {
			if ((ret = agp_map_subregion(dev_priv->agph,
			    obj_priv->gtt_offset, obj->size, &bsh)) != 0) {
				DRM_ERROR("Failed to map pages: %d\n", ret);
				return;
			}
			printf("--- gtt_offset = 0x%08x\n",
			    obj_priv->gtt_offset);
			i915_dump_pages(dev_priv->bst, bsh, obj->size);
			agp_unmap_subregion(dev_priv->agph,
			    dev_priv->rings[RCS].bsh, obj->size);
		}
	}
}

void
i915_ringbuffer_data(int kdev)
{
	struct drm_device	*dev = drm_get_device_from_kdev(kdev);
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	bus_size_t		 off;

	if (!dev_priv->rings[RCS].obj) {
		printf("No ringbuffer setup\n");
		return;
	}

	for (off = 0; off < dev_priv->rings[RCS].size; off += 4)
		printf("%08x :  %08x\n", off, bus_space_read_4(dev_priv->bst,
		    dev_priv->rings[RCS].bsh, off));
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
	printf("RingMask :  %08x\n", dev_priv->rings[RCS].size - 1);
	printf("RingSize :  %08lx\n", dev_priv->rings[RCS].size);
	printf("Acthd :  %08x\n", I915_READ(INTEL_INFO(dev)->gen >= 4 ?
	    ACTHD_I965 : ACTHD));
}

#endif

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
	unsigned short id;
	if (pci_find_device(&pa, intel_pch_match) == 0) {
		DRM_DEBUG_KMS("No Intel PCI-ISA bridge found\n");
	}
	id = PCI_PRODUCT(pa.pa_id) & INTEL_PCH_DEVICE_ID_MASK;
	dev_priv->pch_id = id;

	switch (id) {
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
		break;
	case INTEL_PCH_LPT_LP_DEVICE_ID_TYPE:
		dev_priv->pch_type = PCH_LPT;
		dev_priv->num_pch_pll = 0;
		DRM_DEBUG_KMS("Found LynxPoint LP PCH\n");
		break;
	default:
		DRM_DEBUG_KMS("No PCH detected\n");
	}
}

