/*
 * Copyright 2023 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include <rm/rpc.h>

#include <nvrm/nvtypes.h>
#include <nvrm/535.113.01/nvidia/kernel/inc/vgpu/rpc_global_enums.h>

#define GSP_MSG_MIN_SIZE GSP_PAGE_SIZE
#define GSP_MSG_MAX_SIZE (GSP_MSG_MIN_SIZE * 16)

struct r535_gsp_msg {
	u8 auth_tag_buffer[16];
	u8 aad_buffer[16];
	u32 checksum;
	u32 sequence;
	u32 elem_count;
	u32 pad;
	u8  data[];
};

struct nvfw_gsp_rpc {
	u32 header_version;
	u32 signature;
	u32 length;
	u32 function;
	u32 rpc_result;
	u32 rpc_result_private;
	u32 sequence;
	union {
		u32 spare;
		u32 cpuRmGfid;
	};
	u8  data[];
};

#define GSP_MSG_HDR_SIZE offsetof(struct r535_gsp_msg, data)

int
r535_rpc_status_to_errno(uint32_t rpc_status)
{
	switch (rpc_status) {
	case 0x55: /* NV_ERR_NOT_READY */
	case 0x66: /* NV_ERR_TIMEOUT_RETRY */
		return -EBUSY;
	case 0x51: /* NV_ERR_NO_MEMORY */
		return -ENOMEM;
	default:
		return -EINVAL;
	}
}

static int
r535_gsp_msgq_wait(struct nvkm_gsp *gsp, u32 gsp_rpc_len, int *ptime)
{
	u32 size, rptr = *gsp->msgq.rptr;
	int used;

	size = DIV_ROUND_UP(GSP_MSG_HDR_SIZE + gsp_rpc_len,
			    GSP_PAGE_SIZE);
	if (WARN_ON(!size || size >= gsp->msgq.cnt))
		return -EINVAL;

	do {
		u32 wptr = *gsp->msgq.wptr;

		used = wptr + gsp->msgq.cnt - rptr;
		if (used >= gsp->msgq.cnt)
			used -= gsp->msgq.cnt;
		if (used >= size)
			break;

		usleep_range(1, 2);
	} while (--(*ptime));

	if (WARN_ON(!*ptime))
		return -ETIMEDOUT;

	return used;
}

static struct r535_gsp_msg *
r535_gsp_msgq_get_entry(struct nvkm_gsp *gsp)
{
	u32 rptr = *gsp->msgq.rptr;

	/* Skip the first page, which is the message queue info */
	return (void *)((u8 *)gsp->shm.msgq.ptr + GSP_PAGE_SIZE +
	       rptr * GSP_PAGE_SIZE);
}

/**
 * DOC: Receive a GSP message queue element
 *
 * Receiving a GSP message queue element from the message queue consists of
 * the following steps:
 *
 * - Peek the element from the queue: r535_gsp_msgq_peek().
 *   Peek the first page of the element to determine the total size of the
 *   message before allocating the proper memory.
 *
 * - Allocate memory for the message.
 *   Once the total size of the message is determined from the GSP message
 *   queue element, the caller of r535_gsp_msgq_recv() allocates the
 *   required memory.
 *
 * - Receive the message: r535_gsp_msgq_recv().
 *   Copy the message into the allocated memory. Advance the read pointer.
 *   If the message is a large GSP message, r535_gsp_msgq_recv() calls
 *   r535_gsp_msgq_recv_one_elem() repeatedly to receive continuation parts
 *   until the complete message is received.
 *   r535_gsp_msgq_recv() assembles the payloads of cotinuation parts into
 *   the return of the large GSP message.
 *
 * - Free the allocated memory: r535_gsp_msg_done().
 *   The user is responsible for freeing the memory allocated for the GSP
 *   message pages after they have been processed.
 */
static void *
r535_gsp_msgq_peek(struct nvkm_gsp *gsp, u32 gsp_rpc_len, int *retries)
{
	struct r535_gsp_msg *mqe;
	int ret;

	ret = r535_gsp_msgq_wait(gsp, gsp_rpc_len, retries);
	if (ret < 0)
		return ERR_PTR(ret);

	mqe = r535_gsp_msgq_get_entry(gsp);

	return mqe->data;
}

struct r535_gsp_msg_info {
	int *retries;
	u32 gsp_rpc_len;
	void *gsp_rpc_buf;
	bool continuation;
};

static void
r535_gsp_msg_dump(struct nvkm_gsp *gsp, struct nvfw_gsp_rpc *msg, int lvl);

static void *
r535_gsp_msgq_recv_one_elem(struct nvkm_gsp *gsp,
			    struct r535_gsp_msg_info *info)
{
	u8 *buf = info->gsp_rpc_buf;
	u32 rptr = *gsp->msgq.rptr;
	struct r535_gsp_msg *mqe;
	u32 size, expected, len;
	int ret;

	expected = info->gsp_rpc_len;

	ret = r535_gsp_msgq_wait(gsp, expected, info->retries);
	if (ret < 0)
		return ERR_PTR(ret);

	mqe = r535_gsp_msgq_get_entry(gsp);

	if (info->continuation) {
		struct nvfw_gsp_rpc *rpc = (struct nvfw_gsp_rpc *)mqe->data;

		if (rpc->function != NV_VGPU_MSG_FUNCTION_CONTINUATION_RECORD) {
			nvkm_error(&gsp->subdev,
				   "Not a continuation of a large RPC\n");
			r535_gsp_msg_dump(gsp, rpc, NV_DBG_ERROR);
			return ERR_PTR(-EIO);
		}
	}

	size = ALIGN(expected + GSP_MSG_HDR_SIZE, GSP_PAGE_SIZE);

	len = ((gsp->msgq.cnt - rptr) * GSP_PAGE_SIZE) - sizeof(*mqe);
	len = min_t(u32, expected, len);

	if (info->continuation)
		memcpy(buf, mqe->data + sizeof(struct nvfw_gsp_rpc),
		       len - sizeof(struct nvfw_gsp_rpc));
	else
		memcpy(buf, mqe->data, len);

	expected -= len;

	if (expected) {
		mqe = (void *)((u8 *)gsp->shm.msgq.ptr + 0x1000 + 0 * 0x1000);
		memcpy(buf + len, mqe, expected);
	}

	rptr = (rptr + DIV_ROUND_UP(size, GSP_PAGE_SIZE)) % gsp->msgq.cnt;

	mb();
	(*gsp->msgq.rptr) = rptr;
	return buf;
}

static void *
r535_gsp_msgq_recv(struct nvkm_gsp *gsp, u32 gsp_rpc_len, int *retries)
{
	struct r535_gsp_msg *mqe;
	const u32 max_rpc_size = GSP_MSG_MAX_SIZE - sizeof(*mqe);
	struct nvfw_gsp_rpc *rpc;
	struct r535_gsp_msg_info info = {0};
	u32 expected = gsp_rpc_len;
	void *buf;

	mqe = r535_gsp_msgq_get_entry(gsp);
	rpc = (struct nvfw_gsp_rpc *)mqe->data;

	if (WARN_ON(rpc->length > max_rpc_size))
		return NULL;

	buf = kvmalloc(max_t(u32, rpc->length, expected), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	info.gsp_rpc_buf = buf;
	info.retries = retries;
	info.gsp_rpc_len = rpc->length;

	buf = r535_gsp_msgq_recv_one_elem(gsp, &info);
	if (IS_ERR(buf)) {
		kvfree(info.gsp_rpc_buf);
		info.gsp_rpc_buf = NULL;
		return buf;
	}

	if (expected <= max_rpc_size)
		return buf;

	info.gsp_rpc_buf += info.gsp_rpc_len;
	expected -= info.gsp_rpc_len;

	while (expected) {
		u32 size;

		rpc = r535_gsp_msgq_peek(gsp, sizeof(*rpc), info.retries);
		if (IS_ERR_OR_NULL(rpc)) {
			kvfree(buf);
			return rpc;
		}

		info.gsp_rpc_len = rpc->length;
		info.continuation = true;

		rpc = r535_gsp_msgq_recv_one_elem(gsp, &info);
		if (IS_ERR_OR_NULL(rpc)) {
			kvfree(buf);
			return rpc;
		}

		size = info.gsp_rpc_len - sizeof(*rpc);
		expected -= size;
		info.gsp_rpc_buf += size;
	}

	rpc = buf;
	rpc->length = gsp_rpc_len;
	return buf;
}

static int
r535_gsp_cmdq_push(struct nvkm_gsp *gsp, void *argv)
{
	struct r535_gsp_msg *cmd = container_of(argv, typeof(*cmd), data);
	struct r535_gsp_msg *cqe;
	u32 argc = cmd->checksum;
	u64 *ptr = (void *)cmd;
	u64 *end;
	u64 csum = 0;
	int free, time = 1000000;
	u32 wptr, size, step;
	u32 off = 0;

	argc = ALIGN(GSP_MSG_HDR_SIZE + argc, GSP_PAGE_SIZE);

	end = (u64 *)((char *)ptr + argc);
	cmd->pad = 0;
	cmd->checksum = 0;
	cmd->sequence = gsp->cmdq.seq++;
	cmd->elem_count = DIV_ROUND_UP(argc, 0x1000);

	while (ptr < end)
		csum ^= *ptr++;

	cmd->checksum = upper_32_bits(csum) ^ lower_32_bits(csum);

	wptr = *gsp->cmdq.wptr;
	do {
		do {
			free = *gsp->cmdq.rptr + gsp->cmdq.cnt - wptr - 1;
			if (free >= gsp->cmdq.cnt)
				free -= gsp->cmdq.cnt;
			if (free >= 1)
				break;

			usleep_range(1, 2);
		} while(--time);

		if (WARN_ON(!time)) {
			kvfree(cmd);
			return -ETIMEDOUT;
		}

		cqe = (void *)((u8 *)gsp->shm.cmdq.ptr + 0x1000 + wptr * 0x1000);
		step = min_t(u32, free, (gsp->cmdq.cnt - wptr));
		size = min_t(u32, argc, step * GSP_PAGE_SIZE);

		memcpy(cqe, (u8 *)cmd + off, size);

		wptr += DIV_ROUND_UP(size, 0x1000);
		if (wptr == gsp->cmdq.cnt)
			wptr = 0;

		off  += size;
		argc -= size;
	} while(argc);

	nvkm_trace(&gsp->subdev, "cmdq: wptr %d\n", wptr);
	wmb();
	(*gsp->cmdq.wptr) = wptr;
	mb();

	nvkm_falcon_wr32(&gsp->falcon, 0xc00, 0x00000000);

	kvfree(cmd);
	return 0;
}

static void *
r535_gsp_cmdq_get(struct nvkm_gsp *gsp, u32 argc)
{
	struct r535_gsp_msg *cmd;
	u32 size = GSP_MSG_HDR_SIZE + argc;

	size = ALIGN(size, GSP_MSG_MIN_SIZE);
	cmd = kvzalloc(size, GFP_KERNEL);
	if (!cmd)
		return ERR_PTR(-ENOMEM);

	cmd->checksum = argc;
	return cmd->data;
}

static void
r535_gsp_msg_done(struct nvkm_gsp *gsp, struct nvfw_gsp_rpc *msg)
{
	kvfree(msg);
}

static void
r535_gsp_msg_dump(struct nvkm_gsp *gsp, struct nvfw_gsp_rpc *msg, int lvl)
{
	if (gsp->subdev.debug >= lvl) {
		nvkm_printk__(&gsp->subdev, lvl, info,
			      "msg fn:%d len:0x%x/0x%zx res:0x%x resp:0x%x\n",
			      msg->function, msg->length, msg->length - sizeof(*msg),
			      msg->rpc_result, msg->rpc_result_private);
		print_hex_dump(KERN_INFO, "msg: ", DUMP_PREFIX_OFFSET, 16, 1,
			       msg->data, msg->length - sizeof(*msg), true);
	}
}

struct nvfw_gsp_rpc *
r535_gsp_msg_recv(struct nvkm_gsp *gsp, int fn, u32 repc)
{
	struct nvkm_subdev *subdev = &gsp->subdev;
	struct nvfw_gsp_rpc *msg;
	int retries = 4000000, i;

retry:
	msg = r535_gsp_msgq_peek(gsp, sizeof(*msg), &retries);
	if (IS_ERR_OR_NULL(msg))
		return msg;

	msg = r535_gsp_msgq_recv(gsp, repc, &retries);
	if (IS_ERR_OR_NULL(msg))
		return msg;

	if (msg->rpc_result) {
		r535_gsp_msg_dump(gsp, msg, NV_DBG_ERROR);
		r535_gsp_msg_done(gsp, msg);
		return ERR_PTR(-EINVAL);
	}

	r535_gsp_msg_dump(gsp, msg, NV_DBG_TRACE);

	if (fn && msg->function == fn) {
		if (repc) {
			if (msg->length < repc) {
				nvkm_error(subdev, "msg len %d < %d\n",
					   msg->length, repc);
				r535_gsp_msg_dump(gsp, msg, NV_DBG_ERROR);
				r535_gsp_msg_done(gsp, msg);
				return ERR_PTR(-EIO);
			}

			return msg;
		}

		r535_gsp_msg_done(gsp, msg);
		return NULL;
	}

	for (i = 0; i < gsp->msgq.ntfy_nr; i++) {
		struct nvkm_gsp_msgq_ntfy *ntfy = &gsp->msgq.ntfy[i];

		if (ntfy->fn == msg->function) {
			if (ntfy->func)
				ntfy->func(ntfy->priv, ntfy->fn, msg->data, msg->length - sizeof(*msg));
			break;
		}
	}

	if (i == gsp->msgq.ntfy_nr)
		r535_gsp_msg_dump(gsp, msg, NV_DBG_WARN);

	r535_gsp_msg_done(gsp, msg);
	if (fn)
		goto retry;

	if (*gsp->msgq.rptr != *gsp->msgq.wptr)
		goto retry;

	return NULL;
}

int
r535_gsp_msg_ntfy_add(struct nvkm_gsp *gsp, u32 fn, nvkm_gsp_msg_ntfy_func func, void *priv)
{
	int ret = 0;

	mutex_lock(&gsp->msgq.mutex);
	if (WARN_ON(gsp->msgq.ntfy_nr >= ARRAY_SIZE(gsp->msgq.ntfy))) {
		ret = -ENOSPC;
	} else {
		gsp->msgq.ntfy[gsp->msgq.ntfy_nr].fn = fn;
		gsp->msgq.ntfy[gsp->msgq.ntfy_nr].func = func;
		gsp->msgq.ntfy[gsp->msgq.ntfy_nr].priv = priv;
		gsp->msgq.ntfy_nr++;
	}
	mutex_unlock(&gsp->msgq.mutex);
	return ret;
}

int
r535_gsp_rpc_poll(struct nvkm_gsp *gsp, u32 fn)
{
	void *repv;

	mutex_lock(&gsp->cmdq.mutex);
	repv = r535_gsp_msg_recv(gsp, fn, 0);
	mutex_unlock(&gsp->cmdq.mutex);
	if (IS_ERR(repv))
		return PTR_ERR(repv);

	return 0;
}

static void *
r535_gsp_rpc_handle_reply(struct nvkm_gsp *gsp, u32 fn,
			  enum nvkm_gsp_rpc_reply_policy policy,
			  u32 gsp_rpc_len)
{
	struct nvfw_gsp_rpc *reply;
	void *repv = NULL;

	switch (policy) {
	case NVKM_GSP_RPC_REPLY_NOWAIT:
		break;
	case NVKM_GSP_RPC_REPLY_RECV:
		reply = r535_gsp_msg_recv(gsp, fn, gsp_rpc_len);
		if (!IS_ERR_OR_NULL(reply))
			repv = reply->data;
		else
			repv = reply;
		break;
	case NVKM_GSP_RPC_REPLY_POLL:
		repv = r535_gsp_msg_recv(gsp, fn, 0);
		break;
	}

	return repv;
}

static void *
r535_gsp_rpc_send(struct nvkm_gsp *gsp, void *payload,
		  enum nvkm_gsp_rpc_reply_policy policy, u32 gsp_rpc_len)
{
	struct nvfw_gsp_rpc *rpc = container_of(payload, typeof(*rpc), data);
	u32 fn = rpc->function;
	int ret;

	if (gsp->subdev.debug >= NV_DBG_TRACE) {
		nvkm_trace(&gsp->subdev, "rpc fn:%d len:0x%x/0x%zx\n", rpc->function,
			   rpc->length, rpc->length - sizeof(*rpc));
		print_hex_dump(KERN_INFO, "rpc: ", DUMP_PREFIX_OFFSET, 16, 1,
			       rpc->data, rpc->length - sizeof(*rpc), true);
	}

	ret = r535_gsp_cmdq_push(gsp, rpc);
	if (ret)
		return ERR_PTR(ret);

	return r535_gsp_rpc_handle_reply(gsp, fn, policy, gsp_rpc_len);
}

static void
r535_gsp_rpc_done(struct nvkm_gsp *gsp, void *repv)
{
	struct nvfw_gsp_rpc *rpc = container_of(repv, typeof(*rpc), data);

	r535_gsp_msg_done(gsp, rpc);
}

static void *
r535_gsp_rpc_get(struct nvkm_gsp *gsp, u32 fn, u32 argc)
{
	struct nvfw_gsp_rpc *rpc;

	rpc = r535_gsp_cmdq_get(gsp, ALIGN(sizeof(*rpc) + argc, sizeof(u64)));
	if (IS_ERR(rpc))
		return ERR_CAST(rpc);

	rpc->header_version = 0x03000000;
	rpc->signature = ('C' << 24) | ('P' << 16) | ('R' << 8) | 'V';
	rpc->function = fn;
	rpc->rpc_result = 0xffffffff;
	rpc->rpc_result_private = 0xffffffff;
	rpc->length = sizeof(*rpc) + argc;
	return rpc->data;
}

static void *
r535_gsp_rpc_push(struct nvkm_gsp *gsp, void *payload,
		  enum nvkm_gsp_rpc_reply_policy policy, u32 gsp_rpc_len)
{
	struct nvfw_gsp_rpc *rpc = container_of(payload, typeof(*rpc), data);
	struct r535_gsp_msg *msg = container_of((void *)rpc, typeof(*msg), data);
	const u32 max_msg_size = (16 * 0x1000) - sizeof(struct r535_gsp_msg);
	const u32 max_payload_size = max_msg_size - sizeof(*rpc);
	u32 payload_size = rpc->length - sizeof(*rpc);
	void *repv;

	mutex_lock(&gsp->cmdq.mutex);
	if (payload_size > max_payload_size) {
		const u32 fn = rpc->function;
		u32 remain_payload_size = payload_size;
		void *next;

		/* Send initial RPC. */
		next = r535_gsp_rpc_get(gsp, fn, max_payload_size);
		if (IS_ERR(next)) {
			repv = next;
			goto done;
		}

		memcpy(next, payload, max_payload_size);

		repv = r535_gsp_rpc_send(gsp, next, NVKM_GSP_RPC_REPLY_NOWAIT, 0);
		if (IS_ERR(repv))
			goto done;

		payload += max_payload_size;
		remain_payload_size -= max_payload_size;

		/* Remaining chunks sent as CONTINUATION_RECORD RPCs. */
		while (remain_payload_size) {
			u32 size = min(remain_payload_size,
				       max_payload_size);

			next = r535_gsp_rpc_get(gsp, NV_VGPU_MSG_FUNCTION_CONTINUATION_RECORD, size);
			if (IS_ERR(next)) {
				repv = next;
				goto done;
			}

			memcpy(next, payload, size);

			repv = r535_gsp_rpc_send(gsp, next, NVKM_GSP_RPC_REPLY_NOWAIT, 0);
			if (IS_ERR(repv))
				goto done;

			payload += size;
			remain_payload_size -= size;
		}

		/* Wait for reply. */
		repv = r535_gsp_rpc_handle_reply(gsp, fn, policy, payload_size +
						 sizeof(*rpc));
		if (!IS_ERR(repv))
			kvfree(msg);
	} else {
		repv = r535_gsp_rpc_send(gsp, payload, policy, gsp_rpc_len);
	}

done:
	mutex_unlock(&gsp->cmdq.mutex);
	return repv;
}

const struct nvkm_rm_api_rpc
r535_rpc = {
	.get = r535_gsp_rpc_get,
	.push = r535_gsp_rpc_push,
	.done = r535_gsp_rpc_done,
};
