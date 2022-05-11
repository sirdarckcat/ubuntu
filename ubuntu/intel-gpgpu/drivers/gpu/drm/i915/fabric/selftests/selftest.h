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
 * Copyright(c) 2019 - 2021 Intel Corporation.
 *
 */

#ifndef SELFTESTS_SELFTEST_H_INCLUDED
#define SELFTESTS_SELFTEST_H_INCLUDED

/**
 * selftests_run - Execute selftests.
 *
 * Return: 0 if the driver should continue; non-zero otherwise.
 */
int selftests_run(void);

/*
 * 'make checkpatch' will complain about the return statment in the following
 * macro:
 */
#define FAIL(msg, ...) \
	do { \
		pr_err("TEST FAILED: %s: assert: " msg "\n", __func__, ##__VA_ARGS__); \
		return -EINVAL; \
	} while (0)

#define TEST(cond, msg, ...) \
	do { \
		if (!(cond)) { \
			FAIL(msg, ##__VA_ARGS__); \
		} \
	} while (0)

#endif /* SELFTESTS_SELFTEST_H_INCLUDED */
