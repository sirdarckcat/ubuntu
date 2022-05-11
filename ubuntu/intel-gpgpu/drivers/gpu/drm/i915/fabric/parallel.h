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
/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2020 - 2021 Intel Corporation.
 *
 */

#ifndef PARALLEL_H_INCLUDED
#define PARALLEL_H_INCLUDED

#include <linux/completion.h>
#include <linux/workqueue.h>

/**
 * struct par_group - Parallel execution context for a group of functions that
 * can be collectively waited on.
 * @outstanding: the number of outstanding functions
 * @done: signalled when all parallel work completes
 *
 * This schedules functions onto the system workqueue, and associates them with
 * a shared context.  An API call is provided to wait on this group of
 * functions for completion.
 */
struct par_group {
	atomic_t outstanding;
	struct completion done;
};

void par_start(struct par_group *ctx);
void par_wait(struct par_group *ctx);
int par_work_queue(struct par_group *ctx,
		   void (*fn)(void *), void *fn_ctx);

#endif /* PARALLEL_H_INCLUDED */
