/* $OpenBSD: drm_irq.c,v 1.43 2011/06/02 18:22:00 weerd Exp $ */
/*-
 * Copyright 2003 Eric Anholt
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * ERIC ANHOLT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <anholt@FreeBSD.org>
 *
 */

/** @file drm_irq.c
 * Support code for handling setup/teardown of interrupt handlers and
 * handing interrupt handlers off to the drivers.
 */

#include <sys/workq.h>

#include "drmP.h"
#include "drm.h"

void		drm_update_vblank_count(struct drm_device *, int);
void		vblank_disable(void *);
int		drm_queue_vblank_event(struct drm_device *, int,
		    union drm_wait_vblank *, struct drm_file *);
void		drm_handle_vblank_events(struct drm_device *, int);

#ifdef DRM_VBLANK_DEBUG
#define DPRINTF(x...)	do { printf(x); } while(/* CONSTCOND */ 0)
#else
#define DPRINTF(x...)	do { } while(/* CONSTCOND */ 0)
#endif

int
drm_irq_by_busid(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_irq_busid	*irq = data;

	/*
	 * This is only ever called by root as part of a stupid interface.
	 * just hand over the irq without checking the busid. If all clients
	 * can be forced to use interface 1.2 then this can die.
	 */
	irq->irq = dev->irq;

	DRM_DEBUG("%d:%d:%d => IRQ %d\n", irq->busnum, irq->devnum,
	    irq->funcnum, irq->irq);

	return 0;
}

int
drm_irq_install(struct drm_device *dev)
{
	int	ret;

	if (dev->irq == 0 || dev->dev_private == NULL)
		return (EINVAL);

	DRM_DEBUG("irq=%d\n", dev->irq);

	DRM_LOCK();
	if (dev->irq_enabled) {
		DRM_UNLOCK();
		return (EBUSY);
	}
	dev->irq_enabled = 1;
	DRM_UNLOCK();

	if (dev->driver->irq_install) {
		if ((ret = dev->driver->irq_install(dev)) != 0)
			goto err;
	} else {
		if (dev->driver->irq_preinstall)
			dev->driver->irq_preinstall(dev);
		if (dev->driver->irq_postinstall)
			dev->driver->irq_postinstall(dev);
	}

	return (0);
err:
	DRM_LOCK();
	dev->irq_enabled = 0;
	DRM_UNLOCK();
	return (ret);
}

int
drm_irq_uninstall(struct drm_device *dev)
{
	int i;

	DRM_LOCK();
	if (!dev->irq_enabled) {
		DRM_UNLOCK();
		return (EINVAL);
	}

	dev->irq_enabled = 0;
	DRM_UNLOCK();

	/*
	 * Ick. we're about to turn of vblanks, so make sure anyone waiting
	 * on them gets woken up. Also make sure we update state correctly
	 * so that we can continue refcounting correctly.
	 */
	if (dev->vblank != NULL) {
		mtx_enter(&dev->vblank->vb_lock);
		for (i = 0; i < dev->vblank->vb_num; i++) {
			wakeup(&dev->vblank->vb_crtcs[i]);
			dev->vblank->vb_crtcs[i].vbl_enabled = 0;
			dev->vblank->vb_crtcs[i].vbl_last =
			    dev->driver->get_vblank_counter(dev, i);
		}
		mtx_leave(&dev->vblank->vb_lock);
	}

	DRM_DEBUG("irq=%d\n", dev->irq);

	dev->driver->irq_uninstall(dev);

	return (0);
}

int
drm_control(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_control	*ctl = data;

	/* Handle drivers who used to require IRQ setup no longer does. */
	if (!(dev->driver->flags & DRIVER_IRQ))
		return (0);

	switch (ctl->func) {
	case DRM_INST_HANDLER:
		if (drm_core_check_feature(dev, DRIVER_MODESET))
			return 0;
		if (dev->if_version < DRM_IF_VERSION(1, 2) &&
		    ctl->irq != dev->irq)
			return (EINVAL);
		return (drm_irq_install(dev));
	case DRM_UNINST_HANDLER:
		if (drm_core_check_feature(dev, DRIVER_MODESET))
			return 0;
		return (drm_irq_uninstall(dev));
	default:
		return (EINVAL);
	}
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
void
drm_calc_timestamping_constants(struct drm_crtc *crtc)
{
	int64_t linedur_ns = 0, pixeldur_ns = 0, framedur_ns = 0;
	uint64_t dotclock;

	/* Dot clock in Hz: */
	dotclock = (uint64_t) crtc->hwmode.clock * 1000;

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
		pixeldur_ns = (int64_t)1000000000 / dotclock;
		linedur_ns  = ((uint64_t)crtc->hwmode.crtc_htotal *
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

/**
 * drm_calc_vbltimestamp_from_scanoutpos - helper routine for kms
 * drivers. Implements calculation of exact vblank timestamps from
 * given drm_display_mode timings and current video scanout position
 * of a crtc. This can be called from within get_vblank_timestamp()
 * implementation of a kms driver to implement the actual timestamping.
 *
 * Should return timestamps conforming to the OML_sync_control OpenML
 * extension specification. The timestamp corresponds to the end of
 * the vblank interval, aka start of scanout of topmost-leftmost display
 * pixel in the following video frame.
 *
 * Requires support for optional dev->driver->get_scanout_position()
 * in kms driver, plus a bit of setup code to provide a drm_display_mode
 * that corresponds to the true scanout timing.
 *
 * The current implementation only handles standard video modes. It
 * returns as no operation if a doublescan or interlaced video mode is
 * active. Higher level code is expected to handle this.
 *
 * @dev: DRM device.
 * @crtc: Which crtc's vblank timestamp to retrieve.
 * @max_error: Desired maximum allowable error in timestamps (nanosecs).
 *             On return contains true maximum error of timestamp.
 * @vblank_time: Pointer to struct timeval which should receive the timestamp.
 * @flags: Flags to pass to driver:
 *         0 = Default.
 *         DRM_CALLED_FROM_VBLIRQ = If function is called from vbl irq handler.
 * @refcrtc: drm_crtc* of crtc which defines scanout timing.
 *
 * Returns negative value on error, failure or if not supported in current
 * video mode:
 *
 * -EINVAL   - Invalid crtc.
 * -EAGAIN   - Temporary unavailable, e.g., called before initial modeset.
 * -ENOTSUPP - Function not supported in current display mode.
 * -EIO      - Failed, e.g., due to failed scanout position query.
 *
 * Returns or'ed positive status flags on success:
 *
 * DRM_VBLANKTIME_SCANOUTPOS_METHOD - Signal this method used for timestamping.
 * DRM_VBLANKTIME_INVBL - Timestamp taken while scanout was in vblank interval.
 *
 */
int
drm_calc_vbltimestamp_from_scanoutpos(struct drm_device *dev, int crtc,
					  int *max_error,
					  struct timeval *vblank_time,
					  unsigned flags,
					  struct drm_crtc *refcrtc)
{
	printf("%s stub\n", __func__);
	return EINVAL;
#if 0
	ktime_t stime, etime, mono_time_offset;
	struct timeval tv_etime;
	struct drm_display_mode *mode;
	int vbl_status, vtotal, vdisplay;
	int vpos, hpos, i;
	s64 framedur_ns, linedur_ns, pixeldur_ns, delta_ns, duration_ns;
	bool invbl;

	if (crtc < 0 || crtc >= dev->num_crtcs) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return -EINVAL;
	}

	/* Scanout position query not supported? Should not happen. */
	if (!dev->driver->get_scanout_position) {
		DRM_ERROR("Called from driver w/o get_scanout_position()!?\n");
		return -EIO;
	}

	mode = &refcrtc->hwmode;
	vtotal = mode->crtc_vtotal;
	vdisplay = mode->crtc_vdisplay;

	/* Durations of frames, lines, pixels in nanoseconds. */
	framedur_ns = refcrtc->framedur_ns;
	linedur_ns  = refcrtc->linedur_ns;
	pixeldur_ns = refcrtc->pixeldur_ns;

	/* If mode timing undefined, just return as no-op:
	 * Happens during initial modesetting of a crtc.
	 */
	if (vtotal <= 0 || vdisplay <= 0 || framedur_ns == 0) {
		DRM_DEBUG("crtc %d: Noop due to uninitialized mode.\n", crtc);
		return -EAGAIN;
	}

	/* Get current scanout position with system timestamp.
	 * Repeat query up to DRM_TIMESTAMP_MAXRETRIES times
	 * if single query takes longer than max_error nanoseconds.
	 *
	 * This guarantees a tight bound on maximum error if
	 * code gets preempted or delayed for some reason.
	 */
	for (i = 0; i < DRM_TIMESTAMP_MAXRETRIES; i++) {
		/* Disable preemption to make it very likely to
		 * succeed in the first iteration even on PREEMPT_RT kernel.
		 */
		preempt_disable();

		/* Get system timestamp before query. */
		stime = ktime_get();

		/* Get vertical and horizontal scanout pos. vpos, hpos. */
		vbl_status = dev->driver->get_scanout_position(dev, crtc, &vpos, &hpos);

		/* Get system timestamp after query. */
		etime = ktime_get();
		if (!drm_timestamp_monotonic)
			mono_time_offset = ktime_get_monotonic_offset();

		preempt_enable();

		/* Return as no-op if scanout query unsupported or failed. */
		if (!(vbl_status & DRM_SCANOUTPOS_VALID)) {
			DRM_DEBUG("crtc %d : scanoutpos query failed [%d].\n",
				  crtc, vbl_status);
			return -EIO;
		}

		duration_ns = ktime_to_ns(etime) - ktime_to_ns(stime);

		/* Accept result with <  max_error nsecs timing uncertainty. */
		if (duration_ns <= (s64) *max_error)
			break;
	}

	/* Noisy system timing? */
	if (i == DRM_TIMESTAMP_MAXRETRIES) {
		DRM_DEBUG("crtc %d: Noisy timestamp %d us > %d us [%d reps].\n",
			  crtc, (int) duration_ns/1000, *max_error/1000, i);
	}

	/* Return upper bound of timestamp precision error. */
	*max_error = (int) duration_ns;

	/* Check if in vblank area:
	 * vpos is >=0 in video scanout area, but negative
	 * within vblank area, counting down the number of lines until
	 * start of scanout.
	 */
	invbl = vbl_status & DRM_SCANOUTPOS_INVBL;

	/* Convert scanout position into elapsed time at raw_time query
	 * since start of scanout at first display scanline. delta_ns
	 * can be negative if start of scanout hasn't happened yet.
	 */
	delta_ns = (s64) vpos * linedur_ns + (s64) hpos * pixeldur_ns;

	/* Is vpos outside nominal vblank area, but less than
	 * 1/100 of a frame height away from start of vblank?
	 * If so, assume this isn't a massively delayed vblank
	 * interrupt, but a vblank interrupt that fired a few
	 * microseconds before true start of vblank. Compensate
	 * by adding a full frame duration to the final timestamp.
	 * Happens, e.g., on ATI R500, R600.
	 *
	 * We only do this if DRM_CALLED_FROM_VBLIRQ.
	 */
	if ((flags & DRM_CALLED_FROM_VBLIRQ) && !invbl &&
	    ((vdisplay - vpos) < vtotal / 100)) {
		delta_ns = delta_ns - framedur_ns;

		/* Signal this correction as "applied". */
		vbl_status |= 0x8;
	}

	if (!drm_timestamp_monotonic)
		etime = ktime_sub(etime, mono_time_offset);

	/* save this only for debugging purposes */
	tv_etime = ktime_to_timeval(etime);
	/* Subtract time delta from raw timestamp to get final
	 * vblank_time timestamp for end of vblank.
	 */
	etime = ktime_sub_ns(etime, delta_ns);
	*vblank_time = ktime_to_timeval(etime);

	DRM_DEBUG("crtc %d : v %d p(%d,%d)@ %ld.%ld -> %ld.%ld [e %d us, %d rep]\n",
		  crtc, (int)vbl_status, hpos, vpos,
		  (long)tv_etime.tv_sec, (long)tv_etime.tv_usec,
		  (long)vblank_time->tv_sec, (long)vblank_time->tv_usec,
		  (int)duration_ns/1000, i);

	vbl_status = DRM_VBLANKTIME_SCANOUTPOS_METHOD;
	if (invbl)
		vbl_status |= DRM_VBLANKTIME_INVBL;

	return vbl_status;
#endif
}

void
vblank_disable(void *arg)
{
	struct drm_device	*dev = (struct drm_device*)arg;
	struct drm_vblank_info	*vbl = dev->vblank;
	struct drm_vblank	*crtc;
	int			 i;

	mtx_enter(&vbl->vb_lock);
	for (i = 0; i < vbl->vb_num; i++) {
		crtc = &vbl->vb_crtcs[i];

		if (crtc->vbl_refs == 0 && crtc->vbl_enabled) {
			DPRINTF("%s: disabling crtc %d\n", __func__, i);
			crtc->vbl_last =
			    dev->driver->get_vblank_counter(dev, i);
			dev->driver->disable_vblank(dev, i);
			crtc->vbl_enabled = 0;
		}
	}
	mtx_leave(&vbl->vb_lock);
}

void
drm_vblank_cleanup(struct drm_device *dev)
{
	if (dev->vblank == NULL)
		return; /* not initialised */

	timeout_del(&dev->vblank->vb_disable_timer);

	vblank_disable(dev);

	drm_free(dev->vblank);
	dev->vblank = NULL;
	drm_free(dev->vblank_inmodeset);
	dev->vblank_inmodeset = NULL;
}

int
drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	int	i;

	dev->vblank = malloc(sizeof(*dev->vblank) + (num_crtcs *
	    sizeof(struct drm_vblank)), M_DRM,  M_WAITOK | M_CANFAIL | M_ZERO);
	if (dev->vblank == NULL)
		return (ENOMEM);
	dev->vblank_inmodeset = malloc(num_crtcs * sizeof(int),
	    M_DRM, M_WAITOK | M_ZERO);
	dev->vblank->vb_num = num_crtcs;
	mtx_init(&dev->vblank->vb_lock, IPL_TTY);
	timeout_set(&dev->vblank->vb_disable_timer, vblank_disable, dev);
	for (i = 0; i < num_crtcs; i++)
		TAILQ_INIT(&dev->vblank->vb_crtcs[i].vbl_events);

	return (0);
}

u_int32_t
drm_vblank_count(struct drm_device *dev, int crtc)
{
	return (dev->vblank->vb_crtcs[crtc].vbl_count);
}

void
drm_update_vblank_count(struct drm_device *dev, int crtc)
{
	u_int32_t	cur_vblank, diff;

	/*
	 * Interrupt was disabled prior to this call, so deal with counter wrap
	 * note that we may have lost a full vb_max events if
	 * the register is small or the interrupts were off for a long time.
	 */
	cur_vblank = dev->driver->get_vblank_counter(dev, crtc);
	diff = cur_vblank - dev->vblank->vb_crtcs[crtc].vbl_last;
	if (cur_vblank < dev->vblank->vb_crtcs[crtc].vbl_last)
		diff += dev->vblank->vb_max;

	dev->vblank->vb_crtcs[crtc].vbl_count += diff;
}

int
drm_vblank_get(struct drm_device *dev, int crtc)
{
	struct drm_vblank_info	*vbl = dev->vblank;
	int			 ret = 0;

	if (dev->irq_enabled == 0)
		return (EINVAL);

	mtx_enter(&vbl->vb_lock);
	DPRINTF("%s: %d refs = %d\n", __func__, crtc,
	    vbl->vb_crtcs[crtc].vbl_refs);
	vbl->vb_crtcs[crtc].vbl_refs++;
	if (vbl->vb_crtcs[crtc].vbl_refs == 1 &&
	    vbl->vb_crtcs[crtc].vbl_enabled == 0) {
		if ((ret = dev->driver->enable_vblank(dev, crtc)) == 0) {
			vbl->vb_crtcs[crtc].vbl_enabled = 1;
			drm_update_vblank_count(dev, crtc);
		} else {
			vbl->vb_crtcs[crtc].vbl_refs--;
		}

	}
	mtx_leave(&vbl->vb_lock);

	return (ret);
}

void
drm_vblank_put(struct drm_device *dev, int crtc)
{
	mtx_enter(&dev->vblank->vb_lock);
	/* Last user schedules disable */
	DPRINTF("%s: %d  refs = %d\n", __func__, crtc,
	    dev->vblank->vb_crtcs[crtc].vbl_refs);
	KASSERT(dev->vblank->vb_crtcs[crtc].vbl_refs > 0);
	if (--dev->vblank->vb_crtcs[crtc].vbl_refs == 0)
		timeout_add_sec(&dev->vblank->vb_disable_timer, 5);
	mtx_leave(&dev->vblank->vb_lock);
}

void drm_vblank_off(struct drm_device *dev, int crtc)
{
	printf("%s stub\n", __func__);
#ifdef notyet
	struct drm_pending_vblank_event *e, *t;
	struct timeval now;
	unsigned int seq;

	mtx_enter(&dev->vblank->vb_lock);
	vblank_disable_and_save(dev, crtc);
	mtx_enter(&dev->event_lock);
	wakeup(&dev->_vblank_count[crtc]);

	/* Send any queued vblank events, lest the natives grow disquiet */
	seq = drm_vblank_count_and_time(dev, crtc, &now);
	list_for_each_entry_safe(e, t, &dev->vblank_event_list, base.link) {
		if (e->pipe != crtc)
			continue;
		DRM_DEBUG("Sending premature vblank event on disable: \
			  wanted %d, current %d\n",
			  e->event.sequence, seq);

		e->event.sequence = seq;
		e->event.tv_sec = now.tv_sec;
		e->event.tv_usec = now.tv_usec;
		drm_vblank_put(dev, e->pipe);
		list_move_tail(&e->base.link, &e->base.file_priv->event_list);
		drm_event_wakeup(&e->base);
		CTR3(KTR_DRM, "vblank_event_delivered %d %d %d",
		    e->base.pid, e->pipe, e->event.sequence);
	}

	mtx_leave(&dev->event_lock);
	mtx_leave(&dev->vblank->vb_lock);
#endif
}

/**
 * drm_vblank_pre_modeset - account for vblanks across mode sets
 * @dev: DRM device
 * @crtc: CRTC in question
 * @post: post or pre mode set?
 *
 * Account for vblank events across mode setting events, which will likely
 * reset the hardware frame counter.
 */
void drm_vblank_pre_modeset(struct drm_device *dev, int crtc)
{
	/* vblank is not initialized (IRQ not installed ?) */
	if (!dev->num_crtcs)
		return;
	/*
	 * To avoid all the problems that might happen if interrupts
	 * were enabled/disabled around or between these calls, we just
	 * have the kernel take a reference on the CRTC (just once though
	 * to avoid corrupting the count if multiple, mismatch calls occur),
	 * so that interrupts remain enabled in the interim.
	 */
	if (!dev->vblank_inmodeset[crtc]) {
		dev->vblank_inmodeset[crtc] = 0x1;
		if (drm_vblank_get(dev, crtc) == 0)
			dev->vblank_inmodeset[crtc] |= 0x2;
	}
}

void drm_vblank_post_modeset(struct drm_device *dev, int crtc)
{

	if (dev->vblank_inmodeset[crtc]) {
		mtx_enter(&dev->vblank->vb_lock);
		dev->vblank_disable_allowed = 1;
		mtx_leave(&dev->vblank->vb_lock);

		if (dev->vblank_inmodeset[crtc] & 0x2)
			drm_vblank_put(dev, crtc);

		dev->vblank_inmodeset[crtc] = 0;
	}
}

int
drm_modeset_ctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_modeset_ctl	*modeset = data;
	struct drm_vblank	*vbl;
	int			 crtc, ret = 0;

	/* not initialised yet, just noop */
	if (dev->vblank == NULL)
		return (0);

	crtc = modeset->crtc;
	if (crtc >= dev->vblank->vb_num || crtc < 0)
		return (EINVAL);

	vbl = &dev->vblank->vb_crtcs[crtc];

	/*
	 * If interrupts are enabled/disabled between calls to this ioctl then
	 * it can get nasty. So just grab a reference so that the interrupts
	 * keep going through the modeset
	 */
	switch (modeset->cmd) {
	case _DRM_PRE_MODESET:
		DPRINTF("%s: pre modeset on %d\n", __func__, crtc);
		if (vbl->vbl_inmodeset == 0) {
			mtx_enter(&dev->vblank->vb_lock);
			vbl->vbl_inmodeset = 0x1;
			mtx_leave(&dev->vblank->vb_lock);
			if (drm_vblank_get(dev, crtc) == 0)
				vbl->vbl_inmodeset |= 0x2;
		}
		break;
	case _DRM_POST_MODESET:
		DPRINTF("%s: post modeset on %d\n", __func__, crtc);
		if (vbl->vbl_inmodeset) {
			if (vbl->vbl_inmodeset & 0x2)
				drm_vblank_put(dev, crtc);
			vbl->vbl_inmodeset = 0;
		}
		break;
	default:
		ret = EINVAL;
		break;
	}

	return (ret);
}

int
drm_wait_vblank(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct timeval		 now;
	union drm_wait_vblank	*vblwait = data;
	int			 ret, flags, crtc, seq;

	if (!dev->irq_enabled || dev->vblank == NULL ||
	    vblwait->request.type & _DRM_VBLANK_SIGNAL)
		return (EINVAL);

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	crtc = flags & _DRM_VBLANK_SECONDARY ? 1 : 0;

	if (crtc >= dev->vblank->vb_num)
		return (EINVAL);

	if ((ret = drm_vblank_get(dev, crtc)) != 0)
		return (ret);
	seq = drm_vblank_count(dev, crtc);

	if (vblwait->request.type & _DRM_VBLANK_RELATIVE) {
		vblwait->request.sequence += seq;
		vblwait->request.type &= ~_DRM_VBLANK_RELATIVE;
	}

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	if ((flags & _DRM_VBLANK_NEXTONMISS) &&
	    (seq - vblwait->request.sequence) <= (1<<23)) {
		vblwait->request.sequence = seq + 1;
	}

	if (flags & _DRM_VBLANK_EVENT)
		return (drm_queue_vblank_event(dev, crtc, vblwait, file_priv));

	DPRINTF("%s: %d waiting on %d, current %d\n", __func__, crtc,
	     vblwait->request.sequence, drm_vblank_count(dev, crtc));
	DRM_WAIT_ON(ret, &dev->vblank->vb_crtcs[crtc], &dev->vblank->vb_lock,
	    3 * hz, "drmvblq", ((drm_vblank_count(dev, crtc) -
	    vblwait->request.sequence) <= (1 << 23)) || dev->irq_enabled == 0);

	microtime(&now);
	vblwait->reply.tval_sec = now.tv_sec;
	vblwait->reply.tval_usec = now.tv_usec;
	vblwait->reply.sequence = drm_vblank_count(dev, crtc);
	DPRINTF("%s: %d done waiting, seq = %d\n", __func__, crtc,
	    vblwait->reply.sequence);

	drm_vblank_put(dev, crtc);
	return (ret);
}

int
drm_queue_vblank_event(struct drm_device *dev, int crtc,
    union drm_wait_vblank *vblwait, struct drm_file *file_priv)
{
	struct drm_pending_vblank_event	*vev;
	struct timeval			 now;
	u_int				 seq;


	vev = drm_calloc(1, sizeof(*vev));
	if (vev == NULL)
		return (ENOMEM);

	vev->event.base.type = DRM_EVENT_VBLANK;
	vev->event.base.length = sizeof(vev->event);
	vev->event.user_data = vblwait->request.signal;
	vev->base.event = &vev->event.base;
	vev->base.file_priv = file_priv;
	vev->base.destroy = (void (*) (struct drm_pending_event *))drm_free;

	microtime(&now);

	mtx_enter(&dev->event_lock);
	if (file_priv->event_space < sizeof(vev->event)) {
		mtx_leave(&dev->event_lock);
		drm_free(vev);
		return (ENOMEM);
	}


	seq = drm_vblank_count(dev, crtc);
	file_priv->event_space -= sizeof(vev->event);

	DPRINTF("%s: queueing event %d on crtc %d\n", __func__, seq, crtc);

	if ((vblwait->request.type & _DRM_VBLANK_NEXTONMISS) &&
	    (seq - vblwait->request.sequence) <= (1 << 23)) {
		vblwait->request.sequence = seq + 1;
		vblwait->reply.sequence = vblwait->request.sequence;
	}

	vev->event.sequence = vblwait->request.sequence;
	if ((seq - vblwait->request.sequence) <= (1 << 23)) {
		vev->event.tv_sec = now.tv_sec;
		vev->event.tv_usec = now.tv_usec;
		DPRINTF("%s: already passed, dequeuing: crtc %d, value %d\n",
		    __func__, crtc, seq);
		drm_vblank_put(dev, crtc);
		TAILQ_INSERT_TAIL(&file_priv->evlist, &vev->base, link);
		wakeup(&file_priv->evlist);
		selwakeup(&file_priv->rsel);
	} else {
		TAILQ_INSERT_TAIL(&dev->vblank->vb_crtcs[crtc].vbl_events,
		    &vev->base, link);
	}
	mtx_leave(&dev->event_lock);

	return (0);
}

void
drm_handle_vblank_events(struct drm_device *dev, int crtc)
{
	struct drmevlist		*list;
	struct drm_pending_event	*ev, *tmp;
	struct drm_pending_vblank_event	*vev;
	struct timeval			 now;
	u_int				 seq;

	list = &dev->vblank->vb_crtcs[crtc].vbl_events;
	microtime(&now);
	seq = drm_vblank_count(dev, crtc);

	mtx_enter(&dev->event_lock);
	for (ev = TAILQ_FIRST(list); ev != TAILQ_END(list); ev = tmp) {
		tmp = TAILQ_NEXT(ev, link);

		vev = (struct drm_pending_vblank_event *)ev;

		if ((seq - vev->event.sequence) > (1 << 23))
			continue;
		DPRINTF("%s: got vblank event on crtc %d, value %d\n",
		    __func__, crtc, seq);
		
		vev->event.sequence = seq;
		vev->event.tv_sec = now.tv_sec;
		vev->event.tv_usec = now.tv_usec;
		drm_vblank_put(dev, crtc);
		TAILQ_REMOVE(list, ev, link);
		TAILQ_INSERT_TAIL(&ev->file_priv->evlist, ev, link);
		wakeup(&ev->file_priv->evlist);
		selwakeup(&ev->file_priv->rsel);
	}
	mtx_leave(&dev->event_lock);
}

bool
drm_handle_vblank(struct drm_device *dev, int crtc)
{
	/*
	 * XXX if we had proper atomic operations this mutex wouldn't
	 * XXX need to be held.
	 */
	mtx_enter(&dev->vblank->vb_lock);
	dev->vblank->vb_crtcs[crtc].vbl_count++;
	wakeup(&dev->vblank->vb_crtcs[crtc]);
	mtx_leave(&dev->vblank->vb_lock);
	drm_handle_vblank_events(dev, crtc);
	return true;
}
