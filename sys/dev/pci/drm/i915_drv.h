/* $OpenBSD: i915_drv.h,v 1.73 2012/09/25 10:19:46 jsg Exp $ */
/* i915_drv.h -- Private header for the I915 driver -*- linux-c -*-
 */
/*
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _I915_DRV_H_
#define _I915_DRV_H_

#include "i915_reg.h"
#include "i915_drm.h"
#include "intel_bios.h"
#include "intel_ringbuffer.h"

/* General customization:
 */

#define DRIVER_AUTHOR		"Tungsten Graphics, Inc."

#define DRIVER_NAME		"i915"
#define DRIVER_DESC		"Intel Graphics"
#define DRIVER_DATE		"20080730"

enum pipe {
	PIPE_A = 0,
	PIPE_B,
	PIPE_C,
	I915_MAX_PIPES
};
#define pipe_name(p) ((p) + 'A')

enum plane {
	PLANE_A = 0,
	PLANE_B,
	PLANE_C,
};
#define plane_name(p) ((p) + 'A')

#define for_each_pipe(p) for ((p) = 0; (p) < dev_priv->num_pipe; (p)++)

/* Interface history:
 *
 * 1.1: Original.
 * 1.2: Add Power Management
 * 1.3: Add vblank support
 * 1.4: Fix cmdbuffer path, add heap destroy
 * 1.5: Add vblank pipe configuration
 * 1.6: - New ioctl for scheduling buffer swaps on vertical blank
 *      - Support vertical blank on secondary display pipe
 */
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		6
#define DRIVER_PATCHLEVEL	0

#define I915_GEM_PHYS_CURSOR_0 1
#define I915_GEM_PHYS_CURSOR_1 2
#define I915_GEM_PHYS_OVERLAY_REGS 3
#define I915_MAX_PHYS_OBJECT (I915_GEM_PHYS_OVERLAY_REGS)

struct drm_i915_gem_phys_object {
	int id;
	struct drm_dmamem *handle;
	struct drm_i915_gem_object *cur_obj;
};

struct inteldrm_softc;

struct drm_i915_display_funcs {
	void (*dpms)(struct drm_crtc *crtc, int mode);
	bool (*fbc_enabled)(struct drm_device *dev);
	void (*enable_fbc)(struct drm_crtc *crtc, unsigned long interval);
	void (*disable_fbc)(struct drm_device *dev);
	int (*get_display_clock_speed)(struct drm_device *dev);
	int (*get_fifo_size)(struct drm_device *dev, int plane);
	void (*update_wm)(struct drm_device *dev);
	void (*update_sprite_wm)(struct drm_device *dev, int pipe,
				 uint32_t sprite_width, int pixel_size);
	int (*crtc_mode_set)(struct drm_crtc *crtc,
			     struct drm_display_mode *mode,
			     struct drm_display_mode *adjusted_mode,
			     int x, int y,
			     struct drm_framebuffer *old_fb);
	void (*write_eld)(struct drm_connector *connector,
			  struct drm_crtc *crtc);
	void (*fdi_link_train)(struct drm_crtc *crtc);
	void (*init_clock_gating)(struct drm_device *dev);
	void (*init_pch_clock_gating)(struct drm_device *dev);
	int (*queue_flip)(struct drm_device *dev, struct drm_crtc *crtc,
			  struct drm_framebuffer *fb,
			  struct drm_i915_gem_object *obj);
	void (*force_wake_get)(struct inteldrm_softc *dev_priv);
	void (*force_wake_put)(struct inteldrm_softc *dev_priv);
	int (*update_plane)(struct drm_crtc *crtc, struct drm_framebuffer *fb,
			    int x, int y);
	/* clock updates for mode set */
	/* cursor updates */
	/* render clock increase/decrease */
	/* display clock increase/decrease */
	/* pll clock increase/decrease */
};

struct intel_device_info {
	u8 gen;
	u8 is_mobile:1;
	u8 is_i85x:1;
	u8 is_i915g:1;
	u8 is_i945gm:1;
	u8 is_g33:1;
	u8 need_gfx_hws:1;
	u8 is_g4x:1;
	u8 is_pineview:1;
	u8 is_broadwater:1;
	u8 is_crestline:1;
	u8 is_ivybridge:1;
	u8 is_valleyview:1;
	u8 has_force_wake:1;
	u8 is_haswell:1;
	u8 has_fbc:1;
	u8 has_pipe_cxsr:1;
	u8 has_hotplug:1;
	u8 cursor_needs_physical:1;
	u8 has_overlay:1;
	u8 overlay_needs_physical:1;
	u8 supports_tv:1;
	u8 has_bsd_ring:1;
	u8 has_blt_ring:1;
	u8 has_llc:1;
};

enum no_fbc_reason {
	FBC_NO_OUTPUT, /* no outputs enabled to compress */
	FBC_STOLEN_TOO_SMALL, /* not enough space to hold compressed buffers */
	FBC_UNSUPPORTED_MODE, /* interlace or doublescanned mode */
	FBC_MODE_TOO_LARGE, /* mode too large for compression */
	FBC_BAD_PLANE, /* fbc not supported on plane */
	FBC_NOT_TILED, /* buffer not tiled */
	FBC_MULTIPLE_PIPES, /* more than one pipe active */
	FBC_MODULE_PARAM,
};

struct opregion_header;
struct opregion_acpi;
struct opregion_swsci;
struct opregion_asle;

struct intel_opregion {
	struct opregion_header *header;
	struct opregion_acpi *acpi;
	struct opregion_swsci *swsci;
	struct opregion_asle *asle;
	void *vbt;
	u32 *lid_state;
};
#define OPREGION_SIZE            (8*1024)


#define I915_FENCE_REG_NONE -1

struct drm_i915_fence_reg {
	TAILQ_ENTRY(drm_i915_fence_reg)	 list;
	struct drm_obj			*obj;
	u_int32_t			 last_rendering_seqno;
};

struct sdvo_device_mapping {
	u8 initialized;
	u8 dvo_port;
	u8 slave_addr;
	u8 dvo_wiring;
	u8 i2c_pin;
	u8 ddc_pin;
};

struct gmbus_port {
	struct inteldrm_softc *dev_priv;
	int port;
};

enum intel_pch {
	PCH_NONE = 0,	/* No PCH present */
	PCH_IBX,	/* Ibexpeak PCH */
	PCH_CPT,	/* Cougarpoint PCH */
	PCH_LPT,	/* Lynxpoint PCH */
};

#define QUIRK_PIPEA_FORCE	(1<<0)
#define QUIRK_LVDS_SSC_DISABLE	(1<<1)

struct intel_fbdev;

/*
 * lock ordering:
 * exec lock,
 * request lock
 * list lock.
 *
 * XXX fence lock ,object lock
 */
struct inteldrm_softc {
	struct device		 dev;
	struct device		*drmdev;
	bus_dma_tag_t		 agpdmat; /* tag from intagp for GEM */
	bus_dma_tag_t		 dmat;
	bus_space_tag_t		 bst;
	struct agp_map		*agph;
	bus_space_handle_t	 opregion_ioh;

	struct i2c_controller	 ddc;
	struct gmbus_port	 gp;

	u_long			 flags;

	pci_chipset_tag_t	 pc;
	pcitag_t		 tag;
	pci_intr_handle_t	 ih;
	void			*irqh;

	struct vga_pci_bar	*regs;

	uint32_t		 gpio_mmio_base;

	/** gt_fifo_count and the subsequent register write are synchronized
	 * with dev->struct_mutex. */
	unsigned gt_fifo_count;
	/** forcewake_count is protected by gt_lock */
	unsigned forcewake_count;
	/** gt_lock is also taken in irq contexts. */
	struct mutex gt_lock;

	drm_i915_sarea_t *sarea_priv;
	struct intel_ring_buffer rings[I915_NUM_RINGS];

	union flush {
		struct {
			bus_space_tag_t		bst;
			bus_space_handle_t	bsh;
		} i9xx;
		struct {
			bus_dma_segment_t	seg;
			caddr_t			kva;
		} i8xx;
	}			 ifp;
	struct workq		*workq;
	struct vm_page		*pgs;
	union hws {
		struct drm_obj		*obj;
		struct drm_dmamem	*dmamem;
	}	hws;
#define				 hws_obj	hws.obj
#define				 hws_dmamem	hws.dmamem
	void			*hw_status_page;
	size_t			 max_gem_obj_size; /* XXX */

	/* Protects user_irq_refcount and irq_mask reg */
	struct mutex		 user_irq_lock;
	/* Refcount for user irq, only enabled when needed */
	int			 user_irq_refcount;
	/* Cached value of IMR to avoid reads in updating the bitfield */
	u_int32_t		 irq_mask_reg;
	u_int32_t		 pipestat[2];
	/* these two  ironlake only, we should union this with pipestat XXX */
	u_int32_t		 gt_irq_mask_reg;
	u_int32_t		 pch_irq_mask_reg;

	u_int32_t		 hotplug_supported_mask;

	int			 num_pipe;
	int			 num_pch_pll;

	struct intel_opregion	 opregion;

	int crt_ddc_pin;

	/* overlay */
        struct intel_overlay *overlay;
	bool sprite_scaling_enabled;

	/* LVDS info */
	int backlight_level;  /* restore backlight to this value */
	bool backlight_enabled;
	struct drm_display_mode *lfp_lvds_vbt_mode; /* if any */
	struct drm_display_mode *sdvo_lvds_vbt_mode; /* if any */

	/* Feature bits from the VBIOS */
	unsigned int int_tv_support:1;
	unsigned int lvds_dither:1;
	unsigned int lvds_vbt:1;
	unsigned int int_crt_support:1;
	unsigned int lvds_use_ssc:1;
	unsigned int display_clock_mode:1;
	int lvds_ssc_freq;
	struct {
		int rate;
		int lanes;
		int preemphasis;
		int vswing;

		bool initialized;
		bool support;
		int bpp;
		struct edp_power_seq pps;
	} edp;
	bool no_aux_handshake;

	bool			 render_reclock_avail;
	bool			 lvds_downclock_avail;
	int			 lvds_downclock;
	bool			 busy;
	int			 child_dev_num;
	struct child_device_config *child_dev;

	struct mutex		 fence_lock;
	struct drm_i915_fence_reg fence_regs[16]; /* 965 */
	int			 fence_reg_start; /* 4 by default */
	int			 num_fence_regs; /* 8 pre-965, 16 post */

#define	INTELDRM_QUIET		0x01 /* suspend close, get off the hardware */
#define	INTELDRM_WEDGED		0x02 /* chipset hung pending reset */
#define	INTELDRM_SUSPENDED	0x04 /* in vt switch, no commands */
	int			 sc_flags; /* quiet, suspended, hung */
	/* number of ioctls + faults in flight */
	int			 entries;

	/* protects inactive, flushing, active and exec locks */
	struct mutex		 list_lock;

	/* protects access to request_list */
	struct mutex		 request_lock;

	enum intel_pch pch_type;

	struct drm_i915_display_funcs display;

	struct timeout idle_timeout;

	unsigned long quirks;

	/* Register state */
	bool modeset_on_lid;
	u8 saveLBB;
	u32 saveDSPACNTR;
	u32 saveDSPBCNTR;
	u32 saveDSPARB;
	u32 saveHWS;
	u32 savePIPEACONF;
	u32 savePIPEBCONF;
	u32 savePIPEASRC;
	u32 savePIPEBSRC;
	u32 saveFPA0;
	u32 saveFPA1;
	u32 saveDPLL_A;
	u32 saveDPLL_A_MD;
	u32 saveHTOTAL_A;
	u32 saveHBLANK_A;
	u32 saveHSYNC_A;
	u32 saveVTOTAL_A;
	u32 saveVBLANK_A;
	u32 saveVSYNC_A;
	u32 saveBCLRPAT_A;
	u32 saveTRANSACONF;
	u32 saveTRANS_HTOTAL_A;
	u32 saveTRANS_HBLANK_A;
	u32 saveTRANS_HSYNC_A;
	u32 saveTRANS_VTOTAL_A;
	u32 saveTRANS_VBLANK_A;
	u32 saveTRANS_VSYNC_A;
	u32 savePIPEASTAT;
	u32 saveDSPASTRIDE;
	u32 saveDSPASIZE;
	u32 saveDSPAPOS;
	u32 saveDSPAADDR;
	u32 saveDSPASURF;
	u32 saveDSPATILEOFF;
	u32 savePFIT_PGM_RATIOS;
	u32 saveBLC_HIST_CTL;
	u32 saveBLC_PWM_CTL;
	u32 saveBLC_PWM_CTL2;
	u32 saveBLC_CPU_PWM_CTL;
	u32 saveBLC_CPU_PWM_CTL2;
	u32 saveFPB0;
	u32 saveFPB1;
	u32 saveDPLL_B;
	u32 saveDPLL_B_MD;
	u32 saveHTOTAL_B;
	u32 saveHBLANK_B;
	u32 saveHSYNC_B;
	u32 saveVTOTAL_B;
	u32 saveVBLANK_B;
	u32 saveVSYNC_B;
	u32 saveBCLRPAT_B;
	u32 saveTRANSBCONF;
	u32 saveTRANS_HTOTAL_B;
	u32 saveTRANS_HBLANK_B;
	u32 saveTRANS_HSYNC_B;
	u32 saveTRANS_VTOTAL_B;
	u32 saveTRANS_VBLANK_B;
	u32 saveTRANS_VSYNC_B;
	u32 savePIPEBSTAT;
	u32 saveDSPBSTRIDE;
	u32 saveDSPBSIZE;
	u32 saveDSPBPOS;
	u32 saveDSPBADDR;
	u32 saveDSPBSURF;
	u32 saveDSPBTILEOFF;
	u32 saveVGA0;
	u32 saveVGA1;
	u32 saveVGA_PD;
	u32 saveVGACNTRL;
	u32 saveADPA;
	u32 saveLVDS;
	u32 savePP_ON_DELAYS;
	u32 savePP_OFF_DELAYS;
	u32 saveDVOA;
	u32 saveDVOB;
	u32 saveDVOC;
	u32 savePP_ON;
	u32 savePP_OFF;
	u32 savePP_CONTROL;
	u32 savePP_DIVISOR;
	u32 savePFIT_CONTROL;
	u32 save_palette_a[256];
	u32 save_palette_b[256];
	u32 saveDPFC_CB_BASE;
	u32 saveFBC_CFB_BASE;
	u32 saveFBC_LL_BASE;
	u32 saveFBC_CONTROL;
	u32 saveFBC_CONTROL2;
	u32 saveIER;
	u32 saveIIR;
	u32 saveIMR;
	u32 saveDEIER;
	u32 saveDEIMR;
	u32 saveGTIER;
	u32 saveGTIMR;
	u32 saveFDI_RXA_IMR;
	u32 saveFDI_RXB_IMR;
	u32 saveCACHE_MODE_0;
	u32 saveD_STATE;
	u32 saveDSPCLK_GATE_D;
	u32 saveDSPCLK_GATE;
	u32 saveRENCLK_GATE_D1;
	u32 saveRENCLK_GATE_D2;
	u32 saveRAMCLK_GATE_D;
	u32 saveDEUC;
	u32 saveMI_ARB_STATE;
	u32 saveSWF0[16];
	u32 saveSWF1[16];
	u32 saveSWF2[3];
	u8 saveMSR;
	u8 saveSR[8];
	u8 saveGR[25];
	u8 saveAR_INDEX;
	u8 saveAR[21];
	u8 saveDACMASK;
	u8 saveCR[37];
	uint64_t saveFENCE[16];
	u32 saveCURACNTR;
	u32 saveCURAPOS;
	u32 saveCURABASE;
	u32 saveCURBCNTR;
	u32 saveCURBPOS;
	u32 saveCURBBASE;
	u32 saveCURSIZE;
	u32 saveDP_B;
	u32 saveDP_C;
	u32 saveDP_D;
	u32 savePIPEA_GMCH_DATA_M;
	u32 savePIPEB_GMCH_DATA_M;
	u32 savePIPEA_GMCH_DATA_N;
	u32 savePIPEB_GMCH_DATA_N;
	u32 savePIPEA_DP_LINK_M;
	u32 savePIPEB_DP_LINK_M;
	u32 savePIPEA_DP_LINK_N;
	u32 savePIPEB_DP_LINK_N;
	u32 saveFDI_RXA_CTL;
	u32 saveFDI_TXA_CTL;
	u32 saveFDI_RXB_CTL;
	u32 saveFDI_TXB_CTL;
	u32 savePFA_CTL_1;
	u32 savePFB_CTL_1;
	u32 savePFA_WIN_SZ;
	u32 savePFB_WIN_SZ;
	u32 savePFA_WIN_POS;
	u32 savePFB_WIN_POS;
	u32 savePCH_DREF_CONTROL;
	u32 saveDISP_ARB_CTL;
	u32 savePIPEA_DATA_M1;
	u32 savePIPEA_DATA_N1;
	u32 savePIPEA_LINK_M1;
	u32 savePIPEA_LINK_N1;
	u32 savePIPEB_DATA_M1;
	u32 savePIPEB_DATA_N1;
	u32 savePIPEB_LINK_M1;
	u32 savePIPEB_LINK_N1;
	u32 saveMCHBAR_RENDER_STANDBY;
	u32 savePCH_PORT_HOTPLUG;

	struct {
		/**
		 * List of objects currently involved in rendering from the
		 * ringbuffer.
		 *
		 * Includes buffers having the contents of their GPU caches
		 * flushed, not necessarily primitives. last_rendering_seqno
		 * represents when the rendering involved will be completed.
		 *
		 * A reference is held on the buffer while on this list.
		 */
		TAILQ_HEAD(i915_gem_list, drm_i915_gem_object) active_list;

		/**
		 * List of objects which are not in the ringbuffer but which
		 * still have a write_domain which needs to be flushed before
		 * unbinding.
		 *
		 * last_rendering_seqno is 0 while an object is in this list
		 *
		 * A reference is held on the buffer while on this list.
		 */
		struct i915_gem_list flushing_list;

		/*
		 * list of objects currently pending a GPU write flush.
		 *
		 * All elements on this list will either be on the active
		 * or flushing list, last rendiering_seqno differentiates the
		 * two.
		 */
		struct i915_gem_list gpu_write_list;
		/**
		 * LRU list of objects which are not in the ringbuffer and
		 * are ready to unbind, but are still in the GTT.
		 *
		 * last_rendering_seqno is 0 while an object is in this list
		 *
		 * A reference is not held on the buffer while on this list,
		 * as merely being GTT-bound shouldn't prevent its being
		 * freed, and we'll pull it off the list in the free path.
		 */
		struct i915_gem_list inactive_list;

		/* Fence LRU */
		TAILQ_HEAD(i915_fence, drm_i915_fence_reg)	fence_list;

		/**
		 * List of breadcrumbs associated with GPU requests currently
		 * outstanding.
		 */
		TAILQ_HEAD(i915_request , drm_i915_gem_request) request_list;

		/**
		 * We leave the user IRQ off as much as possible,
		 * but this means that requests will finish and never
		 * be retired once the system goes idle. Set a timer to
		 * fire periodically while the ring is running. When it
		 * fires, go retire requests in a workq.
		 */
		struct timeout retire_timer;
		struct timeout hang_timer;
		/* for hangcheck */
		int		hang_cnt;
		u_int32_t	last_acthd;
		u_int32_t	last_instdone;
		u_int32_t	last_instdone1;

		uint32_t next_gem_seqno;

		/**
		 * Flag if the X Server, and thus DRM, is not currently in
		 * control of the device.
		 *
		 * This is set between LeaveVT and EnterVT.  It needs to be
		 * replaced with a semaphore.  It also needs to be
		 * transitioned away from for kernel modesetting.
		 */
		int suspended;

		/**
		 * Flag if the hardware appears to be wedged.
		 *
		 * This is set when attempts to idle the device timeout.
		 * It prevents command submission from occuring and makes
		 * every pending request fail
		 */
		int wedged;

		/** Bit 6 swizzling required for X tiling */
		uint32_t bit_6_swizzle_x;
		/** Bit 6 swizzling required for Y tiling */
		uint32_t bit_6_swizzle_y;

		/* storage for physical objects */
		struct drm_i915_gem_phys_object *phys_objs[I915_MAX_PHYS_OBJECT];
	} mm;

	const struct intel_device_info *info;

	struct sdvo_device_mapping sdvo_mappings[2];
	/* indicate whether the LVDS_BORDER should be enabled or not */
	unsigned int lvds_border_bits;
	/* Panel fitter placement and size for Ironlake+ */
	u32 pch_pf_pos, pch_pf_size;

	struct drm_crtc *plane_to_crtc_mapping[3];
	struct drm_crtc *pipe_to_crtc_mapping[3];

	bool flip_pending_is_done;

	struct drm_connector *int_lvds_connector;
	struct drm_connector *int_edp_connector;

	struct mutex rps_lock;
	u32 pm_iir;

	u8 cur_delay;
	u8 min_delay;
	u8 max_delay;
	u8 fmax;
	u8 fstart;

	u64 last_count1;
	unsigned long last_time1;
	unsigned long chipset_power;
	u64 last_count2;
//	struct timespec last_time2;
	unsigned long gfx_power;
	int c_m;
	int r_t;
	u8 corr;

	enum no_fbc_reason no_fbc_reason;

	unsigned long cfb_size;
	unsigned int cfb_fb;
	int cfb_plane;
	int cfb_y;
	struct intel_fbc_work *fbc_work;

	unsigned int fsb_freq, mem_freq, is_ddr3;

	struct intel_fbdev *fbdev;

	struct drm_property *broadcast_rgb_property;
	struct drm_property *force_audio_property;
};
typedef struct inteldrm_softc drm_i915_private_t;

/* Iterate over initialised rings */
#define for_each_ring(ring__, dev_priv__, i__) \
	for ((i__) = 0; (i__) < I915_NUM_RINGS; (i__)++) \
		if (((ring__) = &(dev_priv__)->rings[(i__)]), intel_ring_initialized((ring__)))

enum hdmi_force_audio {
	HDMI_AUDIO_OFF_DVI = -2,	/* no aux data for HDMI-DVI converter */
	HDMI_AUDIO_OFF,			/* force turn off HDMI audio */
	HDMI_AUDIO_AUTO,		/* trust EDID */
	HDMI_AUDIO_ON,			/* force turn on HDMI audio */
};

enum i915_cache_level {
	I915_CACHE_NONE,
	I915_CACHE_LLC,
	I915_CACHE_LLC_MLC, /* gen6+ */
};

struct inteldrm_file {
	struct drm_file	file_priv;
	struct {
	} mm;
};

/* chip type flags */
#define CHIP_I830	0x00001
#define CHIP_I845G	0x00002
#define CHIP_I85X	0x00004
#define CHIP_I865G	0x00008
#define CHIP_I9XX	0x00010
#define CHIP_I915G	0x00020
#define CHIP_I915GM	0x00040
#define CHIP_I945G	0x00080
#define CHIP_I945GM	0x00100
#define CHIP_I965	0x00200
#define CHIP_I965GM	0x00400
#define CHIP_G33	0x00800
#define CHIP_GM45	0x01000
#define CHIP_G4X	0x02000
#define CHIP_M		0x04000
#define CHIP_HWS	0x08000
#define CHIP_GEN2	0x10000
#define CHIP_GEN3	0x20000
#define CHIP_GEN4	0x40000
#define CHIP_GEN6	0x80000
#define	CHIP_PINEVIEW	0x100000
#define	CHIP_IRONLAKE	0x200000
#define CHIP_IRONLAKE_D	0x400000
#define CHIP_IRONLAKE_M	0x800000
#define CHIP_SANDYBRIDGE	0x1000000
#define CHIP_IVYBRIDGE	0x2000000
#define CHIP_GEN7	0x4000000

/* flags we use in drm_obj's do_flags */
#define I915_IN_EXEC		0x0020	/* being processed in execbuffer */
#define I915_USER_PINNED	0x0040	/* BO has been pinned from userland */
#define I915_GPU_WRITE		0x0080	/* BO has been not flushed */
#define I915_DONTNEED		0x0100	/* BO backing pages purgable */
#define I915_PURGED		0x0200	/* BO backing pages purged */
#define I915_EXEC_NEEDS_FENCE	0x0800	/* being processed but will need fence*/
#define I915_FENCED_EXEC	0x1000	/* Most recent exec needs fence */
#define I915_FENCE_INVALID	0x2000	/* fence has been lazily invalidated */

/** driver private structure attached to each drm_gem_object */
struct drm_i915_gem_object {
	struct drm_obj				 base;

	/** This object's place on the active/flushing/inactive lists */
	TAILQ_ENTRY(drm_i915_gem_object)	 list;
	TAILQ_ENTRY(drm_i915_gem_object)	 write_list;
	struct i915_gem_list			*current_list;
	/* GTT binding. */
	bus_dmamap_t				 dmamap;
	bus_dma_segment_t			*dma_segs;
	/* Current offset of the object in GTT space. */
	bus_addr_t				 gtt_offset;
	u_int32_t				*bit_17;
	/* extra flags to bus_dma */
	int					 dma_flags;
	/* Fence register for this object. needed for tiling. */
	int					 fence_reg;
	/** refcount for times pinned this object in GTT space */
	int					 pin_count;
	/* number of times pinned by pin ioctl. */
	u_int					 user_pin_count;

	/** Breadcrumb of last rendering to the buffer. */
	u_int32_t				 last_rendering_seqno;
	u_int32_t				 last_write_seqno;
	/** Current tiling mode for the object. */
	u_int32_t				 tiling_mode;
	u_int32_t				 stride;

	/**
	 * This is set if the object is on the active lists (has pending
	 * rendering and so a non-zero seqno), and is not set if it i s on
	 * inactive (ready to be unbound) list.
	 */
	unsigned int active:1;

	/**
	 * This is set if the object has been written to since last bound
	 * to the GTT
	 */
	unsigned int dirty:1;

	/** for phy allocated objects */
	struct drm_i915_gem_phys_object *phys_obj;

	/**
	 * Number of crtcs where this object is currently the fb, but   
	 * will be page flipped away on the next vblank.  When it
	 * reaches 0, dev_priv->pending_flip_queue will be woken up.
	 */
	int					 pending_flip;
};
#define to_gem_object(obj) (&((struct drm_i915_gem_object *)(obj))->base)

#define to_intel_bo(x) container_of(x,struct drm_i915_gem_object, base)

/**
 * Request queue structure.
 *
 * The request queue allows us to note sequence numbers that have been emitted
 * and may be associated with active buffers to be retired.
 *
 * By keeping this list, we can avoid having to do questionable
 * sequence-number comparisons on buffer last_rendering_seqnos, and associate
 * an emission time with seqnos for tracking how far ahead of the GPU we are.
 */
struct drm_i915_gem_request {
	TAILQ_ENTRY(drm_i915_gem_request)	list;
	/** GEM sequence number associated with this request. */
	uint32_t			seqno;
};

u_int32_t	inteldrm_read_hws(struct inteldrm_softc *, int);
int		ring_wait_for_space(struct intel_ring_buffer *, int n);
int		intel_ring_begin(struct intel_ring_buffer *, int);
void		intel_ring_emit(struct intel_ring_buffer *, u_int32_t);
void		intel_ring_advance(struct intel_ring_buffer *);
void		inteldrm_update_ring(struct intel_ring_buffer *);
int		inteldrm_pipe_enabled(struct inteldrm_softc *, int);
int		i915_init_phys_hws(struct inteldrm_softc *, bus_dma_tag_t);

void		i915_update_gfx_val(struct inteldrm_softc *);

int		intel_init_ring_buffer(struct drm_device *,
		    struct intel_ring_buffer *);
int		init_ring_common(struct intel_ring_buffer *);

int		intel_init_render_ring_buffer(struct drm_device *);
void		intel_cleanup_ring_buffer(struct intel_ring_buffer *);

static inline bool
intel_ring_initialized(struct intel_ring_buffer *ring)
{
	return ring->ring_obj != NULL;
}

/* i915_irq.c */

extern int i915_driver_irq_install(struct drm_device * dev);
extern void i915_driver_irq_uninstall(struct drm_device * dev);
extern int i915_enable_vblank(struct drm_device *dev, int crtc);
extern void i915_disable_vblank(struct drm_device *dev, int crtc);
extern u32 i915_get_vblank_counter(struct drm_device *dev, int crtc);
extern void i915_user_irq_get(struct inteldrm_softc *);
extern void i915_user_irq_put(struct inteldrm_softc *);
void	i915_enable_pipestat(struct inteldrm_softc *, int, u_int32_t);
void	i915_disable_pipestat(struct inteldrm_softc *, int, u_int32_t);

/* gem */
/* Ioctls */
int	inteldrm_getparam(struct inteldrm_softc *dev_priv, void *data);
int	inteldrm_setparam(struct inteldrm_softc *dev_priv, void *data);
int	i915_gem_init_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_create_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pread_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pwrite_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_set_domain_ioctl(struct drm_device *, void *,
	    struct drm_file *);
int	i915_gem_execbuffer2(struct drm_device *, void *, struct drm_file *);
int	i915_gem_pin_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_unpin_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_busy_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_entervt_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_leavevt_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_get_aperture_ioctl(struct drm_device *, void *,
	    struct drm_file *);
int	i915_gem_set_tiling(struct drm_device *, void *, struct drm_file *);
int	i915_gem_get_tiling(struct drm_device *, void *, struct drm_file *);
int	i915_gem_gtt_map_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_mmap_gtt_ioctl(struct drm_device *, void *, struct drm_file *);
int	i915_gem_madvise_ioctl(struct drm_device *, void *, struct drm_file *);

/* GEM memory manager functions */
int	i915_gem_init_object(struct drm_obj *);
void	i915_gem_free_object(struct drm_obj *);
int	i915_gem_object_pin(struct drm_i915_gem_object *, uint32_t, int);
void	i915_gem_object_unpin(struct drm_i915_gem_object *);
void	i915_gem_retire_request(struct inteldrm_softc *,
	    struct drm_i915_gem_request *);
void	i915_gem_retire_work_handler(void *, void*);
int	i915_gem_idle(struct inteldrm_softc *);
void	i915_gem_object_move_to_active(struct drm_i915_gem_object *);
void	i915_gem_object_move_off_active(struct drm_i915_gem_object *);
void	i915_gem_object_move_to_inactive(struct drm_i915_gem_object *);
void	i915_gem_object_move_to_inactive_locked(struct drm_i915_gem_object *);
uint32_t	i915_add_request(struct inteldrm_softc *);
void	inteldrm_process_flushing(struct inteldrm_softc *, u_int32_t);
void	i915_move_to_tail(struct drm_i915_gem_object *, struct i915_gem_list *);
void	i915_list_remove(struct drm_i915_gem_object *);
int	i915_gem_init_hws(struct inteldrm_softc *);
void	cleanup_status_page(struct intel_ring_buffer *);
void	i915_gem_cleanup_ringbuffer(struct drm_device *);
int	i915_gem_ring_throttle(struct drm_device *, struct drm_file *);
int	i915_gem_get_relocs_from_user(struct drm_i915_gem_exec_object2 *,
	    u_int32_t, struct drm_i915_gem_relocation_entry **);
int	i915_gem_put_relocs_to_user(struct drm_i915_gem_exec_object2 *,
	    u_int32_t, struct drm_i915_gem_relocation_entry *);
void	i915_dispatch_gem_execbuffer(struct drm_device *,
	    struct drm_i915_gem_execbuffer2 *, uint64_t);
void	i915_gem_object_set_to_gpu_domain(struct drm_obj *);
int	i915_gem_object_pin_and_relocate(struct drm_obj *,
	    struct drm_file *, struct drm_i915_gem_exec_object2 *,
	    struct drm_i915_gem_relocation_entry *);
int	i915_gem_object_bind_to_gtt(struct drm_i915_gem_object *,
	    bus_size_t, int);
u_int32_t	i915_gem_flush(struct inteldrm_softc *, uint32_t, uint32_t);

struct drm_obj	*i915_gem_find_inactive_object(struct inteldrm_softc *,
		     size_t);
int	i915_gem_object_get_fence(struct drm_obj *,
	    struct intel_ring_buffer *);

int	i915_gem_object_set_to_gtt_domain(struct drm_i915_gem_object *,
	    int, int);
int	i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *,
	    u32, struct intel_ring_buffer *);
int	i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *,
	    int, int);
int	i915_gem_object_flush_gpu_write_domain(struct drm_i915_gem_object *,
	    int, int, int);
int	i915_gem_get_fence_reg(struct drm_obj *, int);
int	i915_gem_object_wait_rendering(struct drm_i915_gem_object *obj);
int	i915_gem_object_put_fence_reg(struct drm_obj *, int);
bus_size_t	i915_gem_get_gtt_alignment(struct drm_obj *);

bus_size_t	i915_get_fence_size(struct inteldrm_softc *, bus_size_t);
int	i915_gem_init(struct drm_device *);
int	i915_gem_mmap_gtt(struct drm_file *, struct drm_device *,
	    uint32_t, uint64_t *);
int	i915_gem_object_set_cache_level(struct drm_i915_gem_object *obj,
	    enum i915_cache_level cache_level);

int	i915_tiling_ok(struct drm_device *, int, int, int);
int	i915_gem_object_fence_offset_ok(struct drm_obj *, int);
void	sandybridge_write_fence_reg(struct drm_i915_fence_reg *);
void	i965_write_fence_reg(struct drm_i915_fence_reg *);
void	i915_write_fence_reg(struct drm_i915_fence_reg *);
void	i830_write_fence_reg(struct drm_i915_fence_reg *);
void	i915_gem_bit_17_swizzle(struct drm_i915_gem_object *);
void	i915_gem_save_bit_17_swizzle(struct drm_i915_gem_object *);
int	inteldrm_swizzle_page(struct vm_page *page);

int	i915_gem_init_hw(struct drm_device *);

/* Debug functions, mostly called from ddb */
void	i915_gem_seqno_info(int);
void	i915_interrupt_info(int);
void	i915_gem_fence_regs_info(int);
void	i915_hws_info(int);
void	i915_batchbuffer_info(int);
void	i915_ringbuffer_data(int);
void	i915_ringbuffer_info(int);
#ifdef WATCH_INACTIVE
void inteldrm_verify_inactive(struct inteldrm_softc *, char *, int);
#else
#define inteldrm_verify_inactive(dev,file,line)
#endif

void i915_gem_retire_requests(struct inteldrm_softc *);
struct drm_obj  *i915_gem_find_inactive_object(struct inteldrm_softc *,
	size_t);
int i915_gem_object_unbind(struct drm_i915_gem_object *, int);
int i915_wait_request(struct inteldrm_softc *, uint32_t, int);
u_int32_t i915_gem_flush(struct inteldrm_softc *, uint32_t, uint32_t);
#define I915_GEM_GPU_DOMAINS	(~(I915_GEM_DOMAIN_CPU | I915_GEM_DOMAIN_GTT))

void i915_gem_detach_phys_object(struct drm_device *,
    struct drm_i915_gem_object *);
int i915_gem_attach_phys_object(struct drm_device *dev,
    struct drm_i915_gem_object *, int, int);

int i915_gem_dumb_create(struct drm_file *, struct drm_device *,
    struct drm_mode_create_dumb *);
int i915_gem_mmap_gtt(struct drm_file *, struct drm_device *,
    uint32_t, uint64_t *);
int i915_gem_dumb_destroy(struct drm_file *, struct drm_device *,
    uint32_t);

/* i915_drv.c */
void	inteldrm_wipe_mappings(struct drm_obj *);
void	inteldrm_set_max_obj_size(struct inteldrm_softc *);
void	inteldrm_purge_obj(struct drm_obj *);
void	inteldrm_chipset_flush(struct inteldrm_softc *);

/* i915_gem_evict.c */
int i915_gem_evict_everything(struct inteldrm_softc *, int);
int i915_gem_evict_something(struct inteldrm_softc *, size_t, int);
int i915_gem_evict_inactive(struct inteldrm_softc *, int);

/* i915_suspend.c */

extern void i915_save_display(struct drm_device *);
extern void i915_restore_display(struct drm_device *);
extern int i915_save_state(struct drm_device *);
extern int i915_restore_state(struct drm_device *);

/* intel_i2c.c */
extern int intel_setup_gmbus(struct inteldrm_softc *);
extern void intel_gmbus_set_port(struct inteldrm_softc *, int);

/* i915_gem.c */
int i915_gem_create(struct drm_file *, struct drm_device *, uint64_t,
    uint32_t *);

/* intel_opregion.c */
int intel_opregion_setup(struct drm_device *dev);
extern int intel_opregion_init(struct drm_device *dev);
extern void intel_opregion_fini(struct drm_device *dev);
extern void opregion_asle_intr(struct drm_device *dev);
extern void opregion_enable_asle(struct drm_device *dev);

/* i915_gem_gtt.c */
void i915_gem_cleanup_aliasing_ppgtt(struct drm_device *dev);

/* modesetting */
extern void intel_modeset_init(struct drm_device *dev);
extern void intel_modeset_gem_init(struct drm_device *dev);
extern void intel_modeset_cleanup(struct drm_device *dev);
extern int intel_modeset_vga_set_state(struct drm_device *dev, bool state);
extern bool intel_fbc_enabled(struct drm_device *dev);
extern void intel_disable_fbc(struct drm_device *dev);
extern bool ironlake_set_drps(struct drm_device *dev, u8 val);
extern void ironlake_init_pch_refclk(struct drm_device *dev);
extern void ironlake_enable_rc6(struct drm_device *dev);
extern void gen6_set_rps(struct drm_device *dev, u8 val);
extern void intel_detect_pch(struct inteldrm_softc *dev_priv);
extern int intel_trans_dp_port_sel(struct drm_crtc *crtc);
int i915_load_modeset_init(struct drm_device *dev);

extern void __gen6_gt_force_wake_get(struct inteldrm_softc *dev_priv);
extern void __gen6_gt_force_wake_mt_get(struct inteldrm_softc *dev_priv);
extern void __gen6_gt_force_wake_put(struct inteldrm_softc *dev_priv);
extern void __gen6_gt_force_wake_mt_put(struct inteldrm_softc *dev_priv);

extern struct intel_overlay_error_state *intel_overlay_capture_error_state(
    struct drm_device *dev);
#ifdef notyet
extern void intel_overlay_print_error_state(struct sbuf *m,
    struct intel_overlay_error_state *error);
#endif
extern struct intel_display_error_state *intel_display_capture_error_state(
    struct drm_device *dev);
#ifdef notyet
extern void intel_display_print_error_state(struct sbuf *m,
    struct drm_device *dev, struct intel_display_error_state *error);
#endif

/* On SNB platform, before reading ring registers forcewake bit
 * must be set to prevent GT core from power down and stale values being
 * returned.
 */
void gen6_gt_force_wake_get(struct inteldrm_softc *dev_priv);
void gen6_gt_force_wake_put(struct inteldrm_softc *dev_priv);
int __gen6_gt_wait_for_fifo(struct inteldrm_softc *dev_priv);

/* XXX need bus_space_write_8, this evaluated arguments twice */
static __inline void
write64(struct inteldrm_softc *dev_priv, bus_size_t off, u_int64_t reg)
{
	bus_space_write_4(dev_priv->regs->bst, dev_priv->regs->bsh,
	    off, (u_int32_t)reg);
	bus_space_write_4(dev_priv->regs->bst, dev_priv->regs->bsh,
	    off + 4, upper_32_bits(reg));
}

static __inline u_int64_t
read64(struct inteldrm_softc *dev_priv, bus_size_t off)
{
	u_int32_t low, high;

	low = bus_space_read_4(dev_priv->regs->bst,
	    dev_priv->regs->bsh, off);
	high = bus_space_read_4(dev_priv->regs->bst,
	    dev_priv->regs->bsh, off + 4);

	return ((u_int64_t)low | ((u_int64_t)high << 32));
}

#define I915_READ64(off)	read64(dev_priv, off)

#define I915_WRITE64(off, reg)	write64(dev_priv, off, reg)

#define I915_READ(reg)		bus_space_read_4(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg))
#define I915_READ_NOTRACE(reg)	I915_READ(reg)
#define I915_WRITE(reg,val)	bus_space_write_4(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg), (val))
#define I915_WRITE_NOTRACE(reg,val)	I915_WRITE(reg, val)
#define I915_READ16(reg)	bus_space_read_2(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg))
#define I915_WRITE16(reg,val)	bus_space_write_2(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg), (val))
#define I915_READ8(reg)		bus_space_read_1(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg))
#define I915_WRITE8(reg,val)	bus_space_write_1(dev_priv->regs->bst,	\
				    dev_priv->regs->bsh, (reg), (val))

#define POSTING_READ(reg)	(void)I915_READ(reg)
#define POSTING_READ16(reg)	(void)I915_READ16(reg)

#define INTELDRM_VERBOSE 0
#if INTELDRM_VERBOSE > 0
#define	INTELDRM_VPRINTF(fmt, args...) DRM_INFO(fmt, ##args)
#else
#define	INTELDRM_VPRINTF(fmt, args...)
#endif

#define LP_RING(d) (&((struct inteldrm_softc *)(d))->rings[RCS])

#define BEGIN_LP_RING(n) \
	intel_ring_begin(LP_RING(dev_priv), (n))

#define OUT_RING(x) \
	intel_ring_emit(LP_RING(dev_priv), (x))

#define ADVANCE_LP_RING() \
	intel_ring_advance(LP_RING(dev_priv))

/* MCH IFP BARs */
#define	I915_IFPADDR	0x60
#define I965_IFPADDR	0x70

/**
 * Reads a dword out of the status page, which is written to from the command
 * queue by automatic updates, MI_REPORT_HEAD, MI_STORE_DATA_INDEX, or
 * MI_STORE_DATA_IMM.
 *
 * The following dwords have a reserved meaning:
 * 0x00: ISR copy, updated when an ISR bit not set in the HWSTAM changes.
 * 0x04: ring 0 head pointer
 * 0x05: ring 1 head pointer (915-class)
 * 0x06: ring 2 head pointer (915-class)
 * 0x10-0x1b: Context status DWords (GM45)
 * 0x1f: Last written status offset. (GM45)
 *
 * The area from dword 0x20 to 0x3ff is available for driver usage.
 */
#define READ_HWSP(dev_priv, reg)  inteldrm_read_hws(dev_priv, reg)
#define I915_GEM_HWS_INDEX		0x20

#define INTEL_INFO(dev) (((struct inteldrm_softc *) (dev)->dev_private)->info)

/* Chipset type macros */

#define IS_I830(dev)		((dev)->pci_device == 0x3577)
#define IS_845G(dev)		((dev)->pci_device == 0x2562)
#define IS_I85X(dev)		(INTEL_INFO(dev)->is_i85x)
#define IS_I865G(dev)		((dev)->pci_device == 0x2572)
#define IS_I915G(dev)		(INTEL_INFO(dev)->is_i915g)
#define IS_I915GM(dev)		((dev)->pci_device == 0x2592)
#define IS_I945G(dev)		((dev)->pci_device == 0x2772)
#define IS_I945GM(dev)		(INTEL_INFO(dev)->is_i945gm)
#define IS_BROADWATER(dev)	(INTEL_INFO(dev)->is_broadwater)
#define IS_CRESTLINE(dev)	(INTEL_INFO(dev)->is_crestline)
#define IS_GM45(dev)		((dev)->pci_device == 0x2A42)
#define IS_G4X(dev)		(INTEL_INFO(dev)->is_g4x)
#define IS_PINEVIEW_G(dev)	((dev)->pci_device == 0xa001)
#define IS_PINEVIEW_M(dev)	((dev)->pci_device == 0xa011)
#define IS_PINEVIEW(dev)	(INTEL_INFO(dev)->is_pineview)
#define IS_G33(dev)		(INTEL_INFO(dev)->is_g33)
#define IS_IRONLAKE_D(dev)	((dev)->pci_device == 0x0042)
#define IS_IRONLAKE_M(dev)	((dev)->pci_device == 0x0046)
#define IS_IVYBRIDGE(dev)	(INTEL_INFO(dev)->is_ivybridge)
#define IS_MOBILE(dev)		(INTEL_INFO(dev)->is_mobile)

#define IS_I9XX(dev)		(INTEL_INFO(dev)->gen >= 3)
#define IS_IRONLAKE(dev)	(INTEL_INFO(dev)->gen == 5)

#define IS_SANDYBRIDGE(dev)	(INTEL_INFO(dev)->gen == 6)
#define IS_SANDYBRIDGE_D(dev)	(IS_SANDYBRIDGE(dev) && \
 (INTEL_INFO(dev)->is_mobile == 0))
#define IS_SANDYBRIDGE_M(dev)	(IS_SANDYBRIDGE(dev) && \
 (INTEL_INFO(dev)->is_mobile == 1))
#define IS_VALLEYVIEW(dev)	(INTEL_INFO(dev)->is_valleyview)
#define IS_HASWELL(dev)	(INTEL_INFO(dev)->is_haswell)

/*
 * The genX designation typically refers to the render engine, so render
 * capability related checks should use IS_GEN, while display and other checks
 * have their own (e.g. HAS_PCH_SPLIT for ILK+ display, IS_foo for particular
 * chips, etc.).
 */
#define IS_GEN2(dev)		(INTEL_INFO(dev)->gen == 2)
#define IS_GEN3(dev)		(INTEL_INFO(dev)->gen == 3)
#define IS_GEN4(dev)		(INTEL_INFO(dev)->gen == 4)
#define IS_GEN5(dev)		(INTEL_INFO(dev)->gen == 5)
#define IS_GEN6(dev)		(INTEL_INFO(dev)->gen == 6)
#define IS_GEN7(dev)		(INTEL_INFO(dev)->gen == 7)

#define HAS_BSD(dev)		(INTEL_INFO(dev)->has_bsd_ring)
#define HAS_BLT(dev)		(INTEL_INFO(dev)->has_blt_ring)
#define HAS_LLC(dev)		(INTEL_INFO(dev)->has_llc)
#define I915_NEED_GFX_HWS(dev)	(INTEL_INFO(dev)->need_gfx_hws)

#define HAS_HW_CONTEXTS(dev)	(INTEL_INFO(dev)->gen >= 6)
#define HAS_ALIASING_PPGTT(dev)	(INTEL_INFO(dev)->gen >= 6)

#define HAS_OVERLAY(dev)	(INTEL_INFO(dev)->has_overlay)
#define OVERLAY_NEEDS_PHYSICAL(dev) \
    (INTEL_INFO(dev)->overlay_needs_physical)

/* Early gen2 have a totally busted CS tlb and require pinned batches. */
#define HAS_BROKEN_CS_TLB(dev)	(IS_I830(dev) || IS_845G(dev))

/*
 * With the 945 and later, Y tiling got adjusted so that it was 32 128-byte
 * rows, which changes the alignment requirements and fence programming.
 */
#define HAS_128_BYTE_Y_TILING(dev) (IS_I9XX(dev) &&	\
	!(IS_I915G(dev) || IS_I915GM(dev)))

#define HAS_RESET(dev)	(INTEL_INFO(dev)->gen >= 4 && \
    (!IS_GEN6(dev)) && (!IS_GEN7(dev)))

#define SUPPORTS_DIGITAL_OUTPUTS(dev)	(!IS_GEN2(dev) && !IS_PINEVIEW(dev))
#define SUPPORTS_INTEGRATED_HDMI(dev)	(IS_G4X(dev) || IS_GEN5(dev))
#define SUPPORTS_INTEGRATED_DP(dev)	(IS_G4X(dev) || IS_GEN5(dev))
#define SUPPORTS_EDP(dev)		(IS_IRONLAKE_M(dev))
#define SUPPORTS_TV(dev)		(INTEL_INFO(dev)->supports_tv)
#define I915_HAS_HOTPLUG(dev)		(INTEL_INFO(dev)->has_hotplug)

#define HAS_FW_BLC(dev)		(INTEL_INFO(dev)->gen > 2)
#define HAS_PIPE_CXSR(dev)	(INTEL_INFO(dev)->has_pipe_cxsr)
#define I915_HAS_FBC(dev)	(INTEL_INFO(dev)->has_fbc)

#define HAS_PIPE_CONTROL(dev)	(INTEL_INFO(dev)->gen >= 5)

#define HAS_PCH_SPLIT(dev)	(IS_IRONLAKE(dev) || IS_GEN6(dev) || \
    IS_GEN7(dev))

#define INTEL_PCH_TYPE(dev)	(dev->pch_type)
#define HAS_PCH_CPT(dev)	(INTEL_PCH_TYPE(dev) == PCH_CPT)
#define HAS_PCH_IBX(dev)	(INTEL_PCH_TYPE(dev) == PCH_IBX)

#define HAS_FORCE_WAKE(dev) (INTEL_INFO(dev)->has_force_wake)

#define HAS_L3_GPU_CACHE(dev) (IS_IVYBRIDGE(dev) || IS_HASWELL(dev))

/*
 * Interrupts that are always left unmasked.
 *
 * Since pipe events are edge-triggered from the PIPESTAT register to IIRC,
 * we leave them always unmasked in IMR and then control enabling them through
 * PIPESTAT alone.
 */
#define I915_INTERRUPT_ENABLE_FIX		\
	(I915_DISPLAY_PIPE_A_EVENT_INTERRUPT |	\
	I915_DISPLAY_PIPE_B_EVENT_INTERRUPT |	\
	I915_RENDER_COMMAND_PARSER_ERROR_INTERRUPT)

/* Interrupts that we mask and unmask at runtime */
#define I915_INTERRUPT_ENABLE_VAR	(I915_USER_INTERRUPT)

/* These are all of the interrupts used by the driver */
#define I915_INTERRUPT_ENABLE_MASK	\
	(I915_INTERRUPT_ENABLE_FIX |	\
	I915_INTERRUPT_ENABLE_VAR)

/*
 * if kms we want pch event, gse, and plane flip masks too
 */
#define PCH_SPLIT_DISPLAY_INTR_FIX	(DE_MASTER_IRQ_CONTROL)
#define PCH_SPLIT_DISPLAY_INTR_VAR	(DE_PIPEA_VBLANK | DE_PIPEB_VBLANK)
#define PCH_SPLIT_DISPLAY_ENABLE_MASK	\
	(PCH_SPLIT_DISPLAY_INTR_FIX | PCH_SPLIT_DISPLAY_INTR_VAR)
#define PCH_SPLIT_RENDER_INTR_FIX	(0)
#define PCH_SPLIT_RENDER_INTR_VAR	(GT_USER_INTERRUPT | GT_RENDER_CS_ERROR_INTERRUPT)
#define PCH_SPLIT_RENDER_ENABLE_MASK	\
	(PCH_SPLIT_RENDER_INTR_FIX | PCH_SPLIT_RENDER_INTR_VAR)
/* not yet */
#define PCH_SPLIT_HOTPLUG_INTR_FIX	(0)
#define PCH_SPLIT_HOTPLUG_INTR_VAR	(0)
#define PCH_SPLIT_HOTPLUG_ENABLE_MASK	\
	(PCH_SPLIT_HOTPLUG_INTR_FIX | PCH_SPLIT_HOTPLUG_INTR_VAR)

#define	printeir(val)	printf("%s: error reg: %b\n", __func__, val,	\
	"\20\x10PTEERR\x2REFRESHERR\x1INSTERR")

#define PRIMARY_RINGBUFFER_SIZE         (128*1024)

/* Inlines */

/**
 * Returns true if seq1 is later than seq2.
 */
static __inline int
i915_seqno_passed(uint32_t seq1, uint32_t seq2)
{
	return ((int32_t)(seq1 - seq2) >= 0);
}

/*
 * Read seqence number from the Hardware status page.
 */
static __inline u_int32_t
i915_get_gem_seqno(struct inteldrm_softc *dev_priv)
{
	return (READ_HWSP(dev_priv, I915_GEM_HWS_INDEX));
}

static __inline int
i915_obj_purgeable(struct drm_i915_gem_object *obj_priv)
{
	return (obj_priv->base.do_flags & I915_DONTNEED);
}

static __inline int
i915_obj_purged(struct drm_i915_gem_object *obj_priv)
{
	return (obj_priv->base.do_flags & I915_PURGED);
}

static __inline int
inteldrm_exec_needs_fence(struct drm_i915_gem_object *obj_priv)
{
	return (obj_priv->base.do_flags & I915_EXEC_NEEDS_FENCE);
}

static __inline int
inteldrm_needs_fence(struct drm_i915_gem_object *obj_priv)
{
	return (obj_priv->base.do_flags & I915_FENCED_EXEC);
}

static inline void
i915_gem_object_pin_fence(struct drm_i915_gem_object *obj)
{
	i915_gem_object_pin(obj, 0, 1);
}

static inline void
i915_gem_object_unpin_fence(struct drm_i915_gem_object *obj)
{
	i915_gem_object_unpin(obj);
}

#endif
