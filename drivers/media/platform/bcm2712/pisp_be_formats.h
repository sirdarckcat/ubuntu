/* SPDX-License-Identifier: GPL-2.0 */
/*
 * PiSP Back End driver image format definitions.
 *
 * Copyright (c) 2021 Raspberry Pi Ltd.
 */

#ifndef _PISP_BE_FORMATS_
#define _PISP_BE_FORMATS_

#include <linux/bits.h>
#include <linux/videodev2.h>

#define MAX_PLANES 3
#define P3(x) ((x) * 8)

struct pisp_be_format {
	unsigned int fourcc;
	unsigned int align;
	unsigned int bit_depth;
	/* 0P3 factor for plane sizing */
	unsigned int plane_factor[MAX_PLANES];
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
		.plane_factor	    = { P3(1), P3(0.25), P3(0.25) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_JPEG,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVU420,
		/* 128 alignment to ensure U/V planes are 64 byte aligned. */
		.align		    = 128,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(0.25), P3(0.25) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_NV12,
		.align		    = 32,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(0.5) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_NV21,
		.align		    = 32,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(0.5) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YUYV,
		.align		    = 64,
		.bit_depth	    = 16,
		.plane_factor	    = { P3(1) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_UYVY,
		.align		    = 64,
		.bit_depth	    = 16,
		.plane_factor	    = { P3(1) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVYU,
		.align		    = 64,
		.bit_depth	    = 16,
		.plane_factor	    = { P3(1) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_VYUY,
		.align		    = 64,
		.bit_depth	    = 16,
		.plane_factor	    = { P3(1) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	/* Multiplane YUV formats */
	{
		.fourcc		    = V4L2_PIX_FMT_YUV420M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(0.25), P3(0.25) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_JPEG,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVU420M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(0.25), P3(0.25) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YUV422M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(0.5), P3(0.5) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_JPEG,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVU422M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(0.5), P3(0.5) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YUV444M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(1), P3(1) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_JPEG,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_YVU444M,
		.align		    = 64,
		.bit_depth	    = 8,
		.plane_factor	    = { P3(1), P3(1), P3(1) },
		.num_planes	    = 3,
		.colorspace_mask    = COLORSPACE_MASK_YUV,
		.colorspace_default = V4L2_COLORSPACE_SMPTE170M,
	},
	/* RGB formats */
	{
		.fourcc		    = V4L2_PIX_FMT_RGB24,
		.align		    = 32,
		.bit_depth	    = 24,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_RGB565,
		.align		    = 32,
		.bit_depth	    = 16,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_BGR24,
		.align		    = 32,
		.bit_depth	    = 24,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_XBGR32,
		.align		    = 64,
		.bit_depth	    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_RGBX32,
		.align		    = 64,
		.bit_depth	    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_SRGB,
		.colorspace_default = V4L2_COLORSPACE_SRGB,
	},
	/* Bayer formats - 8-bit */
	{
		.fourcc		    = V4L2_PIX_FMT_SRGGB8,
		.bit_depth	    = 8,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SBGGR8,
		.bit_depth	    = 8,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGRBG8,
		.bit_depth	    = 8,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGBRG8,
		.bit_depth	    = 8,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	/* Bayer formats - 10-bit */
	{
		.fourcc		    = V4L2_PIX_FMT_SRGGB10P,
		.bit_depth	    = 10,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SBGGR10P,
		.bit_depth	    = 10,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGRBG10P,
		.bit_depth	    = 10,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGBRG10P,
		.bit_depth	    = 10,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	/* Bayer formats - 12-bit */
	{
		.fourcc		    = V4L2_PIX_FMT_SRGGB12P,
		.bit_depth	    = 12,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SBGGR12P,
		.bit_depth	    = 12,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGRBG12P,
		.bit_depth	    = 12,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGBRG12P,
		.bit_depth	    = 12,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	/* Bayer formats - 16-bit */
	{
		.fourcc		    = V4L2_PIX_FMT_SRGGB16,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SBGGR16,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGRBG16,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGBRG16,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		/* Bayer formats unpacked to 16bpp */
		/* 10 bit */
		.fourcc		    = V4L2_PIX_FMT_SRGGB10,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SBGGR10,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGRBG10,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGBRG10,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		/* 12 bit */
		.fourcc		    = V4L2_PIX_FMT_SRGGB12,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SBGGR12,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGRBG12,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGBRG12,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		/* 14 bit */
		.fourcc		    = V4L2_PIX_FMT_SRGGB14,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SBGGR14,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGRBG14,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_SGBRG14,
		.bit_depth	    = 16,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	/* Bayer formats - 16-bit PiSP Compressed */
	{
		.fourcc		    = V4L2_PIX_FMT_PISP_COMP1_BGGR,
		.bit_depth	    = 8,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_PISP_COMP1_RGGB,
		.bit_depth	    = 8,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_PISP_COMP1_GRBG,
		.bit_depth	    = 8,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
	},
	{
		.fourcc		    = V4L2_PIX_FMT_PISP_COMP1_GBRG,
		.bit_depth	    = 8,
		.align		    = 32,
		.plane_factor	    = { P3(1.0) },
		.num_planes	    = 1,
		.colorspace_mask    = COLORSPACE_MASK_RAW,
		.colorspace_default = V4L2_COLORSPACE_RAW,
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
