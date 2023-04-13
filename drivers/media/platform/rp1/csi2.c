// SPDX-License-Identifier: GPL-2.0-only
/*
 * RP1 CSI-2 Driver
 *
 * Copyright (C) 2021 - Raspberry Pi Ltd.
 *
 */

#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/seq_file.h>
#include <media/videobuf2-dma-contig.h>

#include "csi2.h"

static int csi2_debug;
module_param(csi2_debug, int, 0644);
MODULE_PARM_DESC(csi2_debug, "Debug level 0-3");

#define csi2_dbg(level, fmt, arg...)	\
		v4l2_dbg(level, csi2_debug, csi2->v4l2_dev, fmt, ##arg)
#define csi2_info(fmt, arg...)	\
		v4l2_info(csi2->v4l2_dev, fmt, ##arg)
#define csi2_err(fmt, arg...)	\
		v4l2_err(csi2->v4l2_dev, fmt, ##arg)

/* DW CSI2 Host registers */
#define VERSION		0x000
#define N_LANES		0x004
#define RESETN		0x008
#define PHY_SHUTDOWNZ	0x040
#define PHY_RSTZ	0x044
#define PHY_RX		0x048
#define	PHY_STOPSTATE	0x04c
#define PHY_TST_CTRL0	0x050
#define PHY_TST_CTRL1	0x054
#define PHY2_TST_CTRL0	0x058
#define PHY2_TST_CTRL1	0x05c

/* DW CSI2 Host Transactions */
#define DPHY_HS_RX_CTRL_LANE0_OFFSET	0x44
#define DPHY_PLL_INPUT_DIV_OFFSET	0x17
#define DPHY_PLL_LOOP_DIV_OFFSET	0x18
#define DPHY_PLL_DIV_CTRL_OFFSET	0x19

/* CSI2-DMA registers */
#define CSI2_STATUS		0x000
#define CSI2_QOS		0x004
#define CSI2_DISCARDS_OVERFLOW	0x008
#define CSI2_DISCARDS_INACTIVE	0x00c
#define CSI2_DISCARDS_UNMATCHED	0x010
#define CSI2_DISCARDS_LEN_LIMIT	0x014
#define CSI2_LLEV_PANICS	0x018
#define CSI2_ULEV_PANICS	0x01c
#define CSI2_IRQ_MASK		0x020
#define CSI2_CTRL		0x024
#define CSI2_CH_CTRL(x)		((x) * 0x40 + 0x28)
#define CSI2_CH_ADDR0(x)	((x) * 0x40 + 0x2c)
#define CSI2_CH_ADDR1(x)	((x) * 0x40 + 0x3c)
#define CSI2_CH_STRIDE(x)	((x) * 0x40 + 0x30)
#define CSI2_CH_LENGTH(x)	((x) * 0x40 + 0x34)
#define CSI2_CH_DEBUG(x)	((x) * 0x40 + 0x38)
#define CSI2_CH_FRAME_SIZE(x)	((x) * 0x40 + 0x40)
#define CSI2_CH_COMP_CTRL(x)	((x) * 0x40 + 0x44)
#define CSI2_CH_FE_FRAME_ID(x)	((x) * 0x40 + 0x48)

/* CSI2_STATUS */
#define IRQ_FS(x)		(BIT(0) << (x))
#define IRQ_FE(x)		(BIT(4) << (x))
#define IRQ_FE_ACK(x)		(BIT(8) << (x))
#define IRQ_LE(x)		(BIT(12) << (x))
#define IRQ_LE_ACK(x)		(BIT(16) << (x))
#define IRQ_OVERFLOW		BIT(20)
#define IRQ_DISCARD_OVERFLOW	BIT(21)
#define IRQ_DISCARD_LEN_LIMIT	BIT(22)
#define IRQ_DISCARD_UNMATCHED	BIT(23)
#define IRQ_DISCARD_INACTIVE	BIT(24)

/* CSI2_CTRL */
#define EOP_IS_EOL		BIT(0)

/* CSI2_CH_CTRL */
#define DMA_EN			BIT(0)
#define FORCE			BIT(3)
#define AUTO_ARM		BIT(4)
#define IRQ_EN_FS		BIT(13)
#define IRQ_EN_FE		BIT(14)
#define IRQ_EN_FE_ACK		BIT(15)
#define IRQ_EN_LE		BIT(16)
#define IRQ_EN_LE_ACK		BIT(17)
#define FLUSH_FE		BIT(28)
#define PACK_LINE		BIT(29)
#define PACK_BYTES		BIT(30)
#define CH_MODE_MASK		GENMASK(2, 1)
#define VC_MASK			GENMASK(6, 5)
#define DT_MASK			GENMASK(12, 7)
#define LC_MASK			GENMASK(27, 18)

/* CHx_COMPRESSION_CONTROL */
#define COMP_OFFSET_MASK	GENMASK(15, 0)
#define COMP_SHIFT_MASK		GENMASK(19, 16)
#define COMP_MODE_MASK		GENMASK(25, 24)

static inline u32 dw_csi2_host_read(struct csi2_device *csi2, u32 offset)
{
	u32 data = readl(csi2->host_base + offset);
	return data;
}

static inline void dw_csi2_host_write(struct csi2_device *csi2, u32 offset,
				      u32 data)
{
	writel(data, csi2->host_base + offset);
}

static inline u32 csi2_reg_read(struct csi2_device *csi2, u32 offset)
{
	u32 val = readl(csi2->base + offset);
	return val;
}

static inline void csi2_reg_write(struct csi2_device *csi2, u32 offset, u32 val)
{
	writel(val, csi2->base + offset);
	csi2_dbg(3, "csi2: write 0x%04x -> 0x%03x\n", val, offset);
}

static inline void set_field(u32 *valp, u32 field, u32 mask)
{
	u32 val = *valp;

	val &= ~mask;
	val |= (field << __ffs(mask)) & mask;
	*valp = val;
}

static int csi2_regs_show(struct seq_file *s, void *data)
{
	struct csi2_device *csi2 = s->private;

#define DUMP(reg) seq_printf(s, #reg " \t0x%08x\n", csi2_reg_read(csi2, reg))
	DUMP(CSI2_STATUS);
	DUMP(CSI2_DISCARDS_OVERFLOW);
	DUMP(CSI2_DISCARDS_INACTIVE);
	DUMP(CSI2_DISCARDS_UNMATCHED);
	DUMP(CSI2_DISCARDS_LEN_LIMIT);
	DUMP(CSI2_LLEV_PANICS);
	DUMP(CSI2_ULEV_PANICS);
	DUMP(CSI2_IRQ_MASK);
	DUMP(CSI2_CTRL);
	DUMP(CSI2_CH_CTRL(0));
	DUMP(CSI2_CH_DEBUG(0));
	DUMP(CSI2_CH_FRAME_SIZE(0));
	DUMP(CSI2_CH_CTRL(1));
	DUMP(CSI2_CH_DEBUG(1));
#undef DUMP

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(csi2_regs);

static inline void set_tstclr(struct csi2_device *csi2, u32 val)
{
	u32 ctrl0 = dw_csi2_host_read(csi2, PHY_TST_CTRL0);

	dw_csi2_host_write(csi2, PHY_TST_CTRL0, (ctrl0 & ~1) | val);
}

static inline void set_tstclk(struct csi2_device *csi2, u32 val)
{
	u32 ctrl0 = dw_csi2_host_read(csi2, PHY_TST_CTRL0);

	dw_csi2_host_write(csi2, PHY_TST_CTRL0, (ctrl0 & ~2) | (val << 1));
}

static inline uint8_t get_tstdout(struct csi2_device *csi2)
{
	u32 ctrl1 = dw_csi2_host_read(csi2, PHY_TST_CTRL1);

	return ((ctrl1 >> 8) & 0xff);
}

static inline void set_testen(struct csi2_device *csi2, u32 val)
{
	u32 ctrl1 = dw_csi2_host_read(csi2, PHY_TST_CTRL1);

	dw_csi2_host_write(csi2, PHY_TST_CTRL1,
			   (ctrl1 & ~(1 << 16)) | (val << 16));
}

static inline void set_testdin(struct csi2_device *csi2, u32 val)
{
	u32 ctrl1 = dw_csi2_host_read(csi2, PHY_TST_CTRL1);

	dw_csi2_host_write(csi2, PHY_TST_CTRL1, (ctrl1 & ~0xff) | val);
}

static uint8_t dphy_transaction(struct csi2_device *csi2,
				u8 test_code, uint8_t test_data)
{
	/* See page 101 of the MIPI DPHY databook. */
	set_tstclk(csi2, 1);
	set_testen(csi2, 0);
	set_testdin(csi2, test_code);
	set_testen(csi2, 1);
	set_tstclk(csi2, 0);
	set_testen(csi2, 0);
	set_testdin(csi2, test_data);
	set_tstclk(csi2, 1);
	return get_tstdout(csi2);
}

static void dphy_set_hsfreqrange(struct csi2_device *csi2, uint32_t freq_mhz)
{
	/* See Table 5-1 on page 65 of dphy databook */
	static const u16 hsfreqrange_table[][2] = {
		{   89, 0b000000 },
		{   99, 0b010000 },
		{  109, 0b100000 },
		{  129, 0b000001 },
		{  139, 0b010001 },
		{  149, 0b100001 },
		{  169, 0b000010 },
		{  179, 0b010010 },
		{  199, 0b100010 },
		{  219, 0b000011 },
		{  239, 0b010011 },
		{  249, 0b100011 },
		{  269, 0b000100 },
		{  299, 0b010100 },
		{  329, 0b000101 },
		{  359, 0b010101 },
		{  399, 0b100101 },
		{  449, 0b000110 },
		{  499, 0b010110 },
		{  549, 0b000111 },
		{  599, 0b010111 },
		{  649, 0b001000 },
		{  699, 0b011000 },
		{  749, 0b001001 },
		{  799, 0b011001 },
		{  849, 0b101001 },
		{  899, 0b111001 },
		{  949, 0b001010 },
		{  999, 0b011010 },
		{ 1049, 0b101010 },
		{ 1099, 0b111010 },
		{ 1149, 0b001011 },
		{ 1199, 0b011011 },
		{ 1249, 0b101011 },
		{ 1299, 0b111011 },
		{ 1349, 0b001100 },
		{ 1399, 0b011100 },
		{ 1449, 0b101100 },
		{ 1500, 0b111100 },
	};
	unsigned int i;

	if (freq_mhz < 80 || freq_mhz > 1500)
		csi2_err("DPHY: Frequency %u MHz out of range\n", freq_mhz);

	for (i = 0; i < ARRAY_SIZE(hsfreqrange_table) - 1; i++) {
		if (freq_mhz <= hsfreqrange_table[i][0])
			break;
	}

	dphy_transaction(csi2, DPHY_HS_RX_CTRL_LANE0_OFFSET,
			 hsfreqrange_table[i][1] << 1);
}

static void dphy_init(struct csi2_device *csi2)
{
	dw_csi2_host_write(csi2, PHY_RSTZ, 0);
	dw_csi2_host_write(csi2, PHY_SHUTDOWNZ, 0);
	set_tstclk(csi2, 1);
	set_testen(csi2, 0);
	set_tstclr(csi2, 1);
	usleep_range(15, 20);
	set_tstclr(csi2, 0);
	usleep_range(15, 20);

	dphy_set_hsfreqrange(csi2, csi2->dphy_freq);

	usleep_range(5, 10);
	dw_csi2_host_write(csi2, PHY_SHUTDOWNZ, 1);
	usleep_range(5, 10);
	dw_csi2_host_write(csi2, PHY_RSTZ, 1);
}

inline void csi2_isr(struct csi2_device *csi2, bool *sof, bool *eof, bool *lci)
{
	unsigned int i;
	u32 status;

	status = csi2_reg_read(csi2, CSI2_STATUS);
	csi2_dbg(3, "ISR: STA: 0x%x\n", status);

	/* Write value back to clear the interrupts */
	csi2_reg_write(csi2, CSI2_STATUS, status);

	for (i = 0; i < csi2->num_lanes; i++) {
		u32 dbg = csi2_reg_read(csi2, CSI2_CH_DEBUG(i));

		csi2_dbg(3, "ISR: [%d], frame: %d line: %d\n",
			 i, dbg >> 16,
			 csi2->num_lines[i] ?
				((dbg & 0xffff) % csi2->num_lines[i]) : 0);

		sof[i] = !!(status & IRQ_FS(i));
		eof[i] = !!(status & IRQ_FE_ACK(i));
		lci[i] = !!(status & IRQ_LE_ACK(i));
	}
}

void csi2_set_buffer(struct csi2_device *csi2, unsigned int channel,
		     dma_addr_t dmaaddr, unsigned int stride, unsigned int size)
{
	u64 addr = dmaaddr;
	/*
	 * ADDRESS0 must be written last as it triggers the double buffering
	 * mechanism for all buffer registers within the hardware.
	 */
	addr >>= 4;
	csi2_reg_write(csi2, CSI2_CH_LENGTH(channel), size >> 4);
	csi2_reg_write(csi2, CSI2_CH_STRIDE(channel), stride >> 4);
	csi2_reg_write(csi2, CSI2_CH_ADDR1(channel), addr >> 32);
	csi2_reg_write(csi2, CSI2_CH_ADDR0(channel), addr & 0xffffffff);
}

void csi2_set_compression(struct csi2_device *csi2, unsigned int channel,
			  unsigned int mode, unsigned int shift,
			  unsigned int offset)
{
	u32 compression = 0;

	set_field(&compression, COMP_OFFSET_MASK, offset);
	set_field(&compression, COMP_SHIFT_MASK, shift);
	set_field(&compression, COMP_MODE_MASK, mode);
	csi2_reg_write(csi2, CSI2_CH_COMP_CTRL(channel), compression);
}

void csi2_start_channel(struct csi2_device *csi2, unsigned int channel,
			u16 dt, enum csi2_mode mode, bool auto_arm,
			bool pack_bytes, unsigned int width,
			unsigned int height)
{
	u32 ctrl;

	csi2_dbg(3, "%s [%d]\n", __func__, channel);

	/*
	 * Disable the channel, but ensure N != 0!  Otherwise we end up with a
	 * spurious LE + LE_ACK interrupt when re-enabling the channel.
	 */
	csi2_reg_write(csi2, CSI2_CH_CTRL(channel), 0x100 << __ffs(LC_MASK));
	csi2_reg_write(csi2, CSI2_CH_DEBUG(channel), 0);
	csi2_reg_write(csi2, CSI2_STATUS, IRQ_FS(channel) +
		       IRQ_FE_ACK(channel) + IRQ_LE_ACK(channel));

	/* Enable channel and FS/FE/LE interrupts. */
	ctrl = DMA_EN + IRQ_EN_FS + IRQ_EN_FE_ACK + IRQ_EN_LE_ACK + PACK_LINE;
	/* PACK_BYTES ensures no striding for embedded data. */
	if (pack_bytes)
		ctrl |= PACK_BYTES;

	if (auto_arm)
		ctrl += AUTO_ARM;

	if (width && height) {
		int line_int_freq = height >> 2;

		line_int_freq = min(max(0x80, line_int_freq), 0x3ff);
		set_field(&ctrl, line_int_freq, LC_MASK);
		set_field(&ctrl, mode, CH_MODE_MASK);
		csi2_reg_write(csi2, CSI2_CH_FRAME_SIZE(channel),
			       (height << 16) | width);
	} else {
		/*
		 * Do not disable line interrupts for the embedded data channel,
		 * set it to the maximum value.  This avoids spamming the ISR
		 * with spurious line interrupts.
		 */
		set_field(&ctrl, 0x3ff, LC_MASK);
		set_field(&ctrl, 0x00, CH_MODE_MASK);
	}

	set_field(&ctrl, dt, DT_MASK);
	csi2_reg_write(csi2, CSI2_CH_CTRL(channel), ctrl);
	csi2->num_lines[channel] = height;
}

void csi2_stop_channel(struct csi2_device *csi2, unsigned int channel)
{
	csi2_dbg(3, "%s [%d]\n", __func__, channel);

	/* Channel disable.  Use FORCE to allow stopping mid-frame. */
	csi2_reg_write(csi2, CSI2_CH_CTRL(channel),
		       (0x100 << __ffs(LC_MASK)) + FORCE);
	/* Latch the above change by writing to the ADDR0 register. */
	csi2_reg_write(csi2, CSI2_CH_ADDR0(channel), 0);
	/* Write this again, the HW needs it! */
	csi2_reg_write(csi2, CSI2_CH_ADDR0(channel), 0);
}

void csi2_open_rx(struct csi2_device *csi2)
{
	dw_csi2_host_write(csi2, N_LANES, (csi2->num_lanes - 1));
	dphy_init(csi2);
	dw_csi2_host_write(csi2, RESETN, 0xffffffff);
	usleep_range(10, 50);

	if (!csi2->multipacket_line)
		csi2_reg_write(csi2, CSI2_CTRL, EOP_IS_EOL);
}

void csi2_close_rx(struct csi2_device *csi2)
{
	/* Set only one lane (lane 0) as active (ON) */
	dw_csi2_host_write(csi2, N_LANES, 0);
	dw_csi2_host_write(csi2, RESETN, 0);
}

static struct csi2_device *to_csi2_device(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct csi2_device, sd);
}

static int csi2_pad_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *state,
			    struct v4l2_subdev_format *format)
{
	struct csi2_device *csi2 = to_csi2_device(sd);

	if (format->pad >= ARRAY_SIZE(csi2->format))
		return -1;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		*format = csi2->format[format->pad];

	return 0;
}

static int csi2_pad_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *state,
			    struct v4l2_subdev_format *format)
{
	struct csi2_device *csi2 = to_csi2_device(sd);

	if (format->pad >= ARRAY_SIZE(csi2->format))
		return -1;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		csi2->format[format->pad] = *format;

	return 0;
}

static int csi2_link_validate(struct v4l2_subdev *sd, struct media_link *link,
			      struct v4l2_subdev_format *source_fmt,
			      struct v4l2_subdev_format *sink_fmt)
{
	struct csi2_device *csi2 = to_csi2_device(sd);

	csi2_dbg(1, "%s: link \"%s\":%u -> \"%s\":%u\n", __func__,
		 link->source->entity->name, link->source->index,
		 link->sink->entity->name, link->sink->index);

	if ((link->source->entity == &csi2->sd.entity &&
	     link->source->index == 1) ||
	    (link->sink->entity == &csi2->sd.entity &&
	     link->sink->index == 1)) {
		csi2_dbg(1, "Ignore metadata pad for now\n");
		return 0;
	}

	/* The width, height and code must match. */
	if (source_fmt->format.width != sink_fmt->format.width ||
	    source_fmt->format.width != sink_fmt->format.width ||
	    source_fmt->format.code != sink_fmt->format.code) {
		csi2_err("%s: format does not match (source %ux%u 0x%x, sink %ux%u 0x%x)\n",
			 __func__,
			 source_fmt->format.width, source_fmt->format.height,
			 source_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height,
			 sink_fmt->format.code);
		return -EPIPE;
	}

	return 0;
}

static const struct v4l2_subdev_pad_ops csi2_subdev_pad_ops = {
	.get_fmt = csi2_pad_get_fmt,
	.set_fmt = csi2_pad_set_fmt,
	.link_validate = csi2_link_validate,
};

static const struct media_entity_operations csi2_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_ops csi2_subdev_ops = {
	.pad = &csi2_subdev_pad_ops,
};

int csi2_init(struct csi2_device *csi2, struct media_device *mdev,
	      struct dentry *debugfs)
{
	unsigned int i, ret;
	u32 host_ver;
	u8 host_ver_major, host_ver_minor;

	host_ver = dw_csi2_host_read(csi2, VERSION);
	host_ver_major = (u8)((host_ver >> 24) - '0');
	host_ver_minor = (u8)((host_ver >> 16) - '0');
	host_ver_minor = host_ver_minor * 10;
	host_ver_minor += (u8)((host_ver >> 8) - '0');

	csi2_info("DW CSI2 Host HW v%u.%u\n",  host_ver_major, host_ver_minor);
	debugfs_create_file("csi2_regs", 0444, debugfs, csi2, &csi2_regs_fops);

	csi2_close_rx(csi2);

	for (i = 0; i < CSI2_NUM_CHANNELS * 2; i++)
		csi2->pad[i].flags = i < CSI2_NUM_CHANNELS ?
				     MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&csi2->sd.entity, ARRAY_SIZE(csi2->pad),
				     csi2->pad);
	if (ret)
		return ret;

	/* Initialize subdev, but register in the caller. */
	v4l2_subdev_init(&csi2->sd, &csi2_subdev_ops);
	csi2->sd.entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	csi2->sd.entity.ops = &csi2_entity_ops;
	csi2->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	csi2->sd.owner = THIS_MODULE;
	snprintf(csi2->sd.name, sizeof(csi2->sd.name), "csi2");

	return 0;
}
