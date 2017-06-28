/*
 * (C) Copyright 2004-2012 Cavium, Inc. <support@cavium.com>
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

#include <common.h>
#include <i2c.h>
#include <asm/gpio.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/octeon_fdt.h>
#include <asm/arch/cvmx-helper-jtag.h>
#include <asm/arch/cvmx-helper.h>
#include <asm/arch/cvmx-helper-board.h>
#include <asm/arch/octeon_eth.h>
#include <cortina_cs4321.h>

extern int nic68_phy_init(void);

DECLARE_GLOBAL_DATA_PTR;

/**
 * Modify the device tree to remove all unused interface types.
 */
int board_fixup_fdt(void)
{
	const char *fdt_key;
	cvmx_mio_qlmx_cfg_t mio_qlmx;

	mio_qlmx.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(0));

	if (mio_qlmx.s.qlm_spd == 15) {
		fdt_key = "0,none";	/* Disabled */
	} else if (mio_qlmx.s.qlm_cfg == 7) {
		fdt_key = "0,rxaui";
		debug("QLM 0 in RXAUI mode\n");
	} else if (mio_qlmx.s.qlm_cfg == 2) {
		fdt_key = "0,sgmii";
		debug("QLM 0 in SGMII mode\n");
	} else {
		debug("Unsupported QLM 0 configuration %d\n",
		      mio_qlmx.s.qlm_cfg);
		fdt_key = "0,none";	/* Disabled */
	}
	octeon_fdt_patch(working_fdt, fdt_key, NULL);

	mio_qlmx.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(3));
	if (mio_qlmx.s.qlm_spd == 15) {
		fdt_key = "3,none";	/* Disabled */
	} else if (mio_qlmx.s.qlm_cfg == 3) {
		debug("QLM 3 in XAUI mode\n");
		fdt_key = "3,xaui";
	} else if (mio_qlmx.s.qlm_cfg == 2) {
		fdt_key = "3,sgmii";
		debug("QLM 3 in SGMII mode\n");
	} else {
		debug("Unsupported QLM 3 configuration %d\n",
		      mio_qlmx.s.qlm_cfg);
		fdt_key = "3,none";
	}
	octeon_fdt_patch(working_fdt, fdt_key, NULL);

	mio_qlmx.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(4));
	if (mio_qlmx.s.qlm_spd == 15) {
		fdt_key = "4,none";	/* Disabled */
	} else if (mio_qlmx.s.qlm_cfg == 3) {
		fdt_key = "4,xaui";
		debug("QLM 4 in XAUI mode\n");
	} else if (mio_qlmx.s.qlm_cfg == 2) {
		fdt_key = "4,sgmii";
		debug("QLM 4 in SGMII mode\n");
	} else {
		debug("Unsupported QLM 4 configuration %d\n",
		      mio_qlmx.s.qlm_cfg);
		fdt_key = "4,none";
	}
	octeon_fdt_patch(working_fdt, fdt_key, NULL);

	/* USB is not present on revision 3 and later of this board. */
	fdt_key = (gd->ogd.board_desc.rev_major >= 3) ? "5,none" : "5,usb";
	octeon_fdt_patch(working_fdt, fdt_key, NULL);

	return 0;
}

/**
 * Callback function used by Cortina PHY
 *
 * @paran[in] phydev - pointer to phy device structure
 *
 * @returns Cortina PHY host mode
 */
enum cortina_cs4321_host_mode
cortina_cs4321_get_host_mode(const struct phy_device *phydev)
{
	const struct octeon_eth_info *oct_eth_info;
	cvmx_phy_host_mode_t mode;
	int ipd_port;

	if (!phydev->dev) {
		printf("%s: Error: eth dev unknown\n", __func__);
		return CS4321_HOST_MODE_UNKNOWN;
	}

	oct_eth_info = (struct octeon_eth_info *)phydev->dev->priv;

	ipd_port = oct_eth_info->port;
	if (ipd_port < 0)
		ipd_port -= CVMX_HELPER_BOARD_MGMT_IPD_PORT;
	mode = cvmx_helper_board_get_phy_host_mode(ipd_port);

	switch (mode) {
	case CVMX_PHY_HOST_MODE_SGMII:
		return CS4321_HOST_MODE_SGMII;
	case CVMX_PHY_HOST_MODE_XAUI:
		return CS4321_HOST_MODE_XAUI;
	case CVMX_PHY_HOST_MODE_RXAUI:
		return CS4321_HOST_MODE_RXAUI;
	default:
		printf("%s: Unknown host mode %d\n", __func__, mode);
	case CVMX_PHY_HOST_MODE_UNKNOWN:
		return CS4321_HOST_MODE_UNKNOWN;
	}
}


int checkboard(void)
{
	cvmx_mio_qlmx_cfg_t mio_qlmx;
	uchar temp;

	mio_qlmx.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(0));

	printf("NIC SFP+ ports configured for %s\n",
	       mio_qlmx.s.qlm_cfg == 7 ? "XAUI" : "SGMII");

	/* Note that the SA56004 will be programmed again later with DTT */
	/* Internal SA56004 temp, approximates ambient/board temp. */
	temp = 80;
	i2c_write(CONFIG_SYS_I2C_DTT_ADDR, 0x20, 1, &temp, 1);

	/* Internal Octeon temp, approximates junction temp. */
	temp = 100;
	i2c_write(CONFIG_SYS_I2C_DTT_ADDR, 0x19, 1, &temp, 1);
	/* Enable USB supply */
	gpio_direction_output(6, 1);

	/* Enable PSRAM (take out of low power mode) */
	gpio_direction_output(0, 1);


	return 0;
}

int early_board_init(void)
{
	/* configure clk_out pin */
	cvmx_mio_fus_pll_t fus_pll;

	fus_pll.u64 = cvmx_read_csr(CVMX_MIO_FUS_PLL);
	fus_pll.cn63xx.c_cout_rst = 1;
	cvmx_write_csr(CVMX_MIO_FUS_PLL, fus_pll.u64);

	/* Sel::  0:rclk, 1:pllout 2:psout 3:gnd */
	fus_pll.cn63xx.c_cout_sel = 0;
	cvmx_write_csr(CVMX_MIO_FUS_PLL, fus_pll.u64);
	fus_pll.cn63xx.c_cout_rst = 0;
	cvmx_write_csr(CVMX_MIO_FUS_PLL, fus_pll.u64);

	memset((void *)&(gd->ogd.mac_desc), 0x0,
	       sizeof(octeon_eeprom_mac_addr_t));
	memset((void *)&(gd->ogd.clock_desc), 0x0,
	       sizeof(octeon_eeprom_clock_desc_t));
	memset((void *)&(gd->ogd.board_desc), 0x0,
	       sizeof(octeon_eeprom_board_desc_t));

	/* NOTE: this is early in the init process, so the serial port is not
	 * yet configured
	 */

	/* Populate global data from eeprom */
	octeon_board_get_clock_info(NIC68_4_DEF_DRAM_FREQ);

	octeon_board_get_descriptor(CVMX_BOARD_TYPE_NIC68_4, 1, 0);

	/* CN63XX has a fixed 50 MHz reference clock */
	gd->ogd.ddr_ref_hertz = 50000000;


	gd->ogd.ddr_clock_mhz = NIC68_4_DEF_DRAM_FREQ;

	octeon_board_get_mac_addr();

	/* Read CPU clock multiplier */
	gd->ogd.cpu_clock_mhz = octeon_get_cpu_multiplier() * 50;

	return 0;
}

extern int phy_cortina_init(void);

int board_phy_init(void)
{
	return phy_cortina_init();
}
