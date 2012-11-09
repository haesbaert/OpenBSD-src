#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "intel_drv.h"
#include "drm_dp_helper.h"

#define DP_RECEIVER_CAP_SIZE	0xf
#define DP_LINK_STATUS_SIZE	6
#define DP_LINK_CHECK_TIMEOUT	(10 * 1000)

#define DP_LINK_CONFIGURATION_SIZE	9

/* XXXKIB what is the right code for the FreeBSD ? */
#define EREMOTEIO	ENXIO

struct intel_dp {
	struct intel_encoder base;
	uint32_t output_reg;
	uint32_t DP;
	uint8_t  link_configuration[DP_LINK_CONFIGURATION_SIZE];
	bool has_audio;
	enum hdmi_force_audio force_audio;
	uint32_t color_range;
	int dpms_mode;
	uint8_t link_bw;
	uint8_t lane_count;
	uint8_t dpcd[DP_RECEIVER_CAP_SIZE];
#ifdef notyet
	device_t dp_iic_bus;
	device_t adapter;
#endif
	bool is_pch_edp;
	uint8_t	train_set[4];
	int panel_power_up_delay;
	int panel_power_down_delay;
	int panel_power_cycle_delay;
	int backlight_on_delay;
	int backlight_off_delay;
	struct drm_display_mode *panel_fixed_mode;  /* for eDP */
#ifdef notyet
	struct timeout_task panel_vdd_task;
#endif
	bool want_panel_vdd;
};

#ifdef notyet
/**
 * is_edp - is the given port attached to an eDP panel (either CPU or PCH)
 * @intel_dp: DP struct
 *
 * If a CPU or PCH DP output is attached to an eDP panel, this function
 * will return true, and false otherwise.
 */
static bool is_edp(struct intel_dp *intel_dp)
{
	return intel_dp->base.type == INTEL_OUTPUT_EDP;
}
#endif

/**
 * is_pch_edp - is the port on the PCH and attached to an eDP panel?
 * @intel_dp: DP struct
 *
 * Returns true if the given DP struct corresponds to a PCH DP port attached
 * to an eDP panel, false otherwise.  Helpful for determining whether we
 * may need FDI resources for a given DP output or not.
 */
static bool is_pch_edp(struct intel_dp *intel_dp)
{
	return intel_dp->is_pch_edp;
}

#ifdef notyet
/**
 * is_cpu_edp - is the port on the CPU and attached to an eDP panel?
 * @intel_dp: DP struct
 *
 * Returns true if the given DP struct corresponds to a CPU eDP port.
 */
static bool is_cpu_edp(struct intel_dp *intel_dp)
{
	return is_edp(intel_dp) && !is_pch_edp(intel_dp);
}
#endif

static struct intel_dp *enc_to_intel_dp(struct drm_encoder *encoder)
{
	return container_of(encoder, struct intel_dp, base.base);
}

#ifdef notyet
static struct intel_dp *intel_attached_dp(struct drm_connector *connector)
{
	return container_of(intel_attached_encoder(connector),
			    struct intel_dp, base);
}
#endif

/**
 * intel_encoder_is_pch_edp - is the given encoder a PCH attached eDP?
 * @encoder: DRM encoder
 *
 * Return true if @encoder corresponds to a PCH attached eDP panel.  Needed
 * by intel_display.c.
 */
bool intel_encoder_is_pch_edp(struct drm_encoder *encoder)
{
	struct intel_dp *intel_dp;

	if (!encoder)
		return false;

	intel_dp = enc_to_intel_dp(encoder);

	return is_pch_edp(intel_dp);
}

void
intel_dp_init(struct drm_device *dev, int output_reg)
{
	printf("%s stub\n", __func__);
}

/* check the VBT to see whether the eDP is on DP-D port */
bool intel_dpd_is_edp(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct child_device_config *p_child;
	int i;

	if (!dev_priv->child_dev_num)
		return false;

	for (i = 0; i < dev_priv->child_dev_num; i++) {
		p_child = dev_priv->child_dev + i;

		if (p_child->dvo_port == PORT_IDPD &&
		    p_child->device_type == DEVICE_TYPE_eDP)
			return true;
	}
	return false;
}
