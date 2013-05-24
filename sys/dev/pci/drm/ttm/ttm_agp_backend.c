/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com>
 *          Keith Packard.
 */

#define pr_fmt(fmt) "[TTM] " fmt

#include <dev/pci/drm/ttm/ttm_module.h>
#include <dev/pci/drm/ttm/ttm_bo_driver.h>
#include <dev/pci/drm/ttm/ttm_page_alloc.h>
#ifdef TTM_HAS_AGP
#include <dev/pci/drm/ttm/ttm_placement.h>

struct ttm_agp_backend {
	struct ttm_tt ttm;
	struct agp_memory *mem;
	struct drm_agp_head *agp;
};

int	 ttm_agp_bind(struct ttm_tt *, struct ttm_mem_reg *);
int	 ttm_agp_unbind(struct ttm_tt *);
void	 ttm_agp_destroy(struct ttm_tt *);

int
ttm_agp_bind(struct ttm_tt *ttm, struct ttm_mem_reg *bo_mem)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);
	struct drm_mm_node *node = bo_mem->mm_node;
	struct agp_memory *mem;
	int ret;
//	int cached = (bo_mem->placement & TTM_PL_FLAG_CACHED);
	unsigned i;
	int pgnum;

	mem = agp_alloc_memory(agp_be->agp->agpdev, 0, /* XXX non zero type? */
	    ttm->num_pages << AGP_PAGE_SHIFT);
	if (unlikely(mem == NULL))
		return -ENOMEM;

//	mem->pages = 0;
	for (i = 0; i < ttm->num_pages; i++) {
		struct vm_page *page = ttm->pages[i];

		if (!page)
			page = ttm->dummy_read_page;

#ifdef notyet
		mem->pages[mem->page_count++] = page;
#endif
	}
	agp_be->mem = mem;

#ifdef notyet
	mem->is_flushed = 1;
	mem->type = (cached) ? AGP_USER_CACHED_MEMORY : AGP_USER_MEMORY;
#endif

	pgnum = (node->start + PAGE_SIZE - 1) / PAGE_SIZE;

	ret = agp_bind_memory(agp_be->agp->agpdev, mem, pgnum * PAGE_SIZE);
	if (ret)
		DRM_ERROR("AGP Bind memory failed\n");
//	mem->bound = agp_be->agp->base + (pgnum << PAGE_SHIFT);

	return ret;
}

int
ttm_agp_unbind(struct ttm_tt *ttm)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);

	if (agp_be->mem) {
#ifdef notyet
		if (agp_be->mem->bound)
			return agp_unbind_memory(agp_be->agp->agpdev, agp_be->mem);
#endif
		agp_free_memory(agp_be->agp->agpdev, agp_be->mem);
		agp_be->mem = NULL;
	}
	return 0;
}

void
ttm_agp_destroy(struct ttm_tt *ttm)
{
	struct ttm_agp_backend *agp_be = container_of(ttm, struct ttm_agp_backend, ttm);

	if (agp_be->mem)
		ttm_agp_unbind(ttm);
	ttm_tt_fini(ttm);
	free(agp_be, M_DRM);
}

static struct ttm_backend_func ttm_agp_func = {
	.bind = ttm_agp_bind,
	.unbind = ttm_agp_unbind,
	.destroy = ttm_agp_destroy,
};

struct ttm_tt *
ttm_agp_tt_create(struct ttm_bo_device *bdev,
				 struct drm_agp_head *agp,
				 unsigned long size, uint32_t page_flags,
				 struct vm_page *dummy_read_page)
{
	struct ttm_agp_backend *agp_be;

	agp_be = malloc(sizeof(*agp_be), M_DRM, M_WAITOK);
	if (!agp_be)
		return NULL;

	agp_be->mem = NULL;
	agp_be->agp = agp;
	agp_be->ttm.func = &ttm_agp_func;

	if (ttm_tt_init(&agp_be->ttm, bdev, size, page_flags, dummy_read_page)) {
		return NULL;
	}

	return &agp_be->ttm;
}
EXPORT_SYMBOL(ttm_agp_tt_create);

int
ttm_agp_tt_populate(struct ttm_tt *ttm)
{
	if (ttm->state != tt_unpopulated)
		return 0;

	return ttm_pool_populate(ttm);
}
EXPORT_SYMBOL(ttm_agp_tt_populate);

void
ttm_agp_tt_unpopulate(struct ttm_tt *ttm)
{
	ttm_pool_unpopulate(ttm);
}
EXPORT_SYMBOL(ttm_agp_tt_unpopulate);

#endif
