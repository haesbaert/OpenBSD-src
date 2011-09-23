/*
 * Copyright (c) 2006-2008 Intel Corporation
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 *
 * DRM core CRTC related functions
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *      Keith Packard
 *	Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */

#include "drmP.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

boolean_t drm_kms_helper_poll = TRUE;

void drm_mode_validate_flag(struct drm_connector *, int);
void drm_encoder_disable(struct drm_encoder *);
void drm_calc_timestamping_constants(struct drm_crtc *);
boolean_t drm_encoder_crtc_ok(struct drm_encoder *, struct drm_crtc *);
void drm_crtc_prepare_encoders(struct drm_device *);
int drm_helper_choose_encoder_dpms(struct drm_encoder *);
int drm_helper_choose_crtc_dpms(struct drm_crtc *);

void 
drm_mode_validate_flag(struct drm_connector *connector,
    int flags)
{
	struct drm_display_mode *mode, *t;

	if (flags == (DRM_MODE_FLAG_DBLSCAN | DRM_MODE_FLAG_INTERLACE))
		return;

	TAILQ_FOREACH_SAFE(mode, &connector->modes, head, t) {
		if ((mode->flags & DRM_MODE_FLAG_INTERLACE) &&
				!(flags & DRM_MODE_FLAG_INTERLACE))
			mode->status = MODE_NO_INTERLACE;
		if ((mode->flags & DRM_MODE_FLAG_DBLSCAN) &&
				!(flags & DRM_MODE_FLAG_DBLSCAN))
			mode->status = MODE_NO_DBLESCAN;
	}

	return;
}

/**
 * drm_helper_probe_single_connector_modes - get complete set of display modes
 * @dev: DRM device
 * @maxX: max width for modes
 * @maxY: max height for modes
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Based on @dev's mode_config layout, scan all the connectors and try to detect
 * modes on them.  Modes will first be added to the connector's probed_modes
 * list, then culled (based on validity and the @maxX, @maxY parameters) and
 * put into the normal modes list.
 *
 * Intended to be used either at bootup time or when major configuration
 * changes have occurred.
 *
 * FIXME: take into account monitor limits
 *
 * RETURNS:
 * Number of modes found on @connector.
 */
int 
drm_helper_probe_single_connector_modes(struct drm_connector *connector,
    uint32_t maxX, uint32_t maxY)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode, *t;
	struct drm_connector_helper_funcs *connector_funcs =
		connector->helper_private;
	int count = 0;
	int mode_flags = 0;

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s]\n", connector->base.id,
			drm_get_connector_name(connector));
	/* set all modes to the unverified state */
	TAILQ_FOREACH_SAFE(mode, &connector->modes, head, t)
		mode->status = MODE_UNVERIFIED;

	if (connector->force) {
		if (connector->force == DRM_FORCE_ON)
			connector->status = connector_status_connected;
		else
			connector->status = connector_status_disconnected;
		if (connector->funcs->force)
			connector->funcs->force(connector);
	} else {
		connector->status = connector->funcs->detect(connector, TRUE);
		drm_kms_helper_poll_enable(dev);
	}

	if (connector->status == connector_status_disconnected) {
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] disconnected\n",
			connector->base.id, drm_get_connector_name(connector));
		drm_mode_connector_update_edid_property(connector, NULL);
		goto prune;
	}

	count = (*connector_funcs->get_modes)(connector);
	if (count == 0 && connector->status == connector_status_connected)
		count = drm_add_modes_noedid(connector, 1024, 768);
	if (count == 0)
		goto prune;

	drm_mode_connector_list_update(connector);

	if (maxX && maxY)
		drm_mode_validate_size(dev, &connector->modes, maxX,
				       maxY, 0);

	if (connector->interlace_allowed)
		mode_flags |= DRM_MODE_FLAG_INTERLACE;
	if (connector->doublescan_allowed)
		mode_flags |= DRM_MODE_FLAG_DBLSCAN;
	drm_mode_validate_flag(connector, mode_flags);

	TAILQ_FOREACH_SAFE(mode, &connector->modes, head, t) {
		if (mode->status == MODE_OK)
			mode->status = connector_funcs->mode_valid(connector,
								   mode);
	}

prune:
	drm_mode_prune_invalid(dev, &connector->modes, TRUE);

	if (TAILQ_EMPTY(&connector->modes))
		return 0;

	drm_mode_sort(&connector->modes);

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s] probed modes :\n", connector->base.id,
			drm_get_connector_name(connector));
	TAILQ_FOREACH_SAFE(mode, &connector->modes, head, t) {
		mode->vrefresh = drm_mode_vrefresh(mode);

		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
		drm_mode_debug_printmodeline(mode);
	}

	return count;
}

/**
 * drm_helper_encoder_in_use - check if a given encoder is in use
 * @encoder: encoder to check
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Walk @encoders's DRM device's mode_config and see if it's in use.
 *
 * RETURNS:
 * TRUE if @encoder is part of the mode_config, FALSE otherwise.
 */
boolean_t 
drm_helper_encoder_in_use(struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;
	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder == encoder)
			return TRUE;
	return FALSE;
}

/**
 * drm_helper_crtc_in_use - check if a given CRTC is in a mode_config
 * @crtc: CRTC to check
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Walk @crtc's DRM device's mode_config and see if it's in use.
 *
 * RETURNS:
 * TRUE if @crtc is part of the mode_config, FALSE otherwise.
 */
boolean_t 
drm_helper_crtc_in_use(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev = crtc->dev;
	/* FIXME: Locking around list access? */
	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head)
		if (encoder->crtc == crtc && drm_helper_encoder_in_use(encoder))
			return TRUE;
	return FALSE;
}

void
drm_encoder_disable(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;

	if (encoder_funcs->disable)
		(*encoder_funcs->disable)(encoder);
	else
		(*encoder_funcs->dpms)(encoder, DRM_MODE_DPMS_OFF);
}

/**
 * drm_helper_disable_unused_functions - disable unused objects
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * If an connector or CRTC isn't part of @dev's mode_config, it can be disabled
 * by calling its dpms function, which should power it off.
 */
void 
drm_helper_disable_unused_functions(struct drm_device *dev)
{
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct drm_crtc *crtc;

	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head) {
		if (!connector->encoder)
			continue;
		if (connector->status == connector_status_disconnected)
			connector->encoder = NULL;
	}

	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {
		if (!drm_helper_encoder_in_use(encoder)) {
			drm_encoder_disable(encoder);
			/* disconnector encoder from any connector */
			encoder->crtc = NULL;
		}
	}

	TAILQ_FOREACH(crtc, &dev->mode_config.crtc_list, head) {
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
		crtc->enabled = drm_helper_crtc_in_use(crtc);
		if (!crtc->enabled) {
			if (crtc_funcs->disable)
				(*crtc_funcs->disable)(crtc);
			else
				(*crtc_funcs->dpms)(crtc, DRM_MODE_DPMS_OFF);
			crtc->fb = NULL;
		}
	}
}

/**
 * drm_encoder_crtc_ok - can a given crtc drive a given encoder?
 * @encoder: encoder to test
 * @crtc: crtc to test
 *
 * Return FALSE if @encoder can't be driven by @crtc, TRUE otherwise.
 */
 
boolean_t 
drm_encoder_crtc_ok(struct drm_encoder *encoder,
				struct drm_crtc *crtc)
{
	struct drm_device *dev;
	struct drm_crtc *tmp;
	int crtc_mask = 1;

	if (crtc == NULL)
		printf("checking null crtc?\n");

	dev = crtc->dev;

	TAILQ_FOREACH(tmp, &dev->mode_config.crtc_list, head) {
		if (tmp == crtc)
			break;
		crtc_mask <<= 1;
	}

	if (encoder->possible_crtcs & crtc_mask)
		return TRUE;
	return FALSE;
}

/*
 * Check the CRTC we're going to map each output to vs. its current
 * CRTC.  If they don't match, we have to disable the output and the CRTC
 * since the driver will have to re-route things.
 */
void
drm_crtc_prepare_encoders(struct drm_device *dev)
{
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_encoder *encoder;

	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {
		encoder_funcs = encoder->helper_private;
		/* Disable unused encoders */
		if (encoder->crtc == NULL)
			drm_encoder_disable(encoder);
		/* Disable encoders whose CRTC is about to change */
		if (encoder_funcs->get_crtc &&
		    encoder->crtc != (*encoder_funcs->get_crtc)(encoder))
			drm_encoder_disable(encoder);
	}
}

/**
 * drm_crtc_set_mode - set a mode
 * @crtc: CRTC to program
 * @mode: mode to use
 * @x: width of mode
 * @y: height of mode
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Try to set @mode on @crtc.  Give @crtc and its associated connectors a chance
 * to fixup or reject the mode prior to trying to set it.
 *
 * RETURNS:
 * TRUE if the mode was set successfully, or FALSE otherwise.
 */
boolean_t 
drm_crtc_helper_set_mode(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      int x, int y,
			      struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	struct drm_display_mode *adjusted_mode, saved_mode, saved_hwmode;
	struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
	struct drm_encoder_helper_funcs *encoder_funcs;
	int saved_x, saved_y;
	struct drm_encoder *encoder;
	boolean_t ret = TRUE;

	crtc->enabled = drm_helper_crtc_in_use(crtc);
	if (!crtc->enabled)
		return TRUE;

	adjusted_mode = drm_mode_duplicate(dev, mode);

	saved_hwmode = crtc->hwmode;
	saved_mode = crtc->mode;
	saved_x = crtc->x;
	saved_y = crtc->y;

	/* Update crtc values up front so the driver can rely on them for mode
	 * setting.
	 */
	crtc->mode = *mode;
	crtc->x = x;
	crtc->y = y;

	/* Pass our mode to the connectors and the CRTC to give them a chance to
	 * adjust it according to limitations or connector properties, and also
	 * a chance to reject the mode entirely.
	 */
	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;
		encoder_funcs = encoder->helper_private;
		if (!(ret = encoder_funcs->mode_fixup(encoder, mode,
						      adjusted_mode))) {
			goto done;
		}
	}

	if (!(ret = crtc_funcs->mode_fixup(crtc, mode, adjusted_mode))) {
		goto done;
	}
	DRM_DEBUG_KMS("[CRTC:%d]\n", crtc->base.id);

	/* Prepare the encoders and CRTCs before setting the mode. */
	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;
		encoder_funcs = encoder->helper_private;
		/* Disable the encoders as the first thing we do. */
		encoder_funcs->prepare(encoder);
	}

	drm_crtc_prepare_encoders(dev);

	crtc_funcs->prepare(crtc);

	/* Set up the DPLL and any encoders state that needs to adjust or depend
	 * on the DPLL.
	 */
	ret = !crtc_funcs->mode_set(crtc, mode, adjusted_mode, x, y, old_fb);
	if (!ret)
	    goto done;

	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;

		DRM_DEBUG_KMS("[ENCODER:%d:%s] set [MODE:%d:%s]\n",
			encoder->base.id, drm_get_encoder_name(encoder),
			mode->base.id, mode->name);
		encoder_funcs = encoder->helper_private;
		encoder_funcs->mode_set(encoder, mode, adjusted_mode);
	}

	/* Now enable the clocks, plane, pipe, and connectors that we set up. */
	crtc_funcs->commit(crtc);

	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;

		encoder_funcs = encoder->helper_private;
		encoder_funcs->commit(encoder);

	}

	/* Store real post-adjustment hardware mode. */
	crtc->hwmode = *adjusted_mode;

	/* Calculate and store various constants which
	 * are later needed by vblank and swap-completion
	 * timestamping. They are derived from TRUE hwmode.
	 */
	drm_calc_timestamping_constants(crtc);

	/* FIXME: add subpixel order */
done:
	drm_mode_destroy(dev, adjusted_mode);
	if (!ret) {
		crtc->hwmode = saved_hwmode;
		crtc->mode = saved_mode;
		crtc->x = saved_x;
		crtc->y = saved_y;
	}

	return ret;
}


/**
 * drm_crtc_helper_set_config - set a new config from userspace
 * @crtc: CRTC to setup
 * @crtc_info: user provided configuration
 * @new_mode: new mode to set
 * @connector_set: set of connectors for the new config
 * @fb: new framebuffer
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Setup a new configuration, provided by the user in @crtc_info, and enable
 * it.
 *
 * RETURNS:
 * Zero. (FIXME)
 */
int 
drm_crtc_helper_set_config(struct drm_mode_set *set)
{
	struct drm_device *dev;
	struct drm_crtc *save_crtcs, *new_crtc, *crtc;
	struct drm_encoder *save_encoders, *new_encoder, *encoder;
	struct drm_framebuffer *old_fb = NULL;
	boolean_t mode_changed = FALSE; /* if TRUE do a full mode set */
	boolean_t fb_changed = FALSE; /* if TRUE and !mode_changed just do a flip */
	struct drm_connector *save_connectors, *connector;
	int count = 0, ro, fail = 0;
	struct drm_crtc_helper_funcs *crtc_funcs;
	int ret = 0;
	int i;

	DRM_DEBUG_KMS("\n");

	if (!set)
		return EINVAL;

	if (!set->crtc)
		return EINVAL;

	if (!set->crtc->helper_private)
		return EINVAL;

	crtc_funcs = set->crtc->helper_private;

	if (!set->mode)
		set->fb = NULL;

	if (set->fb) {
		DRM_DEBUG_KMS("[CRTC:%d] [FB:%d] #connectors=%d (x y) (%i %i)\n",
				set->crtc->base.id, set->fb->base.id,
				(int)set->num_connectors, set->x, set->y);
	} else {
		DRM_DEBUG_KMS("[CRTC:%d] [NOFB]\n", set->crtc->base.id);
		set->mode = NULL;
		set->num_connectors = 0;
	}

	dev = set->crtc->dev;

	/* Allocate space for the backup of all (non-pointer) crtc, encoder and
	 * connector data. */
	save_crtcs = malloc(dev->mode_config.num_crtc *
	    sizeof(struct drm_crtc), M_DRM, M_NOWAIT|M_ZERO);
	if (!save_crtcs)
		return ENOMEM;

	save_encoders = malloc(dev->mode_config.num_encoder *
	    sizeof(struct drm_encoder), M_DRM, M_NOWAIT|M_ZERO);
	if (!save_encoders) {
		free(save_crtcs, M_DRM);
		return ENOMEM;
	}

	save_connectors = malloc(dev->mode_config.num_connector *
	    sizeof(struct drm_connector), M_DRM, M_NOWAIT|M_ZERO);
	if (!save_connectors) {
		free(save_crtcs, M_DRM);
		free(save_encoders, M_DRM);
		return ENOMEM;
	}

	/* Copy data. Note that driver private data is not affected.
	 * Should anything bad happen only the expected state is
	 * restored, not the drivers personal bookkeeping.
	 */
	count = 0;
	TAILQ_FOREACH(crtc, &dev->mode_config.crtc_list, head) {
		save_crtcs[count++] = *crtc;
	}

	count = 0;
	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {
		save_encoders[count++] = *encoder;
	}

	count = 0;
	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head) {
		save_connectors[count++] = *connector;
	}

	/* We should be able to check here if the fb has the same properties
	 * and then just flip_or_move it */
	if (set->crtc->fb != set->fb) {
		/* If we have no fb then treat it as a full mode set */
		if (set->crtc->fb == NULL) {
			DRM_DEBUG_KMS("crtc has no fb, full mode set\n");
			mode_changed = TRUE;
		} else if (set->fb == NULL) {
			mode_changed = TRUE;
		} else if (set->fb->depth != set->crtc->fb->depth) {
			mode_changed = TRUE;
		} else if (set->fb->bits_per_pixel !=
			   set->crtc->fb->bits_per_pixel) {
			mode_changed = TRUE;
		} else
			fb_changed = TRUE;
	}

	if (set->x != set->crtc->x || set->y != set->crtc->y)
		fb_changed = TRUE;

	if (set->mode && !drm_mode_equal(set->mode, &set->crtc->mode)) {
		DRM_DEBUG_KMS("modes are different, full mode set\n");
		drm_mode_debug_printmodeline(&set->crtc->mode);
		drm_mode_debug_printmodeline(set->mode);
		mode_changed = TRUE;
	}

	/* a) traverse passed in connector list and get encoders for them */
	count = 0;
	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head) {
		struct drm_connector_helper_funcs *connector_funcs =
			connector->helper_private;
		new_encoder = connector->encoder;
		for (ro = 0; ro < set->num_connectors; ro++) {
			if (set->connectors[ro] == connector) {
				new_encoder = connector_funcs->best_encoder(connector);
				/* if we can't get an encoder for a connector
				   we are setting now - then fail */
				if (new_encoder == NULL)
					/* don't break so fail path works correct */
					fail = 1;
				break;
			}
		}

		if (new_encoder != connector->encoder) {
			DRM_DEBUG_KMS("encoder changed, full mode switch\n");
			mode_changed = TRUE;
			/* If the encoder is reused for another connector, then
			 * the appropriate crtc will be set later.
			 */
			if (connector->encoder)
				connector->encoder->crtc = NULL;
			connector->encoder = new_encoder;
		}
	}

	if (fail) {
		ret = EINVAL;
		goto fail;
	}

	count = 0;
	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head) {
		if (!connector->encoder)
			continue;

		if (connector->encoder->crtc == set->crtc)
			new_crtc = NULL;
		else
			new_crtc = connector->encoder->crtc;

		for (ro = 0; ro < set->num_connectors; ro++) {
			if (set->connectors[ro] == connector)
				new_crtc = set->crtc;
		}

		/* Make sure the new CRTC will work with the encoder */
		if (new_crtc &&
		    !drm_encoder_crtc_ok(connector->encoder, new_crtc)) {
			ret = EINVAL;
			goto fail;
		}
		if (new_crtc != connector->encoder->crtc) {
			DRM_DEBUG_KMS("crtc changed, full mode switch\n");
			mode_changed = TRUE;
			connector->encoder->crtc = new_crtc;
		}
		if (new_crtc) {
			DRM_DEBUG_KMS("[CONNECTOR:%d:%s] to [CRTC:%d]\n",
				connector->base.id, drm_get_connector_name(connector),
				new_crtc->base.id);
		} else {
			DRM_DEBUG_KMS("[CONNECTOR:%d:%s] to [NOCRTC]\n",
				connector->base.id, drm_get_connector_name(connector));
		}
	}

	/* mode_set_base is not a required function */
	if (fb_changed && !crtc_funcs->mode_set_base)
		mode_changed = TRUE;

	if (mode_changed) {
		set->crtc->enabled = drm_helper_crtc_in_use(set->crtc);
		if (set->crtc->enabled) {
			DRM_DEBUG_KMS("attempting to set mode from"
					" userspace\n");
			drm_mode_debug_printmodeline(set->mode);
			old_fb = set->crtc->fb;
			set->crtc->fb = set->fb;
			if (!drm_crtc_helper_set_mode(set->crtc, set->mode,
						      set->x, set->y,
						      old_fb)) {
				DRM_ERROR("failed to set mode on [CRTC:%d]\n",
					  set->crtc->base.id);
				set->crtc->fb = old_fb;
				ret = EINVAL;
				goto fail;
			}
			DRM_DEBUG_KMS("Setting connector DPMS state to on\n");
			for (i = 0; i < set->num_connectors; i++) {
				DRM_DEBUG_KMS("\t[CONNECTOR:%d:%s] set DPMS on\n", set->connectors[i]->base.id,
					      drm_get_connector_name(set->connectors[i]));
				set->connectors[i]->dpms = DRM_MODE_DPMS_ON;
			}
		}
		drm_helper_disable_unused_functions(dev);
	} else if (fb_changed) {
		set->crtc->x = set->x;
		set->crtc->y = set->y;

		old_fb = set->crtc->fb;
		if (set->crtc->fb != set->fb)
			set->crtc->fb = set->fb;
		ret = crtc_funcs->mode_set_base(set->crtc,
						set->x, set->y, old_fb);
		if (ret != 0) {
			set->crtc->fb = old_fb;
			goto fail;
		}
	}

	free(save_connectors, M_DRM);
	free(save_encoders, M_DRM);
	free(save_crtcs, M_DRM);
	return 0;

fail:
	/* Restore all previous data. */
	count = 0;
	TAILQ_FOREACH(crtc, &dev->mode_config.crtc_list, head) {
		*crtc = save_crtcs[count++];
	}

	count = 0;
	TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {
		*encoder = save_encoders[count++];
	}

	count = 0;
	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head) {
		*connector = save_connectors[count++];
	}

	free(save_connectors, M_DRM);
	free(save_encoders, M_DRM);
	free(save_crtcs, M_DRM);
	return ret;
}

int 
drm_helper_choose_encoder_dpms(struct drm_encoder *encoder)
{
	int dpms = DRM_MODE_DPMS_OFF;
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;

	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder == encoder)
			if (connector->dpms < dpms)
				dpms = connector->dpms;
	return dpms;
}

int 
drm_helper_choose_crtc_dpms(struct drm_crtc *crtc)
{
	int dpms = DRM_MODE_DPMS_OFF;
	struct drm_connector *connector;
	struct drm_device *dev = crtc->dev;

	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder && connector->encoder->crtc == crtc)
			if (connector->dpms < dpms)
				dpms = connector->dpms;
	return dpms;
}

/**
 * drm_helper_connector_dpms
 * @connector affected connector
 * @mode DPMS mode
 *
 * Calls the low-level connector DPMS function, then
 * calls appropriate encoder and crtc DPMS functions as well
 */
void
drm_helper_connector_dpms(struct drm_connector *connector, int mode)
{
	struct drm_encoder *encoder = connector->encoder;
	struct drm_crtc *crtc = encoder ? encoder->crtc : NULL;
	int old_dpms;

	if (mode == connector->dpms)
		return;

	old_dpms = connector->dpms;
	connector->dpms = mode;

	/* from off to on, do crtc then encoder */
	if (mode < old_dpms) {
		if (crtc) {
			struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
		if (encoder) {
			struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
			if (encoder_funcs->dpms)
				(*encoder_funcs->dpms) (encoder,
							drm_helper_choose_encoder_dpms(encoder));
		}
	}

	/* from on to off, do encoder then crtc */
	if (mode > old_dpms) {
		if (encoder) {
			struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
			if (encoder_funcs->dpms)
				(*encoder_funcs->dpms) (encoder,
							drm_helper_choose_encoder_dpms(encoder));
		}
		if (crtc) {
			struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
	}

	return;
}

int drm_helper_mode_fill_fb_struct(struct drm_framebuffer *fb,
				   struct drm_mode_fb_cmd *mode_cmd)
{
	fb->width = mode_cmd->width;
	fb->height = mode_cmd->height;
	fb->pitch = mode_cmd->pitch;
	fb->bits_per_pixel = mode_cmd->bpp;
	fb->depth = mode_cmd->depth;

	return 0;
}

int 
drm_helper_resume_force_mode(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_crtc_helper_funcs *crtc_funcs;
	int ret;

	TAILQ_FOREACH(crtc, &dev->mode_config.crtc_list, head) {

		if (!crtc->enabled)
			continue;

		ret = drm_crtc_helper_set_mode(crtc, &crtc->mode,
					       crtc->x, crtc->y, crtc->fb);

		if (ret == FALSE)
			DRM_ERROR("failed to set mode on crtc %p\n", crtc);

		/* Turn off outputs that were already powered off */
		if (drm_helper_choose_crtc_dpms(crtc)) {
			TAILQ_FOREACH(encoder, &dev->mode_config.encoder_list, head) {

				if(encoder->crtc != crtc)
					continue;

				encoder_funcs = encoder->helper_private;
				if (encoder_funcs->dpms)
					(*encoder_funcs->dpms) (encoder,
								drm_helper_choose_encoder_dpms(encoder));
			}

			crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
	}
	/* disable the unused connectors while restoring the modesetting */
	drm_helper_disable_unused_functions(dev);
	return 0;
}

#ifdef notyet
#define DRM_OUTPUT_POLL_PERIOD (10*HZ)
void 
output_poll_execute(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct drm_device *dev = container_of(delayed_work, struct drm_device, mode_config.output_poll_work);
	struct drm_connector *connector;
	enum drm_connector_status old_status;
	boolean_t repoll = FALSE, changed = FALSE;

	if (!drm_kms_helper_poll)
		return;

	mutex_lock(&dev->mode_config.mutex);
	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head) {

		/* if this is HPD or polled don't check it -
		   TV out for instance */
		if (!connector->polled)
			continue;

		else if (connector->polled & (DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT))
			repoll = TRUE;

		old_status = connector->status;
		/* if we are connected and don't want to poll for disconnect
		   skip it */
		if (old_status == connector_status_connected &&
		    !(connector->polled & DRM_CONNECTOR_POLL_DISCONNECT) &&
		    !(connector->polled & DRM_CONNECTOR_POLL_HPD))
			continue;

		connector->status = connector->funcs->detect(connector, FALSE);
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] status updated from %d to %d\n",
			      connector->base.id,
			      drm_get_connector_name(connector),
			      old_status, connector->status);
		if (old_status != connector->status)
			changed = TRUE;
	}

	mutex_unlock(&dev->mode_config.mutex);

	if (changed) {
		/* send a uevent + call fbdev */
		drm_sysfs_hotplug_event(dev);
		if (dev->mode_config.funcs->output_poll_changed)
			dev->mode_config.funcs->output_poll_changed(dev);
	}

	if (repoll)
		queue_delayed_work(system_nrt_wq, delayed_work, DRM_OUTPUT_POLL_PERIOD);
}
#endif

void 
drm_kms_helper_poll_disable(struct drm_device *dev)
{
#ifdef notyet
	if (!dev->mode_config.poll_enabled)
		return;
	cancel_delayed_work_sync(&dev->mode_config.output_poll_work);
#endif
}

void 
drm_kms_helper_poll_enable(struct drm_device *dev)
{
#ifdef notyet
	boolean_t poll = FALSE;
	struct drm_connector *connector;

	if (!dev->mode_config.poll_enabled || !drm_kms_helper_poll)
		return;

	TAILQ_FOREACH(connector, &dev->mode_config.connector_list, head) {
		if (connector->polled)
			poll = TRUE;
	}

	if (poll)
		queue_delayed_work(system_nrt_wq, &dev->mode_config.output_poll_work, DRM_OUTPUT_POLL_PERIOD);
#endif
}

void 
drm_kms_helper_poll_init(struct drm_device *dev)
{
#ifdef notyet
	INIT_DELAYED_WORK(&dev->mode_config.output_poll_work, output_poll_execute);
	dev->mode_config.poll_enabled = TRUE;

	drm_kms_helper_poll_enable(dev);
#endif
}

void 
drm_kms_helper_poll_fini(struct drm_device *dev)
{
	drm_kms_helper_poll_disable(dev);
}

void 
drm_helper_hpd_irq_event(struct drm_device *dev)
{
#ifdef notyet
	if (!dev->mode_config.poll_enabled)
		return;

	/* kill timer and schedule immediate execution, this doesn't block */
	cancel_delayed_work(&dev->mode_config.output_poll_work);
	if (drm_kms_helper_poll)
		queue_delayed_work(system_nrt_wq, &dev->mode_config.output_poll_work, 0);
#endif
}

/**
 * drm_calc_timestamping_constants - Calculate and
 * store various constants which are later needed by
 * vblank and swap-completion timestamping, e.g, by
 * drm_calc_vbltimestamp_from_scanoutpos().
 * They are derived from crtc's true scanout timing,
 * so they take things like panel scaling or other
 * adjustments into account.
 *
 * @crtc drm_crtc whose timestamp constants should be updated.
 *
 */
void drm_calc_timestamping_constants(struct drm_crtc *crtc)
{
	int64_t linedur_ns = 0, pixeldur_ns = 0, framedur_ns = 0;
	u64 dotclock;

	/* Dot clock in Hz: */
	dotclock = (u64) crtc->hwmode.clock * 1000;

	/* Fields of interlaced scanout modes are only halve a frame duration.
	 * Double the dotclock to get halve the frame-/line-/pixelduration.
	 */
	if (crtc->hwmode.flags & DRM_MODE_FLAG_INTERLACE)
		dotclock *= 2;

	/* Valid dotclock? */
	if (dotclock > 0) {
		/* Convert scanline length in pixels and video dot clock to
		 * line duration, frame duration and pixel duration in
		 * nanoseconds:
		 */
		pixeldur_ns = (int64_t) 1000000000 /  dotclock;
		linedur_ns  = (int64_t) ((u64) crtc->hwmode.crtc_htotal *
					      1000000000) / dotclock;
		framedur_ns = (int64_t) crtc->hwmode.crtc_vtotal * linedur_ns;
	} else
		DRM_ERROR("crtc %d: Can't calculate constants, dotclock = 0!\n",
			  crtc->base.id);

	crtc->pixeldur_ns = pixeldur_ns;
	crtc->linedur_ns  = linedur_ns;
	crtc->framedur_ns = framedur_ns;

	DRM_DEBUG("crtc %d: hwmode: htotal %d, vtotal %d, vdisplay %d\n",
		  crtc->base.id, crtc->hwmode.crtc_htotal,
		  crtc->hwmode.crtc_vtotal, crtc->hwmode.crtc_vdisplay);
	DRM_DEBUG("crtc %d: clock %d kHz framedur %d linedur %d, pixeldur %d\n",
		  crtc->base.id, (int) dotclock/1000, (int) framedur_ns,
		  (int) linedur_ns, (int) pixeldur_ns);
}
