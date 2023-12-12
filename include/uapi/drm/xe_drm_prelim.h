/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _UAPI_XE_DRM_PRELIM_H_
#define _UAPI_XE_DRM_PRELIM_H_

#include "xe_drm.h"

/**
 * DOC: Xe PRELIM uAPI
 *
 * Reasoning:
 *
 * The DRM community imposes some strict requirements on the uAPI:
 *
 * - https://www.kernel.org/doc/html/latest/gpu/drm-uapi.html#open-source-userspace-requirements
 * - "The open-source userspace must not be a toy/test application, but the real thing."
 * - "The userspace patches must be against the canonical upstream, not some vendor fork."
 *
 * In some very specific cases, there will be a particular need to get a preliminary and
 * non-upstream uAPI merged in some of our branches. When this happens, the uAPI cannot
 * take a risk of conflicting IOCTL ranges with other preliminary uAPI or with a possible
 * real user space uAPI that could win the race towards upstream.
 *
 * 'PRELIM' uAPIs are APIs that are not yet merged on upstream. They were designed to be
 * in a different range in a way that the divergence with the upstream and other development
 * branches can be controlled and the conflicts minimized.
 *
 * It is also a mechanism that prevents user space regressions since the prelim modification
 * is always in a two-phase approach, where upstream and prelim can coexist for a period of
 * time while the UMDs adjust to the changes.
 *
 * Rules:
 *
 * - Communication will happen on a specific Teams channel: KMD uAPI Changes
 * - APIs to be considered Preliminary / WIP / Temporary to what will eventually be in upstream.
 *   It's designed to allow kernel and userspace to work together and make sure it works with all
 *   components before committing to support it forever in upstream (we don't want to break Linux's
 *   rule about not breaking userspace).
 * - IOCTL in separate file (this one).
 * - IOCTL, flags, enums, or any number that might conflict should be in separated range
 *   (i.e end of range like IOCTL numbers).
 * - PRELIM prefix mandatory in any define, struct, or non-static functions in PRELIM source files
 *   called by other source files (i.e xe_perf_ioctl -> prelim_xe_perf_ioctl)
 * - Code .c/.h using PRELIM should also be placed in prelim/ sub-directory for easier rebase.
 * - Two-Phase removal:
 *
 *   + When API needs to be modified in non-backwards compatible ways, the current PRELIM API
 *     cannot be removed (yet).
 *   + Either because it must change the behavior or because the final upstream one has landed.
 *   + If a new PRELIM is needed, it needs to be added as a new PRELIM_V<n+1> without removing the
 *     PRELIM_V<n>.
 *   + The previous one can only be removed after all user space components confirmed they are not
 *     using.
 *
 * Other prelim considerations:
 *
 * - Out of tree Sysfs and debugfs need to stay behind a 'prelim' directory.
 * - Out of tree module-parameters need to be identified by a PRELIM prefix.
 *   (xe.prelim_my_awesome_param=not_default)
 * - Internal/downstream declarations must be added here, not to xe_drm.h.
 * - The values in xe_drm_prelim.h must also be kept synchronized with values in xe_drm.h.
 * - PRELIM ioctl's: IOCTL numbers go down from 0x5f
 */

/*
 * IOCTL numbers listed below are reserved, they are taken up by other
 * components. Please add an unreserved ioctl number here to reserve that
 * number.
 */
#endif /* _UAPI_XE_DRM_PRELIM_H_ */
