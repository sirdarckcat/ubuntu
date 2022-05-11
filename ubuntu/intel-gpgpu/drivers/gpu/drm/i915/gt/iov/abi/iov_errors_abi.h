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
 * Copyright © 2022 Intel Corporation
 */

#ifndef _ABI_IOV_ERRORS_ABI_H_
#define _ABI_IOV_ERRORS_ABI_H_

/**
 * DOC: IOV Error Codes
 *
 * IOV uses error codes that mostly match errno values.
 */

#define IOV_ERROR_UNDISCLOSED			0
#define IOV_ERROR_OPERATION_NOT_PERMITTED	1	/* EPERM */
#define IOV_ERROR_PERMISSION_DENIED		13	/* EACCES */
#define IOV_ERROR_INVALID_ARGUMENT		22	/* EINVAL */
#define IOV_ERROR_INVALID_REQUEST_CODE		56	/* EBADRQC */
#define IOV_ERROR_NO_DATA_AVAILABLE		61	/* ENODATA */
#define IOV_ERROR_PROTOCOL_ERROR		71	/* EPROTO */
#define IOV_ERROR_MESSAGE_SIZE			90	/* EMSGSIZE */

#endif /* _ABI_IOV_ERRORS_ABI_H_ */
