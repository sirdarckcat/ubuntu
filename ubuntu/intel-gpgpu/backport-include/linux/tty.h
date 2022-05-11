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
#ifndef __BACKPORT_LINUX_TTY_H
#define __BACKPORT_LINUX_TTY_H
#include_next <linux/tty.h>

/*
 * This really belongs into uapi/asm-generic/termbits.h but
 * that doesn't usually get included directly.
 */
#ifndef EXTPROC
#define EXTPROC	0200000
#endif

#if LINUX_VERSION_IS_LESS(3,7,0)
/* Backports tty_lock: Localise the lock */
#define tty_lock(__tty) tty_lock()
#define tty_unlock(__tty) tty_unlock()

#define tty_port_register_device(port, driver, index, device) \
	tty_register_device(driver, index, device)
#endif

#if LINUX_VERSION_IS_LESS(3,10,0)
extern void tty_port_tty_wakeup(struct tty_port *port);
extern void tty_port_tty_hangup(struct tty_port *port, bool check_clocal);
#endif /* LINUX_VERSION_IS_LESS(3,10,0) */

#if LINUX_VERSION_IS_LESS(4,1,0) && \
    LINUX_VERSION_IS_GEQ(4,0,0)
extern int tty_set_termios(struct tty_struct *tty, struct ktermios *kt);
#endif /* LINUX_VERSION_IS_LESS(4,1,0) */

#ifndef N_NCI
#define N_NCI		25	/* NFC NCI UART */
#endif

#endif /* __BACKPORT_LINUX_TTY_H */
