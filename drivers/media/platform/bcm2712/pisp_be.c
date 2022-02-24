// SPDX-License-Identifier: GPL-2.0
/*
 * PiSP Back End driver.
 * Copyright (c) 2021 Raspberry Pi Trading Ltd.
 *
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>

#include "pisp_be_config.h"
#include "pisp_be_formats.h"

MODULE_DESCRIPTION("PiSP Back End driver");
MODULE_AUTHOR("Someone");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.1");

static unsigned int debug = 2;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "activates debug info");

/* Offset to use when registering the /dev/videoX node */
#define PISPBE_VIDEO_NODE_OFFSET 20

/*
 * The number of groups of these nodes, each group making up a potential client
 * of the PiSP. Each client of the PiSP has the above numbers of output and
 * capture nodes.
 */
#define PISPBE_NUM_NODE_GROUPS 1

/* You can support USERPTR I/O mode or DMABUF, but not both. */
#define SUPPORT_IO_USERPTR 0

#define PISPBE_NAME "pispbe"
#define PISPBE_QUEUE_MEM (80 * 1024 * 1024)
#define PISPBE_ENTITY_NAME_LEN 32

/* Some ISP-BE registers */
#define PISP_BE_VERSION_OFFSET (0x0)
#define PISP_BE_CONTROL_OFFSET (0x4)
#define PISP_BE_TILE_ADDR_LO_OFFSET (0x8)
#define PISP_BE_TILE_ADDR_HI_OFFSET (0xc)
#define PISP_BE_STATUS_OFFSET (0x10)
#define PISP_BE_BATCH_STATUS_OFFSET (0x14)
#define PISP_BE_INTERRUPT_EN_OFFSET (0x18)
#define PISP_BE_INTERRUPT_STATUS_OFFSET (0x1c)
#define PISP_BE_AXI_OFFSET (0x20)
#define PISP_BE_CONFIG_BASE_OFFSET (0x40)
#define PISP_BE_IO_INPUT_ADDR0_LO_OFFSET (PISP_BE_CONFIG_BASE_OFFSET)
#define PISP_BE_GLOBAL_BAYER_ENABLE_OFFSET (PISP_BE_CONFIG_BASE_OFFSET + 0x70)
#define PISP_BE_GLOBAL_RGB_ENABLE_OFFSET (PISP_BE_CONFIG_BASE_OFFSET + 0x74)
#define N_HW_ADDRESSES 14
#define N_HW_ENABLES 2

/*
 * This maps our nodes onto the inputs/outputs of the actual PiSP Back End.
 * Be wary of the word "OUTPUT" which is used ambiguously here. In a V4L2
 * context it means an input to the hardware (source image or metadata).
 * Elsewhere it means an output from the hardware.
 */
enum node_ids {
	MAIN_INPUT_NODE,
	HOG_OUTPUT_NODE,
	OUTPUT0_NODE,
	OUTPUT1_NODE,
	TDN_OUTPUT_NODE,
	STITCH_OUTPUT_NODE,
	CONFIG_NODE,
	PISPBE_NUM_NODES
};

enum recurrent_inputs {
	RECURRENT_TDN_INPUT,
	RECURRENT_STITCH_INPUT,
	PISPBE_NUM_RECURRENT_INPUTS
};

struct node_description {
	const char *name;
	enum v4l2_buf_type buf_type;
	unsigned int caps;
};

struct node_description node_desc[PISPBE_NUM_NODES] = {
	/* MAIN_INPUT_NODE */
	{
		.name = "input",
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.caps = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
	},
	/* HOG_OUTPUT_NODE */
	{
		.name = "hog_output",
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.caps = V4L2_CAP_META_CAPTURE,
	},
	/* OUTPUT0_NODE */
	{
		.name = "output0",
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
	},
	/* OUTPUT1_NODE */
	{
		.name = "output1",
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
	},
	/* TDN_OUTPUT_NODE */
	{
		.name = "tdn_output",
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
	},
	/* STITCH_OUTPUT_NODE */
	{
		.name = "stitch_output",
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
	},
	/* CONFIG_NODE */
	{
		.name = "config",
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.caps = V4L2_CAP_META_OUTPUT,
	}
};

#define NODE_NAME(node) (node_desc[(node)->id].name)
#define NODE_IS_META(node) ( \
	((node)->buf_type == V4L2_BUF_TYPE_META_OUTPUT) || \
	((node)->buf_type == V4L2_BUF_TYPE_META_CAPTURE))
#define NODE_IS_OUTPUT(node) ( \
	((node)->buf_type == V4L2_BUF_TYPE_META_OUTPUT) || \
	((node)->buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT) || \
	((node)->buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE))
#define NODE_IS_CAPTURE(node) ( \
	((node)->buf_type == V4L2_BUF_TYPE_META_CAPTURE) || \
	((node)->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE) || \
	((node)->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))
#define NODE_IS_MPLANE(node) ( \
	((node)->buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) || \
	((node)->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))

/*
 * Structure to describe a single node /dev/video<N> which represents a single
 * input or output queue to the PiSP Back End device.
 */
struct pispbe_node {
	unsigned int id;
	int vfl_dir;
	enum v4l2_buf_type buf_type;
	struct video_device vfd;
	struct media_pad pad;
	struct media_intf_devnode *intf_devnode;
	struct media_link *intf_link;
	struct pispbe_node_group *node_group;
	struct mutex node_lock;
	struct mutex queue_lock;
	spinlock_t ready_lock;
	struct list_head ready_queue;
	int open;
	int streaming;
	/*
	 * Remember that each node can open be opened once, so stuff related to
	 * the file handle can just be kept here.
	 */
	struct v4l2_fh fh;
	struct vb2_queue queue;
	struct v4l2_format format;
	const struct pisp_be_format *pisp_format;
	struct v4l2_ctrl_handler hdl;
	/*
	 * State for TDN and stitch buffer auto-cycling
	 * (protected by ready_lock)
	 */
	unsigned int last_index;
};

#define node_get_pispbe(node) ((node)->node_group->pispbe)

/*
 * Node group structure, which comprises all the input and output nodes that a
 * single PiSP client will need.
 */
struct pispbe_node_group {
	struct pispbe_dev *pispbe;
	struct pispbe_node node[PISPBE_NUM_NODES];
	int num_streaming; /* Number of nodes with streaming turned on */
	struct media_entity entity;
	struct media_pad pad[PISPBE_NUM_NODES]; /* output pads first */
};

/* Records details of the jobs currently running or queued on the h/w. */
struct pispbe_job {
	struct pispbe_node_group *node_group;
	/*
	 * An array of buffer pointers - remember it's source buffers first,
	 * then captures, then metadata last.
	 */
	struct pispbe_buffer *buf[PISPBE_NUM_NODES];
};

/*
 * Structure representing the entire PiSP Back End device, comprising several
 * input and output nodes /dev/video<N>.
 */
struct pispbe_dev {
	struct v4l2_device v4l2_dev; /* does this belong in the node_group? */
	struct device *dev;
	struct media_device mdev;
	struct pispbe_node_group node_group[PISPBE_NUM_NODE_GROUPS];
	int hw_busy; /* non-zero if a job is being worked on */
	struct pispbe_job queued_job, running_job;
	void __iomem *be_reg_base;
	struct clk *clk;
	int irq;
	uint8_t done, started;
	/* protects access to "hw_busy" flag */
	spinlock_t hw_lock;
	 /* prevents re-entrancy in ISR, maybe unnecessary? */
	spinlock_t isr_lock;
	/* prevents re-entrancy in hw_queue_job(), maybe unnecessary? */
	spinlock_t hwq_lock;
};

static inline u32 read_reg(struct pispbe_dev *pispbe, unsigned int offset)
{
	u32 val = readl(pispbe->be_reg_base + offset);

	v4l2_dbg(3, debug, &pispbe->v4l2_dev,
		 "read 0x%08x <- 0x%08x\n", val, offset);
	return val;
}

static inline void write_reg(struct pispbe_dev *pispbe, unsigned int offset,
			     u32 val)
{
	v4l2_dbg(3, debug, &pispbe->v4l2_dev,
		 "write 0x%08x -> 0x%08x\n", val, offset);
	writel(val, pispbe->be_reg_base + offset);
}

/* Check and initialize hardware. */
static int hw_init(struct pispbe_dev *pispbe)
{
	u32 u = read_reg(pispbe, PISP_BE_VERSION_OFFSET);

	dev_info(pispbe->dev, "pispbe_probe: HW version:  0x%08x", u);
	/* Clear leftover interrupts */
	write_reg(pispbe, PISP_BE_INTERRUPT_STATUS_OFFSET, 0xFFFFFFFFu);
	u = read_reg(pispbe, PISP_BE_BATCH_STATUS_OFFSET);
	dev_info(pispbe->dev, "pispbe_probe: BatchStatus: 0x%08x", u);
	pispbe->done = (uint8_t)u;
	pispbe->started = (uint8_t)(u >> 8);
	u = read_reg(pispbe, PISP_BE_STATUS_OFFSET);
	dev_info(pispbe->dev, "pispbe_probe: Status:      0x%08x", u);
	if (u != 0 || pispbe->done != pispbe->started) {
		dev_err(pispbe->dev, "pispbe_probe: HW is stuck or busy\n");
		return -EBUSY;
	}
	/* AXI QOS=0, CACHE=4'b0010, PROT=3'b011 */
	write_reg(pispbe, PISP_BE_AXI_OFFSET, 0x32003200u);
	/* Enable both interrupt flags */
	write_reg(pispbe, PISP_BE_INTERRUPT_EN_OFFSET, 0x00000003u);
	return 0;
}

/*
 * Queue a job to the h/w. If the h/w is idle it will begin immediately.
 * Caller must ensure it is "safe to queue", i.e. we don't already have a
 * queued, unstarted job.
 */
static void hw_queue_job(struct pispbe_dev *pispbe,
			 dma_addr_t hw_dma_addrs[N_HW_ADDRESSES],
			 u32 hw_enables[N_HW_ENABLES],
			 struct pisp_be_config *config, dma_addr_t tiles,
			 unsigned int num_tiles)
{
	unsigned int begin, end;
	unsigned int u;
	unsigned long flags;

	spin_lock_irqsave(&pispbe->hwq_lock, flags);
	if (read_reg(pispbe, PISP_BE_STATUS_OFFSET) & 1)
		v4l2_err(&pispbe->v4l2_dev, "ERROR: not safe to queue new job!\n");

	/*
	 * Write configuration to hardware. DMA addresses and enable flags
	 * are passed separately, because the driver needs to sanitize them,
	 * and we don't want to modify (or be vulnerable to modifications of)
	 * the mmap'd buffer.
	 */
	for (u = 0; u < N_HW_ADDRESSES; ++u) {
		write_reg(pispbe, PISP_BE_IO_INPUT_ADDR0_LO_OFFSET + 8 * u,
			(u32)(hw_dma_addrs[u]));
		write_reg(pispbe, PISP_BE_IO_INPUT_ADDR0_LO_OFFSET + 8 * u + 4,
			(u32)(hw_dma_addrs[u] >> 32));
	}
	write_reg(pispbe, PISP_BE_GLOBAL_BAYER_ENABLE_OFFSET, hw_enables[0]);
	write_reg(pispbe, PISP_BE_GLOBAL_RGB_ENABLE_OFFSET, hw_enables[1]);

	/*
	 * Everything else is as supplied by the user. XXX Buffer sizes not
	 * checked!
	 */
	begin =	offsetof(struct pisp_be_config, global.bayer_order) /
								sizeof(u32);
	end = offsetof(struct pisp_be_config, axi) / sizeof(u32);
	for (u = begin; u < end; u++) {
		unsigned int val = ((u32 *)config)[u];

		write_reg(pispbe, PISP_BE_CONFIG_BASE_OFFSET + 4 * u, val);
	}

	/* Read back the addresses -- an error here could be fatal */
	for (u = 0; u < N_HW_ADDRESSES; ++u) {
		unsigned int offset = PISP_BE_IO_INPUT_ADDR0_LO_OFFSET + 8 * u;
		u64 along = read_reg(pispbe, offset);
		along += ((u64)read_reg(pispbe, offset + 4)) << 32;
		if (along != (u64)(hw_dma_addrs[u])) {
			v4l2_warn(&pispbe->v4l2_dev,
			       "ISP BE config error: check if ISP RAMs enabled?\n");
			spin_unlock_irqrestore(&pispbe->hwq_lock, flags);
			return;
		}
	}

	/*
	 * Write tile pointer to hardware. XXX Tile offsets and sizes not checked
	 * (and even if checked, the user could subsequently modify them)!
	 */
	write_reg(pispbe, PISP_BE_TILE_ADDR_LO_OFFSET, (u32)tiles);
	write_reg(pispbe, PISP_BE_TILE_ADDR_HI_OFFSET, (u32)(tiles >> 32));

	/* Enqueue the job */
	write_reg(pispbe, PISP_BE_CONTROL_OFFSET, 3 + 65536 * num_tiles);

	spin_unlock_irqrestore(&pispbe->hwq_lock, flags);
}

struct pispbe_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head ready_list;
};

static int get_addr_3(dma_addr_t addr[3], struct pispbe_buffer *buf,
		      struct pispbe_node *node)
{
	unsigned int num_planes = node->format.fmt.pix_mp.num_planes;
	unsigned int plane_factor = 0;
	unsigned int size;
	unsigned int p;

	if (!buf)
		return 0;

	WARN_ON(!NODE_IS_MPLANE(node));

	/*
	 * Determine the base plane size. This will not be the same
	 * as node->format.fmt.pix_mp.plane_fmt[0].sizeimage for a single
	 * plane buffer in an mplane format.
	 */
	size = node->format.fmt.pix_mp.plane_fmt[0].bytesperline *
			node->format.fmt.pix_mp.height;

	for (p = 0; p < num_planes && p < 3; p++) {
		addr[p] = vb2_dma_contig_plane_dma_addr(&buf->vb.vb2_buf, p);
		plane_factor += node->pisp_format->plane_factor[p];
	}

	for (; p < MAX_PLANES && node->pisp_format->plane_factor[p]; p++) {
		/*
		 * Calculate the address offset of this plane as needed
		 * by the hardware. This is specifically for non-mplane
		 * buffer formats, where there are 3 image planes, e.g.
		 * for the V4L2_PIX_FMT_YUV420 format.
		 */
		addr[p] = addr[0] + ((size * plane_factor) >> 8);
		plane_factor += node->pisp_format->plane_factor[p];
	}

	return num_planes;
}

static dma_addr_t get_addr(struct pispbe_buffer *buf)
{
	if (buf)
		return vb2_dma_contig_plane_dma_addr(&buf->vb.vb2_buf, 0);
	return 0;
}

static void fixup_addrs_enables(dma_addr_t addrs[N_HW_ADDRESSES],
				u32 hw_enables[N_HW_ENABLES],
				struct pisp_be_tiles_config *config,
				struct pispbe_buffer *buf[PISPBE_NUM_NODES],
				struct pispbe_buffer *rbuf[PISPBE_NUM_RECURRENT_INPUTS],
				struct pispbe_node_group *node_group)
{
	int ret, i;

	/* Take a copy of the "enable" bitmaps so we can modify them. */
	hw_enables[0] = config->config.global.bayer_enables;
	hw_enables[1] = config->config.global.rgb_enables;

	/*
	 * Main input first. There are 3 address pointers, corresponding to up
	 * to 3 planes.
	 */
	ret = get_addr_3(addrs, buf[MAIN_INPUT_NODE],
			&node_group->node[MAIN_INPUT_NODE]);
	if (ret <= 0) {
		/*
		 * This shouldn't happen; pispbe_schedule_internal should insist
		 * on an input.
		 */
		v4l2_warn(&node_group->pispbe->v4l2_dev,
			  "ISP-BE missing input\n");
		hw_enables[0] = 0;
		hw_enables[1] = 0;
		return;
	}

	/*
	 * Now TDN/Stitch inputs and outputs. These are single-plane and only
	 * used with Bayer input. Input buffers are inferred by the driver:
	 * Generally the output from job number N becomes an input to job N+1.
	 *
	 * Input enables must match the expectations of the associated
	 * processing stage, otherwise the hardware can lock up!
	 */
	if (hw_enables[0] & PISP_BE_BAYER_ENABLE_INPUT) {
		addrs[3] = get_addr(rbuf[RECURRENT_TDN_INPUT]);
		if (addrs[3] == 0 ||
		    !(hw_enables[0] & PISP_BE_BAYER_ENABLE_TDN_INPUT) ||
		    !(hw_enables[0] & PISP_BE_BAYER_ENABLE_TDN) ||
		    (config->config.tdn.reset & 1)) {
			hw_enables[0] &= ~(PISP_BE_BAYER_ENABLE_TDN_INPUT |
					   PISP_BE_BAYER_ENABLE_TDN_DECOMPRESS);
			if (!(config->config.tdn.reset & 1))
				hw_enables[0] &= ~PISP_BE_BAYER_ENABLE_TDN;
		}

		addrs[4] = get_addr(rbuf[RECURRENT_STITCH_INPUT]);
		if (addrs[4] == 0 ||
		    !(hw_enables[0] & PISP_BE_BAYER_ENABLE_STITCH_INPUT) ||
		    !(hw_enables[0] & PISP_BE_BAYER_ENABLE_STITCH)) {
			hw_enables[0] &=
				~(PISP_BE_BAYER_ENABLE_STITCH_INPUT |
				  PISP_BE_BAYER_ENABLE_STITCH_DECOMPRESS |
				  PISP_BE_BAYER_ENABLE_STITCH);
		}

		addrs[5] = get_addr(buf[TDN_OUTPUT_NODE]);
		if (addrs[5] == 0)
			hw_enables[0] &= ~(PISP_BE_BAYER_ENABLE_TDN_COMPRESS |
					   PISP_BE_BAYER_ENABLE_TDN_OUTPUT);

		addrs[6] = get_addr(buf[STITCH_OUTPUT_NODE]);
		if (addrs[6] == 0)
			hw_enables[0] &=
				~(PISP_BE_BAYER_ENABLE_STITCH_COMPRESS |
				  PISP_BE_BAYER_ENABLE_STITCH_OUTPUT);
	} else {
		/* No Bayer input? Disable entire Bayer pipe (else lockup) */
		hw_enables[0] = 0;
	}

	/* Main image output channels. */
	for (i = 0; i < PISP_BACK_END_NUM_OUTPUTS; i++) {
		ret = get_addr_3(addrs + 7 + 3 * i, buf[OUTPUT0_NODE + i],
				&node_group->node[OUTPUT0_NODE + i]);
		if (ret <= 0)
			hw_enables[1] &= ~(PISP_BE_RGB_ENABLE_OUTPUT0 << i);
	}

	/* HoG output (always single plane). */
	addrs[13] = get_addr(buf[HOG_OUTPUT_NODE]);
	if (addrs[13] == 0)
		hw_enables[1] &= ~PISP_BE_RGB_ENABLE_HOG;
}

static struct pispbe_buffer *get_last_buffer(struct pispbe_node *node)
{
	if (node && node->open && node->last_index < node->queue.num_buffers) {
		struct vb2_buffer *b = node->queue.bufs[node->last_index];
		if (b) {
			struct vb2_v4l2_buffer *vbuf =
				container_of(b, struct vb2_v4l2_buffer, vb2_buf);
			return container_of(vbuf, struct pispbe_buffer, vb);
		}
	}
	return NULL;
}

/*
 * Internal function. Called from pispbe_schedule_one/all. Returns non-zero if
 * we started a job.
 *
 * Warning: needs to be called with hw_lock taken, and releases it if it
 * schedules a job.
 */
static int pispbe_schedule_internal(struct pispbe_node_group *node_group,
				    unsigned long flags)
{
	struct pispbe_dev *pispbe = node_group->pispbe;

	if (node_group->num_streaming >= 2) {
		unsigned long flags;
		 /* remember: srcimages, captures then metadata */
		struct pispbe_buffer *buf[PISPBE_NUM_NODES];
		struct pispbe_buffer *rbuf[PISPBE_NUM_RECURRENT_INPUTS];
		struct pisp_be_tiles_config *config_tiles_buffer;
		dma_addr_t hw_dma_addrs[N_HW_ADDRESSES];
		u32 hw_enables[N_HW_ENABLES];
		int i;

		/* Check if all the streaming nodes have a buffer ready */
		for (i = 0; i < PISPBE_NUM_NODES; i++) {
			buf[i] = NULL;
			if (i == MAIN_INPUT_NODE || i == CONFIG_NODE ||
			    node_group->node[i].streaming) {
				struct pispbe_node *node = &node_group->node[i];

				spin_lock_irqsave(&node->ready_lock, flags);
				buf[i] = list_first_entry_or_null(
					&node->ready_queue,
					struct pispbe_buffer, ready_list);
				spin_unlock_irqrestore(&node->ready_lock,
						       flags);
				if (buf[i] == NULL)
					goto nothing_to_do;
			}
		}

		/* Pull a buffer from each V4L2 queue to form the queued job */
		for (i = 0; i < PISPBE_NUM_NODES; i++) {
			if (buf[i]) {
				struct pispbe_node *node = &node_group->node[i];

				spin_lock_irqsave(&node->ready_lock, flags);
				list_del(&buf[i]->ready_list);
				spin_unlock_irqrestore(&node->ready_lock,
						       flags);
			}
			pispbe->queued_job.buf[i] = buf[i];
		}

		pispbe->queued_job.node_group = node_group;
		pispbe->hw_busy = 1;
		spin_unlock_irqrestore(&pispbe->hw_lock, flags);

		/*
		 * We can kick the job off without the hw_lock, as this can
		 * never run again until hw_busy is cleared.
		 */
		v4l2_dbg(1, debug, &node_group->pispbe->v4l2_dev,
			 "Have buffers - starting hardware\n");
		v4l2_ctrl_request_setup(
			pispbe->queued_job.buf[0]->vb.vb2_buf.req_obj.req,
			&node_group->node[0].hdl);
		config_tiles_buffer =
			vb2_plane_vaddr(&buf[CONFIG_NODE]->vb.vb2_buf, 0);

		/*
		 * Automation for TDN/Stitch inputs and outputs. Generally,
		 * the output from job number N becomes an input to job N+1.
		 * Because a buffer may be needed by adjacently-queued jobs,
		 * and perhaps (not necessarily) be overwritten in situ, only
		 * Capture buffers can be queued by V4L2; inputs are inferred.
		 *
		 * Furthermore, if a TDN/Stitch Capture node is not streaming,
		 * the driver will automatically cycle through the buffers.
		 * (User must still have called REQBUFS with 1 or 2 buffers
		 * of suitable dimensions and type. The initial state will
		 * always be read from the buffer with index 0.)
		 *
		 * Buffers which weren't queued by V4L2 are not registered
		 * in pispbe->queued_job.
		 */
		spin_lock_irqsave(&node_group->node[TDN_OUTPUT_NODE].ready_lock, flags);
		rbuf[RECURRENT_TDN_INPUT] = get_last_buffer(&node_group->node[TDN_OUTPUT_NODE]);
		if (config_tiles_buffer->config.global.bayer_enables &
		    PISP_BE_BAYER_ENABLE_TDN_OUTPUT) {
			if (buf[TDN_OUTPUT_NODE] == NULL) {
				if (++node_group->node[TDN_OUTPUT_NODE].last_index >=
				      node_group->node[TDN_OUTPUT_NODE].queue.num_buffers)
					node_group->node[TDN_OUTPUT_NODE].last_index = 0;
				buf[TDN_OUTPUT_NODE] =
					get_last_buffer(&node_group->node[TDN_OUTPUT_NODE]);
			} else {
				node_group->node[TDN_OUTPUT_NODE].last_index =
					buf[TDN_OUTPUT_NODE]->vb.vb2_buf.index;
			}
		}
		spin_unlock_irqrestore(&node_group->node[TDN_OUTPUT_NODE].ready_lock, flags);

		spin_lock_irqsave(&node_group->node[STITCH_OUTPUT_NODE].ready_lock, flags);
		rbuf[RECURRENT_STITCH_INPUT] = get_last_buffer(&node_group->node[STITCH_OUTPUT_NODE]);
		if (config_tiles_buffer->config.global.bayer_enables &
		    PISP_BE_BAYER_ENABLE_STITCH_OUTPUT) {
			if (buf[STITCH_OUTPUT_NODE] == NULL) {
				if (++node_group->node[STITCH_OUTPUT_NODE].last_index >=
				      node_group->node[STITCH_OUTPUT_NODE].queue.num_buffers)
					node_group->node[STITCH_OUTPUT_NODE].last_index = 0;
				buf[STITCH_OUTPUT_NODE] =
					get_last_buffer(&node_group->node[STITCH_OUTPUT_NODE]);
			} else {
				node_group->node[STITCH_OUTPUT_NODE].last_index =
					buf[STITCH_OUTPUT_NODE]->vb.vb2_buf.index;
			}
		}
		spin_unlock_irqrestore(&node_group->node[STITCH_OUTPUT_NODE].ready_lock, flags);

		/* Convert buffers to DMA addresses for the hardware */
		fixup_addrs_enables(hw_dma_addrs, hw_enables,
				    config_tiles_buffer, buf, rbuf, node_group);
		/*
		 * This could be a spot to fill in the buf[i]->vb.vb2_buf.planes[j].bytesused
		 * fields?
		 */
		i = config_tiles_buffer->num_tiles;
		if (i <= 0 || i > PISP_BACK_END_NUM_TILES ||
		    !((hw_enables[0] | hw_enables[1]) &
		      PISP_BE_BAYER_ENABLE_INPUT)) {
			/*
			 * Bad job. We can't let it proceed as it could lock up
			 * the hardware, or worse!
			 *
			 * XXX How to deal with this most cleanly? For now, just
			 * force num_tiles to 0, which causes the H/W to do
			 * something bizarre but survivable. It increments
			 * (started,done) counters by more than 1, but we seem
			 * to survive...
			 */
			v4l2_err(&node_group->pispbe->v4l2_dev, "PROBLEM: Bad job");
			i = 0;
		}
		hw_queue_job(pispbe, hw_dma_addrs, hw_enables,
			     &config_tiles_buffer->config,
			     vb2_dma_contig_plane_dma_addr(
				     &buf[CONFIG_NODE]->vb.vb2_buf, 0) +
				     offsetof(struct pisp_be_tiles_config,
					      tiles),
			     i);

		return 1;
	}
nothing_to_do:
	v4l2_dbg(1, debug, &node_group->pispbe->v4l2_dev, "Nothing to do\n");
	return 0;
}

/* Try and schedule a job for just a single node group. */
static void pispbe_schedule_one(struct pispbe_node_group *node_group)
{
	struct pispbe_dev *pispbe = node_group->pispbe;
	unsigned long flags;
	int lock_released = 0;

	spin_lock_irqsave(&pispbe->hw_lock, flags);
	if (pispbe->hw_busy == 0)
		lock_released = pispbe_schedule_internal(node_group, flags);
	if (!lock_released)
		spin_unlock_irqrestore(&pispbe->hw_lock, flags);
}

/* Try and schedule a job for any of the node groups. */
static void pispbe_schedule_all(struct pispbe_dev *pispbe, int clear_hw_busy)
{
	unsigned long flags;

	spin_lock_irqsave(&pispbe->hw_lock, flags);

	if (clear_hw_busy)
		pispbe->hw_busy = 0;
	if (pispbe->hw_busy == 0) {
		unsigned int i;

		for (i = 0; i < PISPBE_NUM_NODE_GROUPS; i++) {
			/*
			 * A non-zero return from pispbe_schedule_internal means
			 * the lock was released.
			 */
			if (pispbe_schedule_internal(&pispbe->node_group[i],
						     flags))
				return;
		}
	}
	spin_unlock_irqrestore(&pispbe->hw_lock, flags);
}

static irqreturn_t pispbe_isr(int irq, void *dev)
{
	struct pispbe_dev *pispbe = (struct pispbe_dev *)dev;
	uint8_t started, done;
	int i;
	int clear_hw_busy = 0;
	unsigned long flags;
	u32 u;

	spin_lock_irqsave(&pispbe->isr_lock, flags);

	u = read_reg(pispbe, PISP_BE_INTERRUPT_STATUS_OFFSET);
	if (u == 0) {
		spin_unlock_irqrestore(&pispbe->isr_lock, flags);
		return IRQ_NONE;
	}
	write_reg(pispbe, PISP_BE_INTERRUPT_STATUS_OFFSET, u);
	v4l2_dbg(1, debug, &pispbe->v4l2_dev, "Hardware interrupt\n");
	u = read_reg(pispbe, PISP_BE_BATCH_STATUS_OFFSET);
	done = (uint8_t)u;
	started = (uint8_t)(u >> 8);
	v4l2_dbg(1, debug, &pispbe->v4l2_dev,
		 "H/W started %d done %d, previously started %d done %d\n",
		 (int)started, (int)done, (int)pispbe->started,
		 (int)pispbe->done);

	/*
	 * Be aware that done can go up by 2 and started by 1 when: a job that
	 * we previously saw "start" now finishes, and we then queued a new job
	 * which we see both start and finish "simultaneously".
	 */
	if (pispbe->done != done && pispbe->running_job.node_group) {
		struct pispbe_node_group *node_group =
			pispbe->running_job.node_group;
		v4l2_ctrl_request_complete(
			pispbe->running_job.buf[0]->vb.vb2_buf.req_obj.req,
			&node_group->node[0].hdl);

		for (i = 0; i < PISPBE_NUM_NODES; i++) {
			if (pispbe->running_job.buf[i])
				vb2_buffer_done(
					&pispbe->running_job.buf[i]->vb.vb2_buf,
					VB2_BUF_STATE_DONE);
		}

		memset(&pispbe->running_job, 0, sizeof(pispbe->running_job));
		pispbe->done++;
		v4l2_dbg(1, debug, &pispbe->v4l2_dev, "Job done (1)\n");
	}

	if (pispbe->started != started) {
		pispbe->started++;
		pispbe->running_job = pispbe->queued_job;
		memset(&pispbe->queued_job, 0, sizeof(pispbe->queued_job));
		clear_hw_busy = 1;
		v4l2_dbg(1, debug, &pispbe->v4l2_dev, "Job started\n");
	}

	if (pispbe->done != done && pispbe->running_job.node_group) {
		struct pispbe_node_group *node_group =
			pispbe->running_job.node_group;
		v4l2_ctrl_request_complete(
			pispbe->running_job.buf[0]->vb.vb2_buf.req_obj.req,
			&node_group->node[0].hdl);

		for (i = 0; i < PISPBE_NUM_NODES; i++) {
			if (pispbe->running_job.buf[i])
				vb2_buffer_done(
					&pispbe->running_job.buf[i]->vb.vb2_buf,
					VB2_BUF_STATE_DONE);
		}

		memset(&pispbe->running_job, 0, sizeof(pispbe->running_job));
		pispbe->done++;
		v4l2_dbg(1, debug, &pispbe->v4l2_dev, "Job done (2)\n");
	}

	if (pispbe->done != done || pispbe->started != started) {
		v4l2_err(&pispbe->v4l2_dev,
			 "PROBLEM: counters not matching!\n");
		pispbe->started = started;
		pispbe->done = done;
	}
	spin_unlock_irqrestore(&pispbe->isr_lock, flags);

	/* must check if there's more to do before going to sleep */
	pispbe_schedule_all(pispbe, clear_hw_busy);

	return IRQ_HANDLED;
}

static int pispbe_node_queue_setup(struct vb2_queue *q, unsigned int *nbuffers,
				   unsigned int *nplanes, unsigned int sizes[],
				   struct device *alloc_devs[])
{
	struct pispbe_node *node = vb2_get_drv_priv(q);

	*nplanes = 1;
	if (NODE_IS_MPLANE(node)) {
		unsigned int i;

		*nplanes = node->format.fmt.pix_mp.num_planes;
		for (i = 0; i < *nplanes; i++)
			sizes[i] =
				node->format.fmt.pix_mp.plane_fmt[i].sizeimage;
	} else if (NODE_IS_META(node)) {
		sizes[0] = node->format.fmt.meta.buffersize;
	}

	if (sizes[0] * (*nbuffers) > PISPBE_QUEUE_MEM)
		*nbuffers = PISPBE_QUEUE_MEM / sizes[0];

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Image (or metadata) size %u, nbuffers %u for node %s\n",
		 sizes[0], *nbuffers, NODE_NAME(node));

	return 0;
}

static int pispbe_node_buffer_prepare(struct vb2_buffer *vb)
{
	struct pispbe_node *node = vb2_get_drv_priv(vb->vb2_queue);
	struct pispbe_dev *pispbe = node_get_pispbe(node);
	unsigned long size = 0;
	unsigned int num_planes = NODE_IS_MPLANE(node) ?
					node->format.fmt.pix_mp.num_planes : 1;
	unsigned int i;

	for (i = 0; i < num_planes; i++) {
		size = NODE_IS_MPLANE(node)
			? node->format.fmt.pix_mp.plane_fmt[i].sizeimage
			: node->format.fmt.meta.buffersize;

		if (vb2_plane_size(vb, i) < size) {
			v4l2_err(&pispbe->v4l2_dev, "data will not fit into plane %d (%lu < %lu)\n",
				 i, vb2_plane_size(vb, i), size);
			return -EINVAL;
		}

		vb2_set_plane_payload(vb, i, size);
	}

	return 0;
}

static void pispbe_node_buffer_queue(struct vb2_buffer *buf)
{
	struct vb2_v4l2_buffer *vbuf =
		container_of(buf, struct vb2_v4l2_buffer, vb2_buf);
	struct pispbe_buffer *buffer =
		container_of(vbuf, struct pispbe_buffer, vb);
	struct pispbe_node *node = vb2_get_drv_priv(buf->vb2_queue);
	struct pispbe_node_group *node_group = node->node_group;
	unsigned long flags;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "%s: for node %s\n", __func__, NODE_NAME(node));
	spin_lock_irqsave(&node->ready_lock, flags);
	list_add_tail(&buffer->ready_list, &node->ready_queue);
	spin_unlock_irqrestore(&node->ready_lock, flags);

	/*
	 * Every time we add a buffer, check if there's now some work for the hw
	 * to do, but only for this client.
	 */
	pispbe_schedule_one(node_group);
}

static int pispbe_node_start_streaming(struct vb2_queue *q, unsigned int count)
{
	unsigned long flags;
	struct pispbe_node *node = vb2_get_drv_priv(q);
	struct pispbe_node_group *node_group = node->node_group;
	struct pispbe_dev *pispbe = node_group->pispbe;

	spin_lock_irqsave(&pispbe->hw_lock, flags);
	node->node_group->num_streaming++;
	node->streaming = 1;
	spin_unlock_irqrestore(&pispbe->hw_lock, flags);

	v4l2_dbg(1, debug, &pispbe->v4l2_dev,
		 "%s: for node %s (count %u)\n",
		 __func__, NODE_NAME(node), count);
	v4l2_dbg(1, debug, &pispbe->v4l2_dev,
		 "Nodes streaming for this group now %d\n",
		 node->node_group->num_streaming);

	/* Maybe we're ready to run. */
	pispbe_schedule_one(node_group);

	return 0;
}

static void pispbe_node_stop_streaming(struct vb2_queue *q)
{
	struct pispbe_node *node = vb2_get_drv_priv(q);
	struct pispbe_node_group *node_group = node->node_group;
	struct pispbe_dev *pispbe = node_group->pispbe;
	struct pispbe_buffer *buf;
	unsigned long flags;

	/*
	 * Now this is a bit awkward. In a simple M2M device we could just wait
	 * for all queued jobs to complete, but here there's a risk that a partial
	 * set of buffers was queued and cannot be run. For now, just cancel all
	 * buffers stuck in the "ready queue", then wait for any running job.
	 * XXX This may return buffers out of order.
	 */
	v4l2_dbg(1, debug, &pispbe->v4l2_dev,
		 "%s: for node %s\n", __func__, NODE_NAME(node));
	spin_lock_irqsave(&pispbe->hw_lock, flags);
	do {
		unsigned long flags1;

		spin_lock_irqsave(&node->ready_lock, flags1);
		buf = list_first_entry_or_null(
			&node->ready_queue, struct pispbe_buffer, ready_list);
		if (buf) {
			list_del(&buf->ready_list);
			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		}
		spin_unlock_irqrestore(&node->ready_lock, flags1);
	} while (buf);
	spin_unlock_irqrestore(&pispbe->hw_lock, flags);

	vb2_wait_for_all_buffers(&node->queue);

	spin_lock_irqsave(&pispbe->hw_lock, flags);
	node_group->num_streaming--;
	node->streaming = 0;
	spin_unlock_irqrestore(&pispbe->hw_lock, flags);

	v4l2_dbg(1, debug, &pispbe->v4l2_dev,
		 "Nodes streaming for this group now %d\n",
		 node_group->num_streaming);
}

static void pispbe_buf_request_complete(struct vb2_buffer *vb)
{
	struct pispbe_node *node = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "%s\n", __func__);
	v4l2_ctrl_request_complete(vb->req_obj.req, &node->hdl);
}

static int pispbe_buf_out_validate(struct vb2_buffer *vb)
{
	struct pispbe_node *node = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "%s\n", __func__);
	return 0;
}

static const struct vb2_ops pispbe_node_queue_ops = {
	.queue_setup = pispbe_node_queue_setup,
	.buf_prepare = pispbe_node_buffer_prepare,
	.buf_queue = pispbe_node_buffer_queue,
	.start_streaming = pispbe_node_start_streaming,
	.stop_streaming = pispbe_node_stop_streaming,
	.buf_request_complete = pispbe_buf_request_complete,
	.buf_out_validate = pispbe_buf_out_validate,
};

static int pispbe_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct pispbe_node *node =
		container_of(ctrl->handler, struct pispbe_node, hdl);
	struct pispbe_node_group *node_group = node->node_group;
	struct pispbe_dev *pispbe = node_group->pispbe;

	v4l2_dbg(1, debug, &pispbe->v4l2_dev, "Ctrl id is %u\n", ctrl->id);

	switch (ctrl->id) {
		/* We have no control parameters, currently. */
	default:
		v4l2_warn(&pispbe->v4l2_dev, "Unrecognised control\n");
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops pispbe_ctrl_ops = {
	.s_ctrl = pispbe_s_ctrl,
};

/*
 * Open one of the nodes /dev/video<N> associated with the PiSP Back End.
 * Each node can be opened only once.
 */
static int pispbe_open(struct file *file)
{
	struct pispbe_node *node = video_drvdata(file);
	struct pispbe_dev *pispbe = node_get_pispbe(node);
	int ret = 0;
	struct vb2_queue *queue;
	struct v4l2_ctrl_handler *hdl;

	if (mutex_lock_interruptible(&node->node_lock))
		return -ERESTARTSYS;

	if (node->open) {
		ret = -EBUSY;
		goto unlock_return;
	}

	v4l2_dbg(1, debug, &pispbe->v4l2_dev, "Opening node %s\n",
		 NODE_NAME(node));

	v4l2_fh_init(&node->fh, video_devdata(file));
	file->private_data = &node->fh;

	hdl = &node->hdl;
	v4l2_ctrl_handler_init(hdl, 0);
	/* We have no controls currently. */
	if (hdl->error) {
		ret = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		goto unlock_return;
	}
	node->fh.ctrl_handler = hdl;
	v4l2_ctrl_handler_setup(hdl);

	v4l2_fh_add(&node->fh);
	node->open = 1;
	node->streaming = 0;

	queue = &node->queue;
	queue->type = node->buf_type;
#if SUPPORT_IO_USERPTR
	queue->io_modes = VB2_USERTPR | VB2_MMAP | VB2_DMABUF;
	queue->mem_ops = &vb2_vmalloc_memops;
#else
	queue->io_modes = VB2_MMAP | VB2_DMABUF;
	queue->mem_ops = &vb2_dma_contig_memops;
#endif
	queue->drv_priv = node;
	queue->ops = &pispbe_node_queue_ops;
	queue->buf_struct_size = sizeof(struct pispbe_buffer);
	queue->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	queue->dev = pispbe->dev;
	queue->lock = &node->queue_lock; /* get V4L2 to handle queue locking */
	if (NODE_IS_OUTPUT(node))
		queue->supports_requests = true;

	ret = vb2_queue_init(queue);
	if (ret < 0) {
		v4l2_err(&pispbe->v4l2_dev, "vb2_queue_init failed\n");
		v4l2_fh_del(&node->fh);
		v4l2_fh_exit(&node->fh);
		node->open = 0;
	}

unlock_return:
	mutex_unlock(&node->node_lock);
	return ret;
}

static int pispbe_release(struct file *file)
{
	struct pispbe_node *node = video_drvdata(file);

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Releasing node %s\n", NODE_NAME(node));

	/* TODO: make sure streamoff was called */

	mutex_lock(&node->node_lock);
	vb2_queue_release(&node->queue);

	v4l2_ctrl_handler_free(&node->hdl);
	v4l2_fh_del(&node->fh);
	v4l2_fh_exit(&node->fh);
	node->open = 0;
	mutex_unlock(&node->node_lock);

	return 0;
}

static unsigned int pispbe_poll(struct file *file, poll_table *wait)
{
	struct pispbe_node *node = video_drvdata(file);
	unsigned int ret;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev, "Polling %s\n",
		 NODE_NAME(node));

	/* locking should be handled by the queue->lock? */
	ret = vb2_poll(&node->queue, file, wait);

	return ret;
}

static int pispbe_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct pispbe_node *node = video_drvdata(file);
	unsigned int ret;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev, "Mmap %s\n",
		 NODE_NAME(node));

	/* locking should be handled by the queue->lock? */
	ret = vb2_mmap(&node->queue, vma);

	return ret;
}

static const struct v4l2_file_operations pispbe_fops = {
	.owner = THIS_MODULE,
	.open = pispbe_open,
	.release = pispbe_release,
	.poll = pispbe_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = pispbe_mmap,
};

static int pispbe_node_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	struct pispbe_node *node = video_drvdata(file);

	strscpy(cap->driver, PISPBE_NAME, sizeof(cap->driver));
	strscpy(cap->card, PISPBE_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 PISPBE_NAME);

	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			    V4L2_CAP_STREAMING | V4L2_CAP_DEVICE_CAPS |
			    V4L2_CAP_META_OUTPUT | V4L2_CAP_META_CAPTURE;
	cap->device_caps = node->vfd.device_caps;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Caps for node %s: %x and %x (dev %x)\n", NODE_NAME(node),
		 cap->capabilities, cap->device_caps, node->vfd.device_caps);
	return 0;
}

static int pispbe_node_g_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);

	if (!NODE_IS_CAPTURE(node) || NODE_IS_META(node)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Cannot get capture fmt for output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}
	*f = node->format;
	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Get capture format for node %s\n", NODE_NAME(node));
	return 0;
}

static int pispbe_node_g_fmt_vid_out(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);

	if (NODE_IS_CAPTURE(node) || NODE_IS_META(node)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Cannot get capture fmt for output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}
	*f = node->format;
	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Get output format for node %s\n", NODE_NAME(node));
	return 0;
}

static int pispbe_node_g_fmt_meta_out(struct file *file, void *priv,
				      struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);

	if (!NODE_IS_META(node) || NODE_IS_CAPTURE(node)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Cannot get capture fmt for meta output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}
	*f = node->format;
	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Get output format for meta node %s\n", NODE_NAME(node));
	return 0;
}

static int pispbe_node_g_fmt_meta_cap(struct file *file, void *priv,
				      struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);

	if (!NODE_IS_META(node) || NODE_IS_OUTPUT(node)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Cannot get capture fmt for meta output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}
	*f = node->format;
	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Get output format for meta node %s\n", NODE_NAME(node));
	return 0;
}

static int verify_be_pix_format(const struct v4l2_format *f,
				struct pispbe_node *node)
{
	unsigned int nplanes = f->fmt.pix_mp.num_planes;
	unsigned int i;

	if (f->fmt.pix_mp.width == 0 || f->fmt.pix_mp.height == 0) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Details incorrect for output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}

	if (nplanes == 0 || nplanes > MAX_PLANES) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Bad number of planes for output node %s, req =%d\n",
			 NODE_NAME(node), nplanes);
		return -EINVAL;
	}

	for (i = 0; i < nplanes; i++) {
		const struct v4l2_plane_pix_format *p;

		p = &f->fmt.pix_mp.plane_fmt[i];
		if (p->bytesperline == 0 || p->sizeimage == 0) {
			v4l2_err(&node_get_pispbe(node)->v4l2_dev,
				 "Invalid plane %d for output node %s\n",
				 i, NODE_NAME(node));
			return -EINVAL;
		}
	}

	return 0;
}

static const struct pisp_be_format *find_format(unsigned int fourcc)
{
	const struct pisp_be_format *fmt;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_formats); i++) {
		fmt = &supported_formats[i];
		if (fmt->fourcc == fourcc)
			return fmt;
	}

	return NULL;
}

static void set_plane_params(struct v4l2_format *f,
			     const struct pisp_be_format *fmt)
{
	unsigned int nplanes = f->fmt.pix_mp.num_planes;
	unsigned int total_plane_factor = 0;
	unsigned int i;

	for (i = 0; i < MAX_PLANES; i++)
		total_plane_factor += fmt->plane_factor[i];

	for (i = 0; i < nplanes; i++) {
		struct v4l2_plane_pix_format *p = &f->fmt.pix_mp.plane_fmt[i];
		unsigned int bpl, plane_size;

		bpl = (f->fmt.pix_mp.width * fmt->bit_depth) >> 3;
		bpl = ALIGN(max(p->bytesperline, bpl), fmt->align);

		plane_size = bpl * f->fmt.pix_mp.height *
		      (nplanes > 1 ? fmt->plane_factor[i] : total_plane_factor);
		/*
		 * The shift is to divide out the plane_factor fixed point
		 * scaling of 256.
		 */
		plane_size = max(p->sizeimage, plane_size >> 8);

		p->bytesperline = bpl;
		p->sizeimage = plane_size;
	}
}

static int try_format(struct v4l2_format *f, struct pispbe_node *node)
{
	const struct pisp_be_format *fmt;
	unsigned int i;
	bool is_rgb;
	u32 pixfmt = f->fmt.pix_mp.pixelformat;

	v4l2_dbg(2, debug,  &node_get_pispbe(node)->v4l2_dev,
		 "%s: [%s] req %ux%u " V4L2_FOURCC_CONV ", planes %d\n",
		 __func__, node_desc[node->id].name, f->fmt.pix_mp.width,
		 f->fmt.pix_mp.height, V4L2_FOURCC_CONV_ARGS(pixfmt),
		 f->fmt.pix_mp.num_planes);

	fmt = find_format(pixfmt);
	if (!fmt)
		return -EINVAL;

	if (pixfmt == V4L2_PIX_FMT_RPI_BE)
		return verify_be_pix_format(f, node);

	f->fmt.pix_mp.pixelformat = fmt->fourcc;
	f->fmt.pix_mp.num_planes = fmt->num_planes;
	f->fmt.pix_mp.field = V4L2_FIELD_NONE;
	f->fmt.pix_mp.width = max(min(f->fmt.pix_mp.width, 65536u),
				  PISP_BACK_END_MIN_TILE_WIDTH);
	f->fmt.pix_mp.height = max(min(f->fmt.pix_mp.height, 65536u),
				   PISP_BACK_END_MIN_TILE_HEIGHT);

	/*
	 * Fill in the actual colour space when the requested one was
	 * not supported. This also catches the case when the "default"
	 * colour space was requested (as that's never in the mask).
	 */
	if (!(COLORSPACE_MASK(f->fmt.pix_mp.colorspace) & fmt->colorspace_mask))
		f->fmt.pix_mp.colorspace = fmt->colorspace_default;

	/* In all cases, we only support the defaults for these: */
	f->fmt.pix_mp.ycbcr_enc =
		V4L2_MAP_YCBCR_ENC_DEFAULT(f->fmt.pix_mp.colorspace);
	f->fmt.pix_mp.xfer_func =
		V4L2_MAP_XFER_FUNC_DEFAULT(f->fmt.pix_mp.colorspace);

	is_rgb = f->fmt.pix_mp.colorspace == V4L2_COLORSPACE_SRGB;
	f->fmt.pix_mp.quantization =
		V4L2_MAP_QUANTIZATION_DEFAULT(is_rgb, f->fmt.pix_mp.colorspace,
					      f->fmt.pix_mp.ycbcr_enc);

	/* Set plane size and bytes/line for each plane. */
	set_plane_params(f, fmt);

	for (i = 0; i < f->fmt.pix_mp.num_planes; i++) {
		v4l2_dbg(2, debug,  &node_get_pispbe(node)->v4l2_dev,
			 "%s: [%s] calc plane %d, %ux%u, depth %u, bpl %u size %u\n",
			 __func__, node_desc[node->id].name, i, f->fmt.pix_mp.width, f->fmt.pix_mp.height,
			 fmt->bit_depth, f->fmt.pix_mp.plane_fmt[i].bytesperline,
			 f->fmt.pix_mp.plane_fmt[i].sizeimage);
	}

	return 0;
}

static int pispbe_node_try_fmt_vid_cap(struct file *file, void *priv,
				       struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret;

	if (!NODE_IS_CAPTURE(node) || NODE_IS_META(node)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Cannot set capture fmt for output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}

	ret = try_format(f, node);
	if (ret < 0)
		return ret;

	return 0;
}

static int pispbe_node_try_fmt_vid_out(struct file *file, void *priv,
				       struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret;

	if (!NODE_IS_OUTPUT(node) || NODE_IS_META(node)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Cannot set capture fmt for output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}

	ret = try_format(f, node);
	if (ret < 0)
		return ret;

	return 0;
}

static int pispbe_node_try_fmt_meta_out(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);

	if (!NODE_IS_META(node) || NODE_IS_CAPTURE(node)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Cannot set capture fmt for meta output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}

	f->fmt.meta.dataformat = V4L2_META_FMT_RPI_BE_CFG;
	f->fmt.meta.buffersize = sizeof(struct pisp_be_tiles_config);

	return 0;
}

static int pispbe_node_try_fmt_meta_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);

	if (!NODE_IS_META(node) || NODE_IS_OUTPUT(node)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev,
			 "Cannot set capture fmt for meta output node %s\n",
			 NODE_NAME(node));
		return -EINVAL;
	}

	if (f->fmt.meta.dataformat != V4L2_PIX_FMT_RPI_BE ||
	    !f->fmt.meta.buffersize)
		return -EINVAL;

	return 0;
}

static int pispbe_node_s_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret = pispbe_node_try_fmt_vid_cap(file, priv, f);

	if (ret < 0)
		return ret;

	node->format = *f;
	node->pisp_format = find_format(f->fmt.pix_mp.pixelformat);

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Set capture format for node %s to " V4L2_FOURCC_CONV "\n",
		 NODE_NAME(node), V4L2_FOURCC_CONV_ARGS(f->fmt.pix_mp.pixelformat));
	return 0;
}

static int pispbe_node_s_fmt_vid_out(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret = pispbe_node_try_fmt_vid_out(file, priv, f);

	if (ret < 0)
		return ret;

	node->format = *f;
	node->pisp_format = find_format(f->fmt.pix_mp.pixelformat);

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Set output format for node %s to " V4L2_FOURCC_CONV "\n",
		 NODE_NAME(node), V4L2_FOURCC_CONV_ARGS(f->fmt.pix_mp.pixelformat));
	return 0;
}

static int pispbe_node_s_fmt_meta_out(struct file *file, void *priv,
				      struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret = pispbe_node_try_fmt_meta_out(file, priv, f);

	if (ret < 0)
		return ret;

	node->format = *f;
	node->pisp_format = find_format(f->fmt.meta.dataformat);

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Set output format for meta node %s to " V4L2_FOURCC_CONV "\n",
		 NODE_NAME(node), V4L2_FOURCC_CONV_ARGS(f->fmt.meta.dataformat));
	return 0;
}

static int pispbe_node_s_fmt_meta_cap(struct file *file, void *priv,
				      struct v4l2_format *f)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret = pispbe_node_try_fmt_meta_cap(file, priv, f);

	if (ret < 0)
		return ret;

	node->format = *f;
	node->pisp_format = find_format(f->fmt.meta.dataformat);

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Set capture format for meta node %s to " V4L2_FOURCC_CONV "\n",
		 NODE_NAME(node), V4L2_FOURCC_CONV_ARGS(f->fmt.meta.dataformat));
	return 0;
}

static int pispbe_node_enum_fmt(struct file *file, void  *priv,
				struct v4l2_fmtdesc *f)
{
	struct pispbe_node *node = video_drvdata(file);

	if (f->type != node->queue.type)
		return -EINVAL;

	if (f->index < ARRAY_SIZE(supported_formats)) {
		/* Format found */
		f->pixelformat = supported_formats[f->index].fourcc;
		f->flags = 0;
		return 0;
	}

	return -EINVAL;
}

static int pispbe_enum_framesizes(struct file *file, void *priv,
				  struct v4l2_frmsizeenum *fsize)
{
	struct pispbe_node *node = video_drvdata(file);

	if (NODE_IS_META(node) || fsize->index)
		return -EINVAL;

	if (!find_format(fsize->pixel_format)) {
		v4l2_err(&node_get_pispbe(node)->v4l2_dev, "Invalid pixel code: %x\n",
			 fsize->pixel_format);
		return -EINVAL;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.min_width = 32;
	fsize->stepwise.max_width = 65535;
	fsize->stepwise.step_width = 2;

	fsize->stepwise.min_height = 32;
	fsize->stepwise.max_height = 65535;
	fsize->stepwise.step_height = 2;

	return 0;
}

static int pispbe_node_querybuf(struct file *file, void *priv,
				struct v4l2_buffer *b)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Querybuf for node %s\n", NODE_NAME(node));

	/* locking should be handled by the queue->lock? */
	ret = vb2_querybuf(&node->queue, b);

	return ret;
}

static int pispbe_node_reqbufs(struct file *file, void *priv,
			       struct v4l2_requestbuffers *rb)
{
	unsigned long flags;
	struct pispbe_node *node = video_drvdata(file);
	int ret;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Reqbufs for node %s\n", NODE_NAME(node));

	/* Initialise last_index (for TDN/Stitch auto-cycling) */
	spin_lock_irqsave(&node->ready_lock, flags);
	node->last_index = 0;
	spin_unlock_irqrestore(&node->ready_lock, flags);

	/* locking should be handled by the queue->lock? */
	ret = vb2_reqbufs(&node->queue, rb);
	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Reqbufs returned %d\n", ret);

	return ret;
}

static int pispbe_node_expbuf(struct file *file, void *priv,
			      struct v4l2_exportbuffer *eb)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Expbuf for node %s\n", NODE_NAME(node));

	/* locking should be handled by the queue->lock? */
	ret = vb2_expbuf(&node->queue, eb);

	return ret;
}

static int pispbe_node_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *b)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Queue buffer for node %s\n", NODE_NAME(node));

	/* locking should be handled by the queue->lock? */
	ret = vb2_qbuf(&node->queue, &node_get_pispbe(node)->mdev, b);

	return ret;
}

static int pispbe_node_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *b)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret;

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Dequeue buffer for node %s\n", NODE_NAME(node));

	/* locking should be handled by the queue->lock? */
	ret = vb2_dqbuf(&node->queue, b, file->f_flags & O_NONBLOCK);

	return ret;
}

static int pispbe_node_streamon(struct file *file, void *priv,
				enum v4l2_buf_type type)
{
	struct pispbe_node *node = video_drvdata(file);
	int ret;

	/* Do we need a node->stream_lock mutex? */

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Stream on for node %s\n", NODE_NAME(node));

	/* Do we care about the type? Each node has only one queue. */

	INIT_LIST_HEAD(&node->ready_queue);

	/* locking should be handled by the queue->lock? */
	ret = vb2_streamon(&node->queue, type);

	return ret;
}

static int pispbe_node_streamoff(struct file *file, void *priv,
				 enum v4l2_buf_type type)
{
	struct pispbe_node *node = video_drvdata(file);

	/* Do we need a node->stream_lock mutex? */

	v4l2_dbg(1, debug, &node_get_pispbe(node)->v4l2_dev,
		 "Stream off for node %s\n", NODE_NAME(node));

	/* Do we care about the type? Each node has only one queue. */

	/* locking should be handled by the queue->lock? */
	vb2_streamoff(&node->queue,
		      type); /* causes any buffers to be returned */

	return 0;
}

static const struct v4l2_ioctl_ops pispbe_node_ioctl_ops = {
	.vidioc_querycap = pispbe_node_querycap,
	.vidioc_g_fmt_vid_cap_mplane = pispbe_node_g_fmt_vid_cap,
	.vidioc_g_fmt_vid_out_mplane = pispbe_node_g_fmt_vid_out,
	.vidioc_g_fmt_meta_out = pispbe_node_g_fmt_meta_out,
	.vidioc_g_fmt_meta_cap = pispbe_node_g_fmt_meta_cap,
	.vidioc_try_fmt_vid_cap_mplane = pispbe_node_try_fmt_vid_cap,
	.vidioc_try_fmt_vid_out_mplane = pispbe_node_try_fmt_vid_out,
	.vidioc_try_fmt_meta_out = pispbe_node_try_fmt_meta_out,
	.vidioc_try_fmt_meta_cap = pispbe_node_try_fmt_meta_cap,
	.vidioc_s_fmt_vid_cap_mplane = pispbe_node_s_fmt_vid_cap,
	.vidioc_s_fmt_vid_out_mplane = pispbe_node_s_fmt_vid_out,
	.vidioc_s_fmt_meta_out = pispbe_node_s_fmt_meta_out,
	.vidioc_s_fmt_meta_cap = pispbe_node_s_fmt_meta_cap,
	.vidioc_enum_fmt_vid_cap = pispbe_node_enum_fmt,
	.vidioc_enum_fmt_vid_out = pispbe_node_enum_fmt,
	.vidioc_enum_fmt_meta_cap = pispbe_node_enum_fmt,
	.vidioc_enum_framesizes = pispbe_enum_framesizes,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_reqbufs = pispbe_node_reqbufs,
	.vidioc_querybuf = pispbe_node_querybuf,
	.vidioc_expbuf = pispbe_node_expbuf,
	.vidioc_qbuf = pispbe_node_qbuf,
	.vidioc_dqbuf = pispbe_node_dqbuf,
	.vidioc_streamon = pispbe_node_streamon,
	.vidioc_streamoff = pispbe_node_streamoff,
};

static const struct video_device pispbe_videodev = {
	.name = PISPBE_NAME,
	.vfl_dir = VFL_DIR_M2M, /* gets overwritten */
	.fops = &pispbe_fops,
	.ioctl_ops = &pispbe_node_ioctl_ops,
	.minor = -1,
	.release = video_device_release_empty,
};

static void node_set_default_format(struct pispbe_node *node)
{
	if (NODE_IS_META(node) && NODE_IS_OUTPUT(node)) {
		/* Config node */
		struct v4l2_format *f = &node->format;

		f->fmt.meta.dataformat = V4L2_META_FMT_RPI_BE_CFG;
		f->fmt.meta.buffersize = sizeof(struct pisp_be_tiles_config);
		f->type = node->buf_type;
	} else if (NODE_IS_META(node) && NODE_IS_CAPTURE(node)) {
		/* HOG output node */
		struct v4l2_format *f = &node->format;

		f->fmt.meta.dataformat = V4L2_PIX_FMT_RPI_BE;
		f->fmt.meta.buffersize = 1 << 20;
		f->type = node->buf_type;
	} else {
		struct v4l2_format f = {0};

		f.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420M;
		f.fmt.pix_mp.width = 1920;
		f.fmt.pix_mp.height = 1080;
		f.type = node->buf_type;
		try_format(&f, node);
		node->format = f;
	}
}

/*
 * Register a device node /dev/video<N> to go along with one of the PiSP Back
 * End's input or output nodes.
 */
static int register_node(struct platform_device *pdev, struct pispbe_node *node,
			 struct pispbe_node_group *node_group)
{
	struct video_device *vfd;
	int ret;

	mutex_init(&node->node_lock);
	node->buf_type = node_desc[node->id].buf_type;
	node->node_group = node_group;
	node->vfd = pispbe_videodev;
	node->open = 0;
	node->format.type = node->buf_type;

	vfd = &node->vfd;
	vfd->v4l2_dev = &node_group->pispbe->v4l2_dev;
	vfd->vfl_dir = NODE_IS_OUTPUT(node) ? VFL_DIR_TX : VFL_DIR_RX;
	vfd->lock = &node->node_lock; /* get V4L2 to serialise our ioctls */
	vfd->v4l2_dev = &node_group->pispbe->v4l2_dev;
	vfd->queue = &node->queue;
	vfd->device_caps = V4L2_CAP_STREAMING | node_desc[node->id].caps;

	mutex_init(&node->queue_lock);
	INIT_LIST_HEAD(&node->ready_queue);
	spin_lock_init(&node->ready_lock);

	node_set_default_format(node);

	ret = video_register_device(vfd, VFL_TYPE_VIDEO,
				    PISPBE_VIDEO_NODE_OFFSET);
	if (ret) {
		v4l2_err(&node_group->pispbe->v4l2_dev,
			 "Failed to register video %s device node\n",
			 NODE_NAME(node));
		return ret;
	}
	video_set_drvdata(vfd, node);
	snprintf(vfd->name, sizeof(vfd->name), "%s", pispbe_videodev.name);
	v4l2_info(&node_group->pispbe->v4l2_dev,
		  "%s device node registered as /dev/video%d\n",
		  NODE_NAME(node), vfd->num);
	return 0;
}

/* Unregister one of the /dev/video<N> nodes associated with the PiSP Back End. */
static void pisp_unregister_node(struct pispbe_node *node)
{
	v4l2_info(&node_get_pispbe(node)->v4l2_dev,
		  "Unregistering " PISPBE_NAME " %s device node /dev/video%d\n",
		  NODE_NAME(node), node->vfd.num);
	video_unregister_device(&node->vfd);
}

/* Unregister the group of /dev/video<N> nodes that make up a single user of the PiSP Back End. */
static void unregister_node_group(struct pispbe_node_group *node_group, int num)
{
	unsigned int i;

	for (i = 0; i < num; i++)
		pisp_unregister_node(&node_group->node[i]);
}

static void
media_controller_unregister_node_group(struct pispbe_node_group *node_group,
				       int group, int num)
{
	int i;

	v4l2_info(&node_group->pispbe->v4l2_dev,
		  "Unregister node group %p from media controller\n",
		  node_group);

	kfree(node_group->entity.name);
	node_group->entity.name = NULL;

	if (group)
		media_device_unregister_entity(&node_group->entity);

	for (i = 0; i < num; i++) {
		media_remove_intf_links(node_group->node[i].intf_link->intf);
		media_entity_remove_links(&node_group->node[i].vfd.entity);
		media_devnode_remove(node_group->node[i].intf_devnode);
		media_device_unregister_entity(&node_group->node[i].vfd.entity);
		kfree(node_group->node[i].vfd.entity.name);
	}
}

static void media_controller_unregister(struct pispbe_dev *pispbe)
{
	int i;

	v4l2_info(&pispbe->v4l2_dev, "Unregister from media controller\n");
	media_device_unregister(&pispbe->mdev);

	for (i = 0; i < PISPBE_NUM_NODE_GROUPS; i++)
		media_controller_unregister_node_group(&pispbe->node_group[i],
						       1, PISPBE_NUM_NODES);

	media_device_cleanup(&pispbe->mdev);
	pispbe->v4l2_dev.mdev = NULL;
}

static int media_controller_register_node(struct pispbe_node_group *node_group,
					  int i, int group_num)
{
	struct pispbe_node *node = &node_group->node[i];
	struct media_entity *entity = &node->vfd.entity;
	int ret;
	char *name;
	char const *node_name = NODE_NAME(node);
	int output = NODE_IS_OUTPUT(node);

	v4l2_info(&node_group->pispbe->v4l2_dev,
		  "Register %s node %d with media controller\n", node_name, i);
	entity->obj_type = MEDIA_ENTITY_TYPE_VIDEO_DEVICE;
	entity->function = MEDIA_ENT_F_IO_V4L;
	entity->info.dev.major = VIDEO_MAJOR;
	entity->info.dev.minor = node->vfd.minor;
	name = kmalloc(PISPBE_ENTITY_NAME_LEN, GFP_KERNEL);
	if (name == NULL) {
		ret = -ENOMEM;
		goto error_no_mem;
	}
	snprintf(name, PISPBE_ENTITY_NAME_LEN, "%s-%s", node->vfd.name,
		 node_name);
	entity->name = name;
	node->pad.flags = output ? MEDIA_PAD_FL_SOURCE : MEDIA_PAD_FL_SINK;
	ret = media_entity_pads_init(entity, 1, &node->pad);
	if (ret)
		goto error_pads_init;
	ret = media_device_register_entity(&node_group->pispbe->mdev, entity);
	if (ret)
		goto error_register_entity;

	node->intf_devnode = media_devnode_create(&node_group->pispbe->mdev,
						  MEDIA_INTF_T_V4L_VIDEO, 0,
						  VIDEO_MAJOR, node->vfd.minor);
	if (node->intf_devnode == NULL) {
		ret = -ENOMEM;
		goto error_devnode_create;
	}

	node->intf_link = media_create_intf_link(
		entity, &node->intf_devnode->intf,
		MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED);
	if (node->intf_link == NULL) {
		ret = -ENOMEM;
		goto error_create_intf_link;
	}

	if (output)
		ret = media_create_pad_link(entity, 0, &node_group->entity, i,
					    MEDIA_LNK_FL_IMMUTABLE |
						    MEDIA_LNK_FL_ENABLED);
	else
		ret = media_create_pad_link(&node_group->entity, i, entity, 0,
					    MEDIA_LNK_FL_IMMUTABLE |
						    MEDIA_LNK_FL_ENABLED);
	if (ret)
		goto error_create_pad_link;

	return 0;

error_create_pad_link:
	media_remove_intf_links(&node->intf_devnode->intf);
error_create_intf_link:
	media_devnode_remove(node->intf_devnode);
error_devnode_create:
error_register_entity:
error_pads_init:
	kfree(entity->name);
	entity->name = NULL;
error_no_mem:
	if (ret)
		v4l2_err(&node_group->pispbe->v4l2_dev,
			 "Error registering node\n");
	return ret;
}

static int pispbe_request_validate(struct media_request *req)
{
	/* Is there any else we need to do here? */
	return vb2_request_validate(req);
}

static void pispbe_request_queue(struct media_request *req)
{
	/* Is there any else we need to do here? */
	vb2_request_queue(req);
}

static const struct media_device_ops pispbe_media_ops = {
	.req_validate = pispbe_request_validate,
	.req_queue = pispbe_request_queue,
};

static int media_controller_register(struct pispbe_dev *pispbe)
{
	int num_registered = 0;
	int num_groups_registered = 0;
	int group_registered = 0;
	int ret;
	int i;

	v4l2_info(&pispbe->v4l2_dev, "Registering with media controller\n");
	pispbe->mdev.dev = pispbe->dev;
	strscpy(pispbe->mdev.model, PISPBE_NAME, sizeof(pispbe->mdev.model));
	snprintf(pispbe->mdev.bus_info, sizeof(pispbe->mdev.bus_info),
		 "platform:%s", dev_name(pispbe->dev));
	media_device_init(&pispbe->mdev);
	pispbe->v4l2_dev.mdev = &pispbe->mdev;
	pispbe->mdev.ops = &pispbe_media_ops;

	for (; num_groups_registered < PISPBE_NUM_NODE_GROUPS;
	     num_groups_registered++) {
		struct pispbe_node_group *node_group =
			&pispbe->node_group[num_groups_registered];
		char *name = kmalloc(PISPBE_ENTITY_NAME_LEN, GFP_KERNEL);

		v4l2_info(&pispbe->v4l2_dev,
			  "Register entity for node group %d\n",
			  num_groups_registered);
		node_group->entity.name = name;
		if (name == NULL) {
			ret = -ENOMEM;
			goto done;
		}
		snprintf(name, PISPBE_ENTITY_NAME_LEN, PISPBE_NAME);
		node_group->entity.obj_type = MEDIA_ENTITY_TYPE_BASE;
		node_group->entity.function = MEDIA_ENT_F_PROC_VIDEO_SCALER;
		for (i = 0; i < PISPBE_NUM_NODES; i++)
			node_group->pad[i].flags =
				NODE_IS_OUTPUT(&node_group->node[i]) ?
					MEDIA_PAD_FL_SINK :
					MEDIA_PAD_FL_SOURCE;
		ret = media_entity_pads_init(&node_group->entity,
					     PISPBE_NUM_NODES, node_group->pad);
		if (ret)
			goto done;
		ret = media_device_register_entity(&pispbe->mdev,
						   &node_group->entity);
		if (ret)
			goto done;
		group_registered = 1;

		for (; num_registered < PISPBE_NUM_NODES; num_registered++) {
			ret = media_controller_register_node(
				node_group, num_registered,
				num_groups_registered);
			if (ret)
				goto done;
		}

		num_registered = 0;
		group_registered = 0;
	}

	ret = media_device_register(&pispbe->mdev);
	if (ret)
		goto done;

done:
	if (ret) {
		if (num_groups_registered < PISPBE_NUM_NODE_GROUPS)
			media_controller_unregister_node_group(
				&pispbe->node_group[num_groups_registered],
				group_registered, num_registered);
		while (--num_groups_registered >= 0)
			media_controller_unregister_node_group(
				&pispbe->node_group[num_groups_registered], 1,
				PISPBE_NUM_NODES);
	}

	return ret;
}

static int pispbe_probe(struct platform_device *pdev)
{
	struct pispbe_dev *pispbe;
	int ret;
	int num_registered = 0;
	int num_groups_registered = 0;

	pispbe = devm_kzalloc(&pdev->dev, sizeof(*pispbe), GFP_KERNEL);
	if (!pispbe)
		return -ENOMEM;

	pispbe->dev = &pdev->dev;
	ret = v4l2_device_register(&pdev->dev, &pispbe->v4l2_dev);
	if (ret)
		return ret;

	pispbe->be_reg_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pispbe->be_reg_base)) {
		dev_err(&pdev->dev, "Failed to get ISP-BE registers address\n");
		ret = PTR_ERR(pispbe->be_reg_base);
		goto done;
	}

	/* TODO: Enable clock only when running (and local RAMs too!) */
	pispbe->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pispbe->clk)) {
		dev_err(&pdev->dev, "Failed to get clock");
		ret = PTR_ERR(pispbe->clk);
		goto done;
	}
	ret = clk_prepare_enable(pispbe->clk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable clock\n");
		ret = -EINVAL;
		goto done;
	}
	dev_info(&pdev->dev, "%s: Enabled clock, rate=%lu\n",
	       __func__, clk_get_rate(pispbe->clk));

	pispbe->irq = platform_get_irq(pdev, 0);
	if (pispbe->irq <= 0) {
		dev_err(&pdev->dev, "No IRQ resource\n");
		ret = -EINVAL;
		goto done;
	}

	/* Hardware initialisation */
	pispbe->hw_busy = 0;
	spin_lock_init(&pispbe->hw_lock);
	spin_lock_init(&pispbe->isr_lock);
	spin_lock_init(&pispbe->hwq_lock);
	ret = hw_init(pispbe);
	if (ret)
		goto done;

	/* Enable interrupt */
	ret = devm_request_irq(&pdev->dev, pispbe->irq, pispbe_isr, 0,
			       PISPBE_NAME, pispbe);
	if (ret) {
		dev_err(&pdev->dev, "Unable to request interrupt\n");
		ret = -EINVAL;
		goto done;
	}

	/* Register lots of nodes */
	for (; num_groups_registered < PISPBE_NUM_NODE_GROUPS;
	     num_groups_registered++) {
		struct pispbe_node_group *node_group =
			&pispbe->node_group[num_groups_registered];
		node_group->pispbe = pispbe;
		v4l2_info(&pispbe->v4l2_dev, "Register nodes for group %d\n",
			  num_groups_registered);

		for (; num_registered < PISPBE_NUM_NODES; num_registered++) {
			node_group->node[num_registered].id = num_registered;
			ret = register_node(pdev,
					    &node_group->node[num_registered],
					    node_group);
			if (ret)
				goto done;
		}

		node_group->num_streaming = 0;

		num_registered = 0;
	}

	ret = media_controller_register(pispbe);
	if (ret)
		goto done;

	ret = dma_set_mask_and_coherent(pispbe->dev, DMA_BIT_MASK(36));
	if (ret)
		goto done;

	platform_set_drvdata(pdev, pispbe);

done:
	dev_info(&pdev->dev, "%s: returning %d", __func__, ret);
	if (ret) {
		if (num_groups_registered < PISPBE_NUM_NODE_GROUPS)
			unregister_node_group(
				&pispbe->node_group[num_groups_registered],
				num_registered);
		while (--num_groups_registered >= 0)
			unregister_node_group(
				&pispbe->node_group[num_groups_registered],
				PISPBE_NUM_NODES);

		media_device_cleanup(&pispbe->mdev);
		pispbe->v4l2_dev.mdev = NULL;

		v4l2_device_unregister(&pispbe->v4l2_dev);
	}

	return ret;
}

static int pispbe_remove(struct platform_device *pdev)
{
	struct pispbe_dev *pispbe;
	unsigned int i;

	pispbe = platform_get_drvdata(pdev);
	media_controller_unregister(pispbe);

	for (i = 0; i < PISPBE_NUM_NODE_GROUPS; i++)
		unregister_node_group(&pispbe->node_group[i], PISPBE_NUM_NODES);

	v4l2_device_unregister(&pispbe->v4l2_dev);

	return 0;
}

static const struct of_device_id pispbe_of_match[] = {
	{
		.compatible = "raspberrypi,pispbe",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, pispbe_of_match);

static struct platform_driver pispbe_pdrv = {
	.probe		= pispbe_probe,
	.remove		= pispbe_remove,
	.driver		= {
		.name	= PISPBE_NAME,
		.of_match_table = of_match_ptr(pispbe_of_match),
	},
};

module_platform_driver(pispbe_pdrv);
