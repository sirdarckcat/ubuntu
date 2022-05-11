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
#ifndef __BACKPORT_PTP_CLOCK_KERNEL_H
#define __BACKPORT_PTP_CLOCK_KERNEL_H

#include <linux/version.h>
#include_next <linux/ptp_clock_kernel.h>

#if LINUX_VERSION_IS_LESS(3,5,0)
#include <linux/posix-clock.h>

#define PTP_MAX_TIMESTAMPS 128
#define PTP_BUF_TIMESTAMPS 30

struct timestamp_event_queue {
	struct ptp_extts_event buf[PTP_MAX_TIMESTAMPS];
	int head;
	int tail;
	spinlock_t lock;
};

struct ptp_clock {
	struct posix_clock clock;
	struct device *dev;
	struct ptp_clock_info *info;
	dev_t devid;
	int index; /* index into clocks.map */
	struct pps_device *pps_source;
	struct timestamp_event_queue tsevq; /* simple fifo for time stamps */
	struct mutex tsevq_mux; /* one process at a time reading the fifo */
	wait_queue_head_t tsev_wq;
	int defunct; /* tells readers to go away when clock is being removed */
};

extern int ptp_clock_index(struct ptp_clock *ptp);
#endif /* LINUX_VERSION_IS_LESS(3,5,0) */

#if LINUX_VERSION_IS_LESS(3,7,0) && !defined(CONFIG_SUSE_KERNEL)
#define ptp_clock_register(info,parent) ptp_clock_register(info)
#endif /* LINUX_VERSION_IS_LESS(3,7,0) */

#endif /* __BACKPORT_PTP_CLOCK_KERNEL_H */
