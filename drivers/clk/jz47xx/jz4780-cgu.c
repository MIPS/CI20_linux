/*
 * Ingenic jz4780 SoC CGU driver
 *
 * Copyright (c) 2013-2015 Imagination Technologies
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

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <dt-bindings/clock/jz4780-cgu.h>
#include "jz47xx-cgu.h"
#include <asm/mach-jz4740/jz4780-cgu.h>

static struct jz47xx_cgu *cgu;

static u8 jz4780_otg_phy_get_parent(struct clk_hw *hw)
{
	/* we only use CLKCORE, revisit if that ever changes */
	return 0;
}

static int jz4780_otg_phy_set_parent(struct clk_hw *hw, u8 idx)
{
	unsigned long flags;
	u32 usbpcr1;

	if (idx > 0)
		return -EINVAL;

	spin_lock_irqsave(&cgu->power_lock, flags);

	usbpcr1 = readl(cgu->base + CGU_REG_USBPCR1);
	usbpcr1 &= ~USBPCR1_REFCLKSEL_MASK;
	/* we only use CLKCORE */
	usbpcr1 |= USBPCR1_REFCLKSEL_CORE;
	writel(usbpcr1, cgu->base + CGU_REG_USBPCR1);

	spin_unlock_irqrestore(&cgu->power_lock, flags);
	return 0;
}

static unsigned long jz4780_otg_phy_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	u32 usbpcr1;
	unsigned refclk_div;

	usbpcr1 = readl(cgu->base + CGU_REG_USBPCR1);
	refclk_div = usbpcr1 & USBPCR1_REFCLKDIV_MASK;

	switch (refclk_div) {
	case USBPCR1_REFCLKDIV_12:
		return 12000000;

	case USBPCR1_REFCLKDIV_24:
		return 24000000;

	case USBPCR1_REFCLKDIV_48:
		return 48000000;

	case USBPCR1_REFCLKDIV_19_2:
		return 19200000;
	}

	BUG();
	return parent_rate;
}

static long jz4780_otg_phy_round_rate(struct clk_hw *hw, unsigned long req_rate,
				      unsigned long *parent_rate)
{
	if (req_rate < 15600000)
		return 12000000;

	if (req_rate < 21600000)
		return 19200000;

	if (req_rate < 36000000)
		return 24000000;

	return 48000000;
}

static int jz4780_otg_phy_set_rate(struct clk_hw *hw, unsigned long req_rate,
				   unsigned long parent_rate)
{
	unsigned long flags;
	u32 usbpcr1, div_bits;

	switch (req_rate) {
	case 12000000:
		div_bits = USBPCR1_REFCLKDIV_12;
		break;

	case 19200000:
		div_bits = USBPCR1_REFCLKDIV_19_2;
		break;

	case 24000000:
		div_bits = USBPCR1_REFCLKDIV_24;
		break;

	case 48000000:
		div_bits = USBPCR1_REFCLKDIV_48;
		break;

	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&cgu->power_lock, flags);

	usbpcr1 = readl(cgu->base + CGU_REG_USBPCR1);
	usbpcr1 &= ~USBPCR1_REFCLKDIV_MASK;
	usbpcr1 |= div_bits;
	writel(usbpcr1, cgu->base + CGU_REG_USBPCR1);

	spin_unlock_irqrestore(&cgu->power_lock, flags);
	return 0;
}

struct clk_ops jz4780_otg_phy_ops = {
	.get_parent = jz4780_otg_phy_get_parent,
	.set_parent = jz4780_otg_phy_set_parent,

	.recalc_rate = jz4780_otg_phy_recalc_rate,
	.round_rate = jz4780_otg_phy_round_rate,
	.set_rate = jz4780_otg_phy_set_rate,
};

static const s8 pll_od_encoding[16] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
	0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
};

static const struct jz47xx_cgu_clk_info jz4780_cgu_clocks[] = {

	/* External clocks */

	[JZ4780_CLK_EXCLK] = { "ext", CGU_CLK_EXT },
	[JZ4780_CLK_RTCLK] = { "rtc", CGU_CLK_EXT },

	/* PLLs */

#define DEF_PLL(name) { \
	.reg = CGU_REG_ ## name, \
	.m_shift = 19, \
	.m_bits = 13, \
	.m_offset = 1, \
	.n_shift = 13, \
	.n_bits = 6, \
	.n_offset = 1, \
	.od_shift = 9, \
	.od_bits = 4, \
	.od_max = 16, \
	.od_encoding = pll_od_encoding, \
	.stable_bit = 6, \
	.bypass_bit = 1, \
	.enable_bit = 0, \
}

	[JZ4780_CLK_APLL] = {
		"apll", CGU_CLK_PLL,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.pll = DEF_PLL(APLL),
	},

	[JZ4780_CLK_MPLL] = {
		"mpll", CGU_CLK_PLL,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.pll = DEF_PLL(MPLL),
	},

	[JZ4780_CLK_EPLL] = {
		"epll", CGU_CLK_PLL,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.pll = DEF_PLL(EPLL),
	},

	[JZ4780_CLK_VPLL] = {
		"vpll", CGU_CLK_PLL,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.pll = DEF_PLL(VPLL),
	},

#undef DEF_PLL

	/* Custom (SoC-specific) OTG PHY */

	[JZ4780_CLK_OTGPHY] = {
		"otg_phy", CGU_CLK_CUSTOM,
		.parents = { -1, -1, JZ4780_CLK_EXCLK, -1 },
		.custom = { &jz4780_otg_phy_ops },
	},

	/* Muxes & dividers */

	[JZ4780_CLK_SCLKA] = {
		"sclk_a", CGU_CLK_MUX,
		.parents = { -1, JZ4780_CLK_APLL, JZ4780_CLK_EXCLK,
			     JZ4780_CLK_RTCLK },
		.mux = { CGU_REG_CLOCKCONTROL, 30, 2 },
	},

	[JZ4780_CLK_CPUMUX] = {
		"cpumux", CGU_CLK_MUX,
		.parents = { -1, JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_EPLL },
		.mux = { CGU_REG_CLOCKCONTROL, 28, 2 },
	},

	[JZ4780_CLK_CPU] = {
		"cpu", CGU_CLK_DIV,
		.parents = { JZ4780_CLK_CPUMUX, -1 },
		.div = { CGU_REG_CLOCKCONTROL, 0, 4, 22, -1, -1 },
	},

	[JZ4780_CLK_L2CACHE] = {
		"l2cache", CGU_CLK_DIV,
		.parents = { JZ4780_CLK_CPUMUX, -1 },
		.div = { CGU_REG_CLOCKCONTROL, 4, 4, -1, -1, -1 },
	},

	[JZ4780_CLK_AHB0] = {
		"ahb0", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { -1, JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_EPLL },
		.mux = { CGU_REG_CLOCKCONTROL, 26, 2 },
		.div = { CGU_REG_CLOCKCONTROL, 8, 4, 21, -1, -1 },
	},

	[JZ4780_CLK_AHB2PMUX] = {
		"ahb2_apb_mux", CGU_CLK_MUX,
		.parents = { -1, JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_RTCLK },
		.mux = { CGU_REG_CLOCKCONTROL, 24, 2 },
	},

	[JZ4780_CLK_AHB2] = {
		"ahb2", CGU_CLK_DIV,
		.parents = { JZ4780_CLK_AHB2PMUX, -1 },
		.div = { CGU_REG_CLOCKCONTROL, 12, 4, 20, -1, -1 },
	},

	[JZ4780_CLK_PCLK] = {
		"pclk", CGU_CLK_DIV,
		.parents = { JZ4780_CLK_AHB2PMUX, -1 },
		.div = { CGU_REG_CLOCKCONTROL, 16, 4, 20, -1, -1 },
	},

	[JZ4780_CLK_DDR] = {
		"ddr", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { -1, JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL, -1 },
		.mux = { CGU_REG_DDRCDR, 30, 2 },
		.div = { CGU_REG_DDRCDR, 0, 4, 29, 28, 27 },
	},

	[JZ4780_CLK_VPU] = {
		"vpu", CGU_CLK_MUX | CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_EPLL, -1 },
		.mux = { CGU_REG_VPUCDR, 30, 2 },
		.div = { CGU_REG_VPUCDR, 0, 4, 29, 28, 27 },
		.gate_bit = 32 + 2,
	},

	[JZ4780_CLK_I2SPLL] = {
		"i2s_pll", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_EPLL, -1 },
		.mux = { CGU_REG_I2SCDR, 30, 1 },
		.div = { CGU_REG_I2SCDR, 0, 8, 29, 28, 27 },
	},

	[JZ4780_CLK_I2S] = {
		"i2s", CGU_CLK_MUX,
		.parents = { JZ4780_CLK_EXCLK, JZ4780_CLK_I2SPLL, -1 },
		.mux = { CGU_REG_I2SCDR, 31, 1 },
	},

	[JZ4780_CLK_LCD0PIXCLK] = {
		"lcd0pixclk", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_VPLL, -1 },
		.mux = { CGU_REG_LP0CDR, 30, 2 },
		.div = { CGU_REG_LP0CDR, 0, 8, 28, 27, 26 },
	},

	[JZ4780_CLK_LCD1PIXCLK] = {
		"lcd1pixclk", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_VPLL, -1 },
		.mux = { CGU_REG_LP1CDR, 30, 2 },
		.div = { CGU_REG_LP1CDR, 0, 8, 28, 27, 26 },
	},

	[JZ4780_CLK_MSCMUX] = {
		"msc_mux", CGU_CLK_MUX,
		.parents = { -1, JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL, -1 },
		.mux = { CGU_REG_MSC0CDR, 30, 2 },
	},

	[JZ4780_CLK_MSC0] = {
		"msc0", CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { JZ4780_CLK_MSCMUX, -1 },
		.div = { CGU_REG_MSC0CDR, 0, 8, 29, 28, 27 },
		.gate_bit = 3,
	},

	[JZ4780_CLK_MSC1] = {
		"msc1", CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { JZ4780_CLK_MSCMUX, -1 },
		.div = { CGU_REG_MSC1CDR, 0, 8, 29, 28, 27 },
		.gate_bit = 11,
	},

	[JZ4780_CLK_MSC2] = {
		"msc2", CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { JZ4780_CLK_MSCMUX, -1 },
		.div = { CGU_REG_MSC2CDR, 0, 8, 29, 28, 27 },
		.gate_bit = 12,
	},

	[JZ4780_CLK_UHC] = {
		"uhc", CGU_CLK_MUX | CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_EPLL, JZ4780_CLK_OTGPHY },
		.mux = { CGU_REG_UHCCDR, 30, 2 },
		.div = { CGU_REG_UHCCDR, 0, 8, 29, 28, 27 },
		.gate_bit = 24,
	},

	[JZ4780_CLK_SSIPLL] = {
		"ssi_pll", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL, -1 },
		.mux = { CGU_REG_SSICDR, 30, 1 },
		.div = { CGU_REG_SSICDR, 0, 8, 29, 28, 27 },
	},

	[JZ4780_CLK_SSI] = {
		"ssi", CGU_CLK_MUX,
		.parents = { JZ4780_CLK_EXCLK, JZ4780_CLK_SSIPLL, -1 },
		.mux = { CGU_REG_SSICDR, 31, 1 },
	},

	[JZ4780_CLK_CIMMCLK] = {
		"cim_mclk", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL, -1 },
		.mux = { CGU_REG_CIMCDR, 31, 1 },
		.div = { CGU_REG_CIMCDR, 0, 8, 30, 29, 28 },
	},

	[JZ4780_CLK_PCMPLL] = {
		"pcm_pll", CGU_CLK_MUX | CGU_CLK_DIV,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_EPLL, JZ4780_CLK_VPLL },
		.mux = { CGU_REG_PCMCDR, 29, 2 },
		.div = { CGU_REG_PCMCDR, 0, 8, 28, 27, 26 },
	},

	[JZ4780_CLK_PCM] = {
		"pcm", CGU_CLK_MUX | CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, JZ4780_CLK_PCMPLL, -1 },
		.mux = { CGU_REG_PCMCDR, 31, 1 },
		.gate_bit = 32 + 3,
	},

	[JZ4780_CLK_GPU] = {
		"gpu", CGU_CLK_MUX | CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { -1, JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_EPLL },
		.mux = { CGU_REG_GPUCDR, 30, 2 },
		.div = { CGU_REG_GPUCDR, 0, 4, 29, 28, 27 },
		.gate_bit = 32 + 4,
	},

	[JZ4780_CLK_HDMI] = {
		"hdmi", CGU_CLK_MUX | CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_VPLL, -1 },
		.mux = { CGU_REG_HDMICDR, 30, 2 },
		.div = { CGU_REG_HDMICDR, 0, 8, 29, 28, 26 },
		.gate_bit = 32 + 9,
	},

	[JZ4780_CLK_BCH] = {
		"bch", CGU_CLK_MUX | CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { -1, JZ4780_CLK_SCLKA, JZ4780_CLK_MPLL,
			     JZ4780_CLK_EPLL },
		.mux = { CGU_REG_BCHCDR, 30, 2 },
		.div = { CGU_REG_BCHCDR, 0, 4, 29, 28, 27 },
		.gate_bit = 1,
	},

	/* Gate-only clocks */

	[JZ4780_CLK_NEMC] = {
		"nemc", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_AHB2, -1 },
		.gate_bit = 0,
	},

	[JZ4780_CLK_OTG0] = {
		"otg0", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 2,
	},

	[JZ4780_CLK_SSI0] = {
		"ssi0", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_SSI, -1 },
		.gate_bit = 4,
	},

	[JZ4780_CLK_SMB0] = {
		"smb0", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_PCLK, -1 },
		.gate_bit = 5,
	},

	[JZ4780_CLK_SMB1] = {
		"smb1", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_PCLK, -1 },
		.gate_bit = 6,
	},

	[JZ4780_CLK_SCC] = {
		"scc", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 7,
	},

	[JZ4780_CLK_AIC] = {
		"aic", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 8,
	},

	[JZ4780_CLK_TSSI0] = {
		"tssi0", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 9,
	},

	[JZ4780_CLK_OWI] = {
		"owi", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 10,
	},

	[JZ4780_CLK_KBC] = {
		"kbc", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 13,
	},

	[JZ4780_CLK_SADC] = {
		"sadc", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 14,
	},

	[JZ4780_CLK_UART0] = {
		"uart0", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 15,
	},

	[JZ4780_CLK_UART1] = {
		"uart1", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 16,
	},

	[JZ4780_CLK_UART2] = {
		"uart2", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 17,
	},

	[JZ4780_CLK_UART3] = {
		"uart3", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 18,
	},

	[JZ4780_CLK_SSI1] = {
		"ssi1", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_SSI, -1 },
		.gate_bit = 19,
	},

	[JZ4780_CLK_SSI2] = {
		"ssi2", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_SSI, -1 },
		.gate_bit = 20,
	},

	[JZ4780_CLK_PDMA] = {
		"pdma", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 21,
	},

	[JZ4780_CLK_GPS] = {
		"gps", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 22,
	},

	[JZ4780_CLK_MAC] = {
		"mac", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 23,
	},

	[JZ4780_CLK_SMB2] = {
		"smb2", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_PCLK, -1 },
		.gate_bit = 25,
	},

	[JZ4780_CLK_CIM] = {
		"cim", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 26,
	},

	[JZ4780_CLK_LCD] = {
		"lcd", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 28,
	},

	[JZ4780_CLK_TVE] = {
		"tve", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_LCD, -1 },
		.gate_bit = 27,
	},

	[JZ4780_CLK_IPU] = {
		"ipu", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 29,
	},

	[JZ4780_CLK_DDR0] = {
		"ddr0", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_DDR, -1 },
		.gate_bit = 30,
	},

	[JZ4780_CLK_DDR1] = {
		"ddr1", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_DDR, -1 },
		.gate_bit = 31,
	},

	[JZ4780_CLK_SMB3] = {
		"smb3", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_PCLK, -1 },
		.gate_bit = 32 + 0,
	},

	[JZ4780_CLK_TSSI1] = {
		"tssi1", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 1,
	},

	[JZ4780_CLK_COMPRESS] = {
		"compress", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 5,
	},

	[JZ4780_CLK_AIC1] = {
		"aic1", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 6,
	},

	[JZ4780_CLK_GPVLC] = {
		"gpvlc", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 7,
	},

	[JZ4780_CLK_OTG1] = {
		"otg1", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 8,
	},

	[JZ4780_CLK_UART4] = {
		"uart4", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 10,
	},

	[JZ4780_CLK_AHBMON] = {
		"ahb_mon", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 11,
	},

	[JZ4780_CLK_SMB4] = {
		"smb4", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_PCLK, -1 },
		.gate_bit = 32 + 12,
	},

	[JZ4780_CLK_DES] = {
		"des", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 13,
	},

	[JZ4780_CLK_X2D] = {
		"x2d", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_EXCLK, -1 },
		.gate_bit = 32 + 14,
	},

	[JZ4780_CLK_CORE1] = {
		"core1", CGU_CLK_GATE,
		.parents = { JZ4780_CLK_CPU, -1 },
		.gate_bit = 32 + 15,
	},

};

static void __init jz4780_cgu_init(struct device_node *np)
{
	int retval;

	cgu = jz47xx_cgu_new(jz4780_cgu_clocks, ARRAY_SIZE(jz4780_cgu_clocks),
			     np);
	if (!cgu)
		pr_err("%s: failed to initialise CGU\n", __func__);

	retval = jz47xx_cgu_register_clocks(cgu);
	if (retval)
		pr_err("%s: failed to register CGU Clocks\n", __func__);

	clk_set_parent(cgu->clocks.clks[JZ4780_CLK_UHC],
		       cgu->clocks.clks[JZ4780_CLK_MPLL]);
}
CLK_OF_DECLARE(jz4780_cgu, "ingenic,jz4780-cgu", jz4780_cgu_init);

int jz4780_cgu_set_usb_suspend(enum jz4780_usb_port port, bool suspend)
{
	unsigned long flags;
	u32 opcr, bit;

	switch (port) {
	case USB_PORT_OTG:
		bit = OPCR_SPENDN0;
		break;

	case USB_PORT_HOST:
		bit = OPCR_SPENDN1;
		break;

	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&cgu->power_lock, flags);

	opcr = readl(cgu->base + CGU_REG_OPCR);
	if (suspend)
		opcr &= ~bit;
	else
		opcr |= bit;
	writel(opcr, cgu->base + CGU_REG_OPCR);

	spin_unlock_irqrestore(&cgu->power_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(jz4780_cgu_set_usb_suspend);

int jz4780_cgu_set_usb_otg_mode(enum jz4780_usb_otg_mode mode)
{
	unsigned long flags;
	u32 usbpcr1;
	int retval = 0;

	spin_lock_irqsave(&cgu->power_lock, flags);
	usbpcr1 = readl(cgu->base + CGU_REG_USBPCR1);

	switch (mode) {
	case USB_OTG_MODE_MENTOR:
		usbpcr1 &= USBPCR1_USB_SEL;
		break;

	case USB_OTG_MODE_SYNOPSYS:
		usbpcr1 |= USBPCR1_USB_SEL;
		break;

	default:
		retval = -EINVAL;
		goto out;
	}

	writel(usbpcr1, cgu->base + CGU_REG_USBPCR1);

out:
	spin_unlock_irqrestore(&cgu->power_lock, flags);

	return retval;
}
EXPORT_SYMBOL_GPL(jz4780_cgu_set_usb_otg_mode);

int jz4780_cgu_set_usb_utmi_bus_width(enum jz4780_usb_port port,
				      enum jz4780_usb_utmi_bus_width width)
{
	unsigned long flags;
	u32 usbpcr1, bit;
	int retval = 0;

	switch (port) {
	case USB_PORT_OTG:
		bit = USBPCR1_WORD_IF0;
		break;

	case USB_PORT_HOST:
		bit = USBPCR1_WORD_IF1;
		break;

	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&cgu->power_lock, flags);
	usbpcr1 = readl(cgu->base + CGU_REG_USBPCR1);

	switch (width) {
	case USB_PORT_UTMI_BUS_WIDTH_8:
		usbpcr1 &= bit;
		break;

	case USB_PORT_UTMI_BUS_WIDTH_16:
		usbpcr1 |= bit;
		break;

	default:
		retval = -EINVAL;
		goto out_unlock;
	}

	writel(usbpcr1, cgu->base + CGU_REG_USBPCR1);

out_unlock:
	spin_unlock_irqrestore(&cgu->power_lock, flags);
	return retval;
}
EXPORT_SYMBOL_GPL(jz4780_cgu_set_usb_utmi_bus_width);

void jz4780_cgu_set_usb_iddigfil(u32 value)
{
	unsigned long flags;
	u32 usbvbfil;

	spin_lock_irqsave(&cgu->power_lock, flags);

	usbvbfil = readl(cgu->base + CGU_REG_USBPCR1);
	usbvbfil &= ~USBVBFIL_IDDIGFIL_MASK;
	usbvbfil |= value;
	writel(usbvbfil, cgu->base + CGU_REG_USBVBFIL);

	spin_unlock_irqrestore(&cgu->power_lock, flags);
}
EXPORT_SYMBOL_GPL(jz4780_cgu_set_usb_iddigfil);

void jz4780_cgu_set_usb_usbvbfil(u32 value)
{
	unsigned long flags;
	u32 usbvbfil;

	spin_lock_irqsave(&cgu->power_lock, flags);

	usbvbfil = readl(cgu->base + CGU_REG_USBPCR1);
	usbvbfil &= ~USBVBFIL_USBVBFIL_MASK;
	usbvbfil |= value;
	writel(usbvbfil, cgu->base + CGU_REG_USBVBFIL);

	spin_unlock_irqrestore(&cgu->power_lock, flags);
}
EXPORT_SYMBOL_GPL(jz4780_cgu_set_usb_usbvbfil);

void jz4780_cgu_set_usb_usbrdt(u32 value)
{
	unsigned long flags;
	u32 usbrdt;

	spin_lock_irqsave(&cgu->power_lock, flags);

	usbrdt = readl(cgu->base + CGU_REG_USBRDT);
	usbrdt &= ~USBRDT_USBRDT_MASK;
	usbrdt |= value;
	writel(usbrdt, cgu->base + CGU_REG_USBRDT);

	spin_unlock_irqrestore(&cgu->power_lock, flags);
}
EXPORT_SYMBOL_GPL(jz4780_cgu_set_usb_usbrdt);

void jz4780_cgu_set_usb_vbfil_ld_en(bool enable)
{
	unsigned long flags;
	u32 usbrdt;

	spin_lock_irqsave(&cgu->power_lock, flags);

	usbrdt = readl(cgu->base + CGU_REG_USBRDT);
	usbrdt &= ~USBRDT_VBFIL_LD_EN;
	usbrdt |= (enable & USBRDT_VBFIL_LD_EN);
	writel(usbrdt, cgu->base + CGU_REG_USBRDT);

	spin_unlock_irqrestore(&cgu->power_lock, flags);
}
EXPORT_SYMBOL_GPL(jz4780_cgu_set_usb_vbfil_ld_en);

void jz4780_cgu_usb_reset(void)
{
	unsigned long flags;
	u32 usbpcr;

	spin_lock_irqsave(&cgu->power_lock, flags);
	usbpcr = readl(cgu + CGU_REG_USBPCR);
	writel(usbpcr | USBPCR_POR, cgu + CGU_REG_USBPCR);

	mdelay(1);
	usbpcr = readl(cgu + CGU_REG_USBPCR);
	writel(usbpcr & (~USBPCR_POR), cgu + CGU_REG_USBPCR);
	spin_unlock_irqrestore(&cgu->power_lock, flags);
}
EXPORT_SYMBOL_GPL(jz4780_cgu_usb_reset);

int jz4780_cgu_set_usbpcr_param(u32 param, bool enable)
{
	unsigned long flags;
	u32 usbpcr;

	switch (param) {
	case USBPCR_USB_MODE:
	case USBPCR_COMMONONN:
	case USBPCR_VBUSVLDEXT:
	case USBPCR_VBUSVLDEXTSEL:
	case USBPCR_OTG_DISABLE:
	case USBPCR_TXPREEMPHTUNE:
		break;
	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&cgu->power_lock, flags);

	usbpcr = readl(cgu->base + CGU_REG_USBPCR);

	if (enable)
		usbpcr |= param;
	else
		usbpcr &= ~param;

	writel(usbpcr, cgu->base + CGU_REG_USBPCR);

	spin_unlock_irqrestore(&cgu->power_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(jz4780_cgu_set_usbpcr_param);

void jz4780_cgu_stop_ehci(void)
{
	u32 reg;

	reg = readl(cgu->base + CGU_REG_OPCR);
	writel(reg & (~OPCR_SPENDN1), cgu->base + CGU_REG_OPCR);
}
EXPORT_SYMBOL_GPL(jz4780_cgu_stop_ehci);

int jz4780_cgu_start_ehci(void)
{
	static int has_reset;
	int ret = 0;
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&cgu->power_lock, flags);
	reg = readl(cgu->base + CGU_REG_USBPCR);
	writel(reg & (~USBPCR_OTG_DISABLE), cgu->base + CGU_REG_USBPCR);

	/* The PLL uses CLKCORE as reference */
	reg = readl(cgu->base + CGU_REG_USBPCR1);
	writel(reg | USBPCR1_REFCLKSEL_MASK, cgu->base + CGU_REG_USBPCR1);
	spin_unlock_irqrestore(&cgu->power_lock, flags);

	/* NOTE: hw and parent_rate aren't used, so we can set them to
	 * whatever. They are only there for compatibility with clk_ops.*/
	jz4780_otg_phy_set_rate(NULL,
			clk_get_rate(cgu->clocks.clks[JZ4780_CLK_EXCLK]), 0);

	spin_lock_irqsave(&cgu->power_lock, flags);
	/* Don't force port1(uhc) into suspend mode. */
	reg = readl(cgu->base + CGU_REG_OPCR);
	writel(reg | OPCR_SPENDN1, cgu->base + CGU_REG_OPCR);

	/* port1's pulldown resistance on D- */
	reg = readl(cgu->base + CGU_REG_USBPCR1);
	writel(reg | USBPCR1_DMPD1, cgu->base + CGU_REG_USBPCR1);

	/* port1's pulldown resistance on D+ */
	reg = readl(cgu->base + CGU_REG_USBPCR1);
	writel(reg | USBPCR1_DPPD1, cgu->base + CGU_REG_USBPCR1);
	spin_unlock_irqrestore(&cgu->power_lock, flags);

	ret = jz4780_cgu_set_usb_utmi_bus_width(USB_PORT_HOST,
			USB_PORT_UTMI_BUS_WIDTH_16);
	if (ret)
		return ret;

	/* Set utmi data bus width of controller to 16bit */
	reg = readl((volatile int *)EHCI_REG_UTMI_BUS);
	writel(reg | UTMIBUS_WIDTH, (volatile int *)EHCI_REG_UTMI_BUS);

	jz4780_cgu_usb_reset();

	if (!has_reset) {
		/* UHC soft reset */
		/* TODO: Check if these delays are correct */
		reg = readl(cgu->base + CGU_REG_SRBC);
		writel(reg | SRBC_UHC_SR, cgu->base + CGU_REG_SRBC);
		udelay(300);
		reg = readl(cgu->base + CGU_REG_SRBC);
		writel(reg & ~(SRBC_UHC_SR), cgu->base + CGU_REG_SRBC);
		udelay(300);
		has_reset = 1;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(jz4780_cgu_start_ehci);
