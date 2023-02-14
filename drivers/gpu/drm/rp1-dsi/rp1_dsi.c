// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * DRM Driver for DSI output on Raspberry Pi RP1
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/printk.h>
#include <linux/console.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/cred.h>
#include <drm/drm_drv.h>
#include <drm/drm_mm.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_vblank.h>
#include <drm/drm_of.h>

#include "rp1_dsi.h"

static void rp1dsi_pipe_update(struct drm_simple_display_pipe *pipe,
			      struct drm_plane_state *old_state)
{
	struct drm_pending_vblank_event *event;
	unsigned long flags;
	struct drm_framebuffer *fb = pipe->plane.state->fb;
	struct rp1dsi_priv *priv = pipe->crtc.dev->dev_private;
	struct drm_gem_object *gem = fb ? drm_gem_fb_get_obj(fb, 0) : NULL;
	struct drm_gem_dma_object *dma_obj = gem ? to_drm_gem_dma_obj(gem) : NULL;
	bool can_update = fb && dma_obj && priv && priv->pipe_enabled;

	/* (Re-)start DSI,DMA where required; and update FB address */
	if (can_update) {
		if (!priv->dma_running || fb->format->format != priv->cur_fmt) {
			if (priv->dma_running && fb->format->format != priv->cur_fmt) {
				rp1dsi_dma_stop(priv);
				priv->dma_running = false;
			}
			if (!priv->dsi_running) {
				rp1dsi_dsi_setup(priv, &pipe->crtc.state->mode);
				priv->dsi_running = true;
			}
			if (!priv->dma_running) {
				rp1dsi_dma_setup(priv,
						fb->format->format, priv->display_format,
						&pipe->crtc.state->mode);
				priv->dma_running = true;
			}
			priv->cur_fmt  = fb->format->format;
			drm_crtc_vblank_on(&pipe->crtc);
		}
		rp1dsi_dsi_set_cmdmode(priv, 0);
		rp1dsi_dma_update(priv, dma_obj->dma_addr, fb->offsets[0], fb->pitches[0]);
	}

	/* Arm VBLANK event (or call it immediately in some error cases) */
	spin_lock_irqsave(&pipe->crtc.dev->event_lock, flags);
	event = pipe->crtc.state->event;
	if (event) {
		pipe->crtc.state->event = NULL;
		if (can_update && drm_crtc_vblank_get(&pipe->crtc) == 0)
			drm_crtc_arm_vblank_event(&pipe->crtc, event);
		else
			drm_crtc_send_vblank_event(&pipe->crtc, event);
	}
	spin_unlock_irqrestore(&pipe->crtc.dev->event_lock, flags);
}

static void rp1dsi_pipe_enable(struct drm_simple_display_pipe *pipe,
			      struct drm_crtc_state *crtc_state,
			      struct drm_plane_state *plane_state)
{
	struct rp1dsi_priv *priv = pipe->crtc.dev->dev_private;

	dev_info(&priv->pdev->dev, __func__);
	priv->pipe_enabled = true;
	priv->cur_fmt = 0xdeadbeef;
	rp1dsi_pipe_update(pipe, 0);
}

static void rp1dsi_pipe_disable(struct drm_simple_display_pipe *pipe)
{
	struct rp1dsi_priv *priv = pipe->crtc.dev->dev_private;

	dev_info(&priv->pdev->dev, __func__);
	drm_crtc_vblank_off(&pipe->crtc);
	if (priv->dma_running) {
		rp1dsi_dma_stop(priv);
		priv->dma_running = false;
		rp1dsi_dsi_set_cmdmode(priv, 1); /* video stopped, so drop to command mode */
	}
	priv->pipe_enabled = false;
}

static int rp1dsi_pipe_enable_vblank(struct drm_simple_display_pipe *pipe)
{
	struct rp1dsi_priv *priv = pipe->crtc.dev->dev_private;

	if (priv)
		rp1dsi_dma_vblank_ctrl(priv, 1);

	return 0;
}

static void rp1dsi_pipe_disable_vblank(struct drm_simple_display_pipe *pipe)
{
	struct rp1dsi_priv *priv = pipe->crtc.dev->dev_private;

	if (priv)
		rp1dsi_dma_vblank_ctrl(priv, 0);
}

static const struct drm_simple_display_pipe_funcs rp1dsi_pipe_funcs = {
	.enable	    = rp1dsi_pipe_enable,
	.update	    = rp1dsi_pipe_update,
	.disable    = rp1dsi_pipe_disable,
	.enable_vblank  = rp1dsi_pipe_enable_vblank,
	.disable_vblank = rp1dsi_pipe_disable_vblank,
};

static const struct drm_mode_config_funcs rp1dsi_mode_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static const u32 rp1dsi_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565
};

static void rp1dsi_stopall(struct drm_device *drm)
{
	if (drm->dev_private) {
		struct rp1dsi_priv *priv = drm->dev_private;

		if (priv->dma_running || rp1dsi_dma_busy(priv)) {
			rp1dsi_dma_stop(priv);
			priv->dma_running = false;
		}
		if (priv->dsi_running) {
			rp1dsi_dsi_stop(priv);
			priv->dsi_running = false;
		}
		if (!priv->running_on_fpga && priv->clocks[RP1DSI_CLOCK_CFG])
			clk_disable_unprepare(priv->clocks[RP1DSI_CLOCK_CFG]);
	}
}

DEFINE_DRM_GEM_DMA_FOPS(rp1dsi_fops);

static struct drm_driver rp1dsi_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops			= &rp1dsi_fops,
	.name			= "drm-rp1-dsi",
	.desc			= "drm-rp1-dsi",
	.date			= "0",
	.major			= 1,
	.minor			= 0,
	DRM_GEM_DMA_DRIVER_OPS,
	.release		= rp1dsi_stopall,
};

static int rp1dsi_bind(struct rp1dsi_priv *priv)
{
	struct platform_device *pdev = priv->pdev;
	struct drm_device *drm = priv->drm;
	struct drm_bridge *bridge = NULL;
	struct drm_panel *panel;
	int ret;

	ret = drm_of_find_panel_or_bridge(pdev->dev.of_node,
					  0, 0,
					  &panel, &bridge);
	if (ret) {
		dev_info(&pdev->dev, "%s: bridge not found\n", __func__);
		return -EPROBE_DEFER;
	}
	if (panel) {
		bridge = devm_drm_panel_bridge_add(drm->dev, panel);
		if (IS_ERR(bridge)) {
			ret = PTR_ERR(bridge);
			goto rtn;
		}
	}

	ret = drmm_mode_config_init(drm);
	if (ret)
		goto rtn;

	drm->mode_config.max_width  = 1920;
	drm->mode_config.max_height = 1280;
	drm->mode_config.preferred_depth = 32;
	drm->mode_config.prefer_shadow	 = 0;
	drm->mode_config.quirk_addfb_prefer_host_byte_order = true;
	drm->mode_config.funcs = &rp1dsi_mode_funcs;
	drm_vblank_init(drm, 1);

	ret = drm_simple_display_pipe_init(drm,
					   &priv->pipe,
					   &rp1dsi_pipe_funcs,
					   rp1dsi_formats,
					   ARRAY_SIZE(rp1dsi_formats),
					   NULL, NULL);
	ret = drm_simple_display_pipe_attach_bridge(&priv->pipe, bridge);
	if (ret)
		goto rtn;

	drm_mode_config_reset(drm);

	if (!priv->running_on_fpga && priv->clocks[RP1DSI_CLOCK_CFG])
		clk_prepare_enable(priv->clocks[RP1DSI_CLOCK_CFG]);

	ret = drm_dev_register(drm, 0);

	if (ret == 0)
		drm_fbdev_generic_setup(drm, 32); /* the "32" is preferred BPP */

rtn:
	if (ret)
		dev_err(&pdev->dev, "%s returned %d\n", __func__, ret);
	else
		dev_info(&pdev->dev, "%s succeeded", __func__);

	return ret;
}

static void rp1dsi_unbind(struct rp1dsi_priv *priv)
{
	struct drm_device *drm = priv->drm;

	rp1dsi_stopall(drm);
	drm_dev_unregister(drm);
	drm_atomic_helper_shutdown(drm);
}

int rp1dsi_host_attach(struct mipi_dsi_host *host, struct mipi_dsi_device *dsi)
{
	struct rp1dsi_priv *priv = container_of(host, struct rp1dsi_priv, dsi_host);

	dev_info(&priv->pdev->dev, __func__);
	dev_info(&priv->pdev->dev, "%s: Attach DSI device name=%s channel=%d "
		"lanes=%d format=%d flags=0x%lx hs_rate=%lu lp_rate=%lu",
		__func__, dsi->name, dsi->channel, dsi->lanes, dsi->format,
		dsi->mode_flags, dsi->hs_rate, dsi->lp_rate);
	priv->vc              = dsi->channel & 3;
	priv->lanes           = dsi->lanes;
	priv->display_format  = dsi->format;
	priv->display_flags   = dsi->mode_flags;
	priv->display_hs_rate = dsi->hs_rate;
	priv->display_lp_rate = dsi->lp_rate;

	/*
	 * Previously, we added a separate component to handle panel/bridge
	 * discovery and DRM registration, but now it's just a function call.
	 * The downstream/attaching device should deal with -EPROBE_DEFER
	 */
	return rp1dsi_bind(priv);
}

int rp1dsi_host_detach(struct mipi_dsi_host *host, struct mipi_dsi_device *dsi)
{
	struct rp1dsi_priv *priv = container_of(host, struct rp1dsi_priv, dsi_host);

	dev_info(&priv->pdev->dev, "%s", __func__);

	/*
	 * Unregister the DRM driver.
	 * TODO: Check we are cleaning up correctly and not doing things multiple times!
	 */
	rp1dsi_unbind(priv);
	return 0;
}

ssize_t rp1dsi_host_transfer(struct mipi_dsi_host *host, const struct mipi_dsi_msg *msg)
{
	struct rp1dsi_priv *priv = container_of(host, struct rp1dsi_priv, dsi_host);
	struct mipi_dsi_packet packet;
	int ret = 0;

	/* Write */
	ret = mipi_dsi_create_packet(&packet, msg);
	if (ret) {
		dev_err(priv->drm->dev, "RP1DSI: failed to create packet: %d\n", ret);
		return ret;
	}
	rp1dsi_dsi_send(priv, *(u32 *)(&packet.header), packet.payload_length, packet.payload);

	/* Optional read back */
	if (msg->rx_len && msg->rx_buf)
		ret = rp1dsi_dsi_recv(priv, msg->rx_len, msg->rx_buf);

	return (ssize_t)ret;
}

static const struct mipi_dsi_host_ops rp1dsi_mipi_dsi_host_ops = {
	.attach = rp1dsi_host_attach,
	.detach = rp1dsi_host_detach,
	.transfer = rp1dsi_host_transfer
};

static int rp1dsi_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drm_device *drm;
	struct rp1dsi_priv *priv;
	int i, ret;

	dev_info(dev, __func__);
	drm = drm_dev_alloc(&rp1dsi_driver, dev);
	if (IS_ERR(drm)) {
		ret = PTR_ERR(drm);
		return ret;
	}
	priv = drmm_kzalloc(drm, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		ret = -ENOMEM;
		goto err_free_drm;
	}
	sema_init(&priv->finished, 0);
	priv->drm = drm;
	priv->pdev = pdev;
	drm->dev_private = priv;
	platform_set_drvdata(pdev, drm);
	ret = rp1dsi_check_platform(priv);
	if (ret)
		goto err_free_drm;

	/* Safe default values for DSI mode */
	priv->lanes = 1;
	priv->display_format = MIPI_DSI_FMT_RGB888;
	priv->display_flags  = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM;

	/* Hardware resources */
	if (!priv->running_on_fpga) {
		for (i = 0; i < RP1DSI_NUM_CLOCKS; i++) {
			static const char * const myclocknames[RP1DSI_NUM_CLOCKS] = {
				"cfgclk", "dpiclk", "byteclk", "refclk"
			};
			priv->clocks[i] = devm_clk_get(dev, myclocknames[i]);
			if (IS_ERR(priv->clocks[i])) {
				ret = PTR_ERR(priv->clocks[i]);
				dev_err(dev, "Error getting clocks[%d]\n", i);
				goto err_free_drm;
			}
		}
	}

	for (i = 0; i < RP1DSI_NUM_HW_BLOCKS; i++) {
		priv->hw_base[i] =
			devm_ioremap_resource(dev,
					      platform_get_resource(priv->pdev, IORESOURCE_MEM, i));
		if (IS_ERR(priv->hw_base[i])) {
			ret = PTR_ERR(priv->hw_base[i]);
			dev_err(dev, "Error memory mapping regs[%d]\n", i);
			goto err_free_drm;
		}
	}
	ret = platform_get_irq(priv->pdev, 0);
	if (ret > 0)
		ret = devm_request_irq(dev, ret, rp1dsi_dma_isr,
				       IRQF_SHARED, "rp1-dsi", priv);
	if (ret) {
		dev_err(dev, "Unable to request interrupt\n");
		ret = -EINVAL;
		goto err_free_drm;
	}
	rp1dsi_mipicfg_setup(priv);
	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));

	/* Create the MIPI DSI Host and wait for the panel/bridge to attach to it */
	priv->dsi_host.ops = &rp1dsi_mipi_dsi_host_ops;
	priv->dsi_host.dev = dev;
	dev_info(dev, "%s: Calling mipi_dsi_host_register", __func__);
	ret = mipi_dsi_host_register(&priv->dsi_host);
	if (ret)
		goto err_free_drm;

	return ret;

err_free_drm:
	dev_err(dev, "%s fail %d\n", __func__, ret);
	drm_dev_put(drm);
	return ret;
}

static int rp1dsi_platform_remove(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);
	struct rp1dsi_priv *priv = drm->dev_private;

	mipi_dsi_host_unregister(&priv->dsi_host);
	return 0;
}

static void rp1dsi_platform_shutdown(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	rp1dsi_stopall(drm);
}

static const struct of_device_id rp1dsi_of_match[] = {
	{
		.compatible = "raspberrypi,rp1dsi",
	},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, rp1dsi_of_match);

static struct platform_driver rp1dsi_platform_driver = {
	.probe		= rp1dsi_platform_probe,
	.remove		= rp1dsi_platform_remove,
	.shutdown       = rp1dsi_platform_shutdown,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = rp1dsi_of_match,
	},
};

module_platform_driver(rp1dsi_platform_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MIPI DSI driver for Raspberry Pi RP1");
MODULE_AUTHOR("Nick Hollinghurst");
