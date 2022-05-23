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
 * Copyright © 2019 Intel Corporation
 */

#ifndef INTEL_RPS_TYPES_H
#define INTEL_RPS_TYPES_H

#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/workqueue.h>

struct intel_ips {
	u64 last_count1;
	unsigned long last_time1;
	unsigned long chipset_power;
	u64 last_count2;
	u64 last_time2;
	unsigned long gfx_power;
	u8 corr;

	int c, m;
};

struct intel_rps_ei {
	ktime_t ktime;
	u32 render_c0;
	u32 media_c0;
};

enum {
	INTEL_RPS_ENABLED = 0,
	INTEL_RPS_ACTIVE,
	INTEL_RPS_INTERRUPTS,
	INTEL_RPS_TIMER,
};

/*
 * Freq caps exposed by HW, values are in "hw units" and intel_gpu_freq()
 * should be used to convert to MHz
 */
struct intel_rps_freq_caps {
	u8 rp0_freq;		/* non-overclocked max frequency */
	u8 rp1_freq;		/* "less than" RP0 power/freqency */
	u8 min_freq;		/* aka RPn, minimum frequency */
};

struct intel_rps {
	struct mutex lock; /* protects enabling and the worker */

	/*
	 * work, interrupts_enabled and pm_iir are protected by
	 * dev_priv->irq_lock
	 */
	struct timer_list timer;
	struct work_struct work;
	unsigned long flags;

	ktime_t pm_timestamp;
	u32 pm_interval;
	u32 pm_iir;

	/* PM interrupt bits that should never be masked */
	u32 pm_intrmsk_mbz;
	u32 pm_events;

	/* Frequencies are stored in potentially platform dependent multiples.
	 * In other words, *_freq needs to be multiplied by X to be interesting.
	 * Soft limits are those which are used for the dynamic reclocking done
	 * by the driver (raise frequencies under heavy loads, and lower for
	 * lighter loads). Hard limits are those imposed by the hardware.
	 *
	 * A distinction is made for overclocking, which is never enabled by
	 * default, and is considered to be above the hard limit if it's
	 * possible at all.
	 */
	u8 cur_freq;		/* Current frequency (cached, may not == HW) */
	u8 last_freq;		/* Last SWREQ frequency */
	u8 min_freq_softlimit;	/* Minimum frequency permitted by the driver */
	u8 max_freq_softlimit;	/* Max frequency permitted by the driver */
	u8 max_freq;		/* Maximum frequency, RP0 if not overclocking */
	u8 min_freq;		/* AKA RPn. Minimum frequency */
	u8 boost_freq;		/* Frequency to request when wait boosting */
	u8 idle_freq;		/* Frequency to request when we are idle */
	u8 efficient_freq;	/* AKA RPe. Pre-determined balanced frequency */
	u8 rp1_freq;		/* "less than" RP0 power/freqency */
	u8 rp0_freq;		/* Non-overclocked max frequency. */
	u16 gpll_ref_freq;	/* vlv/chv GPLL reference frequency */

	int last_adj;

	struct {
		struct mutex mutex;

		enum { LOW_POWER, BETWEEN, HIGH_POWER } mode;
		unsigned int interactive;

		u8 up_threshold; /* Current %busy required to uplock */
		u8 down_threshold; /* Current %busy required to downclock */
	} power;

	atomic_t num_waiters;
	unsigned int boosts;

	/* manual wa residency calculations */
	struct intel_rps_ei ei;
	struct intel_ips ips;
};

#endif /* INTEL_RPS_TYPES_H */
