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
#ifndef STUB_X86_ASM_ATOMIC_H
#define STUB_X86_ASM_ATOMIC_H

#include_next <asm/atomic.h>

#define try_cmpxchg(_ptr, _pold, _new)					\
({									\
	__typeof__(_ptr) _old = (__typeof__(_ptr))(_pold);		\
	__typeof__(*(_ptr)) __old = *_old;				\
	__typeof__(*(_ptr)) __cur = cmpxchg64(_ptr, __old, _new);	\
	bool success = __cur == __old;					\
	if (unlikely(!success))						\
		*_old = __cur;						\
	likely(success);						\
})

#endif
