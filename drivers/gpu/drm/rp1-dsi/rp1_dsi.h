/* SPDX-License-Identifier: GPL-2.0 */
/*
 * DRM Driver for DSI output on Raspberry Pi RP1
 */

#include <linux/types.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <drm/drm_device.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_mipi_dsi.h>

#define MODULE_NAME "drm-rp1-dsi"
#define DRIVER_NAME "drm-rp1-dsi"

/* ---------------------------------------------------------------------- */

#define RP1DSI_HW_BLOCK_DMA   0
#define RP1DSI_HW_BLOCK_DSI   1
#define RP1DSI_HW_BLOCK_CFG   2
#define RP1DSI_NUM_HW_BLOCKS  3

#define RP1DSI_CLOCK_CFG     0
#define RP1DSI_CLOCK_DPI     1
#define RP1DSI_CLOCK_BYTE    2
#define RP1DSI_CLOCK_REF     3
#define RP1DSI_NUM_CLOCKS    4

/* ---------------------------------------------------------------------- */

struct rp1dsi_priv {

	/* DRM and platform device pointers */
	struct drm_device *drm;
	struct platform_device *pdev;

	/* Framework and helper objects */
	struct drm_simple_display_pipe pipe;
	struct mipi_dsi_host dsi_host;

	/* Clocks. We need DPI clock; the others are frequency references */
	struct clk *clocks[RP1DSI_NUM_CLOCKS];

	/* Block (DSI DMA, DSI Host) base addresses, and current state */
	void __iomem *hw_base[RP1DSI_NUM_HW_BLOCKS];
	u32 cur_fmt;
	bool running_on_fpga;
	bool dsi_running, dma_running, pipe_enabled;
	struct semaphore finished;

	/* Attached display parameters (from mipi_dsi_device) */
	unsigned long display_flags, display_hs_rate, display_lp_rate;
	enum mipi_dsi_pixel_format display_format;
	u8 vc;
	u8 lanes;

	/* DPHY */
	u8 hsfreqrange;
};

/* ---------------------------------------------------------------------- */
/* Functions to control the DSI/DPI/DMA block				  */

void rp1dsi_dma_setup(struct rp1dsi_priv *priv,
		u32 in_format, enum mipi_dsi_pixel_format out_format,
		struct drm_display_mode const *mode);
void rp1dsi_dma_update(struct rp1dsi_priv *priv, dma_addr_t addr, u32 offset, u32 stride);
void rp1dsi_dma_stop(struct rp1dsi_priv *priv);
int rp1dsi_dma_busy(struct rp1dsi_priv *priv);
irqreturn_t rp1dsi_dma_isr(int irq, void *dev);
void rp1dsi_dma_vblank_ctrl(struct rp1dsi_priv *priv, int enable);

/* ---------------------------------------------------------------------- */
/* Functions to control the MIPICFG block and check RP1 platform		  */

int  rp1dsi_check_platform(struct rp1dsi_priv *priv);
void rp1dsi_mipicfg_setup(struct rp1dsi_priv *priv);

/* ---------------------------------------------------------------------- */
/* Functions to control the SNPS D-PHY and DSI block setup		  */

void rp1dsi_dsi_setup(struct rp1dsi_priv *priv, struct drm_display_mode const *mode);
void rp1dsi_dsi_send(struct rp1dsi_priv *priv, u32 header, int len, const u8 *buf);
int  rp1dsi_dsi_recv(struct rp1dsi_priv *priv, int len, u8 *buf);
void rp1dsi_dsi_set_cmdmode(struct rp1dsi_priv *priv, int cmd_mode);
void rp1dsi_dsi_stop(struct rp1dsi_priv *priv);
