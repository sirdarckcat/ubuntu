// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause

/* UPHY support for Nvidia Gigabit Ethernet driver
 *
 * Copyright (C) 2022 NVIDIA CORPORATION & AFFILIATES
 */

#include <linux/bitfield.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>

#include "mlxbf_gige.h"
#include "mlxbf_gige_uphy.h"

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_clm_init[] = {
	{.addr = 0x001, .wdata = 0x0105},
	{.addr = 0x008, .wdata = 0x0001},
	{.addr = 0x00B, .wdata = 0x8420},
	{.addr = 0x00E, .wdata = 0x0110},
	{.addr = 0x010, .wdata = 0x3010},
	{.addr = 0x027, .wdata = 0x0104},
	{.addr = 0x02F, .wdata = 0x09EA},
	{.addr = 0x055, .wdata = 0x0008},
	{.addr = 0x058, .wdata = 0x0088},
	{.addr = 0x072, .wdata = 0x3222},
	{.addr = 0x073, .wdata = 0x7654},
	{.addr = 0x074, .wdata = 0xBA98},
	{.addr = 0x075, .wdata = 0xDDDC}
};

#define MLXBF_GIGE_UPHY_CLM_INIT_NUM_ENTRIES \
	(sizeof(mlxbf_gige_clm_init) / sizeof(struct mlxbf_gige_uphy_cfg_reg))

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_imem_init[] = {
	{.addr = 0x39C, .wdata = 0x0000},
	{.addr = 0x39D, .wdata = 0x0095},
	{.addr = 0x3BF, .wdata = 0x9027},
	{.addr = 0x39E, .wdata = 0xA8F6},
	{.addr = 0x39F, .wdata = 0xAA10},
	{.addr = 0x3A0, .wdata = 0xA8D4},
	{.addr = 0x3A1, .wdata = 0xA7AE},
	{.addr = 0x3A2, .wdata = 0xA7CC},
	{.addr = 0x3A3, .wdata = 0x9BE4},
	{.addr = 0x3A4, .wdata = 0xB2D2},
	{.addr = 0x3A5, .wdata = 0xB1F2},
	{.addr = 0x3AE, .wdata = 0x7C38},
	{.addr = 0x3AF, .wdata = 0x7C4A},
	{.addr = 0x3B0, .wdata = 0x7C25},
	{.addr = 0x3B1, .wdata = 0x7C74},
	{.addr = 0x3B2, .wdata = 0x3C00},
	{.addr = 0x3B3, .wdata = 0x3C11},
	{.addr = 0x3B4, .wdata = 0x3C5D},
	{.addr = 0x3B5, .wdata = 0x3C5D}
};

#define MLXBF_GIGE_UPHY_DLM_IMEM_INIT_NUM_ENTRIES \
	(sizeof(mlxbf_gige_dlm_imem_init) / sizeof(struct mlxbf_gige_uphy_cfg_reg))

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_seq_imem_wr_en_init = {
	.addr = 0x39A, .wdata = 0x0001
};

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_seq_imem_wr_dis_init = {
	.addr = 0x39A, .wdata = 0x0000
};

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_imem_data[] = {
	{ /* .iaddr = 0x0000 */ .wdata = 0x02DF},
	{ /* .iaddr = 0x0001 */ .wdata = 0xEEC0},
	{ /* .iaddr = 0x0002 */ .wdata = 0xD508},
	{ /* .iaddr = 0x0003 */ .wdata = 0x022F},
	{ /* .iaddr = 0x0004 */ .wdata = 0xC401},
	{ /* .iaddr = 0x0005 */ .wdata = 0xD341},
	{ /* .iaddr = 0x0006 */ .wdata = 0xC402},
	{ /* .iaddr = 0x0007 */ .wdata = 0xD342},
	{ /* .iaddr = 0x0008 */ .wdata = 0xC403},
	{ /* .iaddr = 0x0009 */ .wdata = 0xD343},
	{ /* .iaddr = 0x000A */ .wdata = 0xC404},
	{ /* .iaddr = 0x000B */ .wdata = 0xD344},
	{ /* .iaddr = 0x000C */ .wdata = 0xC417},
	{ /* .iaddr = 0x000D */ .wdata = 0xD355},
	{ /* .iaddr = 0x000E */ .wdata = 0xC418},
	{ /* .iaddr = 0x000F */ .wdata = 0xD356},
	{ /* .iaddr = 0x0010 */ .wdata = 0xF021},
	{ /* .iaddr = 0x0011 */ .wdata = 0xF003},
	{ /* .iaddr = 0x0012 */ .wdata = 0xE224},
	{ /* .iaddr = 0x0013 */ .wdata = 0x0DA9},
	{ /* .iaddr = 0x0014 */ .wdata = 0xF003},
	{ /* .iaddr = 0x0015 */ .wdata = 0xE21C},
	{ /* .iaddr = 0x0016 */ .wdata = 0xEEC1},
	{ /* .iaddr = 0x0017 */ .wdata = 0x0D87},
	{ /* .iaddr = 0x0018 */ .wdata = 0xEEC1},
	{ /* .iaddr = 0x0019 */ .wdata = 0xE806},
	{ /* .iaddr = 0x001A */ .wdata = 0xC3C5},
	{ /* .iaddr = 0x001B */ .wdata = 0xD306},
	{ /* .iaddr = 0x001C */ .wdata = 0xEEDF},
	{ /* .iaddr = 0x001D */ .wdata = 0xE806},
	{ /* .iaddr = 0x001E */ .wdata = 0xC3C6},
	{ /* .iaddr = 0x001F */ .wdata = 0xD306},
	{ /* .iaddr = 0x0020 */ .wdata = 0xF002},
	{ /* .iaddr = 0x0021 */ .wdata = 0xC3C8},
	{ /* .iaddr = 0x0022 */ .wdata = 0x409A},
	{ /* .iaddr = 0x0023 */ .wdata = 0xF021},
	{ /* .iaddr = 0x0024 */ .wdata = 0xEEE0},
	{ /* .iaddr = 0x0025 */ .wdata = 0xEEC0},
	{ /* .iaddr = 0x0026 */ .wdata = 0xD70D},
	{ /* .iaddr = 0x0027 */ .wdata = 0xC305},
	{ /* .iaddr = 0x0028 */ .wdata = 0xD328},
	{ /* .iaddr = 0x0029 */ .wdata = 0xC300},
	{ /* .iaddr = 0x002A */ .wdata = 0xD314},
	{ /* .iaddr = 0x002B */ .wdata = 0xC301},
	{ /* .iaddr = 0x002C */ .wdata = 0xD318},
	{ /* .iaddr = 0x002D */ .wdata = 0xC303},
	{ /* .iaddr = 0x002E */ .wdata = 0xD320},
	{ /* .iaddr = 0x002F */ .wdata = 0xC302},
	{ /* .iaddr = 0x0030 */ .wdata = 0xD31C},
	{ /* .iaddr = 0x0031 */ .wdata = 0xC304},
	{ /* .iaddr = 0x0032 */ .wdata = 0xD324},
	{ /* .iaddr = 0x0033 */ .wdata = 0xC358},
	{ /* .iaddr = 0x0034 */ .wdata = 0xD330},
	{ /* .iaddr = 0x0035 */ .wdata = 0xC307},
	{ /* .iaddr = 0x0036 */ .wdata = 0xD115},
	{ /* .iaddr = 0x0037 */ .wdata = 0xF021},
	{ /* .iaddr = 0x0038 */ .wdata = 0xD70D},
	{ /* .iaddr = 0x0039 */ .wdata = 0xC305},
	{ /* .iaddr = 0x003A */ .wdata = 0xD328},
	{ /* .iaddr = 0x003B */ .wdata = 0xC300},
	{ /* .iaddr = 0x003C */ .wdata = 0xD314},
	{ /* .iaddr = 0x003D */ .wdata = 0xC301},
	{ /* .iaddr = 0x003E */ .wdata = 0xD318},
	{ /* .iaddr = 0x003F */ .wdata = 0xC303},
	{ /* .iaddr = 0x0040 */ .wdata = 0xD320},
	{ /* .iaddr = 0x0041 */ .wdata = 0xC302},
	{ /* .iaddr = 0x0042 */ .wdata = 0xD31C},
	{ /* .iaddr = 0x0043 */ .wdata = 0xC304},
	{ /* .iaddr = 0x0044 */ .wdata = 0xD324},
	{ /* .iaddr = 0x0045 */ .wdata = 0xC358},
	{ /* .iaddr = 0x0046 */ .wdata = 0xD330},
	{ /* .iaddr = 0x0047 */ .wdata = 0xC307},
	{ /* .iaddr = 0x0048 */ .wdata = 0xD115},
	{ /* .iaddr = 0x0049 */ .wdata = 0xF021},
	{ /* .iaddr = 0x004A */ .wdata = 0xC70D},
	{ /* .iaddr = 0x004B */ .wdata = 0xD70F},
	{ /* .iaddr = 0x004C */ .wdata = 0xC328},
	{ /* .iaddr = 0x004D */ .wdata = 0xD305},
	{ /* .iaddr = 0x004E */ .wdata = 0xC314},
	{ /* .iaddr = 0x004F */ .wdata = 0xD300},
	{ /* .iaddr = 0x0050 */ .wdata = 0xC318},
	{ /* .iaddr = 0x0051 */ .wdata = 0xD301},
	{ /* .iaddr = 0x0052 */ .wdata = 0xC320},
	{ /* .iaddr = 0x0053 */ .wdata = 0xD303},
	{ /* .iaddr = 0x0054 */ .wdata = 0xC31C},
	{ /* .iaddr = 0x0055 */ .wdata = 0xD302},
	{ /* .iaddr = 0x0056 */ .wdata = 0xC324},
	{ /* .iaddr = 0x0057 */ .wdata = 0xD304},
	{ /* .iaddr = 0x0058 */ .wdata = 0xC330},
	{ /* .iaddr = 0x0059 */ .wdata = 0xD358},
	{ /* .iaddr = 0x005A */ .wdata = 0xC115},
	{ /* .iaddr = 0x005B */ .wdata = 0xD307},
	{ /* .iaddr = 0x005C */ .wdata = 0xF021},
	{ /* .iaddr = 0x005D */ .wdata = 0x0249},
	{ /* .iaddr = 0x005E */ .wdata = 0x0362},
	{ /* .iaddr = 0x005F */ .wdata = 0x023D},
	{ /* .iaddr = 0x0060 */ .wdata = 0xEEC1},
	{ /* .iaddr = 0x0061 */ .wdata = 0x0369},
	{ /* .iaddr = 0x0062 */ .wdata = 0xEEC1},
	{ /* .iaddr = 0x0063 */ .wdata = 0x0CEA},
	{ /* .iaddr = 0x0064 */ .wdata = 0xEEC2},
	{ /* .iaddr = 0x0065 */ .wdata = 0xD701},
	{ /* .iaddr = 0x0066 */ .wdata = 0x02C8},
	{ /* .iaddr = 0x0067 */ .wdata = 0xC3C3},
	{ /* .iaddr = 0x0068 */ .wdata = 0xD306},
	{ /* .iaddr = 0x0069 */ .wdata = 0xC3C8},
	{ /* .iaddr = 0x006A */ .wdata = 0x009A},
	{ /* .iaddr = 0x006B */ .wdata = 0xC3D1},
	{ /* .iaddr = 0x006C */ .wdata = 0xD309},
	{ /* .iaddr = 0x006D */ .wdata = 0x0C46},
	{ /* .iaddr = 0x006E */ .wdata = 0x0DE7},
	{ /* .iaddr = 0x006F */ .wdata = 0xEEC0},
	{ /* .iaddr = 0x0070 */ .wdata = 0xC3D9},
	{ /* .iaddr = 0x0071 */ .wdata = 0x0DDE},
	{ /* .iaddr = 0x0072 */ .wdata = 0x02D7},
	{ /* .iaddr = 0x0073 */ .wdata = 0xF021},
	{ /* .iaddr = 0x0074 */ .wdata = 0x1441},
	{ /* .iaddr = 0x0075 */ .wdata = 0xF003},
	{ /* .iaddr = 0x0076 */ .wdata = 0xC03F},
	{ /* .iaddr = 0x0077 */ .wdata = 0xF704},
	{ /* .iaddr = 0x0078 */ .wdata = 0xF009},
	{ /* .iaddr = 0x0079 */ .wdata = 0xE21A},
	{ /* .iaddr = 0x007A */ .wdata = 0xF002},
	{ /* .iaddr = 0x007B */ .wdata = 0x0C52},
	{ /* .iaddr = 0x007C */ .wdata = 0xE206},
	{ /* .iaddr = 0x007D */ .wdata = 0xEEC1},
	{ /* .iaddr = 0x007E */ .wdata = 0xD01A},
	{ /* .iaddr = 0x007F */ .wdata = 0x3C5D},
	{ /* .iaddr = 0x0080 */ .wdata = 0xEEC0},
	{ /* .iaddr = 0x0081 */ .wdata = 0xD01A},
	{ /* .iaddr = 0x0082 */ .wdata = 0x0E12},
	{ /* .iaddr = 0x0083 */ .wdata = 0xEEC0},
	{ /* .iaddr = 0x0084 */ .wdata = 0x13E1},
	{ /* .iaddr = 0x0085 */ .wdata = 0x1441},
	{ /* .iaddr = 0x0086 */ .wdata = 0xEEC1},
	{ /* .iaddr = 0x0087 */ .wdata = 0xD70E},
	{ /* .iaddr = 0x0088 */ .wdata = 0xD70F},
	{ /* .iaddr = 0x0089 */ .wdata = 0xEEC0},
	{ /* .iaddr = 0x008A */ .wdata = 0xD70E},
	{ /* .iaddr = 0x008B */ .wdata = 0xC458},
	{ /* .iaddr = 0x008C */ .wdata = 0x13BE},
	{ /* .iaddr = 0x008D */ .wdata = 0xEEC0},
	{ /* .iaddr = 0x008E */ .wdata = 0xF29B},
	{ /* .iaddr = 0x008F */ .wdata = 0xE20A},
	{ /* .iaddr = 0x0090 */ .wdata = 0xEEC1},
	{ /* .iaddr = 0x0091 */ .wdata = 0xD01D},
	{ /* .iaddr = 0x0092 */ .wdata = 0xEEC1},
	{ /* .iaddr = 0x0093 */ .wdata = 0xD3FD},
	{ /* .iaddr = 0x0094 */ .wdata = 0xF021}
};

#define MLXBF_GIGE_UPHY_DLM_IMEM_DATA_NUM_ENTRIES \
	(sizeof(mlxbf_gige_dlm_imem_data) / sizeof(struct mlxbf_gige_uphy_cfg_reg))

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_seq_imem_csum_en = {
	.addr = 0x39A, .wdata = 0x0004
};

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_seq_imem_csum_dis = {
	.addr = 0x39A, .wdata = 0x0000
};

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_seq_imem_bmap_clr[] = {
	{.addr = 0x39E, .wdata = 0x0000},
	{.addr = 0x39F, .wdata = 0x0000},
	{.addr = 0x3A0, .wdata = 0x0000},
	{.addr = 0x3A1, .wdata = 0x0000},
	{.addr = 0x3A2, .wdata = 0x0000},
	{.addr = 0x3A3, .wdata = 0x0000},
	{.addr = 0x3A4, .wdata = 0x0000},
	{.addr = 0x3A5, .wdata = 0x0000},
	{.addr = 0x3A6, .wdata = 0x0000},
	{.addr = 0x3A7, .wdata = 0x0000},
	{.addr = 0x3A8, .wdata = 0x0000},
	{.addr = 0x3A9, .wdata = 0x0000},
	{.addr = 0x3AA, .wdata = 0x0000},
	{.addr = 0x3AB, .wdata = 0x0000},
	{.addr = 0x3AC, .wdata = 0x0000},
	{.addr = 0x3AD, .wdata = 0x0000},
	{.addr = 0x3AE, .wdata = 0x0000},
	{.addr = 0x3AF, .wdata = 0x0000},
	{.addr = 0x3B0, .wdata = 0x0000},
	{.addr = 0x3B1, .wdata = 0x0000},
	{.addr = 0x3B2, .wdata = 0x0000},
	{.addr = 0x3B3, .wdata = 0x0000},
	{.addr = 0x3B4, .wdata = 0x0000},
	{.addr = 0x3B5, .wdata = 0x0000},
	{.addr = 0x3B6, .wdata = 0x0000},
	{.addr = 0x3B7, .wdata = 0x0000},
	{.addr = 0x3B8, .wdata = 0x0000},
	{.addr = 0x3B9, .wdata = 0x0000},
	{.addr = 0x3BA, .wdata = 0x0000},
	{.addr = 0x3BB, .wdata = 0x0000},
	{.addr = 0x3BC, .wdata = 0x0000},
	{.addr = 0x3BD, .wdata = 0x0000}
};

#define MLXBF_GIGE_DLM_SEQ_IMEM_BMAP_CLR_NUM_ENTRIES \
	(sizeof(mlxbf_gige_dlm_seq_imem_bmap_clr) / sizeof(struct mlxbf_gige_uphy_cfg_reg))

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_tx_init[] = {
	{.addr = 0x002, .wdata = 0x5125},
	{.addr = 0x01C, .wdata = 0x0018},
	{.addr = 0x01E, .wdata = 0x0E00},
	{.addr = 0x01F, .wdata = 0xC200},
	{.addr = 0x023, .wdata = 0x0277},
	{.addr = 0x024, .wdata = 0x026B},
	{.addr = 0x053, .wdata = 0x0700},
	{.addr = 0x059, .wdata = 0x1011},
	{.addr = 0x060, .wdata = 0x0000},
	{.addr = 0x062, .wdata = 0x0135},
	{.addr = 0x063, .wdata = 0x0443},
	{.addr = 0x064, .wdata = 0x0000},
	{.addr = 0x066, .wdata = 0x0061},
	{.addr = 0x067, .wdata = 0x0042},
	{.addr = 0x06A, .wdata = 0x1212},
	{.addr = 0x06B, .wdata = 0x1515},
	{.addr = 0x06C, .wdata = 0x011A},
	{.addr = 0x06D, .wdata = 0x0132},
	{.addr = 0x06E, .wdata = 0x0632},
	{.addr = 0x06F, .wdata = 0x0643},
	{.addr = 0x070, .wdata = 0x0233},
	{.addr = 0x071, .wdata = 0x0433},
	{.addr = 0x07E, .wdata = 0x6A08},
	{.addr = 0x08D, .wdata = 0x2101},
	{.addr = 0x093, .wdata = 0x0015},
	{.addr = 0x096, .wdata = 0x7555},
	{.addr = 0x0A9, .wdata = 0xE754},
	{.addr = 0x0AA, .wdata = 0x7ED1},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000}
};

#define MLXBF_GIGE_DLM_TX_NUM_ENTRIES \
	(sizeof(mlxbf_gige_dlm_tx_init) / sizeof(struct mlxbf_gige_uphy_cfg_reg))

static const struct mlxbf_gige_uphy_cfg_reg
mlxbf_gige_dlm_rx_init[] = {
	{.addr = 0x003, .wdata = 0x5125},
	{.addr = 0x01D, .wdata = 0x0004},
	{.addr = 0x028, .wdata = 0x1000},
	{.addr = 0x029, .wdata = 0x1001},
	{.addr = 0x02E, .wdata = 0x0004},
	{.addr = 0x053, .wdata = 0x0700},
	{.addr = 0x057, .wdata = 0x5044},
	{.addr = 0x05B, .wdata = 0x1011},
	{.addr = 0x0D2, .wdata = 0x0002},
	{.addr = 0x0D9, .wdata = 0x0000},
	{.addr = 0x0DA, .wdata = 0x0000},
	{.addr = 0x0DB, .wdata = 0x0000},
	{.addr = 0x0E2, .wdata = 0x0000},
	{.addr = 0x0E7, .wdata = 0xBB10},
	{.addr = 0x0E8, .wdata = 0xBB10},
	{.addr = 0x0EC, .wdata = 0x0111},
	{.addr = 0x0ED, .wdata = 0x1C00},
	{.addr = 0x0F5, .wdata = 0x0000},
	{.addr = 0x102, .wdata = 0x0CA6},
	{.addr = 0x107, .wdata = 0x0020},
	{.addr = 0x10C, .wdata = 0x1E31},
	{.addr = 0x10D, .wdata = 0x1D29},
	{.addr = 0x111, .wdata = 0x00E7},
	{.addr = 0x112, .wdata = 0x5202},
	{.addr = 0x117, .wdata = 0x0493},
	{.addr = 0x11B, .wdata = 0x0148},
	{.addr = 0x120, .wdata = 0x23DE},
	{.addr = 0x121, .wdata = 0x2294},
	{.addr = 0x125, .wdata = 0x03FF},
	{.addr = 0x126, .wdata = 0x25F0},
	{.addr = 0x12B, .wdata = 0xC633},
	{.addr = 0x136, .wdata = 0x0F6A},
	{.addr = 0x143, .wdata = 0x0000},
	{.addr = 0x148, .wdata = 0x0001},
	{.addr = 0x14E, .wdata = 0x0000},
	{.addr = 0x155, .wdata = 0x2003},
	{.addr = 0x15C, .wdata = 0x099B},
	{.addr = 0x161, .wdata = 0x0088},
	{.addr = 0x16B, .wdata = 0x0433},
	{.addr = 0x172, .wdata = 0x099B},
	{.addr = 0x17C, .wdata = 0x045D},
	{.addr = 0x17D, .wdata = 0x006A},
	{.addr = 0x181, .wdata = 0x0000},
	{.addr = 0x189, .wdata = 0x1590},
	{.addr = 0x18E, .wdata = 0x0080},
	{.addr = 0x18F, .wdata = 0x90EC},
	{.addr = 0x191, .wdata = 0x79F8},
	{.addr = 0x194, .wdata = 0x000A},
	{.addr = 0x195, .wdata = 0x000A},
	{.addr = 0x1EB, .wdata = 0x0133},
	{.addr = 0x1F0, .wdata = 0x0030},
	{.addr = 0x1F1, .wdata = 0x0030},
	{.addr = 0x1F5, .wdata = 0x3737},
	{.addr = 0x1F6, .wdata = 0x3737},
	{.addr = 0x1FA, .wdata = 0x2C00},
	{.addr = 0x1FF, .wdata = 0x0516},
	{.addr = 0x200, .wdata = 0x0516},
	{.addr = 0x204, .wdata = 0x3010},
	{.addr = 0x209, .wdata = 0x0429},
	{.addr = 0x20E, .wdata = 0x0010},
	{.addr = 0x213, .wdata = 0x005A},
	{.addr = 0x214, .wdata = 0x0000},
	{.addr = 0x216, .wdata = 0x0000},
	{.addr = 0x218, .wdata = 0x0000},
	{.addr = 0x225, .wdata = 0x0000},
	{.addr = 0x22A, .wdata = 0x0000},
	{.addr = 0x22B, .wdata = 0x0000},
	{.addr = 0x231, .wdata = 0x0000},
	{.addr = 0x232, .wdata = 0x0000},
	{.addr = 0x233, .wdata = 0x0000},
	{.addr = 0x245, .wdata = 0x0300},
	{.addr = 0x24A, .wdata = 0x0000},
	{.addr = 0x24F, .wdata = 0xFFF3},
	{.addr = 0x254, .wdata = 0x0000},
	{.addr = 0x259, .wdata = 0x0000},
	{.addr = 0x25E, .wdata = 0x0000},
	{.addr = 0x265, .wdata = 0x0009},
	{.addr = 0x267, .wdata = 0x0174},
	{.addr = 0x271, .wdata = 0x01F0},
	{.addr = 0x273, .wdata = 0x0170},
	{.addr = 0x275, .wdata = 0x7828},
	{.addr = 0x279, .wdata = 0x3E3A},
	{.addr = 0x27D, .wdata = 0x8468},
	{.addr = 0x283, .wdata = 0x000C},
	{.addr = 0x285, .wdata = 0x7777},
	{.addr = 0x288, .wdata = 0x5503},
	{.addr = 0x28C, .wdata = 0x0030},
	{.addr = 0x28E, .wdata = 0xBBBB},
	{.addr = 0x290, .wdata = 0xBBBB},
	{.addr = 0x293, .wdata = 0x0021},
	{.addr = 0x2FA, .wdata = 0x3B40},
	{.addr = 0x2FB, .wdata = 0x7777},
	{.addr = 0x30A, .wdata = 0x8022},
	{.addr = 0x319, .wdata = 0x205E},
	{.addr = 0x31B, .wdata = 0x0000},
	{.addr = 0x31D, .wdata = 0x6004},
	{.addr = 0x320, .wdata = 0x3014},
	{.addr = 0x322, .wdata = 0x6004},
	{.addr = 0x326, .wdata = 0x6004},
	{.addr = 0x32A, .wdata = 0x5000},
	{.addr = 0x32E, .wdata = 0x5000},
	{.addr = 0x332, .wdata = 0x6004},
	{.addr = 0x336, .wdata = 0x6063},
	{.addr = 0x389, .wdata = 0x0310},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000},
	{.addr = 0x3FF, .wdata = 0x0000}
};

#define MLXBF_GIGE_DLM_RX_NUM_ENTRIES \
	(sizeof(mlxbf_gige_dlm_rx_init) / sizeof(struct mlxbf_gige_uphy_cfg_reg))

/* returns plu clock p1clk in Hz */
static u64 mlxbf_gige_calculate_p1clk(struct mlxbf_gige *priv)
{
	u8 core_od, core_r;
	u64 freq_output;
	u32 reg1, reg2;
	u32 core_f;

	reg1 = readl(priv->clk_io + MLXBF_GIGE_P1CLK_REG1);
	reg2 = readl(priv->clk_io + MLXBF_GIGE_P1CLK_REG2);

	core_f = (reg1 & MLXBF_GIGE_P1_CORE_F_MASK) >>
		MLXBF_GIGE_P1_CORE_F_SHIFT;
	core_r = (reg1 & MLXBF_GIGE_P1_CORE_R_MASK) >>
		MLXBF_GIGE_P1_CORE_R_SHIFT;
	core_od = (reg2 & MLXBF_GIGE_P1_CORE_OD_MASK) >>
		MLXBF_GIGE_P1_CORE_OD_SHIFT;

	/* Compute PLL output frequency as follow:
	 *
	 *                                     CORE_F / 16384
	 * freq_output = freq_reference * ----------------------------
	 *                              (CORE_R + 1) * (CORE_OD + 1)
	 */
	freq_output = div_u64(MLXBF_GIGE_P1_FREQ_REFERENCE * core_f,
			      MLXBF_GIGE_P1_CLK_CONST);
	freq_output = div_u64(freq_output, (core_r + 1) * (core_od + 1));

	return freq_output;
}

static void mlxbf_gige_ugl_static_config(struct mlxbf_gige *priv)
{
	u32 val, p1clk_mhz;
	u32 const_factor;
	u64 p1clk;

	/* p1clk is the PLU clock in Hz */
	p1clk = mlxbf_gige_calculate_p1clk(priv);

	/* get p1clk in MHz */
	p1clk_mhz = div_u64(p1clk, 1000000);

	/* Multiply the p1clk clock by 12 according to HW requirements */
	const_factor = p1clk_mhz * MLXBF_GIGE_P1CLK_MULT_FACTOR;

	/* ugl_cr_bridge_desc */
	val = readl(priv->plu_base + MLXBF_GIGE_UGL_CR_BRIDGE_DESC);
	val &= ~MLXBF_GIGE_UGL_CR_BRIDGE_ALL_MASK;
	val |= FIELD_PREP(MLXBF_GIGE_UGL_CR_BRIDGE_SETUP_MASK,
			  MLXBF_GIGE_UGL_CR_BRIDGE_SETUP_VAL(const_factor));
	val |= FIELD_PREP(MLXBF_GIGE_UGL_CR_BRIDGE_PULSE_MASK,
			  MLXBF_GIGE_UGL_CR_BRIDGE_PULSE_VAL(const_factor));
	val |= FIELD_PREP(MLXBF_GIGE_UGL_CR_BRIDGE_HOLD_MASK,
			  MLXBF_GIGE_UGL_CR_BRIDGE_HOLD_VAL(const_factor));
	writel(val, priv->plu_base + MLXBF_GIGE_UGL_CR_BRIDGE_DESC);

	/* pll1x_fsm_counters */
	val = MLXBF_GIGE_PLL1X_FSM_DEFAULT_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_PLL1X_FSM_DEFAULT_CYCLES);

	val = MLXBF_GIGE_PLL1X_FSM_SLEEP_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_PLL1X_FSM_SLEEP_CYCLES);

	val = MLXBF_GIGE_PLL1X_FSM_RCAL_FLOW_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_PLL1X_FSM_RCAL_FLOW_CYCLES);

	val = MLXBF_GIGE_PLL1X_FSM_CAL_FLOW_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_PLL1X_FSM_CAL_FLOW_CYCLES);

	val = readl(priv->plu_base + MLXBF_GIGE_PLL1X_FSM_LOCKDET_STS_CYCLES);
	val &= ~MLXBF_GIGE_PLL1X_FSM_LOCKDET_STS_MASK;
	val |= FIELD_PREP(MLXBF_GIGE_PLL1X_FSM_LOCKDET_STS_MASK,
			  MLXBF_GIGE_PLL1X_FSM_LOCKDET_STS_VAL(const_factor));
	writel(val, priv->plu_base + MLXBF_GIGE_PLL1X_FSM_LOCKDET_STS_CYCLES);

	/* tx_fsm_counters */
	val = MLXBF_GIGE_TX_FSM_DEFAULT_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_TX_FSM_DEFAULT_CYCLES);

	val = MLXBF_GIGE_TX_FSM_SLEEP_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_TX_FSM_SLEEP_CYCLES);

	val = MLXBF_GIGE_TX_FSM_POWERUP_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_TX_FSM_POWERUP_CYCLES);

	val = MLXBF_GIGE_TX_FSM_CAL_FLOW_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_TX_FSM_CAL_FLOW_CYCLES);

	val = readl(priv->plu_base + MLXBF_GIGE_TX_FSM_CAL_ABORT_CYCLES);
	val &= ~MLXBF_GIGE_TX_FSM_CAL_ABORT_MASK;
	val |= FIELD_PREP(MLXBF_GIGE_TX_FSM_CAL_ABORT_MASK,
			  MLXBF_GIGE_TX_FSM_CAL_ABORT_VAL(const_factor));
	writel(val, priv->plu_base + MLXBF_GIGE_TX_FSM_CAL_ABORT_CYCLES);

	/* rx_fsm_counters */
	val = MLXBF_GIGE_RX_FSM_DEFAULT_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_DEFAULT_CYCLES);

	val = MLXBF_GIGE_RX_FSM_SLEEP_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_SLEEP_CYCLES);

	val = MLXBF_GIGE_RX_FSM_POWERUP_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_POWERUP_CYCLES);

	val = MLXBF_GIGE_RX_FSM_TERM_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_TERM_CYCLES);

	val = MLXBF_GIGE_RX_FSM_CAL_FLOW_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_CAL_FLOW_CYCLES);

	val = MLXBF_GIGE_RX_FSM_CAL_ABORT_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_CAL_ABORT_CYCLES);

	val = MLXBF_GIGE_RX_FSM_EQ_FLOW_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_EQ_FLOW_CYCLES);

	val = MLXBF_GIGE_RX_FSM_EQ_ABORT_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_EQ_ABORT_CYCLES);

	val = MLXBF_GIGE_RX_FSM_EOM_FLOW_VAL(const_factor);
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_EOM_FLOW_CYCLES);

	val = readl(priv->plu_base + MLXBF_GIGE_RX_FSM_CDR_LOCK_CYCLES);
	val &= ~MLXBF_GIGE_RX_FSM_CDR_LOCK_MASK;
	val |= FIELD_PREP(MLXBF_GIGE_RX_FSM_CDR_LOCK_MASK,
			  MLXBF_GIGE_RX_FSM_CDR_LOCK_VAL(const_factor));
	writel(val, priv->plu_base + MLXBF_GIGE_RX_FSM_CDR_LOCK_CYCLES);

	/* periodic_flows_timer_max_value */
	val = readl(priv->plu_base + MLXBF_GIGE_PERIOD_FLOWS_TIMER_MAX);
	val &= ~MLXBF_GIGE_PERIOD_FLOWS_TIMER_MAX_MASK;
	val |= FIELD_PREP(MLXBF_GIGE_PERIOD_FLOWS_TIMER_MAX_MASK,
			  MLXBF_GIGE_PERIOD_FLOWS_TIMER_MAX_VAL(const_factor));
	writel(val, priv->plu_base + MLXBF_GIGE_PERIOD_FLOWS_TIMER_MAX);

	/* plltop.center.iddq_cycles */
	val = readl(priv->plu_base + MLXBF_GIGE_PLL_IDDQ_CYCLES);
	val &= ~MLXBF_GIGE_PLL_IDDQ_CYCLES_MASK;
	val |= FIELD_PREP(MLXBF_GIGE_PLL_IDDQ_CYCLES_MASK,
			  MLXBF_GIGE_PLL_IDDQ_CYCLES_VAL(const_factor));
	writel(val, priv->plu_base + MLXBF_GIGE_PLL_IDDQ_CYCLES);

	/* lanetop.center.iddq_cycles */
	val = readl(priv->plu_base + MLXBF_GIGE_LANE_IDDQ_CYCLES);
	val &= ~MLXBF_GIGE_LANE_IDDQ_CYCLES_MASK;
	val |= FIELD_PREP(MLXBF_GIGE_LANE_IDDQ_CYCLES_MASK,
			  MLXBF_GIGE_LANE_IDDQ_CYCLES_VAL(const_factor));
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_IDDQ_CYCLES);

	/* lanetop.center.power_governor0 */
	val = FIELD_PREP(MLXBF_GIGE_LANE_PWR_GOV0_RISE_MASK,
			 MLXBF_GIGE_LANE_PWR_GOV0_RISE_VAL(const_factor));
	val |= FIELD_PREP(MLXBF_GIGE_LANE_PWR_GOV0_FALL_MASK,
			  MLXBF_GIGE_LANE_PWR_GOV0_FALL_VAL(const_factor));
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_PWR_GOV0);
}

static int mlxbf_gige_uphy_gw_write(struct mlxbf_gige *priv, u16 addr,
				    u16 data, bool is_pll)
{
	u32 cmd, val;
	int ret;

	cmd = MLXBF_GIGE_UPHY_GW_CREATE_CMD(addr, data, 0, is_pll);

	/* Send PLL or lane GW write request */
	writel(cmd, priv->plu_base + MLXBF_GIGE_UPHY_GW(is_pll));

	/* If the poll times out, drop the request */
	ret = readl_poll_timeout_atomic(priv->plu_base +
					MLXBF_GIGE_UPHY_GW(is_pll),
					val,
					!(val & MLXBF_GIGE_UPHY_GW_BUSY_MASK(is_pll)),
					5, 1000000);
	if (ret)
		dev_dbg(priv->dev, "Failed to send GW write request\n");

	return ret;
}

static int mlxbf_gige_uphy_gw_read(struct mlxbf_gige *priv, u16 addr,
				   bool is_pll)
{
	u32 cmd, val;
	int ret;

	cmd = MLXBF_GIGE_UPHY_GW_CREATE_CMD(addr, 0, 1, is_pll);

	/* Send PLL or lane GW read request */
	writel(cmd, priv->plu_base + MLXBF_GIGE_UPHY_GW(is_pll));

	/* If the poll times out, drop the request */
	ret = readl_poll_timeout_atomic(priv->plu_base +
					MLXBF_GIGE_UPHY_GW(is_pll),
					val,
					!(val & MLXBF_GIGE_UPHY_GW_BUSY_MASK(is_pll)),
					5, 1000000);
	if (ret) {
		dev_dbg(priv->dev, "Failed to send GW read request\n");
		return ret;
	}

	val = readl(priv->plu_base + MLXBF_GIGE_UPHY_GW_DESC0(is_pll));
	val &= MLXBF_GIGE_UPHY_GW_DESC0_DATA_MASK(is_pll);

	return val;
}

static int mlxbf_gige_load_uphy_clm_init_pkg(struct mlxbf_gige *priv)
{
	int ret = 0;
	int i;

	for (i = 0; i < MLXBF_GIGE_UPHY_CLM_INIT_NUM_ENTRIES; i++) {
		ret = mlxbf_gige_uphy_gw_write(priv,
					       mlxbf_gige_clm_init[i].addr,
					       mlxbf_gige_clm_init[i].wdata,
					       true);
		if (ret) {
			dev_dbg(priv->dev, "Failed to load clm init pkg\n");
			return ret;
		}
	}

	return ret;
}

static int mlxbf_gige_load_clm_production_fuses(struct mlxbf_gige *priv)
{
	u8 bg_trim_room;
	u8 cvb_trim_room;
	u8 speedo_room;
	int ret;
	u32 val;

	val = readl(priv->fuse_gw_io);
	bg_trim_room = (val & MLXBF_GIGE_YU_BG_TRIM_ROOM_MASK) >>
			MLXBF_GIGE_YU_BG_TRIM_ROOM_SHIFT;
	cvb_trim_room = (val & MLXBF_GIGE_YU_CVB_TRIM_ROOM_MASK) >>
			MLXBF_GIGE_YU_CVB_TRIM_ROOM_SHIFT;
	speedo_room = (val & MLXBF_GIGE_YU_SPEEDO_ROOM_MASK) >>
			MLXBF_GIGE_YU_SPEEDO_ROOM_SHIFT;

	val = ((bg_trim_room >> MLXBF_GIGE_YU_FUSE_VALID_SHIFT) <<
		MLXBF_GIGE_PLL_MGMT_BGAP_FUSE_CTRL_BG_TRIM_VLD_SHIFT);
	val |= ((cvb_trim_room >> MLXBF_GIGE_YU_FUSE_VALID_SHIFT) <<
		MLXBF_GIGE_PLL_MGMT_BGAP_FUSE_CTRL_CVB_TRIM_VLD_SHIFT);
	val |= ((speedo_room >> MLXBF_GIGE_YU_FUSE_VALID_SHIFT) <<
		MLXBF_GIGE_PLL_MGMT_BGAP_FUSE_CTRL_SPEEDO_VLD_SHIFT);
	val |= ((bg_trim_room & MLXBF_GIGE_YU_FUSE_MASK) <<
		MLXBF_GIGE_PLL_MGMT_BGAP_FUSE_CTRL_BG_TRIM_SHIFT);
	val |= ((cvb_trim_room & MLXBF_GIGE_YU_FUSE_MASK) <<
		MLXBF_GIGE_PLL_MGMT_BGAP_FUSE_CTRL_CVB_TRIM_SHIFT);
	val |= ((speedo_room & MLXBF_GIGE_YU_FUSE_MASK) <<
		MLXBF_GIGE_PLL_MGMT_BGAP_FUSE_CTRL_SPEEDO_SHIFT);

	ret = mlxbf_gige_uphy_gw_write(priv, MLXBF_GIGE_MGMT_BGAP_FUSE_CTRL_ADDR, val, true);
	if (ret)
		dev_dbg(priv->dev, "Failed to load clm production fuses\n");

	return ret;
}

static int mlxbf_gige_init_pll(struct mlxbf_gige *priv)
{
	int ret;

	ret = mlxbf_gige_load_uphy_clm_init_pkg(priv);
	if (ret)
		return ret;

	ret = mlxbf_gige_load_clm_production_fuses(priv);

	return ret;
}

static int mlxbf_gige_lock_pll(struct mlxbf_gige *priv)
{
	int ret;
	u32 val;

	/* plltop.center.uphy_pll_rst_reg_ */
	val = readl(priv->plu_base + MLXBF_GIGE_UPHY_PLL_RST_REG);
	val |= MLXBF_GIGE_UPHY_PLL_RST_REG_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_UPHY_PLL_RST_REG);

	/* cause_or.clrcause.bulk */
	val = readl(priv->plu_base + MLXBF_GIGE_PLL1X_CAUSE_CLRCAUSE_BULK);
	val |= MLXBF_GIGE_PLL1X_CAUSE_CLRCAUSE_BULK_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_PLL1X_CAUSE_CLRCAUSE_BULK);

	writel(0, priv->plu_base + MLXBF_GIGE_PLL_CAL);

	/* Stop polling when fsm state is UGL_PLL1X_FSM_STATE_SLEEP */
	ret = readl_poll_timeout_atomic(priv->plu_base +
					MLXBF_GIGE_PLL_FSM_CTRL,
					val, (val == MLXBF_GIGE_UGL_PLL1X_FSM_STATE_SLEEP),
					5, 1000000);
	if (ret) {
		dev_dbg(priv->dev, "Polling timeout on fsm state sleep\n");
		return ret;
	}

	udelay(MLXBF_GIGE_PLL_STAB_TIME);

	val = readl(priv->plu_base + MLXBF_GIGE_PLL_SLEEP_FW);
	val |= MLXBF_GIGE_PLL_SLEEP_FW_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_PLL_SLEEP_FW);

	udelay(MLXBF_GIGE_PLL_STAB_TIME);
	writel(MLXBF_GIGE_PLL_RCAL_MASK, priv->plu_base + MLXBF_GIGE_PLL_RCAL);

	/* Stop polling when fsm state is UGL_PLL1X_FSM_STATE_IDLE */
	ret = readl_poll_timeout_atomic(priv->plu_base +
					MLXBF_GIGE_PLL_FSM_CTRL,
					val, (val == MLXBF_GIGE_UGL_PLL1X_FSM_STATE_IDLE),
					5, 1000000);
	if (ret) {
		dev_dbg(priv->dev, "Polling timeout on fsm state idle\n");
		return ret;
	}

	val = readl(priv->plu_base + MLXBF_GIGE_PLL_SLEEP_FW);
	val &= ~MLXBF_GIGE_PLL_SLEEP_FW_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_PLL_SLEEP_FW);

	writel(MLXBF_GIGE_PLL_CAL_MASK, priv->plu_base + MLXBF_GIGE_PLL_CAL);

	/* Stop polling when cal_valid is different from 0 */
	ret = readl_poll_timeout_atomic(priv->plu_base + MLXBF_GIGE_PLL_CAL_VLD,
					val, !!(val & MLXBF_GIGE_PLL_CAL_VLD_MASK),
					5, 1000000);
	if (ret) {
		dev_dbg(priv->dev, "Polling timeout on cal_valid\n");
		return ret;
	}

	/* pll_enable */
	val = readl(priv->plu_base + MLXBF_GIGE_PLL_ENABLE);
	val |= MLXBF_GIGE_PLL_ENABLE_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_PLL_ENABLE);

	return ret;
}

static void mlxbf_gige_get_lane_out_of_rst(struct mlxbf_gige *priv)
{
	u32 val;

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_RST_REG);
	val |= MLXBF_GIGE_LANE_RST_REG_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_RST_REG);
}

static int mlxbf_gige_load_imem(struct mlxbf_gige *priv)
{
	u16 csum_status;
	int ret = 0;
	int i;

	for (i = 0; i < MLXBF_GIGE_UPHY_DLM_IMEM_INIT_NUM_ENTRIES; i++) {
		ret = mlxbf_gige_uphy_gw_write(priv,
					       mlxbf_gige_dlm_imem_init[i].addr,
					       mlxbf_gige_dlm_imem_init[i].wdata,
					       false);
		if (ret)
			return ret;
	}

	/* Resets the internal counter for MLXBF_GIGE_DLM_IMEM_DATA_ADDR to base address */
	ret = mlxbf_gige_uphy_gw_write(priv,
				       mlxbf_gige_dlm_seq_imem_wr_en_init.addr,
				       mlxbf_gige_dlm_seq_imem_wr_en_init.wdata,
				       false);
	if (ret)
		return ret;

	/* HW increments the address MLXBF_GIGE_DLM_IMEM_DATA_ADDR internally. */
	for (i = 0; i < MLXBF_GIGE_UPHY_DLM_IMEM_DATA_NUM_ENTRIES; i++) {
		ret = mlxbf_gige_uphy_gw_write(priv,
					       MLXBF_GIGE_LANE_IMEM_DATA_ADDR,
					       mlxbf_gige_dlm_imem_data[i].wdata,
					       false);
		if (ret)
			return ret;
	}

	ret = mlxbf_gige_uphy_gw_write(priv,
				       mlxbf_gige_dlm_seq_imem_wr_dis_init.addr,
				       mlxbf_gige_dlm_seq_imem_wr_dis_init.wdata,
				       false);
	if (ret)
		return ret;

	ret = mlxbf_gige_uphy_gw_write(priv,
				       mlxbf_gige_dlm_seq_imem_csum_en.addr,
				       mlxbf_gige_dlm_seq_imem_csum_en.wdata,
				       false);
	if (ret)
		return ret;

	udelay(MLXBF_GIGE_PLL_DLM_IMEM_CSUM_TIMEOUT);

	ret = mlxbf_gige_uphy_gw_read(priv, MLXBF_GIGE_LANE_CSUM_STS_ADDR, false);
	if (ret < 0)
		return ret;

	csum_status = ((ret & MLXBF_GIGE_IMEM_CSUM_STATUS_MASK) >>
			MLXBF_GIGE_IMEM_CSUM_STATUS_SHIFT);

	ret = mlxbf_gige_uphy_gw_write(priv,
				       mlxbf_gige_dlm_seq_imem_csum_dis.addr,
				       mlxbf_gige_dlm_seq_imem_csum_dis.wdata,
				       false);
	if (ret)
		return ret;

	if (csum_status != MLXBF_GIGE_IMEM_CSUM_RUN_AND_VALID) {
		dev_err(priv->dev, "%s: invalid checksum\n", __func__);

		/* recovery flow */
		for (i = 0; i < MLXBF_GIGE_DLM_SEQ_IMEM_BMAP_CLR_NUM_ENTRIES; i++) {
			mlxbf_gige_uphy_gw_write(priv,
						 mlxbf_gige_dlm_seq_imem_bmap_clr[i].addr,
						 mlxbf_gige_dlm_seq_imem_bmap_clr[i].wdata,
						 false);
		}

		return MLXBF_GIGE_INVALID_IMEM_CSUM;
	}

	return ret;
}

static int mlxbf_gige_plu_tx_power_ctrl(struct mlxbf_gige *priv, bool is_pwr_on)
{
	int ret = 0;
	u32 val;

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_RATE_ID0_SPEED);
	val &= ~MLXBF_GIGE_LANE_TX_SLEEP_VAL_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_RATE_ID0_SPEED);

	if (is_pwr_on) {
		val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);
		val &= ~MLXBF_GIGE_LANE_TX_IDDQ_VAL_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);

		val = readl(priv->plu_base + MLXBF_GIGE_PLU_POWERUP);
		val |= MLXBF_GIGE_PLU_TX_POWERUP_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_PLU_POWERUP);
	} else {
		val = readl(priv->plu_base + MLXBF_GIGE_PLU_POWERUP);
		val &= ~MLXBF_GIGE_PLU_TX_POWERUP_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_PLU_POWERUP);

		val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);
		val |= MLXBF_GIGE_LANE_TX_IDDQ_VAL_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);

		ret = readl_poll_timeout_atomic(priv->plu_base +
			MLXBF_GIGE_LANE_TX_FSM_CTRL, val,
			((val & MLXBF_GIGE_LANE_TX_FSM_PS_MASK) == MLXBF_GIGE_TX_FSM_IDDQ),
			5, 1000000);
		if (ret)
			dev_dbg(priv->dev, "Polling timeout on tx fsm iddq state\n");
	}

	return ret;
}

static int mlxbf_gige_dlm_tx_init_pkg(struct mlxbf_gige *priv)
{
	int ret = 0;
	int i;

	for (i = 0; i < MLXBF_GIGE_DLM_TX_NUM_ENTRIES; i++) {
		ret = mlxbf_gige_uphy_gw_write(priv,
					       mlxbf_gige_dlm_tx_init[i].addr,
					       mlxbf_gige_dlm_tx_init[i].wdata,
					       false);
		if (ret) {
			dev_dbg(priv->dev, "Failed to load dlm tx init pkg\n");
			return ret;
		}
	}

	return ret;
}

static int mlxbf_gige_tx_lane_open(struct mlxbf_gige *priv)
{
	u32 val;
	int ret;

	/* Prepare the TX lane before opening it */

	ret = mlxbf_gige_plu_tx_power_ctrl(priv, false);
	if (ret)
		return ret;

	/* Calibration of TX elastic buffer */
	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_BITS_SWAP);
	val &= ~MLXBF_GIGE_TX_EB_BLOCK_PUSH_DIST_MASK_MASK;
	val |= MLXBF_GIGE_TX_EB_BLOCK_PUSH_DIST_MASK_VAL;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_BITS_SWAP);

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);
	val |= MLXBF_GIGE_LANE_TX_DATA_EN_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);

	writel(MLXBF_GIGE_LANE_TX_CAL_MASK, priv->plu_base + MLXBF_GIGE_LANE_TX_CAL);

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);
	val &= ~MLXBF_GIGE_LANE_TX_RATE_ID_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_RATE_ID0_SPEED);
	val &= ~MLXBF_GIGE_LANE_TX_RATE_ID0_SPEED_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_RATE_ID0_SPEED);

	/* Loading the DLM tx init package should be done before lane power on */
	ret = mlxbf_gige_dlm_tx_init_pkg(priv);
	if (ret)
		return ret;

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_BITS_SWAP);
	val &= ~MLXBF_GIGE_LANE_TX_BITS_SWAP_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_BITS_SWAP);

	ret = mlxbf_gige_plu_tx_power_ctrl(priv, true);
	if (ret)
		return ret;

	/* After preparing the TX lane, open it for data transmission */

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_BITS_SWAP);
	val &= ~MLXBF_GIGE_TX_EB_BLOCK_PUSH_DIST_MASK_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_BITS_SWAP);

	ret = readl_poll_timeout_atomic(priv->plu_base +
			MLXBF_GIGE_LANE_TX_FSM_CTRL, val,
			((val & MLXBF_GIGE_LANE_TX_FSM_PS_MASK) == MLXBF_GIGE_TX_DATA_EN),
			5, 1000000);
	if (ret) {
		dev_dbg(priv->dev, "Polling timeout on fsm tx data enable state\n");
		return ret;
	}

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);
	val |= MLXBF_GIGE_LANE_TX_PERIODIC_CAL_EN_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_TX_DATA_EN);

	return ret;
}

static int mlxbf_gige_dlm_rx_init_pkg(struct mlxbf_gige *priv)
{
	int ret = 0;
	int i;

	for (i = 0; i < MLXBF_GIGE_DLM_RX_NUM_ENTRIES; i++) {
		ret = mlxbf_gige_uphy_gw_write(priv,
					       mlxbf_gige_dlm_rx_init[i].addr,
					       mlxbf_gige_dlm_rx_init[i].wdata,
					       false);
		if (ret) {
			dev_dbg(priv->dev, "Failed to load dlm rx init pkg\n");
			return ret;
		}
	}

	return ret;
}

static int mlxbf_gige_plu_rx_power_ctrl(struct mlxbf_gige *priv, bool is_pwr_on)
{
	int ret = 0;
	u32 val;

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);
	val &= ~MLXBF_GIGE_LANE_RX_SLEEP_VAL_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);

	if (is_pwr_on) {
		val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);
		val &= ~MLXBF_GIGE_LANE_RX_IDDQ_VAL_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);

		val = readl(priv->plu_base + MLXBF_GIGE_PLU_POWERUP);
		val |= MLXBF_GIGE_PLU_RX_POWERUP_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_PLU_POWERUP);
	} else {
		/* Enable HW watchdogs. */
		val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_EQ_DONE_TIMER_EN);
		val |= MLXBF_GIGE_LANE_RX_EQ_DONE_TIMER_EN_MASK;
		val |= MLXBF_GIGE_LANE_RX_CAL_DONE_TIMER_EN_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_EQ_DONE_TIMER_EN);

		val = readl(priv->plu_base + MLXBF_GIGE_PLU_POWERUP);
		val &= ~MLXBF_GIGE_PLU_RX_POWERUP_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_PLU_POWERUP);

		val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);
		val |= MLXBF_GIGE_LANE_RX_IDDQ_VAL_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);

		ret = readl_poll_timeout_atomic(priv->plu_base +
			MLXBF_GIGE_LANE_RX_FSM_CTRL, val,
			((val & MLXBF_GIGE_LANE_RX_FSM_PS_MASK) == MLXBF_GIGE_RX_FSM_IDDQ),
			5, 1000000);
		if (ret) {
			dev_dbg(priv->dev, "Polling timeout on rx fsm iddq state\n");
			return ret;
		}

		val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_EQ_DONE_TIMER_EN);
		val &= ~MLXBF_GIGE_LANE_RX_EQ_DONE_TIMER_EN_MASK;
		val &= ~MLXBF_GIGE_LANE_RX_CAL_DONE_TIMER_EN_MASK;
		writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_EQ_DONE_TIMER_EN);
	}

	return ret;
}

static int mlxbf_gige_rx_lane_open(struct mlxbf_gige *priv)
{
	u32 val;
	int ret;

	ret = mlxbf_gige_plu_rx_power_ctrl(priv, false);
	if (ret)
		return ret;

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);
	val &= ~MLXBF_GIGE_LANE_RX_RATE_ID_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_SYNC_FIFO_POP);
	val &= ~MLXBF_GIGE_LANE_RX_SYNC_FIFO_POP_RDY_CHICKEN_MASK;
	val &= ~MLXBF_GIGE_LANE_RX_DATA_SPLIT_LSB_VLD_CHICKEN_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_SYNC_FIFO_POP);

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);
	val &= ~MLXBF_GIGE_LANE_RX_RATE_ID0_SPEED_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_RATE_ID);

	ret = mlxbf_gige_dlm_rx_init_pkg(priv);
	if (ret)
		return ret;

	writel(MLXBF_GIGE_LANE_RX_CAL_MASK, priv->plu_base + MLXBF_GIGE_LANE_RX_CAL);

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_SYNC_FIFO_POP);
	val &= ~MLXBF_GIGE_LANE_RX_CDR_RESET_REG_MASK;
	val |= MLXBF_GIGE_LANE_RX_CDR_EN_MASK;
	val |= MLXBF_GIGE_LANE_RX_DATA_EN_MASK;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_SYNC_FIFO_POP);

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_EQ_TRAIN);
	val &= ~MLXBF_GIGE_LANE_RX_EQ_TRAIN_MASK;
	val |= MLXBF_GIGE_LANE_RX_EQ_TRAIN_VAL;
	writel(val, priv->plu_base + MLXBF_GIGE_LANE_RX_EQ_TRAIN);

	ret = mlxbf_gige_plu_rx_power_ctrl(priv, true);
	if (ret)
		return ret;

	ret = readl_poll_timeout_atomic(priv->plu_base +
			MLXBF_GIGE_LANE_RX_FSM_CTRL, val,
			((val & MLXBF_GIGE_LANE_RX_FSM_PS_MASK) == MLXBF_GIGE_RX_FSM_ACTIVE),
			5, 1000000);
	if (ret)
		dev_dbg(priv->dev, "Polling timeout on rx fsm active state\n");

	return ret;
}

static bool mlxbf_gige_is_uphy_ready(struct mlxbf_gige *priv)
{
	u32 val;

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_TX_FSM_CTRL);
	if ((val & MLXBF_GIGE_LANE_TX_FSM_PS_MASK) != MLXBF_GIGE_TX_DATA_EN)
		return false;

	val = readl(priv->plu_base + MLXBF_GIGE_LANE_RX_FSM_CTRL);
	if ((val & MLXBF_GIGE_LANE_RX_FSM_PS_MASK) != MLXBF_GIGE_RX_FSM_ACTIVE)
		return false;

	return true;
}

int mlxbf_gige_config_uphy(struct mlxbf_gige *priv)
{
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	int ret = 0;

	priv->fuse_gw_io = devm_platform_ioremap_resource(pdev, MLXBF_GIGE_RES_FUSE_GW);
	if (IS_ERR(priv->fuse_gw_io))
		return PTR_ERR(priv->fuse_gw_io);

	if (mlxbf_gige_is_uphy_ready(priv))
		return 0;

	mlxbf_gige_ugl_static_config(priv);
	ret = mlxbf_gige_init_pll(priv);
	if (ret) {
		dev_err(dev, "%s: Failed to initialize PLL\n", __func__);
		return ret;
	}

	ret = mlxbf_gige_lock_pll(priv);
	if (ret) {
		dev_err(dev, "%s: Failed to lock PLL\n", __func__);
		return ret;
	}

	/* Due to hardware design issue, we need to get the lanes out of reset
	 * before configuring the imem.
	 */
	mlxbf_gige_get_lane_out_of_rst(priv);
	ret = mlxbf_gige_load_imem(priv);
	if (ret) {
		dev_err(dev, "%s: Failed to load imem\n", __func__);
		return ret;
	}

	ret = mlxbf_gige_tx_lane_open(priv);
	if (ret) {
		dev_err(dev, "%s: Failed to open tx lane\n", __func__);
		return ret;
	}

	ret = mlxbf_gige_rx_lane_open(priv);
	if (ret)
		dev_err(dev, "%s: Failed to open rx lane\n", __func__);

	return ret;
}
