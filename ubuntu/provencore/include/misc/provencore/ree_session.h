// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, ProvenRun S.A.S
 */
/**
 * @file session.h
 * @brief Public Provencore's kernel API
 *
 * @author Henri Chataing
 * @date November 26th, 2019 (creation)
 * @copyright (c) 2019-2021, Prove & Run S.A.S and/or its affiliates.
 *   All rights reserved.
 */

#ifndef _PROVENCORE_SESSION_H_INCLUDED_
#define _PROVENCORE_SESSION_H_INCLUDED_

#include <linux/poll.h>
#include <linux/types.h>

#define NO_TIMEOUT          0

struct pnc_session;
typedef struct pnc_session pnc_session_t;

/**
 * @brief Open a new session for communicating with a provencore application.
 *
 * Will wait until secure world is ready to initiate session. If secure world
 * never syncs with normal world, this call blocks indefinitely.
 * On the other hand, it is a good function to use to sync with secure world if
 * it is sure secure world will become ready.
 *
 * @param session       Updated with the pointer to the allocated session handle
 * @return 0 if new session opened, -ENOMEM if no room for new session
 */
int pnc_session_open(pnc_session_t **session);

/**
 * @brief Try to open a new session for communicating with a provencore
 *        application.
 *
 * Also forward some \p flags as a bitfield of properties required during open
 * operation.
 * List of supported flags:
 *  - O_NONBLOCK: don't wait for secure world readyness
 *
 * @param session       Updated with the pointer to the allocated session handle
 * @return 0 if new session opened, -ENOMEM if no room for new session, -EAGAIN
 *         if secure world not ready.
 */
int pnc_session_open_with_flags(pnc_session_t **session, unsigned int flags);

/**
 * @brief Close the selected session.
 * @param session       Pointer to the session handle
 */
void pnc_session_close(pnc_session_t *session);

/**
 * @brief Get version for the REE solution used between S and NS world.
 *
 * Some functionalities, some API may or may not be available for given versions.
 * This function shall help developpers to know what is possible and what is not.
 *
 * Call to this function is only possible after a successfull call to
 * \ref pnc_session_open because its a way to confirm S and NS are synced on
 * REE version.
 *
 * @brief session       Pointer to the session handle
 * @param version       Buffer filled with value of running REE version
 * @return              - EBADF if \p session or \p version is NULL
 *                      - 0 and \p version filled with REE version.
 */
int pnc_session_get_version(pnc_session_t *session, uint32_t *version);

/**
 * @brief Allocate shared memory for the selected session.
 *
 * The memory cannot be freed or reallocated without closing the session.
 *
 * @param session       Pointer to the session handle
 * @param size          Requested size in bytes
 * @return              - -EEXIST if the \ref pnc_session_alloc was already
 *                          called for \p session
 *                      - -EINVAL if \p size is 0
 *                      - -ENOMEM if \p size bytes could not be allocated
 *                      - -ERESTARTSYS if system signal while trying to down
 *                        shm allocator's semaphore.
 *                      - 0 on success
 */
int pnc_session_alloc(pnc_session_t *session, unsigned long size);

/**
 * @brief Configure the selected session with the identifier of the provencore
 *  application.
 *
 * @param session       Pointer to the session handle
 * @param name          Name of the ProvenCore service or process to connect to
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -EBADF if session not S_NULL
 *                      - -ERESTARTSYS if system error
 *                      - -ENODEV is configuration failed
 *                      - -EOVERFLOW if service name is too long (unlikely)
 *                      - -ENOTSUPP if REE version is not supporting the feature
 *                      - 0 on success
 *                      - strictly positive value to report S error...
 *
 * Notes: - Available in kernel API only since REEV3.03.
 *        - process name must follow the DTS node name convention:
 *            at most 31 bytes long
 *            characters in 0-9, A-Z, a-z, ',', '.', '_', '+', '-'
 *            must start with a lower or uppercase letter
 */
int pnc_session_config_by_name(pnc_session_t *session, const char *name);

/**
 * @brief Configure the selected session with the service identifier
 *
 * @param session       Pointer to the session handle
 * @param id            Service identifier
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -EBADF if session not S_NULL
 *                      - -ERESTARTSYS if system error
 *                      - -ENODEV is configuration failed
 *                      - 0 on success
 *                      - strictly positive value to report S error...
 */
int pnc_session_config(pnc_session_t *session, uint64_t sid);

/**
 * @brief Retrieve SHM information for the selected session.
 *
 * @param session       Pointer to the session handle
 * @param ptr           Updated with the virtual address of the shared buffer
 * @param size          Updated with the number of pages of the shared buffer
 * @return              - -EINVAL if \p session is NULL
 *                      - -ENOMEM if the session does not have allocated memory.
 *                      - -ENODEV if whole SHM not allocated.
 *                      - 0 on success
 */
int pnc_session_get_mem(pnc_session_t *session, char **ptr, unsigned long *size);

/**
 * @brief Send response to a previous request.
 *
 * @param session       Pointer to the session handle
 * @param response      Response to send
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ERESTARTSYS if system error
 *                      - -ENODEV if session not configured
 *                      - -EPROTO if server not ready to send response
 *                      - 0 on success
 */
int pnc_session_send_response(pnc_session_t *session, uint32_t response);

/**
 * @brief Fetch available response for a given session
 *
 * Protocol forbids client to send new request until A_RESPONSE received (unless
 * request is a `special request` such as A_CANCEL or A_TERM).
 * As a result, the latest pending `normal response` is always available with a
 * call to this function. Subsequent call to this function, without sending new
 * request in between, will return -EAGAIN.
 *
 * @param session       Pointer to the session handle
 * @param response      Buffer updated with available response
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -EBADF if response buffer is NULL
 *                      - -ERESTARTSYS if system error
 *                      - -ENODEV if session not configured
 *                      - -EAGAIN if no response available
 *                      - 0 on success
 */
int pnc_session_get_response(pnc_session_t *session, uint32_t *response);

/**
 * @brief Wait for response reception
 *
 * @param session       Pointer to the session handle
 * @param response      Buffer updated with available response
 * @param timeout       Timeout in milliseconds to wait for application's
 *                      response.
 *                      Using \p timeout = \ref NO_TIMEOUT (0) sets
 *                      an infinite timeout.
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ENODEV if session not configured
 *                      - -ERESTARTSYS if system error
 *                      - -ETIMEDOUT if no response in time
 *                      - -EPIPE if session terminated while waiting
 *                      - -EBADF if response buffer is NULL
 *                      - 0 on success
 */
int pnc_session_wait_response(pnc_session_t *session, uint32_t *response,
        uint32_t timeout);

/**
 * @brief Send a request through the selected session.
 *
 * Don't wait for response.
 * API user can then do anything else and then:
 * - later call @pnc_session_get_response to check if response available
 * - later call @pnc_session_wait_response to force busy wait for response
 *
 * @param session       Pointer to the session handle
 * @param request       Request to send
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ERESTARTSYS if system error
 *                      - -ENODEV if session not configured
 *                      - -EPROTO if client not ready to send request
 *                      - 0 on success
 */
int pnc_session_send_request(pnc_session_t *session, uint32_t request);

/**
 * @brief Fetch available request for a given session
 *
 * Protocol forbids client to send new request until A_RESPONSE received (unless
 * request is a `special request` such as A_CANCEL or A_TERM).
 * As a result, the latest pending `normal request` is always available with a
 * call to this function. Subsequent calls to this function, without having sent
 * A_RESPONSE in between, will return -EAGAIN.
 *
 * @param session       Pointer to the session handle
 * @param request       Buffer filled with available request
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -EBADF if request is NULL
 *                      - -ERESTARTSYS if system error
 *                      - -ENODEV if session not configured
 *                      - -EAGAIN if no request available
 *                      - 0 on success
 */
int pnc_session_get_request(pnc_session_t *session, uint32_t *request);

/**
 * @brief Wait for request reception
 *
 * @param session       Pointer to the session handle
 * @param request       Buffer filled with available request
 * @param timeout       Timeout in milliseconds to wait for application's
 *                      request.
 *                      Using \p timeout = \ref NO_TIMEOUT (0) sets
 *                      an infinite timeout.
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ERESTARTSYS if system error
 *                      - -ENODEV if session not configured
 *                      - -EPROTO if server not ready to wait for new request
 *                      - -ETIMEDOUT if no request in time
 *                      - -EPIPE if session terminated while waiting
 *                      - -EBADF if \p request is NULL
 *                      - 0 on success
 */
int pnc_session_wait_request(pnc_session_t *session, uint32_t *request,
        uint32_t timeout);

#define REQUEST_CANCEL_OK       UINT32_C(0xABE00001)
#define REQUEST_CANCEL_RESPONSE UINT32_C(0xABE00002)

/**
 * @brief Request previous request cancellation and wait for acknowledge
 *
 * Regarding timings, acknowledge can also be a response to previous request.
 *
 * Out of usual negative error, this function may return:
 *  - REQUEST_CANCEL_OK: request cancelled and not handled by server application
 *  - REQUEST_CANCEL_RESPONSE: response to previous request fetched in
 * \p response buffer.
 *
 * @param session       Pointer to the session handle
 * @param response      Buffer filled with response
 * @param timeout       Timeout in milliseconds to wait for application's
 *                      acknowledge..
 *                      Using \p timeout = \ref NO_TIMEOUT (0) sets
 *                      an infinite timeout.
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ERESTARTSYS if system error
 *                      - -ENODEV if session not configured
 *                      - -EPROTO if client not ready to cancel request
 *                      - -ETIMEDOUT if no acknowledge in time
 *                      - -EPIPE if session terminated while waiting
 *                      - REQUEST_CANCEL_xxx status on success (see above)
 */
int pnc_session_cancel_request(pnc_session_t *session, uint32_t *response,
     uint32_t timeout);

/**
 * @brief Send a request through the selected session and wait for response.
 *
 * Composite function for the handling of consecutive calls to:
 *  - pnc_session_send_request
 *  - pnc_session_wait_response
 *
 * @param session       Pointer to the session handle
 * @param request       Request to send
 * @param timeout       Timeout in milliseconds to wait for application's
 *                      response.
 *                      Using \p timeout = \ref NO_TIMEOUT (0) sets
 *                      an infinite timeout.
 * @param response      Updated with the request's response in case of success
 *                      and return value is 0
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ERESTARTSYS if system error
 *                      - -EBADF if response buffer is NULL
 *                      - -ENODEV if session not configured
 *                      - -EPROTO if client not ready to send request
 *                      - -ETIMEDOUT if response not received in time
 *                      - -EPIPE if session terminated while waiting
 *                      - -EAGAIN if response not yet available (should never
 *                      occur)
 *                      - 0 on success
 *
 */
int pnc_session_send_request_and_wait_response(pnc_session_t *session,
        uint32_t request, uint32_t timeout, uint32_t *response);

/**
 * @brief Set signal pending and notify S
 *
 * A signal is a bit pending in a 32-bit signal register. This function allow to
 * set bits pending in session's signal register and to notify S if signal not
 * already pending.
 *
 * @param session       Pointer to the session handle
 * @param bits          Bitfield of all bits that must be set pending in
 *                      session's signal register.
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ENODEV if session not configured
 *                      - 0 on success
 */
int pnc_session_send_signal(pnc_session_t *session, uint32_t bits);

/**
 * @brief Fetch any pending signal for a given session
 *
 * When receiving E_SIGNAL(s), NS driver only acknowledges E_SIGNAL notification
 * but signals are kept pending in S to NS session's signal register.
 * It is call to this function responsible for any pending signal acknowledge:
 * no new E_SIGNAL notification will be sent for an already pending signal as
 * long as this function is not called.
 * Subsequent calls to this function can always lead to new bits in \p value.
 *
 * @param session       Pointer to the session handle
 * @param signals       Buffer updated with session's signal register content
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ENODEV if session not configured
 *                      - -EBADF if \p value is NULL
 *                      - 0 on success
 */
int pnc_session_get_signal(pnc_session_t *session, uint32_t *signals);

/**
 * @brief Wait for new S signal
 *
 * Returns as soon as any S to NS signal is pending.
 * This function removes any pending signal from session's signal register thus
 * allowing S to notify any new signal if any.
 *
 * @param session       Pointer to the session handle
 * @param signals       Buffer updated with session's signal register content
 * @param timeout       Timeout in milliseconds to wait for application's
 *                      signal.
 *                      Using \p timeout = \ref NO_TIMEOUT (0) sets
 *                      an infinite timeout.
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ENODEV if session not configured
 *                      - -ERESTARTSYS if system error
 *                      - -ETIMEDOUT if signal not received in time
 *                      - -EPIPE if session terminated while waiting
 *                      - -EBADF if \p signals is NULL
 *                      - 0 on success
 */
int pnc_session_wait_signal(pnc_session_t *session, uint32_t *signals,
    uint32_t timeout);

/**
 * Bits that can be used to build mask when calling \ref pnc_session_wait_event
 */
#define EVENT_PENDING_SIGNAL    (UINT32_C(1) << 0)
#define EVENT_PENDING_REQUEST   (UINT32_C(1) << 1)
#define EVENT_PENDING_RESPONSE  (UINT32_C(1) << 2)
#define EVENT_PENDING_ALL       (EVENT_PENDING_SIGNAL  | \
                                 EVENT_PENDING_REQUEST | \
                                 EVENT_PENDING_RESPONSE)

/**
 * @brief Wait for any S event
 *
 * A S event can be a client request, a server response and/or a signal.
 *
 * Available bits in mask to wait event(s) are defined above:
 *  - EVENT_PENDING_SIGNAL: function returns if S signal received
 *  - EVENT_PENDING_REQUEST: function returns if S request received
 *  - EVENT_PENDING_RESPONSE: function returns if S response received
 *  - EVENT_PENDING_ALL: function returns if any of S event received
 *
 * When returning successfully, \p events bits are set to indicate which kind of
 * event was received and the corresponding \ref pnc_session_get_response,
 * \ref pnc_session_get_request and \ref pnc_session_get_signal functions can be
 * used to get each event details.
 *
 * @param session       Pointer to the session handle
 * @param events        Buffer filled with available event(s) bitfield.
 * @param mask          Bitmask to filter required events to wait for
 * @param timeout       Timeout in milliseconds to wait for application's
 *                      signal.
 *                      Using \p timeout = \ref NO_TIMEOUT (0) sets
 *                      an infinite timeout.
 * @return              - -ENOENT if SHM is not ready
 *                      - -EINVAL if invalid session handle
 *                      - -ENODEV if session not configured
 *                      - -ETIMEDOUT if signal not received in time
 *                      - -EPIPE if session terminated while waiting
 *                      - -EBADF if \p events is NULL
 *                      - 0 on success
 */
int pnc_session_wait_event(pnc_session_t *session, uint32_t *events,
        uint32_t mask, uint32_t timeout);

/**
 * @brief Get a bitmask representing the available pending events
 *
 * A S event can be a client request, a server response and/or a signal.
 *
 * Bits returned in \p events are defined above:
 *  - EVENT_PENDING_SIGNAL if S signal received
 *  - EVENT_PENDING_REQUEST if S request received
 *  - EVENT_PENDING_RESPONSE if S response received
 *
 * When returning successfully, \p events bits are set to indicate which kind of
 * event was received and the corresponding \ref pnc_session_get_response,
 * \ref pnc_session_get_request and \ref pnc_session_get_signal functions can be
 * used to get each event details.
 *
 * @param session       Pointer to the session handle
 * @param events        Buffer filled with available event(s) bitfield.
 * @return              - -ENOENT if SHM is not ready
 *                      - -ERESTARTSYS if interrupted while taking the semaphore
 *                      - -EAGAIN if the sessions framework was not yet enabled
 *                      - -EINVAL if invalid session handle
 *                      - -ENODEV if session not configured
 *                      - -EBADF if \p events is NULL
 *                      - 0 on success
 */
int pnc_session_get_pending_events(pnc_session_t *session, uint32_t *events);

/**
 * @brief Wait until either an event is received or another work in the poll
 *        table is unblocked
 *
 * @param session   session handle
 * @param file      pointer to the file structure used when calling poll_wait
 * @param wait      poll_table pointer used when calling poll_wait
 *
 * @return
 *   - EPOLLIN | EPOLLRDNORM    if unblocked by a received event
 *   - 0                        otherwise
 */
__poll_t pnc_session_poll_wait(pnc_session_t *session, struct file *file,
                               poll_table *wait);

/**
 * !!!!!!!!!!!!!!!!!!!!!!!! DEPRECATED: DON'T USE !!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 * Kept here for backward compatibility waiting for all applications to
 * switch to latest API.
 *
 * Replaced by @pnc_session_send_request_and_wait_response
 *
 *
 * @brief Send a request through the selected session.
 * The function sends a notification and wait for the provencore application
 * response.
 * @param session       Pointer to the session handle
 * @param type          Request type
 * @param flags         Request flags
 * @param timeout       Request timeout in milliseconds.
 *                      Using \p timeout = \ref NO_TIMEOUT (0) sets
 *                      an infinite timeout.
 * @param status        Updated with the request status in case of success
 *                      (return value is 0)
 * @return              - 0 on success
 *                      - an error code otherwise
 */
int pnc_session_request(pnc_session_t *session, uint32_t type,
                        uint16_t flags, uint32_t timeout,
                        uint32_t *status);

#endif /* _PROVENCORE_SESSION_H_INCLUDED_ */
