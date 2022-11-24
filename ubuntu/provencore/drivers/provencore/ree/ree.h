// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, ProvenRun S.A.S
 */
/**
 * @file ree.h
 * @brief PNC's REE solution description
 *
 * This file is shared between REE service application and Linux Trustzone
 * driver.
 *
 * @author Alexandre Berdery
 * @date october 22nd, 2020 (creation)
 * @copyright (c) 2020-2021, Prove & Run and/or its affiliates.
 *   All rights reserved.
 */

#ifndef REE_H_INCLUDED
#define REE_H_INCLUDED

#include "misc/provencore/pnr_ring.h"

#ifndef BIT
#define BIT(x)  (UINT32_C(0x01) << x)
#endif

/**
 * PNC's REE solution uses shared memory to build data communication channels
 * between Non Secure (NS) and Secure (S) applications.
 *
 * Application stands for:
 *  - PNC service in S world
 *  - Linux kernel driver or user space application in NS world
 *
 * A channel, configured to allow communication between 2 applications running
 * each in NS and S world is called a session.
 *
 * With current REE solution, it is only NS application responsible for
 * initiating session configuration. Once configured, either NS or S application
 * can be responsible for session termination.
 *
 * After SHM initialization, REE solution supports 2 styles of communication, in
 * each direction (NS-->S or S-->NS).
 * For direction A-->B, those 2 styles are:
 *  - synchronous: A sends a message (aka the request) to which B must reply
 * with another message (aka the response). After A has sent a request to B it
 * is not allowed to send another until it has received the response except if
 * request is a cancellation request (A notifies B it will not need the response)
 * or a termination request (A notifies B it is terminating session).
 *  - asynchronous: A notifies B that a certain event has occured by setting a
 * certain signal pending. After a signal has become pending, it stays so,
 * until B acknowledges it: signals have no response payload but only
 * counterpart acknowledge.
 *
 */

/**
 * @brief SHM header constants
 *
 * @REE_MAGIC_1: indicates 1st step of SHM init (set by NS).
 * @REE_MAGIC_2: indicates last step of SHM init (set by S).
 * @REE_RESERVED_PAGES: num of pages reserved in SHM for storage of SHM layout
 * used for communications handling.
 * @REE_MAX_SESSIONS: maximum number of sessions that can be used at the same
 * time. MUST be even value less or equal to 28. The length of 28 is a
 * limitation due to the length of the notification register, described below,
 * used to forward S-->NS and NS-->S notifications.
 */
#define REE_MAGIC_1         UINT32_C(0xdeadcafe)
#define REE_MAGIC_2         UINT32_C(0xfee1ca4e)
#define REE_RESERVED_PAGES  3
#define REE_MAX_SESSIONS    28

/*
 * Also part of SHM header used at startup to sync with S world.
 *
 * REE scheme version used for SHM handling. Starting REEV3, the version number 
 * is a bitfield allowing definition of major and minor numbers:
 *     - bits[0;7]: minor number
 *     - bits[8;31]: major number
 *
 * CHANGELOG:
 *  - 3.00: 
 *      original REE V3 
 *
 *  - 3.01: 
 *      add poll support 
 *          Linux user can now use poll function to wait for new events on an 
 *          opened session in the same time it waits for events on other file
 *          descriptors. For that purpose, 2 new functions were added to the
 *          public userland API: \ref pnc_session_get_fd and 
 *          pnc_session_get_pending_events
 *      change pnc_session_config kernel API
 *          Removal of useless is_sid argument. This is a break of compatibility
 *          with 3.00 and kernel drivers using kernel API have been updated in 
 *          the same time to use latest one.
 *
 * - 3.02:
 *     add support for direct configuration to a service by its name (string)
 *          Registration is not mandatory when using a service name rather than its id.
 *          This is done by using a special id (GETSYSPROC_PID_SID),
 *          and filling the shared memory with the process name.
 *     No compatibility break known.
 *
 * - 3.03:
 *     add support for direct configuration to a service by its name (string) to 
 *     the kernel API.
 *          See 3.02 changelog for feature details
 *
 * - 3.04:
 *     register /dev/trustzone only when ProvenCore is in sync with Linux
 *     rework internal session status
 *
 * - 3.05:
 *     fix handling of session config failure in secure world.
 */
#define REE_VERSION         UINT32_C(0x305) /* 3.05 */

/**
 * @brief List of NS <--> S notifications.
 *
 * Notifications are stored as part of 2 32-bit notification registers stored
 * in reserved SHM. One register is used to store NS-->S notifications, one
 * register is used to store S-->NS ones.
 *
 * Regarding notifications from A to B, a notification register supports 3
 * different kind of events:
 * - E_RESET: some unrecoverable change in A SHM configuration has occurred and
 * A needed to warn about coming shut down and/or reset. It is then expected B
 * to initiate shut down and reset in order to replay startup SHM
 * initialization/synchronization.
 * - E_MESSAGE: some message(s) is(are) ready to be consumed in A-->B ring
 * buffer.
 * - E_SIGNAL(s): for specific s session, some A-->B signal(s) is(are) pending
 * in corresponding session's signal register.
 *
 * 32-bit notification register bitmap:
 *
 *  31   30   29   28 --------------------------------->  0
 * --------------------------------------------------------
 * | C  | M  | R1 | R2 | S27 | ----------------------- | S0 |
 * --------------------------------------------------------
 *
 * - Bit 31: for E_RESET notification
 * - Bit 30: for E_MESSAGE notification
 * - Bit 29 and Bit 28: reserved
 * - Bit 27 to Bit 0: for E_SIGNAL notification for any of the available 28
 * sessions
 *
 * Atomic read/modify/write is used to set bit pending in notification register.
 * A (respectively B) notifies B (respectively A) about new notification only if
 * it wasn't already pending.
 * Upon notification (SGI) reception, B (respectively A) perform atomic
 * read/clear/write as notification acknowledgement.
 */
#define E_RESET     BIT(31)
#define E_MESSAGE    BIT(30)
#define E_SIGNAL(s)  BIT(s)

/* Mask to extract sessions signal notifications from notification register */
#define SESSIONS_SIGNAL_MASK    UINT32_C(0x0FFFFFFF)

/** Description for notification register: 32-bit register  */
typedef uint32_t pnc_notification_t;

/** Description for session's signal register
 *
 * For each session, 2 session's signal registers are stored in SHM for
 * handling of S-->NS and NS-->S signals.
 * A signal register is a 32-bit interger offering possibility for 32 different
 * signals, for a given session, between 2 communicating applications.
 *
 * Atomic read/modify/write is used to set bit pending in signal register.
 * A (respectively B) notifies B (respectively A) about new signal only if it
 * wasn't already pending.
 * Upon notification (SGI) reception, B (respectively A) perform atomic
 * read/clear/write as notification acknowledgement.
 *
 */
typedef uint32_t pnc_signal_t;

/**
 * @brief SHM header structure
 *
 * SHM header is used at start up to synchronize both worlds around SHM
 * initialization:
 *  - NS world allocates SHM, initializes its part, fully fills header,
 * including magic with REE_MAGIC_1, and sends SGI to S.
 *  - S, upon SGI reception, checks for SHM geometry and valid header content.
 * Then it initializes its part, replaces header's magic with REE_MAGIC_2 and
 * sends SGI to NS.
 *
 * SHM header can be used at runtime to check SHM coherency.
 */
typedef struct pnc_header
{
    /** For SHM coherency: REE_MAGIC_1 or REE_MAGIC_2 */
    uint32_t magic;

    /** Should be \ref REE_VERSION once SHM initialised */
    uint32_t version;

    /** Should be \ref REE_RESERVED_PAGES once SHM initialised */
    uint16_t reserved_pages;

    /** Should be \ref REE_MAX_SESSIONS once SHM initialised */
    uint16_t max_sessions;

    /** Reserved for future use and keep structure size on 64-bit boundary */
    uint32_t rfu;
} pnc_header_t;

/**
 * @brief Description of synchronous PNC message
 *
 * Message is used to transmit one of the below \ref session_action
 *
 */
typedef struct pnc_message
{
    /** 64-bit message parameter */
    uint64_t p0;

    /** 32-bit message parameter */
    uint32_t p1;

    /** 16-bit message parameter */
    uint16_t p2;

    /** session identifier */
    uint8_t index;

    /** action requested with this message (see \ref session_action) */
    uint8_t action;
} pnc_message_t;

/**
 * @brief List session message actions.
 */
typedef enum session_action
{
    /**< Normal request. Message payload contains infos */
    A_REQUEST,
    /**< Response to a normal request. Message payload contains response */
    A_RESPONSE,
    /**< Special request for session config. Message payload contains session infos */
    A_CONFIG,
    /**< A_CONFIG acknowledge. Message payload contains configuration status */
    A_CONFIG_ACK,
    /**< Special request for request cancellation. No info in message payload */
    A_CANCEL,
    /**< A_CANCEL acknowledge. No info in message payload */
    A_CANCEL_ACK,
    /**< Special request for session termination. No info in message payload */
    A_TERM,
    /**< A_TERM acknowledge. No info in message payload */
    A_TERM_ACK
} session_action_t;

/**
 * @brief List session states.
 *
 * This list contains states for global session and states used to define state
 * of client or server for a given session. Client stands for the one sending
 * requests and server is the one processing requests and sending response.
 */
typedef enum session_state
{
    /**< Session (or client or server...) is invalid. */
    S_NULL,

    /**< [global] In NS world, session configured but not yet acknowledged */
    S_CONFIG_WAITING,

    /**< [global] Session i scongifured, ready for any client or server
     * operation*/
    S_CONFIGURED,

    /**< [global] A_TERM sent, waiting for A_TERM_ACK: session is on hold. */
    S_TERM_WAITING,

    /**< [client/server] Ready to send or receive request */
    S_IDLE,
    /**< [server] A_REQUEST received, corresponding application notified.
     *   [client] A_RESPONSE received, corresponding application notified */
    S_NOTIFIED,

    /**< [server] Application is handling fetched request. */
    S_BUSY,

    /**< [client] New A_REQUEST sent. Waiting for A_RESPONSE. */
    S_WAITING,

    /**< [client] A_CANCEL sent. Waiting for A_CANCEL_ACK or A_RESPONSE. */
    S_CANCEL_WAITING,

    /**< [server] A_TERM received, waiting to confirm process received the info
     * S world only because of notify/send mechanism. 
     * In that state, we stop accepting new process messages */
    S_TERMINATING, 
} session_state_t;

/**
 * @brief Max num of simultaneous messages, for one direction (NS-->S or
 *        S-->NS), for one session
 *
 * This number shall be used to check length of SHM reserved for the 2 ring
 * buffers allocation is enough to handle worst case, thus avoiding handling of
 * full ring buffer.
 *
 * With current REE protocol, for one session, the maximum num of A-->B message
 * slots that may be used at the same time in A-->B ring buffer is the sum of
 * following slots:
 *  - 1 `normal request`. No more than one `normal request` can be sent at the
 * same time (e.g until A_REQUEST consumed in A-->B ring buffer and
 * A_RESPONSE received)
 *  - 2 `special request`. Protocol doesn't prevent sending `special request`
 * while a `normal request` is still on going and it doesn't either prevent
 * A_CANCEL and A_TERM to be sent consecutively.
 *  - 1 `normal response`
 *  - 1 `special response`. Since `special request` can be sent without waiting
 * for previous response, it may be possible to receive it right after sending
 * `normal response` so that a second simultaneaous response slot is required.
 */
#define SESSION_MAX_SIMULTANEOUS_MSG  5

/**
 * The capacity requested for each ring buffer in num of slot, e.g pnc_message_t:
 *  - should be a power of 2 for a correct alignment of ring buffers.
 *  - should allow storage of the maximum num of pnc_message_t possible
 */
#define PNC_MESSAGE_RING_SLOT_CAPACITY  UINT32_C(0x100)
_Static_assert(PNC_MESSAGE_RING_SLOT_CAPACITY > 0 &&
    (PNC_MESSAGE_RING_SLOT_CAPACITY & (PNC_MESSAGE_RING_SLOT_CAPACITY - 1)) == 0,
    "PNC_MESSAGE_RING_SLOT_CAPACITY must be a power of 2");
_Static_assert(PNC_MESSAGE_RING_SLOT_CAPACITY >=
    REE_MAX_SESSIONS * SESSION_MAX_SIMULTANEOUS_MSG,
    "PNC_MESSAGE_RING_SLOT_CAPACITY must be large enough for the worst case");

/** Generate API for ring buffer handling.
 *
 * We're using identical unidirectional ring buffers:
 *   - one used for S --> NS messages
 *   - one used for NS --> S messages
 *
 * Linux is:
 * - producer on NS --> S ring buffer
 * - consumer on S --> NS ring buffer
 *
 * PNC is:
 * - producer on S --> NS ring buffer
 * - consumer on NS --> S ring buffer
 */
PNR_RING_GENERATE_UNI(struct pnc_message, pnc_message_ring);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-variable-sized-type-not-at-end"
#endif

/** Static definition of ring buffer used to manipulate pnc_message_t
 *
 * This structure is used to statically define amount of bytes used by a ring
 * buffer in pnc_shm_t defined below.
 */
typedef struct pnc_message_ring
{
    /* Shared part of a pnc_message_t ring buffer */
    pnc_message_ring_shared_t shared;
    /* Ring's slot padding. Not used directly, just here to allocate enough
     * space for PNC_MESSAGE_RING_SLOT_CAPACITY elements in shared.array */
    pnc_message_t padding[PNC_MESSAGE_RING_SLOT_CAPACITY];
} pnc_message_ring_t;

/**
* @brief Reserved SHM structure once initialised
*
* - a SHM header: used at init to validate SHM coherency between NS and S
* worlds.
* - 2 notification registers used to share notifications between NS and S
* worlds.
* - 2*N session's signals registers. With N the max num of sessions
* (\ref REE_MAX_SESSIONS). A set of registers, one for NS-->S, one for S-->NS,
* used to store pending signals indications.
* - 2 unidirectional ring buffers (one for NS->S, one for S->NS) to store
* pending messages indications.
*/
typedef struct pnc_shm
{
    /** Common SHM header */
    pnc_header_t hdr;

    /** NS-->S notification register */
    pnc_notification_t notif_ns_to_s;

    /** S-->NS notification register */
    pnc_notification_t notif_s_to_ns;

    /** NS->S sessions signal registers */
    pnc_signal_t signals_ns_to_s[REE_MAX_SESSIONS];

    /** S->NS sessions signal registers */
    pnc_signal_t signals_s_to_ns[REE_MAX_SESSIONS];

    /** NS->S ring buffer */
    pnc_message_ring_t ring_ns_to_s;

    /** S->NS ring buffer */
    pnc_message_ring_t ring_s_to_ns;
} pnc_shm_t;

#if defined(__clang__)
#pragma GCC diagnostic pop
#endif

_Static_assert((sizeof(pnc_shm_t) <= (REE_RESERVED_PAGES*PAGE_SIZE)), "not enough SHM reserved pages");

#endif /* REE_H_INCLUDED */
