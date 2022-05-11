/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("LICENSE"). Unless the LICENSE provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no
 * express or implied warranties, other than those that are expressly
 * stated in the License.
 *
 */
#ifndef __BACKPORT_RHASHTABLE_H
#define __BACKPORT_RHASHTABLE_H
#include_next <linux/rhashtable.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,12,0)
/**
 * rhashtable_lookup_get_insert_fast - lookup and insert object into hash table
 * @ht:		hash table
 * @obj:	pointer to hash head inside object
 * @params:	hash table parameters
 *
 * Just like rhashtable_lookup_insert_fast(), but this function returns the
 * object if it exists, NULL if it did not and the insertion was successful,
 * and an ERR_PTR otherwise.
 */
#define rhashtable_lookup_get_insert_fast LINUX_I915_BACKPORT(rhashtable_lookup_get_insert_fast)
static inline void *rhashtable_lookup_get_insert_fast(
	struct rhashtable *ht, struct rhash_head *obj,
	const struct rhashtable_params params)
{
	const char *key = rht_obj(ht, obj);

	BUG_ON(ht->p.obj_hashfn);

	return __rhashtable_insert_fast(ht, key + ht->p.key_offset, obj, params,
					false);
}
#endif

#endif /* __BACKPORT_RHASHTABLE_H */
