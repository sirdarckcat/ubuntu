/*
 * SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#ifndef TEGRA_HV_H
#define TEGRA_HV_H

/**
 * @defgroup hypervisor_ivc_framework Hypervisor IVC Framework
 * @{
 */

#include <soc/tegra/virt/syscalls.h>

/** @brief Supports trap notification to peer end via MSI irqs */
#define SUPPORTS_TRAP_MSI_NOTIFICATION

/** @brief Page size of IVC meta data queried from hypervisor */
#define IVC_INFO_PAGE_SIZE 65536

/** @brief Maximum Guest VM count */
#define MAX_NUM_GUESTS		16U
/** @brief The maximum number of IVC queues supported by the PCT. */
#define PCT_MAX_NUM_IVC_QUEUES	512U
/** @brief The maximum number of mempools supported by the PCT. */
#define PCT_MAX_NUM_MEMPOOLS	120U

/**
 * @brief          Query IVC meta data from hypervisor
 *
 * @retval         ptr To ivc_info_page structure having ivc meta data info
 *                 shared by hypervisor
 *
 * @pre
 *                 - Tegra hypervisor driver should have been initialized.
 *                 - This API should be invoked on virtual/hypervisor environment only.
 *
 * @post
 *                 - IVC meta data will be retuned to client
 *
 * @usage
 *                 - Allowed context for the API call
 *                   - Interrupt handler: Yes
 *                   - Signal handler: N/A
 *                   - Thread-safe: No
 *                   - Async/Sync: Sync
 *                   - Re-entrant: No
 *                 - API Group
 *                   - Init: No
 *                   - Runtime: Yes
 *                   - De-Init: No
 */
const struct ivc_info_page *tegra_hv_get_ivc_info(void);

/**
 * @brief          Query vmid of the GOS VM from hypervisor
 *
 * @retval         vmid On success else -ve value
 *
 * @pre
 *                 - Tegra hypervisor driver should have been initialized.
 *                 - This API should be invoked on virtual/hypervisor environment only.
 *
 * @post
 *                 - Guest VM ID will be returned.
 *
 * @usage
 *                 - Allowed context for the API call
 *                   - Interrupt handler: Yes
 *                   - Signal handler: N/A
 *                   - Thread-safe: No
 *                   - Async/Sync: Sync
 *                   - Re-entrant: No
 *                 - API Group
 *                   - Init: No
 *                   - Runtime: Yes
 *                   - De-Init: No
 */
int tegra_hv_get_vmid(void);

/** @} */

#endif /* TEGRA_HV_H */
