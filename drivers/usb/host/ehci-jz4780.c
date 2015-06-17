/*
 * EHCI-compliant USB host controller driver for Ingenic jz4780 SoC
 *
 * Copyright (c) 2013 Imagination Technologies
 * Author: Paul Burton <paul.burton@imgtec.com>
 *
 * Based on ehci-tegra.c:
 *   Copyright (C) 2010 Google, Inc.
 *   Copyright (C) 2009 - 2013 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/usb/ehci_def.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>
#include <asm/mach-jz4740/jz4780-cgu.h>

#include "ehci.h"

static struct hc_driver __read_mostly jz4780_ehci_hc_driver;

struct jz4780_ehci_hcd {
	struct clk *clk;
};

static int jz4780_ehci_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	struct jz4780_ehci_hcd *jz_ehci;
	int irq, gpio_vbus, err = 0;
	u32 temp;

	/* Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we have dma capability bindings this can go away.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	gpio_vbus = of_get_named_gpio(pdev->dev.of_node,
				      "ingenic,vbus-gpio", 0);
	if (!gpio_is_valid(gpio_vbus))
		return 0;

	err = gpio_request(gpio_vbus, "vbus_gpio");
	if (err) {
		dev_err(&pdev->dev, "failed to request vbus gpio %d\n",
			gpio_vbus);
		goto cleanup_vbus_req;
	}

	err = gpio_direction_output(gpio_vbus, 1);
	if (err) {
		dev_err(&pdev->dev, "failed to enable vbus\n");
		goto cleanup_vbus_req;
	}

	hcd = usb_create_hcd(&jz4780_ehci_hc_driver, &pdev->dev,
					dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "failed to create HCD\n");
		err = -ENOMEM;
		goto cleanup_vbus_req;
	}
	platform_set_drvdata(pdev, hcd);
	ehci = hcd_to_ehci(hcd);
	jz_ehci = (struct jz4780_ehci_hcd *)ehci->priv;

	jz_ehci->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(jz_ehci->clk)) {
		dev_err(&pdev->dev, "failed to get ehci clock\n");
		err = PTR_ERR(jz_ehci->clk);
		goto cleanup_hcd_create;
	}

	err = clk_set_rate(jz_ehci->clk, 48000000);
	if (err)
		goto cleanup_clk_get;

	err = clk_prepare_enable(jz_ehci->clk);
	if (err)
		goto cleanup_clk_get;

	err = jz4780_cgu_set_usb_suspend(USB_PORT_HOST, false);
	if (err) {
		dev_err(&pdev->dev, "failed to unsuspend port\n");
		goto cleanup_clk_en;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get I/O memory\n");
		err = -ENXIO;
		goto cleanup_clk_en;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!hcd->regs) {
		dev_err(&pdev->dev, "failed to remap I/O memory\n");
		err = -ENOMEM;
		goto cleanup_clk_en;
	}
	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs +
		HC_LENGTH(ehci, readl(&ehci->caps->hc_capbase));
	ehci->hcs_params = readl(&ehci->caps->hcs_params);

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		err = -ENODEV;
		goto cleanup_clk_en;
	}

	jz4780_cgu_start_ehci();

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err) {
		dev_err(&pdev->dev, "failed to add USB HCD\n");
		goto cleanup_clk_en;
	}

	/* Set utmi data bus width of controller to 16bit */
	temp = readl((volatile int *)EHCI_REG_UTMI_BUS);
	writel(temp | UTMIBUS_WIDTH, (volatile int *)EHCI_REG_UTMI_BUS);

	return err;

cleanup_clk_en:
	clk_disable_unprepare(jz_ehci->clk);
cleanup_clk_get:
	clk_put(jz_ehci->clk);
cleanup_hcd_create:
	usb_put_hcd(hcd);
cleanup_vbus_req:
	gpio_free(gpio_vbus);
	return err;
}

static int jz4780_ehci_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct jz4780_ehci_hcd *jz_ehci =
		(struct jz4780_ehci_hcd *)hcd_to_ehci(hcd)->priv;

	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	jz4780_cgu_stop_ehci();

	clk_disable_unprepare(jz_ehci->clk);

	return 0;
}

static void jz4780_ehci_hcd_shutdown(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

static struct of_device_id jz4780_ehci_of_match[] = {
	{ .compatible = "ingenic,jz4780-ehci", },
	{ },
};

static struct platform_driver jz4780_ehci_driver = {
	.probe		= jz4780_ehci_probe,
	.remove		= jz4780_ehci_remove,
	.shutdown	= jz4780_ehci_hcd_shutdown,
	.driver		= {
		.name	= "jz4780-ehci",
		.of_match_table = jz4780_ehci_of_match,
	}
};

static const struct ehci_driver_overrides jz4780_overrides __initconst = {
	.extra_priv_size = sizeof(struct jz4780_ehci_hcd),
};

static int __init ehci_jz4780_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	ehci_init_driver(&jz4780_ehci_hc_driver, &jz4780_overrides);

	return platform_driver_register(&jz4780_ehci_driver);
}
module_init(ehci_jz4780_init);

static void __exit ehci_jz4780_cleanup(void)
{
	platform_driver_unregister(&jz4780_ehci_driver);
}
module_exit(ehci_jz4780_cleanup);

MODULE_DESCRIPTION("Ingenic jz4780 EHCI driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:jz4780-ehci");
MODULE_DEVICE_TABLE(of, jz4780_ehci_of_match);
