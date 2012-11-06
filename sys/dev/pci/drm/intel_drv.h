
struct intel_encoder {
	struct drm_encoder		 base;
	int				 type;
	void				 (*hot_plug)(struct intel_encoder *);
	int				 crtc_mask;
	int				 clone_mask;
};

struct intel_connector {
	struct drm_connector		 base;
	struct intel_encoder		*encoder;
};

struct intel_crtc {
	struct drm_crtc			 base;
};

extern bool intel_lvds_init(struct drm_device *dev);
extern struct drm_encoder *intel_best_encoder(struct drm_connector *connector);
extern struct drm_display_mode *intel_crtc_mode_get(struct drm_device *dev,
						    struct drm_crtc *crtc);
extern void intel_encoder_destroy(struct drm_encoder *encoder);

extern void intel_connector_attach_encoder(struct intel_connector *connector,
					   struct intel_encoder *encoder);
extern int intel_panel_setup_backlight(struct drm_device *dev);
extern void intel_fb_output_poll_changed(struct drm_device *dev);
extern int intel_plane_init(struct drm_device *dev, enum pipe pipe);
extern void intel_init_clock_gating(struct drm_device *dev);
extern void ironlake_enable_drps(struct drm_device *dev);
extern void ironlake_disable_drps(struct drm_device *dev);
extern void gen6_enable_rps(struct inteldrm_softc *dev_priv);
extern void gen6_update_ring_freq(struct inteldrm_softc *dev_priv);
extern void gen6_disable_rps(struct drm_device *dev);
extern void intel_init_emon(struct drm_device *dev);


/* these are outputs from the chip - integrated only
   external chips are via DVO or SDVO output */
#define INTEL_OUTPUT_UNUSED 0
#define INTEL_OUTPUT_ANALOG 1
#define INTEL_OUTPUT_DVO 2
#define INTEL_OUTPUT_SDVO 3
#define INTEL_OUTPUT_LVDS 4
#define INTEL_OUTPUT_TVOUT 5
#define INTEL_OUTPUT_HDMI 6
#define INTEL_OUTPUT_DISPLAYPORT 7
#define INTEL_OUTPUT_EDP 8

/* Intel Pipe Clone Bit */
#define INTEL_HDMIB_CLONE_BIT 1
#define INTEL_HDMIC_CLONE_BIT 2
#define INTEL_HDMID_CLONE_BIT 3
#define INTEL_HDMIE_CLONE_BIT 4
#define INTEL_HDMIF_CLONE_BIT 5
#define INTEL_SDVO_NON_TV_CLONE_BIT 6
#define INTEL_SDVO_TV_CLONE_BIT 7
#define INTEL_SDVO_LVDS_CLONE_BIT 8
#define INTEL_ANALOG_CLONE_BIT 9
#define INTEL_TV_CLONE_BIT 10
#define INTEL_DP_B_CLONE_BIT 11
#define INTEL_DP_C_CLONE_BIT 12
#define INTEL_DP_D_CLONE_BIT 13
#define INTEL_LVDS_CLONE_BIT 14
#define INTEL_DVO_TMDS_CLONE_BIT 15
#define INTEL_DVO_LVDS_CLONE_BIT 16
#define INTEL_EDP_CLONE_BIT 17

static inline struct drm_crtc *
intel_get_crtc_for_pipe(struct drm_device *dev, int pipe)
{
	printf("%s stub\n", __func__);
	return (NULL);
}
