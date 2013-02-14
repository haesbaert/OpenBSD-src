#include "drmP.h"
#include "drm.h"
#include "i915_drv.h"
#include "intel_drv.h"

void
i915_gem_cleanup_aliasing_ppgtt(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

void
i915_gem_restore_gtt_mappings(struct drm_device *dev)
{
	/* Nothing to be done on OpenBSD */
}
