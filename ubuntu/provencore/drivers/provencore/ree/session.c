// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, ProvenRun S.A.S
 */
/**
 * @file
 * @brief
 * @author Henri Chataing
 * @date October 07th, 2019 (creation)
 * @copyright (c) 2019-2021, Prove & Run S.A.S and/or its affiliates.
 *   All rights reserved.
 */

#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>

#include "internal.h"
#include "ree.h"
#include "session.h"
#include "shm.h"
#include "smc.h"

/* The SID value used when configuring a session by its PNC sysproc name */
#define TZ_CONFIG_ARG_GETSYSPROC_SID UINT32_MAX

/** Max time to wait for A_TERM_ACK when terminating a session. In order to
 * avoid being blocked if termination triggered after communication errors... */
#define TERMINATION_TIMEOUT     500  /** Arbitrary 500 ms... */

#ifndef CONFIG_PROVENCORE_REE_SERVICE_TIMEOUT
#define CONFIG_PROVENCORE_REE_SERVICE_TIMEOUT 0
#endif

/**
 * @brief handle on a session opened between a linux application and
 *  a Provencore service.
 */
struct pnc_session
{
    /** Availability of the session handle. */
    _Bool free;

    /** Session index (fixed). */
    unsigned int index;

    /** Allocated memory range. */
    pnc_shm_block_t *mem;

    /** Session states. */
    session_state_t global_state;
    session_state_t server_state;
    session_state_t client_state;

    /** Content of last pending message(s) */
    pnc_message_t server_message;
    pnc_message_t client_message;

    /** Semaphore to protect against multiple session accesses */
    struct semaphore sem;

    /** Pending event(s) for this session */
    uint32_t event_pending;

    /** Wait queue for event polling. */
    wait_queue_head_t event_wait;
};

/**
 * Sessions framework handling
 *
 * At start-up, no session work shall start until secure world signals it is
 * ready to work with SHM. So, @_session_ready is set to SESSION_NOT_READY by 
 * default and @_session_waitq is used to wait for @_session_ready switching to 
 * SESSION_READY.
 *
 * @_session_ready is also switched to SESSION_ENDED while running module_exit.
 * Upon E_RESET reception, @_session_ready is set to SESSION_NOT_READY: in 
 * addition of invalidating SHM header, this is a way to stop all sessions 
 * operations until the end of exit or reset.
 *
 * A spin_lock is used to protect quick read/write accesses to @_session_ready
 */
enum {
    SESSION_NOT_READY = 0,
    SESSION_READY,
    SESSION_ENDED
};
static int _session_ready = SESSION_NOT_READY;
static DECLARE_WAIT_QUEUE_HEAD(_session_waitq);
static DEFINE_SPINLOCK(_session_lock);

/** Preallocated session handles. */
static pnc_session_t _sessions[REE_MAX_SESSIONS];

/** Mutex to protect @_sessions accesses */
static DEFINE_MUTEX(_sessions_mutex);

/** Linux private part of the NS --> S ring buffer. */
static pnc_message_ring_producer_t _ns_to_s_ring;

/** Linux private part of the S --> NS ring buffer. */
static pnc_message_ring_consumer_t _s_to_ns_ring;

/** Notification registers addr */
static uint32_t *_ns_to_s_notification_register = NULL;
static uint32_t *_s_to_ns_notification_register = NULL;

/** Sessions signal registers */
static pnc_signal_t *_ns_to_s_signals = NULL;
static pnc_signal_t *_s_to_ns_signals = NULL;

/** Spinlock to control access to NS-->S ring buffer. */
static DEFINE_SPINLOCK(ring_lock);

/** Session to start signal notification */
static pnc_session_t *_signal_session = NULL;

/** This code is built with a REE code version defined by REE_VERSION 
 * It is the same for non secure code that may differ.
 * Protocol at start up is:
 *  - NS puts his value of REE_VERSION and signals S
 *  - S acknowledges, puts synchronized value and signals NS
 * S and NS parts synchronize on the real version of code to use: the lower one.
 * As a result, both S and NS can decide to use a feature, or not, given that 
 * the counterpart is expected to support it, or not.
 * We store the synchronized value in below variable the 1st time we acknowledge
 * S value from SHM header.
 */
static uint32_t _ree_version = 0;

/**
 * Notify the ProvenCore ree application.
 */
static void notify_s(void)
{
    /* Send ACTION_FROM_NS to the monitor. */
    struct pnc_smc_params params = { .a0 = SMC_ACTION_FROM_NS, };
    pnc_sched_smc(&params);
}

/**
 * Notify the Provencore ree application for new message if not already done
 */
static void notify_ns_message(void)
{
    int ret;
    uint32_t ns_notifications;

    /* Commit _ns_to_s_ring produced message(s).*/
    spin_lock(&ring_lock);
    ret = pnc_message_ring_producer_commit(&_ns_to_s_ring);
    spin_unlock(&ring_lock);

    /* If new message(s) produced: notify S. */
    if (ret != 0) {
        /* Atomically:
         *  - read _ns_to_s_notification_register
         *  - set E_MESSAGE
         */
        ns_notifications = atomic_fetch_or_explicit(_ns_to_s_notification_register,
            E_MESSAGE, memory_order_release);
        /* Check if there is already a pending notification */
        if (ns_notifications == 0) {
            /* There was no pending NS notification */
            notify_s();
        }
    }
}

/**
 * @brief Write message in NS-->S ring buffer if room available
 *
 * IMPORTANT NOTE:
 * At the end of this call, the message is not visible to consumer until a call
 * to pnc_message_ring_producer_commit is done.
 *
 * Assumption is that ring buffer can never be full and that write is always
 * possible.
 *
 * @param ree_msg_ptr  Formatted message to write in NS-->S ring buffer
 */
static void write_ns_message(pnc_message_t *ree_msg_ptr)
{
    /* Write the message in ring buffer and quit: this is caller responsability
     * to signal new message is available and to notify S if needed */
    spin_lock(&ring_lock);
    pnc_message_ring_producer_checkout(&_ns_to_s_ring);
    pnc_message_ring_producer_produce(&_ns_to_s_ring, ree_msg_ptr);
    spin_unlock(&ring_lock);
    return;
}

/**
 * @brief Disable any NS or S session operation
 *
 * This function is called during module exit or upon E_RESET reception.
 *
 * Under critical section, we disable sessions framework and reset SHM
 * header. As a result, until all configured sessions are closed and S has
 * re-initialised its part of the SHM, any S notification, so as any NS request
 * will be ignored.
 */
static void invalidate_sessions(void)
{
    unsigned long flags;
    spin_lock_irqsave(&_session_lock, flags);
    _session_ready = SESSION_NOT_READY;
    pnc_shm_init_header();
    spin_unlock_irqrestore(&_session_lock, flags);
}

/* ========================================================================== *
 *   Code for NOTIF_S_MESSAGE handling                                        *
 * ========================================================================== */

/*
 * @brief Handle A_REQUEST reception
 */
static void handle_s_request(pnc_message_t *ree_msg_ptr)
{
    pnc_session_t *s = &_sessions[ree_msg_ptr->index];

    /* Check session state: do nothing if not S_CONFIGURED */
    if (s->global_state == S_CONFIGURED) {
        /* Check server state */
        switch (s->server_state) {
            case S_IDLE:
                /* Copy S request */
                memcpy(&s->server_message, ree_msg_ptr, sizeof(pnc_message_t));
                /* Notify any application waiting for new request */
                s->event_pending |= EVENT_PENDING_REQUEST;
                wake_up_interruptible(&s->event_wait);
                /* Update server state */
                s->server_state = S_NOTIFIED;
                break;
            default:
                /* Break of protocol: ignore */
                return;
        }
    }
}

/*
 * @brief Handle A_RESPONSE reception
 */
static void handle_s_response(pnc_message_t *ree_msg_ptr)
{
    pnc_session_t *s = &_sessions[ree_msg_ptr->index];

    /* Check session state: do nothing if not S_CONFIGURED */
    if (s->global_state == S_CONFIGURED) {
        /* Check client state */
        switch (s->client_state) {
            case S_WAITING:
            case S_CANCEL_WAITING:
                /* Copy S response */
                memcpy(&s->client_message, ree_msg_ptr, sizeof(pnc_message_t));
                /* Notify any application waiting for A_RESPONSE */
                s->event_pending |= EVENT_PENDING_RESPONSE;
                wake_up_interruptible(&s->event_wait);
                /* Update client state */
                s->client_state = S_NOTIFIED;
                break;
            default:
                /* Break of protocol: ignore */
                break;
        }
    }
}

/*
 * @brief Handle A_CONFIG_ACK reception
 */
static void handle_s_config_ack(pnc_message_t *ree_msg_ptr)
{
    pnc_session_t *s = &_sessions[ree_msg_ptr->index];

    /* Check session state: do nothing if not S_CONFIG_WAITING */
    if (s->global_state == S_CONFIG_WAITING) {
        if (ree_msg_ptr->p1 == 0) {
            /* Session is configured, ready for client or server operations. */
            s->global_state = S_CONFIGURED;
            s->server_state = S_IDLE;
            s->client_state = S_IDLE;
        } else {
            pr_err("(%s) session (%u) S config failure (%u)\n", __func__,
                s->index, ree_msg_ptr->p1);
            s->global_state = S_NULL;
            /* Copy S "server" answer for session user */
            memcpy(&s->client_message, ree_msg_ptr, sizeof(pnc_message_t));
        }
        /* Notify any application waiting end of config */
        s->event_pending |= EVENT_PENDING_RESPONSE;
        wake_up_interruptible(&s->event_wait);
    }
}

/*
 * @brief Handle A_CANCEL reception
 */
static void handle_s_cancel(pnc_message_t *ree_msg_ptr)
{
    pnc_session_t *s = &_sessions[ree_msg_ptr->index];
    pnc_message_t ree_msg = { 0 };

    ree_msg.index = s->index;

    /* Check session state: do nothing if not S_CONFIGURED */
    if (s->global_state == S_CONFIGURED) {
        /* Check server state */
        switch (s->server_state) {
            case S_NOTIFIED:
                /* Remove pending request: application will never receive it */
                memset(&s->server_message, 0, sizeof(pnc_message_t));
                /* Send A_CANCEL_ACK */
                ree_msg.action = A_CANCEL_ACK;
                write_ns_message(&ree_msg);
                s->server_state = S_IDLE;
                break;
            default:
                /* A_RESPONSE is in the pipe, do nothing and it will be sent
                 * soon instead of A_CANCEL_ACK.
                 */
                return;
        }
    }
}

/*
 * @brief Handle A_CANCEL_ACK reception
 */
static void handle_s_cancel_ack(pnc_message_t *ree_msg_ptr)
{
    pnc_session_t *s = &_sessions[ree_msg_ptr->index];

    /* Check session state: do nothing if not S_CONFIGURED */
    if (s->global_state == S_CONFIGURED) {
        /* Check client state */
        switch (s->client_state) {
            case S_CANCEL_WAITING:
                /* Copy S response */
                memcpy(&s->client_message, ree_msg_ptr, sizeof(pnc_message_t));
                /* Notify any application waiting for A_CANCEL_ACK */
                s->event_pending |= EVENT_PENDING_RESPONSE;
                wake_up_interruptible(&s->event_wait);
                /* Update client state */
                s->client_state = S_NOTIFIED;
                break;
            default:
                /* Break of protocol: ignore. */
                break;
        }
    }
}

/*
 * @brief Handle A_TERM reception
 */
static void handle_s_term(pnc_message_t *ree_msg_ptr)
{
    pnc_session_t *s = &_sessions[ree_msg_ptr->index];
    pnc_message_t ree_msg = { 0 };

    if (s->global_state == S_CONFIGURED) {
        /* Clear any pending NS-->S pending signal */
        atomic_exchange_explicit(&_ns_to_s_signals[s->index], 0,
            memory_order_acquire);

        /* Notify any waiting application */
        s->event_pending = EVENT_PENDING_ALL;
        wake_up_interruptible(&s->event_wait);
    }

    /* Send A_TERM_ACK */
    ree_msg.index = s->index;
    ree_msg.action = A_TERM_ACK;
    write_ns_message(&ree_msg);

    /* Switch session to S_NULL */
    s->global_state = S_NULL;

    return;
}

/*
 * @brief Handle A_TERM_ACK reception
 */
static void handle_s_term_ack(pnc_message_t *ree_msg_ptr)
{
    pnc_session_t *s = &_sessions[ree_msg_ptr->index];

    /* Check session state: do nothing if not S_TERM_WAITING */
    if (s->global_state == S_TERM_WAITING) {
        /* Notify any application waiting end of session termination */
        s->event_pending |= EVENT_PENDING_RESPONSE;
        wake_up_interruptible(&s->event_wait);
    }
}

/**
 * @brief Handle new S pnc_message_t
 */
static void handle_s_message(pnc_message_t *ree_msg_ptr)
{
    pnc_session_t *s;

    if (ree_msg_ptr->index >= REE_MAX_SESSIONS ||
        _sessions[ree_msg_ptr->index].free) {
            pr_debug("(%s) bad state\n", __func__);
            return;
    }

    pr_debug("(%s) index=%u\n", __func__, ree_msg_ptr->index);

    s = &_sessions[ree_msg_ptr->index];

    /* Acquire the lock on the session. */
    down(&s->sem);

    switch(ree_msg_ptr->action) {
        case A_REQUEST:
            handle_s_request(ree_msg_ptr);
            break;

        case A_RESPONSE:
            handle_s_response(ree_msg_ptr);
            break;

        case A_CONFIG:
            /* It is NS responsible for A_CONFIG sending, can't receive it...
             * Simply ignore...
             */
            break;

        case A_CONFIG_ACK:
            handle_s_config_ack(ree_msg_ptr);
            break;

        case A_CANCEL:
            handle_s_cancel(ree_msg_ptr);
            break;

        case A_CANCEL_ACK:
            handle_s_cancel_ack(ree_msg_ptr);
            break;

        case A_TERM:
            handle_s_term(ree_msg_ptr);
            break;

        case A_TERM_ACK:
            handle_s_term_ack(ree_msg_ptr);
            break;

        default:
            pr_err("(%s) unknown message (%u) for session %u\n", __func__,
                ree_msg_ptr->action, ree_msg_ptr->index);
            break;
    }

    up(&s->sem);
    return;
}

/**
 * @brief Wake up any application waiting for signal on a given session
 */
static void handle_s_signal(pnc_session_t *s)
{
    down(&s->sem);

    /* Wake up any application waiting for new signal */
    s->event_pending |= EVENT_PENDING_SIGNAL;
    wake_up_interruptible(&s->event_wait);

    up(&s->sem);

    return;
}

/**
 * @brief handle E_RESET notification
 */
static void handle_s_reset(void)
{
    int i;

    /* Close configured sessions */
    for (i=0; i<REE_MAX_SESSIONS; i++) {
        if (_sessions[i].free == 0) {
            pnc_session_close(&_sessions[i]);
        }
    }

    /* Invalidate any next NS or S session operation until NS<-->S re-sync */
    invalidate_sessions();

    /* Forward SHM geometry to the secure monitor */
    pnc_shm_forward();

    /* Trigger 1st valid action from NS after new geometry forward.
     * Just like at driver start-up, this action is the signal for secure REE
     * application that SHM re-init is ended in the non secure world */
    notify_s();
}

/* ========================================================================== *
 *   Code for S notifications handling                                        *
 * ========================================================================== */

/**
 * @brief Handle notifications from the secure application.
 * @param work          work parameter
 */
static void handle_s_notification(struct work_struct *work)
{
    pnc_notification_t s_notifications;
    uint32_t signals;
    pnc_session_t *s;
    pnc_message_t ree_msg = {0};

    (void)work;

    /* Check SHM coherency */
    if (!pnc_shm_ready()) {
        pr_err("(%s) SHM not ready\n", __func__);
        return;
    }

    /* Atomically:
     *  - read _s_to_ns_notification register
     *  - clear it
     * Secure world may now send new notifications if needed.
     */
    s_notifications = atomic_exchange_explicit(_s_to_ns_notification_register,
                        0, memory_order_acquire);

    /* Look for config change notification.
     * If any, reset and restart the driver
     */
    if (s_notifications & E_RESET) {
        /* Secure world indicates it is about to reset the system: initiate
         * NS reset...
         */
        handle_s_reset();
        return;
    }

    /* Look for new session's signal notification.
     * If any, look for targetted session and notify any application waiting
     * for signal on this session.
     * Signals are kept pending until applications acknowledge them.
     */
    signals =  s_notifications & SESSIONS_SIGNAL_MASK;
    if (signals) {
        pr_debug("(%s) signal for sessions: (0x%x) \n", __func__, signals);
        /* Notify any application waiting for signal on matching session */
        s = _signal_session;
        do {
            if (signals & (UINT32_C(1) << s->index)) {
                handle_s_signal(s);
            }
            /* Switch to next session in table */
            s =  &_sessions[(s->index+1)%REE_MAX_SESSIONS];
        } while(s != _signal_session);

        /* Next time we will have to notify session for signal, we will not
         * start from the same session in table...
         */
        _signal_session = &_sessions[(_signal_session->index+1)%REE_MAX_SESSIONS];
    }

    /* Look for new message(s) notification. */
    if (s_notifications & E_MESSAGE) {
        /* Parse _s_to_ns_ring for new messages */
        while (pnc_message_ring_consumer_checkout(&_s_to_ns_ring)) {
            /* Consume new message */
            while (pnc_message_ring_consumer_consume(&_s_to_ns_ring, &ree_msg)) {
                /* Handle new message */
                handle_s_message(&ree_msg);
            }
        }

        /* Commit _s_to_ns_ring message consumption. */
        pnc_message_ring_consumer_commit(&_s_to_ns_ring);

        /* Notify S for any new message produced in NS --> S ring buffer */
        notify_ns_message();
    }
}

DECLARE_WORK(_notification_work, handle_s_notification);

irqreturn_t pnc_session_interrupt_handler(int irq, void *dev_id)
{
    (void)dev_id;
    (void)irq;
    void *shm_base;
    pnc_header_t *header;

    if (_session_ready == SESSION_NOT_READY) {
        /* Could be an interrupt from secure world to indicate it is ready to
         * use SHM: check for it.
         */
        if (pnc_shm_ready()) {
            /* This is indeed the 1st S interrupt to signal REE readiness. Let's
             * take the opportunity to sync on code versions: get synchronized 
             * REE version from SHM header.
             */
            shm_base = pnc_shm_base();
            header = (pnc_header_t *)shm_base;
            _ree_version = header->version;

            /* Secure world is ready...
             * Unlock any client waiting to open new session.
             */
            spin_lock(&_session_lock);
            _session_ready = SESSION_READY;
            spin_unlock(&_session_lock);
            wake_up_all(&_session_waitq);

            /* Schedule secure notification handler because S driver may have
             * other things to signal.
             */
            schedule_work(&_notification_work);
        }
        /* Can be a spurious interrupt or the signal from S world. In any
         * case we don't try to look at notification register for now... */
        return IRQ_HANDLED;
    }

    /* Schedule secure notification handler. */
    schedule_work(&_notification_work);
    return IRQ_HANDLED;
}

int pnc_sessions_init(void)
{
    void *ns_to_s_ring_base, *s_to_ns_ring_base;
    size_t ns_to_s_ring_size, s_to_ns_ring_size;
    unsigned int index;
    unsigned long rings_size;
    pnc_shm_t *shm_base;
    void *rings_base;

    /* Get SHM base addr and clear REE reserved area */
    shm_base = (pnc_shm_t *)pnc_shm_base();
    if (shm_base == NULL) {
        pr_err("(%s) SHM not initialised\n", __func__);
        return -ENOMEM;
    }
    memset(shm_base, 0, sizeof(pnc_shm_t));

    /* Init SHM header */
    pnc_shm_init_header();

    /* Get notification registers addr */
    _ns_to_s_notification_register = (uint32_t *)&shm_base->notif_ns_to_s;
    _s_to_ns_notification_register = (uint32_t *)&shm_base->notif_s_to_ns;

    /* Get signals area addr */
    _ns_to_s_signals = shm_base->signals_ns_to_s;
    _s_to_ns_signals = shm_base->signals_s_to_ns;

    /* Build ring buffers memory geometry */
    rings_base = &shm_base->ring_ns_to_s.shared;
    rings_size = 2*sizeof(pnc_message_ring_t);

    ns_to_s_ring_base = rings_base;
    ns_to_s_ring_size = rings_size / 2;
    s_to_ns_ring_base = (void *)((char *)rings_base + ns_to_s_ring_size);
    s_to_ns_ring_size = rings_size - ns_to_s_ring_size;

    /* Configure the ring buffers. */
    pnc_message_ring_shared_init(ns_to_s_ring_base);
    pnc_message_ring_producer_init(&_ns_to_s_ring, ns_to_s_ring_base,
        ns_to_s_ring_size);
    pnc_message_ring_shared_init(s_to_ns_ring_base);
    pnc_message_ring_consumer_init(&_s_to_ns_ring, s_to_ns_ring_base,
        s_to_ns_ring_size);

    /* Clear the session handles. */
    memset(_sessions, 0, sizeof(_sessions));
    for (index = 0; index < REE_MAX_SESSIONS; index++) {
        _sessions[index].index = index;
        _sessions[index].free = 1;
        _sessions[index].global_state = S_NULL;
        _sessions[index].event_pending = 0;
        sema_init(&_sessions[index].sem, 1);
        init_waitqueue_head(&_sessions[index].event_wait);
    }
    _signal_session = &_sessions[0];
    return 0;
}

void pnc_sessions_exit(void)
{
    int i;
    uint32_t ns_notifications;

    /* We're leaving driver... Flush any pending notification work in order to
     * avoid spurious kernel crash...
     */
    flush_work(&_notification_work);

    /* Close configured sessions */
    for (i=0; i<REE_MAX_SESSIONS; i++) {
        if (_sessions[i].free == 0) {
            pnc_session_close(&_sessions[i]);
        }
    }

    /* Invalidate any next NS or S session operation until NS<-->S re-sync */
    invalidate_sessions();

    /* ... + last E_RESET notification in order to warn S about the end...
     * Indicate new notification is pending for this session
     * Atomically:
     *  - read _ns_to_s_notification_register
     *  - set s->index bit pending
     */
    ns_notifications = atomic_fetch_or_explicit(_ns_to_s_notification_register,
            E_RESET, memory_order_release);

    /* Check if there is already a pending notification */
    if (ns_notifications == 0) {
        /* There was no pending NS notification */
        notify_s();
    }
}

int pnc_session_get_mem_offset(pnc_session_t *session,
                               unsigned long *offset, unsigned long *nr_pages)
{
    if (session == NULL) {
        pr_err("(%s) invalid session\n", __func__);
        return -EINVAL;
    }
    if (session->mem == NULL) {
        /* No memory allocated for this session */
        pr_warn("(%s) no memory allocated\n", __func__);
        return -ENOMEM;
    }

    if (offset != NULL) {
        *offset = session->mem->offset;
    }
    if (nr_pages != NULL) {
        *nr_pages = session->mem->nr_pages;
    }
    return 0;
}

void pnc_sessions_sync(struct work_struct *work)
{
    (void)work;
    int ret;
    unsigned long lock_flags;
    uint32_t local_sessions;

    /* Wait for global synchro between NS and S regarding SHM initialization */
    wait_event(_session_waitq, (_session_ready != SESSION_NOT_READY));

    spin_lock_irqsave(&_session_lock, lock_flags);
    local_sessions = _session_ready;
    spin_unlock_irqrestore(&_session_lock, lock_flags);
    if (local_sessions == SESSION_READY) {
        /* Now that the driver is synchronized with ProvenCore, register the device */
        ret = register_device();
        if (ret) {
            return;
        }
        pr_info("Framework ready with version 0x%x\n", _ree_version);
    } else {
        pr_info("REE synchro. aborted.\n");
    }
}

void pnc_sessions_release(void)
{
    unsigned long lock_flags;
    spin_lock_irqsave(&_session_lock, lock_flags);
    _session_ready = SESSION_ENDED;
    spin_unlock_irqrestore(&_session_lock, lock_flags);
    wake_up_all(&_session_waitq);
}

/* ========================================================================== *
 *   Code for session public Kernel API                                       *
 *   Descriptions of exported symbol in include/misc/provencore/session.h     *
 * ========================================================================== */

/**
 * @brief Do some coherency checks for a given session and check it is in
 *        S_CONFIGURED state
 *
 * @param s     handle of session to check
 * @return      - 0 if OK and S_CONFIGURED
 *              - -ERESTARTSYS: system error trying to take s->sem lock.
 *              - -EAGAIN: sessions framework not yet enabled.
 *              - -ENOENT: SHM init is not finalized...SHM not ready or corrupted
 *              - -EINVAL: invalid session handle or session slot is not in use
 *              - -ENODEV: session not configured
 *              for a valid session.
 */
static int check_session_configured(pnc_session_t *s)
{
    int ret;
    unsigned long flags;
    uint32_t local_sessions;

    /* Check sessions framework */
    spin_lock_irqsave(&_session_lock, flags);
    local_sessions = _session_ready;
    spin_unlock_irqrestore(&_session_lock, flags);
    if (local_sessions != SESSION_READY) {
        pr_warn("(%s) session framework disabled\n", __func__);
        return -EAGAIN;
    }

    /* Check whether SHM is initialized. */
    if (!pnc_shm_ready()) {
        pr_err("(%s) SHM not initialized\n", __func__);
        return -ENOENT;
    }

    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    /* Check session is not invalid */
    if (s->index >= REE_MAX_SESSIONS) {
        pr_err("(%s) session invalid (%u)\n", __func__, s->index);
        ret = -EINVAL;
        goto end_check;
    }

    /* Check session is not closed */
    if (s->free) {
        pr_err("(%s) closed session\n", __func__);
        ret = -EINVAL;
        goto end_check;
    }

    /* Check session is configured */
    if (s->global_state != S_CONFIGURED) {
        pr_err("(%s) session (%u) not configured\n", __func__, s->index);
        ret = -ENODEV;
        goto end_check;
    }

    /* Session ON and configured... */
    ret = 0;

end_check:
    up(&s->sem);
    return ret;
}

/**
 * @brief Deal with broken or terminated session
 *
 * @param session The session to check
 *
 * @return
 *   - -ERESTARTSYS if interrupted while waiting for the semaphore
 *   - -EPIPE       if the session was terminated
 *   - 0            otherwise
 */
static int check_and_handle_terminated_session(pnc_session_t *session)
{
    if (session->global_state == S_CONFIGURED)
        return 0;

    if (down_interruptible(&session->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    if (session->global_state == S_TERM_WAITING) {
        /* It is normal session is not configured after wait... */
        session->global_state = S_NULL;
        up(&session->sem);
        return 0;
    }

    /* Terminated session: normally because of a call to pnc_session_close
     * Just to be sure, finalize close if not done.
     */
    if (session->global_state != S_NULL) {
        session->global_state = S_NULL;
    }
    if (session->mem) {
        pnc_shm_free(session->mem);
        session->mem = NULL;
    }
    up(&session->sem);
    mutex_lock(&_sessions_mutex);
    session->free = 1;
    mutex_unlock(&_sessions_mutex);

    return -EPIPE;
}

static int wait_session_event(pnc_session_t *s, uint32_t mask, uint32_t *events,
    uint32_t timeout)
{
    int ret;

    /* Filter out invalid events */
    mask &= EVENT_PENDING_ALL;

    /* Wait for A_RESPONSE reception */
    if (timeout != 0) {
        /* Wait for a while...
         * Returns:
         *  - 0 if condition evaluated to false after timeout elapsed
         *  - 1 if condition evaluated to true after timeout elapsed
         *  - the remaining jiffies (at least 1) if the condition
         *      evaluated to true before the timeout elapsed
         *  - -ERESTARTSYS if it was interrupted by a signal
         */
        ret = wait_event_interruptible_timeout(s->event_wait,
                (s->event_pending & mask), (timeout * HZ) / 1000);
        if (ret == 0) {
            ret = -ETIMEDOUT;
        } else if (ret >= 1) {
            ret = 0;
        }
    } else {
        /*
         * Returns:
         *  - 0 if condition evaluated to true
         *  - -ERESTARTSYS if it was interrupted by a signal
         */
        ret = wait_event_interruptible(s->event_wait,
                (s->event_pending & mask));
    }

    if (down_interruptible(&s->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }
    if (ret == 0) {
        if (events != NULL) {
            /* Get event(s) value. Only the one matching expected \p mask... */
            *events = (s->event_pending & mask);
        }
        /* Clear pending event(s) we were waiting for */
        s->event_pending &= ~mask;
    }
    up(&s->sem);

    /* Need to check if session broken or terminated while waiting response */
    int ret2 = check_and_handle_terminated_session(s);
    if (ret2 == -ERESTARTSYS)
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);

    return ret2 == 0 ? ret : ret2;
}

/**
 * @brief Send A_TERM and wait for A_TERM_ACK
 *
 * Internal function so that no coherency check done before starting terminating
 * this session...
 *
 * @param s     session handle
 */
static void send_term(pnc_session_t *s)
{
    pnc_message_t ree_msg = {0};

    /* Clear any pending NS-->S pending signal */
    atomic_exchange_explicit(&_ns_to_s_signals[s->index], 0,
        memory_order_acquire);

    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return;
    }

    /* Fill in the request. */
    ree_msg.index = s->index;
    ree_msg.action = A_TERM;

    /* Write request in ring buffer */
    write_ns_message(&ree_msg);

    /* Switch session to S_TERM_WAITING */
    s->global_state = S_TERM_WAITING;
    up(&s->sem);

    /* Notify request */
    notify_ns_message();

    /* Will now wait for A_TERM_ACK reception. Don't wait forever... */
    wait_session_event(s, EVENT_PENDING_RESPONSE, NULL, TERMINATION_TIMEOUT);

    return;
}

/**
 * @brief [client] Send A_REQUEST
 *
 * Don't wait for A_RESPONSE...
 *
 * @param s         session handle
 * @param request   request to send
 * @return  0 if request correctly written in ring buffer and server notified so
 *          that client can start waiting for server response, negative error
 *          otherwise:
 *              -ERESTARTSYS: system signal received to end current
 *              processus/thread whiel trying to down ring's or session's sem
 *              -ENODEV: session not configured...
 *              -EBUSY: client is busy, NOK to send new request
 */
static int send_request(pnc_session_t *s, uint32_t request)
{
    pnc_message_t ree_msg = {0};

    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    /* Check client state */
    if (s->client_state != S_IDLE) {
        pr_err("(%s) session %u client is not ready for sending request (%u)\n",
            __func__, s->index, (unsigned int)s->client_state);
        up(&s->sem);
        return -EPROTO;
    }

    /* Fill in the message. */
    ree_msg.index = s->index;
    ree_msg.action = A_REQUEST;
    ree_msg.p1 = request;

    /* Write message in ring buffer */
    write_ns_message(&ree_msg);
    /* notify S */
    s->client_state = S_WAITING;
    up(&s->sem);
    notify_ns_message();

    return 0;
}

/**
 * @brief [client] Get available A_RESPONSE if any
 */
static int get_response(pnc_session_t *s, uint32_t *response)
{
    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    /* Check client is S_NOTIFIED */
    if (s->client_state != S_NOTIFIED) {
        /* Client busy, not really an error here: return -EAGAIN as a status */
        pr_warn("(%s) client busy (%d/%u)\n", __func__, s->index, s->client_state);
        up(&s->sem);
        return -EAGAIN;
    }

    /* Get response and switch session client to S_IDLE */
    if (response) {
        *response = s->client_message.p1;
        memset(&s->client_message, 0, sizeof(pnc_message_t));
    }
    s->client_state = S_IDLE;

    s->event_pending &= ~EVENT_PENDING_RESPONSE;

    up(&s->sem);

    return 0;
}

static int _session_open(pnc_session_t **session, unsigned int flags)
{
    unsigned int index;
    unsigned long lock_flags;
    uint32_t local_sessions;

    pr_debug("%s: flags: 0x%x\n", __func__, flags);

    if (flags & O_NONBLOCK) {
        /* Don't wait for secure world if not ready */
        goto check_ready;
    }

    /* By default, wait for global synchro between NS and S regarding SHM 
     * initialization */
    wait_event(_session_waitq, (_session_ready != SESSION_NOT_READY));

    /* Check if secure world is ready */
check_ready:
    spin_lock_irqsave(&_session_lock, lock_flags);
    local_sessions = _session_ready;
    spin_unlock_irqrestore(&_session_lock, lock_flags);
    if (local_sessions != SESSION_READY) {
        /* Secure world is not ready yet */
        if (flags & O_NONBLOCK) {
            /* This may be acceptable */
            return -EAGAIN;
        }
        /* This is an error */
        pr_err("(%s) secure world is not ready.\n", __func__);
        return -ENOENT;
    }

    mutex_lock(&_sessions_mutex);
    /* Allocate a session handle. */
    for (index = 0; index < REE_MAX_SESSIONS; index++) {
        if (_sessions[index].free) {
            _sessions[index].free = 0;
            *session = &_sessions[index];
            _sessions[index].event_pending = 0;
            memset(&_sessions[index].client_message, 0, sizeof(pnc_message_t));
            memset(&_sessions[index].server_message, 0, sizeof(pnc_message_t));
            mutex_unlock(&_sessions_mutex);
            return 0;
        }
    }
    mutex_unlock(&_sessions_mutex);
    /* Fail if all session handles are used. */
    pr_err("(%s) no free session slot\n", __func__);
    return -ENOMEM;
}

int pnc_session_open(pnc_session_t **session)
{
    return _session_open(session, 0);
}
EXPORT_SYMBOL(pnc_session_open);

int pnc_session_open_with_flags(pnc_session_t **session, unsigned int flags)
{
    return _session_open(session, flags);
}
EXPORT_SYMBOL(pnc_session_open_with_flags);

void pnc_session_close(pnc_session_t *session)
{
    int ret;
    if (session != NULL) {
        ret = check_session_configured(session);
        if (ret == 0) {
            send_term(session);
        }
        /* down session->sem: don't care about result (we need to close...) but 
         * keep result to avoid up not owned semaphore.
         */
        ret = down_interruptible(&session->sem);
        session->global_state = S_NULL;
        pnc_shm_free(session->mem);
        session->mem = NULL;
        /* Notify any waiting application */
        session->event_pending = EVENT_PENDING_ALL;
        wake_up_interruptible(&session->event_wait);
        if (ret == 0)
            up(&session->sem);
        mutex_lock(&_sessions_mutex);
        session->free = 1;
        mutex_unlock(&_sessions_mutex);
    }
}
EXPORT_SYMBOL(pnc_session_close);

int pnc_session_get_version(pnc_session_t *session, uint32_t *version)
{
    if ((session == NULL) || (version == NULL)) {
        pr_err("(%s) Bad descriptors\n", __func__);
        return -EBADF;
    }

    *version = _ree_version;
    return 0;
}
EXPORT_SYMBOL(pnc_session_get_version);

int pnc_session_alloc(pnc_session_t *session, unsigned long size)
{
    if (session->mem != NULL) {
        pr_err("(%s) Session already configured\n", __func__);
        return -EEXIST;
    }
    if (size == 0) {
        pr_err("(%s) Size requested\n", __func__);
        return -EINVAL;
    }
    size += PAGE_SIZE - 1;
    return pnc_shm_alloc(size >> PAGE_SHIFT, &session->mem);
}
EXPORT_SYMBOL(pnc_session_alloc);

static int configure_session(pnc_session_t *s, uint64_t sid, const char *name)
{
    int ret;
    pnc_message_t ree_msg = {0};
    unsigned long flags;
    uint32_t local_sessions;
    void *base = pnc_shm_base();

    if (name != NULL) {
        pr_debug("(%s) index=%u name=%s\n", __func__, s->index, name);
    } else {
        pr_debug("(%s) index=%u sid=%llu\n", __func__, s->index, sid);
    }

    /* Check sessions framework */
    spin_lock_irqsave(&_session_lock, flags);
    local_sessions = _session_ready;
    spin_unlock_irqrestore(&_session_lock, flags);
    if (local_sessions != SESSION_READY) {
        pr_err("(%s) session framework disabled\n", __func__);
        return -EAGAIN;
    }

    /* Check whether SHM is initialized. */
    if (!pnc_shm_ready()) {
        pr_err("(%s) SHM not initialized\n", __func__);
        return -ENOENT;
    }

    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: [1] interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    /* Check session is not invalid */
    if (s->index >= REE_MAX_SESSIONS) {
        pr_err("(%s) session invalid (%u)\n", __func__, s->index);
        up(&s->sem);
        return -EINVAL;
    }

    /* Check session is not closed */
    if (s->free) {
        pr_err("(%s) configuring closed session.\n", __func__);
        up(&s->sem);
        return -EINVAL;
    }

    if (s->global_state != S_NULL) {
        pr_err("(%s) session not in null state (%u)\n", __func__,
                (unsigned int)s->global_state);
        up(&s->sem);
        return -EBADF;
    }

    if (name != NULL) {
        if (_ree_version < 0x303) {
            pr_err("config by name failure\n");
            return -ENOTSUPP;
        }
        if (base == NULL) {
            /* Will certainly never get there but this check is mandatory for
             * a safe usage of base...
             */
            return -ENOMEM;
        }
        if (strlen(name) >= (s->mem->nr_pages * PAGE_SIZE)) {
            pr_err("invalid service name\n");
            return -EOVERFLOW;
        }
        strcpy((char *)base + (s->mem->offset * PAGE_SIZE), name);
    }

    /* Set SID marking bits */
    sid |= 1LLU << 62;
    sid |= 1LLU << 63;

    /* Fill in the request. */
    ree_msg.index = s->index;
    ree_msg.action = A_CONFIG;
    ree_msg.p0 = sid;
    if (s->mem != NULL) {
        /* Some additional pages allocated for this session */
        ree_msg.p1 = s->mem->offset;
        ree_msg.p2 = s->mem->nr_pages;
    }

    /* Write request in ring buffer */
    write_ns_message(&ree_msg);
    /* Notify S */
    s->global_state = S_CONFIG_WAITING;
    up(&s->sem);
    notify_ns_message();

    /* We will now wait for A_CONFIG_ACK reception and session to become
     * S_CONFIGURED */
    ret = wait_session_event(s, EVENT_PENDING_RESPONSE, NULL,
            CONFIG_PROVENCORE_REE_SERVICE_TIMEOUT);
    if (ret == 0) {
        goto end_config;
    } else if (ret == -EPIPE) {
        if (down_interruptible(&s->sem)) {
            pr_err("%s: [2] interrupted while waiting semaphore.\n", __func__);
            return -ERESTARTSYS;
        }
        pr_err("(%s) session (%u/%u) config failure\n", __func__, s->index,
            s->global_state);
        ret = s->client_message.p1;
        memset(&s->client_message, 0, sizeof(pnc_message_t));
        if (ret == 0) {
            /* We were woken up by valid reception of a S message.
             * But session is not configured, so this message wasn't A_CONFIG_ACK
             * There's a problem... Anyway, reports -ENODEV...
             */
            pr_err("(%s) system issue\n", __func__);
            ret = -ENODEV;
        }
        up(&s->sem);
        goto end_config;
    }
    pr_err("(%s) wait config failure (%d)\n", __func__, ret);

end_config:
    return ret;
}

int pnc_session_config_by_name(pnc_session_t *session, const char *name)
{
    return configure_session(session, TZ_CONFIG_ARG_GETSYSPROC_SID, name);
}
EXPORT_SYMBOL(pnc_session_config_by_name);

int pnc_session_config(pnc_session_t *session, uint64_t sid)
{
    return configure_session(session, sid, NULL);
}
EXPORT_SYMBOL(pnc_session_config);

int pnc_session_get_mem(pnc_session_t *session, char **ptr, unsigned long *size)
{
    int ret;
    unsigned long offset;
    unsigned long nr_pages;
    void *base = pnc_shm_base();

    if (base == NULL) {
        /* We MUST check for NULL pointer before using it.
         * Nevertheless, if a session is opened, then REE is setup and SHM 
         * was allocated: such event should never occur...  
         */
        pr_err("(%s) !!!! SHM not allocated\n", __func__);
        return -ENODEV;
    }

    ret = pnc_session_get_mem_offset(session, &offset, &nr_pages);
    if (ret != 0) {
        return ret;
    }
    if (ptr != NULL) {
        *ptr = (char *)base + (offset * PAGE_SIZE);
    }
    if (size != NULL) {
        *size = nr_pages * PAGE_SIZE;
    }
    return 0;
}
EXPORT_SYMBOL(pnc_session_get_mem);

int pnc_session_send_response(pnc_session_t *s, uint32_t response)
{
    int ret;
    pnc_message_t ree_msg = {0};

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    /* Check server state */
    if (s->server_state != S_BUSY) {
        pr_err("(%s) server in invalid state (%u)\n", __func__, s->server_state);
        up(&s->sem);
        return -EPROTO;
    }

    /* Fill in the message. */
    ree_msg.index = s->index;
    ree_msg.action = A_RESPONSE;
    ree_msg.p1 = response;

    /* Was waiting for sending A_RESPONSE: write it in ring buffer. */
    write_ns_message(&ree_msg);
    /* Notify S */
    s->server_state = S_IDLE;
    up(&s->sem);
    notify_ns_message();

    return ret;
}
EXPORT_SYMBOL(pnc_session_send_response);

int pnc_session_get_response(pnc_session_t *s, uint32_t *response)
{
    int ret;

    if (response == NULL) {
        pr_err("(%s) no buffer for request reception...\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    return get_response(s, response);
}
EXPORT_SYMBOL(pnc_session_get_response);

int pnc_session_wait_response(pnc_session_t *s, uint32_t *response,
        uint32_t timeout)
{
    int ret;

    if (response == NULL) {
        pr_err("(%s) no response buffer.\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    ret = wait_session_event(s, EVENT_PENDING_RESPONSE, NULL, timeout);
    if (ret == 0) {
        ret = get_response(s, response);
    }
    return ret;
}
EXPORT_SYMBOL(pnc_session_wait_response);

int pnc_session_send_request(pnc_session_t *s, uint32_t request)
{
    int ret;

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    return send_request(s, request);
}
EXPORT_SYMBOL(pnc_session_send_request);

int pnc_session_get_request(pnc_session_t *s, uint32_t *request)
{
    int ret;

    if (request == NULL) {
        pr_err("(%s) no buffer for request reception...\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: [1] interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    /* Check server is S_NOTIFIED */
    if (s->server_state != S_NOTIFIED) {
        /* Not an error here, return -EAGAIN as a status... */
        pr_warn("(%s) server not ready (%d/%u)\n", __func__, s->index,
            s->server_state);
        up(&s->sem);
        return -EAGAIN;
    }

    /* Get request */
    *request = s->server_message.p1;
    memset(&s->server_message, 0, sizeof(pnc_message_t));

    /* Server is now S_BUSY, waiting for application to response */
    s->server_state = S_BUSY;

    s->event_pending &= ~EVENT_PENDING_REQUEST;

    up(&s->sem);

    return 0;
}
EXPORT_SYMBOL(pnc_session_get_request);

int pnc_session_wait_request(pnc_session_t *s, uint32_t *request,
        uint32_t timeout)
{
    int ret;

    if (request == NULL) {
        pr_err("(%s) no request buffer.\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    if ((s->server_state != S_IDLE) && (s->server_state != S_NOTIFIED)) {
        /* Any other state is a break in protocol because we're asking for
         * new request whereas previous one wasn't answered.
         */
        up(&s->sem);
        pr_err("(%s) previous request not answered for session %u\n",
            __func__, s->index);
        return -EPROTO;
    }
    up(&s->sem);

    /* Wait for A_REQUEST reception and session's server to become S_NOTIFIED
     * if not already
     */
    ret = wait_session_event(s, EVENT_PENDING_REQUEST, NULL, timeout);
    if (ret == 0) {
        ret = pnc_session_get_request(s, request);
    }
    return ret;
}
EXPORT_SYMBOL(pnc_session_wait_request);

int pnc_session_cancel_request(pnc_session_t *s, uint32_t *response,
    uint32_t timeout)
{
    int ret;
    pnc_message_t ree_msg = {0};

    if (response == NULL) {
        pr_err("(%s) no response buffer.\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    /* Acquire the lock on the session. */
    if (down_interruptible(&s->sem)) {
        pr_err("%s: [1] interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    switch (s->client_state) {
        case S_WAITING:
            /* Waiting for previous request's response: try to cancel request */
            ree_msg.index = s->index;
            ree_msg.action = A_CANCEL;
            write_ns_message(&ree_msg);
            s->client_state = S_CANCEL_WAITING;
            notify_ns_message();
            break;
        case S_NOTIFIED:
            /* Response to previously sent A_REQUEST already there but not
             * yet fetched by client:
             *  - fetch it since we have buffer for it
             *  - remove pending event
             */
            *response = s->client_message.p1;
            memset(&s->client_message, 0, sizeof(pnc_message_t));
            s->event_pending &= ~(EVENT_PENDING_RESPONSE);
            s->client_state = S_IDLE;
            ret = REQUEST_CANCEL_RESPONSE;
            break;
        default:
            pr_err("(%s) client not in a good state (%u)\n", __func__,
                s->client_state);
            ret = -EPROTO;
            break;
    }

    if (s->client_state == S_CANCEL_WAITING) {
        up(&s->sem);
        /* Wait for A_CANCEL_ACK or A_RESPONSE */
        ret = wait_session_event(s, EVENT_PENDING_RESPONSE, NULL, timeout);
        if (ret == 0) {
            if (down_interruptible(&s->sem)) {
                pr_err("%s: [2] interrupted while waiting semaphore.\n", __func__);
                return -ERESTARTSYS;
            }
            if (s->client_state == S_NOTIFIED) {
                if (s->client_message.action == A_CANCEL_ACK) {
                    /* We received A_CANCEL_ACK before A_RESPONSE */
                    ret = REQUEST_CANCEL_OK;
                } else {
                    /* A_RESPONSE received before A_CANCEL_ACK */
                    ret = REQUEST_CANCEL_RESPONSE;
                    *response = s->client_message.p1;
                }
                memset(&s->client_message, 0, sizeof(pnc_message_t));
                s->client_state = S_IDLE;
            } else {
                /* Don't know if it can be possible... */
                ret = -EFAULT;
            }
            up(&s->sem);
        }
        goto end_cancel;
    }
    up(&s->sem);

end_cancel:
    return ret;
}
EXPORT_SYMBOL(pnc_session_cancel_request);

int pnc_session_send_request_and_wait_response(pnc_session_t *session,
        uint32_t request, uint32_t timeout, uint32_t *response)
{
    int ret;

    if (response == NULL) {
        pr_err("(%s) no buffer for response reception...\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(session);
    if (ret) {
        return ret;
    }

    do {
        ret = send_request(session, request);
        if (ret != 0) {
            break;
        }

        ret = wait_session_event(session, EVENT_PENDING_RESPONSE, NULL, timeout);
        if (ret != 0) {
            break;
        }

        ret = get_response(session, response);
    } while (0);
    return ret;
}
EXPORT_SYMBOL(pnc_session_send_request_and_wait_response);

int pnc_session_send_signal(pnc_session_t *s, uint32_t bits)
{
    int ret;
    uint32_t signals, ns_notifications;

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    /* Set requested signal bits pending for this session
     * Atomically:
     *  - read _ns_to_s_signals.signals[s->index]
     *  - store new requested bits
     */
    signals = atomic_fetch_or_explicit(&_ns_to_s_signals[s->index], bits,
                memory_order_release);

    /* Check if some bits were already pending */
    if (signals == 0) {
        /* No bits already pending */

        /* Indicate new signal notification is pending for this session
         * Atomically:
         *  - read _ns_to_s_notification_register
         *  - set s->index bit pending
         */
        ns_notifications = atomic_fetch_or_explicit(_ns_to_s_notification_register,
                E_SIGNAL(s->index), memory_order_release);

        /* Check if there is already a pending notification */
        if (ns_notifications == 0) {
            /* There was no pending NS notification */
            notify_s();
        }
    }

    return 0;
}
EXPORT_SYMBOL(pnc_session_send_signal);

int pnc_session_get_signal(pnc_session_t *s, uint32_t *signals)
{
    int ret;

    if (signals == NULL) {
        pr_err("(%s) no signals buffer.\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    if (down_interruptible(&s->sem)) {
        pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return -ERESTARTSYS;
    }

    s->event_pending &= ~EVENT_PENDING_SIGNAL;

    up(&s->sem);

    /* Get and acknowledge received signal(s) if any for this session */
    *signals = atomic_exchange_explicit(&_s_to_ns_signals[s->index], 0,
                memory_order_acquire);

    return 0;
}
EXPORT_SYMBOL(pnc_session_get_signal);

int pnc_session_wait_signal(pnc_session_t *s, uint32_t *signals,
        uint32_t timeout)
{
    int ret;

    if (signals == NULL) {
        pr_err("(%s) no signals buffer.\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    ret = wait_session_event(s, EVENT_PENDING_SIGNAL, NULL, timeout);
    if (ret == 0) {
        /* Get and acknowledge received signal(s) if any for this session */
        *signals = atomic_exchange_explicit(&_s_to_ns_signals[s->index], 0,
                    memory_order_acquire);
    }
    return ret;
}
EXPORT_SYMBOL(pnc_session_wait_signal);

int pnc_session_wait_event(pnc_session_t *s, uint32_t *events, uint32_t mask,
        uint32_t timeout)
{
    int ret;

    if (events == NULL) {
        pr_err("(%s) no signals buffer.\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(s);
    if (ret) {
        return ret;
    }

    return wait_session_event(s, mask, events, timeout);
}
EXPORT_SYMBOL(pnc_session_wait_event);

/* Legacy and obsolete API kept here for backward compatibility */
int pnc_session_request(pnc_session_t *session, uint32_t request,
        uint16_t flags, uint32_t timeout, uint32_t *response)
{
    uint32_t buffer;

    (void)flags;

    if (response == NULL) {
        response = &buffer;
    }
    return pnc_session_send_request_and_wait_response(session, request, timeout,
        response);
}
EXPORT_SYMBOL(pnc_session_request);

__poll_t pnc_session_poll_wait(pnc_session_t *session, struct file *file,
                               poll_table *wait)
{
    if (wait == NULL || file == NULL || session == NULL)
        return EPOLLERR;

    if (check_session_configured(session) != 0)
        return EPOLLERR;

    if (session->event_pending != 0)
        return (EPOLLIN | EPOLLRDNORM);

    poll_wait(file, &session->event_wait, wait);

    /* Need to check if session broken or terminated while waiting response */
    int ret = check_and_handle_terminated_session(session);
    if (ret == -EPIPE)
        return EPOLLHUP;

    if (ret != 0) {
        if (ret == -ERESTARTSYS)
            pr_err("%s: interrupted while waiting semaphore.\n", __func__);
        return EPOLLERR;
    }

    return session->event_pending == 0 ? 0 : (EPOLLIN | EPOLLRDNORM);
}
EXPORT_SYMBOL(pnc_session_poll_wait);

int pnc_session_get_pending_events(pnc_session_t *session, uint32_t *events)
{
    int ret;

    if (events == NULL) {
        pr_err("(%s) no signals buffer.\n", __func__);
        return -EBADF;
    }

    ret = check_session_configured(session);
    if (ret) {
        return ret;
    }

    *events = session->event_pending;

    return 0;
}
EXPORT_SYMBOL(pnc_session_get_pending_events);
