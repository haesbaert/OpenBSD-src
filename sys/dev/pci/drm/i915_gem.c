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

int i915_gem_init_phys_object(struct drm_device *, int, int, int);
int i915_gem_phys_pwrite(struct drm_device *, struct drm_i915_gem_object *,
			 struct drm_i915_gem_pwrite *, struct drm_file *);
bool intel_enable_blt(struct drm_device *);
int i915_gem_handle_seqno_wrap(struct drm_device *);
void i915_gem_object_update_fence(struct drm_i915_gem_object *,
    struct drm_i915_fence_reg *, bool);
int i915_gem_object_flush_fence(struct drm_i915_gem_object *);
struct drm_i915_fence_reg *i915_find_fence_reg(struct drm_device *);

static inline void
i915_gem_object_fence_lost(struct drm_i915_gem_object *obj)
{
#ifdef notyet
	if (obj->tiling_mode)
		i915_gem_release_mmap(obj);
#endif

	/* As we do not have an associated fence register, we will force
	 * a tiling change if we ever need to acquire one.
	 */
	obj->fence_dirty = false;
	obj->fence_reg = I915_FENCE_REG_NONE;
}

// i915_gem_info_add_obj
// i915_gem_info_remove_obj
// i915_gem_wait_for_error
// i915_mutex_lock_interruptible
// i915_gem_object_is_inactive

static inline bool
i915_gem_object_is_inactive(struct drm_i915_gem_object *obj)
{
	return obj->dmamap && !obj->active && obj->pin_count == 0;
}

int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_init	*args = data;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return (ENODEV);

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
i915_gem_create(struct drm_file *file, struct drm_device *dev, uint64_t size,
    uint32_t *handle_p)
{
	struct drm_i915_gem_object *obj;
	uint32_t handle;
	int ret;

	size = round_page(size);
	if (size == 0)
		return (-EINVAL);

	obj = i915_gem_alloc_object(dev, size);
	if (obj == NULL)
		return (-ENOMEM);

	handle = 0;
	ret = drm_handle_create(file, &obj->base, &handle);
	if (ret != 0) {
		drm_unref(&obj->base.uobj);
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
	struct drm_i915_gem_object	*obj;
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
	obj = i915_gem_alloc_object(dev, args->size);
	if (obj == NULL)
		return (ENOMEM);

	/* we give our reference to the handle */
	ret = drm_handle_create(file_priv, &obj->base, &handle);

	if (ret == 0)
		args->handle = handle;
	else
		drm_unref(&obj->base.uobj);

	return (ret);
}

// i915_gem_object_needs_bit17_swizzle
// __copy_to_user_swizzled
// __copy_from_user_swizzled
// shmem_pread_fast
// shmem_clflush_swizzled_range
// shmem_pread_slow
// i915_gem_shmem_pread

/**
 * Reads data from the object referenced by handle.
 *
 * On error, the contents of *data are undefined.
 */
int
i915_gem_pread_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_pread	*args = data;
	struct drm_i915_gem_object	*obj;
	char				*vaddr;
	bus_space_handle_t		 bsh;
	bus_size_t			 bsize;
	voff_t				 offset;
	int				 ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (obj == NULL)
		return (EBADF);
	DRM_READLOCK();
	drm_hold_object(&obj->base);

	/*
	 * Bounds check source.
	 */
	if (args->offset > obj->base.size || args->size > obj->base.size ||
	    args->offset + args->size > obj->base.size) {
		ret = EINVAL;
		goto out;
	}

	ret = i915_gem_object_pin(obj, 0, 1);
	if (ret) {
		goto out;
	}
	ret = i915_gem_object_set_to_gtt_domain(obj, 0);
	if (ret)
		goto unpin;

	offset = obj->gtt_offset + args->offset;
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
	drm_unhold_and_unref(&obj->base);
	DRM_READUNLOCK();

	return (ret);
}

// fast_user_write
// i915_gem_gtt_pwrite_fast
// shmem_pwrite_fast
// shmem_pwrite_slow
// i915_gem_shmem_pwrite

/**
 * Writes data to the object referenced by handle.
 *
 * On error, the contents of the buffer that were to be modified are undefined.
 */
int
i915_gem_pwrite_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_pwrite	*args = data;
	struct drm_i915_gem_object	*obj;
	char				*vaddr;
	bus_space_handle_t		 bsh;
	bus_size_t			 bsize;
	off_t				 offset;
	int				 ret = 0;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (obj == NULL)
		return (EBADF);
	DRM_READLOCK();
	drm_hold_object(&obj->base);

	/* Bounds check destination. */
	if (args->offset > obj->base.size || args->size > obj->base.size ||
	    args->offset + args->size > obj->base.size) {
		ret = EINVAL;
		goto out;
	}

	if (obj->phys_obj) {
		ret = i915_gem_phys_pwrite(dev, obj, args, file);
		goto out;
	}

	ret = i915_gem_object_pin(obj, 0, 1);
	if (ret) {
		goto out;
	}
	ret = i915_gem_object_set_to_gtt_domain(obj, 1);
	if (ret)
		goto unpin;

	offset = obj->gtt_offset + args->offset;
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
	drm_unhold_and_unref(&obj->base);
	DRM_READUNLOCK();

	return (ret);
}

int
i915_gem_check_wedge(struct inteldrm_softc *dev_priv,
		     bool interruptible)
{
	if (dev_priv->mm.wedged)
		return (EIO);
	return 0;
}

// i915_gem_check_olr
// __wait_seqno

/**
 * Waits for a sequence number to be signaled, and cleans up the
 * request and object lists appropriately for that event.
 *
 * Called locked, sleeps with it.
 */
int
i915_wait_seqno(struct intel_ring_buffer *ring, uint32_t seqno)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	bool interruptible = dev_priv->mm.interruptible;
	int ret = 0;

	/* Check first because poking a wedged chip is bad. */
	if (dev_priv->mm.wedged)
		return (EIO);

	if (seqno == ring->outstanding_lazy_request) {
		ret = i915_add_request(ring, NULL, &seqno);
		if (ret)
			return (ret);
	}

	if (!i915_seqno_passed(ring->get_seqno(ring, true), seqno)) {
		mtx_enter(&dev_priv->irq_lock);
		ring->irq_get(ring);
		while (ret == 0) {
			if (i915_seqno_passed(ring->get_seqno(ring, false),
			    seqno) || dev_priv->mm.wedged)
				break;
			ret = msleep(dev_priv, &dev_priv->irq_lock,
			    PZERO | (interruptible ? PCATCH : 0), "gemwt", 0);
		}
		ring->irq_put(ring);
		mtx_leave(&dev_priv->irq_lock);
	}
	if (dev_priv->mm.wedged)
		ret = EIO;

	/* Directly dispatch request retiring.  While we have the work queue
	 * to handle this, the waiter on a request often wants an associated
	 * buffer to have made it to the inactive list, and we would need
	 * a separate wait queue to handle that.
	 */
	if (ret == 0)
		i915_gem_retire_requests_ring(ring);

	return (ret);
}

int
i915_gem_object_wait_rendering(struct drm_i915_gem_object *obj,
			       bool readonly)
{
	struct intel_ring_buffer *ring = obj->ring;
	u32 seqno;
	int ret;

	seqno = readonly ? obj->last_write_seqno : obj->last_read_seqno;
	if (seqno == 0)
		return 0;

	ret = i915_wait_seqno(ring, seqno);
	if (ret)
		return ret;

	i915_gem_retire_requests_ring(ring);

	/* Manually manage the write flush as we may have not yet
	 * retired the buffer.
	 */
	if (obj->last_write_seqno &&
	    i915_seqno_passed(seqno, obj->last_write_seqno)) {
		obj->last_write_seqno = 0;
		obj->base.write_domain &= ~I915_GEM_GPU_DOMAINS;
	}

	return 0;
}

// i915_gem_object_wait_rendering__nonblocking

/**
 * Called when user space prepares to use an object with the CPU, either through
 * the mmap ioctl's mapping or a GTT mapping.
 */
int
i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file)
{
	struct drm_i915_gem_set_domain*args = data;
	struct drm_i915_gem_object *obj;
	uint32_t read_domains = args->read_domains;
	uint32_t write_domain = args->write_domain;
	int ret;

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

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (obj == NULL)
		return (EBADF);
	drm_hold_object(&obj->base);

	ret = i915_gem_object_set_to_gtt_domain(obj, write_domain != 0);

	drm_unhold_and_unref(&obj->base);
	/*
	 * Silently promote `you're not bound, there was nothing to do'
	 * to success, since the client was just asking us to make sure
	 * everything was done.
	 */
	return ((ret == EINVAL) ? 0 : ret);
}

// i915_gem_sw_finish_ioctl
// i915_gem_mmap_ioctl

int
i915_gem_fault(struct drm_obj *gem_obj, struct uvm_faultinfo *ufi,
    off_t offset, vaddr_t vaddr, vm_page_t *pps, int npages, int centeridx,
    vm_prot_t access_type, int flags)
{
	struct drm_i915_gem_object *obj = to_intel_bo(gem_obj);
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	paddr_t paddr;
	int lcv, ret;
	int write = !!(access_type & VM_PROT_WRITE);
	vm_prot_t mapprot;
	boolean_t locked = TRUE;

	/* Are we about to suspend?, if so wait until we're done */
	if (dev_priv->sc_flags & INTELDRM_QUIET) {
		/* we're about to sleep, unlock the map etc */
		uvmfault_unlockall(ufi, NULL, &obj->base.uobj, NULL);
		while (dev_priv->sc_flags & INTELDRM_QUIET)
			tsleep(&dev_priv->flags, 0, "intelflt", 0);
		dev_priv->entries++;
		/*
		 * relock so we're in the same state we would be in if we
		 * were not quiesced before
		 */
		locked = uvmfault_relock(ufi);
		if (locked) {
			drm_lock_obj(&obj->base);
		} else {
			dev_priv->entries--;
			if (dev_priv->sc_flags & INTELDRM_QUIET)
				wakeup(&dev_priv->entries);
			return (VM_PAGER_REFAULT);
		}
	} else {
		dev_priv->entries++;
	}

	if (rw_enter(&dev->dev_lock, RW_NOSLEEP | RW_READ) != 0) {
		uvmfault_unlockall(ufi, NULL, &obj->base.uobj, NULL);
		DRM_READLOCK();
		locked = uvmfault_relock(ufi);
		if (locked)
			drm_lock_obj(&obj->base);
	}
	if (locked)
		drm_hold_object_locked(&obj->base);
	else { /* obj already unlocked */
		dev_priv->entries--;
		if (dev_priv->sc_flags & INTELDRM_QUIET)
			wakeup(&dev_priv->entries);
		return (VM_PAGER_REFAULT);
	}

	/* we have a hold set on the object now, we can unlock so that we can
	 * sleep in binding and flushing.
	 */
	drm_unlock_obj(&obj->base);

	if (obj->dmamap != NULL &&
	    (obj->gtt_offset & (i915_gem_get_gtt_alignment(&obj->base) - 1) ||
	    (!i915_gem_object_fence_ok(&obj->base, obj->tiling_mode)))) {
		/*
		 * pinned objects are defined to have a sane alignment which can
		 * not change.
		 */
		KASSERT(obj->pin_count == 0);
		if ((ret = i915_gem_object_unbind(obj)))
			goto error;
	}

	if (obj->dmamap == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj, 0);
		if (ret) {
			printf("%s: failed to bind\n", __func__);
			goto error;
		}
		i915_gem_object_move_to_inactive(obj);
	}

	/*
	 * We could only do this on bind so allow for map_buffer_range
	 * unsynchronised objects (where buffer suballocation
	 * is done by the GL application), however it gives coherency problems
	 * normally.
	 */
	ret = i915_gem_object_set_to_gtt_domain(obj, write);
	if (ret) {
		panic("%s: failed to set to gtt (%d)\n",
		    __func__, ret);
		goto error;
	}

	mapprot = ufi->entry->protection;
	/*
	 * if it's only a read fault, we only put ourselves into the gtt
	 * read domain, so make sure we fault again and set ourselves to write.
	 * this prevents us needing userland to do domain management and get
	 * it wrong, and makes us fully coherent with the gpu re mmap.
	 */
	if (write == 0)
		mapprot &= ~VM_PROT_WRITE;
	/* XXX try and  be more efficient when we do this */
	for (lcv = 0 ; lcv < npages ; lcv++, offset += PAGE_SIZE,
	    vaddr += PAGE_SIZE) {
		if ((flags & PGO_ALLPAGES) == 0 && lcv != centeridx)
			continue;

		if (pps[lcv] == PGO_DONTCARE)
			continue;

		paddr = dev->agp->base + obj->gtt_offset + offset;

		if (pmap_enter(ufi->orig_map->pmap, vaddr, paddr,
		    mapprot, PMAP_CANFAIL | mapprot) != 0) {
			drm_unhold_object(&obj->base);
			uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap,
			    NULL, NULL);
			DRM_READUNLOCK();
			dev_priv->entries--;
			if (dev_priv->sc_flags & INTELDRM_QUIET)
				wakeup(&dev_priv->entries);
			uvm_wait("intelflt");
			return (VM_PAGER_REFAULT);
		}
	}
error:
	drm_unhold_object(&obj->base);
	uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, NULL, NULL);
	DRM_READUNLOCK();
	dev_priv->entries--;
	if (dev_priv->sc_flags & INTELDRM_QUIET)
		wakeup(&dev_priv->entries);
	pmap_update(ufi->orig_map->pmap);
	if (ret == EIO) {
		/*
		 * EIO means we're wedged, so upon resetting the gpu we'll
		 * be alright and can refault. XXX only on resettable chips.
		 */
		ret = VM_PAGER_REFAULT;
	} else if (ret) {
		ret = VM_PAGER_ERROR;
	} else {
		ret = VM_PAGER_OK;
	}
	return (ret);
}

// i915_gem_release_mmap
// i915_gem_get_gtt_size

/*
 * return required GTT alignment for an object, taking into account potential
 * fence register needs
 */
bus_size_t
i915_gem_get_gtt_alignment(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = to_intel_bo(obj);
	bus_size_t		 start, i;

	/*
	 * Minimum alignment is 4k (GTT page size), but fence registers may
	 * modify this
	 */
	if (INTEL_INFO(dev)->gen >= 4 ||
	    obj_priv->tiling_mode == I915_TILING_NONE)
		return (4096);

	/*
	 * Older chips need to be aligned to the size of the smallest fence
	 * register that can contain the object.
	 */
	if (IS_I9XX(dev))
		start = 1024 * 1024;
	else
		start = 512 * 1024;

	for (i = start; i < obj->size; i <<= 1)
		;

	return (i);
}

// i915_gem_get_unfenced_gtt_alignment
// i915_gem_object_create_mmap_offset
// i915_gem_object_free_mmap_offset

int
i915_gem_mmap_gtt(struct drm_file *file, struct drm_device *dev,
    uint32_t handle, uint64_t *mmap_offset)
{
	struct drm_i915_gem_object	*obj;
	struct drm_local_map		*map;
	voff_t				 offset; 
	vsize_t				 end, nsize;
	int				 ret;

	offset = (voff_t)*mmap_offset;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, handle));
	if (obj == NULL)
		return (EBADF);

	/* Since we are doing purely uvm-related operations here we do
	 * not need to hold the object, a reference alone is sufficient
	 */

	/* Check size. */
	if (offset > obj->base.size) {
		ret = EINVAL;
		goto done;
	}

	if (obj->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to mmap a purgeable buffer\n");
		ret = EINVAL;
		goto done;
	}

	ret = i915_gem_object_bind_to_gtt(obj, 0);
	if (ret) {
		printf("%s: failed to bind\n", __func__);
		goto done;
	}
	i915_gem_object_move_to_inactive(obj);

	end = round_page(offset + obj->base.size);
	offset = trunc_page(offset);
	nsize = end - offset;

	ret = drm_addmap(dev, offset + obj->gtt_offset, nsize, _DRM_AGP,
	    _DRM_WRITE_COMBINING, &map);
	
done:
	if (ret == 0)
		*mmap_offset = map->ext;
	else
		drm_unref(&obj->base.uobj);

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

// i915_gem_object_truncate
// i915_gem_object_is_purgeable
// i915_gem_object_put_pages_gtt
// i915_gem_object_put_pages
// __i915_gem_shrink
// i915_gem_purge
// i915_gem_shrink_all
// i915_gem_object_get_pages_gtt
// i915_gem_object_get_pages

/* called locked */
void
i915_gem_object_move_to_active(struct drm_i915_gem_object *obj,
			       struct intel_ring_buffer *ring)
{
	struct drm_device		*dev = obj->base.dev;
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	u_int32_t			 seqno;

	seqno = i915_gem_next_request_seqno(ring);

	BUG_ON(ring == NULL);
	obj->ring = ring;

	/* Add a reference if we're newly entering the active list. */
	if (!obj->active) {
		drm_gem_object_reference(&obj->base);
		obj->active = 1;
	}

	if (obj->fenced_gpu_access) {
		obj->last_fenced_seqno = seqno;
	}
	if (obj->base.write_domain)
		obj->last_write_seqno = seqno;

	/* Move from whatever list we were on to the tail of execution. */
	list_move_tail(&obj->mm_list, &dev_priv->mm.active_list);
	list_move_tail(&obj->ring_list, &ring->active_list);
	obj->last_read_seqno = seqno;
}

void
i915_gem_object_move_off_active(struct drm_i915_gem_object *obj)
{
	DRM_OBJ_ASSERT_LOCKED(&obj->base);

	obj->last_read_seqno = 0;
	obj->last_fenced_seqno = 0;
	if (obj->base.write_domain == 0)
		obj->last_write_seqno = 0;
}

void
i915_gem_object_move_to_flushing(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	BUG_ON(!obj->active);
	list_move_tail(&obj->mm_list, &dev_priv->mm.flushing_list);

	i915_gem_object_move_off_active(obj);
}

/* called locked */
void
i915_gem_object_move_to_inactive_locked(struct drm_i915_gem_object *obj)
{
	struct drm_device	*dev = obj->base.dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;

	DRM_OBJ_ASSERT_LOCKED(&obj->base);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	if (obj->pin_count != 0)
		list_del_init(&obj->mm_list);
	else
		list_move_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	list_del_init(&obj->ring_list);
	obj->ring = NULL;

	i915_gem_object_move_off_active(obj);
	obj->fenced_gpu_access = false;

	/* unlock because this unref could recurse */
	if (obj->active) {
		obj->active = 0;
		obj->pending_gpu_write = false;
		drm_unref_locked(&obj->base.uobj);
	} else {
		drm_unlock_obj(&obj->base);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
}

/* If you call this on an object that you have held, you must have your own
 * reference, not just the reference from the active list.
 */
void
i915_gem_object_move_to_inactive(struct drm_i915_gem_object *obj)
{
	drm_lock_obj(&obj->base);
	/* unlocks object lock */
	i915_gem_object_move_to_inactive_locked(obj);
}

void
i915_gem_process_flushing_list(struct intel_ring_buffer *ring,
			       uint32_t flush_domains)
{
	struct drm_i915_gem_object *obj, *next;

	list_for_each_entry_safe(obj, next,
				 &ring->gpu_write_list,
				 gpu_write_list) {
		if (obj->base.write_domain & flush_domains) {
//			uint32_t old_write_domain = obj->base.write_domain;

			obj->base.write_domain = 0;

			list_del_init(&obj->gpu_write_list);
			i915_gem_object_move_to_active(obj, ring);

//			trace_i915_gem_object_change_domain(obj,
//							    obj->base.read_domains,
//							    old_write_domain);
		}
	}
}

int
i915_gem_handle_seqno_wrap(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int ret, i, j;

	/* The hardware uses various monotonic 32-bit counters, if we
	 * detect that they will wraparound we need to idle the GPU
	 * and reset those counters.
	 */
	ret = 0;
	for_each_ring(ring, dev_priv, i) {
		for (j = 0; j < nitems(ring->sync_seqno); j++)
			ret |= ring->sync_seqno[j] != 0;
	}
	if (ret == 0)
		return ret;

	ret = i915_gpu_idle(dev);
	if (ret)
		return ret;

	i915_gem_retire_requests(dev_priv);
	for_each_ring(ring, dev_priv, i) {
		for (j = 0; j < nitems(ring->sync_seqno); j++)
			ring->sync_seqno[j] = 0;
	}

	return 0;
}

u32
i915_gem_next_request_seqno(struct intel_ring_buffer *ring)
{
	if (ring->outstanding_lazy_request == 0)
		/* XXX check return */
		i915_gem_get_seqno(ring->dev, &ring->outstanding_lazy_request);

	return ring->outstanding_lazy_request;
}

int
i915_gem_get_seqno(struct drm_device *dev, u32 *seqno)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	/* reserve 0 for non-seqno */
	if (dev_priv->next_seqno == 0) {
#ifdef notyet
		int ret = i915_gem_handle_seqno_wrap(dev);
		if (ret)
			return ret;
#endif

		dev_priv->next_seqno = 1;
	}

	*seqno = dev_priv->next_seqno++;
	return 0;
}

/**
 * Creates a new sequence number, emitting a write of it to the status page
 * plus an interrupt, which will trigger and interrupt if they are currently
 * enabled.
 *
 * Returned sequence numbers are nonzero on success.
 */
int
i915_add_request(struct intel_ring_buffer *ring,
		 struct drm_file *file,
		 u32 *out_seqno)
{
	drm_i915_private_t	*dev_priv = ring->dev->dev_private;
	struct drm_i915_gem_request	*request;
	uint32_t			 seqno;
	u32				 request_ring_position;
	int				 was_empty, ret;

	request = drm_calloc(1, sizeof(*request));
	if (request == NULL) {
		printf("%s: failed to allocate request\n", __func__);
		return -ENOMEM;
	}

	/* Record the position of the start of the request so that
	 * should we detect the updated seqno part-way through the
	 * GPU processing the request, we never over-estimate the
	 * position of the head.
	 */
	request_ring_position = intel_ring_get_tail(ring);

	ret = ring->add_request(ring, &seqno);
	if (ret) {
		drm_free(request);
		return ret;
	}

	DRM_DEBUG("%d\n", seqno);

	/* XXX request timing for throttle */
	request->seqno = seqno;
	request->ring = ring;
	request->tail = request_ring_position;
	was_empty = list_empty(&ring->request_list);
	list_add_tail(&request->list, &ring->request_list);

	ring->outstanding_lazy_request = 0;

	if (dev_priv->mm.suspended == 0) {
		if (was_empty)
			timeout_add_sec(&dev_priv->mm.retire_timer, 1);
		/* XXX was_empty? */
		timeout_add_msec(&dev_priv->hangcheck_timer, 750);
	}

	if (out_seqno)
		*out_seqno = request->seqno;
	return 0;
}

// i915_gem_request_remove_from_client
// i915_gem_reset_ring_lists

void
i915_gem_reset_fences(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;

	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		struct drm_i915_fence_reg *reg = &dev_priv->fence_regs[i];

		i915_gem_write_fence(dev, i, NULL);

		if (reg->obj)
			i915_gem_object_fence_lost(reg->obj);

		reg->pin_count = 0;
		reg->obj = NULL;
		INIT_LIST_HEAD(&reg->lru_list);
	}

	INIT_LIST_HEAD(&dev_priv->mm.fence_list);
}

// i915_gem_reset

/**
 * This function clears the request list as sequence numbers are passed.
 */
void
i915_gem_retire_requests_ring(struct intel_ring_buffer *ring)
{
	struct drm_device		*dev = ring->dev;
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_i915_gem_request	*request;
	uint32_t			 seqno;

	if (list_empty(&ring->request_list))
		return;

	seqno = ring->get_seqno(ring, true);

	while (!list_empty(&ring->request_list)) {
		request = list_first_entry(&ring->request_list,
		    struct drm_i915_gem_request, list);

		if (!(i915_seqno_passed(seqno, request->seqno) ||
		    dev_priv->mm.wedged))
			break;

		/* We know the GPU must have read the request to have
		 * sent us the seqno + interrupt, so use the position
		 * of tail of the request to update the last known position
		 * of the GPU head.
		 */
		ring->last_retired_head = request->tail;

		list_del_init(&request->list);
		drm_free(request);
	}

	/* Move any buffers on the active list that are no longer referenced
	 * by the ringbuffer to the flushing/inactive lists as appropriate.
	 */
	while (!list_empty(&ring->active_list)) {
		struct drm_i915_gem_object *obj;

		obj = list_first_entry(&ring->active_list,
				      struct drm_i915_gem_object,
				      ring_list);

		if (!i915_seqno_passed(seqno, obj->last_read_seqno))
			break;

		drm_lock_obj(&obj->base);
		if (obj->base.write_domain != 0) {
			KASSERT(obj->active);
			list_move_tail(&obj->mm_list,
			    &dev_priv->mm.flushing_list);
			list_del_init(&obj->ring_list);
			i915_gem_object_move_off_active(obj);
			drm_unlock_obj(&obj->base);
		} else {
			/* unlocks object for us and drops ref */
			i915_gem_object_move_to_inactive_locked(obj);
		}
	}
}

void
i915_gem_retire_requests(struct inteldrm_softc *dev_priv)
{
	struct intel_ring_buffer *ring;
	int i;

	for_each_ring(ring, dev_priv, i)
		i915_gem_retire_requests_ring(ring);
}

// i915_gem_retire_work_handler
// i915_gem_object_flush_active
// i915_gem_wait_ioctl
// i915_gem_object_sync
// i915_gem_object_finish_gtt

/**
 * Unbinds an object from the GTT aperture.
 *
 * XXX track dirty and pass down to uvm (note, DONTNEED buffers are clean).
 */
int
i915_gem_object_unbind(struct drm_i915_gem_object *obj)
{
	struct drm_device	*dev = obj->base.dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int			 ret = 0;

	DRM_ASSERT_HELD(&obj->base);
	/*
	 * if it's already unbound, or we've already done lastclose, just
	 * let it happen. XXX does this fail to unwire?
	 */
	if (obj->dmamap == NULL || dev_priv->agpdmat == NULL)
		return 0;

	if (obj->pin_count != 0) {
		DRM_ERROR("Attempting to unbind pinned buffer\n");
		return (EINVAL);
	}

	KASSERT(obj->madv != __I915_MADV_PURGED);

	/* Move the object to the CPU domain to ensure that
	 * any possible CPU writes while it's not in the GTT
	 * are flushed when we go to remap it. This will
	 * also ensure that all pending GPU writes are finished
	 * before we unbind.
	 */
	ret = i915_gem_object_set_to_cpu_domain(obj, 1);
	if (ret)
		return ret;

	KASSERT(!obj->active);

	/* if it's purgeable don't bother dirtying the pages */
	if (i915_gem_object_is_purgeable(obj))
		obj->dirty = 0;
	/*
	 * unload the map, then unwire the backing object.
	 */
	i915_gem_object_save_bit_17_swizzle(obj);
	bus_dmamap_unload(dev_priv->agpdmat, obj->dmamap);
	uvm_objunwire(obj->base.uao, 0, obj->base.size);
	/* XXX persistent dmamap worth the memory? */
	bus_dmamap_destroy(dev_priv->agpdmat, obj->dmamap);
	obj->dmamap = NULL;
	free(obj->dma_segs, M_DRM);
	obj->dma_segs = NULL;
	/* XXX this should change whether we tell uvm the page is dirty */
	obj->dirty = 0;

	obj->gtt_offset = 0;
	atomic_dec(&dev->gtt_count);
	atomic_sub(obj->base.size, &dev->gtt_memory);

	/* Remove ourselves from any LRU list if present. */
	list_del_init(&obj->mm_list);

	if (i915_gem_object_is_purgeable(obj))
		inteldrm_purge_obj(&obj->base);

	return (0);
}

int
i915_gpu_idle(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int ret, i;

	/* Flush everything onto the inactive list. */
	for_each_ring(ring, dev_priv, i) {
#ifdef notyet
		ret = i915_switch_context(ring, NULL, DEFAULT_CONTEXT_ID);
		if (ret)
			return ret;
#endif

		ret = intel_ring_idle(ring);
		if (ret)
			return ret;
	}

	return 0;
}

void
sandybridge_write_fence_reg(struct drm_device *dev, int reg,
					struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint64_t val;

	if (obj) {
		u32 size = obj->base.size;

		val = (uint64_t)((obj->gtt_offset + size - 4096) &
				 0xfffff000) << 32;
		val |= obj->gtt_offset & 0xfffff000;
		val |= (uint64_t)((obj->stride / 128) - 1) <<
			SANDYBRIDGE_FENCE_PITCH_SHIFT;

		if (obj->tiling_mode == I915_TILING_Y)
			val |= 1 << I965_FENCE_TILING_Y_SHIFT;
		val |= I965_FENCE_REG_VALID;
	} else
		val = 0;

	I915_WRITE64(FENCE_REG_SANDYBRIDGE_0 + reg * 8, val);
	POSTING_READ(FENCE_REG_SANDYBRIDGE_0 + reg * 8);
}

void
i965_write_fence_reg(struct drm_device *dev, int reg,
				 struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint64_t val;

	if (obj) {
		u32 size = obj->base.size;

		val = (uint64_t)((obj->gtt_offset + size - 4096) &
				 0xfffff000) << 32;
		val |= obj->gtt_offset & 0xfffff000;
		val |= ((obj->stride / 128) - 1) << I965_FENCE_PITCH_SHIFT;
		if (obj->tiling_mode == I915_TILING_Y)
			val |= 1 << I965_FENCE_TILING_Y_SHIFT;
		val |= I965_FENCE_REG_VALID;
	} else
		val = 0;

	I915_WRITE64(FENCE_REG_965_0 + reg * 8, val);
	POSTING_READ(FENCE_REG_965_0 + reg * 8);
}

void
i915_write_fence_reg(struct drm_device *dev, int reg,
				 struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 val;

	if (obj) {
		u32 size = obj->base.size;
		int pitch_val;
		int tile_width;

		WARN((obj->gtt_offset & ~I915_FENCE_START_MASK) ||
		     (size & -size) != size ||
		     (obj->gtt_offset & (size - 1)),
		     "object 0x%08x [fenceable? %d] not 1M or pot-size (0x%08x) aligned\n",
		     obj->gtt_offset, obj->map_and_fenceable, size);

		if (obj->tiling_mode == I915_TILING_Y && HAS_128_BYTE_Y_TILING(dev))
			tile_width = 128;
		else
			tile_width = 512;

		/* Note: pitch better be a power of two tile widths */
		pitch_val = obj->stride / tile_width;
		pitch_val = ffs(pitch_val) - 1;

		val = obj->gtt_offset;
		if (obj->tiling_mode == I915_TILING_Y)
			val |= 1 << I830_FENCE_TILING_Y_SHIFT;
		val |= I915_FENCE_SIZE_BITS(size);
		val |= pitch_val << I830_FENCE_PITCH_SHIFT;
		val |= I830_FENCE_REG_VALID;
	} else
		val = 0;

	if (reg < 8)
		reg = FENCE_REG_830_0 + reg * 4;
	else
		reg = FENCE_REG_945_8 + (reg - 8) * 4;

	I915_WRITE(reg, val);
	POSTING_READ(reg);
}

void
i830_write_fence_reg(struct drm_device *dev, int reg,
				struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t val;

	if (obj) {
		u32 size = obj->base.size;
		uint32_t pitch_val;

		WARN((obj->gtt_offset & ~I830_FENCE_START_MASK) ||
		     (size & -size) != size ||
		     (obj->gtt_offset & (size - 1)),
		     "object 0x%08x not 512K or pot-size 0x%08x aligned\n",
		     obj->gtt_offset, size);

		pitch_val = obj->stride / 128;
		pitch_val = ffs(pitch_val) - 1;

		val = obj->gtt_offset;
		if (obj->tiling_mode == I915_TILING_Y)
			val |= 1 << I830_FENCE_TILING_Y_SHIFT;
		val |= I830_FENCE_SIZE_BITS(size);
		val |= pitch_val << I830_FENCE_PITCH_SHIFT;
		val |= I830_FENCE_REG_VALID;
	} else
		val = 0;

	I915_WRITE(FENCE_REG_830_0 + reg * 4, val);
	POSTING_READ(FENCE_REG_830_0 + reg * 4);
}

void
i915_gem_write_fence(struct drm_device *dev, int reg,
				 struct drm_i915_gem_object *obj)
{
	switch (INTEL_INFO(dev)->gen) {
	case 7:
	case 6: sandybridge_write_fence_reg(dev, reg, obj); break;
	case 5:
	case 4: i965_write_fence_reg(dev, reg, obj); break;
	case 3: i915_write_fence_reg(dev, reg, obj); break;
	case 2: i830_write_fence_reg(dev, reg, obj); break;
	default: break;
	}
}

static inline int
fence_number(drm_i915_private_t *dev_priv,
			       struct drm_i915_fence_reg *fence)
{
	return fence - dev_priv->fence_regs;
}

void
i915_gem_object_update_fence(struct drm_i915_gem_object *obj,
					 struct drm_i915_fence_reg *fence,
					 bool enable)
{
	drm_i915_private_t *dev_priv = obj->base.dev->dev_private;
	int reg = fence_number(dev_priv, fence);

	i915_gem_write_fence(obj->base.dev, reg, enable ? obj : NULL);

	if (enable) {
		obj->fence_reg = reg;
		fence->obj = obj;
		list_move_tail(&fence->lru_list, &dev_priv->mm.fence_list);
	} else {
		obj->fence_reg = I915_FENCE_REG_NONE;
		fence->obj = NULL;
		list_del_init(&fence->lru_list);
	}
}

int
i915_gem_object_flush_fence(struct drm_i915_gem_object *obj)
{
	if (obj->last_fenced_seqno) {
		int ret = i915_wait_seqno(obj->ring, obj->last_fenced_seqno);
		if (ret)
			return ret;

		obj->last_fenced_seqno = 0;
	}

	/* Ensure that all CPU reads are completed before installing a fence
	 * and all writes before removing the fence.
	 */
	if (obj->base.read_domains & I915_GEM_DOMAIN_GTT)
		DRM_WRITEMEMORYBARRIER();

	obj->fenced_gpu_access = false;
	return 0;
}

int
i915_gem_object_put_fence(struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = obj->base.dev->dev_private;
	int ret;

	ret = i915_gem_object_flush_fence(obj);
	if (ret)
		return ret;

	if (obj->fence_reg == I915_FENCE_REG_NONE)
		return 0;

	i915_gem_object_update_fence(obj,
				     &dev_priv->fence_regs[obj->fence_reg],
				     false);
	i915_gem_object_fence_lost(obj);

	return 0;
}

struct drm_i915_fence_reg *
i915_find_fence_reg(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_fence_reg *reg, *avail;
	int i;

	/* First try to find a free reg */
	avail = NULL;
	for (i = dev_priv->fence_reg_start; i < dev_priv->num_fence_regs; i++) {
		reg = &dev_priv->fence_regs[i];
		if (!reg->obj)
			return reg;

		if (!reg->pin_count)
			avail = reg;
	}

	if (avail == NULL)
		return NULL;

	/* None available, try to steal one or wait for a user to finish */
	list_for_each_entry(reg, &dev_priv->mm.fence_list, lru_list) {
		if (reg->pin_count)
			continue;

		return reg;
	}

	return NULL;
}

/**
 * i915_gem_object_get_fence - set up fencing for an object
 * @obj: object to map through a fence reg
 *
 * When mapping objects through the GTT, userspace wants to be able to write
 * to them without having to worry about swizzling if the object is tiled.
 * This function walks the fence regs looking for a free one for @obj,
 * stealing one if it can't find any.
 *
 * It then sets up the reg based on the object's properties: address, pitch
 * and tiling format.
 *
 * For an untiled surface, this removes any existing fence.
 */
int
i915_gem_object_get_fence(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	bool enable = obj->tiling_mode != I915_TILING_NONE;
	struct drm_i915_fence_reg *reg;
	int ret;

	/* Have we updated the tiling parameters upon the object and so
	 * will need to serialise the write to the associated fence register?
	 */
	if (obj->fence_dirty) {
		ret = i915_gem_object_flush_fence(obj);
		if (ret)
			return ret;
	}

	/* Just update our place in the LRU if our fence is getting reused. */
	if (obj->fence_reg != I915_FENCE_REG_NONE) {
		reg = &dev_priv->fence_regs[obj->fence_reg];
		if (!obj->fence_dirty) {
			list_move_tail(&reg->lru_list,
				       &dev_priv->mm.fence_list);
			return 0;
		}
	} else if (enable) {
		reg = i915_find_fence_reg(dev);
		if (reg == NULL)
			return -EDEADLK;

		if (reg->obj) {
			struct drm_i915_gem_object *old = reg->obj;

			ret = i915_gem_object_flush_fence(old);
			if (ret)
				return ret;

			i915_gem_object_fence_lost(old);
		}
	} else
		return 0;

	i915_gem_object_update_fence(obj, reg, enable);
	obj->fence_dirty = false;

	return 0;
}

// i915_gem_valid_gtt_space
// i915_gem_verify_gtt

/**
 * Finds free space in the GTT aperture and binds the object there.
 */
int
i915_gem_object_bind_to_gtt(struct drm_i915_gem_object *obj,
    bus_size_t alignment)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	DRM_ASSERT_HELD(&obj->base);
	if (dev_priv->agpdmat == NULL)
		return (EINVAL);
	if (alignment == 0) {
		alignment = i915_gem_get_gtt_alignment(&obj->base);
	} else if (alignment & (i915_gem_get_gtt_alignment(&obj->base) - 1)) {
		DRM_ERROR("Invalid object alignment requested %u\n", alignment);
		return (EINVAL);
	}

	if (obj->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to bind a purgeable object\n");
		return (EINVAL);
	}

	if ((ret = bus_dmamap_create(dev_priv->agpdmat, obj->base.size, 1,
	    obj->base.size, 0, BUS_DMA_WAITOK, &obj->dmamap)) != 0) {
		DRM_ERROR("Failed to create dmamap\n");
		return (ret);
	}
	agp_bus_dma_set_alignment(dev_priv->agpdmat, obj->dmamap,
	    alignment);

 search_free:
	/*
	 * the helper function wires the uao then binds it to the aperture for
	 * us, so all we have to do is set up the dmamap then load it.
	 */
	ret = drm_gem_load_uao(dev_priv->agpdmat, obj->dmamap, obj->base.uao,
	    obj->base.size, BUS_DMA_WAITOK | obj->dma_flags,
	    &obj->dma_segs);
	/* XXX NOWAIT? */
	if (ret != 0) {
		/* If the gtt is empty and we're still having trouble
		 * fitting our object in, we're out of memory.
		 */
		if (list_empty(&dev_priv->mm.inactive_list) &&
		    list_empty(&dev_priv->mm.flushing_list) &&
		    list_empty(&dev_priv->mm.active_list)) {
			DRM_ERROR("GTT full, but LRU list empty\n");
			goto error;
		}

		ret = i915_gem_evict_something(dev_priv, obj->base.size);
		if (ret != 0)
			goto error;
		goto search_free;
	}
	i915_gem_object_save_bit_17_swizzle(obj);

	obj->gtt_offset = obj->dmamap->dm_segs[0].ds_addr - dev->agp->base;

	atomic_inc(&dev->gtt_count);
	atomic_add(obj->base.size, &dev->gtt_memory);

	/* Assert that the object is not currently in any GPU domain. As it
	 * wasn't in the GTT, there shouldn't be any way it could have been in
	 * a GPU cache
	 */
	KASSERT((obj->base.read_domains & I915_GEM_GPU_DOMAINS) == 0);
	KASSERT((obj->base.write_domain & I915_GEM_GPU_DOMAINS) == 0);

	return (0);

error:
	bus_dmamap_destroy(dev_priv->agpdmat, obj->dmamap);
	obj->dmamap = NULL;
	obj->gtt_offset = 0;
	return (ret);
}

// i915_gem_clflush_object

// i915_gem_object_flush_gtt_write_domain
/*
 * Flush the GPU write domain for the object if dirty, then wait for the
 * rendering to complete. When this returns it is safe to unbind from the
 * GTT or access from the CPU.
 */
int
i915_gem_object_flush_gpu_write_domain(struct drm_i915_gem_object *obj,
    int pipelined, int write)
{
	u_int32_t		 seqno;
	int			 ret = 0;

	DRM_ASSERT_HELD(&obj->base);
	if ((obj->base.write_domain & I915_GEM_GPU_DOMAINS) != 0) {
		/*
		 * Queue the GPU write cache flushing we need.
		 * This call will move stuff form the flushing list to the
		 * active list so all we need to is wait for it.
		 */
		(void)i915_gem_flush(obj->ring, 0, obj->base.write_domain);
		KASSERT(obj->base.write_domain == 0);
	}

	/* wait for queued rendering so we know it's flushed and bo is idle */
	if (pipelined == 0 && obj->active) {
		if (write) {
			seqno = obj->last_read_seqno;
		} else {
			seqno = obj->last_write_seqno;
		}
		ret =  i915_wait_seqno(obj->ring, seqno);
	}
	return (ret);
}

void
i915_gem_object_flush_cpu_write_domain(struct drm_obj *obj)
{
	printf("%s stub\n", __func__);
}

/*
 * Moves a single object to the GTT and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to occur.
 */
int
i915_gem_object_set_to_gtt_domain(struct drm_i915_gem_object *obj, int write)
{
	drm_i915_private_t *dev_priv = obj->base.dev->dev_private;
	int ret;

	DRM_ASSERT_HELD(&obj->base);
	/* Not valid to be called on unbound objects. */
	if (obj->dmamap == NULL)
		return (EINVAL);

	/* Wait on any GPU rendering and flushing to occur. */
	if ((ret = i915_gem_object_flush_gpu_write_domain(obj, 0,
	    write)) != 0)
		return (ret);

	if (obj->base.write_domain == I915_GEM_DOMAIN_CPU) {
		/* clflush the pages, and flush chipset cache */
		bus_dmamap_sync(dev_priv->agpdmat, obj->dmamap, 0,
		    obj->base.size, BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
		inteldrm_chipset_flush(dev_priv);
		obj->base.write_domain = 0;
	}

	/* We're accessing through the gpu, so grab a new fence register or
	 * update the LRU.
	 */
	if (obj->fence_dirty) {
		ret = i915_gem_object_put_fence(obj);
		if (ret)
			return (ret);
	}
	if (obj->tiling_mode != I915_TILING_NONE)
		ret = i915_gem_object_get_fence(obj);

	/*
	 * If we're writing through the GTT domain then the CPU and GPU caches
	 * will need to be invalidated at next use.
	 * It should now be out of any other write domains and we can update
	 * to the correct ones
	 */
	KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_GTT) == 0);
	if (write) {
		obj->base.read_domains = obj->base.write_domain = I915_GEM_DOMAIN_GTT;
		obj->dirty = 1;
	} else {
		obj->base.read_domains |= I915_GEM_DOMAIN_GTT;
	}

	return (ret);
}

int
i915_gem_object_set_cache_level(struct drm_i915_gem_object *obj,
    enum i915_cache_level cache_level)
{
	printf("%s stub\n", __func__);
	return (0);
}

// i915_gem_get_caching_ioctl
// i915_gem_set_caching_ioctl

int
i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *obj,
    u32 alignment, struct intel_ring_buffer *pipelined)
{
//	u32 old_read_domains, old_write_domain;
	int ret;

	ret = i915_gem_object_flush_gpu_write_domain(obj, pipelined != NULL,
	    0);
	if (ret != 0)
		return (ret);

	if (pipelined != obj->ring) {
		ret = i915_gem_object_wait_rendering(obj, false);
		if (ret == -ERESTART || ret == -EINTR)
			return (ret);
	}

	ret = i915_gem_object_set_cache_level(obj, I915_CACHE_NONE);
	if (ret != 0)
		return (ret);

	ret = i915_gem_object_pin(obj, alignment, true);
	if (ret != 0)
		return (ret);

#ifdef notyet
	i915_gem_object_flush_cpu_write_domain(obj);

	old_write_domain = obj->write_domain;
	old_read_domains = obj->read_domains;

	KASSERT((obj->write_domain & ~I915_GEM_DOMAIN_GTT) == 0);
	obj->read_domains |= I915_GEM_DOMAIN_GTT;
#else
	printf("%s skipping write domain flush\n", __func__);
#endif

#ifdef notyet
	CTR3(KTR_DRM, "object_change_domain pin_to_display_plan %p %x %x",
	    obj, old_read_domains, obj->write_domain);
#endif
	return (0);
}

int
i915_gem_object_finish_gpu(struct drm_i915_gem_object *obj)
{
	int ret;

	if ((obj->base.read_domains & I915_GEM_GPU_DOMAINS) == 0)
		return 0;

	ret = i915_gem_object_wait_rendering(obj, false);
	if (ret)
		return ret;

	/* Ensure that we invalidate the GPU's caches and TLBs. */
	obj->base.read_domains &= ~I915_GEM_GPU_DOMAINS;
	return 0;
}

/*
 * Moves a single object to the CPU read and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to return.
 */
int
i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *obj, int write)
{
	struct drm_device	*dev = obj->base.dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int			 ret;

	DRM_ASSERT_HELD(obj);
	/* Wait on any GPU rendering and flushing to occur. */
	if ((ret = i915_gem_object_flush_gpu_write_domain(obj, 0,
	    write)) != 0)
		return (ret);

	if (obj->base.write_domain == I915_GEM_DOMAIN_GTT ||
	    (write && obj->base.read_domains & I915_GEM_DOMAIN_GTT)) {
		/*
		 * No actual flushing is required for the GTT write domain.
		 * Writes to it immeditately go to main memory as far as we
		 * know, so there's no chipset flush. It also doesn't land
		 * in render cache.
		 */
		inteldrm_wipe_mappings(&obj->base);
		if (obj->base.write_domain == I915_GEM_DOMAIN_GTT)
			obj->base.write_domain = 0;
	}

	/* remove the fence register since we're not using it anymore */
	if ((ret = i915_gem_object_put_fence(obj)) != 0)
		return (ret);

	/* Flush the CPU cache if it's still invalid. */
	if ((obj->base.read_domains & I915_GEM_DOMAIN_CPU) == 0) {
		bus_dmamap_sync(dev_priv->agpdmat, obj->dmamap, 0,
		    obj->base.size, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		obj->base.read_domains |= I915_GEM_DOMAIN_CPU;
	}

	/*
	 * It should now be out of any other write domain, and we can update
	 * the domain value for our changes.
	 */
	KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_CPU) == 0);

	/*
	 * If we're writing through the CPU, then the GPU read domains will
	 * need to be invalidated at next use.
	 */
	if (write)
		obj->base.read_domains = obj->base.write_domain = I915_GEM_DOMAIN_CPU;

	return (0);
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

int
i915_gem_object_pin(struct drm_i915_gem_object *obj, uint32_t alignment,
    int needs_fence)
{
	struct drm_device	*dev = obj->base.dev;
	int			 ret;

	DRM_ASSERT_HELD(&obj->base);
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	/*
	 * if already bound, but alignment is unsuitable, unbind so we can
	 * fix it. Similarly if we have constraints due to fence registers,
	 * adjust if needed. Note that if we are already pinned we may as well
	 * fail because whatever depends on this alignment will render poorly
	 * otherwise, so just fail the pin (with a printf so we can fix a
	 * wrong userland).
	 */
	if (obj->dmamap != NULL &&
	    ((alignment && obj->gtt_offset & (alignment - 1)) ||
	    obj->gtt_offset & (i915_gem_get_gtt_alignment(&obj->base) - 1) ||
	    !i915_gem_object_fence_ok(&obj->base, obj->tiling_mode))) {
		/* if it is already pinned we sanitised the alignment then */
		KASSERT(obj->pin_count == 0);
		if ((ret = i915_gem_object_unbind(obj)))
			return (ret);
	}

	if (obj->dmamap == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj, alignment);
		if (ret != 0)
			return (ret);
	}

	/*
	 * due to lazy fence destruction we may have an invalid fence now.
	 * So if so, nuke it before we do anything with the gpu.
	 * XXX 965+ can put this off.. and be faster
	 */
	if (obj->fence_dirty) {
		ret= i915_gem_object_put_fence(obj);
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
	if (needs_fence && obj->tiling_mode != I915_TILING_NONE &&
	    (ret = i915_gem_object_get_fence(obj)) != 0)
		return (ret);

	/* If the object is not active and not pending a flush,
	 * remove it from the inactive list
	 */
	if (++obj->pin_count == 1) {
		atomic_inc(&dev->pin_count);
		atomic_add(obj->base.size, &dev->pin_memory);
		if (!obj->active)
			list_del_init(&obj->mm_list);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	return (0);
}

void
i915_gem_object_unpin(struct drm_i915_gem_object *obj)
{
	struct drm_device	*dev = obj->base.dev;

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
	KASSERT(obj->pin_count >= 1);
	KASSERT(obj->dmamap != NULL);
	DRM_ASSERT_HELD(&obj->base);

	/* If the object is no longer pinned, and is
	 * neither active nor being flushed, then stick it on
	 * the inactive list
	 */
	if (--obj->pin_count == 0) {
		if (!obj->active)
			i915_gem_object_move_to_inactive(obj);
		atomic_dec(&dev->pin_count);
		atomic_sub(obj->base.size, &dev->pin_memory);
	}
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);
}

int
i915_gem_pin_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_i915_gem_pin	*args = data;
	struct drm_i915_gem_object *obj;
	int			 ret = 0;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (obj == NULL)
		return (EBADF);
	DRM_LOCK();
	drm_hold_object(&obj->base);

	if (obj->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to pin a purgeable buffer\n");
		ret = EINVAL;
		goto out;
	}

	if (++obj->user_pin_count == 1) {
		ret = i915_gem_object_pin(obj, args->alignment, 1);
		if (ret != 0)
			goto out;
		inteldrm_set_max_obj_size(dev_priv);
	}

	/* XXX - flush the CPU caches for pinned objects
	 * as the X server doesn't manage domains yet
	 */
	i915_gem_object_set_to_gtt_domain(obj, 1);
	args->offset = obj->gtt_offset;

out:
	drm_unhold_and_unref(&obj->base);
	DRM_UNLOCK();

	return (ret);
}

int
i915_gem_unpin_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_i915_gem_pin	*args = data;
	struct drm_i915_gem_object *obj;
	int ret = 0;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL)
		return (EBADF);

	DRM_LOCK();
	drm_hold_object(&obj->base);

	if (obj->user_pin_count == 0) {
		ret = EINVAL;
		goto out;
	}

	obj->user_pin_count--;
	if (obj->user_pin_count == 0) {
		i915_gem_object_unpin(obj);
		inteldrm_set_max_obj_size(dev_priv);
	}

out:
	drm_unhold_and_unref(&obj->base);
	DRM_UNLOCK();
	return (ret);
}

int
i915_gem_busy_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_busy *args = data;
	struct drm_i915_gem_object *obj;
	int ret = 0;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		DRM_ERROR("Bad handle in i915_gem_busy_ioctl(): %d\n",
			  args->handle);
		return (EBADF);
	}

	args->busy = obj->active;
	if (args->busy) {
		/*
		 * Unconditionally flush objects write domain if they are
		 * busy. The fact userland is calling this ioctl means that
		 * it wants to use this buffer sooner rather than later, so
		 * flushing now shoul reduce latency.
		 */
		if (obj->base.write_domain)
			(void)i915_gem_flush(obj->ring, obj->base.write_domain,
			    obj->base.write_domain);
		/*
		 * Update the active list after the flush otherwise this is
		 * only updated on a delayed timer. Updating now reduces 
		 * working set size.
		 */
		i915_gem_retire_requests_ring(obj->ring);
		args->busy = obj->active;
	}

	drm_gem_object_unreference(&obj->base);
	return ret;
}

// i915_gem_throttle_ioctl

int
i915_gem_madvise_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_gem_madvise *args = data;
	struct drm_i915_gem_object *obj;
	int ret = 0;

	switch (args->madv) {
	case I915_MADV_DONTNEED:
	case I915_MADV_WILLNEED:
		break;
	default:
		return (EINVAL);
	}

	obj = to_intel_bo(drm_gem_object_lookup(dev, file_priv, args->handle));
	if (&obj->base == NULL)
		return (EBADF);

	drm_hold_object(&obj->base);

	/* invalid to madvise on a pinned BO */
	if (obj->pin_count) {
		ret = EINVAL;
		goto out;
	}

	if (obj->madv != __I915_MADV_PURGED)
		obj->madv = args->madv;

	/* if the object is no longer bound, discard its backing storage */
	if (i915_gem_object_is_purgeable(obj) &&
	    obj->dmamap == NULL)
		inteldrm_purge_obj(&obj->base);

	args->retained = obj->madv != __I915_MADV_PURGED;

out:
	drm_unhold_and_unref(&obj->base);

	return (ret);
}

struct drm_i915_gem_object *
i915_gem_alloc_object(struct drm_device *dev, size_t size)
{
	struct drm_obj			*obj;
	struct drm_i915_gem_object	*obj_priv;

	obj = drm_gem_object_alloc(dev, size);
	if (obj == NULL)
		return NULL;

	obj_priv = to_intel_bo(obj);

	return (obj_priv);
}

int
i915_gem_init_object(struct drm_obj *obj)
{
	struct drm_i915_gem_object *obj_priv = to_intel_bo(obj);

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
	obj_priv->madv = I915_MADV_WILLNEED;
	/* Avoid an unnecessary call to unbind on the first bind. */
	obj_priv->map_and_fenceable = true;

	INIT_LIST_HEAD(&obj_priv->mm_list);
	INIT_LIST_HEAD(&obj_priv->ring_list);
	INIT_LIST_HEAD(&obj_priv->gpu_write_list);

	return 0;
}

/*
 * NOTE all object unreferences in this driver need to hold the DRM_LOCK(),
 * because if they free they poke around in driver structures.
 */
void
i915_gem_free_object(struct drm_obj *gem_obj)
{
	struct drm_i915_gem_object *obj = to_intel_bo(gem_obj);
	struct drm_device *dev = gem_obj->dev;

	DRM_ASSERT_HELD(&obj->base);

	if (obj->phys_obj)
		i915_gem_detach_phys_object(dev, obj);
	
	while (obj->pin_count > 0)
		i915_gem_object_unpin(obj);

	i915_gem_object_unbind(obj);
	drm_free(obj->bit_17);
	obj->bit_17 = NULL;
	/* XXX dmatag went away? */
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
	if (dev_priv->mm.suspended || dev_priv->rings[RCS].obj == NULL) {
		KASSERT(list_empty(&dev_priv->mm.flushing_list));
		KASSERT(list_empty(&dev_priv->mm.active_list));
		(void)i915_gem_evict_inactive(dev_priv);
		DRM_UNLOCK();
		return (0);
	}

	/*
	 * To idle the gpu, flush anything pending then unbind the whole
	 * shebang. If we're wedged, assume that the reset workq will clear
	 * everything out and continue as normal.
	 */
	if ((ret = i915_gem_evict_everything(dev_priv)) != 0 &&
	    ret != ENOSPC && ret != EIO) {
		DRM_UNLOCK();
		return (ret);
	}

	i915_gem_reset_fences(dev);

	/* Hack!  Don't let anybody do execbuf while we don't control the chip.
	 * We need to replace this with a semaphore, or something.
	 */
	dev_priv->mm.suspended = 1;
	/* if we hung then the timer alredy fired. */
	timeout_del(&dev_priv->hangcheck_timer);

	i915_kernel_lost_context(dev);
	i915_gem_cleanup_ringbuffer(dev);
	DRM_UNLOCK();

	/* this should be idle now */
	timeout_del(&dev_priv->mm.retire_timer);

	return 0;
}

// i915_gem_l3_remap

void
i915_gem_init_swizzling(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (INTEL_INFO(dev)->gen < 5 ||
	    dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_NONE)
		return;

	I915_WRITE(DISP_ARB_CTL, I915_READ(DISP_ARB_CTL) |
				 DISP_TILE_SURFACE_SWIZZLING);

	if (IS_GEN5(dev))
		return;

	I915_WRITE(TILECTL, I915_READ(TILECTL) | TILECTL_SWZCTL);
	if (IS_GEN6(dev))
		I915_WRITE(ARB_MODE, _MASKED_BIT_ENABLE(ARB_MODE_SWIZZLE_SNB));
	else
		I915_WRITE(ARB_MODE, _MASKED_BIT_ENABLE(ARB_MODE_SWIZZLE_IVB));
}

bool
intel_enable_blt(struct drm_device *dev)
{
	if (!HAS_BLT(dev))
		return false;

#ifdef notyet
	/* The blitter was dysfunctional on early prototypes */
	if (IS_GEN6(dev) && dev->pdev->revision < 8) {
		DRM_INFO("BLT not supported on this pre-production hardware;"
			 " graphics performance will be degraded.\n");
		return false;
	}
#endif

	return true;
}

int
i915_gem_init_hw(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

#ifdef notyet
	if (INTEL_INFO(dev)->gen < 6 && !intel_enable_gtt())
		return -EIO;

	if (IS_HASWELL(dev) && (I915_READ(0x120010) == 1))
		I915_WRITE(0x9008, I915_READ(0x9008) | 0xf0000);

	i915_gem_l3_remap(dev);

#endif
	i915_gem_init_swizzling(dev);

	ret = intel_init_render_ring_buffer(dev);
	if (ret)
		return ret;

	if (HAS_BSD(dev)) {
		ret = intel_init_bsd_ring_buffer(dev);
		if (ret)
			goto cleanup_render_ring;
	}

	if (intel_enable_blt(dev)) {
		ret = intel_init_blt_ring_buffer(dev);
		if (ret)
			goto cleanup_bsd_ring;
	}

	dev_priv->next_seqno = 1;

	/*
	 * XXX: There was some w/a described somewhere suggesting loading
	 * contexts before PPGTT.
	 */
#ifdef notyet
	i915_gem_context_init(dev);
	i915_gem_init_ppgtt(dev);
#endif

	return 0;

cleanup_bsd_ring:
	intel_cleanup_ring_buffer(&dev_priv->rings[VCS]);
cleanup_render_ring:
	intel_cleanup_ring_buffer(&dev_priv->rings[RCS]);
	return ret;
}

// intel_enable_ppgtt

int
i915_gem_init(struct drm_device *dev)
{
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	uint64_t			 gtt_start, gtt_end;
	struct agp_softc		*asc;
	int				 ret;

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

	ret = i915_gem_init_hw(dev);
	if (ret != 0) {
		DRM_UNLOCK();
		return (ret);
	}

	DRM_UNLOCK();

	return 0;
}

void
i915_gem_cleanup_ringbuffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int i;

	for_each_ring(ring, dev_priv, i)
		intel_cleanup_ring_buffer(ring);
}

int
i915_gem_entervt_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct inteldrm_softc *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int ret, i;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return (0);

	/* XXX until we have support for the rings on sandybridge */
	if (IS_GEN6(dev) || IS_GEN7(dev))
		return (0);

	if (dev_priv->mm.wedged) {
		DRM_ERROR("Reenabling wedged hardware, good luck\n");
		dev_priv->mm.wedged = 0;
	}


	DRM_LOCK();
	dev_priv->mm.suspended = 0;

	ret = i915_gem_init_hw(dev);
	if (ret != 0) {
		DRM_UNLOCK();
		return (ret);
	}

	/* gtt mapping means that the inactive list may not be empty */
	KASSERT(list_empty(&dev_priv->mm.active_list));
	KASSERT(list_empty(&dev_priv->mm.flushing_list));
	for_each_ring(ring, dev_priv, i)
		KASSERT(list_empty(&ring->request_list));
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

// i915_gem_lastclose

void
init_ring_lists(struct intel_ring_buffer *ring)
{
	INIT_LIST_HEAD(&ring->active_list);
	INIT_LIST_HEAD(&ring->request_list);
	INIT_LIST_HEAD(&ring->gpu_write_list);
}

// i915_gem_load

int
i915_gem_init_phys_object(struct drm_device *dev,
			  int id, int size, int align)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_phys_object *phys_obj;
	int ret;

	if (dev_priv->mm.phys_objs[id - 1] || !size)
		return 0;

	phys_obj = drm_alloc(sizeof(struct drm_i915_gem_phys_object));
	if (!phys_obj)
		return -ENOMEM;

	phys_obj->id = id;

	phys_obj->handle = drm_dmamem_alloc(dev->dmat, size, align, 1, size, BUS_DMA_NOCACHE, 0);
	if (!phys_obj->handle) {
		ret = -ENOMEM;
		goto kfree_obj;
	}

	dev_priv->mm.phys_objs[id - 1] = phys_obj;

	return 0;
kfree_obj:
	drm_free(phys_obj);
	return ret;
}

// i915_gem_free_phys_object
// i915_gem_free_all_phys_object

void i915_gem_detach_phys_object(struct drm_device *dev,
				 struct drm_i915_gem_object *obj)
{
	char *vaddr;
	int i;
	int page_count;

	if (!obj->phys_obj)
		return;
	vaddr = obj->phys_obj->handle->kva;

	page_count = obj->base.size / PAGE_SIZE;
	for (i = 0; i < page_count; i++) {
#ifdef notyet
		struct page *page = shmem_read_mapping_page(mapping, i);
		if (!IS_ERR(page)) {
			char *dst = kmap_atomic(page);
			memcpy(dst, vaddr + i*PAGE_SIZE, PAGE_SIZE);
			kunmap_atomic(dst);

			drm_clflush_pages(&page, 1);

			set_page_dirty(page);
			mark_page_accessed(page);
			page_cache_release(page);
		}
#endif
	}
	inteldrm_chipset_flush(dev->dev_private);

	obj->phys_obj->cur_obj = NULL;
	obj->phys_obj = NULL;
}

int
i915_gem_attach_phys_object(struct drm_device *dev,
    struct drm_i915_gem_object *obj, int id, int align)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret = 0;
	int page_count;
	int i;

	if (id > I915_MAX_PHYS_OBJECT)
		return -EINVAL;

	if (obj->phys_obj) {
		if (obj->phys_obj->id == id)
			return 0;
		i915_gem_detach_phys_object(dev, obj);
	}

	/* create a new object */
	if (!dev_priv->mm.phys_objs[id - 1]) {
		ret = i915_gem_init_phys_object(dev, id,
						obj->base.size, align);
		if (ret) {
			DRM_ERROR("failed to init phys object %d size: %zu\n",
				  id, obj->base.size);
			return (ret);
		}
	}

	/* bind to the object */
	obj->phys_obj = dev_priv->mm.phys_objs[id - 1];
	obj->phys_obj->cur_obj = obj;

	page_count = obj->base.size / PAGE_SIZE;

	for (i = 0; i < page_count; i++) {
#ifdef notyet
		struct page *page;
		char *dst, *src;

		page = shmem_read_mapping_page(mapping, i);
		if (IS_ERR(page))
			return PTR_ERR(page);

		src = kmap_atomic(page);
		dst = obj->phys_obj->handle->kva + (i * PAGE_SIZE);
		memcpy(dst, src, PAGE_SIZE);
		kunmap_atomic(src);

		mark_page_accessed(page);
		page_cache_release(page);
#endif
	}

	return 0;
}

int
i915_gem_phys_pwrite(struct drm_device *dev,
		     struct drm_i915_gem_object *obj,
		     struct drm_i915_gem_pwrite *args,
		     struct drm_file *file_priv)
{
	void *vaddr = obj->phys_obj->handle->kva + args->offset;
	int ret;

	ret = copyin((char *)(uintptr_t)args->data_ptr,
	    vaddr, args->size);

	inteldrm_chipset_flush(dev->dev_private);

	return ret;
}

// i915_gem_release
// mutex_is_locked_by
// i915_gem_inactive_shrink
