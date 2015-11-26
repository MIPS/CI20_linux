/*
 * Copyright (C) 2014 Imagination Technologies
 * Author: Zubair Lutfullah Kakakhel <Zubair.Kakakhel@imgtec.com>
 *
 * LCDC CRTC driver for Ingenic JZ4780, based on the tilcdc driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "drm_flip_work.h"

#include "jz4780_drv.h"
#include "jz4780_regs.h"

/**
 * @next: physical address of next frame descriptor
 * @databuf: physical address of buffer
 * @id: frame ID
 * @cmd: DMA command and buffer length(in word)
 * @offsize: DMA off size, in word
 * @page_width: DMA page width, in word
 * @cpos: smart LCD mode is commands' number, other is bpp,
 * premulti and position of foreground 0, 1
 * @desc_size: alpha and size of foreground 0, 1
 */
struct jz4780_framedesc {
	uint32_t next;
	uint32_t databuf;
	uint32_t id;
	uint32_t cmd;
	uint32_t offsize;
	uint32_t page_width;
	uint32_t cpos;
	uint32_t desc_size;
} __packed;

struct jz4780_crtc {
	struct drm_crtc base;

	const struct jz4780_panel_info *info;
	uint32_t dirty;

	struct drm_pending_vblank_event *event;
	int dpms;
	wait_queue_head_t frame_done_wq;
	bool frame_done;

	/* fb currently set to scanout 0/1: */
	struct drm_framebuffer *scanout[2];

	/* for deferred fb unref's: */
	struct drm_flip_work unref_work;

	/* DMA descriptors */
	struct jz4780_framedesc *framedesc;
	dma_addr_t framedesc_phys;
};
#define to_jz4780_crtc(x) container_of(x, struct jz4780_crtc, base)

static void unref_worker(struct drm_flip_work *work, void *val)
{
	struct jz4780_crtc *jz4780_crtc =
		container_of(work, struct jz4780_crtc, unref_work);
	struct drm_device *dev = jz4780_crtc->base.dev;

	mutex_lock(&dev->mode_config.mutex);
	drm_framebuffer_unreference(val);
	mutex_unlock(&dev->mode_config.mutex);
}

static void set_scanout(struct drm_crtc *crtc, int n)
{
	struct jz4780_crtc *jz4780_crtc = to_jz4780_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct jz4780_drm_private *priv = dev->dev_private;
	struct drm_framebuffer *fb = crtc->primary->fb;
	struct jz4780_framedesc *framedesc = jz4780_crtc->framedesc;
	struct drm_gem_cma_object *gem;
	unsigned int depth, bpp;
	int fg0_line_size;
	int fg0_frm_size;
	int height_width;
	static const uint32_t stat[] = {
		LCDC_STATE_IFU0, LCDC_STATE_IFU1,
	 };

	gem = drm_fb_cma_get_gem_obj(fb, 0);
	drm_fb_get_bpp_depth(fb->pixel_format, &depth, &bpp);
	pm_runtime_get_sync(dev->dev);

	/* lcd display area */
	fg0_line_size = crtc->mode.hdisplay * bpp >> 3;
	/* word aligned and in word */
	fg0_line_size = ALIGN(fg0_line_size, 4) >> 2;
	fg0_frm_size = fg0_line_size * crtc->mode.vdisplay;

	height_width = (crtc->mode.vdisplay - 1) << LCDC_DESSIZE_HEIGHT_BIT
		& LCDC_DESSIZE_HEIGHT_MASK;
	height_width |= ((crtc->mode.hdisplay - 1) << LCDC_DESSIZE_WIDTH_BIT
			     & LCDC_DESSIZE_WIDTH_MASK);

	if (n == 0) {
		framedesc[0].next = jz4780_crtc->framedesc_phys
				    + sizeof(struct jz4780_framedesc);
		framedesc[0].databuf = gem->paddr;
		framedesc[0].id = 0xda0;
		framedesc[0].cmd = LCDC_CMD_EOFINT | LCDC_CMD_FRM_EN
				   | fg0_frm_size;
		framedesc[0].offsize = 0;
		framedesc[0].page_width = 0;
		framedesc[0].cpos = 0x2d000000;
		framedesc[0].desc_size = 0xff << LCDC_DESSIZE_ALPHA_BIT;
		framedesc[0].desc_size |= height_width;

		jz4780_write(dev, LCDC_DA0, framedesc[0].next);
	} else {
		framedesc[1].next = jz4780_crtc->framedesc_phys;
		framedesc[1].id = 0xda1;
		framedesc[1].databuf = gem->paddr;
		framedesc[1].offsize = 0;
		framedesc[1].page_width = 0;
		framedesc[1].cmd = (LCDC_CMD_EOFINT | LCDC_CMD_FRM_EN)
				   | fg0_frm_size;

		framedesc[1].desc_size = 0xff << LCDC_DESSIZE_ALPHA_BIT;
		framedesc[1].desc_size |= height_width;

		framedesc[1].cpos = 0x2f000000;
		jz4780_write(dev, LCDC_DA1, framedesc[1].next);
	}

	if (jz4780_crtc->scanout[n]) {
		drm_flip_work_queue(&jz4780_crtc->unref_work,
				    jz4780_crtc->scanout[n]);
		drm_flip_work_commit(&jz4780_crtc->unref_work, priv->wq);
	}
	jz4780_crtc->scanout[n] = crtc->primary->fb;
	drm_framebuffer_reference(jz4780_crtc->scanout[n]);
	jz4780_crtc->dirty &= ~stat[n];
	pm_runtime_put_sync(dev->dev);
}

static void update_scanout(struct drm_crtc *crtc, bool async)
{
	struct jz4780_crtc *jz4780_crtc = to_jz4780_crtc(crtc);
	struct drm_device *dev = crtc->dev;

	if (!async && (jz4780_crtc->dpms == DRM_MODE_DPMS_ON)) {
		jz4780_crtc->dirty |= LCDC_STATE_IFU0 | LCDC_STATE_IFU1;
		drm_vblank_get(dev, 0);
	} else {
		/* not enabled yet, so update registers immediately: */
		jz4780_write(dev, LCDC_STATE, 0);
		set_scanout(crtc, 0);
		set_scanout(crtc, 1);
	}
}

static void start(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	uint32_t ctrl;

	jz4780_write(dev, LCDC_STATE, 0);
	jz4780_write(dev, LCDC_OSDS, 0);
	ctrl = jz4780_read(dev, LCDC_CTRL);
	ctrl |= LCDC_CTRL_ENA;
	ctrl &= ~LCDC_CTRL_DIS;
	jz4780_write(dev, LCDC_CTRL, ctrl);

}

static void stop(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	int count = 5;
	uint32_t ctrl;

	ctrl = jz4780_read(dev, LCDC_CTRL);
	ctrl |= LCDC_CTRL_DIS;
	jz4780_write(dev, LCDC_CTRL, ctrl);
	while (!(jz4780_read(dev, LCDC_STATE) & LCDC_STATE_LDD)
	       && count--) {
		usleep_range(1000, 2000);
	}
	if (count >= 0) {
		ctrl = jz4780_read(dev, LCDC_STATE);
		ctrl &= ~LCDC_STATE_LDD;
		jz4780_write(dev, LCDC_STATE, ctrl);
	} else {
		DRM_DEBUG_DRIVER("LCDC normal disable state wrong");
	}

}

static void jz4780_crtc_destroy(struct drm_crtc *crtc)
{
	struct jz4780_crtc *jz4780_crtc = to_jz4780_crtc(crtc);

	WARN_ON(jz4780_crtc->dpms == DRM_MODE_DPMS_ON);

	drm_crtc_cleanup(crtc);
	drm_flip_work_cleanup(&jz4780_crtc->unref_work);

	kfree(jz4780_crtc);
}

static int jz4780_crtc_page_flip(struct drm_crtc *crtc,
		struct drm_framebuffer *fb,
		struct drm_pending_vblank_event *event,
		uint32_t page_flip_flags)
{
	struct jz4780_crtc *jz4780_crtc = to_jz4780_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	bool async_flip = page_flip_flags & DRM_MODE_PAGE_FLIP_ASYNC;
	unsigned long flags;

	if (jz4780_crtc->event) {
		dev_err(dev->dev, "already pending page flip!\n");
		return -EBUSY;
	}

	crtc->primary->fb = fb;

	if (!async_flip)
		jz4780_crtc->event = event;

	update_scanout(crtc, async_flip);

	if (event && async_flip) {
		spin_lock_irqsave(&dev->event_lock, flags);
		drm_send_vblank_event(dev, 0, event);
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	return 0;
}

static void jz4780_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct jz4780_crtc *jz4780_crtc = to_jz4780_crtc(crtc);
	struct drm_device *dev = crtc->dev;

	/* we really only care about on or off: */
	if (mode != DRM_MODE_DPMS_ON)
		mode = DRM_MODE_DPMS_OFF;

	if (jz4780_crtc->dpms == mode)
		return;

	jz4780_crtc->dpms = mode;

	pm_runtime_get_sync(dev->dev);

	if (mode == DRM_MODE_DPMS_ON) {
		pm_runtime_forbid(dev->dev);
		start(crtc);
	} else {
		jz4780_crtc->frame_done = false;
		stop(crtc);
		pm_runtime_allow(dev->dev);
	}

	pm_runtime_put_sync(dev->dev);
}

static bool jz4780_crtc_mode_fixup(struct drm_crtc *crtc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	DRM_DEBUG_DRIVER("Mode Fixup not supported by driver yet\n");
	return true;
}

static void jz4780_crtc_prepare(struct drm_crtc *crtc)
{

	jz4780_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
}

static void jz4780_crtc_commit(struct drm_crtc *crtc)
{

	jz4780_crtc_dpms(crtc, DRM_MODE_DPMS_ON);
}

static int jz4780_crtc_mode_set(struct drm_crtc *crtc,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode,
		int x, int y,
		struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	uint32_t hbp, hfp, hsw, vbp, vfp, vsw;
	int ret;
	uint32_t pcfg;

	uint16_t hds, vds;
	uint16_t hde, vde;
	uint16_t ht, vt;
	uint32_t cfg, ctrl;
	unsigned int rgb_ctrl;

	ret = jz4780_crtc_mode_valid(crtc, mode);
	if (WARN_ON(ret))
		return ret;

	pm_runtime_get_sync(dev->dev);

	/* Configure timings: */
	hbp = mode->htotal - mode->hsync_end;
	hfp = mode->hsync_start - mode->hdisplay;
	hsw = mode->hsync_end - mode->hsync_start;
	vbp = mode->vtotal - mode->vsync_end;
	vfp = mode->vsync_start - mode->vdisplay;
	vsw = mode->vsync_end - mode->vsync_start;

	hds = hsw + hbp;
	hde = hds + mode->hdisplay;
	ht = hde + hfp;

	vds = vsw + vbp;
	vde = vds + mode->vdisplay;
	vt = vde + vfp;

	cfg = LCDC_CFG_NEWDES | LCDC_CFG_RECOVER | LCDC_CFG_MODE_TFT_24BIT;
	cfg |= LCDC_CFG_PSM;
	cfg |= LCDC_CFG_CLSM;
	cfg |= LCDC_CFG_SPLM;
	cfg |= LCDC_CFG_REVM;
	cfg |= LCDC_CFG_PCP;

	ctrl = LCDC_CTRL_BST_64 | LCDC_CTRL_OFUM;

	/* magic number */
	pcfg = 0xC0000000 | (511<<18) | (400<<9) | (256<<0);

	jz4780_write(dev, LCDC_VAT, (ht << 16) | vt);
	jz4780_write(dev, LCDC_DAH, (hds << 16) | hde);
	jz4780_write(dev, LCDC_DAV, (vds << 16) | vde);

	jz4780_write(dev, LCDC_HSYNC, hsw);
	jz4780_write(dev, LCDC_VSYNC, vsw);

	jz4780_write(dev, LCDC_CFG, cfg);
	ctrl |= jz4780_read(dev, LCDC_CTRL);
	jz4780_write(dev, LCDC_CTRL, ctrl);
	jz4780_write(dev, LCDC_PCFG, pcfg);

	rgb_ctrl = LCDC_RGBC_RGBFMT | LCDC_RGBC_ODD_RGB |
			LCDC_RGBC_EVEN_RGB;

	jz4780_write(dev, LCDC_RGBC, rgb_ctrl);

	update_scanout(crtc, false);
	jz4780_crtc_update_clk(crtc);

	pm_runtime_put_sync(dev->dev);

	return 0;
}

static const struct drm_crtc_funcs jz4780_crtc_funcs = {
		.destroy        = jz4780_crtc_destroy,
		.set_config     = drm_crtc_helper_set_config,
		.page_flip      = jz4780_crtc_page_flip,
};

static const struct drm_crtc_helper_funcs jz4780_crtc_helper_funcs = {
		.dpms           = jz4780_crtc_dpms,
		.mode_fixup     = jz4780_crtc_mode_fixup,
		.prepare        = jz4780_crtc_prepare,
		.commit         = jz4780_crtc_commit,
		.mode_set       = jz4780_crtc_mode_set,
};

int jz4780_crtc_max_width(struct drm_crtc *crtc)
{
	return 2048;
}

int jz4780_crtc_mode_valid(struct drm_crtc *crtc, struct drm_display_mode *mode)
{
	struct jz4780_drm_private *priv = crtc->dev->dev_private;
	unsigned int bandwidth;
	uint32_t hbp, hfp, hsw, vbp, vfp, vsw;

	/*
	 * check to see if the width is within the range that
	 * the LCD Controller physically supports
	 */
	if (mode->hdisplay > 2048)
		return MODE_VIRTUAL_X;

	/* width must be multiple of 16 */
	if (mode->hdisplay & 0xf)
		return MODE_VIRTUAL_X;

	if (mode->vdisplay > 2048)
		return MODE_VIRTUAL_Y;

	DRM_DEBUG_DRIVER("Processing mode %dx%d@%d with pixel clock %d",
		mode->hdisplay, mode->vdisplay,
		drm_mode_vrefresh(mode), mode->clock);

	hbp = mode->htotal - mode->hsync_end;
	hfp = mode->hsync_start - mode->hdisplay;
	hsw = mode->hsync_end - mode->hsync_start;
	vbp = mode->vtotal - mode->vsync_end;
	vfp = mode->vsync_start - mode->vdisplay;
	vsw = mode->vsync_end - mode->vsync_start;

	if ((hbp-1) & ~0x3ff) {
		DRM_DEBUG_DRIVER("Prune: Horizontal Back Porch out of range");
		return MODE_HBLANK_WIDE;
	}

	if ((hfp-1) & ~0x3ff) {
		DRM_DEBUG_DRIVER("Prune: Horizontal Front Porch out of range");
		return MODE_HBLANK_WIDE;
	}

	if ((hsw-1) & ~0x3ff) {
		DRM_DEBUG_DRIVER("Prune: Horizontal Sync Width out of range");
		return MODE_HSYNC_WIDE;
	}

	if (vbp & ~0xff) {
		DRM_DEBUG_DRIVER("Prune: Vertical Back Porch out of range");
		return MODE_VBLANK_WIDE;
	}

	if (vfp & ~0xff) {
		DRM_DEBUG_DRIVER("Prune: Vertical Front Porch out of range");
		return MODE_VBLANK_WIDE;
	}

	if ((vsw-1) & ~0x3f) {
		DRM_DEBUG_DRIVER("Prune: Vertical Sync Width out of range");
		return MODE_VSYNC_WIDE;
	}

	/*
	 * some devices have a maximum allowed pixel clock
	 * configured from the DT
	 */
	if (mode->clock > priv->max_pixelclock) {
		DRM_DEBUG_DRIVER("Prune: pixel clock too high");
		return MODE_CLOCK_HIGH;
	}

	/*
	 * some devices further limit the max horizontal resolution
	 * configured from the DT
	 */
	if (mode->hdisplay > priv->max_width) {
		DRM_DEBUG_DRIVER("Prune: Bad width");
		return MODE_BAD_WIDTH;
	}

	/* filter out modes that would require too much memory bandwidth: */
	bandwidth = mode->hdisplay * mode->vdisplay *
		drm_mode_vrefresh(mode);
	if (bandwidth > priv->max_bandwidth) {
		DRM_DEBUG_DRIVER("Prune: exceeds defined bandwidth limit %d",
				 bandwidth);
		return MODE_BAD;
	}

	return MODE_OK;
}

void jz4780_crtc_update_clk(struct drm_crtc *crtc)
{
	struct jz4780_crtc *jz4780_crtc = to_jz4780_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct jz4780_drm_private *priv = dev->dev_private;
	int dpms = jz4780_crtc->dpms;
	unsigned int lcd_clk;
	int ret;

	pm_runtime_get_sync(dev->dev);

	if (dpms == DRM_MODE_DPMS_ON)
		jz4780_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);

	/* in raster mode, minimum divisor is 2: */
	ret = clk_set_rate(priv->disp_clk, crtc->mode.clock * 1000);
	if (ret) {
		dev_err(dev->dev, "failed to set display clock rate to: %d\n",
				crtc->mode.clock);
		goto out;
	}

	lcd_clk = clk_get_rate(priv->clk);

	DRM_DEBUG_DRIVER("lcd_clk=%u, mode clock=%d", lcd_clk,
			  crtc->mode.clock);
	DRM_DEBUG_DRIVER("fck=%lu, dpll_disp_ck=%lu", clk_get_rate(priv->clk),
			 clk_get_rate(priv->disp_clk));

	if (dpms == DRM_MODE_DPMS_ON)
		jz4780_crtc_dpms(crtc, DRM_MODE_DPMS_ON);

out:
	pm_runtime_put_sync(dev->dev);
}

irqreturn_t jz4780_crtc_irq(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_pending_vblank_event *event;
	struct jz4780_crtc *jz4780_crtc = to_jz4780_crtc(crtc);
	unsigned int state;
	unsigned int tmp;
	unsigned long flags;
	uint32_t dirty;

	state = jz4780_read(dev, LCDC_STATE);
	dirty = jz4780_crtc->dirty & state;

	if (dirty) {
		if (dirty & LCDC_STATE_IFU0) {
			jz4780_write(dev, LCDC_STATE, state & ~LCDC_STATE_IFU0);
			set_scanout(crtc, 0);
		}
		if (dirty & LCDC_STATE_IFU1) {
			jz4780_write(dev, LCDC_STATE, state & ~LCDC_STATE_IFU1);
			set_scanout(crtc, 1);
		}

		drm_handle_vblank(dev, 0);

		spin_lock_irqsave(&dev->event_lock, flags);
		event = jz4780_crtc->event;
		jz4780_crtc->event = NULL;
		if (event)
			drm_send_vblank_event(dev, 0, event);
		spin_unlock_irqrestore(&dev->event_lock, flags);

		if (dirty && !jz4780_crtc->dirty)
			drm_vblank_put(dev, 0);
	}

	if (state & LCDC_STATE_EOF) {
		jz4780_write(dev, LCDC_STATE, state & ~LCDC_STATE_EOF);
	}

	if (state & LCDC_STATE_OFU) {
		DRM_DEBUG_DRIVER("Out FiFo underrun\n");
		jz4780_write(dev, LCDC_STATE, state & ~LCDC_STATE_OFU);
		tmp = jz4780_read(dev, LCDC_CTRL);
		jz4780_write(dev, LCDC_CTRL, tmp & ~LCDC_CTRL_OFUM);
		update_scanout(crtc, false);
		start(crtc);
	}

	return IRQ_HANDLED;
}

void jz4780_crtc_cancel_page_flip(struct drm_crtc *crtc, struct drm_file *file)
{
	struct jz4780_crtc *jz4780_crtc = to_jz4780_crtc(crtc);
	struct drm_pending_vblank_event *event;
	struct drm_device *dev = crtc->dev;
	unsigned long flags;

	/* Destroy the pending vertical blanking event associated with the
	 * pending page flip, if any, and disable vertical blanking interrupts.
	 */
	spin_lock_irqsave(&dev->event_lock, flags);
	event = jz4780_crtc->event;
	if (event && event->base.file_priv == file) {
		jz4780_crtc->event = NULL;
		event->base.destroy(&event->base);
		drm_vblank_put(dev, 0);
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

struct drm_crtc *jz4780_crtc_create(struct drm_device *dev)
{
	struct jz4780_crtc *jz4780_crtc;
	struct drm_crtc *crtc;
	int ret;

	jz4780_crtc = kzalloc(sizeof(*jz4780_crtc), GFP_KERNEL);
	if (!jz4780_crtc) {
		dev_err(dev->dev, "allocation failed\n");
		return NULL;
	}

	jz4780_crtc->framedesc = dma_alloc_coherent(dev->dev,
				 sizeof(struct jz4780_framedesc) * 2,
				 &jz4780_crtc->framedesc_phys,
				 GFP_KERNEL | GFP_ATOMIC);
	if (!jz4780_crtc->framedesc) {
		dev_err(dev->dev, "desc allocation failed\n");
		return NULL;
	}

	crtc = &jz4780_crtc->base;

	jz4780_crtc->dpms = DRM_MODE_DPMS_OFF;
	init_waitqueue_head(&jz4780_crtc->frame_done_wq);

	ret = drm_flip_work_init(&jz4780_crtc->unref_work, 16,
			"unref", unref_worker);
	if (ret) {
		dev_err(dev->dev, "could not allocate unref FIFO\n");
		goto fail;
	}

	ret = drm_crtc_init(dev, crtc, &jz4780_crtc_funcs);
	if (ret < 0)
		goto fail;

	drm_crtc_helper_add(crtc, &jz4780_crtc_helper_funcs);

	return crtc;

fail:
	jz4780_crtc_destroy(crtc);
	return NULL;
}
