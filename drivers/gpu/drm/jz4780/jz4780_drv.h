/*
 * Copyright (C) 2014 Imagination Technologies
 * Author: Zubair Lutfullah Kakakhel <Zubair.Kakakhel@imgtec.com>
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

#ifndef __jz4780_DRV_H__
#define __jz4780_DRV_H__

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/list.h>
#include <linux/reservation.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>

/* Defaulting to maximum capability of JZ4780 */
#define JZ4780_DEFAULT_MAX_PIXELCLOCK	200000
#define JZ4780_DEFAULT_MAX_WIDTH	2048
#define JZ4780_DEFAULT_MAX_BANDWIDTH	(1920*1080*60)


struct jz4780_drm_private {
	void __iomem *mmio;

	struct clk *disp_clk;    /* display dpll */
	struct clk *clk;         /* functional clock */
	int rev;                /* IP revision */

	/* don't attempt resolutions w/ higher W * H * Hz: */
	uint32_t max_bandwidth;
	/*
	 * Pixel Clock will be restricted to some value as
	 * defined in the device datasheet measured in KHz
	 */
	uint32_t max_pixelclock;
	/*
	 * Max allowable width is limited on a per device basis
	 * measured in pixels
	 */
	uint32_t max_width;

	struct workqueue_struct *wq;

	struct drm_fbdev_cma *fbdev;

	struct drm_crtc *crtc;

	unsigned int num_encoders;
	struct drm_encoder *encoders[8];

	unsigned int num_connectors;
	struct drm_connector *connectors[8];
};

struct drm_crtc *jz4780_crtc_create(struct drm_device *dev);
void jz4780_crtc_cancel_page_flip(struct drm_crtc *crtc,
				  struct drm_file *file);
irqreturn_t jz4780_crtc_irq(struct drm_crtc *crtc);
void jz4780_crtc_update_clk(struct drm_crtc *crtc);
int jz4780_crtc_mode_valid(struct drm_crtc *crtc,
			   struct drm_display_mode *mode);
int jz4780_crtc_max_width(struct drm_crtc *crtc);
struct reservation_object *jz4780_drv_lookup_resobj(struct drm_gem_object *obj);

#endif /* __jz4780_DRV_H__ */
