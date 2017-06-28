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
#include <i2c.h>
#include <asm/mipsregs.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>

DECLARE_GLOBAL_DATA_PTR;

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

	if (!gmx_mode.cn58xx.en) {
		fdt_key ="0,none"; /* Disabled */
	} else {
		if (gmx_mode.cn58xx.type) {
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
}

/* Boot bus init for flash and peripheral access */
#define FLASH_RoundUP(_Dividend, _Divisor) (((_Dividend)+(_Divisor))/(_Divisor))
int octeon_boot_bus_init(void)
{
	cvmx_mio_boot_reg_timx_t __attribute__ ((unused)) reg_tim;

#if CONFIG_RAM_RESIDENT
	/* Install the 2nd moveable region to the debug exception main entry
	   point. This way the debugger will work even if there isn't any
	   flash. */
	extern void debugHandler_entrypoint(void);
	const uint64_t *handler_code =
	    (const uint64_t *)debugHandler_entrypoint;
	int count;
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_CFGX(1),
		       (1 << 31) | (0x1fc00480 >> 7 << 3));
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_ADR, 0x80);
	for (count = 0; count < 128 / 8; count++)
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_DAT, *handler_code++);
#endif

	return (0);
}

int checkboard(void)
{
	if (gd->ogd.board_desc.rev_major >= 2) {
		uchar value;
		/* Disable access to the DIMM TWSI eeproms so that secondary
		 * tswi address of the SFP modules does not conflict
		 */
		gpio_direction_output(5, 0);

		/* Enable SFP output using twsi GPIO chip */

		/* Set bits 3, 7, 11, 15 to output.  Registers 2/3 control
		 * the direction of the 16 bits.  1 -> input, 0 -> output.
		 * All the other signals are inputs, so this is a boot time
		 * configuration that only needs to be done once.
		 */
		value = 0x77;
		i2c_write(0x27, 2, 1, &value, 1);
		i2c_write(0x27, 3, 1, &value, 1);

		/* Set the tx disable signal low for the 4 SFP modules.  This
		 * Enables transmit for the 4 SFP modules.
		 */
		i2c_write(0x27, 6, 1, &value, 1);
		i2c_write(0x27, 7, 1, &value, 1);
		/* Reads from addresses 0/1 provide the current
		 * state of the input pins
		 */

	}
	return 0;
}

int early_board_init(void)
{
	int cpu_ref = DEFAULT_CPU_REF_FREQUENCY_MHZ;

	DECLARE_GLOBAL_DATA_PTR;

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

	cpu_ref = DEFAULT_CPU_REF_FREQUENCY_MHZ;
	gd->ogd.ddr_clock_mhz = NICPRO2_REV1_DEF_DRAM_FREQ;
	addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
					 EEPROM_CLOCK_DESC_TYPE, 0, ee_buf,
					 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	if (addr >= 0) {
		memcpy((void *)&(gd->ogd.clock_desc), ee_buf,
		       sizeof(octeon_eeprom_clock_desc_t));
		gd->ogd.ddr_clock_mhz = gd->ogd.clock_desc.ddr_clock_mhz;
	}

	octeon_board_get_descriptor(CVMX_BOARD_TYPE_NICPRO2, 1, 0);


	/*
	 * For the cn58xx the DDR reference clock frequency is used to
	 * configure the appropriate internal DDR_CK/DDR2_REF_CLK ratio in
	 * order to generate the ddr clock frequency specified by
	 * ddr_clock_mhz.  Not used for CN38XX.
// 	 */
#define FIXED_DDR_REF_CLOCK_HERTZ	133333333;
	gd->ogd.ddr_ref_hertz = FIXED_DDR_REF_CLOCK_HERTZ;

	octeon_board_get_mac_addr();

	/* Read CPU clock multiplier */
	uint64_t data = cvmx_read_csr(CVMX_DBG_DATA);
	data = data >> 18;
	data &= 0x1f;

	gd->ogd.cpu_clock_mhz = data * cpu_ref;

	/* adjust for 33.33 Mhz clock */
	if (cpu_ref == 33)
		gd->ogd.cpu_clock_mhz += (data) / 4 + data / 8;

	if (gd->ogd.cpu_clock_mhz < 100 || gd->ogd.cpu_clock_mhz > 1100) {
		gd->ogd.cpu_clock_mhz = DEFAULT_ECLK_FREQ_MHZ;
	}

	octeon_led_str_write("Booting.");
	return 0;

}
