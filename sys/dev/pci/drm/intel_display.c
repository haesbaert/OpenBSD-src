#include "drmP.h"
#include "drm.h"
#include "i915_drv.h"
#include "intel_drv.h"

struct drm_encoder *intel_best_encoder(struct drm_connector *connector)
{
	printf("%s stub\n", __func__);
	return (NULL);
}

void intel_encoder_destroy(struct drm_encoder *encoder)
{
	printf("%s stub\n", __func__);
}

void intel_connector_attach_encoder(struct intel_connector *connector,
				    struct intel_encoder *encoder)
{
	printf("%s stub\n", __func__);
}

