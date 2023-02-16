/* SPDX-License-Identifier: GPL-2.0 */
/*
 * RP1 CSI-2 driver.
 * Copyright (c) 2021 Raspberry Pi Ltd.
 *
 */
#ifndef _RP1_CSI2_
#define _RP1_CSI2_

#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/types.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define CSI2_NUM_CHANNELS 4

enum csi2_mode {
	CSI2_MODE_NORMAL,
	CSI2_MODE_REMAP,
	CSI2_MODE_COMPRESSED,
	CSI2_MODE_FE_STREAMING
};

struct csi2_cfg {
	uint16_t width;
	uint16_t height;
	uint32_t stride;
	uint32_t buffer_size;
};

struct csi2_device {

	/* Parent V4l2 device */
	struct v4l2_device *v4l2_dev;

	void __iomem *base;
	void __iomem *host_base;

	enum v4l2_mbus_type bus_type;
	unsigned int bus_flags;
	u32 num_lanes;
	u32 active_data_lanes;
	u32 dphy_freq;
	bool multipacket_line;
	unsigned int num_lines[CSI2_NUM_CHANNELS];

	struct media_pad pad[CSI2_NUM_CHANNELS*2];
	struct v4l2_subdev sd;
	struct v4l2_subdev_format format[CSI2_NUM_CHANNELS*2];
};

void csi2_isr(struct csi2_device *csi2, bool *sof, bool *eof, bool *lci);
void csi2_set_buffer(struct csi2_device *csi2, unsigned int channel,
		     dma_addr_t dmaaddr, unsigned int stride, unsigned int size);
void csi2_set_compression(struct csi2_device *csi2, unsigned int channel,
			  unsigned int mode, unsigned int shift, unsigned int offset);
void csi2_start_channel(struct csi2_device *csi2, unsigned int channel,
			uint16_t dt, enum csi2_mode mode, bool auto_arm,
			bool pack_bytes, unsigned int width, unsigned int height);
void csi2_stop_channel(struct csi2_device *csi2, unsigned int channel);
void csi2_open_rx(struct csi2_device *csi2);
void csi2_close_rx(struct csi2_device *csi2);
int csi2_init(struct csi2_device *csi2, struct media_device *mdev,
	      struct dentry *debugfs);

#endif
