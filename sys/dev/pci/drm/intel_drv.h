#include "i915_drm.h"
#include "drm_linux_list.h"

struct intel_framebuffer {
	struct drm_framebuffer base;
#ifdef notyet
	struct drm_i915_gem_object *obj;
#endif
};

struct intel_fbdev {
//	struct drm_fb_helper helper;
	struct intel_framebuffer ifb;
	struct list_head fbdev_list;
	struct drm_display_mode *our_mode;
};

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
	enum pipe			 pipe;
	enum plane			 plane;
	u8				 lut_r[256];
	u8				 lut_g[256];
	u8				 lut_b[256];
	int				 dpms_mode;
	bool active; /* is the crtc on? independent of the dpms mode */
	bool busy; /* is scanout buffer being updated frequently? */
	unsigned int bpp;

	bool no_pll; /* tertiary pipe for IVB */
	bool use_pll_a;
};

struct intel_plane {
	struct drm_plane base;
	enum pipe pipe;
//	struct drm_i915_gem_object *obj;
	bool primary_disabled;
	int max_downscale;
	u32 lut_r[1024], lut_g[1024], lut_b[1024];
#ifdef notyet
	void (*update_plane)(struct drm_plane *plane,
			     struct drm_framebuffer *fb,
			     struct drm_i915_gem_object *obj,
			     int crtc_x, int crtc_y,
			     unsigned int crtc_w, unsigned int crtc_h,
			     uint32_t x, uint32_t y,
			     uint32_t src_w, uint32_t src_h);
#endif
	void (*disable_plane)(struct drm_plane *plane);
	int (*update_colorkey)(struct drm_plane *plane,
			       struct drm_intel_sprite_colorkey *key);
	void (*get_colorkey)(struct drm_plane *plane,
			     struct drm_intel_sprite_colorkey *key);
};

#define to_intel_crtc(x) container_of(x, struct intel_crtc, base)
#define to_intel_connector(x) container_of(x, struct intel_connector, base)
#define to_intel_encoder(x) container_of(x, struct intel_encoder, base)
#define to_intel_framebuffer(x) container_of(x, struct intel_framebuffer, base)
#define to_intel_plane(x) container_of(x, struct intel_plane, base)

#define DIP_HEADER_SIZE	5

#define DIP_TYPE_AVI    0x82
#define DIP_VERSION_AVI 0x2
#define DIP_LEN_AVI     13

#define DIP_TYPE_SPD	0x83
#define DIP_VERSION_SPD	0x1
#define DIP_LEN_SPD	25
#define DIP_SPD_UNKNOWN	0
#define DIP_SPD_DSTB	0x1
#define DIP_SPD_DVDP	0x2
#define DIP_SPD_DVHS	0x3
#define DIP_SPD_HDDVR	0x4
#define DIP_SPD_DVC	0x5
#define DIP_SPD_DSC	0x6
#define DIP_SPD_VCD	0x7
#define DIP_SPD_GAME	0x8
#define DIP_SPD_PC	0x9
#define DIP_SPD_BD	0xa
#define DIP_SPD_SCD	0xb

struct dip_infoframe {
	uint8_t type;		/* HB0 */
	uint8_t ver;		/* HB1 */
	uint8_t len;		/* HB2 - body len, not including checksum */
	uint8_t ecc;		/* Header ECC */
	uint8_t checksum;	/* PB0 */
	union {
		struct {
			/* PB1 - Y 6:5, A 4:4, B 3:2, S 1:0 */
			uint8_t Y_A_B_S;
			/* PB2 - C 7:6, M 5:4, R 3:0 */
			uint8_t C_M_R;
			/* PB3 - ITC 7:7, EC 6:4, Q 3:2, SC 1:0 */
			uint8_t ITC_EC_Q_SC;
			/* PB4 - VIC 6:0 */
			uint8_t VIC;
			/* PB5 - PR 3:0 */
			uint8_t PR;
			/* PB6 to PB13 */
			uint16_t top_bar_end;
			uint16_t bottom_bar_start;
			uint16_t left_bar_end;
			uint16_t right_bar_start;
		} avi;
		struct {
			uint8_t vn[8];
			uint8_t pd[16];
			uint8_t sdi;
		} spd;
		uint8_t payload[27];
	} __attribute__ ((packed)) body;
} __attribute__((packed));

int intel_connector_update_modes(struct drm_connector *connector,
				struct edid *edid);
int intel_ddc_get_modes(struct drm_connector *c, struct i2c_controller *adapter);

extern bool intel_ddc_probe(struct intel_encoder *intel_encoder, int ddc_bus);

extern void intel_attach_force_audio_property(struct drm_connector *connector);
extern void intel_attach_broadcast_rgb_property(struct drm_connector *connector);

extern void intel_crt_init(struct drm_device *dev);
extern void intel_hdmi_init(struct drm_device *dev, int sdvox_reg);
void intel_dip_infoframe_csum(struct dip_infoframe *avi_if);
extern bool intel_sdvo_init(struct drm_device *dev, int output_device);
extern void intel_dvo_init(struct drm_device *dev);
extern void intel_tv_init(struct drm_device *dev);
extern bool intel_lvds_init(struct drm_device *dev);
extern void intel_dp_init(struct drm_device *dev, int dp_reg);
extern bool intel_dpd_is_edp(struct drm_device *dev);
extern bool intel_encoder_is_pch_edp(struct drm_encoder *encoder);
extern struct drm_encoder *intel_best_encoder(struct drm_connector *connector);
extern struct drm_display_mode *intel_crtc_mode_get(struct drm_device *dev,
						    struct drm_crtc *crtc);
extern void intel_wait_for_vblank(struct drm_device *dev, int pipe);
extern void intel_wait_for_pipe_off(struct drm_device *dev, int pipe);

struct intel_load_detect_pipe {
	struct drm_framebuffer *release_fb;
	bool load_detect_temp;
	int dpms_mode;
};
extern bool intel_get_load_detect_pipe(struct intel_encoder *intel_encoder,
				       struct drm_connector *connector,
				       struct drm_display_mode *mode,
				       struct intel_load_detect_pipe *old);
extern void intel_release_load_detect_pipe(struct intel_encoder *intel_encoder,
					   struct drm_connector *connector,
					   struct intel_load_detect_pipe *old);

extern void intel_encoder_destroy(struct drm_encoder *encoder);

extern void intel_connector_attach_encoder(struct intel_connector *connector,
					   struct intel_encoder *encoder);

/* intel_panel.c */
extern void intel_fixed_panel_mode(struct drm_display_mode *fixed_mode,
				   struct drm_display_mode *adjusted_mode);
extern void intel_pch_panel_fitting(struct drm_device *dev,
				    int fitting_mode,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode);
extern u32 intel_panel_get_max_backlight(struct drm_device *dev);
extern u32 intel_panel_get_backlight(struct drm_device *dev);
extern void intel_panel_set_backlight(struct drm_device *dev, u32 level);
extern int intel_panel_setup_backlight(struct drm_device *dev);
extern void intel_panel_enable_backlight(struct drm_device *dev);
extern void intel_panel_disable_backlight(struct drm_device *dev);
extern void intel_panel_destroy_backlight(struct drm_device *dev);
extern enum drm_connector_status intel_panel_detect(struct drm_device *dev);

extern void intel_encoder_prepare(struct drm_encoder *encoder);
extern void intel_encoder_commit(struct drm_encoder *encoder);
extern void intel_encoder_destroy(struct drm_encoder *encoder);

static inline struct intel_encoder *intel_attached_encoder(struct drm_connector *connector)
{
	return to_intel_connector(connector)->encoder;
}

extern void intel_fb_output_poll_changed(struct drm_device *dev);
extern int intel_plane_init(struct drm_device *dev, enum pipe pipe);
extern void intel_init_clock_gating(struct drm_device *dev);
extern void ironlake_enable_drps(struct drm_device *dev);
extern void ironlake_disable_drps(struct drm_device *dev);
extern void gen6_enable_rps(struct inteldrm_softc *dev_priv);
extern void gen6_update_ring_freq(struct inteldrm_softc *dev_priv);
extern void gen6_disable_rps(struct drm_device *dev);
extern void intel_init_emon(struct drm_device *dev);

extern int intel_fbdev_init(struct drm_device *dev);

extern void intel_crtc_load_lut(struct drm_crtc *crtc);
extern void intel_write_eld(struct drm_encoder *encoder,
			    struct drm_display_mode *mode);
extern void intel_cpt_verify_modeset(struct drm_device *dev, int pipe);

/* For use by IVB LP watermark workaround in intel_sprite.c */
extern void sandybridge_update_wm(struct drm_device *dev);

/* maximum connectors per crtcs in the mode set */
#define INTELFB_CONN_LIMIT 4

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

#define INTEL_DVO_CHIP_NONE 0
#define INTEL_DVO_CHIP_LVDS 1
#define INTEL_DVO_CHIP_TMDS 2
#define INTEL_DVO_CHIP_TVOUT 4

/* drm_display_mode->private_flags */
#define INTEL_MODE_PIXEL_MULTIPLIER_SHIFT (0x0)
#define INTEL_MODE_PIXEL_MULTIPLIER_MASK (0xf << INTEL_MODE_PIXEL_MULTIPLIER_SHIFT)
#define INTEL_MODE_DP_FORCE_6BPC (0x10)
/* This flag must be set by the encoder's mode_fixup if it changes the crtc
 * timings in the mode to prevent the crtc fixup from overwriting them.
 * Currently only lvds needs that. */
#define INTEL_MODE_CRTC_TIMINGS_SET (0x20)

static inline struct drm_crtc *
intel_get_crtc_for_pipe(struct drm_device *dev, int pipe)
{
	printf("%s stub\n", __func__);
	return (NULL);
}

extern int intel_sprite_set_colorkey(struct drm_device *dev, void *data,
				     struct drm_file *file_priv);
extern int intel_sprite_get_colorkey(struct drm_device *dev, void *data,
				     struct drm_file *file_priv);
