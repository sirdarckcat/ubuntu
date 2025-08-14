/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *   Copyright (c) 2025 Stefan Metzmacher
 */

#ifndef __FS_SMB_COMMON_SMBDIRECT_SMBDIRECT_SOCKET_H__
#define __FS_SMB_COMMON_SMBDIRECT_SMBDIRECT_SOCKET_H__

enum smbdirect_socket_status {
	SMBDIRECT_SOCKET_CREATED,
	SMBDIRECT_SOCKET_CONNECTING,
	SMBDIRECT_SOCKET_CONNECTED,
	SMBDIRECT_SOCKET_NEGOTIATE_FAILED,
	SMBDIRECT_SOCKET_DISCONNECTING,
	SMBDIRECT_SOCKET_DISCONNECTED,
	SMBDIRECT_SOCKET_DESTROYED
};

struct smbdirect_socket {
	enum smbdirect_socket_status status;

	/* RDMA related */
	struct {
		struct rdma_cm_id *cm_id;
	} rdma;

	/* IB verbs related */
	struct {
		struct ib_pd *pd;
		struct ib_cq *send_cq;
		struct ib_cq *recv_cq;

		/*
		 * shortcuts for rdma.cm_id->{qp,device};
		 */
		struct ib_qp *qp;
		struct ib_device *dev;
	} ib;

	struct smbdirect_socket_parameters parameters;

	/*
	 * The state for posted receive buffers
	 */
	struct {
		/*
		 * The type of PDU we are expecting
		 */
		enum {
			SMBDIRECT_EXPECT_NEGOTIATE_REQ = 1,
			SMBDIRECT_EXPECT_NEGOTIATE_REP = 2,
			SMBDIRECT_EXPECT_DATA_TRANSFER = 3,
		} expected;
	} recv_io;

	/*
	 * The state for RDMA read/write requests on the server
	 */
	struct {
		/*
		 * The credit state for the send side
		 */
		struct {
			/*
			 * The maximum number of rw credits
			 */
			size_t max;
			/*
			 * The number of pages per credit
			 */
			size_t num_pages;
			atomic_t count;
			wait_queue_head_t wait_queue;
		} credits;
	} rw_io;
};

static __always_inline void smbdirect_socket_init(struct smbdirect_socket *sc)
{
	/*
	 * This also sets status = SMBDIRECT_SOCKET_CREATED
	 */
	BUILD_BUG_ON(SMBDIRECT_SOCKET_CREATED != 0);
	memset(sc, 0, sizeof(*sc));

	init_waitqueue_head(&sc->status_wait);

	INIT_LIST_HEAD(&sc->recv_io.free.list);
	spin_lock_init(&sc->recv_io.free.lock);

	INIT_LIST_HEAD(&sc->recv_io.reassembly.list);
	spin_lock_init(&sc->recv_io.reassembly.lock);
	init_waitqueue_head(&sc->recv_io.reassembly.wait_queue);

	atomic_set(&sc->rw_io.credits.count, 0);
	init_waitqueue_head(&sc->rw_io.credits.wait_queue);
}

struct smbdirect_send_io {
	struct smbdirect_socket *socket;
	struct ib_cqe cqe;

	/*
	 * The SGE entries for this work request
	 *
	 * The first points to the packet header
	 */
#define SMBDIRECT_SEND_IO_MAX_SGE 6
	size_t num_sge;
	struct ib_sge sge[SMBDIRECT_SEND_IO_MAX_SGE];

	/*
	 * Link to the list of sibling smbdirect_send_io
	 * messages.
	 */
	struct list_head sibling_list;
	struct ib_send_wr wr;

	/* SMBD packet header follows this structure */
	u8 packet[];
};

struct smbdirect_recv_io {
	struct smbdirect_socket *socket;
	struct ib_cqe cqe;
	struct ib_sge sge;

	/* Link to free or reassembly list */
	struct list_head list;

	/* Indicate if this is the 1st packet of a payload */
	bool first_segment;

	/* SMBD packet header and payload follows this structure */
	u8 packet[];
};

#endif /* __FS_SMB_COMMON_SMBDIRECT_SMBDIRECT_SOCKET_H__ */
