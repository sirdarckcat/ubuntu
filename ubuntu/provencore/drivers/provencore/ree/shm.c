// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, ProvenRun S.A.S
 */
/**
 * @file
 * @brief
 * @author Henri Chataing
 * @date December 20th, 2017 (creation)
 * @copyright (c) 2017-2021, Prove & Run and/or its affiliates.
 *   All rights reserved.
 */

#include <linux/semaphore.h>
#include <linux/slab.h>

#include "internal.h"
#include "shm.h"
#include "ree.h"
#include "smc.h"

/** SHM virtual base addr */
static void *_shm_base = NULL;

/** SHM physical base addr */
static uint64_t _shm_pbase;

/** SHM total num of pages */
static unsigned int _shm_nr_pages;

/** Semaphore used to restrict access to block allocator. */
static struct semaphore _shm_sem;

/**
 * List shared memory blocks ; this list constitutes a partition of the total
 * shared memory area.
 */
static pnc_shm_block_t *_blocks;

int pnc_shm_init(void *vbase, uint64_t pbase, unsigned int nr_pages)
{
    pnc_shm_block_t *b;

    /* Store base addresses */
    _shm_base     = vbase;
    _shm_pbase    = pbase;
    _shm_nr_pages = nr_pages;

    /* Initialize the mem semaphore. */
    sema_init(&_shm_sem, 1);

    /* Create and insert original block. */
    b = kmalloc(sizeof(pnc_shm_block_t), GFP_KERNEL);
    if (b == NULL) {
        return -ENOMEM;
    }

    b->offset = REE_RESERVED_PAGES;
    b->nr_pages = nr_pages - REE_RESERVED_PAGES;
    b->free = 1;
    b->next = NULL;
    _blocks = b;
    return 0;
}

void pnc_shm_exit(void)
{
    pnc_shm_block_t *b = _blocks, *n;

    while (b != NULL) {
        n = b->next;
        kfree(b);
        b = n;
    }

    _blocks = NULL;
}

int pnc_shm_alloc(unsigned int nr_pages, pnc_shm_block_t **block)
{
    pnc_shm_block_t *b = _blocks, *n;
    int ret = 0;

    if (down_interruptible(&_shm_sem)) {
        return -ERESTARTSYS;
    }

    while (b != NULL) {
        n = b->next;
        if (!b->free) {
            b = n;
            continue;
        }
        /* Nice and all. */
        if (b->nr_pages == nr_pages) {
            goto found;
        }
        /* Not large enough, but check next block. */
        else if (b->nr_pages < nr_pages) {
            if (n == NULL) {
                break;
            }
            if (n->free) {
                b->nr_pages += n->nr_pages;
                b->next = n->next;
                kfree(n);
            } else {
                b = n->next;
                /* continue */
            }
        }
        /* Too large, check if can do away without splitting. */
        else if (n != NULL && n->free) {
            n->nr_pages += b->nr_pages - nr_pages;
            n->offset = b->offset + nr_pages;
            b->nr_pages = nr_pages;
            goto found;
        }
        /* Do split the block. */
        else {
            goto split;
        }
    }
    ret = -ENOMEM;
    goto err;

split:
    n = kmalloc(sizeof(pnc_shm_block_t), GFP_KERNEL);
    if (n == NULL) {
        ret = -ENOMEM;
        goto err;
    }
    n->next = b->next;
    n->offset = b->offset + nr_pages;
    n->nr_pages = b->nr_pages - nr_pages;
    n->free = 1;
    b->next = n;
    b->nr_pages = nr_pages;
found:
    pr_debug("shm alloc range [%#.8x - %#.8x]\n", b->offset,
        b->offset + b->nr_pages);
    b->free = 0;
    *block = b;
err:
    up(&_shm_sem);
    return ret;
}

int pnc_shm_free(pnc_shm_block_t *b)
{
    if (b == NULL) {
        return 0;
    }

    pr_debug("shm free range [%#.8x - %#.8x]\n", b->offset,
        b->offset + b->nr_pages);

    if (down_interruptible(&_shm_sem)) {
        return -ERESTARTSYS;
    }
    b->free = 1;
    up(&_shm_sem);
    return 0;
}

_Bool pnc_shm_ready(void)
{
    pnc_header_t *header = (pnc_header_t *)_shm_base;
    if (header)
        return __atomic_load_n(&header->magic, __ATOMIC_ACQUIRE) == REE_MAGIC_2;
    else
        return 0;
}

void *pnc_shm_base(void)
{
    return _shm_base;
}

/**
 * @brief Init shm header
 *
 * @param base  SHM base address
 */
void pnc_shm_init_header(void)
{
    pnc_header_t *header = (pnc_header_t *)_shm_base;

    /* Init SHM header */
    header->version         = REE_VERSION;
    header->reserved_pages  = REE_RESERVED_PAGES;
    header->max_sessions    = REE_MAX_SESSIONS;

    /* Atomically mark the header as initialized */
    __atomic_store_n(&header->magic, REE_MAGIC_1, __ATOMIC_RELEASE);
}

void pnc_shm_forward(void)
{
    struct pnc_smc_params params;

    params.a0 = SMC_CONFIG_SHAREDMEM;
    params.a1 = _shm_pbase;
    params.a2 = _shm_pbase >> 32;
    params.a3 = _shm_nr_pages * PAGE_SIZE;
    params.a4 = LINUX_SHARED_MEM_TAG;
    pnc_sched_smc(&params);
}
