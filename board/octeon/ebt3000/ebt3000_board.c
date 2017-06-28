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

#include <config.h>
#include <common.h>
#include <command.h>
#include <asm/mipsregs.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_board_common.h>
#include <pci.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/cvmx-spi.h>
#include <asm/arch/octeon_fdt.h>

DECLARE_GLOBAL_DATA_PTR;

int octeon_ebt3000_get_board_major_rev(void)
{
    return(gd->ogd.board_desc.rev_major);
}
int octeon_ebt3000_get_board_minor_rev(void)
{
    return(gd->ogd.board_desc.rev_minor);
}

/**
 * Modify the device tree to remove all unused interface types.
 */
int board_fixup_fdt(void)
{
	const char *fdt_key;
	int is_spi4000 = 0;
	int is_spi = 0;
	union cvmx_gmxx_inf_mode gmx_mode;

	gmx_mode.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(0));

	if (!gmx_mode.cn38xx.en) {
		fdt_key ="0,none"; /* Disabled */
	} else {
		if (gmx_mode.cn38xx.type) {
			fdt_key = "0,spi";
			is_spi4000 = cvmx_spi4000_is_present(0);
			is_spi = 1;
		} else {
			fdt_key = "0,rgmii";
		}
	}
	octeon_fdt_patch(working_fdt, fdt_key, NULL);

	if (is_spi) {
		if (is_spi4000)
			fdt_key = "9,spi4000";
		else
			fdt_key = "9,not-spi4000";
		octeon_fdt_patch(working_fdt, fdt_key, NULL);
	}
	return 0;
}

int checkboard(void)
{

	if (octeon_ebt3000_get_board_major_rev() == 1) {
		printf("Error: EBT3000 revision 1 board not supported!\n");
		return 1;
	}

	if (octeon_show_info())
		show_pal_info();
	else
		octeon_led_str_write("Boot    ");

	return 0;
}

int early_board_init(void)
{
	int cpu_ref = 33;

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
	uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH];
	int addr;

	addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
					 EEPROM_CLOCK_DESC_TYPE, 0, ee_buf,
					 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	if (addr >= 0) {
		memcpy((void *)&(gd->ogd.clock_desc), ee_buf,
		       sizeof(octeon_eeprom_clock_desc_t));
	}

	octeon_board_get_descriptor(CVMX_BOARD_TYPE_EBT3000, 1, 2);

	if (gd->ogd.board_desc.rev_major == 1) {
		if (gd->ogd.clock_desc.cpu_ref_clock_mhz_x_8) {
			cpu_ref = gd->ogd.clock_desc.cpu_ref_clock_mhz_x_8 / 8;
			gd->ogd.ddr_clock_mhz =
			    gd->ogd.clock_desc.ddr_clock_mhz;
		} else {
			gd->flags |= GD_FLG_CLOCK_DESC_MISSING;
			/* Default values */
			cpu_ref = 33;
			gd->ogd.ddr_clock_mhz = EBT3000_REV1_DEF_DRAM_FREQ;
		}
	} else {
		/* Even though the CPU ref freq is stored in the clock
		 * descriptor, we don't read it here.  The MCU reads it and
		 * configures the clock, and we read how the clock is actually
		 * configured.  The bootloader does not need to read the clock
		 * descriptor tuple for normal operation on rev 2 and later
		 * boards.
		 */
		cpu_ref = octeon_mcu_read_cpu_ref();
		gd->ogd.ddr_clock_mhz = octeon_mcu_read_ddr_clock();
	}

	/* Some sanity checks */
	if (cpu_ref <= 0) {
		/* Default to 33 */
		cpu_ref = 33;
	}
	if (gd->ogd.ddr_clock_mhz <= 0) {
		/* Default to 33 */
		gd->ogd.ddr_clock_mhz = 200;
	}

	/*
	 * For the cn58xx the DDR reference clock frequency is used to
	 * configure the appropriate internal DDR_CK/DDR2_REF_CLK ratio in
	 * order to generate the ddr clock frequency specified by
	 * ddr_clock_mhz.  For cn38xx this setting should match the state
	 * of the DDR2 output clock divide by 2 pin DDR2_PLL_DIV2.
	 *
	 * For DDR@_PLL_DIV2 = 0 the DDR_CK/DDR2_REF_CLK ratio is 4
	 * For DDR@_PLL_DIV2 = 1 the DDR_CK/DDR2_REF_CLK ratio is 2
	 */
#define FIXED_DDR_REF_CLOCK_RATIO	4

	/*
	 * More details about DDR clock configuration used for LMC
	 * configuration for the CN58XX.  Not used for CN38XX.  Since the
	 * reference clock frequency is not known it is inferred from the
	 * specified DCLK frequency divided by the DDR_CK/DDR2_REF_CLK
	 * ratio.
	 */
	gd->ogd.ddr_ref_hertz =
	    gd->ogd.ddr_clock_mhz * 1000000 / FIXED_DDR_REF_CLOCK_RATIO;

	octeon_board_get_mac_addr();

	/* Read CPU clock multiplier */
	uint64_t data = cvmx_read_csr(CVMX_DBG_DATA);
	data = data >> 18;
	data &= 0x1f;

	gd->ogd.cpu_clock_mhz = data * cpu_ref;

	/* adjust for 33.33 Mhz clock */
	if (cpu_ref == 33)
		gd->ogd.cpu_clock_mhz += (data) / 4 + data / 8;

	if (gd->ogd.cpu_clock_mhz < 100 || gd->ogd.cpu_clock_mhz > 600) {
		gd->ogd.cpu_clock_mhz = DEFAULT_ECLK_FREQ_MHZ;
	}
	return 0;
}
