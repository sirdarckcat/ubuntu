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
#ifndef STUB_X86_ASM_PGTABLE_H
#define STUB_X86_ASM_PGTABLE_H 1

/* ALL THIS IS **PLAIN WRONG** - just allowing to compile for now */

#include_next <asm/pgtable-types.h>

#define _PAGE_BIT_PWT           3       /* page write through */
#define _PAGE_BIT_PCD           4       /* page cache disabled */
#define _PAGE_BIT_PAT           7       /* on 4KB pages */

#define _PAGE_PWT       (_AT(pteval_t, 1) << _PAGE_BIT_PWT)
#define _PAGE_PCD       (_AT(pteval_t, 1) << _PAGE_BIT_PCD)
#define _PAGE_PAT       (_AT(pteval_t, 1) << _PAGE_BIT_PAT)

#endif
