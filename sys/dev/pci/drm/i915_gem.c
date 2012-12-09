/* $OpenBSD: i915_drv.c,v 1.123 2012/09/25 10:19:46 jsg Exp $ */
/*
 * Copyright (c) 2008-2009 Owain G. Ainsworth <oga@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*-
 * Copyright © 2008 Intel Corporation
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

#include <machine/pmap.h>

#include <sys/queue.h>
#include <sys/workq.h>

#include "drmP.h"
#include "drm.h"
#include "i915_drv.h"
#include "intel_drv.h"

void	 i915_gem_object_flush_cpu_write_domain(struct drm_obj *obj);

// i915_gem_info_add_obj
// i915_gem_info_remove_obj
// i915_gem_wait_for_error
// i915_mutex_lock_interruptible
// i915_gem_free_object_tail
 
/*
 * NOTE all object unreferences in this driver need to hold the DRM_LOCK(),
 * because if they free they poke around in driver structures.
 */
void
i915_gem_free_object(struct drm_obj *obj)
{
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	DRM_ASSERT_HELD(obj);
	while (obj_priv->pin_count > 0)
		i915_gem_object_unpin(obj);

	i915_gem_object_unbind(obj, 0);
	drm_free(obj_priv->bit_17);
	obj_priv->bit_17 = NULL;
	/* XXX dmatag went away? */
}

// init_ring_lists
// i915_gem_load
// i915_gem_do_init

int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_init	*args = data;

	DRM_LOCK();

	if (args->gtt_start >= args->gtt_end ||
	    args->gtt_end > dev->agp->info.ai_aperture_size ||
	    (args->gtt_start & PAGE_MASK) != 0 ||
	    (args->gtt_end & PAGE_MASK) != 0) {
		DRM_UNLOCK();
		return (EINVAL);
	}
	/*
	 * putting stuff in the last page of the aperture can cause nasty
	 * problems with prefetch going into unassigned memory. Since we put
	 * a scratch page on all unused aperture pages, just leave the last
	 * page as a spill to prevent gpu hangs.
	 */
	if (args->gtt_end == dev->agp->info.ai_aperture_size)
		args->gtt_end -= 4096;

	if (agp_bus_dma_init((struct agp_softc *)dev->agp->agpdev,
	    dev->agp->base + args->gtt_start, dev->agp->base + args->gtt_end,
	    &dev_priv->agpdmat) != 0) {
		DRM_UNLOCK();
		return (ENOMEM);
	}

	dev->gtt_total = (uint32_t)(args->gtt_end - args->gtt_start);
	inteldrm_set_max_obj_size(dev_priv);

	DRM_UNLOCK();

	return 0;
}

int
i915_gem_idle(struct inteldrm_softc *dev_priv)
{
	struct drm_device	*dev = (struct drm_device *)dev_priv->drmdev;
	int			 ret;

	/* If drm attach failed */
	if (dev == NULL)
		return (0);

	DRM_LOCK();
	if (dev_priv->mm.suspended || dev_priv->ring.ring_obj == NULL) {
		KASSERT(TAILQ_EMPTY(&dev_priv->mm.flushing_list));
		KASSERT(TAILQ_EMPTY(&dev_priv->mm.active_list));
		(void)i915_gem_evict_inactive(dev_priv, 0);
		DRM_UNLOCK();
		return (0);
	}

	/*
	 * To idle the gpu, flush anything pending then unbind the whole
	 * shebang. If we're wedged, assume that the reset workq will clear
	 * everything out and continue as normal.
	 */
	if ((ret = i915_gem_evict_everything(dev_priv, 1)) != 0 &&
	    ret != ENOSPC && ret != EIO) {
		DRM_UNLOCK();
		return (ret);
	}

	/* Hack!  Don't let anybody do execbuf while we don't control the chip.
	 * We need to replace this with a semaphore, or something.
	 */
	dev_priv->mm.suspended = 1;
	/* if we hung then the timer alredy fired. */
	timeout_del(&dev_priv->mm.hang_timer);

	inteldrm_update_ring(dev_priv);
	i915_gem_cleanup_ringbuffer(dev_priv);
	DRM_UNLOCK();

	/* this should be idle now */
	timeout_del(&dev_priv->mm.retire_timer);

	return 0;
}

// i915_gem_init_swizzling
// i915_gem_init_ppgtt
// i915_gem_init_hw

int
i915_gem_init(struct drm_device *dev)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	uint64_t			 gtt_start, gtt_end;
	struct agp_softc		*asc;

	DRM_LOCK();

	asc = (struct agp_softc *)dev->agp->agpdev;
	gtt_start = asc->sc_stolen_entries * 4096;

	/*
	 * putting stuff in the last page of the aperture can cause nasty
	 * problems with prefetch going into unassigned memory. Since we put
	 * a scratch page on all unused aperture pages, just leave the last
	 * page as a spill to prevent gpu hangs.
	 */
	gtt_end = dev->agp->info.ai_aperture_size - 4096;

	if (agp_bus_dma_init(asc,
	    dev->agp->base + gtt_start, dev->agp->base + gtt_end,
	    &dev_priv->agpdmat) != 0) {
		DRM_UNLOCK();
		return (ENOMEM);
	}

	dev->gtt_total = (uint32_t)(gtt_end - gtt_start);
	inteldrm_set_max_obj_size(dev_priv);

	DRM_UNLOCK();

	return 0;
}

int
i915_gem_get_aperture_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_gem_get_aperture	*args = data;

	/* we need a write lock here to make sure we get the right value */
	DRM_LOCK();
	args->aper_size = dev->gtt_total;
	args->aper_available_size = (args->aper_size -
	    atomic_read(&dev->pin_memory));
	DRM_UNLOCK();

	return (0);
}

int
i915_gem_object_pin(struct drm_obj *obj, uint32_t alignment, int needs_fence)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	/*
	 * if already bound, but alignment is unsuitable, unbind so we can
	 * fix it. Similarly if we have constraints due to fence registers,
	 * adjust if needed. Note that if we are already pinned we may as well
	 * fail because whatever depends on this alignment will render poorly
	 * otherwise, so just fail the pin (with a printf so we can fix a
	 * wrong userland).
	 */
	if (obj_priv->dmamap != NULL &&
	    ((alignment && obj_priv->gtt_offset & (alignment - 1)) ||
	    obj_priv->gtt_offset & (i915_gem_get_gtt_alignment(obj) - 1) ||
	    !i915_gem_object_fence_offset_ok(obj, obj_priv->tiling_mode))) {
		/* if it is already pinned we sanitised the alignment then */
		KASSERT(obj_priv->pin_count == 0);
		if ((ret = i915_gem_object_unbind(obj, 1)))
			return (ret);
	}

	if (obj_priv->dmamap == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj, alignment, 1);
		if (ret != 0)
			return (ret);
	}

	/*
	 * due to lazy fence destruction we may have an invalid fence now.
	 * So if so, nuke it before we do anything with the gpu.
	 * XXX 965+ can put this off.. and be faster
	 */
	if (obj->do_flags & I915_FENCE_INVALID) {
		ret= i915_gem_object_put_fence_reg(obj, 1);
		if (ret)
			return (ret);
	}
	/*
	 * Pre-965 chips may need a fence register set up in order to
	 * handle tiling properly. GTT mapping may have blown it away so
	 * restore.
	 * With execbuf2 support we don't always need it, but if we do grab
	 * it.
	 */
	if (needs_fence && obj_priv->tiling_mode != I915_TILING_NONE &&
	    (ret = i915_gem_get_fence_reg(obj, 1)) != 0)
		return (ret);

	/* If the object is not active and not pending a flush,
	 * remove it from the inactive list
	 */
	if (++obj_priv->pin_count == 1) {
		atomic_inc(&dev->pin_count);
		atomic_add(obj->size, &dev->pin_memory);
		if (!inteldrm_is_active(obj_priv))
			i915_list_remove(obj_priv);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	return (0);
}

void
i915_gem_object_unpin(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	KASSERT(obj_priv->pin_count >= 1);
	KASSERT(obj_priv->dmamap != NULL);
	DRM_ASSERT_HELD(obj);

	/* If the object is no longer pinned, and is
	 * neither active nor being flushed, then stick it on
	 * the inactive list
	 */
	if (--obj_priv->pin_count == 0) {
		if (!inteldrm_is_active(obj_priv))
			i915_gem_object_move_to_inactive(obj);
		atomic_dec(&dev->pin_count);
		atomic_sub(obj->size, &dev->pin_memory);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
}

int
i915_gem_pin_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_i915_gem_pin	*args = data;
	struct drm_obj		*obj;
	struct inteldrm_obj	*obj_priv;
	int			 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	DRM_LOCK();
	drm_hold_object(obj);

	obj_priv = (struct inteldrm_obj *)obj;
	if (i915_obj_purgeable(obj_priv)) {
		printf("%s: pinning purgeable object\n", __func__);
		ret = EINVAL;
		goto out;
	}

	if (++obj_priv->user_pin_count == 1) {
		ret = i915_gem_object_pin(obj, args->alignment, 1);
		if (ret != 0)
			goto out;
		inteldrm_set_max_obj_size(dev_priv);
	}

	/* XXX - flush the CPU caches for pinned objects
	 * as the X server doesn't manage domains yet
	 */
	i915_gem_object_set_to_gtt_domain(obj, 1, 1);
	args->offset = obj_priv->gtt_offset;

out:
	drm_unhold_and_unref(obj);
	DRM_UNLOCK();

	return (ret);
}

int
i915_gem_unpin_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_i915_gem_pin	*args = data;
	struct inteldrm_obj	*obj_priv;
	struct drm_obj		*obj;
	int			 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);

	DRM_LOCK();
	drm_hold_object(obj);

	obj_priv = (struct inteldrm_obj *)obj;
	if (obj_priv->user_pin_count == 0) {
		ret = EINVAL;
		goto out;
	}

	if (--obj_priv->user_pin_count == 0) {
		i915_gem_object_unpin(obj);
		inteldrm_set_max_obj_size(dev_priv);
	}

out:
	drm_unhold_and_unref(obj);
	DRM_UNLOCK();
	return (ret);
}

int
i915_gem_busy_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_busy	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	int				 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("Bad handle in i915_gem_busy_ioctl(): %d\n",
			  args->handle);
		return (EBADF);
	}
	
	obj_priv = (struct inteldrm_obj *)obj;
	args->busy = inteldrm_is_active(obj_priv);
	if (args->busy) {
		/*
		 * Unconditionally flush objects write domain if they are
		 * busy. The fact userland is calling this ioctl means that
		 * it wants to use this buffer sooner rather than later, so
		 * flushing now shoul reduce latency.
		 */
		if (obj->write_domain)
			(void)i915_gem_flush(dev_priv, obj->write_domain,
			    obj->write_domain);
		/*
		 * Update the active list after the flush otherwise this is
		 * only updated on a delayed timer. Updating now reduces 
		 * working set size.
		 */
		i915_gem_retire_requests(dev_priv);
		args->busy = inteldrm_is_active(obj_priv);
	}

	drm_unref(&obj->uobj);
	return (ret);
}

/* Throttle our rendering by waiting until the ring has completed our requests
 * emitted over 20 msec ago.
 *
 * This should get us reasonable parallelism between CPU and GPU but also
 * relatively low latency when blocking on a particular request to finish.
 */
int
i915_gem_ring_throttle(struct drm_device *dev, struct drm_file *file_priv)
{
#if 0
	struct inteldrm_file	*intel_file = (struct inteldrm_file *)file_priv;
	u_int32_t		 seqno;
#endif
	int			 ret = 0;

	return ret;
}

// i915_gem_throttle_ioctl

int
i915_gem_madvise_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_gem_madvise	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	int				 need, ret = 0;

	switch (args->madv) {
	case I915_MADV_DONTNEED:
		need = 0;
		break;
	case I915_MADV_WILLNEED:
		need = 1;
		break;
	default:
		return (EINVAL);
	}

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);

	drm_hold_object(obj);
	obj_priv = (struct inteldrm_obj *)obj;

	/* invalid to madvise on a pinned BO */
	if (obj_priv->pin_count) {
		ret = EINVAL;
		goto out;
	}

	if (!i915_obj_purged(obj_priv)) {
		if (need) {
			atomic_clearbits_int(&obj->do_flags,
			    I915_DONTNEED);
		} else {
			atomic_setbits_int(&obj->do_flags, I915_DONTNEED);
		}
	}


	/* if the object is no longer bound, discard its backing storage */
	if (i915_obj_purgeable(obj_priv) && obj_priv->dmamap == NULL)
		inteldrm_purge_obj(obj);

	args->retained = !i915_obj_purged(obj_priv);

out:
	drm_unhold_and_unref(obj);

	return (ret);
}

void
i915_gem_cleanup_ringbuffer(struct inteldrm_softc *dev_priv)
{
	if (dev_priv->ring.ring_obj == NULL)
		return;
	agp_unmap_subregion(dev_priv->agph, dev_priv->ring.bsh,
	    dev_priv->ring.ring_obj->size);
	drm_hold_object(dev_priv->ring.ring_obj);
	i915_gem_object_unpin(dev_priv->ring.ring_obj);
	drm_unhold_and_unref(dev_priv->ring.ring_obj);
	dev_priv->ring.ring_obj = NULL;
	memset(&dev_priv->ring, 0, sizeof(dev_priv->ring));

	i915_gem_cleanup_hws(dev_priv);
}

int
i915_gem_entervt_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	int ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return (0);

	/* XXX until we have support for the rings on sandybridge */
	if (IS_GEN6(dev_priv) || IS_GEN7(dev_priv))
		return (0);

	if (dev_priv->mm.wedged) {
		DRM_ERROR("Reenabling wedged hardware, good luck\n");
		dev_priv->mm.wedged = 0;
	}


	DRM_LOCK();
	dev_priv->mm.suspended = 0;

	ret = i915_gem_init_ringbuffer(dev_priv);
	if (ret != 0) {
		DRM_UNLOCK();
		return (ret);
	}

	/* gtt mapping means that the inactive list may not be empty */
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.active_list));
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.flushing_list));
	KASSERT(TAILQ_EMPTY(&dev_priv->mm.request_list));
	DRM_UNLOCK();

	drm_irq_install(dev);

	return (0);
}

int
i915_gem_leavevt_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int			 ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	/* don't unistall if we fail, repeat calls on failure will screw us */
	if ((ret = i915_gem_idle(dev_priv)) == 0)
		drm_irq_uninstall(dev);
	return (ret);
}

int
i915_gem_create(struct drm_file *file, struct drm_device *dev, uint64_t size,
    uint32_t *handle_p)
{
	struct drm_obj *obj;
	uint32_t handle;
	int ret;

	size = round_page(size);
	if (size == 0)
		return (-EINVAL);

	obj = drm_gem_object_alloc(dev, size);
	if (obj == NULL)
		return (-ENOMEM);

	handle = 0;
	ret = drm_handle_create(file, obj, &handle);
	if (ret != 0) {
		drm_unref(&obj->uobj);
		return (-ret);
	}

	*handle_p = handle;
	return (0);
}

int
i915_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
    struct drm_mode_create_dumb *args)
{

	/* have to work out size/pitch and return them */
	args->pitch = roundup2(args->width * ((args->bpp + 7) / 8), 64);
	args->size = args->pitch * args->height;
	return (i915_gem_create(file, dev, args->size, &args->handle));
}

int
i915_gem_dumb_destroy(struct drm_file *file, struct drm_device *dev,
    uint32_t handle)
{

	printf("%s stub\n", __func__);
	return ENOSYS;
//	return (drm_gem_handle_delete(file, handle));
}

/**
 * Creates a new mm object and returns a handle to it.
 */
int
i915_gem_create_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_create	*args = data;
	struct drm_obj			*obj;
	int				 handle, ret;

	args->size = round_page(args->size);
	/*
	 * XXX to avoid copying between 2 objs more than half the aperture size
	 * we don't allow allocations that are that big. This will be fixed
	 * eventually by intelligently falling back to cpu reads/writes in
	 * such cases. (linux allows this but does cpu maps in the ddx instead).
	 */
	if (args->size > dev_priv->max_gem_obj_size)
		return (EFBIG);

	/* Allocate the new object */
	obj = drm_gem_object_alloc(dev, args->size);
	if (obj == NULL)
		return (ENOMEM);

	/* we give our reference to the handle */
	ret = drm_handle_create(file_priv, obj, &handle);

	if (ret == 0)
		args->handle = handle;
	else
		drm_unref(&obj->uobj);

	return (ret);
}

// i915_gem_swap_io
// i915_gem_gtt_write
// i915_gem_obj_io

/**
 * Reads data from the object referenced by handle.
 *
 * On error, the contents of *data are undefined.
 */
int
i915_gem_pread_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_pread	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	char				*vaddr;
	bus_space_handle_t		 bsh;
	bus_size_t			 bsize;
	voff_t				 offset;
	int				 ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	DRM_READLOCK();
	drm_hold_object(obj);

	/*
	 * Bounds check source.
	 */
	if (args->offset > obj->size || args->size > obj->size ||
	    args->offset + args->size > obj->size) {
		ret = EINVAL;
		goto out;
	}

	ret = i915_gem_object_pin(obj, 0, 1);
	if (ret) {
		goto out;
	}
	ret = i915_gem_object_set_to_gtt_domain(obj, 0, 1);
	if (ret)
		goto unpin;

	obj_priv = (struct inteldrm_obj *)obj;
	offset = obj_priv->gtt_offset + args->offset;

	bsize = round_page(offset + args->size) - trunc_page(offset);

	if ((ret = agp_map_subregion(dev_priv->agph,
	    trunc_page(offset), bsize, &bsh)) != 0)
		goto unpin;
	vaddr = bus_space_vaddr(dev->bst, bsh);
	if (vaddr == NULL) {
		ret = EFAULT;
		goto unmap;
	}

	ret = copyout(vaddr + (offset & PAGE_MASK),
	    (char *)(uintptr_t)args->data_ptr, args->size);

unmap:
	agp_unmap_subregion(dev_priv->agph, bsh, bsize);
unpin:
	i915_gem_object_unpin(obj);
out:
	drm_unhold_and_unref(obj);
	DRM_READUNLOCK();

	return (ret);
}

/**
 * Writes data to the object referenced by handle.
 *
 * On error, the contents of the buffer that were to be modified are undefined.
 */
int
i915_gem_pwrite_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_pwrite	*args = data;
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	char				*vaddr;
	bus_space_handle_t		 bsh;
	bus_size_t			 bsize;
	off_t				 offset;
	int				 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	DRM_READLOCK();
	drm_hold_object(obj);

	/* Bounds check destination. */
	if (args->offset > obj->size || args->size > obj->size ||
	    args->offset + args->size > obj->size) {
		ret = EINVAL;
		goto out;
	}

	ret = i915_gem_object_pin(obj, 0, 1);
	if (ret) {
		goto out;
	}
	ret = i915_gem_object_set_to_gtt_domain(obj, 1, 1);
	if (ret)
		goto unpin;

	obj_priv = (struct inteldrm_obj *)obj;
	offset = obj_priv->gtt_offset + args->offset;
	bsize = round_page(offset + args->size) - trunc_page(offset);

	if ((ret = agp_map_subregion(dev_priv->agph,
	    trunc_page(offset), bsize, &bsh)) != 0)
		goto unpin;
	vaddr = bus_space_vaddr(dev_priv->bst, bsh);
	if (vaddr == NULL) {
		ret = EFAULT;
		goto unmap;
	}

	ret = copyin((char *)(uintptr_t)args->data_ptr,
	    vaddr + (offset & PAGE_MASK), args->size);


unmap:
	agp_unmap_subregion(dev_priv->agph, bsh, bsize);
unpin:
	i915_gem_object_unpin(obj);
out:
	drm_unhold_and_unref(obj);
	DRM_READUNLOCK();

	return (ret);
}

/**
 * Called when user space prepares to use an object with the CPU, either through
 * the mmap ioctl's mapping or a GTT mapping.
 */
int
i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_i915_gem_set_domain	*args = data;
	struct drm_obj			*obj;
	u_int32_t			 read_domains = args->read_domains;
	u_int32_t			 write_domain = args->write_domain;
	int				 ret;

	/*
	 * Only handle setting domains to types we allow the cpu to see.
	 * while linux allows the CPU domain here, we only allow GTT since that
	 * is all that we let userland near.
	 * Also sanity check that having something in the write domain implies
	 * it's in the read domain, and only that read domain.
	 */
	if ((write_domain | read_domains)  & ~I915_GEM_DOMAIN_GTT ||
	    (write_domain != 0 && read_domains != write_domain))
		return (EINVAL);

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	drm_hold_object(obj);

	ret = i915_gem_object_set_to_gtt_domain(obj, write_domain != 0, 1);

	drm_unhold_and_unref(obj);
	/*
	 * Silently promote `you're not bound, there was nothing to do'
	 * to success, since the client was just asking us to make sure
	 * everything was done.
	 */
	return ((ret == EINVAL) ? 0 : ret);
}

// i915_gem_sw_finish_ioctl
// i915_gem_mmap_ioctl
// i915_gem_pager_ctor
// i915_gem_pager_fault
// i915_gem_pager_dtor
// struct cdev_pager_ops i915_gem_pager_ops = {

int
i915_gem_mmap_gtt(struct drm_file *file, struct drm_device *dev,
    uint32_t handle, uint64_t *mmap_offset)
{
	struct drm_obj			*obj;
	struct inteldrm_obj		*obj_priv;
	vaddr_t				 addr;
	voff_t				 offset; 
	vsize_t				 end, nsize;
	int				 ret;

	offset = (voff_t)mmap_offset;

	obj = drm_gem_object_lookup(dev, file, handle);
	if (obj == NULL)
		return (EBADF);

	/* Since we are doing purely uvm-related operations here we do
	 * not need to hold the object, a reference alone is sufficient
	 */
	obj_priv = (struct inteldrm_obj *)obj;

	/* Check size. Also ensure that the object is not purgeable */
	if (offset > obj->size || i915_obj_purgeable(obj_priv)) {
		ret = EINVAL;
		goto done;
	}

	end = round_page(offset + obj->size);
	offset = trunc_page(offset);
	nsize = end - offset;

	/*
	 * We give our reference from object_lookup to the mmap, so only
	 * must free it in the case that the map fails.
	 */
	addr = 0;
	ret = uvm_map(&curproc->p_vmspace->vm_map, &addr, nsize, &obj->uobj,
	    offset, 0, UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW,
	    UVM_INH_SHARE, UVM_ADV_RANDOM, 0));

done:
	if (ret == 0)
		*mmap_offset = (uint64_t)addr + (offset & PAGE_MASK);
	else
		drm_unref(&obj->uobj);

	return (ret);
}

int
i915_gem_mmap_gtt_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct inteldrm_softc *dev_priv;
	struct drm_i915_gem_mmap_gtt *args;

	dev_priv = dev->dev_private;
	args = data;

	return (i915_gem_mmap_gtt(file, dev, args->handle, &args->offset));
}

// i915_gem_alloc_object
// i915_gem_clflush_object

void
i915_gem_object_flush_cpu_write_domain(struct drm_obj *obj)
{
	printf("%s stub\n", __func__);
}

/*
 * Flush the GPU write domain for the object if dirty, then wait for the
 * rendering to complete. When this returns it is safe to unbind from the
 * GTT or access from the CPU.
 */
int
i915_gem_object_flush_gpu_write_domain(struct drm_obj *obj, int pipelined,
    int interruptible, int write)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	u_int32_t		 seqno;
	int			 ret = 0;

	DRM_ASSERT_HELD(obj);
	if ((obj->write_domain & I915_GEM_GPU_DOMAINS) != 0) {
		/*
		 * Queue the GPU write cache flushing we need.
		 * This call will move stuff form the flushing list to the
		 * active list so all we need to is wait for it.
		 */
		(void)i915_gem_flush(dev_priv, 0, obj->write_domain);
		KASSERT(obj->write_domain == 0);
	}

	/* wait for queued rendering so we know it's flushed and bo is idle */
	if (pipelined == 0 && inteldrm_is_active(obj_priv)) {
		if (write) {
			seqno = obj_priv->last_rendering_seqno;
		} else {
			seqno = obj_priv->last_write_seqno;
		}
		ret =  i915_wait_request(dev_priv, seqno, interruptible);
	}
	return (ret);
}

// i915_gem_object_flush_gtt_write_domain

/*
 * Moves a single object to the GTT and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to occur.
 */
int
i915_gem_object_set_to_gtt_domain(struct drm_obj *obj, int write,
    int interruptible)
{
	struct drm_device	*dev = (struct drm_device *)obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	/* Not valid to be called on unbound objects. */
	if (obj_priv->dmamap == NULL)
		return (EINVAL);

	/* Wait on any GPU rendering and flushing to occur. */
	if ((ret = i915_gem_object_flush_gpu_write_domain(obj, 0,
	    interruptible, write)) != 0)
		return (ret);

	if (obj->write_domain == I915_GEM_DOMAIN_CPU) {
		/* clflush the pages, and flush chipset cache */
		bus_dmamap_sync(dev_priv->agpdmat, obj_priv->dmamap, 0,
		    obj->size, BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
		inteldrm_chipset_flush(dev_priv);
		obj->write_domain = 0;
	}

	/* We're accessing through the gpu, so grab a new fence register or
	 * update the LRU.
	 */
	if (obj->do_flags & I915_FENCE_INVALID) {
		ret = i915_gem_object_put_fence_reg(obj, interruptible);
		if (ret)
			return (ret);
	}
	if (obj_priv->tiling_mode != I915_TILING_NONE)
		ret = i915_gem_get_fence_reg(obj, interruptible);

	/*
	 * If we're writing through the GTT domain then the CPU and GPU caches
	 * will need to be invalidated at next use.
	 * It should now be out of any other write domains and we can update
	 * to the correct ones
	 */
	KASSERT((obj->write_domain & ~I915_GEM_DOMAIN_GTT) == 0);
	if (write) {
		obj->read_domains = obj->write_domain = I915_GEM_DOMAIN_GTT;
		atomic_setbits_int(&obj->do_flags, I915_DIRTY);
	} else {
		obj->read_domains |= I915_GEM_DOMAIN_GTT;
	}

	return (ret);
}

// i915_gem_object_set_cache_level

int
i915_gem_object_set_cache_level(struct drm_obj *obj,
    enum i915_cache_level cache_level)
{
	printf("%s stub\n", __func__);
	return -1;
}

int
i915_gem_object_pin_to_display_plane(struct drm_obj *obj,
    u32 alignment, struct intel_ring_buffer *pipelined)
{
	u32 old_read_domains, old_write_domain;
	int ret;
	int interruptable = 1;

	ret = i915_gem_object_flush_gpu_write_domain(obj, pipelined != NULL,
	    interruptable, 0);
	if (ret != 0)
		return (ret);

#ifdef notyet
	if (pipelined != obj_priv->ring) {
#else
	if (1) {
#endif
		ret = i915_gem_object_wait_rendering(obj);
		if (ret == -ERESTART || ret == -EINTR)
			return (ret);
	}

	ret = i915_gem_object_set_cache_level(obj, I915_CACHE_NONE);
	if (ret != 0)
		return (ret);

	ret = i915_gem_object_pin(obj, alignment, true);
	if (ret != 0)
		return (ret);

	i915_gem_object_flush_cpu_write_domain(obj);

	old_write_domain = obj->write_domain;
	old_read_domains = obj->read_domains;

	KASSERT((obj->write_domain & ~I915_GEM_DOMAIN_GTT) == 0);
	obj->read_domains |= I915_GEM_DOMAIN_GTT;

#ifdef notyet
	CTR3(KTR_DRM, "object_change_domain pin_to_display_plan %p %x %x",
	    obj, old_read_domains, obj->write_domain);
#endif
	return (0);
}

// i915_gem_object_finish_gpu

/*
 * Moves a single object to the CPU read and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to return.
 */
int
i915_gem_object_set_to_cpu_domain(struct drm_obj *obj, int write,
    int interruptible)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	/* Wait on any GPU rendering and flushing to occur. */
	if ((ret = i915_gem_object_flush_gpu_write_domain(obj, 0,
	    interruptible, write)) != 0)
		return (ret);

	if (obj->write_domain == I915_GEM_DOMAIN_GTT ||
	    (write && obj->read_domains & I915_GEM_DOMAIN_GTT)) {
		/*
		 * No actual flushing is required for the GTT write domain.
		 * Writes to it immeditately go to main memory as far as we
		 * know, so there's no chipset flush. It also doesn't land
		 * in render cache.
		 */
		inteldrm_wipe_mappings(obj);
		if (obj->write_domain == I915_GEM_DOMAIN_GTT)
			obj->write_domain = 0;
	}

	/* remove the fence register since we're not using it anymore */
	if ((ret = i915_gem_object_put_fence_reg(obj, interruptible)) != 0)
		return (ret);

	/* Flush the CPU cache if it's still invalid. */
	if ((obj->read_domains & I915_GEM_DOMAIN_CPU) == 0) {
		bus_dmamap_sync(dev_priv->agpdmat, obj_priv->dmamap, 0,
		    obj->size, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		obj->read_domains |= I915_GEM_DOMAIN_CPU;
	}

	/*
	 * It should now be out of any other write domain, and we can update
	 * the domain value for our changes.
	 */
	KASSERT((obj->write_domain & ~I915_GEM_DOMAIN_CPU) == 0);

	/*
	 * If we're writing through the CPU, then the GPU read domains will
	 * need to be invalidated at next use.
	 */
	if (write)
		obj->read_domains = obj->write_domain = I915_GEM_DOMAIN_CPU;

	return (0);
}

// i915_gem_object_set_to_full_cpu_read_domain
// i915_gem_object_set_cpu_read_domain_range
// i915_gem_get_gtt_size

/*
 * return required GTT alignment for an object, taking into account potential
 * fence register needs
 */
bus_size_t
i915_gem_get_gtt_alignment(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	bus_size_t		 start, i;

	/*
	 * Minimum alignment is 4k (GTT page size), but fence registers may
	 * modify this
	 */
	if (IS_I965G(dev_priv) || obj_priv->tiling_mode == I915_TILING_NONE)
		return (4096);

	/*
	 * Older chips need to be aligned to the size of the smallest fence
	 * register that can contain the object.
	 */
	if (IS_I9XX(dev_priv))
		start = 1024 * 1024;
	else
		start = 512 * 1024;

	for (i = start; i < obj->size; i <<= 1)
		;

	return (i);
}

// i915_gem_get_unfenced_gtt_alignment

/**
 * Finds free space in the GTT aperture and binds the object there.
 */
int
i915_gem_object_bind_to_gtt(struct drm_obj *obj, bus_size_t alignment,
    int interruptible)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	if (dev_priv->agpdmat == NULL)
		return (EINVAL);
	if (alignment == 0) {
		alignment = i915_gem_get_gtt_alignment(obj);
	} else if (alignment & (i915_gem_get_gtt_alignment(obj) - 1)) {
		DRM_ERROR("Invalid object alignment requested %u\n", alignment);
		return (EINVAL);
	}

	/* Can't bind a purgeable buffer */
	if (i915_obj_purgeable(obj_priv)) {
		printf("tried to bind purgeable buffer");
		return (EINVAL);
	}

	if ((ret = bus_dmamap_create(dev_priv->agpdmat, obj->size, 1,
	    obj->size, 0, BUS_DMA_WAITOK, &obj_priv->dmamap)) != 0) {
		DRM_ERROR("Failed to create dmamap\n");
		return (ret);
	}
	agp_bus_dma_set_alignment(dev_priv->agpdmat, obj_priv->dmamap,
	    alignment);

 search_free:
	/*
	 * the helper function wires the uao then binds it to the aperture for
	 * us, so all we have to do is set up the dmamap then load it.
	 */
	ret = drm_gem_load_uao(dev_priv->agpdmat, obj_priv->dmamap, obj->uao,
	    obj->size, BUS_DMA_WAITOK | obj_priv->dma_flags,
	    &obj_priv->dma_segs);
	/* XXX NOWAIT? */
	if (ret != 0) {
		/* If the gtt is empty and we're still having trouble
		 * fitting our object in, we're out of memory.
		 */
		if (TAILQ_EMPTY(&dev_priv->mm.inactive_list) &&
		    TAILQ_EMPTY(&dev_priv->mm.flushing_list) &&
		    TAILQ_EMPTY(&dev_priv->mm.active_list)) {
			DRM_ERROR("GTT full, but LRU list empty\n");
			goto error;
		}

		ret = i915_gem_evict_something(dev_priv, obj->size,
		    interruptible);
		if (ret != 0)
			goto error;
		goto search_free;
	}
	i915_gem_bit_17_swizzle(obj);

	obj_priv->gtt_offset = obj_priv->dmamap->dm_segs[0].ds_addr -
	    dev->agp->base;

	atomic_inc(&dev->gtt_count);
	atomic_add(obj->size, &dev->gtt_memory);

	/* Assert that the object is not currently in any GPU domain. As it
	 * wasn't in the GTT, there shouldn't be any way it could have been in
	 * a GPU cache
	 */
	KASSERT((obj->read_domains & I915_GEM_GPU_DOMAINS) == 0);
	KASSERT((obj->write_domain & I915_GEM_GPU_DOMAINS) == 0);

	return (0);

error:
	bus_dmamap_destroy(dev_priv->agpdmat, obj_priv->dmamap);
	obj_priv->dmamap = NULL;
	obj_priv->gtt_offset = 0;
	return (ret);
}

// i915_gem_object_finish_gtt

/**
 * Unbinds an object from the GTT aperture.
 *
 * XXX track dirty and pass down to uvm (note, DONTNEED buffers are clean).
 */
int
i915_gem_object_unbind(struct drm_obj *obj, int interruptible)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 ret = 0;

	DRM_ASSERT_HELD(obj);
	/*
	 * if it's already unbound, or we've already done lastclose, just
	 * let it happen. XXX does this fail to unwire?
	 */
	if (obj_priv->dmamap == NULL || dev_priv->agpdmat == NULL)
		return 0;

	if (obj_priv->pin_count != 0) {
		DRM_ERROR("Attempting to unbind pinned buffer\n");
		return (EINVAL);
	}

	KASSERT(!i915_obj_purged(obj_priv));

	/* Move the object to the CPU domain to ensure that
	 * any possible CPU writes while it's not in the GTT
	 * are flushed when we go to remap it. This will
	 * also ensure that all pending GPU writes are finished
	 * before we unbind.
	 */
	ret = i915_gem_object_set_to_cpu_domain(obj, 1, interruptible);
	if (ret)
		return ret;

	KASSERT(!inteldrm_is_active(obj_priv));

	/* if it's purgeable don't bother dirtying the pages */
	if (i915_obj_purgeable(obj_priv))
		atomic_clearbits_int(&obj->do_flags, I915_DIRTY);
	/*
	 * unload the map, then unwire the backing object.
	 */
	i915_gem_save_bit_17_swizzle(obj);
	bus_dmamap_unload(dev_priv->agpdmat, obj_priv->dmamap);
	uvm_objunwire(obj->uao, 0, obj->size);
	/* XXX persistent dmamap worth the memory? */
	bus_dmamap_destroy(dev_priv->agpdmat, obj_priv->dmamap);
	obj_priv->dmamap = NULL;
	free(obj_priv->dma_segs, M_DRM);
	obj_priv->dma_segs = NULL;
	/* XXX this should change whether we tell uvm the page is dirty */
	atomic_clearbits_int(&obj->do_flags, I915_DIRTY);

	obj_priv->gtt_offset = 0;
	atomic_dec(&dev->gtt_count);
	atomic_sub(obj->size, &dev->gtt_memory);

	/* Remove ourselves from any LRU list if present. */
	i915_list_remove((struct inteldrm_obj *)obj);

	if (i915_obj_purgeable(obj_priv))
		inteldrm_purge_obj(obj);

	return (0);
}

// i915_gem_object_get_pages_gtt
// i915_gem_assert_pages_not_mapped
// i915_gem_object_put_pages_gtt
// i915_gem_release_mmap

int
i915_gem_object_wait_rendering(struct drm_obj *obj)
{
	printf("%s stub\n", __func__);
	return -1;
}

/* called locked */
void
i915_gem_object_move_to_active(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct inteldrm_fence	*reg;
	u_int32_t		 seqno = dev_priv->mm.next_gem_seqno;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);
	MUTEX_ASSERT_LOCKED(&dev_priv->list_lock);

	/* Add a reference if we're newly entering the active list. */
	if (!inteldrm_is_active(obj_priv)) {
		drm_ref(&obj->uobj);
		atomic_setbits_int(&obj->do_flags, I915_ACTIVE);
	}

	if (inteldrm_needs_fence(obj_priv)) {
		reg = &dev_priv->fence_regs[obj_priv->fence_reg];
		reg->last_rendering_seqno = seqno;
	}
	if (obj->write_domain)
		obj_priv->last_write_seqno = seqno;

	/* Move from whatever list we were on to the tail of execution. */
	i915_move_to_tail(obj_priv, &dev_priv->mm.active_list);
	obj_priv->last_rendering_seqno = seqno;
}

void
i915_gem_object_move_off_active(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct inteldrm_fence	*reg;

	MUTEX_ASSERT_LOCKED(&dev_priv->list_lock);
	DRM_OBJ_ASSERT_LOCKED(obj);

	obj_priv->last_rendering_seqno = 0;
	/* if we have a fence register, then reset the seqno */
	if (obj_priv->fence_reg != I915_FENCE_REG_NONE) {
		reg = &dev_priv->fence_regs[obj_priv->fence_reg];
		reg->last_rendering_seqno = 0;
	}
	if (obj->write_domain == 0)
		obj_priv->last_write_seqno = 0;
}

// i915_gem_object_move_to_flushing

/* If you call this on an object that you have held, you must have your own
 * reference, not just the reference from the active list.
 */
void
i915_gem_object_move_to_inactive(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;

	mtx_enter(&dev_priv->list_lock);
	drm_lock_obj(obj);
	/* unlocks list lock and object lock */
	i915_gem_object_move_to_inactive_locked(obj);
}

/* called locked */
void
i915_gem_object_move_to_inactive_locked(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	MUTEX_ASSERT_LOCKED(&dev_priv->list_lock);
	DRM_OBJ_ASSERT_LOCKED(obj);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	if (obj_priv->pin_count != 0)
		i915_list_remove(obj_priv);
	else
		i915_move_to_tail(obj_priv, &dev_priv->mm.inactive_list);

	i915_gem_object_move_off_active(obj);
	atomic_clearbits_int(&obj->do_flags, I915_FENCED_EXEC);

	KASSERT((obj->do_flags & I915_GPU_WRITE) == 0);
	/* unlock because this unref could recurse */
	mtx_leave(&dev_priv->list_lock);
	if (inteldrm_is_active(obj_priv)) {
		atomic_clearbits_int(&obj->do_flags,
		    I915_ACTIVE);
		drm_unref_locked(&obj->uobj);
	} else {
		drm_unlock_obj(obj);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
}

// i915_gem_object_truncate
// i915_gem_object_is_purgeable
// i915_gem_process_flushing_list
// i915_gem_object_needs_bit17_swizzle
// i915_gem_wire_page
// i915_gem_flush_ring
// i915_ring_idle
// i915_gpu_idle

/**
 * Waits for a sequence number to be signaled, and cleans up the
 * request and object lists appropriately for that event.
 *
 * Called locked, sleeps with it.
 */
int
i915_wait_request(struct inteldrm_softc *dev_priv, uint32_t seqno,
    int interruptible)
{
	int ret = 0;

	/* Check first because poking a wedged chip is bad. */
	if (dev_priv->mm.wedged)
		return (EIO);

	if (seqno == dev_priv->mm.next_gem_seqno) {
		mtx_enter(&dev_priv->request_lock);
		seqno = i915_add_request(dev_priv);
		mtx_leave(&dev_priv->request_lock);
		if (seqno == 0)
			return (ENOMEM);
	}

	if (!i915_seqno_passed(i915_get_gem_seqno(dev_priv), seqno)) {
		mtx_enter(&dev_priv->user_irq_lock);
		i915_user_irq_get(dev_priv);
		while (ret == 0) {
			if (i915_seqno_passed(i915_get_gem_seqno(dev_priv),
			    seqno) || dev_priv->mm.wedged)
				break;
			ret = msleep(dev_priv, &dev_priv->user_irq_lock,
			    PZERO | (interruptible ? PCATCH : 0), "gemwt", 0);
		}
		i915_user_irq_put(dev_priv);
		mtx_leave(&dev_priv->user_irq_lock);
	}
	if (dev_priv->mm.wedged)
		ret = EIO;

	/* Directly dispatch request retiring.  While we have the work queue
	 * to handle this, the waiter on a request often wants an associated
	 * buffer to have made it to the inactive list, and we would need
	 * a separate wait queue to handle that.
	 */
	if (ret == 0)
		i915_gem_retire_requests(dev_priv);

	return (ret);
}

// i915_gem_get_seqno
// i915_gem_next_request_seqno

/**
 * Creates a new sequence number, emitting a write of it to the status page
 * plus an interrupt, which will trigger and interrupt if they are currently
 * enabled.
 *
 * Must be called with struct_lock held.
 *
 * Returned sequence numbers are nonzero on success.
 */
uint32_t
i915_add_request(struct inteldrm_softc *dev_priv)
{
	struct inteldrm_request	*request;
	uint32_t		 seqno;
	int			 was_empty;

	MUTEX_ASSERT_LOCKED(&dev_priv->request_lock);

	request = drm_calloc(1, sizeof(*request));
	if (request == NULL) {
		printf("%s: failed to allocate request\n", __func__);
		return 0;
	}

	/* Grab the seqno we're going to make this request be, and bump the
	 * next (skipping 0 so it can be the reserved no-seqno value).
	 */
	seqno = dev_priv->mm.next_gem_seqno;
	dev_priv->mm.next_gem_seqno++;
	if (dev_priv->mm.next_gem_seqno == 0)
		dev_priv->mm.next_gem_seqno++;

	if (IS_GEN6(dev_priv) || IS_GEN7(dev_priv))
		BEGIN_LP_RING(10);
	else 
		BEGIN_LP_RING(4);

	OUT_RING(MI_STORE_DWORD_INDEX);
	OUT_RING(I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	OUT_RING(seqno);
	OUT_RING(MI_USER_INTERRUPT);
	ADVANCE_LP_RING();

	DRM_DEBUG("%d\n", seqno);

	/* XXX request timing for throttle */
	request->seqno = seqno;
	was_empty = TAILQ_EMPTY(&dev_priv->mm.request_list);
	TAILQ_INSERT_TAIL(&dev_priv->mm.request_list, request, list);

	if (dev_priv->mm.suspended == 0) {
		if (was_empty)
			timeout_add_sec(&dev_priv->mm.retire_timer, 1);
		/* XXX was_empty? */
		timeout_add_msec(&dev_priv->mm.hang_timer, 750);
	}
	return seqno;
}

// i915_gem_request_remove_from_client
// i915_gem_release
// i915_gem_reset_ring_lists
// i915_gem_reset_fences
// i915_gem_reset
// i915_gem_retire_requests_ring

/**
 * This function clears the request list as sequence numbers are passed.
 */
void
i915_gem_retire_requests(struct inteldrm_softc *dev_priv)
{
	struct inteldrm_request	*request;
	uint32_t		 seqno;

	if (dev_priv->hw_status_page == NULL)
		return;

	seqno = i915_get_gem_seqno(dev_priv);

	mtx_enter(&dev_priv->request_lock);
	while ((request = TAILQ_FIRST(&dev_priv->mm.request_list)) != NULL) {
		if (i915_seqno_passed(seqno, request->seqno) ||
		    dev_priv->mm.wedged) {
			TAILQ_REMOVE(&dev_priv->mm.request_list, request, list);
			i915_gem_retire_request(dev_priv, request);
			mtx_leave(&dev_priv->request_lock);

			drm_free(request);
			mtx_enter(&dev_priv->request_lock);
		} else
			break;
	}
	mtx_leave(&dev_priv->request_lock);
}

void
sandybridge_write_fence_reg(struct inteldrm_fence *reg)
{
	struct drm_obj		*obj = reg->obj;
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 regnum = obj_priv->fence_reg;
	u_int64_t		 val;

	val = (uint64_t)((obj_priv->gtt_offset + obj->size - 4096) &
		    0xfffff000) << 32;
	val |= obj_priv->gtt_offset & 0xfffff000;
	val |= (uint64_t)((obj_priv->stride / 128) - 1) <<
		    SANDYBRIDGE_FENCE_PITCH_SHIFT;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I965_FENCE_TILING_Y_SHIFT;
	val |= I965_FENCE_REG_VALID;

	I915_WRITE64(FENCE_REG_SANDYBRIDGE_0 + (regnum * 8), val);
}

void
i965_write_fence_reg(struct inteldrm_fence *reg)
{
	struct drm_obj		*obj = reg->obj;
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 regnum = obj_priv->fence_reg;
	u_int64_t		 val;

	val = (uint64_t)((obj_priv->gtt_offset + obj->size - 4096) &
		    0xfffff000) << 32;
	val |= obj_priv->gtt_offset & 0xfffff000;
	val |= ((obj_priv->stride / 128) - 1) << I965_FENCE_PITCH_SHIFT;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I965_FENCE_TILING_Y_SHIFT;
	val |= I965_FENCE_REG_VALID;

	I915_WRITE64(FENCE_REG_965_0 + (regnum * 8), val);
}

void
i915_write_fence_reg(struct inteldrm_fence *reg)
{
	struct drm_obj		*obj = reg->obj;
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	bus_size_t		 fence_reg;
	u_int32_t		 val;
	u_int32_t		 pitch_val;
	int			 regnum = obj_priv->fence_reg;
	int			 tile_width;

	if ((obj_priv->gtt_offset & ~I915_FENCE_START_MASK) ||
	    (obj_priv->gtt_offset & (obj->size - 1))) {
		DRM_ERROR("object 0x%lx not 1M or size (0x%zx) aligned\n",
		     obj_priv->gtt_offset, obj->size);
		return;
	}

	if (obj_priv->tiling_mode == I915_TILING_Y &&
	    HAS_128_BYTE_Y_TILING(dev_priv))
		tile_width = 128;
	else
		tile_width = 512;

	/* Note: pitch better be a power of two tile widths */
	pitch_val = obj_priv->stride / tile_width;
	pitch_val = ffs(pitch_val) - 1;

	/* XXX print more */
	if ((obj_priv->tiling_mode == I915_TILING_Y &&
	    HAS_128_BYTE_Y_TILING(dev_priv) &&
	    pitch_val > I830_FENCE_MAX_PITCH_VAL) ||
	    pitch_val > I915_FENCE_MAX_PITCH_VAL)
		printf("%s: invalid pitch provided", __func__);

	val = obj_priv->gtt_offset;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I830_FENCE_TILING_Y_SHIFT;
	val |= I915_FENCE_SIZE_BITS(obj->size);
	val |= pitch_val << I830_FENCE_PITCH_SHIFT;
	val |= I830_FENCE_REG_VALID;

	if (regnum < 8)
		fence_reg = FENCE_REG_830_0 + (regnum * 4);
	else
		fence_reg = FENCE_REG_945_8 + ((regnum - 8) * 4);
	I915_WRITE(fence_reg, val);
}

void
i830_write_fence_reg(struct inteldrm_fence *reg)
{
	struct drm_obj		*obj = reg->obj;
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	int			 regnum = obj_priv->fence_reg;
	u_int32_t		 pitch_val, val;

	if ((obj_priv->gtt_offset & ~I830_FENCE_START_MASK) ||
	    (obj_priv->gtt_offset & (obj->size - 1))) {
		DRM_ERROR("object 0x%08x not 512K or size aligned 0x%lx\n",
		     obj_priv->gtt_offset, obj->size);
		return;
	}

	pitch_val = ffs(obj_priv->stride / 128) - 1;

	val = obj_priv->gtt_offset;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I830_FENCE_TILING_Y_SHIFT;
	val |= I830_FENCE_SIZE_BITS(obj->size);
	val |= pitch_val << I830_FENCE_PITCH_SHIFT;
	val |= I830_FENCE_REG_VALID;

	I915_WRITE(FENCE_REG_830_0 + (regnum * 4), val);

}

// static bool ring_passed_seqno
// i915_gem_object_flush_fence

int
i915_gem_object_put_fence_reg(struct drm_obj *obj, int interruptible)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;
	struct inteldrm_fence	*reg;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	if (obj_priv->fence_reg == I915_FENCE_REG_NONE)
		return (0);

	/*
	 * If the last execbuffer we did on the object needed a fence then
	 * we must emit a flush.
	 */
	if (inteldrm_needs_fence(obj_priv)) {
		ret = i915_gem_object_flush_gpu_write_domain(obj, 1,
		    interruptible, 0);
		if (ret != 0)
			return (ret);
	}

	/* if rendering is queued up that depends on the fence, wait for it */
	reg = &dev_priv->fence_regs[obj_priv->fence_reg];
	if (reg->last_rendering_seqno != 0) {
		ret = i915_wait_request(dev_priv, reg->last_rendering_seqno,
		    interruptible);
		if (ret != 0)
			return (ret);
	}

	/* tiling changed, must wipe userspace mappings */
	if ((obj->write_domain | obj->read_domains) & I915_GEM_DOMAIN_GTT) {
		inteldrm_wipe_mappings(obj);
		if (obj->write_domain == I915_GEM_DOMAIN_GTT)
			obj->write_domain = 0;
	}

	mtx_enter(&dev_priv->fence_lock);
	if (IS_I965G(dev_priv)) {
		I915_WRITE64(FENCE_REG_965_0 + (obj_priv->fence_reg * 8), 0);
	} else {
		u_int32_t fence_reg;

		if (obj_priv->fence_reg < 8)
			fence_reg = FENCE_REG_830_0 + obj_priv->fence_reg * 4;
		else
			fence_reg = FENCE_REG_945_8 +
			    (obj_priv->fence_reg - 8) * 4;
		I915_WRITE(fence_reg , 0);
	}

	reg->obj = NULL;
	TAILQ_REMOVE(&dev_priv->mm.fence_list, reg, list);
	obj_priv->fence_reg = I915_FENCE_REG_NONE;
	mtx_leave(&dev_priv->fence_lock);
	atomic_clearbits_int(&obj->do_flags, I915_FENCE_INVALID);

	return (0);
}

// i915_find_fence_reg

int
i915_gem_object_get_fence(struct drm_obj *obj,
    struct intel_ring_buffer *pipelined)
{
	return (i915_gem_get_fence_reg(obj, 1));
}

// i915_gem_clear_fence_reg

int
i915_gem_init_object(struct drm_obj *obj)
{
	struct inteldrm_obj	*obj_priv = (struct inteldrm_obj *)obj;

	/*
	 * We've just allocated pages from the kernel,
	 * so they've just been written by the CPU with
	 * zeros. They'll need to be flushed before we
	 * use them with the GPU.
	 */
	obj->write_domain = I915_GEM_DOMAIN_CPU;
	obj->read_domains = I915_GEM_DOMAIN_CPU;

	/* normal objects don't need special treatment */
	obj_priv->dma_flags = 0;
	obj_priv->fence_reg = I915_FENCE_REG_NONE;

	return 0;
}

// i915_gem_object_is_inactive
// i915_gem_retire_task_handler
// i915_gem_lastclose
// i915_gem_init_phys_object
// i915_gem_free_phys_object
// i915_gem_free_all_phys_object
// i915_gem_detach_phys_object
// i915_gem_attach_phys_object
// i915_gem_phys_pwrite
// i915_gpu_is_active
// i915_gem_lowmem
// i915_gem_unload


