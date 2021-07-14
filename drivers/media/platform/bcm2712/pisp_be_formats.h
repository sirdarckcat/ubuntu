/* SPDX-License-Identifier: GPL-2.0 */
/*
 * PiSP Back End driver image format definitions.
 *
 * Copyright (c) 2021 Raspberry Pi Trading Ltd.
 */

#ifndef _PISP_BE_FORMATS_
#define _PISP_BE_FORMATS_

#include <linux/bits.h>
#include <linux/videodev2.h>

#define MAX_PLANES 3
#define P8(x) ((x) * 256)

struct pisp_be_format {
	unsigned int fourcc;
	unsigned int align;
	unsigned int bit_depth;
	 /* 0p8 factor for plane sizing based on bpl * height */
	unsigned int plane_size[MAX_PLANES];
	unsigned int num_planes;
	unsigned int colorspace_mask;
	enum v4l2_colorspace colorspace_default;
};

#define COLORSPACE_MASK(c) BIT(c)

#define COLORSPACE_MASK_JPEG	COLORSPACE_MASK(V4L2_COLORSPACE_JPEG)
#define COLORSPACE_MASK_REC709	COLORSPACE_MASK(V4L2_COLORSPACE_REC709)
#define COLORSPACE_MASK_SRGB	COLORSPACE_MASK(V4L2_COLORSPACE_SRGB)
#define COLORSPACE_MASK_RAW	COLORSPACE_MASK(V4L2_COLORSPACE_RAW)
#define COLORSPACE_MASK_SMPTE170M COLORSPACE_MASK(V4L2_COLORSPACE_SMPTE170M)

/*
 * The colour spaces we support for YUV outputs. SRGB features here because,
 * once you assign the default transfer func and so on, it and JPEG effectively
 * mean the same.
 */
#define COLORSPACE_MASK_YUV (COLORSPACE_MASK_JPEG | COLORSPACE_MASK_SRGB | \
			     COLORSPACE_MASK_SMPTE170M | COLORSPACE_MASK_REC709)

static const struct pisp_be_format supported_formats[] = {
	/* Single plane YUV formats */
	{
		.fourcc		    = V4L2_PIX_FMT_YUV420,
		/* 128 alignment to ensure U/V planes are 64 byte aligned. */
		.align		    = 128,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1.5) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_JPEG,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVU420,
		/* 128 alignment to ensure U/V planes are 64 byte aligned. */
		.align		    = 128,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1.5) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_NV12,
		.align		    = 32,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1.5) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_NV21,
		.align		    = 32,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1.5) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YUYV,
		.align		    = 64,
		.bit_depth	    = 16,
		.plane_size	    = { P8(1) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_UYVY,
		.align		    = 64,
		.bit_depth	    = 16,
		.plane_size	    = { P8(1) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVYU,
		.align		    = 64,
		.bit_depth	    = 16,
		.plane_size	    = { P8(1) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_VYUY,
		.align		    = 64,
		.bit_depth	    = 16,
		.plane_size	    = { P8(1) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	/* Multiplane YUV formats */
	{
		.fourcc		    = V4L2_PIX_FMT_YUV420M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1), P8(0.25), P8(0.25) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_JPEG,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVU420M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1), P8(0.25), P8(0.25) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YUV422M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1), P8(0.5), P8(0.5) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_JPEG,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVU422M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1), P8(0.5), P8(0.5) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YUV444M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1), P8(1), P8(1) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_JPEG,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVU444M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_size	    = { P8(1), P8(1), P8(1) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	/* RGB formats */
	{
		.fourcc		    = V4L2_PIX_FMT_RGB24,
		.align		    = 32,
		.bit_depth	    = 24,
		.plane_size	    = { P8(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_RGB565,
		.align		    = 32,
		.bit_depth	    = 16,
		.plane_size	    = { P8(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_BGR24,
		.align		    = 32,
		.bit_depth	    = 24,
		.plane_size	    = { P8(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_XBGR32,
		.align		    = 64,
		.bit_depth	    = 32,
		.plane_size	    = { P8(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_RGBX32,
		.align		    = 64,
		.bit_depth	    = 32,
		.plane_size	    = { P8(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	/* Special opaque format for userland verification suite. */
	{
		.fourcc		    = V4L2_PIX_FMT_RPI_BE,
	},
	/* Configuration buffer format. */
	{
		.fourcc		    = V4L2_META_FMT_RPI_BE_CFG,
	},
};

#endif /* _PISP_BE_FORMATS_ */
