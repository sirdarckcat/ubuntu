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
#ifndef __BACKPORT_TTY_FLIP_H
#define __BACKPORT_TTY_FLIP_H
#include_next <linux/tty_flip.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,9,0)
#define tty_flip_buffer_push(port) tty_flip_buffer_push((port)->tty)
#define tty_insert_flip_string(port, chars, size) tty_insert_flip_string((port)->tty, chars, size)
#endif

#endif /* __BACKPORT_TTY_FLIP_H */
