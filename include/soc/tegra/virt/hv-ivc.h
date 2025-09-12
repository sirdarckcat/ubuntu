/*
 * SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#ifndef __TEGRA_HV_IVC_H
#define __TEGRA_HV_IVC_H

#include <linux/of.h>
#include <linux/version.h>

/**
 * @defgroup hypervisor_ivc_framework Hypervisor IVC Framework
 * @{
 */

/** @brief structure representing ivc queue cookie in hypervisor driver */
struct tegra_hv_ivc_cookie {
	/** @brief irq linked to the ivc queue */
	int irq;
	/** @brief vmid of the peer */
	int peer_vmid;
	/** @brief number of frames in an ivc queue */
	int nframes;
	/** @brief ivc frame size*/
	int frame_size;
	/** @brief address used to notify end-point */
	uint32_t *notify_va;
};

/** @brief represents the ivc queue operations */
struct tegra_hv_ivc_ops {
	/** @brief called when data are received */
	void (*rx_rdy)(struct tegra_hv_ivc_cookie *ivck);
	/** @brief called when space is available to write data */
	void (*tx_rdy)(struct tegra_hv_ivc_cookie *ivck);
};

/** @brief structure representing mempool cookie in hypervisor driver */
struct tegra_hv_ivm_cookie {
	/** @brief mempool base ipa address */
	uint64_t ipa;
	/** @brief mempool size */
	uint64_t size;
	/** @brief vmid of the peer */
	unsigned int peer_vmid;
	/** @brief reserved */
	void *reserved;
};

/**
 * @brief          checks whether platform supports virtualization or not
 *
 * @retval         true if platform supports virtualization else false.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *
 * @post           Client can take runtime decision in driver if client
 *                 driver support native and virtualized environment.
 *
 * @usage          Call this function to check if OS is running in native environment
 *                 or in virtualized environment.
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
bool is_tegra_hypervisor_mode(void);

/**
 * @brief          Reserve an IVC queue for use
 * @param[in]      dn Device node pointer to the queue in the DT
 *		   If NULL, then operate on first HV device
 * @param[in]      id Id number of the queue to use.
 * @param[in]      ops Ops structure or NULL (deprecated)
 *
 * @retval         ptr ivc queue cookie pointer or else errors.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *
 * @post           After reserving IVC queue client can perform
 *                 I/O operation on the IVC queue.
 *
 * @usage          reserve ivc queue before performing I/O operations.
 *                 driver should have been probed successfully.
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
struct tegra_hv_ivc_cookie *tegra_hv_ivc_reserve(
		struct device_node *dn, int id,
		const struct tegra_hv_ivc_ops *ops);

/**
 * @brief          Unreserve an IVC queue used
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         0 on success and an error code otherwise
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *
 * @post           After unreserving IVC queue client can not perform
 *                 I/O operation on the IVC queue.
 *
 * @usage          unreserve ivc queue after performing I/O operations.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_unreserve(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          Write a number of bytes (as a single frame) from the queue.
 * @param[in]      ivck IVC cookie of the queue
 * @param[in]      buf Pointer to the data to write
 * @param[in]      size Size of the data to write
 *
 * @retval         size on success and an error code otherwise
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           data will be written on the ivc queue successfully.
 *
 * @usage          use this API to send data to peer end.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_write(struct tegra_hv_ivc_cookie *ivck, const void *buf,
		int size);

/**
 * @brief          Write a number of bytes (as a single frame) from the queue.
 * @param[in]      ivck IVC cookie of the queue
 * @param[in]      buf Pointer to the userspace data to write
 * @param[in]      size Size of the data to write
 *
 * @retval         size Returns size on success and an error code otherwise
 *
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           data will be written on the ivc queue successfully from userspace buffer.
 *
 * @usage          use this API to send data to peer end.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_write_user(struct tegra_hv_ivc_cookie *ivck, const void __user *buf,
		int size);

/**
 * @brief          Reads a number of bytes (as a single frame) from the queue.
 * @param[in]      ivck IVC cookie of the queue
 * @param[in,out]  buf Pointer to the data to read
 * @param[in]      size max size of the data to read
 *
 * @retval         size Returns size on success and an error code otherwise
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           data will be read from the ivc queue successfully.
 *
 * @usage          use this API to receive data from peer end.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_read(struct tegra_hv_ivc_cookie *ivck, void *buf, int size);

/**
 * @brief          Reads a number of bytes (as a single frame) from the queue.
 * @param[in]      ivck IVC cookie of the queue
 * @param[in,out]  buf Pointer to the userspace data to read
 * @param[in]      size max size of the data to read
 *
 * @retval         size Returns size on success and an error code otherwise
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           data will be read from the ivc queue successfully into userspace buffer.
 *
 * @usage          use this API to receive data from peer end.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_read_user(struct tegra_hv_ivc_cookie *ivck, void __user *buf, int size);

/**
 * @brief          Test whether data are available
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         1 if data are available in the rx queue, 0 if not
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           client take decision to read data if data is present
 *
 * @usage          use this API if client want to check if response has been received peer end.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_can_read(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          Test whether data can be written
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         1 if data are can be written to the tx queue, 0 if not
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           client take decision to write data if write slot is free.
 *
 * @usage          use this API if client want to check if write slot is present or not.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_can_write(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          Test whether the tx queue is empty
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         1 if the queue is empty, zero otherwise
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           client will know whether tx queue is empty or not.
 *
 * @usage          use this API if tx queue is empty or not.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_tx_empty(struct tegra_hv_ivc_cookie *ivck);

#ifndef CONFIG_KERNEL_BUILD_WITH_PROD_DEFCONFIG
/*
 * @brief          gets number of free entries in tx queue
 *                 Returns the number of unused entries in the tx queue. Assuming the caller
 *                 does not write any additional frames, this number may increase from the
 *                 value returned as the receiver consumes frames.
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         num Number ot frames available.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
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
uint32_t tegra_hv_ivc_tx_frames_available(struct tegra_hv_ivc_cookie *ivck);

/*
 * @brief          Dump ivc info in dmesg logs
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         0 on success, a negative error code otherwise
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
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
int tegra_hv_ivc_dump(struct tegra_hv_ivc_cookie *ivck);
#endif

/**
 * @brief          Peek at the next frame to receive
 *                 Peek at the next frame to be received, without removing it from
 *                 the queue.
 *
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         ptr Returns a pointer to the frame, or an error encoded pointer.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           after peeking next frame we can check the content of frames.
 *
 * @usage          if client want to check next frame without removing it from queue.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
void *tegra_hv_ivc_read_get_next_frame(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          Advance the read queue
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         0, or a negative error value if failed.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           internal ivc counters point to next read frame.
 *
 * @usage          use this API if client wants to advance to next read frame
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_read_advance(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          Poke at the next frame to transmit
 *                 Get access to the next frame.
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         ptr Returns a pointer to the frame, or an error encoded pointer.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           client can write data to frame after getting it without advacing tx frame counter.
 *
 * @usage          if client want to send data to peer end without removing it from queue.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
void *tegra_hv_ivc_write_get_next_frame(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          Advance the write queue
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         0 on success or a negative error value if failed.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           internal ivc counters point to next write frame.
 *
 * @usage          use this API if client wants to advance to next write frame
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_write_advance(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          reserve a mempool for use
 * @param[in]      id Id of the requested mempool.
 *
 * @retval         ivck Returns a cookie representing the mempool on success, otherwise an ERR_PTR.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *
 * @post           reserved mempool will be available for I/O operations.
 *
 * @usage          mempool will be available for data transfer with peer end.
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
struct tegra_hv_ivm_cookie *tegra_hv_mempool_reserve(unsigned int id);

/**
 * @brief          release a reserved mempool
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         ret 0 on success or a negative error code otherwise.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *
 * @post           unreserved mempool will not be available for I/O operations.
 *
 * @usage          mempool will not be available for data transfer available for data transfer.
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
int tegra_hv_mempool_unreserve(struct tegra_hv_ivm_cookie *ivck);

/**
 * @brief          handle internal messages
 *                 This function must be called following every notification (interrupt or
 *                 callback invocation) for the tegra_hv_- version).
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         0 if the channel is ready for communication, or -EAGAIN if a channel
 *                 reset is in progress.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           IVC channel will be establised with peer end and will be ready for communication
 *
 * @usage          use this APi to establish connection with peer end for communication
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_channel_notified(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          initiates a reset of the shared memory state
 *                 This function must be called after a channel is reserved before it is used
 *                 for communication. The channel will be ready for use when a subsequent call
 *                 to ivc_channel_notified() returns 0.
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         None
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           channel will be in reseted state.
 *
 * @usage          before establishing connection reset ivc channel first after reserving ivc queue.
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
void tegra_hv_ivc_channel_reset(struct tegra_hv_ivc_cookie *ivck);

/**
 * @brief          Get info (IPA and size) of Guest shared area
 * @param[in]      ivck IVC cookie of the queue
 * @param[out]     pa IPA of shared area
 * @param[out]     size Size of the shared area
 *
 * @retval         0 on success & -EINVAL on failure.
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           IPA address and size of ivc queue will be available to client.
 *
 * @usage          when client want to mmap ivc memory to userspace via mmap API.
 *                 driver should have been probed successfully.
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
int tegra_hv_ivc_get_info(struct tegra_hv_ivc_cookie *ivck, uint64_t *pa,
			  uint64_t *size);

/**
 * @brief          Notify remote guest
 * @param[in]      ivck IVC cookie of the queue
 *
 * @retval         None
 *
 * @pre            Tegra hypervisor driver should have been initialized.
 *                 This API should be invoked on virtual/hypervisor environment only.
 *                 IVC should have been reserved before performing any ivc operations.
 *
 * @post           remote guest will be notified
 *
 * @usage          use when want to notify the peer end after writing or reading data to ivc queue
 *                 ivc should have been reserved before using this API.
 *                 never use this API after unreserving ivc queue.
 *                 driver should have been probed successfully.
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
void tegra_hv_ivc_notify(struct tegra_hv_ivc_cookie *ivck);

/** @} */

#endif /* __TEGRA_HV_IVC_H */
