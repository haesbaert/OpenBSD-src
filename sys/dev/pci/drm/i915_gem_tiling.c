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

// i915_gem_detect_bit_6_swizzle

int
i915_tiling_ok(struct drm_device *dev, int stride, int size, int tiling_mode)
{
	struct inteldrm_softc	*dev_priv = dev->dev_private;
	int			 tile_width;

	/* Linear is always ok */
	if (tiling_mode == I915_TILING_NONE)
		return (1);

	if (!IS_I9XX(dev_priv) || (tiling_mode == I915_TILING_Y &&
	    HAS_128_BYTE_Y_TILING(dev_priv)))
		tile_width = 128;
	else
		tile_width = 512;

	/* Check stride and size constraints */
	if (INTEL_INFO(dev_priv)->gen >= 4) {
		/* fence reg has end address, so size is ok */
		if (stride / 128 > I965_FENCE_MAX_PITCH_VAL)
			return (0);
	} else if (IS_GEN3(dev_priv) || IS_GEN2(dev_priv)) {
		if (stride > 8192)
			return (0);
		if (IS_GEN3(dev_priv)) {
			if (size > I830_FENCE_MAX_SIZE_VAL << 20)
				return (0);
		} else if (size > I830_FENCE_MAX_SIZE_VAL << 19)
			return (0);
	}

	/* 965+ just needs multiples of the tile width */
	if (INTEL_INFO(dev_priv)->gen >= 4)
		return ((stride & (tile_width - 1)) == 0);

	/* Pre-965 needs power-of-two */
	if (stride < tile_width || stride & (stride - 1) ||
	    i915_get_fence_size(dev_priv, size) != size)
		return (0);
	return (1);
}

// i915_gem_object_fence_ok

/**
 * Sets the tiling mode of an object, returning the required swizzling of
 * bit 6 of addresses in the object.
 */
int
i915_gem_set_tiling(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_i915_gem_set_tiling	*args = data;
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_obj			*obj;
	struct drm_i915_gem_object	*obj_priv;
	int				 ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	obj_priv = (struct drm_i915_gem_object *)obj;
	drm_hold_object(obj);

	if (obj_priv->pin_count != 0) {
		ret = EBUSY;
		goto out;
	}
	if (i915_tiling_ok(dev, args->stride, obj->size,
	    args->tiling_mode) == 0) {
		ret = EINVAL;
		goto out;
	}

	if (args->tiling_mode == I915_TILING_NONE) {
		args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
		args->stride = 0;
	} else {
		if (args->tiling_mode == I915_TILING_X)
			args->swizzle_mode = dev_priv->mm.bit_6_swizzle_x;
		else
			args->swizzle_mode = dev_priv->mm.bit_6_swizzle_y;
		/* If we can't handle the swizzling, make it untiled. */
		if (args->swizzle_mode == I915_BIT_6_SWIZZLE_UNKNOWN) {
			args->tiling_mode = I915_TILING_NONE;
			args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
			args->stride = 0;
		}
	}

	if (args->tiling_mode != obj_priv->tiling_mode ||
	    args->stride != obj_priv->stride) {
		/*
		 * We need to rebind the object if its current allocation no
		 * longer meets the alignment restrictions for its new tiling
		 * mode. Otherwise we can leave it alone, but must clear any
		 * fence register.
		 */
		/* fence may no longer be correct, wipe it */
		inteldrm_wipe_mappings(obj);
		if (obj_priv->fence_reg != I915_FENCE_REG_NONE)
			atomic_setbits_int(&obj->do_flags,
			    I915_FENCE_INVALID);
		obj_priv->tiling_mode = args->tiling_mode;
		obj_priv->stride = args->stride;
	}

out:
	drm_unhold_and_unref(obj);

	return (ret);
}

/**
 * Returns the current tiling mode and required bit 6 swizzling for the object.
 */
int
i915_gem_get_tiling(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_i915_gem_get_tiling	*args = data;
	struct inteldrm_softc		*dev_priv = dev->dev_private;
	struct drm_obj			*obj;
	struct drm_i915_gem_object	*obj_priv;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return (EBADF);
	drm_hold_object(obj);
	obj_priv = (struct drm_i915_gem_object *)obj;

	args->tiling_mode = obj_priv->tiling_mode;
	switch (obj_priv->tiling_mode) {
	case I915_TILING_X:
		args->swizzle_mode = dev_priv->mm.bit_6_swizzle_x;
		break;
	case I915_TILING_Y:
		args->swizzle_mode = dev_priv->mm.bit_6_swizzle_y;
		break;
	case I915_TILING_NONE:
		args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
		break;
	default:
		DRM_ERROR("unknown tiling mode\n");
	}

	drm_unhold_and_unref(obj);

	return 0;
}

// i915_gem_swizzle_page
// i915_gem_object_do_bit_17_swizzle
// i915_gem_object_save_bit_17_swizzle

