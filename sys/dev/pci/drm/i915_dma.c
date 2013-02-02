#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "intel_drv.h"
#include "drm_crtc_helper.h"

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
