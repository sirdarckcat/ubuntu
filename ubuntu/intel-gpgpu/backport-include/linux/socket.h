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
#ifndef __BACKPORT_SOCKET_H
#define __BACKPORT_SOCKET_H
#include_next <linux/socket.h>

#ifndef SOL_NFC
/*
 * backport SOL_NFC -- see commit:
 * NFC: llcp: Implement socket options
 */
#define SOL_NFC		280
#endif

#ifndef __sockaddr_check_size
#define __sockaddr_check_size(size)	\
	BUILD_BUG_ON(((size) > sizeof(struct __kernel_sockaddr_storage)))
#endif

#endif /* __BACKPORT_SOCKET_H */
