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
	connector->encoder = encoder;
	drm_mode_connector_attach_encoder(&connector->base,
					  &encoder->base);
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

static void ironlake_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	printf("%s stub\n", __func__);
}

static int ironlake_crtc_mode_set(struct drm_crtc *crtc,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode,
				  int x, int y,
				  struct drm_framebuffer *old_fb)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static int ironlake_update_plane(struct drm_crtc *crtc,
				 struct drm_framebuffer *fb, int x, int y)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static void i9xx_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	printf("%s stub\n", __func__);
}

static int i9xx_crtc_mode_set(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode,
			      int x, int y,
			      struct drm_framebuffer *old_fb)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static int i9xx_update_plane(struct drm_crtc *crtc, struct drm_framebuffer *fb,
			     int x, int y)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static bool ironlake_fbc_enabled(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
	return false;
}

static void ironlake_enable_fbc(struct drm_crtc *crtc, unsigned long interval)
{
	printf("%s stub\n", __func__);
}

static void ironlake_disable_fbc(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static bool g4x_fbc_enabled(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
	return false;
}

static void g4x_enable_fbc(struct drm_crtc *crtc, unsigned long interval)
{
	printf("%s stub\n", __func__);
}

static void g4x_disable_fbc(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static bool i8xx_fbc_enabled(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
	return false;
}

static void i8xx_enable_fbc(struct drm_crtc *crtc, unsigned long interval)
{
	printf("%s stub\n", __func__);
}

static void i8xx_disable_fbc(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static int i945_get_display_clock_speed(struct drm_device *dev)
{
	return 400000;
}

static int i915_get_display_clock_speed(struct drm_device *dev)
{
	return 333000;
}

static int i9xx_misc_get_display_clock_speed(struct drm_device *dev)
{
	return 200000;
}

static int i915gm_get_display_clock_speed(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
	return 0;
}

static int i865_get_display_clock_speed(struct drm_device *dev)
{
	return 266000;
}

static int i855_get_display_clock_speed(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
	return 0;
}

static int i830_get_display_clock_speed(struct drm_device *dev)
{
	return 133000;
}

static void ibx_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void cpt_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void ironlake_update_wm(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void ironlake_fdi_link_train(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static void ironlake_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void ironlake_write_eld(struct drm_connector *connector,
			       struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

void sandybridge_update_wm(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void sandybridge_update_sprite_wm(struct drm_device *dev, int pipe,
					 uint32_t sprite_width, int pixel_size)
{
	printf("%s stub\n", __func__);
}

static void gen6_fdi_link_train(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static void gen6_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void ivb_manual_fdi_link_train(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static void ivybridge_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void pineview_update_wm(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void gen3_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void g4x_write_eld(struct drm_connector *connector,
			  struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static void g4x_update_wm(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void g4x_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void i965_update_wm(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void crestline_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void broadwater_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void i9xx_update_wm(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static int i9xx_get_fifo_size(struct drm_device *dev, int plane)
{
	printf("%s stub\n", __func__);
	return 0;
}

static void i830_update_wm(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static void i85x_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static int i830_get_fifo_size(struct drm_device *dev, int plane)
{
	printf("%s stub\n", __func__);
	return 0;
}

static int i85x_get_fifo_size(struct drm_device *dev, int plane)
{
	printf("%s stub\n", __func__);
	return 0;
}

static void i830_init_clock_gating(struct drm_device *dev)
{
	printf("%s stub\n", __func__);
}

static int i845_get_fifo_size(struct drm_device *dev, int plane)
{
	printf("%s stub\n", __func__);
	return 0;
}

struct cxsr_latency {
	int is_desktop;
	int is_ddr3;
	unsigned long fsb_freq;
	unsigned long mem_freq;
	unsigned long display_sr;
	unsigned long display_hpll_disable;
	unsigned long cursor_sr;
	unsigned long cursor_hpll_disable;
};

static const struct cxsr_latency cxsr_latency_table[] = {
	{1, 0, 800, 400, 3382, 33382, 3983, 33983},    /* DDR2-400 SC */
	{1, 0, 800, 667, 3354, 33354, 3807, 33807},    /* DDR2-667 SC */
	{1, 0, 800, 800, 3347, 33347, 3763, 33763},    /* DDR2-800 SC */
	{1, 1, 800, 667, 6420, 36420, 6873, 36873},    /* DDR3-667 SC */
	{1, 1, 800, 800, 5902, 35902, 6318, 36318},    /* DDR3-800 SC */

	{1, 0, 667, 400, 3400, 33400, 4021, 34021},    /* DDR2-400 SC */
	{1, 0, 667, 667, 3372, 33372, 3845, 33845},    /* DDR2-667 SC */
	{1, 0, 667, 800, 3386, 33386, 3822, 33822},    /* DDR2-800 SC */
	{1, 1, 667, 667, 6438, 36438, 6911, 36911},    /* DDR3-667 SC */
	{1, 1, 667, 800, 5941, 35941, 6377, 36377},    /* DDR3-800 SC */

	{1, 0, 400, 400, 3472, 33472, 4173, 34173},    /* DDR2-400 SC */
	{1, 0, 400, 667, 3443, 33443, 3996, 33996},    /* DDR2-667 SC */
	{1, 0, 400, 800, 3430, 33430, 3946, 33946},    /* DDR2-800 SC */
	{1, 1, 400, 667, 6509, 36509, 7062, 37062},    /* DDR3-667 SC */
	{1, 1, 400, 800, 5985, 35985, 6501, 36501},    /* DDR3-800 SC */

	{0, 0, 800, 400, 3438, 33438, 4065, 34065},    /* DDR2-400 SC */
	{0, 0, 800, 667, 3410, 33410, 3889, 33889},    /* DDR2-667 SC */
	{0, 0, 800, 800, 3403, 33403, 3845, 33845},    /* DDR2-800 SC */
	{0, 1, 800, 667, 6476, 36476, 6955, 36955},    /* DDR3-667 SC */
	{0, 1, 800, 800, 5958, 35958, 6400, 36400},    /* DDR3-800 SC */

	{0, 0, 667, 400, 3456, 33456, 4103, 34106},    /* DDR2-400 SC */
	{0, 0, 667, 667, 3428, 33428, 3927, 33927},    /* DDR2-667 SC */
	{0, 0, 667, 800, 3443, 33443, 3905, 33905},    /* DDR2-800 SC */
	{0, 1, 667, 667, 6494, 36494, 6993, 36993},    /* DDR3-667 SC */
	{0, 1, 667, 800, 5998, 35998, 6460, 36460},    /* DDR3-800 SC */

	{0, 0, 400, 400, 3528, 33528, 4255, 34255},    /* DDR2-400 SC */
	{0, 0, 400, 667, 3500, 33500, 4079, 34079},    /* DDR2-667 SC */
	{0, 0, 400, 800, 3487, 33487, 4029, 34029},    /* DDR2-800 SC */
	{0, 1, 400, 667, 6566, 36566, 7145, 37145},    /* DDR3-667 SC */
	{0, 1, 400, 800, 6042, 36042, 6584, 36584},    /* DDR3-800 SC */
};

static const struct cxsr_latency *intel_get_cxsr_latency(int is_desktop,
							 int is_ddr3,
							 int fsb,
							 int mem)
{
	const struct cxsr_latency *latency;
	int i;

	if (fsb == 0 || mem == 0)
		return NULL;

	for (i = 0; i < nitems(cxsr_latency_table); i++) {
		latency = &cxsr_latency_table[i];
		if (is_desktop == latency->is_desktop &&
		    is_ddr3 == latency->is_ddr3 &&
		    fsb == latency->fsb_freq && mem == latency->mem_freq)
			return latency;
	}

	DRM_DEBUG_KMS("Unknown FSB/MEM found, disable CxSR\n");

	return NULL;
}

static void pineview_disable_cxsr(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;

	/* deactivate cxsr */
	I915_WRITE(DSPFW3, I915_READ(DSPFW3) & ~PINEVIEW_SELF_REFRESH_EN);
}


/* Set up chip specific display functions */
static void intel_init_display(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;

	/* We always want a DPMS function */
	if (HAS_PCH_SPLIT(dev_priv)) {
		dev_priv->display.dpms = ironlake_crtc_dpms;
		dev_priv->display.crtc_mode_set = ironlake_crtc_mode_set;
		dev_priv->display.update_plane = ironlake_update_plane;
	} else {
		dev_priv->display.dpms = i9xx_crtc_dpms;
		dev_priv->display.crtc_mode_set = i9xx_crtc_mode_set;
		dev_priv->display.update_plane = i9xx_update_plane;
	}

	if (I915_HAS_FBC(dev_priv)) {
		if (HAS_PCH_SPLIT(dev_priv)) {
			dev_priv->display.fbc_enabled = ironlake_fbc_enabled;
			dev_priv->display.enable_fbc = ironlake_enable_fbc;
			dev_priv->display.disable_fbc = ironlake_disable_fbc;
		} else if (IS_GM45(dev_priv)) {
			dev_priv->display.fbc_enabled = g4x_fbc_enabled;
			dev_priv->display.enable_fbc = g4x_enable_fbc;
			dev_priv->display.disable_fbc = g4x_disable_fbc;
		} else if (IS_CRESTLINE(dev_priv)) {
			dev_priv->display.fbc_enabled = i8xx_fbc_enabled;
			dev_priv->display.enable_fbc = i8xx_enable_fbc;
			dev_priv->display.disable_fbc = i8xx_disable_fbc;
		}
		/* 855GM needs testing */
	}

	/* Returns the core display clock speed */
	if (IS_I945G(dev_priv) || (IS_G33(dev_priv) && !IS_PINEVIEW_M(dev_priv)))
		dev_priv->display.get_display_clock_speed =
			i945_get_display_clock_speed;
	else if (IS_I915G(dev_priv))
		dev_priv->display.get_display_clock_speed =
			i915_get_display_clock_speed;
	else if (IS_I945GM(dev_priv) || IS_845G(dev_priv) || IS_PINEVIEW_M(dev_priv))
		dev_priv->display.get_display_clock_speed =
			i9xx_misc_get_display_clock_speed;
	else if (IS_I915GM(dev_priv))
		dev_priv->display.get_display_clock_speed =
			i915gm_get_display_clock_speed;
	else if (IS_I865G(dev_priv))
		dev_priv->display.get_display_clock_speed =
			i865_get_display_clock_speed;
	else if (IS_I85X(dev_priv))
		dev_priv->display.get_display_clock_speed =
			i855_get_display_clock_speed;
	else /* 852, 830 */
		dev_priv->display.get_display_clock_speed =
			i830_get_display_clock_speed;

	/* For FIFO watermark updates */
	if (HAS_PCH_SPLIT(dev_priv)) {
		dev_priv->display.force_wake_get = __gen6_gt_force_wake_get;
		dev_priv->display.force_wake_put = __gen6_gt_force_wake_put;

		/* IVB configs may use multi-threaded forcewake */
		if (IS_IVYBRIDGE(dev_priv)) {
			u32	ecobus;

			/* A small trick here - if the bios hasn't configured MT forcewake,
			 * and if the device is in RC6, then force_wake_mt_get will not wake
			 * the device and the ECOBUS read will return zero. Which will be
			 * (correctly) interpreted by the test below as MT forcewake being
			 * disabled.
			 */
			DRM_LOCK();
			__gen6_gt_force_wake_mt_get(dev_priv);
			ecobus = I915_READ_NOTRACE(ECOBUS);
			__gen6_gt_force_wake_mt_put(dev_priv);
			DRM_UNLOCK();

			if (ecobus & FORCEWAKE_MT_ENABLE) {
				DRM_DEBUG_KMS("Using MT version of forcewake\n");
				dev_priv->display.force_wake_get =
					__gen6_gt_force_wake_mt_get;
				dev_priv->display.force_wake_put =
					__gen6_gt_force_wake_mt_put;
			}
		}

		if (HAS_PCH_IBX(dev_priv))
			dev_priv->display.init_pch_clock_gating = ibx_init_clock_gating;
		else if (HAS_PCH_CPT(dev_priv))
			dev_priv->display.init_pch_clock_gating = cpt_init_clock_gating;

		if (IS_GEN5(dev_priv)) {
			if (I915_READ(MLTR_ILK) & ILK_SRLT_MASK)
				dev_priv->display.update_wm = ironlake_update_wm;
			else {
				DRM_DEBUG_KMS("Failed to get proper latency. "
					      "Disable CxSR\n");
				dev_priv->display.update_wm = NULL;
			}
			dev_priv->display.fdi_link_train = ironlake_fdi_link_train;
			dev_priv->display.init_clock_gating = ironlake_init_clock_gating;
			dev_priv->display.write_eld = ironlake_write_eld;
		} else if (IS_GEN6(dev_priv)) {
			if (SNB_READ_WM0_LATENCY()) {
				dev_priv->display.update_wm = sandybridge_update_wm;
				dev_priv->display.update_sprite_wm = sandybridge_update_sprite_wm;
			} else {
				DRM_DEBUG_KMS("Failed to read display plane latency. "
					      "Disable CxSR\n");
				dev_priv->display.update_wm = NULL;
			}
			dev_priv->display.fdi_link_train = gen6_fdi_link_train;
			dev_priv->display.init_clock_gating = gen6_init_clock_gating;
			dev_priv->display.write_eld = ironlake_write_eld;
		} else if (IS_IVYBRIDGE(dev_priv)) {
			/* FIXME: detect B0+ stepping and use auto training */
			dev_priv->display.fdi_link_train = ivb_manual_fdi_link_train;
			if (SNB_READ_WM0_LATENCY()) {
				dev_priv->display.update_wm = sandybridge_update_wm;
				dev_priv->display.update_sprite_wm = sandybridge_update_sprite_wm;
			} else {
				DRM_DEBUG_KMS("Failed to read display plane latency. "
					      "Disable CxSR\n");
				dev_priv->display.update_wm = NULL;
			}
			dev_priv->display.init_clock_gating = ivybridge_init_clock_gating;
			dev_priv->display.write_eld = ironlake_write_eld;
		} else
			dev_priv->display.update_wm = NULL;
	} else if (IS_PINEVIEW(dev_priv)) {
		if (!intel_get_cxsr_latency(IS_PINEVIEW_G(dev_priv),
					    dev_priv->is_ddr3,
					    dev_priv->fsb_freq,
					    dev_priv->mem_freq)) {
			DRM_INFO("failed to find known CxSR latency "
				 "(found ddr%s fsb freq %d, mem freq %d), "
				 "disabling CxSR\n",
				 (dev_priv->is_ddr3 == 1) ? "3" : "2",
				 dev_priv->fsb_freq, dev_priv->mem_freq);
			/* Disable CxSR and never update its watermark again */
			pineview_disable_cxsr(dev);
			dev_priv->display.update_wm = NULL;
		} else
			dev_priv->display.update_wm = pineview_update_wm;
		dev_priv->display.init_clock_gating = gen3_init_clock_gating;
	} else if (IS_G4X(dev_priv)) {
		dev_priv->display.write_eld = g4x_write_eld;
		dev_priv->display.update_wm = g4x_update_wm;
		dev_priv->display.init_clock_gating = g4x_init_clock_gating;
	} else if (IS_GEN4(dev_priv)) {
		dev_priv->display.update_wm = i965_update_wm;
		if (IS_CRESTLINE(dev_priv))
			dev_priv->display.init_clock_gating = crestline_init_clock_gating;
		else if (IS_BROADWATER(dev_priv))
			dev_priv->display.init_clock_gating = broadwater_init_clock_gating;
	} else if (IS_GEN3(dev_priv)) {
		dev_priv->display.update_wm = i9xx_update_wm;
		dev_priv->display.get_fifo_size = i9xx_get_fifo_size;
		dev_priv->display.init_clock_gating = gen3_init_clock_gating;
	} else if (IS_I865G(dev_priv)) {
		dev_priv->display.update_wm = i830_update_wm;
		dev_priv->display.init_clock_gating = i85x_init_clock_gating;
		dev_priv->display.get_fifo_size = i830_get_fifo_size;
	} else if (IS_I85X(dev_priv)) {
		dev_priv->display.update_wm = i9xx_update_wm;
		dev_priv->display.get_fifo_size = i85x_get_fifo_size;
		dev_priv->display.init_clock_gating = i85x_init_clock_gating;
	} else {
		dev_priv->display.update_wm = i830_update_wm;
		dev_priv->display.init_clock_gating = i830_init_clock_gating;
		if (IS_845G(dev_priv))
			dev_priv->display.get_fifo_size = i845_get_fifo_size;
		else
			dev_priv->display.get_fifo_size = i830_get_fifo_size;
	}

#ifdef notyet
	/* Default just returns -ENODEV to indicate unsupported */
	dev_priv->display.queue_flip = intel_default_queue_flip;

	switch (INTEL_INFO(dev_priv)->gen) {
	case 2:
		dev_priv->display.queue_flip = intel_gen2_queue_flip;
		break;

	case 3:
		dev_priv->display.queue_flip = intel_gen3_queue_flip;
		break;

	case 4:
	case 5:
		dev_priv->display.queue_flip = intel_gen4_queue_flip;
		break;

	case 6:
		dev_priv->display.queue_flip = intel_gen6_queue_flip;
		break;
	case 7:
		dev_priv->display.queue_flip = intel_gen7_queue_flip;
		break;
	}
#endif
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
