/* i915_dma.c -- DMA support for the I915 -*- linux-c -*-
 */
/*
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "intel_drv.h"
#include "drm_crtc_helper.h"

int
inteldrm_getparam(struct inteldrm_softc *dev_priv, void *data)
{
	drm_i915_getparam_t	*param = data;
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	int			 value;

	switch (param->param) {
	case I915_PARAM_CHIPSET_ID:
		value = dev->pci_device;
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
	case I915_PARAM_HAS_OVERLAY:
		value = dev_priv->overlay ? 1 : 0;
		break;
	case I915_PARAM_HAS_BSD:
#ifdef notyet
		// XXX support BSD ring
		value = HAS_BSD(dev);
#else
		return EINVAL;
#endif
		break;
	case I915_PARAM_HAS_BLT:
#ifdef notyet
		// XXX support BLT ring
		value = HAS_BLT(dev);
#else
		return EINVAL;
#endif
		break;
	case I915_PARAM_HAS_RELAXED_FENCING:
		value = 0;
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
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	u_int64_t		 mchbar_addr;
	pcireg_t		 tmp, low, high = 0;
	u_long			 addr;
	int			 reg, ret = 1, enabled = 0;

	reg = INTEL_INFO(dev)->gen >= 4 ?  MCHBAR_I965 : MCHBAR_I915;

	if (IS_I915G(dev) || IS_I915GM(dev)) {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, DEVEN_REG);
		enabled = !!(tmp & DEVEN_MCHBAR_EN);
	} else {
		tmp = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		enabled = tmp & 1;
	}

	if (enabled) {
		return (0);
	}

	if (INTEL_INFO(dev)->gen >= 4)
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
			if (INTEL_INFO(dev)->gen >= 4)
				pci_conf_write(bpa->pa_pc, bpa->pa_tag,
				    reg + 4, upper_32_bits(mchbar_addr));
			pci_conf_write(bpa->pa_pc, bpa->pa_tag,
			    reg, mchbar_addr & 0xffffffff);
		}
	}
	/* set the enable bit */
	if (IS_I915G(dev) || IS_I915GM(dev)) {
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
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	u_int64_t		 mchbar_addr;
	pcireg_t		 tmp, low, high = 0;
	int			 reg;

	reg = INTEL_INFO(dev)->gen >= 4 ? MCHBAR_I965 : MCHBAR_I915;

	switch(disable) {
	case 2:
		if (INTEL_INFO(dev)->gen >= 4)
			high = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg + 4);
		low = pci_conf_read(bpa->pa_pc, bpa->pa_tag, reg);
		mchbar_addr = ((u_int64_t)high << 32) | low;
		if (bpa->pa_memex)
			extent_free(bpa->pa_memex, mchbar_addr, MCHBAR_SIZE, 0);
		/* FALLTHROUGH */
	case 1:
		if (IS_I915G(dev) || IS_I915GM(dev)) {
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

int
i915_load_modeset_init(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	int ret;

	ret = intel_parse_bios(dev);
	if (ret)
		DRM_INFO("failed to find VBIOS tables\n");

#if 0
	intel_register_dsm_handler();
#endif

	/* IIR "flip pending" bit means done if this bit is set */
	if (IS_GEN3(dev) && (I915_READ(ECOSKPD) & ECO_FLIP_DONE))
		dev_priv->flip_pending_is_done = true;

	intel_modeset_init(dev);

	ret = i915_gem_init(dev);
	if (ret != 0)
		goto cleanup_gem;

	intel_modeset_gem_init(dev);

	ret = drm_irq_install(dev);
	if (ret)
		goto cleanup_gem;

	dev->vblank_disable_allowed = 1;

	ret = intel_fbdev_init(dev);
	if (ret)
		goto cleanup_gem;

	drm_kms_helper_poll_init(dev);

	/* We're off and running w/KMS */
	dev_priv->mm.suspended = 0;

	return (0);

cleanup_gem:
	DRM_LOCK();
	i915_gem_cleanup_ringbuffer(dev);
	DRM_UNLOCK();
	i915_gem_cleanup_aliasing_ppgtt(dev);
	return (ret);
}

void
inteldrm_lastclose(struct drm_device *dev)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct vm_page		*p;
	int			 ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		intel_fb_restore_mode(dev);
		return;
	}

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
