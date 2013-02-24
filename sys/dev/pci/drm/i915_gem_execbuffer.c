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

int	i915_reset_gen7_sol_offsets(struct drm_device *,
	    struct intel_ring_buffer *);

/*
 * Set the next domain for the specified object. This
 * may not actually perform the necessary flushing/invaliding though,
 * as that may want to be batched with other set_domain operations
 *
 * This is (we hope) the only really tricky part of gem. The goal
 * is fairly simple -- track which caches hold bits of the object
 * and make sure they remain coherent. A few concrete examples may
 * help to explain how it works. For shorthand, we use the notation
 * (read_domains, write_domain), e.g. (CPU, CPU) to indicate the
 * a pair of read and write domain masks.
 *
 * Case 1: the batch buffer
 *
 *	1. Allocated
 *	2. Written by CPU
 *	3. Mapped to GTT
 *	4. Read by GPU
 *	5. Unmapped from GTT
 *	6. Freed
 *
 *	Let's take these a step at a time
 *
 *	1. Allocated
 *		Pages allocated from the kernel may still have
 *		cache contents, so we set them to (CPU, CPU) always.
 *	2. Written by CPU (using pwrite)
 *		The pwrite function calls set_domain (CPU, CPU) and
 *		this function does nothing (as nothing changes)
 *	3. Mapped by GTT
 *		This function asserts that the object is not
 *		currently in any GPU-based read or write domains
 *	4. Read by GPU
 *		i915_gem_execbuffer calls set_domain (COMMAND, 0).
 *		As write_domain is zero, this function adds in the
 *		current read domains (CPU+COMMAND, 0).
 *		flush_domains is set to CPU.
 *		invalidate_domains is set to COMMAND
 *		clflush is run to get data out of the CPU caches
 *		then i915_dev_set_domain calls i915_gem_flush to
 *		emit an MI_FLUSH and drm_agp_chipset_flush
 *	5. Unmapped from GTT
 *		i915_gem_object_unbind calls set_domain (CPU, CPU)
 *		flush_domains and invalidate_domains end up both zero
 *		so no flushing/invalidating happens
 *	6. Freed
 *		yay, done
 *
 * Case 2: The shared render buffer
 *
 *	1. Allocated
 *	2. Mapped to GTT
 *	3. Read/written by GPU
 *	4. set_domain to (CPU,CPU)
 *	5. Read/written by CPU
 *	6. Read/written by GPU
 *
 *	1. Allocated
 *		Same as last example, (CPU, CPU)
 *	2. Mapped to GTT
 *		Nothing changes (assertions find that it is not in the GPU)
 *	3. Read/written by GPU
 *		execbuffer calls set_domain (RENDER, RENDER)
 *		flush_domains gets CPU
 *		invalidate_domains gets GPU
 *		clflush (obj)
 *		MI_FLUSH and drm_agp_chipset_flush
 *	4. set_domain (CPU, CPU)
 *		flush_domains gets GPU
 *		invalidate_domains gets CPU
 *		flush_gpu_write (obj) to make sure all drawing is complete.
 *		This will include an MI_FLUSH to get the data from GPU
 *		to memory
 *		clflush (obj) to invalidate the CPU cache
 *		Another MI_FLUSH in i915_gem_flush (eliminate this somehow?)
 *	5. Read/written by CPU
 *		cache lines are loaded and dirtied
 *	6. Read written by GPU
 *		Same as last GPU access
 *
 * Case 3: The constant buffer
 *
 *	1. Allocated
 *	2. Written by CPU
 *	3. Read by GPU
 *	4. Updated (written) by CPU again
 *	5. Read by GPU
 *
 *	1. Allocated
 *		(CPU, CPU)
 *	2. Written by CPU
 *		(CPU, CPU)
 *	3. Read by GPU
 *		(CPU+RENDER, 0)
 *		flush_domains = CPU
 *		invalidate_domains = RENDER
 *		clflush (obj)
 *		MI_FLUSH
 *		drm_agp_chipset_flush
 *	4. Updated (written) by CPU again
 *		(CPU, CPU)
 *		flush_domains = 0 (no previous write domain)
 *		invalidate_domains = 0 (no new read domains)
 *	5. Read by GPU
 *		(CPU+RENDER, 0)
 *		flush_domains = CPU
 *		invalidate_domains = RENDER
 *		clflush (obj)
 *		MI_FLUSH
 *		drm_agp_chipset_flush
 */
void
i915_gem_object_set_to_gpu_domain(struct drm_obj *obj)
{
	struct drm_device	*dev = obj->dev;
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = to_intel_bo(obj);
	u_int32_t		 invalidate_domains = 0;
	u_int32_t		 flush_domains = 0;

	DRM_ASSERT_HELD(obj);
	KASSERT((obj->pending_read_domains & I915_GEM_DOMAIN_CPU) == 0);
	KASSERT(obj->pending_write_domain != I915_GEM_DOMAIN_CPU);
	/*
	 * If the object isn't moving to a new write domain,
	 * let the object stay in multiple read domains
	 */
	if (obj->pending_write_domain == 0)
		obj->pending_read_domains |= obj->read_domains;
	else
		obj_priv->dirty = 1;

	/*
	 * Flush the current write domain if
	 * the new read domains don't match. Invalidate
	 * any read domains which differ from the old
	 * write domain
	 */
	if (obj->write_domain &&
	    obj->write_domain != obj->pending_read_domains) {
		flush_domains |= obj->write_domain;
		invalidate_domains |= obj->pending_read_domains &
		    ~obj->write_domain;
	}
	/*
	 * Invalidate any read caches which may have
	 * stale data. That is, any new read domains.
	 */
	invalidate_domains |= obj->pending_read_domains & ~obj->read_domains;
	/* clflush the cpu now, gpu caches get queued. */
	if ((flush_domains | invalidate_domains) & I915_GEM_DOMAIN_CPU) {
		bus_dmamap_sync(dev_priv->agpdmat, obj_priv->dmamap, 0,
		    obj->size, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	if ((flush_domains | invalidate_domains) & I915_GEM_DOMAIN_GTT) {
		inteldrm_wipe_mappings(obj);
	}

	/* The actual obj->write_domain will be updated with
	 * pending_write_domain after we emit the accumulated flush for all of
	 * the domain changes in execuffer (which clears object's write
	 * domains). So if we have a current write domain that we aren't
	 * changing, set pending_write_domain to it.
	 */
	if (flush_domains == 0 && obj->pending_write_domain == 0 &&
	    (obj->pending_read_domains == obj->write_domain ||
	    obj->write_domain == 0))
		obj->pending_write_domain = obj->write_domain;
	obj->read_domains = obj->pending_read_domains;
	obj->pending_read_domains = 0;

	dev->invalidate_domains |= invalidate_domains;
	dev->flush_domains |= flush_domains;
}

// struct eb_objects {
// eb_create
// eb_reset
// eb_add_object
// eb_get_object
// eb_destroy
// i915_gem_execbuffer_relocate_entry
// i915_gem_execbuffer_relocate_object
// i915_gem_execbuffer_relocate_object_slow
// i915_gem_execbuffer_relocate
// pin_and_fence_object
// i915_gem_execbuffer_reserve
// i915_gem_execbuffer_relocate_slow
// i915_gem_execbuffer_flush
// intel_enable_semaphores
// i915_gem_execbuffer_sync_rings
// i915_gem_execbuffer_wait_for_flips
// i915_gem_execbuffer_move_to_gpu 
// i915_gem_check_execbuffer
// validate_exec_list
// i915_gem_execbuffer_move_to_active
// i915_gem_execbuffer_retire_commands
// i915_gem_fix_mi_batchbuffer_end
// i915_reset_gen7_sol_offsets

int
i915_reset_gen7_sol_offsets(struct drm_device *dev,
			    struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret, i;

	if (!IS_GEN7(dev) || ring != &dev_priv->rings[RCS])
		return 0;

	ret = intel_ring_begin(ring, 4 * 3);
	if (ret)
		return ret;

	for (i = 0; i < 4; i++) {
		intel_ring_emit(ring, MI_LOAD_REGISTER_IMM(1));
		intel_ring_emit(ring, GEN7_SO_WRITE_OFFSET(i));
		intel_ring_emit(ring, 0);
	}

	intel_ring_advance(ring);

	return 0;
}

// i915_gem_do_execbuffer
// i915_gem_execbuffer

int
i915_gem_execbuffer2(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct inteldrm_softc			*dev_priv = dev->dev_private;
	struct drm_i915_gem_execbuffer2		*args = data;
	struct drm_i915_gem_exec_object2	*exec_list = NULL;
	struct drm_i915_gem_relocation_entry	*relocs = NULL;
	struct drm_i915_gem_object		*obj_priv, *batch_obj_priv;
	struct drm_obj				**object_list = NULL;
	struct drm_obj				*batch_obj, *obj;
	struct intel_ring_buffer		*ring;
	size_t					 oflow;
	int					 ret, ret2, i;
	int					 pinned = 0, pin_tries;
	uint32_t				 reloc_index;

	/*
	 * Check for valid execbuffer offset. We can do this early because
	 * bound object are always page aligned, so only the start offset
	 * matters. Also check for integer overflow in the batch offset and size
	 */
	 if ((args->batch_start_offset | args->batch_len) & 0x7 ||
	    args->batch_start_offset + args->batch_len < args->batch_len ||
	    args->batch_start_offset + args->batch_len <
	    args->batch_start_offset)
		return (EINVAL);

	if (args->buffer_count < 1) {
		DRM_ERROR("execbuf with %d buffers\n", args->buffer_count);
		return (EINVAL);
	}

	switch (args->flags & I915_EXEC_RING_MASK) {
	case I915_EXEC_DEFAULT:
	case I915_EXEC_RENDER:
		ring = &dev_priv->rings[RCS];
		break;
	case I915_EXEC_BSD:
		ring = &dev_priv->rings[VCS];
		break;
	case I915_EXEC_BLT:
		ring = &dev_priv->rings[BCS];
		break;
	default:
		printf("unknown ring %d\n",
		    (int)(args->flags & I915_EXEC_RING_MASK));
		return (EINVAL);
	}
	if (!intel_ring_initialized(ring)) {
		DRM_DEBUG("execbuf with invalid ring: %d\n",
			  (int)(args->flags & I915_EXEC_RING_MASK));
		return (EINVAL);
	}

	/* Copy in the exec list from userland, check for overflow */
	oflow = SIZE_MAX / args->buffer_count;
	if (oflow < sizeof(*exec_list) || oflow < sizeof(*object_list))
		return (EINVAL);
	exec_list = drm_alloc(sizeof(*exec_list) * args->buffer_count);
	object_list = drm_alloc(sizeof(*object_list) * args->buffer_count);
	if (exec_list == NULL || object_list == NULL) {
		ret = ENOMEM;
		goto pre_mutex_err;
	}
	ret = copyin((void *)(uintptr_t)args->buffers_ptr, exec_list,
	    sizeof(*exec_list) * args->buffer_count);
	if (ret != 0)
		goto pre_mutex_err;

	ret = i915_gem_get_relocs_from_user(exec_list, args->buffer_count,
	    &relocs);
	if (ret != 0)
		goto pre_mutex_err;

	DRM_LOCK();
	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	/* XXX check these before we copyin... but we do need the lock */
	if (dev_priv->mm.wedged) {
		ret = EIO;
		goto unlock;
	}

	if (dev_priv->mm.suspended) {
		ret = EBUSY;
		goto unlock;
	}

	/* Look up object handles */
	for (i = 0; i < args->buffer_count; i++) {
		object_list[i] = drm_gem_object_lookup(dev, file_priv,
		    exec_list[i].handle);
		obj = object_list[i];
		if (obj == NULL) {
			DRM_ERROR("Invalid object handle %d at index %d\n",
				   exec_list[i].handle, i);
			ret = EBADF;
			goto err;
		}
		if (obj->do_flags & I915_IN_EXEC) {
			DRM_ERROR("Object %p appears more than once in object_list\n",
			    object_list[i]);
			ret = EBADF;
			goto err;
		}
		atomic_setbits_int(&obj->do_flags, I915_IN_EXEC);
	}

	/* Pin and relocate */
	for (pin_tries = 0; ; pin_tries++) {
		ret = pinned = 0;
		reloc_index = 0;

		for (i = 0; i < args->buffer_count; i++) {
			object_list[i]->pending_read_domains = 0;
			object_list[i]->pending_write_domain = 0;
			to_intel_bo(object_list[i])->pending_fenced_gpu_access = false;
			drm_hold_object(object_list[i]);
			ret = i915_gem_object_pin_and_relocate(object_list[i],
			    file_priv, &exec_list[i], &relocs[reloc_index]);
			if (ret) {
				drm_unhold_object(object_list[i]);
				break;
			}
			pinned++;
			reloc_index += exec_list[i].relocation_count;
		}
		/* success */
		if (ret == 0)
			break;

		/* error other than GTT full, or we've already tried again */
		if (ret != ENOSPC || pin_tries >= 1)
			goto err;

		/*
		 * unpin all of our buffers and unhold them so they can be
		 * unbound so we can try and refit everything in the aperture.
		 */
		for (i = 0; i < pinned; i++) {
			if (object_list[i]->do_flags & __EXEC_OBJECT_HAS_FENCE) {
				i915_gem_object_unpin_fence(to_intel_bo(object_list[i]));
				object_list[i]->do_flags &= ~__EXEC_OBJECT_HAS_FENCE;
			}
			i915_gem_object_unpin(to_intel_bo(object_list[i]));
			drm_unhold_object(object_list[i]);
		}
		pinned = 0;
		/* evict everyone we can from the aperture */
		ret = i915_gem_evict_everything(dev_priv);
		if (ret)
			goto err;
	}

	/* If we get here all involved objects are referenced, pinned, relocated
	 * and held. Now we can finish off the exec processing.
	 *
	 * First, set the pending read domains for the batch buffer to
	 * command.
	 */
	batch_obj = object_list[args->buffer_count - 1];
	batch_obj_priv = to_intel_bo(batch_obj);
	if (args->batch_start_offset + args->batch_len > batch_obj->size ||
	    batch_obj->pending_write_domain) {
		ret = EINVAL;
		goto err;
	}
	batch_obj->pending_read_domains |= I915_GEM_DOMAIN_COMMAND;

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	/*
	 * Zero the global flush/invalidate flags. These will be modified as
	 * new domains are computed for each object
	 */
	dev->invalidate_domains = 0;
	dev->flush_domains = 0;

	/* Compute new gpu domains and update invalidate/flush */
	for (i = 0; i < args->buffer_count; i++)
		i915_gem_object_set_to_gpu_domain(object_list[i]);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	/* flush and invalidate any domains that need them. */
	(void)i915_gem_flush(ring, dev->invalidate_domains,
	    dev->flush_domains);

	/*
	 * update the write domains, and fence/gpu write accounting information.
	 * Also do the move to active list here. The lazy seqno accounting will
	 * make sure that they have the correct seqno. If the add_request
	 * fails, then we will wait for a later batch (or one added on the
	 * wait), which will waste some time, but if we're that low on memory
	 * then we could fail in much worse ways.
	 */
	for (i = 0; i < args->buffer_count; i++) {
		obj = object_list[i];
		obj_priv = to_intel_bo(obj);
		drm_lock_obj(obj);

		obj->write_domain = obj->pending_write_domain;
		obj_priv->fenced_gpu_access = obj_priv->pending_fenced_gpu_access;
		/*
		 * if we have a write domain, add us to the gpu write list
		 */
		if (obj->write_domain) {
			obj_priv->pending_gpu_write = true;
			list_move_tail(&obj_priv->gpu_write_list,
				       &ring->gpu_write_list);
		}

		i915_gem_object_move_to_active(to_intel_bo(object_list[i]), ring);
		drm_unlock_obj(obj);
	}

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	if (args->flags & I915_EXEC_GEN7_SOL_RESET) {
		ret = i915_reset_gen7_sol_offsets(dev, ring);
		if (ret)
			goto err;
	}

	/* Exec the batchbuffer */
	/*
	 * XXX make sure that this may never fail by preallocating the request.
	 */
	i915_dispatch_gem_execbuffer(ring, args, batch_obj_priv->gtt_offset);

	inteldrm_verify_inactive(dev_priv, __FILE__, __LINE__);

	ret = copyout(exec_list, (void *)(uintptr_t)args->buffers_ptr,
	    sizeof(*exec_list) * args->buffer_count);

err:
	for (i = 0; i < args->buffer_count; i++) {
		if (object_list[i] == NULL)
			break;

		if (object_list[i]->do_flags & __EXEC_OBJECT_HAS_FENCE) {
			i915_gem_object_unpin_fence(to_intel_bo(object_list[i]));
			object_list[i]->do_flags &= ~__EXEC_OBJECT_HAS_FENCE;
		}

		atomic_clearbits_int(&object_list[i]->do_flags, I915_IN_EXEC |
		    I915_EXEC_NEEDS_FENCE);
		if (i < pinned) {
			i915_gem_object_unpin(to_intel_bo(object_list[i]));
			drm_unhold_and_unref(object_list[i]);
		} else {
			drm_unref(&object_list[i]->uobj);
		}
	}

unlock:
	DRM_UNLOCK();

pre_mutex_err:
	/* update userlands reloc state. */
	ret2 = i915_gem_put_relocs_to_user(exec_list,
	    args->buffer_count, relocs);
	if (ret2 != 0 && ret == 0)
		ret = ret2;

	drm_free(object_list);
	drm_free(exec_list);

	return ret;
}
