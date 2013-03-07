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
#include "intel_drv.h"

#include <machine/pmap.h>

#include <sys/queue.h>
#include <sys/workq.h>

struct change_domains {
	uint32_t invalidate_domains;
	uint32_t flush_domains;
	uint32_t flush_rings;
	uint32_t flips;
};

int	i915_reset_gen7_sol_offsets(struct drm_device *,
	    struct intel_ring_buffer *);
int	i915_gem_execbuffer_flush(struct drm_device *, uint32_t, uint32_t,
	    uint32_t);
bool	intel_enable_semaphores(struct drm_device *);
int	i915_gem_execbuffer_sync_rings(struct drm_i915_gem_object *,
	    struct intel_ring_buffer *);
int	i915_gem_execbuffer_wait_for_flips(struct intel_ring_buffer *, u32);
int	i915_gem_execbuffer_move_to_gpu(struct intel_ring_buffer *,
	    struct drm_obj **, int);
void	i915_gem_object_set_to_gpu_domain(struct drm_i915_gem_object *,
	    struct intel_ring_buffer *, struct change_domains *);
void	i915_gem_execbuffer_move_to_active(struct drm_obj **, int,
	    struct intel_ring_buffer *);
void	i915_gem_execbuffer_retire_commands(struct drm_device *,
	    struct drm_file *, struct intel_ring_buffer *);

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
 *		wait_rendering (obj) to make sure all drawing is complete.
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
i915_gem_object_set_to_gpu_domain(struct drm_i915_gem_object *obj,
				  struct intel_ring_buffer *ring,
				  struct change_domains *cd)
{
	uint32_t invalidate_domains = 0, flush_domains = 0;

	/*
	 * If the object isn't moving to a new write domain,
	 * let the object stay in multiple read domains
	 */
	if (obj->base.pending_write_domain == 0)
		obj->base.pending_read_domains |= obj->base.read_domains;

	/*
	 * Flush the current write domain if
	 * the new read domains don't match. Invalidate
	 * any read domains which differ from the old
	 * write domain
	 */
	if (obj->base.write_domain &&
	    (((obj->base.write_domain != obj->base.pending_read_domains ||
	       obj->ring != ring)) ||
	     (obj->fenced_gpu_access && !obj->pending_fenced_gpu_access))) {
		flush_domains |= obj->base.write_domain;
		invalidate_domains |=
			obj->base.pending_read_domains & ~obj->base.write_domain;
	}
	/*
	 * Invalidate any read caches which may have
	 * stale data. That is, any new read domains.
	 */
	invalidate_domains |= obj->base.pending_read_domains & ~obj->base.read_domains;
	if ((flush_domains | invalidate_domains) & I915_GEM_DOMAIN_CPU)
		i915_gem_clflush_object(obj);

	if ((flush_domains | invalidate_domains) & I915_GEM_DOMAIN_GTT) {
		inteldrm_wipe_mappings(&obj->base);
	}

	if (obj->base.pending_write_domain)
		cd->flips |= atomic_read(&obj->pending_flip);

	/* The actual obj->write_domain will be updated with
	 * pending_write_domain after we emit the accumulated flush for all
	 * of our domain changes in execbuffers (which clears objects'
	 * write_domains).  So if we have a current write domain that we
	 * aren't changing, set pending_write_domain to that.
	 */
	if (flush_domains == 0 && obj->base.pending_write_domain == 0)
		obj->base.pending_write_domain = obj->base.write_domain;

	cd->invalidate_domains |= invalidate_domains;
	cd->flush_domains |= flush_domains;
	if (flush_domains & I915_GEM_GPU_DOMAINS)
		cd->flush_rings |= intel_ring_flag(obj->ring);
	if (invalidate_domains & I915_GEM_GPU_DOMAINS)
		cd->flush_rings |= intel_ring_flag(ring);
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

int
i915_gem_execbuffer_flush(struct drm_device *dev,
			  uint32_t invalidate_domains,
			  uint32_t flush_domains,
			  uint32_t flush_rings)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i, ret;

	if (flush_domains & I915_GEM_DOMAIN_CPU)
		inteldrm_chipset_flush(dev_priv);

	if (flush_domains & I915_GEM_DOMAIN_GTT)
		DRM_WRITEMEMORYBARRIER();

	if ((flush_domains | invalidate_domains) & I915_GEM_GPU_DOMAINS) {
		for (i = 0; i < I915_NUM_RINGS; i++)
			if (flush_rings & (1 << i)) {
				ret = i915_gem_flush_ring(&dev_priv->rings[i],
							  invalidate_domains,
							  flush_domains);
				if (ret)
					return ret;
			}
	}

	return 0;
}

bool
intel_enable_semaphores(struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen < 6)
		return 0;

	if (i915_semaphores >= 0)
		return i915_semaphores;

	/* Disable semaphores on SNB */
	if (INTEL_INFO(dev)->gen == 6)
		return 0;

	return 1;
}

int
i915_gem_execbuffer_sync_rings(struct drm_i915_gem_object *obj,
			       struct intel_ring_buffer *to)
{
	struct intel_ring_buffer *from = obj->ring;
	u32 seqno;
	int ret, idx;

	if (from == NULL || to == from)
		return 0;

	/* XXX gpu semaphores are implicated in various hard hangs on SNB */
	if (!intel_enable_semaphores(obj->base.dev))
		return i915_gem_object_wait_rendering(obj, false);
	idx = intel_ring_sync_index(from, to);

	seqno = obj->last_read_seqno;
	if (seqno <= from->sync_seqno[idx])
		return 0;

	if (seqno == from->outstanding_lazy_request) {
		ret = i915_add_request(from, NULL, &seqno);
		if (ret) {
			return ret;
		}
	}

	from->sync_seqno[idx] = seqno;

	return to->sync_to(to, from, seqno);
}

int
i915_gem_execbuffer_wait_for_flips(struct intel_ring_buffer *ring, u32 flips)
{
	u32 plane, flip_mask;
	int ret;

	/* Check for any pending flips. As we only maintain a flip queue depth
	 * of 1, we can simply insert a WAIT for the next display flip prior
	 * to executing the batch and avoid stalling the CPU.
	 */

	for (plane = 0; flips >> plane; plane++) {
		if (((flips >> plane) & 1) == 0)
			continue;

		if (plane)
			flip_mask = MI_WAIT_FOR_PLANE_B_FLIP;
		else
			flip_mask = MI_WAIT_FOR_PLANE_A_FLIP;

		ret = intel_ring_begin(ring, 2);
		if (ret)
			return ret;

		intel_ring_emit(ring, MI_WAIT_FOR_EVENT | flip_mask);
		intel_ring_emit(ring, MI_NOOP);
		intel_ring_advance(ring);
	}

	return 0;
}

int
i915_gem_execbuffer_move_to_gpu(struct intel_ring_buffer *ring,
   struct drm_obj **object_list, int buffer_count)
{
	struct drm_i915_gem_object *obj;
	struct change_domains cd;
	int ret, i;

	memset(&cd, 0, sizeof(cd));
	for (i = 0; i < buffer_count; i++) {
		obj = to_intel_bo(object_list[i]);
		i915_gem_object_set_to_gpu_domain(obj, ring, &cd);
	}

	if (cd.invalidate_domains | cd.flush_domains) {
		ret = i915_gem_execbuffer_flush(ring->dev,
						cd.invalidate_domains,
						cd.flush_domains,
						cd.flush_rings);
		if (ret)
			return ret;
	}

	if (cd.flips) {
		ret = i915_gem_execbuffer_wait_for_flips(ring, cd.flips);
		if (ret)
			return ret;
	}

	for (i = 0; i < buffer_count; i++) {
		obj = to_intel_bo(object_list[i]);
		ret = i915_gem_execbuffer_sync_rings(obj, ring);
		if (ret)
			return ret;
	}

	return 0;
}

// i915_gem_check_execbuffer
// validate_exec_list

void
i915_gem_execbuffer_move_to_active(struct drm_obj **object_list,
    int buffer_count, struct intel_ring_buffer *ring)
{
	struct drm_i915_gem_object *obj;
	int i;

	for (i = 0; i < buffer_count; i++) {
		obj = to_intel_bo(object_list[i]);
#if 0
		u32 old_read = obj->base.read_domains;
		u32 old_write = obj->base.write_domain;
#endif

		obj->base.read_domains = obj->base.pending_read_domains;
		obj->base.write_domain = obj->base.pending_write_domain;
		obj->fenced_gpu_access = obj->pending_fenced_gpu_access;

		i915_gem_object_move_to_active(obj, ring);
		if (obj->base.write_domain) {
			obj->dirty = 1;
			obj->pending_gpu_write = true;
			list_move_tail(&obj->gpu_write_list,
				       &ring->gpu_write_list);
			intel_mark_busy(ring->dev);
		}

//		trace_i915_gem_object_change_domain(obj, old_read, old_write);
	}
}

void
i915_gem_execbuffer_retire_commands(struct drm_device *dev,
				    struct drm_file *file,
				    struct intel_ring_buffer *ring)
{
	u32 invalidate;

	/*
	 * Ensure that the commands in the batch buffer are
	 * finished before the interrupt fires.
	 *
	 * The sampler always gets flushed on i965 (sigh).
	 */
	invalidate = I915_GEM_DOMAIN_COMMAND;
	if (INTEL_INFO(dev)->gen >= 4)
		invalidate |= I915_GEM_DOMAIN_SAMPLER;
	if (ring->flush(ring, invalidate, 0)) {
		i915_gem_next_request_seqno(ring);
		return;
	}

	/* Add a breadcrumb for the completion of the batch buffer */
	if (i915_add_request(ring, file, NULL))
		i915_gem_next_request_seqno(ring);
}

// i915_gem_fix_mi_batchbuffer_end

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
	struct drm_i915_gem_object		*batch_obj_priv;
	struct drm_obj				**object_list = NULL;
	struct drm_obj				*batch_obj, *obj;
	struct intel_ring_buffer		*ring;
	size_t					 oflow;
	int					 ret, ret2, i;
	int					 pinned = 0, pin_tries;
	uint32_t				 reloc_index;
	uint32_t				 seqno, flags;
	uint32_t				 exec_start, exec_len;

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

	flags = 0;
	if (args->flags & I915_EXEC_SECURE) {
		if (!DRM_SUSER(curproc))
			return (EPERM);

		flags |= I915_DISPATCH_SECURE;
	}
	if (args->flags & I915_EXEC_IS_PINNED)
		flags |= I915_DISPATCH_PINNED;

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

	ret = i915_gem_execbuffer_move_to_gpu(ring, object_list,
	    args->buffer_count);
	if (ret)
		goto err;

	seqno = i915_gem_next_request_seqno(ring);
	for (i = 0; i < ARRAY_SIZE(ring->sync_seqno); i++) {
		if (seqno < ring->sync_seqno[i]) {
			/* The GPU can not handle its semaphore value wrapping,
			 * so every billion or so execbuffers, we need to stall
			 * the GPU in order to reset the counters.
			 */
			ret = i915_gpu_idle(dev);
			if (ret)
				goto err;

			BUG_ON(ring->sync_seqno[i]);
                }
	}

	if (args->flags & I915_EXEC_GEN7_SOL_RESET) {
		ret = i915_reset_gen7_sol_offsets(dev, ring);
		if (ret)
			goto err;
	}

	/* Exec the batchbuffer */
	/*
	 * XXX make sure that this may never fail by preallocating the request.
	 */

	exec_start = batch_obj_priv->gtt_offset + args->batch_start_offset;
	exec_len = args->batch_len;

	ret = ring->dispatch_execbuffer(ring, exec_start, exec_len, flags);
	if (ret)
		goto err;

	i915_gem_execbuffer_move_to_active(object_list, args->buffer_count, ring);
	i915_gem_execbuffer_retire_commands(dev, file_priv, ring);

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
