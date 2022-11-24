// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, ProvenRun S.A.S
 */
/**
 * @file smc.h
 * @brief Internal provencore driver definitions for SMC handling
 *
 * This file is supposed to be shared between all provencore driver files only.
 *
 * @author Alexandre Berdery
 * @date October 6th, 2020 (creation)
 * @copyright (c) 2020-2021, Prove & Run and/or its affiliates.
 *   All rights reserved.
 */

#ifndef PNC_SMC_H_INCLUDED
#define PNC_SMC_H_INCLUDED

#include <linux/version.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,3,6)
#define ARM_SMCCC_OWNER_TRUSTED_OS 50
#else
#include <linux/arm-smccc.h>
#endif

/* Structure to package SMC params */
struct pnc_smc_params {
	uint32_t a0;
	uint32_t a1;
	uint32_t a2;
	uint32_t a3;
	uint32_t a4;
	uint32_t a5;
	uint32_t a6;
	uint32_t a7;
};

/* TZSW owned SMCs: Provencore uses '63' */
#define ARM_SMCCC_OWNER_PNC (ARM_SMCCC_OWNER_TRUSTED_OS + 13)

/* Below are constants used by REE driver for SMC handling */

#define LINUX_SHARED_MEM_TAG	0xcafe

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,3,6)
/* We keep part of our legacy tzapi for older kernels that would like to use
 * REE V3: ARM SMC calling convention is natively built in Linux kernel only
 * after 4.3.6...
 */
#define SMC_32BIT               UINT32_C(0)
#define SMC_FASTCALL            UINT32_C(0x80000000)
#define SMC_OWNER_MASK          UINT32_C(0x3f)
#define SMC_OWNER_SHIFT         24
#define SMC_OWNER(owner)        (((owner) & SMC_OWNER_MASK) << SMC_OWNER_SHIFT)
#define SMC_FUNC_MASK           UINT32_C(0xffff)
#define SMC_FUNC(func)          ((func) & SMC_FUNC_MASK)
#define SMC_FUNC_ID(arch, type, owner, func) \
    ((arch) | (type) | SMC_OWNER(owner) | SMC_FUNC(func))

#define SMC_ACTION_FROM_NS \
    SMC_FUNC_ID(SMC_32BIT, SMC_FASTCALL, ARM_SMCCC_OWNER_PNC, 4)

#define SMC_CONFIG_SHAREDMEM \
    SMC_FUNC_ID(SMC_32BIT, SMC_FASTCALL, ARM_SMCCC_OWNER_PNC, 3)
#else
#define SMC_ACTION_FROM_NS	\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, \
		ARM_SMCCC_OWNER_PNC, 4)

#define SMC_CONFIG_SHAREDMEM	\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, \
		ARM_SMCCC_OWNER_PNC, 3)
#endif

/**
 * @brief Schedule SMC execution the CPU 0.
 *
 * If the calling process is executed on CPU != 0, the function schedules SMC
 * work on CPU#0. Otherwise, it directly executes SMC.
 *
 * @param params        Parameters to the SMC call
 */
void pnc_sched_smc(struct pnc_smc_params *params);

/**
 * @brief Init REE driver's SMC framework.
 *
 * Only called during module init.
 */
int pnc_smc_init(void);

/**
 * @brief Release REE driver's SMC framework.
 *
 * Only called during module exit.
 */ 
void pnc_smc_exit(void);

#endif /* PNC_SMC_H_INCLUDED */
