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
#include <asm/gpio.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_board_common.h>
#include <miiphy.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/cvmx-helper-jtag.h>

DECLARE_GLOBAL_DATA_PTR;

int board_early_init_f(void)
{
	return 0;
}

/**
 * Modify the device tree to remove all unused interface types.
 */
int board_fixup_fdt(void)
{
	return 0;
}

/* Raise an integer to a power */
static uint64_t ipow(uint64_t base, uint64_t exp)
{
	uint64_t result = 1;
	while (exp) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

int checkboard(void)
{

	/* Get MAC addresses from Environment. */
	gd->ogd.mac_desc.count = 2;
	eth_getenv_enetaddr_by_index("eth", 0, gd->ogd.mac_desc.mac_addr_base);
	/* Set GPIO 5, 8 as output, starting low */
	gpio_direction_output(5, 0);	/* IPPS_TP */
	gpio_direction_output(8, 0);	/* RF_RESETB_GPIO */
	printf("Clearing GPIOs 5,8 waiting 1 second.\n");

	/* Wait 1 second*/
	mdelay(1000);

	/* set GPIOs high*/
	gpio_set_value(5, 1);
	gpio_set_value(8, 1);
	printf("Setting GPIOs 5,8 high.\n");



	return 0;
}

int early_board_init(void)
{
	/* Configure the QLM modes.... */
	{
		cvmx_mio_qlmx_cfg_t qlm_cfg;

		/* Configure QLM0 as 2x SGMII */
		qlm_cfg.u64= 0;
		qlm_cfg.s.qlm_spd = 0x3;
		qlm_cfg.s.qlm_cfg = 0x2;
		cvmx_write_csr(CVMX_MIO_QLMX_CFG(0), qlm_cfg.u64);

		/* Configure QLM1 as disabled */
		qlm_cfg.u64= 0;
		qlm_cfg.s.qlm_spd = 0xf;
		cvmx_write_csr(CVMX_MIO_QLMX_CFG(1), qlm_cfg.u64);

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

	gd->ogd.ddr_clock_mhz = CNF71XX_REF_DEF_DRAM_FREQ;

	gd->ogd.board_desc.board_type = CVMX_BOARD_TYPE_CNF71XX_REF;
	gd->ogd.board_desc.rev_major = 1;
	gd->ogd.board_desc.rev_minor = 1;

	/* CNF71XX has a fixed 50 MHz reference clock */
	gd->ogd.ddr_ref_hertz = 50000000;

	if (gd->ogd.ddr_clock_mhz < 100 || gd->ogd.ddr_clock_mhz > 2000) {
		gd->ogd.ddr_clock_mhz = CNF71XX_REF_DEF_DRAM_FREQ;
	}

	/* Read CPU clock multiplier */
	gd->ogd.cpu_clock_mhz = octeon_get_cpu_multiplier() * DEFAULT_CPU_REF_FREQUENCY_MHZ;

	return 0;
}


int late_board_init(void)
{
    cvmx_bbp_adma_init(0);
    return(0);
}










/* Bring back gunzip command that is used by command scripts */
static int do_unzip(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned long src, dst;
	unsigned long src_len = ~0UL, dst_len = ~0UL;
	char buf[32];

	switch (argc) {
		case 4:
			dst_len = simple_strtoul(argv[3], NULL, 16);
			/* fall through */
		case 3:
			src = simple_strtoul(argv[1], NULL, 16);
			dst = simple_strtoul(argv[2], NULL, 16);
			break;
		default:
			return CMD_RET_USAGE;
	}

	if (gunzip((void *) dst, dst_len, (void *) src, &src_len) != 0)
		return 1;

	printf("Uncompressed size: %ld = 0x%lX\n", src_len, src_len);
	sprintf(buf, "%lX", src_len);
	setenv("filesize", buf);
	sprintf(buf, "%lX", dst);
	setenv("fileaddr", buf);

	return 0;
}

U_BOOT_CMD(
       gunzip, 4,      1,      do_unzip,
       "unzip a memory region",
       "srcaddr dstaddr [dstsize]"
);

