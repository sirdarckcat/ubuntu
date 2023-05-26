// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Clockwork Tech LLC
 * Copyright (c) 2021-2022 Max Fierke <max@maxfierke.com>
 *
 * Based on Pinfan Zhu's work on panel-cwd686.c for ClockworkPi's 5.10 BSP
 */

#include <drm/drm_modes.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <video/mipi_display.h>

struct cwd686 {
	struct device *dev;
	struct drm_panel panel;
	struct regulator *iovcc;
	struct regulator *vci;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;
	struct backlight_device *backlight;
	enum drm_panel_orientation orientation;
	bool prepared;
	bool enabled;
};

static const struct drm_display_mode default_mode = {
	.clock		= 54465,
	.hdisplay	= 480,
	.hsync_start	= 480 + 64,
	.hsync_end	= 480 + 64 + 40,
	.htotal		= 480 + 64 + 40 + 110,
	.vdisplay	= 1280,
	.vsync_start	= 1280 + 16,
	.vsync_end	= 1280 + 16 + 10,
	.vtotal		= 1280 + 16 + 10 + 2,
};

static inline struct cwd686 *panel_to_cwd686(struct drm_panel *panel)
{
	return container_of(panel, struct cwd686, panel);
}

#define dcs_write_seq(seq...)                              \
({                                                              \
	static const u8 d[] = { seq };                          \
	ssize_t r = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	 \
	if (r < 0) \
		return r; \
})

#define ICNL9707_CMD_CGOUTL 0xB3
#define ICNL9707_CMD_CGOUTR 0xB4
#define ICNL9707_P_CGOUT_VGL 0x00
#define ICNL9707_P_CGOUT_VGH 0x01
#define ICNL9707_P_CGOUT_HZ 0x02
#define ICNL9707_P_CGOUT_GND 0x03
#define ICNL9707_P_CGOUT_GSP1 0x04
#define ICNL9707_P_CGOUT_GSP2 0x05
#define ICNL9707_P_CGOUT_GSP3 0x06
#define ICNL9707_P_CGOUT_GSP4 0x07
#define ICNL9707_P_CGOUT_GSP5 0x08
#define ICNL9707_P_CGOUT_GSP6 0x09
#define ICNL9707_P_CGOUT_GSP7 0x0A
#define ICNL9707_P_CGOUT_GSP8 0x0B
#define ICNL9707_P_CGOUT_GCK1 0x0C
#define ICNL9707_P_CGOUT_GCK2 0x0D
#define ICNL9707_P_CGOUT_GCK3 0x0E
#define ICNL9707_P_CGOUT_GCK4 0x0F
#define ICNL9707_P_CGOUT_GCK5 0x10
#define ICNL9707_P_CGOUT_GCK6 0x11
#define ICNL9707_P_CGOUT_GCK7 0x12
#define ICNL9707_P_CGOUT_GCK8 0x13
#define ICNL9707_P_CGOUT_GCK9 0x14
#define ICNL9707_P_CGOUT_GCK10 0x15
#define ICNL9707_P_CGOUT_GCK11 0x16
#define ICNL9707_P_CGOUT_GCK12 0x17
#define ICNL9707_P_CGOUT_GCK13 0x18
#define ICNL9707_P_CGOUT_GCK14 0x19
#define ICNL9707_P_CGOUT_GCK15 0x1A
#define ICNL9707_P_CGOUT_GCK16 0x1B
#define ICNL9707_P_CGOUT_DIR 0x1C
#define ICNL9707_P_CGOUT_DIRB 0x1D
#define ICNL9707_P_CGOUT_ECLK_AC 0x1E
#define ICNL9707_P_CGOUT_ECLK_ACB 0x1F
#define ICNL9707_P_CGOUT_ECLK_AC2 0x20
#define ICNL9707_P_CGOUT_ECLK_AC2B 0x21
#define ICNL9707_P_CGOUT_GCH 0x22
#define ICNL9707_P_CGOUT_GCL 0x23
#define ICNL9707_P_CGOUT_XDON 0x24
#define ICNL9707_P_CGOUT_XDONB 0x25

#define ICNL9707_MADCTL_ML  0x10
#define ICNL9707_MADCTL_RGB 0x00
#define ICNL9707_MADCTL_BGR 0x08
#define ICNL9707_MADCTL_MH  0x04

#define ICNL9707_CMD_PWRCON_VCOM 0xB6
#define ICNL9707_P_PWRCON_VCOM_0495V 0x0D

#define ICNL9707_CMD_PWRCON_SEQ 0xB7
#define ICNL9707_CMD_PWRCON_CLK 0xB8
#define ICNL9707_CMD_PWRCON_BTA 0xB9
#define ICNL9707_CMD_PWRCON_MODE 0xBA
#define ICNL9707_CMD_PWRCON_REG 0xBD

#define ICNL9707_CMD_TCON 0xC1
#define ICNL9707_CMD_TCON2 0xC2
#define ICNL9707_CMD_TCON3 0xC3
#define ICNL9707_CMD_SRC_TIM 0xC6
#define ICNL9707_CMD_SRCCON 0xC7
#define ICNL9707_CMD_SET_GAMMA 0xC8

#define ICNL9707_CMD_ETC 0xD0

#define ICNL9707_CMD_PASSWORD1 0xF0
#define ICNL9707_P_PASSWORD1_DEFAULT 0xA5
#define ICNL9707_P_PASSWORD1_ENABLE_LVL2 0x5A

#define ICNL9707_CMD_PASSWORD2 0xF1
#define ICNL9707_P_PASSWORD2_DEFAULT 0x5A
#define ICNL9707_P_PASSWORD2_ENABLE_LVL2 0xA5

static int cwd686_init_sequence(struct cwd686 *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	dcs_write_seq(0xF0,0x5A,0x59);
	dcs_write_seq(0xF1,0xA5,0xA6);
	dcs_write_seq(0xB0,0x54,0x32,0x23,0x45,0x44,0x44,0x44,0x44,0x9F,0x00,0x01,0x9F,0x00,0x01);
	dcs_write_seq(0xB1,0x32,0x84,0x02,0x83,0x29,0x06,0x06,0x72,0x06,0x06);
	dcs_write_seq(0xB2,0x73);
	dcs_write_seq(0xB3,0x0B,0x09,0x13,0x11,0x0F,0x0D,0x00,0x00,0x00,0x03,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x05,0x07);
	dcs_write_seq(0xB4,0x0A,0x08,0x12,0x10,0x0E,0x0C,0x00,0x00,0x00,0x03,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x04,0x06);
	dcs_write_seq(0xB6,0x13,0x13);
	dcs_write_seq(0xB8,0xB4,0x43,0x02,0xCC);
	dcs_write_seq(0xB9,0xA5,0x20,0xFF,0xC8);
	dcs_write_seq(0xBA,0x88,0x23);
	dcs_write_seq(0xBD,0x43,0x0E,0x0E,0x50,0x50,0x29,0x10,0x03,0x44,0x03);
	dcs_write_seq(0xC1,0x00,0x0C,0x16,0x04,0x00,0x30,0x10,0x04);
	dcs_write_seq(0xC2,0x21,0x81);
	dcs_write_seq(0xC3,0x02,0x30);
	dcs_write_seq(0xC7,0x25,0x6A);
	dcs_write_seq(0xC8,0x7C,0x68,0x59,0x4E,0x4B,0x3C,0x41,0x2B,0x44,0x43,0x43,0x60,0x4E,0x55,0x47,0x44,0x38,0x27,0x06,0x7C,0x68,0x59,0x4E,0x4B,0x3C,0x41,0x2B,0x44,0x43,0x43,0x60,0x4E,0x55,0x47,0x44,0x38,0x27,0x06);
	dcs_write_seq(0xD4,0x00,0x00,0x00,0x32,0x04,0x51);
	dcs_write_seq(0xF1,0x5A,0x59);
	dcs_write_seq(0xF0,0xA5,0xA6);
	dcs_write_seq(0x36,0x14);
	dcs_write_seq(0x35,0x00);

	return 0;
}

static int cwd686_disable(struct drm_panel *panel)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);

	if (!ctx->enabled)
		return 0;

	backlight_disable(ctx->backlight);

	ctx->enabled = false;

	return 0;
}

static int cwd686_unprepare(struct drm_panel *panel)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int err;

	if (!ctx->prepared)
		return 0;

	err = mipi_dsi_dcs_set_display_off(dsi);
	if (err) {
		dev_err(ctx->dev, "failed to turn display off (%d)\n", err);
		return err;
	}

	err = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (err) {
		dev_err(ctx->dev, "failed to enter sleep mode (%d)\n", err);
		return err;
	}

	msleep(120);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	regulator_disable(ctx->vci);
	regulator_disable(ctx->iovcc);

	ctx->prepared = false;

	return 0;
}

static int cwd686_prepare(struct drm_panel *panel)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int err;

	if (ctx->prepared)
		return 0;

	err = regulator_enable(ctx->iovcc);
	if (err) {
		dev_err(ctx->dev, "failed to enable iovcc (%d)\n", err);
		return err;
	}
	msleep(20);

	err = regulator_enable(ctx->vci);
	if (err) {
		dev_err(ctx->dev, "failed to enable vci (%d)\n", err);
		return err;
	}
	msleep(120);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	/* T2 */
	msleep(10);

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	/* T3 */
	msleep(120);

	/* Exit sleep mode and power on */

	err = cwd686_init_sequence(ctx);
	if (err) {
		dev_err(ctx->dev, "failed to initialize display (%d)\n", err);
		return err;
	}

	err = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (err) {
		dev_err(ctx->dev, "failed to exit sleep mode (%d)\n", err);
		return err;
	}
	/* T6 */
	msleep(120);

	err = mipi_dsi_dcs_set_display_on(dsi);
	if (err) {
		dev_err(ctx->dev, "failed to turn display on (%d)\n", err);
		return err;
	}
	msleep(20);

	ctx->prepared = true;

	return 0;
}

static int cwd686_enable(struct drm_panel *panel)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);

	if (ctx->enabled)
		return 0;

	backlight_enable(ctx->backlight);

	ctx->enabled = true;

	return 0;
}

static int cwd686_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct cwd686 *ctx = panel_to_cwd686(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "bad mode or failed to add mode\n");
		return -EINVAL;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	/* set up connector's "panel orientation" property */
	drm_connector_set_panel_orientation(connector, ctx->orientation);

	drm_mode_probed_add(connector, mode);

	return 1; /* Number of modes */
}

static const struct drm_panel_funcs cwd686_drm_funcs = {
	.disable = cwd686_disable,
	.unprepare = cwd686_unprepare,
	.prepare = cwd686_prepare,
	.enable = cwd686_enable,
	.get_modes = cwd686_get_modes,
};

static int cwd686_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct cwd686 *ctx;
	int err;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			  MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		err = PTR_ERR(ctx->reset_gpio);
		if (err != -EPROBE_DEFER)
			dev_err(dev, "failed to request GPIO (%d)\n", err);
		return err;
	}

	ctx->iovcc = devm_regulator_get(dev, "iovcc");
	if (IS_ERR(ctx->iovcc))
		return PTR_ERR(ctx->iovcc);

	ctx->vci = devm_regulator_get(dev, "vci");
	if (IS_ERR(ctx->vci))
		return PTR_ERR(ctx->vci);

	ctx->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(ctx->backlight))
		return PTR_ERR(ctx->backlight);

	err = of_drm_get_panel_orientation(dev->of_node, &ctx->orientation);
	if (err) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, err);
		return err;
	}

	drm_panel_init(&ctx->panel, dev, &cwd686_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	err = mipi_dsi_attach(dsi);
	if (err < 0) {
		dev_err(dev, "mipi_dsi_attach() failed: %d\n", err);
		drm_panel_remove(&ctx->panel);
		return err;
	}

	return 0;
}

static int cwd686_remove(struct mipi_dsi_device *dsi)
{
	struct cwd686 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	if (ctx->prepared)
		cwd686_unprepare(&ctx->panel);

	return 0;
}

static const struct of_device_id cwd686_of_match[] = {
	{ .compatible = "clockwork,cwd686" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, cwd686_of_match);

static struct mipi_dsi_driver cwd686_driver = {
	.probe = cwd686_probe,
	.remove = cwd686_remove,
	.driver = {
		.name = "panel-clockwork-cwd686",
		.of_match_table = cwd686_of_match,
	},
};
module_mipi_dsi_driver(cwd686_driver);

MODULE_AUTHOR("Pinfan Zhu <zhu@clockworkpi.com>");
MODULE_AUTHOR("Max Fierke <max@maxfierke.com>");
MODULE_DESCRIPTION("ClockworkPi CWD686 panel driver");
MODULE_LICENSE("GPL");
