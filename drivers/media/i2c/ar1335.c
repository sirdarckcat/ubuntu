// SPDX-License-Identifier: GPL-2.0
/*
 * AR1335 driver
 *
 * Copyright (C) 2014-2015, NVIDIA CORPORATION, All Rights Reserved.
 * Copyright (C) 2020 Xilinx, Inc.
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
 *
 * Contacts: Anil Kumar Mamidala <anil.mamidal@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#define AR1335_NAME "ar1335"
#define AR1335_TABLE_WAIT_MS 0
#define AR1335_TABLE_END 1
#define AR1335_MAX_RETRIES 3
#define AR1335_WAIT_MS 100
#define AR1335_DEFAULT_WIDTH 1920
#define AR1335_DEFAULT_HEIGHT 1080
#define AR1335_DEF_FRAME_RATE 30
#define AR1335_MAX_RATIO_MISMATCH 10
#define AR1335_FRAME_LENGTH_ADDR 0x300A
#define AR1335_COARSE_TIME_ADDR 0x3012
#define MAX_FRAME_RATE 60
#define MIN_FRAME_RATE 30
#define V4L2_CID_HDR_MODE (V4L2_CID_CAMERA_CLASS_BASE + 1045)
#define V4L2_CID_DEFECT_CORRECTION (V4L2_CID_CAMERA_CLASS_BASE + 1046)
#define V4L2_CID_LENGTH_LINE_PCK (V4L2_CID_CAMERA_CLASS_BASE + 1047)
#define V4L2_CID_COARSE_INT_TIME (V4L2_CID_CAMERA_CLASS_BASE + 1048)
#define V4L2_CID_COLOR_FORMAT (V4L2_CID_CAMERA_CLASS_BASE + 1049)
#define to_ar1335_device(sub_dev)                                              \
	container_of(sub_dev, struct ar1335_device, sd)

struct ar1335_reg {
	u16 addr;
	u16 val;
};

struct ar1335_res_struct {
	u16 width;
	u16 height;
	u16 out_fmt;
	u16 fps;
	struct ar1335_reg *ar1335_mode;
};

struct ar1335_context_res {
	s32 res_num;
	s32 cur_res;
	struct ar1335_res_struct *res_table;
};

struct ar1335_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	const struct firmware *fw;
	struct mutex input_lock; /* serialize sensor's ioctl */
	struct v4l2_mbus_framefmt format;
	struct v4l2_fract frame_rate;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *test_pattern;
	struct ar1335_res_struct *res_table;
	s32 cur_res;
	unsigned int num_lanes;
	struct gpio_desc *rst_gpio;
	struct regmap *regmap16;
	bool sys_activated;
	bool sys_init;
};

struct ar1335_firmware {
	u32 crc;
	u32 pll_init_size;
	u32 total_size;
	u32 reserved;
};

struct ar1335_context_info {
	u16 offset;
	u16 len;
	char *name;
};

static struct ar1335_reg ar1335_init[] = {
	{0x301A, 0x0210},
	{0x3EB6, 0x004D},
	{0x3EBC, 0xAA06},
	{0x3EC0, 0x1E02},
	{0x3EC2, 0x7700},
	{0x3EC4, 0x1C08},
	{0x3EC6, 0xEA44},
	{0x3EC8, 0x0F0F},
	{0x3ECA, 0x0F4A},
	{0x3ECC, 0x0706},
	{0x3ECE, 0x443B},
	{0x3ED0, 0x12F0},
	{0x3ED2, 0x0039},
	{0x3ED4, 0x862F},
	{0x3ED6, 0x4080},
	{0x3ED8, 0x0523},
	{0x3EDA, 0xF896},
	{0x3EDC, 0x508C},
	{0x3EDE, 0x5005},
	{0x316A, 0x8200},
	{0x316E, 0x8200},
	{0x316C, 0x8200},
	{0x3EF0, 0x414D},
	{0x3EF2, 0x0101},
	{0x3EF6, 0x0307},
	{0x3EFA, 0x0F0F},
	{0x3EFC, 0x0F0F},
	{0x3EFE, 0x0F0F},
	{0x3D00, 0x0446},
	{0x3D02, 0x4C66},
	{0x3D04, 0xFFFF},
	{0x3D06, 0xFFFF},
	{0x3D08, 0x5E40},
	{0x3D0A, 0x1146},
	{0x3D0C, 0x5D41},
	{0x3D0E, 0x1088},
	{0x3D10, 0x8342},
	{0x3D12, 0x00C0},
	{0x3D14, 0x5580},
	{0x3D16, 0x5B83},
	{0x3D18, 0x6084},
	{0x3D1A, 0x5A8D},
	{0x3D1C, 0x00C0},
	{0x3D1E, 0x8342},
	{0x3D20, 0x925A},
	{0x3D22, 0x8664},
	{0x3D24, 0x1030},
	{0x3D26, 0x801C},
	{0x3D28, 0x00A0},
	{0x3D2A, 0x56B0},
	{0x3D2C, 0x5788},
	{0x3D2E, 0x5150},
	{0x3D30, 0x824D},
	{0x3D32, 0x8D58},
	{0x3D34, 0x58D2},
	{0x3D36, 0x438A},
	{0x3D38, 0x4592},
	{0x3D3A, 0x458A},
	{0x3D3C, 0x4389},
	{0x3D3E, 0x51FF},
	{0x3D40, 0x8451},
	{0x3D42, 0x8410},
	{0x3D44, 0x0C88},
	{0x3D46, 0x5959},
	{0x3D48, 0x8A5F},
	{0x3D4A, 0xDA42},
	{0x3D4C, 0x9361},
	{0x3D4E, 0X8262},
	{0x3D50, 0x8342},
	{0x3D52, 0x8010},
	{0x3D54, 0xC041},
	{0x3D56, 0x64FF},
	{0x3D58, 0xFFB7},
	{0x3D5A, 0x4081},
	{0x3D5C, 0x4080},
	{0x3D5E, 0x4180},
	{0x3D60, 0x4280},
	{0x3D62, 0x438D},
	{0x3D64, 0x44BA},
	{0x3D66, 0x4488},
	{0x3D68, 0x4380},
	{0x3D6A, 0x4241},
	{0x3D6C, 0x8140},
	{0x3D6E, 0x8240},
	{0x3D70, 0x8041},
	{0x3D72, 0x8042},
	{0x3D74, 0x8043},
	{0x3D76, 0x8D44},
	{0x3D78, 0xBA44},
	{0x3D7A, 0x875E},
	{0x3D7C, 0x4354},
	{0x3D7E, 0x4241},
	{0x3D80, 0x8140},
	{0x3D82, 0x8120},
	{0x3D84, 0x2881},
	{0x3D86, 0x6026},
	{0x3D88, 0x8055},
	{0x3D8A, 0x8070},
	{0x3D8C, 0x8040},
	{0x3D8E, 0x4C81},
	{0x3D90, 0x45C3},
	{0x3D92, 0x4581},
	{0x3D94, 0x4C40},
	{0x3D96, 0x8070},
	{0x3D98, 0x8040},
	{0x3D9A, 0x4C85},
	{0x3D9C, 0x6CA8},
	{0x3D9E, 0x6C8C},
	{0x3DA0, 0x000E},
	{0x3DA2, 0xBE44},
	{0x3DA4, 0x8844},
	{0x3DA6, 0xBC78},
	{0x3DA8, 0x0900},
	{0x3DAA, 0x8904},
	{0x3DAC, 0x8080},
	{0x3DAE, 0x0240},
	{0x3DB0, 0x8609},
	{0x3DB2, 0x008E},
	{0x3DB4, 0x0900},
	{0x3DB6, 0x8002},
	{0x3DB8, 0x4080},
	{0x3DBA, 0x0480},
	{0x3DBC, 0x887C},
	{0x3DBE, 0xAA86},
	{0x3DC0, 0x0900},
	{0x3DC2, 0x877A},
	{0x3DC4, 0x000E},
	{0x3DC6, 0xC379},
	{0x3DC8, 0x4C40},
	{0x3DCA, 0xBF70},
	{0x3DCC, 0x5E40},
	{0x3DCE, 0x114E},
	{0x3DD0, 0x5D41},
	{0x3DD2, 0x5383},
	{0x3DD4, 0x4200},
	{0x3DD6, 0xC055},
	{0x3DD8, 0xA400},
	{0x3DDA, 0xC083},
	{0x3DDC, 0x4288},
	{0x3DDE, 0x6083},
	{0x3DE0, 0x5B80},
	{0x3DE2, 0x5A64},
	{0x3DE4, 0x1030},
	{0x3DE6, 0x801C},
	{0x3DE8, 0x00A5},
	{0x3DEA, 0x5697},
	{0x3DEC, 0x57A5},
	{0x3DEE, 0x5180},
	{0x3DF0, 0x505A},
	{0x3DF2, 0x814D},
	{0x3DF4, 0x8358},
	{0x3DF6, 0x8058},
	{0x3DF8, 0xA943},
	{0x3DFA, 0x8345},
	{0x3DFC, 0xB045},
	{0x3DFE, 0x8343},
	{0x3E00, 0xA351},
	{0x3E02, 0xE251},
	{0x3E04, 0x8C59},
	{0x3E06, 0x8059},
	{0x3E08, 0x8A5F},
	{0x3E0A, 0xEC7C},
	{0x3E0C, 0xCC84},
	{0x3E0E, 0x6182},
	{0x3E10, 0x6283},
	{0x3E12, 0x4283},
	{0x3E14, 0x10CC},
	{0x3E16, 0x6496},
	{0x3E18, 0x4281},
	{0x3E1A, 0x41BB},
	{0x3E1C, 0x4082},
	{0x3E1E, 0x407E},
	{0x3E20, 0xCC41},
	{0x3E22, 0x8042},
	{0x3E24, 0x8043},
	{0x3E26, 0x8300},
	{0x3E28, 0xC088},
	{0x3E2A, 0x44BA},
	{0x3E2C, 0x4488},
	{0x3E2E, 0x00C8},
	{0x3E30, 0x8042},
	{0x3E32, 0x4181},
	{0x3E34, 0x4082},
	{0x3E36, 0x4080},
	{0x3E38, 0x4180},
	{0x3E3A, 0x4280},
	{0x3E3C, 0x4383},
	{0x3E3E, 0x00C0},
	{0x3E40, 0x8844},
	{0x3E42, 0xBA44},
	{0x3E44, 0x8800},
	{0x3E46, 0xC880},
	{0x3E48, 0x4241},
	{0x3E4A, 0x8240},
	{0x3E4C, 0x8140},
	{0x3E4E, 0x8041},
	{0x3E50, 0x8042},
	{0x3E52, 0x8043},
	{0x3E54, 0x8300},
	{0x3E56, 0xC088},
	{0x3E58, 0x44BA},
	{0x3E5A, 0x4488},
	{0x3E5C, 0x00C8},
	{0x3E5E, 0x8042},
	{0x3E60, 0x4181},
	{0x3E62, 0x4082},
	{0x3E64, 0x4080},
	{0x3E66, 0x4180},
	{0x3E68, 0x4280},
	{0x3E6A, 0x4383},
	{0x3E6C, 0x00C0},
	{0x3E6E, 0x8844},
	{0x3E70, 0xBA44},
	{0x3E72, 0x8800},
	{0x3E74, 0xC880},
	{0x3E76, 0x4241},
	{0x3E78, 0x8140},
	{0x3E7A, 0x9F5E},
	{0x3E7C, 0x8A54},
	{0x3E7E, 0x8620},
	{0x3E80, 0x2881},
	{0x3E82, 0x6026},
	{0x3E84, 0x8055},
	{0x3E86, 0x8070},
	{0x3E88, 0x0000},
	{0x3E8A, 0x0000},
	{0x3E8C, 0x0000},
	{0x3E8E, 0x0000},
	{0x3E90, 0x0000},
	{0x3E92, 0x0000},
	{0x3E94, 0x0000},
	{0x3E96, 0x0000},
	{0x3E98, 0x0000},
	{0x3E9A, 0x0000},
	{0x3E9C, 0x0000},
	{0x3E9E, 0x0000},
	{0x3EA0, 0x0000},
	{0x3EA2, 0x0000},
	{0x3EA4, 0x0000},
	{0x3EA6, 0x0000},
	{0x3EA8, 0x0000},
	{0x3EAA, 0x0000},
	{0x3EAC, 0x0000},
	{0x3EAE, 0x0000},
	{0x3EB0, 0x0000},
	{0x3EB2, 0x0000},
	{0x3EB4, 0x0000},
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg ar1335_defect_cor[] = {
	{0x31E0, 0x0781},
	{0x3F00, 0x004F},
	{0x3F02, 0x0125},
	{0x3F04, 0x0020},
	{0x3F06, 0x0040},
	{0x3F08, 0x0070},
	{0x3F0A, 0x0101},
	{0x3F0C, 0x0302},
	{0x3F1E, 0x0022},
	{0x3F1A, 0x01FF},
	{0x3F14, 0x0101},
	{0x3F44, 0x0707},
	{0x3F18, 0x011E},
	{0x3F12, 0x0303},
	{0x3F42, 0x1511},
	{0x3F16, 0x011E},
	{0x3F10, 0x0505},
	{0x3F40, 0x1511}, //Enable defect correction
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg ar1335_hdr_on[] = {
	{0x317A, 0x416E},
	{0x0400, 0x0000},
	{0x3EFA, 0x070F},
	{0x3EFC, 0x070F},
	{0x31E0, 0x0091},
	{0x316c, 0x8400},
	{0x303E, 0x0001},
	{0x3012, 0x0960},
	{0x3088, 0x012C},
	{0x305E, 0x2013},
	{AR1335_TABLE_WAIT_MS, AR1335_WAIT_MS},
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg ar1335_hdr_off[] = {
	{0x3EFA, 0x0F0F},
	{0x3EFC, 0x0F0F},
	{0x31E0, 0x0781},
	{0x316c, 0x8200},
	{0x303E, 0x0000},
	{0x305E, 0x2010},
	{AR1335_TABLE_WAIT_MS, AR1335_WAIT_MS},
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg ar1335_start_stream[] = {
	{0x3F3C, 0x0003},
	{0x301A, 0x023C},
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg ar1335_stop_stream[] = {
	{0x3F3C, 0x0002},
	{0x301A, 0x0210},
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg mode_4208x3120_30[] = {
	{0x31B0, 0x005C},
	{0x31B2, 0x002D},
	{0x31B4, 0x2412},
	{0x31B6, 0x142A},
	{0x31B8, 0x2413},
	{0x31BA, 0x1C70},
	{0x31BC, 0x868B},
	{0x31AE, 0x0204},
//These timing are for ar1335_rev1 sensor pll_setup_max
	{0x0300, 0x0005},
	{0x0302, 0x0001},
	{0x0304, 0x0101},
	{0x0306, 0x2E2E},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0112, 0x0A0A},
	{0x3016, 0x0101},
	{AR1335_TABLE_WAIT_MS, AR1335_WAIT_MS},

	{0x0344, 0x0010},
	{0x0348, 0x107F},
	{0x0346, 0x0010},
	{0x034A, 0x0C3F},
	{0x034C, 0x1070},
	{0x034E, 0x0C30},
	{0x3040, 0x0041},
	{0x0112, 0x0A0A},
	{0x0112, 0x0A0A},
	{0x3172, 0x0206},
	{0x317A, 0x416E},
	{0x3F3C, 0x0003},
	{0x0342, 0x1240},
	{0x0340, 0x0C4E},
	{0x0202, 0x0C44},

	{0x3F3C, 0x0003},
	{0x301A, 0x021C},
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg mode_3840x2160_30[] = {
	{0x31B0, 0x0086},
	{0x31B2, 0x0057},
	{0x31B4, 0x2412},
	{0x31B6, 0x142A},
	{0x31B8, 0x2413},
	{0x31BA, 0x1C70},
	{0x31BC, 0x068B},// mipi_timing_recommended
	{0x31AE, 0x0204},
//These timing are for ar1335_rev1 sensor pll_setup_max
	{0x0300, 0x0004},
	{0x0302, 0x0001},
	{0x0304, 0x0903},
	{0x0306, 0xCF37},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0112, 0x0A0A},
	{0x3016, 0x0101}, //pll_setup_recommended
	{AR1335_TABLE_WAIT_MS, AR1335_WAIT_MS},
	{0x0344, 0x00C8},
	{0x0348, 0x0FC7},
	{0x0346, 0x01F0},
	{0x034A, 0x0A5F},
	{0x034C, 0x0F00},
	{0x034E, 0x0870},
	{0x3040, 0x4041},
	{0x3172, 0x0206},
	{0x317A, 0x416E},
	{0x3F3C, 0x0003},
	{0x0400, 0x0000},
	{0x0404, 0x0010}, //scalar settings
	{0x0342, 0x1230},
	{0x0340, 0x0C4E}, //30 fps
	{0x0202, 0x0C2E}, //30fps setting
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg mode_1920x1080_60[] = {
	{0x31B0, 0x005C},
	{0x31B2, 0x002E},
	{0x31B4, 0x2412},
	{0x31B6, 0x142A},
	{0x31B8, 0x2413},
	{0x31BA, 0x1C72},
	{0x31BC, 0x860B},// mipi_timing_recommended
	{0x3024, 0x0001},
	{0x31AE, 0x0204},
	{0x0300, 0x0004},
	{0x0302, 0x0001},
	{0x0304, 0x0001},
	{0x0306, 0x0019},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0112, 0x0A0A},
	{0x3016, 0x0101}, //pll_setup_recommended
	{AR1335_TABLE_WAIT_MS, AR1335_WAIT_MS},
	{0x0344, 0x00C8},
	{0x0348, 0x0FC7},
	{0x0346, 0x01F0},
	{0x034A, 0x0A5D},
	{0x034C, 0x0780},
	{0x034E, 0x0438},
	{0x3040, 0x0041},
	{0x3172, 0x0000},
	{0x317A, 0x0001},
	{0x3F3C, 0x0000},
	{0x0400, 0x0001},
	{0x0404, 0x0020}, //scalar settings
	{0x0342, 0x18CC},
	{0x0340, 0x0C2F}, //60 fps
	{0x0202, 0x0C4E}, //60 fps setting

	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg mode_1920x1080_30[] = {
	{0x31B0, 0x004D},
	{0x31B2, 0x0028},
	{0x31B4, 0x230E},
	{0x31B6, 0x1348},
	{0x31B8, 0x1C12},
	{0x31BA, 0x185B},
	{0x31BC, 0x8509},// mipi_timing_recommended
	{0x31AE, 0x0204},
	{0x3024, 0x0001},
//These timing are for ar1335_rev1 sensor pll_setup_max
	{0x0300, 0x0004},
	{0x0302, 0x0001},
	{0x0304, 0x0303},
	{0x0306, 0x3737},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0112, 0x0A0A},
	{0x3016, 0x0101}, //pll_setup_recommended
	{AR1335_TABLE_WAIT_MS, AR1335_WAIT_MS},
	{0x0344, 0x00C8},
	{0x0348, 0x0FC7},
	{0x0346, 0x01F0},
	{0x034A, 0x0A5D},
	{0x034C, 0x0780},
	{0x034E, 0x0438},
	{0x3040, 0x4041},
	{0x3172, 0x0206},
	{0x317A, 0x516E},
	{0x3F3C, 0x0003},
	{0x0400, 0x0001},
	{0x0404, 0x0020}, //scalar settings
	{0x0342, 0x1230},
	{0x0340, 0x0625}, //30 fps
	{0x0202, 0x0626}, //30fps setting
	{AR1335_TABLE_END, 0x00}
};

static struct ar1335_reg mode_1280x720_60[] = {
	{0x31B0, 0x004D},
	{0x31B2, 0x0028},
	{0x31B4, 0x230E},
	{0x31B6, 0x1348},
	{0x31B8, 0x1C12},
	{0x31BA, 0x185B},
	{0x31BC, 0x8509},// mipi_timing_recommended
	{0x31AE, 0x0204},
	{0x3024, 0x0001},
//These timing are for ar1335_rev1 sensor pll_setup_max
	{0x0300, 0x0004},
	{0x0302, 0x0001},
	{0x0304, 0x0303},
	{0x0306, 0x3737},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0112, 0x0A0A},
	{0x3016, 0x0101}, //pll_setup_recommended
	{AR1335_TABLE_WAIT_MS, AR1335_WAIT_MS},
	{0x0344, 0x00C8},
	{0x0348, 0x0FC7},
	{0x0346, 0x01F0},
	{0x034A, 0x0A5B},
	{0x034C, 0x0500},
	{0x034E, 0x02D0},
	{0x3040, 0x4045},
	{0x3172, 0x0206},
	{0x317A, 0x516E},
	{0x3F3C, 0x0003},
	{0x0400, 0x0001},
	{0x0404, 0x0020}, //scalar settings
	{0x0342, 0x1230},
	{0x0340, 0x0626}, //30 fps
	{0x0202, 0x05E8}, //30fps setting
	{AR1335_TABLE_END, 0x00}
};

/* Static definitions */
static struct regmap_config ar1335_reg16_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_BIG,
};

static struct ar1335_res_struct ar1335_res_table[] = {
	{
		.width = 1280,
		.height = 720,
		.ar1335_mode = mode_1280x720_60,
	},
	{
		.width = 1920,
		.height = 1080,
		.ar1335_mode = mode_1920x1080_30,
	},
	{
		.width = 3840,
		.height = 2160,
		.ar1335_mode = mode_3840x2160_30,
	}
};

static int ar1335_i2c_read_reg(struct v4l2_subdev *sd,
			       u16 reg, void *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ar1335_device *dev = to_ar1335_device(sd);
	int ret;

	ret = regmap_read(dev->regmap16, reg, val);
	if (ret) {
		dev_info(&client->dev, "Read reg failed. reg=0x%04X\n", reg);
		return ret;
	}
	dev_info(&client->dev, "read_reg[0x%04X] = 0x%04X\n",
		 reg, *(u16 *)val);
	return ret;
}

static int ar1335_i2c_write_reg(struct v4l2_subdev *sd,
				u16 reg,  u32 val)
{
	struct ar1335_device *dev = to_ar1335_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = regmap_write(dev->regmap16, reg, val);
	if (ret) {
		dev_info(&client->dev, "Write reg failed. reg=0x%04X\n", reg);
		return ret;
	}
	dev_info(&client->dev, "write_reg[0x%04X] = 0x%04X\n", reg, (u16)val);
	return ret;
}

static int ar1335_write_table(struct v4l2_subdev *sd,
			      const struct ar1335_reg table[],
			      const struct ar1335_reg override_list[],
			      int num_override_regs)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct ar1335_reg *next;
	int err, i;
	u16 val;

	for (next = table; next->addr != AR1335_TABLE_END; next++) {
		if (next->addr == AR1335_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = ar1335_i2c_write_reg(sd, next->addr, val);
		if (err) {
			dev_err(&client->dev, "%s:%d\n", __func__, err);
			return err;
		}
	}
	return 0;
}

static int ar1335_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (code->index) {
		dev_err(&client->dev, "%s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	return 0;
}

static int ar1335_match_resolution(struct v4l2_mbus_framefmt *fmt)
{
	s32 w0, h0, mismatch, distance;
	s32 w1 = fmt->width;
	s32 h1 = fmt->height;
	s32 min_distance = INT_MAX;
	s32 i, idx = -1;

	if (w1 == 0 || h1 == 0)
		return -1;

	for (i = 0; i < ARRAY_SIZE(ar1335_res_table); i++) {
		w0 = ar1335_res_table[i].width;
		h0 = ar1335_res_table[i].height;
		if (w0 < w1 || h0 < h1)
			continue;
		mismatch = abs(w0 * h1 - w1 * h0) * 8192 / w1 / h0;

		if (mismatch > 8192 * AR1335_MAX_RATIO_MISMATCH / 100)
			continue;
		distance = (w0 * h1 + w1 * h0) * 8192 / w1 / h1;
		if (distance < min_distance) {
			min_distance = distance;
			idx = i;
			break;
		}
	}

	return idx;
}

static s32 ar1335_try_mbus_fmt_locked(struct v4l2_subdev *sd,
				      struct v4l2_mbus_framefmt *fmt)
{
	s32 res_num, idx = -1;

	res_num = ARRAY_SIZE(ar1335_res_table);

	if (fmt->width <= ar1335_res_table[res_num - 1].width &&
	    fmt->height <= ar1335_res_table[res_num - 1].height)
		idx = ar1335_match_resolution(fmt);
	if (idx == -1)
		idx = res_num - 1;

	fmt->width = ar1335_res_table[idx].width;
	fmt->height = ar1335_res_table[idx].height;

	return idx;
}

static int ar1335_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt = &format->format;
	struct ar1335_device *dev = to_ar1335_device(sd);

	if (format->pad)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fmt->width = dev->format.width;
	fmt->height = dev->format.height;
	fmt->code = dev->format.code;
	fmt->field = dev->format.field;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ar1335_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *format)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ar1335_device *dev = to_ar1335_device(sd);
	struct v4l2_mbus_framefmt *fmt = &format->format;

	s32 idx, ret = 0;

	mutex_lock(&dev->input_lock);

	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
		ar1335_try_mbus_fmt_locked(sd, fmt);
		state->pads->try_fmt = *fmt;
		mutex_unlock(&dev->input_lock);
		return 0;
	}
	idx = ar1335_try_mbus_fmt_locked(sd, &format->format);
	dev->cur_res = idx;

	dev->format.width = format->format.width;
	dev->format.height = format->format.height;
	dev->format.field = V4L2_FIELD_NONE;
	if (format->format.code == MEDIA_BUS_FMT_SRGGB10_1X10 ||
	    format->format.code == MEDIA_BUS_FMT_SRGGB8_1X8) {
		dev->format.code = format->format.code;
	} else {
		dev_err(&client->dev, "%s %d format->format.code %d\n", __func__, __LINE__,
			format->format.code);
		return -EINVAL;
	}

	mutex_unlock(&dev->input_lock);
	return ret;
}

static int ar1335_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *ival)
{
	struct ar1335_device *dev = to_ar1335_device(sd);
	struct v4l2_fract *tpf = &ival->interval;

	if (tpf->numerator == 0 || tpf->denominator == 0 ||
	    (tpf->denominator > tpf->numerator * MAX_FRAME_RATE)) {
		/* reset to max frame rate */
		tpf->numerator = 1;
		tpf->denominator = MAX_FRAME_RATE;
	}
	dev->frame_rate.numerator = tpf->numerator;
	if (tpf->numerator == 30) {
		ar1335_i2c_write_reg(sd, 0x340, 0xC4E);
		ar1335_i2c_write_reg(sd, 0x202, 0xC4E);
		dev->frame_rate.denominator = tpf->denominator;
	} else if (tpf->numerator == 60) {
		ar1335_i2c_write_reg(sd, 0x340, 0x626);
		ar1335_i2c_write_reg(sd, 0x202, 0x5E8);
		dev->frame_rate.denominator = tpf->denominator;
	} else {
		ar1335_i2c_write_reg(sd, 0x340, 0xC4E);
		ar1335_i2c_write_reg(sd, 0x202, 0xC4E);
		dev->frame_rate.denominator = MIN_FRAME_RATE;
	}
	return 0;
}

static int ar1335_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *interval)
{
	struct ar1335_device *dev = to_ar1335_device(sd);

	mutex_lock(&dev->input_lock);
	interval->interval.denominator = dev->frame_rate.denominator;
	interval->interval.numerator = dev->frame_rate.numerator;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ar1335_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct ar1335_device *dev = to_ar1335_device(sd);
	struct ar1335_res_struct *res_table;
	int index = fse->index;

	mutex_lock(&dev->input_lock);
	if (index >= dev->cur_res) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	res_table = dev->res_table;
	fse->min_width = res_table[index].width;
	fse->min_height = res_table[index].height;
	fse->max_width = res_table[index].width;
	fse->max_height = res_table[index].height;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ar1335_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	*frames = 0;
	return 0;
}

static int ar1335_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar1335_device *dev = to_ar1335_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 idx;
	int ret;

	mutex_lock(&dev->input_lock);
	idx = dev->cur_res;

	if (enable) {
		if (!dev->sys_activated) {
			ret = ar1335_write_table(sd, ar1335_init, 0, 0);
			if (ret < 0) {
				dev_err(&client->dev,
					"could not sent common table %d\n",
					ret);
				goto error;
			}
			dev->sys_activated = 1;

			ret = ar1335_write_table(sd,
						 ar1335_res_table[idx].ar1335_mode,
						 0, 0);
			if (ret < 0) {
				dev_err(&client->dev,
					"could not sent mode table %d\n", ret);
				goto error;
			}

		} else {
			ret = ar1335_write_table(sd, ar1335_res_table[idx].ar1335_mode,
						 0, 0);
			if (ret < 0) {
				dev_err(&client->dev,
					"could not sent mode table %d\n", ret);
				goto error;
			}
		}

		ret = ar1335_write_table(sd, ar1335_start_stream, 0, 0);
		if (ret < 0) {
			dev_err(&client->dev,
				"could not sent common table %d\n", ret);
			goto error;
		}
	} else {
		ret = ar1335_write_table(sd, ar1335_stop_stream, 0, 0);
		if (ret < 0) {
			dev_err(&client->dev,
				"could not sent common table %d\n", ret);
			goto error;
		}
		dev_info(&client->dev, "Stop stream.\n");
	}
error:
	mutex_unlock(&dev->input_lock);
	return ret;
}

static u16 ar1335_gain_values[] = { 0x2015, 0x2025, 0x2035,
				    0x2BBF, 0x573F, 0xAE3F };

static int ar1335_set_gain(struct v4l2_subdev *sd, s32 val)
{
	return ar1335_i2c_write_reg(sd, 0x305E, ar1335_gain_values[val]);
}

static int ar1335_set_hmirror(struct v4l2_subdev *sd, s32 val)
{
	int ret = 0;
	u16 reg_val;

	ret = ar1335_i2c_read_reg(sd, 0x3040, &reg_val);
	if (ret)
		return ret;

	if (val == 1)
		reg_val |= (1 << 14);
	else
		reg_val &= ~(1 << 14);

	return ar1335_i2c_write_reg(sd, 0x3040, reg_val);
}

static int ar1335_set_vflip(struct v4l2_subdev *sd, s32 val)
{
	int ret = 0;
	u16 reg_val;

	ret = ar1335_i2c_read_reg(sd, 0x3040, &reg_val);
	if (ret)
		return ret;

	if (val == 1)
		reg_val |= (1 << 15);
	else
		reg_val &= ~(1 << 15);

	return ar1335_i2c_write_reg(sd, 0x3040, reg_val);
}

static int ar1335_set_line_length_pck(struct v4l2_subdev *sd, s32 val)
{
	return ar1335_i2c_write_reg(sd, 0x0342, val);
}

static int ar1335_set_coarse_integration_time(struct v4l2_subdev *sd, s32 val)
{
	return ar1335_i2c_write_reg(sd, 0x0202, val);
}

static int ar1335_defect_correction(struct v4l2_subdev *sd, s32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (val) {
		ret = ar1335_write_table(sd, ar1335_defect_cor, 0, 0);
		if (ret < 0)
			dev_err(&client->dev,
				"could not sent common table %d\n", ret);
	} else {
		ret = ar1335_i2c_write_reg(sd, 0x31E0, 0x0);
		if (ret < 0)
			dev_err(&client->dev,
				"could not sent common table %d\n", ret);
	}

	return ret;
}

static int ar1335_hdr_mode(struct v4l2_subdev *sd, s32 val)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (val) {
		ret = ar1335_write_table(sd, ar1335_hdr_on, 0, 0);
		if (ret < 0)
			dev_err(&client->dev,
				"could not sent common table %d\n", ret);
	} else {
		ret = ar1335_write_table(sd, ar1335_hdr_off, 0, 0);
		if (ret < 0)
			dev_err(&client->dev,
				"could not sent common table %d\n", ret);
	}

	return ret;
}

static u16 ar1335_test_pattern_values[] = {
	0x1, // Solid color
	0x2, // 100% color bar
	0x3, // fade to gray color
	0x100, // walking 1 (10bit)
	0x101, // walking 1 (8bit)
};

static int ar1335_test_pattern(struct v4l2_subdev *sd, s32 val)
{
	return ar1335_i2c_write_reg(sd, 0x0600,
				    ar1335_test_pattern_values[val]);
}

static int ar1335_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ar1335_device *dev =
		container_of(ctrl->handler, struct ar1335_device, ctrl_handler);
	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ar1335_set_gain(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ar1335_test_pattern(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		ar1335_set_vflip(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ar1335_set_hmirror(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_HDR_MODE:
		ar1335_hdr_mode(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_DEFECT_CORRECTION:
		ar1335_defect_correction(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_LENGTH_LINE_PCK:
		ar1335_set_line_length_pck(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_COARSE_INT_TIME:
		ar1335_set_coarse_integration_time(&dev->sd, ctrl->val);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ar1335_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct ar1335_device *dev = to_ar1335_device(sd);
	int ret;
	u32 reg_val;

	mutex_lock(&dev->input_lock);
	ret = ar1335_i2c_read_reg(sd, reg->reg, &reg_val);
	mutex_unlock(&dev->input_lock);
	if (ret)
		return ret;

	reg->val = reg_val;

	return 0;
}

static int ar1335_s_register(struct v4l2_subdev *sd,
			     const struct v4l2_dbg_register *reg)
{
	struct ar1335_device *dev = to_ar1335_device(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = ar1335_i2c_write_reg(sd, reg->reg, reg->val);
	mutex_unlock(&dev->input_lock);
	return ret;
}

static long ar1335_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	switch (cmd) {
	case VIDIOC_DBG_G_REGISTER:
		ret = ar1335_g_register(sd, arg);
		break;
	case VIDIOC_DBG_S_REGISTER:
		ret = ar1335_s_register(sd, arg);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = ar1335_s_ctrl,
};

static const char * const ctrl_run_mode_menu[] = {
	NULL,
	"Video",
	"Still capture",
	"Continuous capture",
	"Preview",
};

static const char * const tp_menu[] = {
	"Solid color",
	"100% Color Bar",
	"Fade-to-Gray Color Bars",
	"Walking 1s (10-bit)",
	"Walking 1s (8-bit)",
};

static const struct v4l2_ctrl_config ctrls[] = {
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_GAIN,
		.name = "Gain",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 5,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_VFLIP,
		.name = "Vertical flip",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 1,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_HFLIP,
		.name = "Horizontal Mirror",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 1,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_HDR_MODE,
		.name = "HDR Mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 1,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_DEFECT_CORRECTION,
		.name = "Defect Correction",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 1,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_LENGTH_LINE_PCK,
		.name = "Line Length Pak",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 0xffff,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_COARSE_INT_TIME,
		.name = "Coarse integration time",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 0xffff,
		.step = 1,
	},

};

static struct v4l2_subdev_sensor_ops ar1335_sensor_ops = {
	.g_skip_frames = ar1335_g_skip_frames,
};

static const struct v4l2_subdev_video_ops ar1335_video_ops = {
	.s_stream = ar1335_s_stream,
	.s_frame_interval = ar1335_s_frame_interval,
	.g_frame_interval = ar1335_g_frame_interval,
};

static const struct v4l2_subdev_core_ops ar1335_core_ops = {
	.ioctl = ar1335_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ar1335_g_register,
	.s_register = ar1335_s_register,
#endif
};

static const struct v4l2_subdev_pad_ops ar1335_pad_ops = {
	.enum_mbus_code = ar1335_enum_mbus_code,
	.enum_frame_size = ar1335_enum_frame_size,
	.get_fmt = ar1335_get_fmt,
	.set_fmt = ar1335_set_fmt,
};

static const struct v4l2_subdev_ops ar1335_ops = {
	.core = &ar1335_core_ops,
	.pad = &ar1335_pad_ops,
	.video = &ar1335_video_ops,
	.sensor = &ar1335_sensor_ops
};

static int ar1335_sw_reset(struct v4l2_subdev *sd)
{
	struct ar1335_device *dev = to_ar1335_device(sd);
	int ret;
	int val;

	mutex_lock(&dev->input_lock);
	ret = ar1335_i2c_write_reg(sd, 0x103, 0x100);
	mdelay(500);
	ar1335_i2c_read_reg(sd, 0x103, &val);
	mdelay(500);
	ar1335_i2c_read_reg(sd, 0x103, &val);
	mutex_unlock(&dev->input_lock);
	/* need to wait for 1032 external clocks to
	 * complete soft standby reset
	 */
	return ret;
}

/* Verify chip ID */
static int ar1335_identify_module(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u32 val;

	ret = ar1335_i2c_read_reg(sd, 0x00, &val);
	if (ret)
		return ret;
	if (val != 0x153) {
		dev_err(&client->dev, "chip id mismatch: 0x153 != %x\n", val);
		return -ENXIO;
	}

	ret = ar1335_i2c_read_reg(sd, 0x03, &val);
	if (ret)
		return ret;

	if (val != 0x60A) {
		dev_err(&client->dev, "chip id mismatch: 0x60A != %x\n", val);
		return -ENXIO;
	}

	return 0;
}

static int ar1335_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ar1335_device *dev = to_ar1335_device(sd);

	media_entity_cleanup(&dev->sd.entity);
	v4l2_device_unregister_subdev(sd);

	return 0;
}

static int ar1335_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ar1335_device *dev;
	int ret;
	int err;
	unsigned int i;

	dev_info(&client->dev, "ar1335 probe called.\n");

	/* allocate device & init sub device */
	dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	/* initialize format */
	dev->format.width = AR1335_DEFAULT_WIDTH;
	dev->format.height = AR1335_DEFAULT_HEIGHT;
	dev->format.field = V4L2_FIELD_NONE;
	dev->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
	dev->format.colorspace = V4L2_COLORSPACE_SRGB;
	dev->frame_rate.numerator = 1;
	dev->frame_rate.denominator = AR1335_DEF_FRAME_RATE;

	v4l2_i2c_subdev_init(&dev->sd, client, &ar1335_ops);

	dev->regmap16 = devm_regmap_init_i2c(client, &ar1335_reg16_config);
	if (IS_ERR(dev->regmap16)) {
		ret = PTR_ERR(dev->regmap16);
		dev_err(&client->dev,
			"Failed to allocate 16bit register map: %d\n", ret);
		return ret;
	}

	dev->rst_gpio = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(dev->rst_gpio)) {
		err = PTR_ERR(dev->rst_gpio);
		if (err == -EPROBE_DEFER)
			dev_info(&client->dev,
				 "Probe deferred due to GPIO reset defer\n");
		else
			dev_err(&client->dev,
				"Unable to locate reset property in dt\n");
	}

	//reset Sensor
	gpiod_set_value(dev->rst_gpio, 0);
	mdelay(1);
	/* hold reset pin low for sufficient clk cycles  */
	gpiod_set_value(dev->rst_gpio, 1);
	mdelay(1);
	/* wait till system reset */

	ret = ar1335_identify_module(&dev->sd);
	if (ret) {
		dev_err(&client->dev, "Failed to identity ar1335 sensor: %d\n",
			ret);
		ar1335_remove(client);
		return ret;
	}

	ret = ar1335_sw_reset(&dev->sd);
	if (ret) {
		dev_err(&client->dev,
			"Failed to do software reset for ar1335 sensor: %d\n",
			ret);
		ar1335_remove(client);
		return ret;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	ret = v4l2_ctrl_handler_init(&dev->ctrl_handler, ARRAY_SIZE(ctrls));
	if (ret) {
		ar1335_remove(client);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(ctrls); i++)
		v4l2_ctrl_new_custom(&dev->ctrl_handler, &ctrls[i], NULL);

	dev->test_pattern = v4l2_ctrl_new_std_menu_items
		(&dev->ctrl_handler, &ctrl_ops,
		 V4L2_CID_TEST_PATTERN,
		 ARRAY_SIZE(tp_menu) - 1, 0, 0, tp_menu);

	if (dev->ctrl_handler.error) {
		ar1335_remove(client);
		return dev->ctrl_handler.error;
	}

	/* Use same lock for controls as for everything else. */
	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = &dev->ctrl_handler;

	ret = media_entity_pads_init(&dev->sd.entity, 1, &dev->pad);
	if (ret)
		ar1335_remove(client);
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret < 0) {
		dev_err(&client->dev, "failed to register subdev\n");
		goto out_free;
	}
	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	return ret;
}

static const struct of_device_id ar1335_id[] = {
	{
		.compatible =  AR1335_NAME,
	},
	{}
};

MODULE_DEVICE_TABLE(of, ar1335_id);

static struct i2c_driver ar1335_driver = {
	.driver = {
		.name = AR1335_NAME,
		.of_match_table = ar1335_id,
	},
	.probe = ar1335_probe,
	.remove = ar1335_remove,
};

module_i2c_driver(ar1335_driver);

MODULE_AUTHOR("Anil Kumar Mamidala <amamidal@xilinx.com>");
MODULE_DESCRIPTION("V4L driver for camera sensor AR1335");
MODULE_LICENSE("GPL v2");
