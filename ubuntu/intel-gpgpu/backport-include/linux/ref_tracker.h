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
#ifndef _BACKPORT_LINUX_REF_TRACKER_H
#define _BACKPORT_LINUX_REF_TRACKER_H
struct ref_tracker;

struct ref_tracker_dir {
};

static inline void ref_tracker_dir_init(struct ref_tracker_dir *dir,
					unsigned int quarantine_count,
					const char *name)
{
}

static inline void ref_tracker_dir_exit(struct ref_tracker_dir *dir)
{
}

static inline void __ref_tracker_dir_print(struct ref_tracker_dir *dir,
					   unsigned int display_limit)
{
}

static inline void ref_tracker_dir_print(struct ref_tracker_dir *dir,
					 unsigned int display_limit)
{
}

static inline int ref_tracker_dir_snprint(struct ref_tracker_dir *dir,
					  char *buf, size_t size)
{
	return 0;
}

static inline int ref_tracker_alloc(struct ref_tracker_dir *dir,
				    struct ref_tracker **trackerp,
				    gfp_t gfp)
{
	return 0;
}

static inline int ref_tracker_free(struct ref_tracker_dir *dir,
				   struct ref_tracker **trackerp)
{
	return 0;
}
#endif /* _BACKPORT_LINUX_REF_TRACKER_H */
