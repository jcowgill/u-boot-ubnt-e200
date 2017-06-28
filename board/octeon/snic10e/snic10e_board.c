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
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/octeon_board_phy.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/cvmx-qlm.h>
#include <asm/gpio.h>

DECLARE_GLOBAL_DATA_PTR;

int checkboard(void)
{
	return 0;
}

void board_mdio_init(void)
{
	static const uint8_t buffer1[] = { 0x1e, 0x01, 0x08, 0x00, 0x00 };
	static const uint8_t buffer2[] = { 0x1e, 0x01, 0x2c, 0x00, 0x00 };
	octeon_board_phy_init();

	if (gd->ogd.board_desc.rev_major < 2) {
		/* We only need to use twsi for the small number of rev 1
		 * boards.
		 * For production boards (rev 2 and later) the MDIO writes above
		 * serve the same purpose.
		 */
		i2c_set_bus_num(1);
		i2c_write(0x40, 5, 1, buffer1, sizeof(buffer1));
		i2c_write(0x40, 5, 1, buffer2, sizeof(buffer2));
		i2c_set_bus_num(0);
	}
}

int early_board_init(void)
{
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
	}
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
	octeon_board_get_clock_info(SNIC10E_DEF_DRAM_FREQ);

	octeon_board_get_descriptor(CVMX_BOARD_TYPE_SNIC10E, 1, 0);

	/* CN63XX has a fixed 50 MHz reference clock */
	gd->ogd.ddr_ref_hertz = 50000000;

	octeon_board_get_mac_addr();

	/* Read CPU clock multiplier */
	gd->ogd.cpu_clock_mhz = octeon_get_cpu_multiplier() * 50;

	/* Reset the PHY and set the mode */
	gpio_direction_output(15, 1);
	gpio_direction_output(16, 1);
	gpio_direction_output(17, 0);
	udelay(10);
	gpio_set_value(17, 1);
	return 0;
}

int late_board_init(void)
{
	int qlm;

	for (qlm = 1; qlm <= 2; qlm++)
		octeon_board_vcs8488_qlm_tune(qlm);

	return 0;
}
