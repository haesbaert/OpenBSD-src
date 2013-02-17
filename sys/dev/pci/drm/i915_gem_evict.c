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

int
i915_gem_evict_something(struct inteldrm_softc *dev_priv, size_t min_size)
{
	struct drm_obj		*obj;
	struct drm_i915_gem_request *request;
	struct drm_i915_gem_object *obj_priv;
	struct intel_ring_buffer *ring;
	u_int32_t		 seqno;
	int			 ret = 0, write_domain = 0, i;
	int			 found;

	for (;;) {
		i915_gem_retire_requests(dev_priv);

		/* If there's an inactive buffer available now, grab it
		 * and be done.
		 */
		obj = i915_gem_find_inactive_object(dev_priv, min_size);
		if (obj != NULL) {
			obj_priv = to_intel_bo(obj);
			/* find inactive object returns the object with a
			 * reference for us, and held
			 */
			KASSERT(obj_priv->pin_count == 0);
			KASSERT(!obj_priv->active);
			DRM_ASSERT_HELD(obj);

			/* Wait on the rendering and unbind the buffer. */
			ret = i915_gem_object_unbind(obj_priv);
			drm_unhold_and_unref(obj);
			return (ret);
		}

		/* If we didn't get anything, but the ring is still processing
		 * things, wait for one of those things to finish and hopefully
		 * leave us a buffer to evict.
		 */
		found = 0;
		mtx_enter(&dev_priv->request_lock);
		for_each_ring(ring, dev_priv, i) {
			if (!list_empty(&ring->request_list)) {
				request = list_first_entry(&ring->request_list,
				    struct drm_i915_gem_request, list);

				seqno = request->seqno;
				mtx_leave(&dev_priv->request_lock);

				ret = i915_wait_seqno(request->ring, seqno);
				if (ret)
					return (ret);

				found = 1;
				break;
			}
		}
		if (found)
			continue;
		mtx_leave(&dev_priv->request_lock);

		/* If we didn't have anything on the request list but there
		 * are buffers awaiting a flush, emit one and try again.
		 * When we wait on it, those buffers waiting for that flush
		 * will get moved to inactive.
		 */
		mtx_enter(&dev_priv->list_lock);
		list_for_each_entry(obj_priv, &dev_priv->mm.flushing_list,
		    mm_list) {
			obj = &obj_priv->base;
			if (obj->size >= min_size) {
				write_domain = obj->write_domain;
				break;
			}
			obj = NULL;
		}
		mtx_leave(&dev_priv->list_lock);

		if (write_domain) {
			if (i915_gem_flush(obj_priv->ring, write_domain,
			    write_domain) == 0)
				return (ENOMEM);
			continue;
		}

		/*
		 * If we didn't do any of the above, there's no single buffer
		 * large enough to swap out for the new one, so just evict
		 * everything and start again. (This should be rare.)
		 */
		if (!list_empty(&dev_priv->mm.inactive_list))
			return (i915_gem_evict_inactive(dev_priv));
		else
			return (i915_gem_evict_everything(dev_priv));
	}
	/* NOTREACHED */
}

int
i915_gem_evict_everything(struct inteldrm_softc *dev_priv)
{
	struct drm_device *dev = (struct drm_device *)dev_priv->drmdev;
	int		ret;

	if (list_empty(&dev_priv->mm.inactive_list) &&
	    list_empty(&dev_priv->mm.flushing_list) &&
	    list_empty(&dev_priv->mm.active_list))
		return (ENOSPC);

	/* The gpu_idle will flush everything in the write domain to the
	 * active list. Then we must move everything off the active list
	 * with retire requests.
	 */
	ret = i915_gpu_idle(dev);
	if (ret)
		return ret;

	i915_gem_retire_requests(dev_priv);

	i915_gem_evict_inactive(dev_priv);

	/*
	 * All lists should be empty because we flushed the whole queue, then
	 * we evicted the whole shebang, only pinned objects are still bound.
	 */
	KASSERT(list_empty(&dev_priv->mm.inactive_list));
	KASSERT(list_empty(&dev_priv->mm.flushing_list));
	KASSERT(list_empty(&dev_priv->mm.active_list));

	return (0);
}

/* Clear out the inactive list and unbind everything in it. */
int
i915_gem_evict_inactive(struct inteldrm_softc *dev_priv)
{
	struct drm_i915_gem_object *obj_priv, *next;
	int			 ret = 0;

	mtx_enter(&dev_priv->list_lock);
	list_for_each_entry_safe(obj_priv, next,
				 &dev_priv->mm.inactive_list, mm_list) {
		if (obj_priv->pin_count != 0) {
			ret = EINVAL;
			DRM_ERROR("Pinned object in unbind list\n");
			break;
		}
		/* reference it so that we can frob it outside the lock */
		drm_ref(&obj_priv->base.uobj);
		mtx_leave(&dev_priv->list_lock);

		drm_hold_object(&obj_priv->base);
		ret = i915_gem_object_unbind(obj_priv);
		drm_unhold_and_unref(&obj_priv->base);

		mtx_enter(&dev_priv->list_lock);
		if (ret)
			break;
	}
	mtx_leave(&dev_priv->list_lock);

	return (ret);
}
