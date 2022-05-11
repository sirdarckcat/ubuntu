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
// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2019 - 2021 Intel Corporation.
 *
 */

#include <linux/errno.h>
#include <linux/module.h>

#include "selftest.h"
#include "routing_selftest.h"

enum selftest_mode {
	SELFTEST_MODE_DISABLED = 0,
	SELFTEST_MODE_RUN_CONTINUE = 1,
	SELFTEST_MODE_RUN_EXIT = -1,
};

static enum selftest_mode selftest_mode;

typedef int (*selftest_fn)(void);

struct selftest_entry {
	char *name;
	selftest_fn fn;
};

#define SELFTEST_ENTRY(name) { #name, name }
#define SELFTEST_END { NULL, NULL }

static struct selftest_entry selftests[] = {
	SELFTEST_ENTRY(routing_selftest),
	SELFTEST_END,
};

static int execute(void)
{
	struct selftest_entry *entry = selftests;
	int err = 0;

	for (; entry->fn; entry++) {
		err = entry->fn();
		if (err) {
			pr_err("SELFTEST: %s: FAIL: %d\n", entry->name, err);
			goto exit;
		} else {
			pr_info("SELFTEST: %s: SUCCESS\n", entry->name);
		}
	}

exit:
	pr_info("selftests complete\n");
	return err;
}

int selftests_run(void)
{
	switch (selftest_mode) {
	case SELFTEST_MODE_DISABLED:
		return 0;
	case SELFTEST_MODE_RUN_CONTINUE:
		return execute();
	case SELFTEST_MODE_RUN_EXIT:
		execute();
		return -1;
	}

	pr_err("%s: invalid selftest mode: %d\n", __func__, selftest_mode);
	return -1;
}

module_param_named(selftests, selftest_mode, int, 0400);
MODULE_PARM_DESC(selftests, "Run selftests on driver load (0:disabled [default], 1:run tests then continue, -1:run tests then exit module)");
