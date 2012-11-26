#include "drmP.h"
#include "drm.h"
#include "i915_drv.h"
#include "intel_drv.h"
#include "drm_crtc_helper.h"
#include "drm_edid.h"

int i915_panel_use_ssc = -1;

/**
 * intel_wait_for_vblank - wait for vblank on a given pipe
 * @dev: drm device
 * @pipe: pipe to wait for
 *
 * Wait for vblank to occur on a given pipe.  Needed for various bits of
 * mode setting code.
 */
void intel_wait_for_vblank(struct drm_device *dev, int pipe)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	int pipestat_reg = PIPESTAT(pipe);
	int retries;

	/* Clear existing vblank status. Note this will clear any other
	 * sticky status fields as well.
	 *
	 * This races with i915_driver_irq_handler() with the result
	 * that either function could miss a vblank event.  Here it is not
	 * fatal, as we will either wait upon the next vblank interrupt or
	 * timeout.  Generally speaking intel_wait_for_vblank() is only
	 * called during modeset at which time the GPU should be idle and
	 * should *not* be performing page flips and thus not waiting on
	 * vblanks...
	 * Currently, the result of us stealing a vblank from the irq
	 * handler is that a single frame will be skipped during swapbuffers.
	 */
	I915_WRITE(pipestat_reg,
		   I915_READ(pipestat_reg) | PIPE_VBLANK_INTERRUPT_STATUS);

	/* Wait for vblank interrupt bit to set */
	for (retries = 50; retries > 0; retries--) {
		if (I915_READ(pipestat_reg) & PIPE_VBLANK_INTERRUPT_STATUS)
			break;
	}
	if (retries == 0)
		DRM_DEBUG_KMS("vblank wait timed out\n");
}

struct drm_encoder *intel_best_encoder(struct drm_connector *connector)
{
	printf("%s stub\n", __func__);
	return (NULL);
}

void intel_connector_attach_encoder(struct intel_connector *connector,
				    struct intel_encoder *encoder)
{
	connector->encoder = encoder;
	drm_mode_connector_attach_encoder(&connector->base,
					  &encoder->base);
}

static int intel_encoder_clones(struct drm_device *dev, int type_mask)
{
	struct drm_encoder *encoder;
	struct intel_encoder *intel_encoder;
	int index_mask = 0;
	int entry = 0;

	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {
		if (type_mask & intel_encoder->clone_mask)
			index_mask |= (1 << entry);
		entry++;
	}

	return index_mask;
}

#ifdef notyet
static bool has_edp_a(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;

	if (!IS_MOBILE(dev_priv))
		return false;

	if ((I915_READ(DP_A) & DP_DETECTED) == 0)
		return false;

	if (IS_GEN5(dev_priv) &&
	    (I915_READ(ILK_DISPLAY_CHICKEN_FUSES) & ILK_eDP_A_DISABLE))
		return false;

	return true;
}
#endif

static inline bool intel_panel_use_ssc(struct inteldrm_softc *dev_priv)
{
	if (i915_panel_use_ssc >= 0)
		return i915_panel_use_ssc != 0;
	return dev_priv->lvds_use_ssc
	    && !(dev_priv->quirks & QUIRK_LVDS_SSC_DISABLE);
}


/*
 * Initialize reference clocks when the driver loads
 */
void ironlake_init_pch_refclk(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct drm_encoder *encoder;
	struct intel_encoder *intel_encoder;
	u32 temp;
	bool has_lvds = false;
	bool has_cpu_edp = false;
	bool has_pch_edp = false;
	bool has_panel = false;
	bool has_ck505 = false;
	bool can_ssc = false;

	/* We need to take the global config into account */
	TAILQ_FOREACH(encoder, &mode_config->encoder_list, head) {
		switch (intel_encoder->type) {
		case INTEL_OUTPUT_LVDS:
			has_panel = true;
			has_lvds = true;
			break;
		case INTEL_OUTPUT_EDP:
			has_panel = true;
			if (intel_encoder_is_pch_edp(encoder))
				has_pch_edp = true;
			else
				has_cpu_edp = true;
			break;
		}
	}

	if (HAS_PCH_IBX(dev_priv)) {
		has_ck505 = dev_priv->display_clock_mode;
		can_ssc = has_ck505;
	} else {
		has_ck505 = false;
		can_ssc = true;
	}

	DRM_DEBUG_KMS("has_panel %d has_lvds %d has_pch_edp %d has_cpu_edp %d has_ck505 %d\n",
		      has_panel, has_lvds, has_pch_edp, has_cpu_edp,
		      has_ck505);

	/* Ironlake: try to setup display ref clock before DPLL
	 * enabling. This is only under driver's control after
	 * PCH B stepping, previous chipset stepping should be
	 * ignoring this setting.
	 */
	temp = I915_READ(PCH_DREF_CONTROL);
	/* Always enable nonspread source */
	temp &= ~DREF_NONSPREAD_SOURCE_MASK;

	if (has_ck505)
		temp |= DREF_NONSPREAD_CK505_ENABLE;
	else
		temp |= DREF_NONSPREAD_SOURCE_ENABLE;

	if (has_panel) {
		temp &= ~DREF_SSC_SOURCE_MASK;
		temp |= DREF_SSC_SOURCE_ENABLE;

		/* SSC must be turned on before enabling the CPU output  */
		if (intel_panel_use_ssc(dev_priv) && can_ssc) {
			DRM_DEBUG_KMS("Using SSC on panel\n");
			temp |= DREF_SSC1_ENABLE;
		} else
			temp &= ~DREF_SSC1_ENABLE;

		/* Get SSC going before enabling the outputs */
		I915_WRITE(PCH_DREF_CONTROL, temp);
		POSTING_READ(PCH_DREF_CONTROL);
		DELAY(200);

		temp &= ~DREF_CPU_SOURCE_OUTPUT_MASK;

		/* Enable CPU source on CPU attached eDP */
		if (has_cpu_edp) {
			if (intel_panel_use_ssc(dev_priv) && can_ssc) {
				DRM_DEBUG_KMS("Using SSC on eDP\n");
				temp |= DREF_CPU_SOURCE_OUTPUT_DOWNSPREAD;
			}
			else
				temp |= DREF_CPU_SOURCE_OUTPUT_NONSPREAD;
		} else
			temp |= DREF_CPU_SOURCE_OUTPUT_DISABLE;

		I915_WRITE(PCH_DREF_CONTROL, temp);
		POSTING_READ(PCH_DREF_CONTROL);
		DELAY(200);
	} else {
		DRM_DEBUG_KMS("Disabling SSC entirely\n");

		temp &= ~DREF_CPU_SOURCE_OUTPUT_MASK;

		/* Turn off CPU output */
		temp |= DREF_CPU_SOURCE_OUTPUT_DISABLE;

		I915_WRITE(PCH_DREF_CONTROL, temp);
		POSTING_READ(PCH_DREF_CONTROL);
		DELAY(200);

		/* Turn off the SSC source */
		temp &= ~DREF_SSC_SOURCE_MASK;
		temp |= DREF_SSC_SOURCE_DISABLE;

		/* Turn off SSC1 */
		temp &= ~ DREF_SSC1_ENABLE;

		I915_WRITE(PCH_DREF_CONTROL, temp);
		POSTING_READ(PCH_DREF_CONTROL);
		DELAY(200);
	}
}

static void intel_setup_outputs(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct drm_encoder *encoder;
	struct intel_encoder *intel_encoder;
//	bool dpd_is_edp = false;
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

	intel_crt_init(dev);

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

	} else if (SUPPORTS_DIGITAL_OUTPUTS(dev_priv)) {
		bool found = false;

		if (I915_READ(SDVOB) & SDVO_DETECTED) {
			DRM_DEBUG_KMS("probing SDVOB\n");
			found = intel_sdvo_init(dev, SDVOB);
			if (!found && SUPPORTS_INTEGRATED_HDMI(dev_priv)) {
				DRM_DEBUG_KMS("probing HDMI on SDVOB\n");
				intel_hdmi_init(dev, SDVOB);
			}

			if (!found && SUPPORTS_INTEGRATED_DP(dev_priv)) {
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

			if (SUPPORTS_INTEGRATED_HDMI(dev_priv)) {
				DRM_DEBUG_KMS("probing HDMI on SDVOC\n");
				intel_hdmi_init(dev, SDVOC);
			}
			if (SUPPORTS_INTEGRATED_DP(dev_priv)) {
				DRM_DEBUG_KMS("probing DP_C\n");
				intel_dp_init(dev, DP_C);
			}
		}

		if (SUPPORTS_INTEGRATED_DP(dev_priv) &&
		    (I915_READ(DP_D) & DP_DETECTED)) {
			DRM_DEBUG_KMS("probing DP_D\n");
			intel_dp_init(dev, DP_D);
		}
	} else if (IS_GEN2(dev_priv)) {
		intel_dvo_init(dev);
	}

	if (SUPPORTS_TV(dev_priv))
		intel_tv_init(dev);
#endif // notyet

	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {
	
		intel_encoder = to_intel_encoder(encoder);
		encoder->possible_crtcs = intel_encoder->crtc_mask;
		encoder->possible_clones =
			intel_encoder_clones(dev, intel_encoder->clone_mask);
	}

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(dev);

	if (HAS_PCH_SPLIT(dev_priv))
		ironlake_init_pch_refclk(dev);
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

static void intel_disable_plane(struct inteldrm_softc *dev_priv,
				enum plane plane, enum pipe pipe)
{
	printf("%s stub\n", __func__);
}

static void intel_disable_pipe(struct inteldrm_softc *dev_priv,
			       enum pipe pipe)
{
	printf("%s stub\n", __func__);
}

static void intel_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	printf("%s stub\n", __func__);
}

static bool intel_crtc_mode_fixup(struct drm_crtc *crtc,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	printf("%s stub\n", __func__);
	return false;
}

static int intel_crtc_mode_set(struct drm_crtc *crtc,
			       struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode,
			       int x, int y,
			       struct drm_framebuffer *old_fb)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static int
intel_pipe_set_base(struct drm_crtc *crtc, int x, int y,
		    struct drm_framebuffer *old_fb)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static int
intel_pipe_set_base_atomic(struct drm_crtc *crtc, struct drm_framebuffer *fb,
			   int x, int y, enum mode_set_atomic state)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

void intel_crtc_load_lut(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static void intel_crtc_disable(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static int intel_crtc_cursor_set(struct drm_crtc *crtc,
				 struct drm_file *file,
				 uint32_t handle,
				 uint32_t width, uint32_t height)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static int intel_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static void intel_crtc_gamma_set(struct drm_crtc *crtc, u16 *red, u16 *green,
				 u16 *blue, uint32_t start, uint32_t size)
{
	printf("%s stub\n", __func__);
}

void intel_write_eld(struct drm_encoder *encoder,
		     struct drm_display_mode *mode)
{
	struct drm_crtc *crtc = encoder->crtc;
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;
	struct inteldrm_softc *dev_priv = dev->dev_private;

	connector = drm_select_eld(encoder, mode);
	if (!connector)
		return;

	DRM_DEBUG_KMS("ELD on [CONNECTOR:%d:%s], [ENCODER:%d:%s]\n",
			 connector->base.id,
			 drm_get_connector_name(connector),
			 connector->encoder->base.id,
			 drm_get_encoder_name(connector->encoder));

	connector->eld[6] = drm_av_sync_delay(connector, mode) / 2;

	if (dev_priv->display.write_eld)
		dev_priv->display.write_eld(connector, crtc);
}

/**
 * Get a pipe with a simple mode set on it for doing load-based monitor
 * detection.
 *
 * It will be up to the load-detect code to adjust the pipe as appropriate for
 * its requirements.  The pipe will be connected to no other encoders.
 * 
 * Currently this code will only succeed if there is a pipe with no encoders
 * configured for it.  In the future, it could choose to temporarily disable
 * some outputs to free up a pipe for its use.
 * 
 * \return crtc, or NULL if no pipes are available.
 */

/* VESA 640x480x72Hz mode to set on the pipe */
static struct drm_display_mode load_detect_mode = {
	DRM_MODE("640x480", DRM_MODE_TYPE_DEFAULT, 31500, 640, 664,
		704, 832, 0, 480, 489, 491, 520, 0, DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
};

static int
intel_framebuffer_create_for_mode(struct drm_device *dev,
    struct drm_display_mode *mode, int depth, int bpp,
    struct drm_framebuffer **res)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static int
mode_fits_in_fbdev(struct drm_device *dev,
    struct drm_display_mode *mode, struct drm_framebuffer **res)
{
	printf("%s stub\n", __func__);
	*res = NULL;
	return (0);
#if 0
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	struct drm_framebuffer *fb;

	if (dev_priv->fbdev == NULL) {
		*res = NULL;
		return (0);
	}

	obj = dev_priv->fbdev->ifb.obj;
	if (obj == NULL) {
		*res = NULL;
		return (0);
	}

	fb = &dev_priv->fbdev->ifb.base;
	if (fb->pitches[0] < intel_framebuffer_pitch_for_width(mode->hdisplay,
	    fb->bits_per_pixel)) {
		*res = NULL;
		return (0);
	}

	if (obj->base.size < mode->vdisplay * fb->pitches[0]) {
		*res = NULL;
		return (0);
	}

	*res = fb;
	return (0);
#endif
}

bool intel_get_load_detect_pipe(struct intel_encoder *intel_encoder,
				struct drm_connector *connector,
				struct drm_display_mode *mode,
				struct intel_load_detect_pipe *old)
{
	struct intel_crtc *intel_crtc;
	struct drm_crtc *possible_crtc;
	struct drm_encoder *encoder = &intel_encoder->base;
	struct drm_crtc *crtc = NULL;
	struct drm_device *dev = encoder->dev;
	struct drm_framebuffer *old_fb;
	int i = -1, r;

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s], [ENCODER:%d:%s]\n",
		      connector->base.id, drm_get_connector_name(connector),
		      encoder->base.id, drm_get_encoder_name(encoder));

	/*
	 * Algorithm gets a little messy:
	 *
	 *   - if the connector already has an assigned crtc, use it (but make
	 *     sure it's on first)
	 *
	 *   - try to find the first unused crtc that can drive this connector,
	 *     and use that if we find one
	 */

	/* See if we already have a CRTC for this connector */
	if (encoder->crtc) {
		crtc = encoder->crtc;

		intel_crtc = to_intel_crtc(crtc);
		old->dpms_mode = intel_crtc->dpms_mode;
		old->load_detect_temp = false;

		/* Make sure the crtc and connector are running */
		if (intel_crtc->dpms_mode != DRM_MODE_DPMS_ON) {
			struct drm_encoder_helper_funcs *encoder_funcs;
			struct drm_crtc_helper_funcs *crtc_funcs;

			crtc_funcs = crtc->helper_private;
			crtc_funcs->dpms(crtc, DRM_MODE_DPMS_ON);

			encoder_funcs = encoder->helper_private;
			encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);
		}

		return true;
	}

	/* Find an unused one (if possible) */
	TAILQ_FOREACH(possible_crtc, &dev->mode_config.crtc_list, head) {
		i++;
		if (!(encoder->possible_crtcs & (1 << i)))
			continue;
		if (!possible_crtc->enabled) {
			crtc = possible_crtc;
			break;
		}
	}

	/*
	 * If we didn't find an unused CRTC, don't use any.
	 */
	if (!crtc) {
		DRM_DEBUG_KMS("no pipe available for load-detect\n");
		return false;
	}

	encoder->crtc = crtc;
	connector->encoder = encoder;

	intel_crtc = to_intel_crtc(crtc);
	old->dpms_mode = intel_crtc->dpms_mode;
	old->load_detect_temp = true;
	old->release_fb = NULL;

	if (!mode)
		mode = &load_detect_mode;

	old_fb = crtc->fb;

	/* We need a framebuffer large enough to accommodate all accesses
	 * that the plane may generate whilst we perform load detection.
	 * We can not rely on the fbcon either being present (we get called
	 * during its initialisation to detect all boot displays, or it may
	 * not even exist) or that it is large enough to satisfy the
	 * requested mode.
	 */
	r = mode_fits_in_fbdev(dev, mode, &crtc->fb);
	if (crtc->fb == NULL) {
		DRM_DEBUG_KMS("creating tmp fb for load-detection\n");
		r = intel_framebuffer_create_for_mode(dev, mode, 24, 32,
		    &crtc->fb);
		old->release_fb = crtc->fb;
	} else
		DRM_DEBUG_KMS("reusing fbdev for load-detection framebuffer\n");
	if (r != 0) {
		DRM_DEBUG_KMS("failed to allocate framebuffer for load-detection\n");
		crtc->fb = old_fb;
		return false;
	}

	if (!drm_crtc_helper_set_mode(crtc, mode, 0, 0, old_fb)) {
		DRM_DEBUG_KMS("failed to set mode on load-detect pipe\n");
		if (old->release_fb)
			old->release_fb->funcs->destroy(old->release_fb);
		crtc->fb = old_fb;
		return false;
	}

	/* let the connector get through one full cycle before testing */
	intel_wait_for_vblank(dev, intel_crtc->pipe);

	return true;
}

void intel_release_load_detect_pipe(struct intel_encoder *intel_encoder,
				    struct drm_connector *connector,
				    struct intel_load_detect_pipe *old)
{
	struct drm_encoder *encoder = &intel_encoder->base;
	struct drm_device *dev = encoder->dev;
	struct drm_crtc *crtc = encoder->crtc;
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
	struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s], [ENCODER:%d:%s]\n",
		      connector->base.id, drm_get_connector_name(connector),
		      encoder->base.id, drm_get_encoder_name(encoder));

	if (old->load_detect_temp) {
		connector->encoder = NULL;
		drm_helper_disable_unused_functions(dev);

		if (old->release_fb)
			old->release_fb->funcs->destroy(old->release_fb);

		return;
	}

	/* Switch crtc and encoder back off if necessary */
	if (old->dpms_mode != DRM_MODE_DPMS_ON) {
		encoder_funcs->dpms(encoder, old->dpms_mode);
		crtc_funcs->dpms(crtc, old->dpms_mode);
	}
}


static void intel_crtc_destroy(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

void intel_cpt_verify_modeset(struct drm_device *dev, int pipe)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	int dslreg = PIPEDSL(pipe), tc2reg = TRANS_CHICKEN2(pipe);
	u32 temp;
	int retries;

	temp = I915_READ(dslreg);
	DELAY(500);
	for (retries = 5; retries > 0; retries--) {
		if (I915_READ(dslreg) != temp)
			break;
	}
	if (retries == 0) {
		/* Without this, mode sets may fail silently on FDI */
		I915_WRITE(tc2reg, TRANS_AUTOTRAIN_GEN_STALL_DIS);
		DELAY(250);
		I915_WRITE(tc2reg, 0);
		for (retries = 5; retries > 0; retries--) {
			if (I915_READ(dslreg) != temp)
				break;
		}
		if (retries == 0)
			DRM_ERROR("mode set failed: pipe %d stuck\n", pipe);
	}
}

static void ironlake_crtc_enable(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static void ironlake_crtc_disable(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static void i9xx_crtc_enable(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

static void i9xx_crtc_disable(struct drm_crtc *crtc)
{
	printf("%s stub\n", __func__);
}

/* Prepare for a mode set.
 *
 * Note we could be a lot smarter here.  We need to figure out which outputs
 * will be enabled, which disabled (in short, how the config will changes)
 * and perform the minimum necessary steps to accomplish that, e.g. updating
 * watermarks, FBC configuration, making sure PLLs are programmed correctly,
 * panel fitting is in the proper state, etc.
 */
static void i9xx_crtc_prepare(struct drm_crtc *crtc)
{
	i9xx_crtc_disable(crtc);
}

static void i9xx_crtc_commit(struct drm_crtc *crtc)
{
	i9xx_crtc_enable(crtc);
}

static void ironlake_crtc_prepare(struct drm_crtc *crtc)
{
	ironlake_crtc_disable(crtc);
}

static void ironlake_crtc_commit(struct drm_crtc *crtc)
{
	ironlake_crtc_enable(crtc);
}

void intel_encoder_prepare(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
	/* lvds has its own version of prepare see intel_lvds_prepare */
	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
}

void intel_encoder_commit(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
	struct drm_device *dev = encoder->dev;
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct intel_encoder *intel_encoder = to_intel_encoder(encoder);
	struct intel_crtc *intel_crtc = to_intel_crtc(intel_encoder->base.crtc);

	/* lvds has its own version of commit see intel_lvds_commit */
	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);

	if (HAS_PCH_CPT(dev_priv))
		intel_cpt_verify_modeset(dev, intel_crtc->pipe);
}

void intel_encoder_destroy(struct drm_encoder *encoder)
{
	struct intel_encoder *intel_encoder = to_intel_encoder(encoder);

	drm_encoder_cleanup(encoder);
	free(intel_encoder, M_DRM);
}


static int intel_crtc_page_flip(struct drm_crtc *crtc,
				struct drm_framebuffer *fb,
				struct drm_pending_vblank_event *event)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static void intel_sanitize_modesetting(struct drm_device *dev,
				       int pipe, int plane)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	u32 reg, val;

	/* Clear any frame start delays used for debugging left by the BIOS */
	for_each_pipe(pipe) {
		reg = PIPECONF(pipe);
		I915_WRITE(reg, I915_READ(reg) & ~PIPECONF_FRAME_START_DELAY_MASK);
	}

	if (HAS_PCH_SPLIT(dev_priv))
		return;

	/* Who knows what state these registers were left in by the BIOS or
	 * grub?
	 *
	 * If we leave the registers in a conflicting state (e.g. with the
	 * display plane reading from the other pipe than the one we intend
	 * to use) then when we attempt to teardown the active mode, we will
	 * not disable the pipes and planes in the correct order -- leaving
	 * a plane reading from a disabled pipe and possibly leading to
	 * undefined behaviour.
	 */

	reg = DSPCNTR(plane);
	val = I915_READ(reg);

	if ((val & DISPLAY_PLANE_ENABLE) == 0)
		return;
	if (!!(val & DISPPLANE_SEL_PIPE_MASK) == pipe)
		return;

	/* This display plane is active and attached to the other CPU pipe. */
	pipe = !pipe;

	/* Disable the plane and wait for it to stop reading from the pipe. */
	intel_disable_plane(dev_priv, plane, pipe);
	intel_disable_pipe(dev_priv, pipe);
}

static void intel_crtc_reset(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);

	/* Reset flags back to the 'unknown' status so that they
	 * will be correctly set on the initial modeset.
	 */
	intel_crtc->dpms_mode = -1;

	/* We need to fix up any BIOS configuration that conflicts with
	 * our expectations.
	 */
	intel_sanitize_modesetting(dev, intel_crtc->pipe, intel_crtc->plane);
}

static struct drm_crtc_helper_funcs intel_helper_funcs = {
	.dpms = intel_crtc_dpms,
	.mode_fixup = intel_crtc_mode_fixup,
	.mode_set = intel_crtc_mode_set,
	.mode_set_base = intel_pipe_set_base,
	.mode_set_base_atomic = intel_pipe_set_base_atomic,
	.load_lut = intel_crtc_load_lut,
	.disable = intel_crtc_disable,
};

static const struct drm_crtc_funcs intel_crtc_funcs = {
	.reset = intel_crtc_reset,
	.cursor_set = intel_crtc_cursor_set,
	.cursor_move = intel_crtc_cursor_move,
	.gamma_set = intel_crtc_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.destroy = intel_crtc_destroy,
	.page_flip = intel_crtc_page_flip,
};

static void intel_crtc_init(struct drm_device *dev, int pipe)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc;
	int i;

	intel_crtc = malloc(sizeof(struct intel_crtc) +
	    (INTELFB_CONN_LIMIT * sizeof(struct drm_connector *)),
	    M_DRM, M_WAITOK | M_ZERO);

	drm_crtc_init(dev, &intel_crtc->base, &intel_crtc_funcs);

	drm_mode_crtc_set_gamma_size(&intel_crtc->base, 256);
	for (i = 0; i < 256; i++) {
		intel_crtc->lut_r[i] = i;
		intel_crtc->lut_g[i] = i;
		intel_crtc->lut_b[i] = i;
	}

	/* Swap pipes & planes for FBC on pre-965 */
	intel_crtc->pipe = pipe;
	intel_crtc->plane = pipe;
	if (IS_MOBILE(dev_priv) && IS_GEN3(dev_priv)) {
		DRM_DEBUG_KMS("swapping pipes & planes for FBC\n");
		intel_crtc->plane = !pipe;
	}

	KASSERT(pipe < nitems(dev_priv->plane_to_crtc_mapping) &&
	    dev_priv->plane_to_crtc_mapping[intel_crtc->plane] == NULL);
#ifdef notyet
	    ("plane_to_crtc is already initialized"));
#endif
	dev_priv->plane_to_crtc_mapping[intel_crtc->plane] = &intel_crtc->base;
	dev_priv->pipe_to_crtc_mapping[intel_crtc->pipe] = &intel_crtc->base;

	intel_crtc_reset(&intel_crtc->base);
	intel_crtc->active = true; /* force the pipe off on setup_init_config */
	intel_crtc->bpp = 24; /* default for pre-Ironlake */

	if (HAS_PCH_SPLIT(dev_priv)) {
		if (pipe == 2 && IS_IVYBRIDGE(dev_priv))
			intel_crtc->no_pll = true;
		intel_helper_funcs.prepare = ironlake_crtc_prepare;
		intel_helper_funcs.commit = ironlake_crtc_commit;
	} else {
		intel_helper_funcs.prepare = i9xx_crtc_prepare;
		intel_helper_funcs.commit = i9xx_crtc_commit;
	}

	drm_crtc_helper_add(&intel_crtc->base, &intel_helper_funcs);

	intel_crtc->busy = false;

#ifdef notyet
	callout_init(&intel_crtc->idle_callout, CALLOUT_MPSAFE);
#endif
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

/* Disable the VGA plane that we never use */
static void i915_disable_vga(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	u8 sr1;
	u32 vga_reg;

printf("%s skipping vga disable for now...\n", __func__);
return;

	if (HAS_PCH_SPLIT(dev_priv))
		vga_reg = CPU_VGACNTRL;
	else
		vga_reg = VGACNTRL;

//	vga_get_uninterruptible(dev->pdev, VGA_RSRC_LEGACY_IO);
	outb(VGA_SR_INDEX, 1);
	sr1 = inb(VGA_SR_DATA);
	outb(VGA_SR_DATA, sr1 | 1<<5);
//	vga_put(dev->pdev, VGA_RSRC_LEGACY_IO);
	DELAY(300);

	I915_WRITE(vga_reg, VGA_DISP_DISABLE);
	POSTING_READ(vga_reg);
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
