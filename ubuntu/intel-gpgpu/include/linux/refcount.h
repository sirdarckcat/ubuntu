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
/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_REFCOUNT
#include_next <linux/refcount.h>
#else
#undef refcount_warn_saturate
#define refcount_warn_saturate LINUX_I915_BACKPORT(refcount_warn_saturate)
#undef refcount_dec_if_one
#define refcount_dec_if_one LINUX_I915_BACKPORT(refcount_dec_if_one)
#undef refcount_dec_not_one
#define refcount_dec_not_one LINUX_I915_BACKPORT(refcount_dec_not_one)
#undef refcount_dec_and_mutex_lock
#define refcount_dec_and_mutex_lock LINUX_I915_BACKPORT(refcount_dec_and_mutex_lock)
#undef refcount_dec_and_lock
#define refcount_dec_and_lock LINUX_I915_BACKPORT(refcount_dec_and_lock)
#undef refcount_dec_and_lock_irqsave
#define refcount_dec_and_lock_irqsave LINUX_I915_BACKPORT(refcount_dec_and_lock_irqsave)
#include <linux/backport-refcount.h>
#endif /* CPTCFG_BPAUTO_REFCOUNT */
