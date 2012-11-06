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

static void intel_setup_outputs(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
#ifdef notyet
	struct intel_encoder *encoder;
	bool dpd_is_edp = false;
#endif
	bool has_lvds;

	has_lvds = intel_lvds_init(dev);
	if (!has_lvds && !HAS_PCH_SPLIT(dev_priv)) {
		/* disable the panel fitter on everything but LVDS */
		I915_WRITE(PFIT_CONTROL, 0);
	}

#ifdef notyet
	if (HAS_PCH_SPLIT(dev_priv)) {
		dpd_is_edp = intel_dpd_is_edp(dev);

		if (has_edp_a(dev))
			intel_dp_init(dev, DP_A);

		if (dpd_is_edp && (I915_READ(PCH_DP_D) & DP_DETECTED))
			intel_dp_init(dev, PCH_DP_D);
	}
#endif

#ifdef notyet
	intel_crt_init(dev);
#endif

#ifdef notyet
	if (HAS_PCH_SPLIT(dev_priv)) {
		int found;

		DRM_DEBUG_KMS(
"HDMIB %d PCH_DP_B %d HDMIC %d HDMID %d PCH_DP_C %d PCH_DP_D %d LVDS %d\n",
		    (I915_READ(HDMIB) & PORT_DETECTED) != 0,
		    (I915_READ(PCH_DP_B) & DP_DETECTED) != 0,
		    (I915_READ(HDMIC) & PORT_DETECTED) != 0,
		    (I915_READ(HDMID) & PORT_DETECTED) != 0,
		    (I915_READ(PCH_DP_C) & DP_DETECTED) != 0,
		    (I915_READ(PCH_DP_D) & DP_DETECTED) != 0,
		    (I915_READ(PCH_LVDS) & LVDS_DETECTED) != 0);

		if (I915_READ(HDMIB) & PORT_DETECTED) {
			/* PCH SDVOB multiplex with HDMIB */
			found = intel_sdvo_init(dev, PCH_SDVOB);
			if (!found)
				intel_hdmi_init(dev, HDMIB);
			if (!found && (I915_READ(PCH_DP_B) & DP_DETECTED))
				intel_dp_init(dev, PCH_DP_B);
		}

		if (I915_READ(HDMIC) & PORT_DETECTED)
			intel_hdmi_init(dev, HDMIC);

		if (I915_READ(HDMID) & PORT_DETECTED)
			intel_hdmi_init(dev, HDMID);

		if (I915_READ(PCH_DP_C) & DP_DETECTED)
			intel_dp_init(dev, PCH_DP_C);

		if (!dpd_is_edp && (I915_READ(PCH_DP_D) & DP_DETECTED))
			intel_dp_init(dev, PCH_DP_D);

	} else if (SUPPORTS_DIGITAL_OUTPUTS(dev)) {
		bool found = false;

		if (I915_READ(SDVOB) & SDVO_DETECTED) {
			DRM_DEBUG_KMS("probing SDVOB\n");
			found = intel_sdvo_init(dev, SDVOB);
			if (!found && SUPPORTS_INTEGRATED_HDMI(dev)) {
				DRM_DEBUG_KMS("probing HDMI on SDVOB\n");
				intel_hdmi_init(dev, SDVOB);
			}

			if (!found && SUPPORTS_INTEGRATED_DP(dev)) {
				DRM_DEBUG_KMS("probing DP_B\n");
				intel_dp_init(dev, DP_B);
			}
		}

		/* Before G4X SDVOC doesn't have its own detect register */

		if (I915_READ(SDVOB) & SDVO_DETECTED) {
			DRM_DEBUG_KMS("probing SDVOC\n");
			found = intel_sdvo_init(dev, SDVOC);
		}

		if (!found && (I915_READ(SDVOC) & SDVO_DETECTED)) {

			if (SUPPORTS_INTEGRATED_HDMI(dev)) {
				DRM_DEBUG_KMS("probing HDMI on SDVOC\n");
				intel_hdmi_init(dev, SDVOC);
			}
			if (SUPPORTS_INTEGRATED_DP(dev)) {
				DRM_DEBUG_KMS("probing DP_C\n");
				intel_dp_init(dev, DP_C);
			}
		}

		if (SUPPORTS_INTEGRATED_DP(dev) &&
		    (I915_READ(DP_D) & DP_DETECTED)) {
			DRM_DEBUG_KMS("probing DP_D\n");
			intel_dp_init(dev, DP_D);
		}
	} else if (IS_GEN2(dev)) {
#if 1
		KIB_NOTYET();
#else
		intel_dvo_init(dev);
#endif
	}
#endif // notyet

#ifdef notyet
	if (SUPPORTS_TV(dev))
		intel_tv_init(dev);
#endif

#ifdef notyet
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, base.head) {
		encoder->base.possible_crtcs = encoder->crtc_mask;
		encoder->base.possible_clones =
			intel_encoder_clones(dev, encoder->clone_mask);
	}

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(dev);

	if (HAS_PCH_SPLIT(dev_priv))
		ironlake_init_pch_refclk(dev);
#endif
}

static struct drm_framebuffer *
intel_user_framebuffer_create(struct drm_device *dev,
    struct drm_file *filp, struct drm_mode_fb_cmd *mode_cmd)
{
	printf("%s stub\n", __func__);
	return (NULL);
}


static const struct drm_mode_config_funcs intel_mode_funcs = {
	.fb_create = intel_user_framebuffer_create,
//	.output_poll_changed = intel_fb_output_poll_changed,
};

static void intel_init_quirks(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void intel_init_display(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void intel_crtc_init(struct drm_device *dev, int pipe)
{
	printf("%s stub\n", __func__);
}

/* Disable the VGA plane that we never use */
static void i915_disable_vga(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

void intel_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

void gen6_enable_rps(struct inteldrm_softc *dev_priv)
{
	printf("%s stub\n", __func__);
}

void ironlake_enable_drps(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

void gen6_update_ring_freq(struct inteldrm_softc *dev_priv)
{
	printf("%s stub\n", __func__);
}

void intel_init_emon(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

void intel_modeset_init(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	int i, ret;

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	dev->mode_config.preferred_depth = 24;
	dev->mode_config.prefer_shadow = 1;

	dev->mode_config.funcs = __DECONST(struct drm_mode_config_funcs *,
	    &intel_mode_funcs);

	intel_init_quirks(dev);

	intel_init_display(dev);

	if (IS_GEN2(dev_priv)) {
		dev->mode_config.max_width = 2048;
		dev->mode_config.max_height = 2048;
	} else if (IS_GEN3(dev_priv)) {
		dev->mode_config.max_width = 4096;
		dev->mode_config.max_height = 4096;
	} else {
		dev->mode_config.max_width = 8192;
		dev->mode_config.max_height = 8192;
	}
//	dev->mode_config.fb_base = dev->agp->base;

	DRM_DEBUG_KMS("%d display pipe%s available.\n",
		      dev_priv->num_pipe, dev_priv->num_pipe > 1 ? "s" : "");

	for (i = 0; i < dev_priv->num_pipe; i++) {
		intel_crtc_init(dev, i);
		ret = intel_plane_init(dev, i);
		if (ret)
			DRM_DEBUG_KMS("plane %d init failed: %d\n", i, ret);
	}

	/* Just disable it once at startup */
	i915_disable_vga(dev);
	intel_setup_outputs(dev);

	intel_init_clock_gating(dev);

	if (IS_IRONLAKE_M(dev_priv)) {
		ironlake_enable_drps(dev);
		intel_init_emon(dev);
	}

	if (IS_GEN6(dev_priv)) {
		gen6_enable_rps(dev_priv);
		gen6_update_ring_freq(dev_priv);
	}

#ifdef notyet
	TASK_INIT(&dev_priv->idle_task, 0, intel_idle_update, dev_priv);
	callout_init(&dev_priv->idle_callout, CALLOUT_MPSAFE);
#endif
}

void intel_modeset_gem_init(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}
