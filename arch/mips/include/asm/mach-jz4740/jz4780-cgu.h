/*
 * JZ4780 CGU driver
 *
 * Copyright (c) 2013 Imagination Technologies
 * Author: Paul Burton <paul.burton@imgtec.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __MIPS_ASM_MACH_JZ4780_JZ4780_CGU_H__
#define __MIPS_ASM_MACH_JZ4780_JZ4780_CGU_H__

/* CGU register offsets */
#define CGU_REG_CLOCKCONTROL	0x00
#define CGU_REG_PLLCONTROL	0x0c
#define CGU_REG_APLL		0x10
#define CGU_REG_MPLL		0x14
#define CGU_REG_EPLL		0x18
#define CGU_REG_VPLL		0x1c
#define CGU_REG_OPCR		0x24
#define CGU_REG_DDRCDR		0x2c
#define CGU_REG_VPUCDR		0x30
#define CGU_REG_USBPCR		0x3c
#define CGU_REG_USBRDT		0x40
#define CGU_REG_USBVBFIL	0x44
#define CGU_REG_USBPCR1		0x48
#define CGU_REG_LP0CDR		0x54
#define CGU_REG_I2SCDR		0x60
#define CGU_REG_LP1CDR		0x64
#define CGU_REG_MSC0CDR		0x68
#define CGU_REG_UHCCDR		0x6c
#define CGU_REG_SSICDR		0x74
#define CGU_REG_CIMCDR		0x7c
#define CGU_REG_PCMCDR		0x84
#define CGU_REG_GPUCDR		0x88
#define CGU_REG_HDMICDR		0x8c
#define CGU_REG_MSC1CDR		0xa4
#define CGU_REG_MSC2CDR		0xa8
#define CGU_REG_BCHCDR		0xac
#define CGU_REG_SRBC		0xc4
#define CGU_REG_CLOCKSTATUS	0xd4

/* This isn't documented in the Ingenic manual but was provided in their
 * kernel as a hack. */
#define EHCI_REG_UTMI_BUS	0xb34900b0

/* bits within a PLL control register */
#define PLLCTL_M_SHIFT		19
#define PLLCTL_M_MASK		(0x1fff << PLLCTL_M_SHIFT)
#define PLLCTL_N_SHIFT		13
#define PLLCTL_N_MASK		(0x3f << PLLCTL_N_SHIFT)
#define PLLCTL_OD_SHIFT		9
#define PLLCTL_OD_MASK		(0xf << PLLCTL_OD_SHIFT)
#define PLLCTL_ON		(1 << 4)
#define PLLCTL_BYPASS		(1 << 1)
#define PLLCTL_ENABLE		(1 << 0)

/* bits within the OPCR register */
#define OPCR_SPENDN0		(1 << 7)
#define OPCR_SPENDN1		(1 << 6)

/* bits within the USBPCR register */
#define USBPCR_USB_MODE		BIT(31)
#define USBPCR_IDPULLUP_MASK	(0x3 << 28)
#define USBPCR_COMMONONN	BIT(25)
#define USBPCR_VBUSVLDEXT	BIT(24)
#define USBPCR_VBUSVLDEXTSEL	BIT(23)
#define USBPCR_POR		BIT(22)
#define USBPCR_OTG_DISABLE	BIT(20)
#define USBPCR_COMPDISTUNE_MASK	(0x7 << 17)
#define USBPCR_OTGTUNE_MASK	(0x7 << 14)
#define USBPCR_SQRXTUNE_MASK	(0x7 << 11)
#define USBPCR_TXFSLSTUNE_MASK	(0xf << 7)
#define USBPCR_TXPREEMPHTUNE	BIT(6)
#define USBPCR_TXHSXVTUNE_MASK	(0x3 << 4)
#define USBPCR_TXVREFTUNE_MASK	0xf

/* bits within the USBPCR1 register */
#define USBPCR1_REFCLKSEL_SHIFT	26
#define USBPCR1_REFCLKSEL_MASK	(0x3 << USBPCR1_REFCLKSEL_SHIFT)
#define USBPCR1_REFCLKSEL_CORE	(0x2 << USBPCR1_REFCLKSEL_SHIFT)
#define USBPCR1_REFCLKDIV_SHIFT	24
#define USBPCR1_REFCLKDIV_MASK	(0x3 << USBPCR1_REFCLKDIV_SHIFT)
#define USBPCR1_REFCLKDIV_19_2	(0x3 << USBPCR1_REFCLKDIV_SHIFT)
#define USBPCR1_REFCLKDIV_48	(0x2 << USBPCR1_REFCLKDIV_SHIFT)
#define USBPCR1_REFCLKDIV_24	(0x1 << USBPCR1_REFCLKDIV_SHIFT)
#define USBPCR1_REFCLKDIV_12	(0x0 << USBPCR1_REFCLKDIV_SHIFT)
#define USBPCR1_USB_SEL		BIT(28)
#define USBPCR1_WORD_IF0	BIT(19)
#define USBPCR1_WORD_IF1	BIT(18)
#define USBPCR1_DPPD1		BIT(22)
#define USBPCR1_DMPD1		BIT(23)

/* bits within the USBRDT register */
#define USBRDT_VBFIL_LD_EN	BIT(25)
#define USBRDT_USBRDT_MASK	0x7fffff

/* bits within the USBVBFIL register */
#define USBVBFIL_IDDIGFIL_SHIFT	16
#define USBVBFIL_IDDIGFIL_MASK	(0xffff << USBVBFIL_IDDIGFIL_SHIFT)
#define USBVBFIL_USBVBFIL_MASK	(0xffff)

/* Bits within the UTMI Bus register */
#define UTMIBUS_WIDTH		BIT(6)
/* bits within the SRBC register */
#define SRBC_UHC_SR		BIT(14)

/* bits within the SRBC register */
#define SRBC_UHC_SR		BIT(14)

enum jz4780_usb_port {
	USB_PORT_OTG	= 0,
	USB_PORT_HOST	= 1,
};

enum jz4780_usb_otg_mode {
	USB_OTG_MODE_MENTOR	= 0,
	USB_OTG_MODE_SYNOPSYS	= 1,
};

enum jz4780_usb_utmi_bus_width {
	USB_PORT_UTMI_BUS_WIDTH_8	= 0,
	USB_PORT_UTMI_BUS_WIDTH_16	= 1,
};

/**
 * jz4780_cgu_set_usb_suspend - (un)suspend a USB port
 * @port: the USB port whose state should be changed
 * @suspend: non-zero if the port should be suspended, else zero
 *
 * Returns zero on success, else -ERRNO.
 */
#ifndef CONFIG_MACH_JZ4780
static inline int jz4780_cgu_set_usb_suspend(enum jz4780_usb_port port,
					     bool suspend)
{
	BUG();
}
#else
extern int jz4780_cgu_set_usb_suspend(enum jz4780_usb_port port,
				      bool suspend);
#endif

/**
 * jz4780_cgu_set_usb_otg_mode - Set OTG Mode
 * @mode: the USB mode to be configured.
 *
 * Returns zero on success, else -ERRNO.
 */
extern int jz4780_cgu_set_usb_otg_mode(enum jz4780_usb_otg_mode mode);

/**
 * jz4780_cgu_set_usb_port_utmi_bus_width - Set the USB port bus width
 * @port: the USB port whose width should be changed
 * @width: the bus width to set the USB port to
 *
 * Returns zero on success, else -ERRNO.
 */
extern int jz4780_cgu_set_usb_utmi_bus_width(enum jz4780_usb_port port,
					enum jz4780_usb_utmi_bus_width width);

/**
 * jz4780_cgu_set_usb_usbvbfil - Set the USB VBUS jitter filter time
 * @value: the value to set the VBUS jitter filter time to
 */
extern void jz4780_cgu_set_usb_usbvbfil(u32 value);

/**
 * jz4780_cgu_set_usb_iddigfil - Set the USB iddig jitter filter time
 * @value: the value to set the iddig jitter filter time to
 */
extern void jz4780_cgu_set_usb_iddigfil(u32 value);

/**
 * jz4780_cgu_set_usb_usbrdt - Set the USB reset detect time
 * @value: the value to set the reset detect time to
 */
extern void jz4780_cgu_set_usb_usbrdt(u32 value);

/**
 * jz4780_cgu_set_usb_vbfil_ld_en - Enable the VBUS filter data load enable
 * @value: the value to set the reset detect time to
 */
extern void jz4780_cgu_set_usb_vbfil_ld_en(bool enable);

/**
 * jz4780_cgu_usb_reset - Resets the USB module.
 */
extern void jz4780_cgu_usb_reset(void);

/**
 * jz4780_cgu_set_usbpcr_param - Sets the parameters of USBPCR reg
 * @param: the parameter to change
 * @value: the value to set it to
 * Returns zero on success, else -ERRNO.
 */
extern int jz4780_cgu_set_usbpcr_param(u32 param, bool enable);

/**
 * jz4780_cgu_start_ehci - Setup SoC registers so that EHCI mode can work.
 * Returns zero on success, else -ERRNO.
 */
extern int jz4780_cgu_start_ehci(void);

/**
 * jz4780_cgu_stop_ehci - Force the USB port into suspend mode, thus disabling
 * EHCI.
 */
extern void jz4780_cgu_stop_ehci(void);

extern int jz4780_cgu_start_ehci(void);

#endif /* __MIPS_ASM_MACH_JZ4780_JZ4780_CGU_H__ */
