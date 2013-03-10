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
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;

	list_for_each_entry(obj, &dev_priv->mm.bound_list, gtt_list) {
		i915_gem_clflush_object(obj);
		i915_gem_gtt_rebind_object(obj, obj->cache_level);
	}

	i915_gem_chipset_flush(dev);
}

void
i915_gem_gtt_rebind_object(struct drm_i915_gem_object *obj,
			   enum i915_cache_level cache_level)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int flags = obj->dma_flags;

	switch (cache_level) {
	case I915_CACHE_NONE:
		flags |= BUS_DMA_GTT_NOCACHE;
		break;
	case I915_CACHE_LLC:
		flags |= BUS_DMA_GTT_CACHE_LLC;
		break;
	case I915_CACHE_LLC_MLC:
		flags |= BUS_DMA_GTT_CACHE_LLC_MLC;
		break;
	default:
		BUG();
	}

	agp_bus_dma_rebind(dev_priv->agpdmat, obj->dmamap, flags);
}
