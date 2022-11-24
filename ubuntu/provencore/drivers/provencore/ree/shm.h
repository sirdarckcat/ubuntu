// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, ProvenRun S.A.S
 */
/**
 * @file
 * @brief
 * @author Henri Chataing
 * @date December 19th, 2017 (creation)
 * @copyright (c) 2017-2021, Prove & Run and/or its affiliates.
 *   All rights reserved.
 */

#ifndef SHM_H_INCLUDED
#define SHM_H_INCLUDED

/**
 * @brief Represent a page range in the shared memory area.
 */
typedef struct pnc_shm_block
{
    /* Availability of the block */
    _Bool free;
    /* Offset of the first page in the shared memory area. */
    unsigned int offset;
    /* Block size in pages. */
    unsigned int nr_pages;
    /* Next block in the list. */
    struct pnc_shm_block *next;
} pnc_shm_block_t;

/**
 * @brief Initialise the structures used by the block allocator.
 *   The list \ref _blocks is created with one initial block representing the
 *   full range of available pages.
 *
 * @param vbase         Virtual base addr of contiguous allocated memory
 * @param pbase         Physical base addr of contiguous allocated memory
 * @param nr_pages      Number of allocated pages
 * @return              - 0 on success
 *                      - -ENOMEM on internal allocation failure
 */
int pnc_shm_init(void *vbase, uint64_t pbase, unsigned int nr_pages);

/**
 * @brief Destroy the structures used by the block allocator.
 */
void pnc_shm_exit(void);

/**
 * @brief Allocate a block of \p nr_pages pages.
 * @param nr_pages      Requested number of pages
 * @param block         Pointer to the allocated block
 * @return              - -ERESTARTSYS if the lock could not be acquired
 *                      - -ENOMEM on allocation failure
 *                      - 0 on success
 */
int pnc_shm_alloc(unsigned int nr_pages, pnc_shm_block_t **b);

/**
 * @brief Release the shared memory block \p b.
 * @param b             Memory block to be released
 * @return              - -ERESTARTSYS if the lock could not be acquired
 *                      - 0 on success
 */
int pnc_shm_free(pnc_shm_block_t *b);

/**
 * @brief Check whether Secure world finalized SHM initialization.
 */
_Bool pnc_shm_ready(void);

/**
 * @brief returns SHM base addr.
 */
void *pnc_shm_base(void);

/**
 * @brief Init SHM header
 */
void pnc_shm_init_header(void);

/**
 * @brief Forward SHM geometry to the secure monitor
 */
void pnc_shm_forward(void);

#endif /* SHM_H_INCLUDED */
