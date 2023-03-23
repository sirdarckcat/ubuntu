// SPDX-License-Identifier: GPL-2.0
/*
 * PiSP Front End driver.
 * Copyright (c) 2021 Raspberry Pi Ltd.
 *
 */
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/seq_file.h>
#include <media/videobuf2-dma-contig.h>

#include "pisp_fe.h"
#include "pisp_fe_config.h"

#define VERSION		0x000
#define CONTROL		0x004
#define STATUS		0x008
#define FRAME_STATUS	0x00c
#define ERROR_STATUS	0x010
#define OUTPUT_STATUS	0x014
#define INT_EN		0x018
#define INT_STATUS	0x01c

/* CONTROL */
#define QUEUE		BIT(0)
#define ABORT		BIT(1)
#define RESET		BIT(2)
#define LATCH_REGS	BIT(3)

/* INT_EN / INT_STATUS */
#define EOF		BIT(0)
#define SOF		BIT(1)
#define LINES0		BIT(8)
#define LINES1		BIT(9)
#define STATS		BIT(16)
#define QREADY		BIT(24)

/* STATUS */
#define QUEUED		BIT(0)
#define WAITING		BIT(1)
#define ACTIVE		BIT(2)

#define PISP_FE_CONFIG_BASE_OFFSET	0x0040

static int pisp_fe_debug;
module_param(pisp_fe_debug, int, 0644);
MODULE_PARM_DESC(pisp_fe_debug, "Debug level 0-3");

struct pisp_fe_config_param {
	u32 dirty_flags;
	u32 dirty_flags_extra;
	size_t offset;
	size_t size;
};

static const struct pisp_fe_config_param pisp_fe_config_map[] = {
	/* *_dirty_flag_extra types */
	{ 0, PISP_FE_DIRTY_GLOBAL,     offsetof(struct pisp_fe_config, global),           sizeof(struct pisp_fe_global_config)         },
	{ 0, PISP_FE_DIRTY_FLOATING,   offsetof(struct pisp_fe_config, floating_stats),   sizeof(struct pisp_fe_floating_stats_config) },
	{ 0, PISP_FE_DIRTY_OUTPUT_AXI, offsetof(struct pisp_fe_config, output_axi),       sizeof(struct pisp_fe_output_axi_config)     },
	/* *_dirty_flag types */
	{ PISP_FE_ENABLE_INPUT,      0, offsetof(struct pisp_fe_config, input),           sizeof(struct pisp_fe_input_config)          },
	{ PISP_FE_ENABLE_DECOMPRESS, 0, offsetof(struct pisp_fe_config, decompress),      sizeof(struct pisp_decompress_config)        },
	{ PISP_FE_ENABLE_DECOMPAND,  0, offsetof(struct pisp_fe_config, decompand),       sizeof(struct pisp_fe_decompand_config)      },
	{ PISP_FE_ENABLE_BLA,        0, offsetof(struct pisp_fe_config, bla),             sizeof(struct pisp_bla_config)               },
	{ PISP_FE_ENABLE_DPC,        0, offsetof(struct pisp_fe_config, dpc),             sizeof(struct pisp_fe_dpc_config)            },
	{ PISP_FE_ENABLE_STATS_CROP, 0, offsetof(struct pisp_fe_config, stats_crop),      sizeof(struct pisp_fe_crop_config)           },
	{ PISP_FE_ENABLE_BLC,	     0, offsetof(struct pisp_fe_config, blc),             sizeof(struct pisp_bla_config)               },
	{ PISP_FE_ENABLE_CDAF_STATS, 0, offsetof(struct pisp_fe_config, cdaf_stats),      sizeof(struct pisp_fe_cdaf_stats_config)     },
	{ PISP_FE_ENABLE_AWB_STATS,  0, offsetof(struct pisp_fe_config, awb_stats),       sizeof(struct pisp_fe_awb_stats_config)      },
	{ PISP_FE_ENABLE_RGBY,       0, offsetof(struct pisp_fe_config, rgby),            sizeof(struct pisp_fe_rgby_config)           },
	{ PISP_FE_ENABLE_LSC,        0, offsetof(struct pisp_fe_config, lsc),             sizeof(struct pisp_fe_lsc_config)            },
	{ PISP_FE_ENABLE_AGC_STATS,  0, offsetof(struct pisp_fe_config, agc_stats),       sizeof(struct pisp_agc_statistics)           },
	{ PISP_FE_ENABLE_CROP0,      0, offsetof(struct pisp_fe_config, ch[0].crop),      sizeof(struct pisp_fe_crop_config)           },
	{ PISP_FE_ENABLE_DOWNSCALE0, 0, offsetof(struct pisp_fe_config, ch[0].downscale), sizeof(struct pisp_fe_downscale_config)      },
	{ PISP_FE_ENABLE_COMPRESS0,  0, offsetof(struct pisp_fe_config, ch[0].compress),  sizeof(struct pisp_compress_config)          },
	{ PISP_FE_ENABLE_OUTPUT0,    0, offsetof(struct pisp_fe_config, ch[0].output),    sizeof(struct pisp_fe_output_config)         },
	{ PISP_FE_ENABLE_CROP1,      0, offsetof(struct pisp_fe_config, ch[1].crop),      sizeof(struct pisp_fe_crop_config)           },
	{ PISP_FE_ENABLE_DOWNSCALE1, 0, offsetof(struct pisp_fe_config, ch[1].downscale), sizeof(struct pisp_fe_downscale_config)      },
	{ PISP_FE_ENABLE_COMPRESS1,  0, offsetof(struct pisp_fe_config, ch[1].compress),  sizeof(struct pisp_compress_config)          },
	{ PISP_FE_ENABLE_OUTPUT1,    0, offsetof(struct pisp_fe_config, ch[1].output),    sizeof(struct pisp_fe_output_config)         },
};

#define pisp_fe_dbg(level, fmt, arg...)	\
		    v4l2_dbg(level, pisp_fe_debug, fe->v4l2_dev, fmt, ##arg)
#define pisp_fe_info(fmt, arg...)	\
		     v4l2_info(fe->v4l2_dev, fmt, ##arg)
#define pisp_fe_err(fmt, arg...)	\
		    v4l2_err(fe->v4l2_dev, fmt, ##arg)

static inline u32 pisp_fe_reg_read(struct pisp_fe_device *fe, u32 offset)
{
	return readl(fe->base + offset);
}

static inline void pisp_fe_reg_write(struct pisp_fe_device *fe, u32 offset,
				     u32 val)
{
	writel(val, fe->base + offset);
	pisp_fe_dbg(3, "fe: write 0x%04x -> 0x%03x\n", val, offset);
}

static inline void pisp_fe_reg_write_relaxed(struct pisp_fe_device *fe, u32 offset,
					     u32 val)
{
	writel_relaxed(val, fe->base + offset);
	pisp_fe_dbg(3, "fe: write 0x%04x -> 0x%03x\n", val, offset);
}

static int pisp_regs_show(struct seq_file *s, void *data)
{
	struct pisp_fe_device *fe = s->private;

	pisp_fe_reg_write(fe, CONTROL, LATCH_REGS);

#define DUMP(reg) seq_printf(s, #reg " \t0x%08x\n", pisp_fe_reg_read(fe, reg))
	DUMP(VERSION);
	DUMP(CONTROL);
	DUMP(STATUS);
	DUMP(FRAME_STATUS);
	DUMP(ERROR_STATUS);
	DUMP(OUTPUT_STATUS);
	DUMP(INT_EN);
	DUMP(INT_STATUS);
#undef DUMP

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(pisp_regs);

static void pisp_config_write(struct pisp_fe_device *fe,
			      struct pisp_fe_config *config,
			      unsigned int start_offset,
			      unsigned int size)
{
	const unsigned int max_offset =
		offsetof(struct pisp_fe_config, ch[PISP_FE_NUM_OUTPUTS]);
	unsigned int i, end_offset;
	u32 *cfg = (u32 *)config;

	start_offset = min(start_offset, max_offset);
	end_offset = min(start_offset + size, max_offset);

	cfg += start_offset >> 2;
	for (i = start_offset; i < end_offset; i += 4, cfg++)
		pisp_fe_reg_write_relaxed(fe, PISP_FE_CONFIG_BASE_OFFSET + i,
					  *cfg);
}

inline void pisp_fe_isr(struct pisp_fe_device *fe, bool *sof, bool *eof)
{
	u32 status, int_status, out_status, frame_status, error_status;
	unsigned int i;

	pisp_fe_reg_write(fe, CONTROL, LATCH_REGS);
	status = pisp_fe_reg_read(fe, STATUS);
	out_status = pisp_fe_reg_read(fe, OUTPUT_STATUS);
	frame_status = pisp_fe_reg_read(fe, FRAME_STATUS);
	error_status = pisp_fe_reg_read(fe, ERROR_STATUS);

	int_status = pisp_fe_reg_read(fe, INT_STATUS);
	pisp_fe_reg_write(fe, INT_STATUS, int_status);

	pisp_fe_dbg(3, "%s: status 0x%x out_status 0x%x frame_status 0x%x error_status 0x%x int_status 0x%x\n",
		    __func__, status, out_status, frame_status, error_status,
		    int_status);

	/* We do not report interrupts for the input/stream pad. */
	for (i = 0; i < FE_NUM_PADS - 1; i++) {
		sof[i] = !!(int_status & SOF);
		eof[i] = !!(int_status & EOF);
	}
}

void pisp_fe_submit_job(struct pisp_fe_device *fe, struct vb2_buffer **vb2_bufs,
			struct v4l2_format *f)
{
	struct pisp_fe_config *cfg;
	unsigned int i;
	dma_addr_t addr;
	u32 status;

	if (WARN_ON(!vb2_bufs[FE_CONFIG_PAD])) {
		pisp_fe_err("%s: No config buffer provided, cannot run.",
			    __func__);
		return;
	}

	cfg = vb2_plane_vaddr(vb2_bufs[FE_CONFIG_PAD], 0);

	/* Buffer config. */
	if (vb2_bufs[FE_OUTPUT0_PAD]) {
		addr = vb2_dma_contig_plane_dma_addr(vb2_bufs[FE_OUTPUT0_PAD],
						     0);
		cfg->output_buffer[0].addr_lo = addr & 0xffffffff;
		cfg->output_buffer[0].addr_hi = addr >> 32;
	}
	if (vb2_bufs[FE_OUTPUT1_PAD]) {
		addr = vb2_dma_contig_plane_dma_addr(vb2_bufs[FE_OUTPUT1_PAD],
						     0);
		cfg->output_buffer[1].addr_lo = addr & 0xffffffff;
		cfg->output_buffer[1].addr_hi = addr >> 32;
	}
	if (vb2_bufs[FE_STATS_PAD]) {
		addr = vb2_dma_contig_plane_dma_addr(vb2_bufs[FE_STATS_PAD], 0);
		cfg->stats_buffer.addr_lo = addr & 0xffffffff;
		cfg->stats_buffer.addr_hi = addr >> 32;
	}

	/* Neither dimension can be zero, or the HW will lockup! */
	BUG_ON(!f->fmt.pix.width || !f->fmt.pix.height);

	/* Input dimensions. */
	cfg->input.format.width = f->fmt.pix.width;
	cfg->input.format.height = f->fmt.pix.height;

	/* Output dimensions. */
	cfg->ch[0].output.format.width = f->fmt.pix.width;
	cfg->ch[0].output.format.height = f->fmt.pix.height;
	cfg->ch[0].output.format.stride = f->fmt.pix.bytesperline;
	cfg->ch[0].output.ilines = min(max(0x80u, f->fmt.pix.height >> 2),
				       f->fmt.pix.height);

	/* Output setup. */
	cfg->output_axi.maxlen_flags = 0x8f;

	pisp_fe_dbg(3, "%s: in: %dx%d out: %dx%d (stride: %d)\n", __func__,
		    cfg->input.format.width,
		    cfg->input.format.height,
		    cfg->ch[0].output.format.width,
		    cfg->ch[0].output.format.height,
		    cfg->ch[0].output.format.stride);

	status = pisp_fe_reg_read(fe, STATUS);
	pisp_fe_dbg(2, "%s: status = 0x%x\n", __func__, status);

	/* The hardware should have queued the previous config by now. */
	WARN_ON(status & QUEUED);

	/*
	 * Memory barrier before the calls to pisp_config_write as we do relaxed
	 * writes to the registers. The pisp_fe_reg_write() call at the end
	 * is a non-relaxed write, so will have an inherent wmb() call.
	 */
	wmb();

	/*
	 * Only selectively write the parameters that have been marked as
	 * changed through the dirty flags.
	 */
	for (i = 0; i < ARRAY_SIZE(pisp_fe_config_map); i++) {
		const struct pisp_fe_config_param *p = &pisp_fe_config_map[i];

		if (cfg->dirty_flags & p->dirty_flags ||
		    cfg->dirty_flags_extra & p->dirty_flags_extra)
			pisp_config_write(fe, cfg, p->offset, p->size);
	}

	/* Unconditionally write buffer, input, output parameters */
	pisp_config_write(fe, cfg, 0,
			  sizeof(cfg->stats_buffer) +
				sizeof(cfg->output_buffer) +
				sizeof(cfg->input_buffer));
	pisp_config_write(fe, cfg,
			  offsetof(struct pisp_fe_config, input) +
				offsetof(struct pisp_fe_input_config, format),
			  sizeof(cfg->input.format));
	pisp_config_write(fe, cfg,
			  offsetof(struct pisp_fe_config, ch[0]) +
			  offsetof(struct pisp_fe_output_branch_config, output),
			  sizeof(cfg->ch[0].output.format));

	pisp_fe_reg_write(fe, CONTROL, QUEUE);
}

void pisp_fe_start(struct pisp_fe_device *fe)
{
	pisp_fe_reg_write(fe, CONTROL, RESET);
	pisp_fe_reg_write(fe, INT_STATUS, -1);
	pisp_fe_reg_write(fe, INT_EN, EOF + SOF + LINES0 + LINES1);
	fe->inframe_count = 0;
}

void pisp_fe_stop(struct pisp_fe_device *fe)
{
	pisp_fe_reg_write(fe, INT_EN, 0);
	pisp_fe_reg_write(fe, CONTROL, ABORT);
	usleep_range(1000, 2000);
}

static struct pisp_fe_device *to_pisp_fe_device(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct pisp_fe_device, sd);
}

static int pisp_fe_pad_get_fmt(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *state,
			       struct v4l2_subdev_format *format)
{
	struct pisp_fe_device *fe = to_pisp_fe_device(sd);

	if (format->pad >= ARRAY_SIZE(fe->format))
		return -1;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		*format = fe->format[format->pad];

	return 0;
}

static int pisp_fe_pad_set_fmt(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *state,
			       struct v4l2_subdev_format *format)
{
	struct pisp_fe_device *fe = to_pisp_fe_device(sd);

	if (format->pad >= ARRAY_SIZE(fe->format))
		return -1;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		fe->format[format->pad] = *format;

	return 0;
}

static int pisp_fe_link_validate(struct v4l2_subdev *sd,
				 struct media_link *link,
				 struct v4l2_subdev_format *source_fmt,
				 struct v4l2_subdev_format *sink_fmt)
{
	struct pisp_fe_device *fe = to_pisp_fe_device(sd);

	pisp_fe_dbg(1, "%s: link \"%s\":%u -> \"%s\":%u\n", __func__,
		    link->source->entity->name, link->source->index,
		    link->sink->entity->name, link->sink->index);

	/* The width, height and code must match. */
	if (source_fmt->format.width != sink_fmt->format.width ||
	    source_fmt->format.width != sink_fmt->format.width ||
	    source_fmt->format.code != sink_fmt->format.code) {
		pisp_fe_err("%s: format does not match (source %ux%u 0x%x, sink %ux%u 0x%x)\n",
			    __func__,
			     source_fmt->format.width,
			     source_fmt->format.height,
			     source_fmt->format.code,
			     sink_fmt->format.width,
			     sink_fmt->format.height,
			     sink_fmt->format.code);
		return -EPIPE;
	}

	return 0;
}

static const struct v4l2_subdev_pad_ops pisp_fe_subdev_pad_ops = {
	.get_fmt = pisp_fe_pad_get_fmt,
	.set_fmt = pisp_fe_pad_set_fmt,
	.link_validate = pisp_fe_link_validate,
};

static const struct media_entity_operations pisp_fe_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_ops pisp_fe_subdev_ops = {
	.pad = &pisp_fe_subdev_pad_ops,
};

int pisp_fe_init(struct pisp_fe_device *fe, struct media_device *mdev,
		 struct dentry *debugfs)
{
	int ret;
	u32 ver;

	debugfs_create_file("pisp_regs", 0444, debugfs, fe, &pisp_regs_fops);

	ver = pisp_fe_reg_read(fe, VERSION);
	pisp_fe_info("PiSP FE HW v%u.%u\n",
		     (ver >> 24) & 0xff, (ver >> 20) & 0x0f);

	fe->pad[FE_STREAM_PAD].flags =
		MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_MUST_CONNECT;
	fe->pad[FE_CONFIG_PAD].flags = MEDIA_PAD_FL_SINK;
	fe->pad[FE_OUTPUT0_PAD].flags = MEDIA_PAD_FL_SOURCE;
	fe->pad[FE_OUTPUT1_PAD].flags = MEDIA_PAD_FL_SOURCE;
	fe->pad[FE_STATS_PAD].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&fe->sd.entity, ARRAY_SIZE(fe->pad),
				     fe->pad);
	if (ret)
		return ret;

	/* Initialize subdev, but register in the caller. */
	v4l2_subdev_init(&fe->sd, &pisp_fe_subdev_ops);
	fe->sd.entity.function = MEDIA_ENT_F_PROC_VIDEO_SCALER;
	fe->sd.entity.ops = &pisp_fe_entity_ops;
	fe->sd.entity.name = "pisp-fe";
	fe->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	fe->sd.owner = THIS_MODULE;
	snprintf(fe->sd.name, sizeof(fe->sd.name), "pisp-fe");

	pisp_fe_stop(fe);

	/* Must be in IDLE state (STATUS == 0) here. */
	WARN_ON(pisp_fe_reg_read(fe, STATUS));

	return 0;
}
