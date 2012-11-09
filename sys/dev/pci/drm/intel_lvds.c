#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"
#include "i915_drv.h"
#include "intel_drv.h"

/* Private structure for the integrated LVDS support */
struct intel_lvds {
	struct intel_encoder	 base;

	struct edid 		*edid;

	int			 fitting_mode;
	uint32_t		 pfit_control;
	uint32_t		 pfit_pgm_ratios;
	bool			 pfit_dirty;

	struct drm_display_mode	*fixed_mode;
};

static struct intel_lvds *intel_attached_lvds(struct drm_connector *connector)
{
	return container_of(intel_attached_encoder(connector),
			    struct intel_lvds, base);
}

static void intel_lvds_dpms(struct drm_encoder *encoder, int mode)
{
	printf("%s stub\n", __func__);
}

static bool intel_lvds_mode_fixup(struct drm_encoder *encoder, 
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	printf("%s stub\n", __func__);
	return false;
}

static void intel_lvds_prepare(struct drm_encoder *encoder)
{
	printf("%s stub\n", __func__);
}

static void intel_lvds_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	printf("%s stub\n", __func__);
}

static void intel_lvds_commit(struct drm_encoder *encoder)
{
	printf("%s stub\n", __func__);
}

static int intel_lvds_set_property(struct drm_connector *connector,
				   struct drm_property *property,
				   uint64_t value)
{
	printf("%s stub\n", __func__);
	return EINVAL;
}

static void intel_lvds_destroy(struct drm_connector *connector)
{
	printf("%s stub\n", __func__);
}

static int intel_lvds_mode_valid(struct drm_connector *connector,
				 struct drm_display_mode *mode)
{
	struct intel_lvds *intel_lvds = intel_attached_lvds(connector);
	struct drm_display_mode *fixed_mode = intel_lvds->fixed_mode;

	if (mode->hdisplay > fixed_mode->hdisplay)
		return MODE_PANEL;
	if (mode->vdisplay > fixed_mode->vdisplay)
		return MODE_PANEL;

	return MODE_OK;
}

/**
 * Detect the LVDS connection.
 *
 * Since LVDS doesn't have hotlug, we use the lid as a proxy.  Open means
 * connected and closed means disconnected.  We also send hotplug events as
 * needed, using lid status notification from the input layer.
 */
static enum drm_connector_status
intel_lvds_detect(struct drm_connector *connector, bool force)
{
	struct drm_device *dev = connector->dev;
	enum drm_connector_status status;

	status = intel_panel_detect(dev);
	if (status != connector_status_unknown)
		return status;

	return connector_status_connected;
}

/**
 * Return the list of DDC modes if available, or the BIOS fixed mode otherwise.
 */
static int intel_lvds_get_modes(struct drm_connector *connector)
{
	struct intel_lvds *intel_lvds = intel_attached_lvds(connector);
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode;

	if (intel_lvds->edid)
		return drm_add_edid_modes(connector, intel_lvds->edid);

	mode = drm_mode_duplicate(dev, intel_lvds->fixed_mode);
	if (mode == NULL)
		return 0;

	drm_mode_probed_add(connector, mode);
	return 1;
}

static const struct drm_encoder_helper_funcs intel_lvds_helper_funcs = {
	.dpms = intel_lvds_dpms,
	.mode_fixup = intel_lvds_mode_fixup,
	.prepare = intel_lvds_prepare,
	.mode_set = intel_lvds_mode_set,
	.commit = intel_lvds_commit,
};

static const struct drm_connector_helper_funcs intel_lvds_connector_helper_funcs = {
	.get_modes = intel_lvds_get_modes,
	.mode_valid = intel_lvds_mode_valid,
	.best_encoder = intel_best_encoder,
};

static const struct drm_connector_funcs intel_lvds_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = intel_lvds_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = intel_lvds_set_property,
	.destroy = intel_lvds_destroy,
};

static const struct drm_encoder_funcs intel_lvds_enc_funcs = {
	.destroy = intel_encoder_destroy,
};

static void intel_find_lvds_downclock(struct drm_device *dev,
				      struct drm_display_mode *fixed_mode,
				      struct drm_connector *connector)
{
	printf("%s stub\n", __func__);
}


static bool intel_lvds_supported(struct drm_device *dev)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;

	/* With the introduction of the PCH we gained a dedicated
	 * LVDS presence pin, use it. */
	if (HAS_PCH_SPLIT(dev_priv))
		return true;

	/* Otherwise LVDS was only attached to mobile products,
	 * except for the inglorious 830gm */
	return IS_MOBILE(dev_priv) && !IS_I830(dev_priv);
}

static int intel_no_lvds_dmi_callback(const struct dmi_system_id *id)
{
	DRM_DEBUG_KMS("Skipping LVDS initialization for %s\n", id->ident);
	return 1;
}

/* These systems claim to have LVDS, but really don't */
static const struct dmi_system_id intel_no_lvds[] = {
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Apple Mac Mini (Core series)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Macmini1,1"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Apple Mac Mini (Core 2 series)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Macmini2,1"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "MSI IM-945GSE-A",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "MSI"),
			DMI_MATCH(DMI_PRODUCT_NAME, "A9830IMS"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Dell Studio Hybrid",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Studio Hybrid 140g"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Dell OptiPlex FX170",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex FX170"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "AOpen Mini PC",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "AOpen"),
			DMI_MATCH(DMI_PRODUCT_NAME, "i965GMx-IF"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "AOpen Mini PC MP915",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AOpen"),
			DMI_MATCH(DMI_BOARD_NAME, "i915GMx-F"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "AOpen i915GMm-HFS",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AOpen"),
			DMI_MATCH(DMI_BOARD_NAME, "i915GMm-HFS"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
                .ident = "AOpen i45GMx-I",
                .matches = {
                        DMI_MATCH(DMI_BOARD_VENDOR, "AOpen"),
                        DMI_MATCH(DMI_BOARD_NAME, "i45GMx-I"),
                },
        },
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Aopen i945GTt-VFA",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_VERSION, "AO00001JW"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Clientron U800",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Clientron"),
			DMI_MATCH(DMI_PRODUCT_NAME, "U800"),
		},
	},
	{
                .callback = intel_no_lvds_dmi_callback,
                .ident = "Clientron E830",
                .matches = {
                        DMI_MATCH(DMI_SYS_VENDOR, "Clientron"),
                        DMI_MATCH(DMI_PRODUCT_NAME, "E830"),
                },
        },
        {
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Asus EeeBox PC EB1007",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "EB1007"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Asus AT5NM10T-I",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "ASUSTeK Computer INC."),
			DMI_MATCH(DMI_BOARD_NAME, "AT5NM10T-I"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Hewlett-Packard t5745",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "hp t5745"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Hewlett-Packard st5747",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "hp st5747"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "MSI Wind Box DC500",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "MICRO-STAR INTERNATIONAL CO., LTD"),
			DMI_MATCH(DMI_BOARD_NAME, "MS-7469"),
		},
	},

	{ }	/* terminating entry */
};

/*
 * Enumerate the child dev array parsed from VBT to check whether
 * the LVDS is present.
 * If it is present, return 1.
 * If it is not present, return false.
 * If no child dev is parsed from VBT, it assumes that the LVDS is present.
 */
static bool lvds_is_present_in_vbt(struct drm_device *dev,
				   u8 *i2c_pin)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	int i;

	if (!dev_priv->child_dev_num)
		return true;

	for (i = 0; i < dev_priv->child_dev_num; i++) {
		struct child_device_config *child = dev_priv->child_dev + i;

		/* If the device type is not LFP, continue.
		 * We have to check both the new identifiers as well as the
		 * old for compatibility with some BIOSes.
		 */
		if (child->device_type != DEVICE_TYPE_INT_LFP &&
		    child->device_type != DEVICE_TYPE_LFP)
			continue;

		if (child->i2c_pin)
		    *i2c_pin = child->i2c_pin;

		/* However, we cannot trust the BIOS writers to populate
		 * the VBT correctly.  Since LVDS requires additional
		 * information from AIM blocks, a non-zero addin offset is
		 * a good indicator that the LVDS is actually present.
		 */
		if (child->addin_offset)
			return true;

		/* But even then some BIOS writers perform some black magic
		 * and instantiate the device without reference to any
		 * additional data.  Trust that if the VBT was written into
		 * the OpRegion then they have validated the LVDS's existence.
		 */
		if (dev_priv->opregion.vbt)
			return true;
	}

	return false;
}

/**
 * intel_lvds_init - setup LVDS connectors on this device
 * @dev: drm device
 *
 * Create the connector, register the LVDS DDC bus, and try to figure out what
 * modes we can display on the LVDS panel (if present).
 */
bool intel_lvds_init(struct drm_device *dev)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct intel_lvds *intel_lvds;
	struct intel_encoder *intel_encoder;
	struct intel_connector *intel_connector;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_display_mode *scan; /* *modes, *bios_mode; */
	struct drm_crtc *crtc;
	u32 lvds;
	int pipe;
	u8 pin;

	if (!intel_lvds_supported(dev))
		return false;

	/* Skip init on machines we know falsely report LVDS */
	if (dmi_check_system(intel_no_lvds))
		return false;

	pin = GMBUS_PORT_PANEL;
	if (!lvds_is_present_in_vbt(dev, &pin)) {
		DRM_DEBUG_KMS("LVDS is not present in VBT\n");
		return false;
	}

	if (HAS_PCH_SPLIT(dev_priv)) {
		if ((I915_READ(PCH_LVDS) & LVDS_DETECTED) == 0)
			return false;
		if (dev_priv->edp.support) {
			DRM_DEBUG_KMS("disable LVDS for eDP support\n");
			return false;
		}
	}

	intel_lvds = malloc(sizeof(struct intel_lvds), M_DRM,
	    M_WAITOK | M_ZERO);
	intel_connector = malloc(sizeof(struct intel_connector), M_DRM,
	    M_WAITOK | M_ZERO);

	if (!HAS_PCH_SPLIT(dev_priv)) {
		intel_lvds->pfit_control = I915_READ(PFIT_CONTROL);
	}

	intel_encoder = &intel_lvds->base;
	encoder = &intel_encoder->base;
	connector = &intel_connector->base;
	drm_connector_init(dev, &intel_connector->base, &intel_lvds_connector_funcs,
			   DRM_MODE_CONNECTOR_LVDS);

	drm_encoder_init(dev, &intel_encoder->base, &intel_lvds_enc_funcs,
			 DRM_MODE_ENCODER_LVDS);

	intel_connector_attach_encoder(intel_connector, intel_encoder);
	intel_encoder->type = INTEL_OUTPUT_LVDS;

	intel_encoder->clone_mask = (1 << INTEL_LVDS_CLONE_BIT);
	if (HAS_PCH_SPLIT(dev_priv))
		intel_encoder->crtc_mask = (1 << 0) | (1 << 1) | (1 << 2);
	else
		intel_encoder->crtc_mask = (1 << 1);

	drm_encoder_helper_add(encoder, &intel_lvds_helper_funcs);
	drm_connector_helper_add(connector, &intel_lvds_connector_helper_funcs);
	connector->display_info.subpixel_order = SubPixelHorizontalRGB;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;

	/* create the scaling mode property */
	drm_mode_create_scaling_mode_property(dev);
	/*
	 * the initial panel fitting mode will be FULL_SCREEN.
	 */

	drm_connector_attach_property(&intel_connector->base,
				      dev->mode_config.scaling_mode_property,
				      DRM_MODE_SCALE_ASPECT);
	intel_lvds->fitting_mode = DRM_MODE_SCALE_ASPECT;
	/*
	 * LVDS discovery:
	 * 1) check for EDID on DDC
	 * 2) check for VBT data
	 * 3) check to see if LVDS is already on
	 *    if none of the above, no panel
	 * 4) make sure lid is open
	 *    if closed, act like it's not there for now
	 */

	/*
	 * Attempt to get the fixed panel mode from DDC.  Assume that the
	 * preferred mode is the right one.
	 */
	intel_lvds->edid = drm_get_edid(connector, dev_priv->ddc);
	if (intel_lvds->edid) {
		if (drm_add_edid_modes(connector,
				       intel_lvds->edid)) {
			drm_mode_connector_update_edid_property(connector,
								intel_lvds->edid);
		} else {
			free(intel_lvds->edid, M_DRM);
			intel_lvds->edid = NULL;
		}
	}
	if (!intel_lvds->edid) {
		/* Didn't get an EDID, so
		 * Set wide sync ranges so we get all modes
		 * handed to valid_mode for checking
		 */
		connector->display_info.min_vfreq = 0;
		connector->display_info.max_vfreq = 200;
		connector->display_info.min_hfreq = 0;
		connector->display_info.max_hfreq = 200;
	}

	TAILQ_FOREACH(scan, &connector->probed_modes, head) {
		if (scan->type & DRM_MODE_TYPE_PREFERRED) {
			intel_lvds->fixed_mode =
				drm_mode_duplicate(dev, scan);
			intel_find_lvds_downclock(dev,
						  intel_lvds->fixed_mode,
						  connector);
			goto out;
		}
	}

	/* Failed to get EDID, what about VBT? */
	if (dev_priv->lfp_lvds_vbt_mode) {
		intel_lvds->fixed_mode =
			drm_mode_duplicate(dev, dev_priv->lfp_lvds_vbt_mode);
		if (intel_lvds->fixed_mode) {
			intel_lvds->fixed_mode->type |=
				DRM_MODE_TYPE_PREFERRED;
			goto out;
		}
	}

	/*
	 * If we didn't get EDID, try checking if the panel is already turned
	 * on.  If so, assume that whatever is currently programmed is the
	 * correct mode.
	 */

	/* Ironlake: FIXME if still fail, not try pipe mode now */
	if (HAS_PCH_SPLIT(dev_priv))
		goto failed;

	lvds = I915_READ(LVDS);
	pipe = (lvds & LVDS_PIPEB_SELECT) ? 1 : 0;
	crtc = intel_get_crtc_for_pipe(dev, pipe);

	if (crtc && (lvds & LVDS_PORT_EN)) {
		intel_lvds->fixed_mode = intel_crtc_mode_get(dev, crtc);
		if (intel_lvds->fixed_mode) {
			intel_lvds->fixed_mode->type |=
				DRM_MODE_TYPE_PREFERRED;
			goto out;
		}
	}

	/* If we still don't have a mode after all that, give up. */
	if (!intel_lvds->fixed_mode)
		goto failed;

out:
	if (HAS_PCH_SPLIT(dev_priv)) {
		u32 pwm;

		pipe = (I915_READ(PCH_LVDS) & LVDS_PIPEB_SELECT) ? 1 : 0;

		/* make sure PWM is enabled and locked to the LVDS pipe */
		pwm = I915_READ(BLC_PWM_CPU_CTL2);
		if (pipe == 0 && (pwm & PWM_PIPE_B))
			I915_WRITE(BLC_PWM_CPU_CTL2, pwm & ~PWM_ENABLE);
		if (pipe)
			pwm |= PWM_PIPE_B;
		else
			pwm &= ~PWM_PIPE_B;
		I915_WRITE(BLC_PWM_CPU_CTL2, pwm | PWM_ENABLE);

		pwm = I915_READ(BLC_PWM_PCH_CTL1);
		pwm |= PWM_PCH_ENABLE;
		I915_WRITE(BLC_PWM_PCH_CTL1, pwm);
		/*
		 * Unlock registers and just
		 * leave them unlocked
		 */
		I915_WRITE(PCH_PP_CONTROL,
			   I915_READ(PCH_PP_CONTROL) | PANEL_UNLOCK_REGS);
	} else {
		/*
		 * Unlock registers and just
		 * leave them unlocked
		 */
		I915_WRITE(PP_CONTROL,
			   I915_READ(PP_CONTROL) | PANEL_UNLOCK_REGS);
	}
#ifdef NOTYET
	dev_priv->lid_notifier.notifier_call = intel_lid_notify;
	if (acpi_lid_notifier_register(&dev_priv->lid_notifier)) {
		DRM_DEBUG_KMS("lid notifier registration failed\n");
		dev_priv->lid_notifier.notifier_call = NULL;
	}
#endif
	/* keep the LVDS connector */
	dev_priv->int_lvds_connector = connector;
#if 0
	drm_sysfs_connector_add(connector);
#endif
	intel_panel_setup_backlight(dev);
	return true;

failed:
	DRM_DEBUG_KMS("No LVDS modes found, disabling.\n");
	drm_connector_cleanup(connector);
	drm_encoder_cleanup(encoder);
	free(intel_lvds, M_DRM);
	free(intel_connector, M_DRM);
	return false;
}
