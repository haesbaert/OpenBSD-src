#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_drv.h"

int intel_overlay_switch_off(struct intel_overlay *overlay)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

void intel_setup_overlay(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

int intel_overlay_attrs(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

int intel_overlay_put_image(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}
