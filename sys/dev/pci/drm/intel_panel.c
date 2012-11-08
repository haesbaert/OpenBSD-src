#include "drmP.h"
#include "drm.h"
#include "i915_drv.h"
#include "intel_drv.h"

int i915_panel_ignore_lid = 0;

int intel_panel_setup_backlight(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

enum drm_connector_status
intel_panel_detect(struct drm_device *dev)
{
#if 0
	struct drm_i915_private *dev_priv = dev->dev_private;
#endif

	if (i915_panel_ignore_lid)
		return i915_panel_ignore_lid > 0 ?
			connector_status_connected :
			connector_status_disconnected;

	/* opregion lid state on HP 2540p is wrong at boot up,
	 * appears to be either the BIOS or Linux ACPI fault */
#if 0
	/* Assume that the BIOS does not lie through the OpRegion... */
	if (dev_priv->opregion.lid_state)
		return ioread32(dev_priv->opregion.lid_state) & 0x1 ?
			connector_status_connected :
			connector_status_disconnected;
#endif

	return connector_status_unknown;
}
