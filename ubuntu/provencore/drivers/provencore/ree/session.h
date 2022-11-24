// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, ProvenRun S.A.S
 */
/**
 * @file session.h
 * @brief   Private session API description (public session API described in
 *          misc/provencore/session.h)
 * @author Henri Chataing
 * @date October 07th, 2019 (creation)
 * @copyright (c) 2019-2021, Prove & Run S.A.S and/or its affiliates.
 *   All rights reserved.
 */

#ifndef _SESSION_H_INCLUDED_
#define _SESSION_H_INCLUDED_

#include <linux/irq.h>
#include <linux/poll.h>
#include "misc/provencore/ree_session.h"

/**
 * @brief Handle interrupt (secure SGI) raised by S world to notify new event(s)
 *
 * As soon as SHM get initialized in NS and S worlds, this function will be
 * responsible for scheduling work to check S notification register containing
 * new S pending events.
 *
 * @param irq           Interrupt number
 * @param dev_id        Device identifier (ignored)
 * @return              IRQ_HANDLED
 */
irqreturn_t pnc_session_interrupt_handler(int irq, void *dev_id);

/**
 * @brief Init sessions framework.
 *
 * Initialize SHM header, static sessions table so as NS-->S and S-->NS ring
 * buffers
 *
 * @return      - 0 if success
 *              - -ENOMEM if SHM not initialised
 */
int pnc_sessions_init(void);

/**
 * @brief Release all sessions work.
 *
 * Ensure no work is on going for any session. Supposed to be called whereas
 * S-->NS irq is disabled.
 */
void pnc_sessions_exit(void);

/**
 * @brief Retreive SHM geometry for a given session
 *
 * A part of whole S<-->NS shared memory can be allocated for a given session.
 * This function allows session user to get its geometry.
 *
 * @param session       session handle
 * @param offset        updated with offset of session's SHM area if any
 * @param nr_pages      updated with num of pages for session's SHM area if any
 * @return 0 if memory allocated in SHM for this session, negative error
 *          otherwise:
 *             - -EINVAL: invalid session
 *             - -ENOMEM: no memory allocated
 */
int pnc_session_get_mem_offset(pnc_session_t *session, unsigned long *offset,
        unsigned long *nr_pages);

/**
 * @brief Wait for end of synchro with S at start up to display REE version.
 * 
 * This output in logs, for debug purpose, to be a way to check for full REE 
 * readiness easily at start up.
 */
void pnc_sessions_sync(struct work_struct *work);

/**
 * @brief Release any thread blocked waiting for secure world readyness
 */
void pnc_sessions_release(void);

/**
 * @brief Register the device in linux.
 *
 * Create the /dev/trustzone node
 *
 * @return      - 0 if success
 *              - error code if misc_register() fails
 */
int register_device(void);

#endif /* _SESSION_H_INCLUDED_ */
