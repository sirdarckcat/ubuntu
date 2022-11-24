// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, ProvenRun S.A.S
 */
/**
 * @file
 * @brief Misc internal functions shared with different parts of the REE driver
 * @author Alexandre Berdery
 * @date November 26th, 2020 (creation)
 * @copyright (c) 2020-2021, Prove & Run and/or its affiliates.
 *   All rights reserved.
 */

#include <linux/smp.h>
#include <linux/workqueue.h>

#include "smc.h"

#ifdef CONFIG_SMP
/* Use a dedicated workqueue to schedule SMC work on CPU #0 */
static struct workqueue_struct *smc_wq = NULL;
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,3,6)
static void do_arm_smc_call(struct pnc_smc_params *params)
{
#if defined(__aarch64__)
    __asm__ __volatile__ (
            "mov    x8, %[params]    \n\t"
            "ldp    w0, w1, [x8], #8 \n\t"
            "ldp    w2, w3, [x8], #8 \n\t"
            "ldp    w4, w5, [x8], #8 \n\t"
            "ldp    w6, w7, [x8], #8 \n\t"
            "sub    x8, x8, #32      \n\t"
            "smc    #0               \n\t"
            "stp    w0, w1, [x8], #8 \n\t"
            "stp    w2, w3, [x8], #8 \n\t"
            "stp    w4, w5, [x8], #8 \n\t"
            "stp    w6, w7, [x8], #8 \n\t"
            "sub    x8, x8, #32      \n\t"
            :: [params] "r" (params)
            : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "memory");
#elif defined(__arm__)
    __asm__ __volatile__ (
            "mov	r8, %[params]	\n\t"
            "ldm	r8, {r0-r7}	\n\t"
            ".arch_extension sec	\n\t"
            "smc	#0		\n\t"
            "stm	r8, {r0-r7}	\n\t"
            :: [params] "r" (params)
            : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "memory");
#else
#error "Unsupported smc on this architecture"
#endif
}
#else
static void do_arm_smc_call(struct pnc_smc_params *params)
{
    struct arm_smccc_res res;

    arm_smccc_smc(params->a0, params->a1, params->a2, params->a3, params->a4,
        params->a5, params->a6, params->a7, &res);

    params->a0 = res.a0;
    params->a1 = res.a1;
    params->a2 = res.a2;
    params->a3 = res.a3;
}
#endif

static void do_smc(struct pnc_smc_params *params)
{
    dsb(ish);
    do_arm_smc_call(params);
}

#ifdef CONFIG_SMP
/**
 * Work struct for a scheduled smc operation.
 */
struct smc_work_struct {
    struct work_struct work;
    struct pnc_smc_params *params;
};

/**
 * @brief Raise an SMC with the parameters found in the \ref pnc_call_work_struct
 *  which encloses \p work.
 * @param work          Pointer to the work contained in a pnc_call_work_struct
 *                      object
 */
static void do_smc_work_handler(struct work_struct *work)
{
    struct smc_work_struct *obj = container_of(work, struct smc_work_struct,
        work);
    struct pnc_smc_params *params = obj->params;
    do_smc(params);
}
#endif /* CONFIG_SMP */

void pnc_sched_smc(struct pnc_smc_params *params)
{
#ifndef CONFIG_SMP
    do_smc(params);
#else
    struct smc_work_struct work;
    int cpuid;
    /* Retrieve the CPU id and prevent rescheduling to a different CPU */
    cpuid = get_cpu();
    if (cpuid == 0) {
        do_smc(params);
        put_cpu();
    } else {
        put_cpu();
        work.params = params;
        INIT_WORK(&work.work, do_smc_work_handler);
        queue_work_on(0, smc_wq, &work.work);
        flush_work(&work.work);
    }
 #endif /* !CONFIG_SMP */
}

int pnc_smc_init(void)
{
#ifdef CONFIG_SMP
    /* Alloc dedicated workqueue, that will be used to schedule smc on CPU #0.
     *
     * Note that we originally used "system" workqueue to schedule job on CPU #0
     * but that some drivers were using REE driver's kernel API from a workqueue
     * thus causing spurious warnings because of system queue not initialised
     * correctly: don't try to optimize here, and keep using this dedicated 
     * queue.
     */ 
    smc_wq = alloc_workqueue("smc_wq", WQ_MEM_RECLAIM, 10);
    if (!smc_wq)
        return ENOMEM;
#endif /* CONFIG_SMP */

    return 0;
}

void pnc_smc_exit(void)
{
#ifdef CONFIG_SMP
    /* Destroy dedicated work queue */
    if (smc_wq)
        destroy_workqueue(smc_wq);
#endif /* CONFIG_SMP */
}
