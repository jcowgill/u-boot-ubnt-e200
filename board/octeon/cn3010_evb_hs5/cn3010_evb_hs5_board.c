/*
 * (C) Copyright 2004 - 2012 Cavium Inc. <support@cavium.com>
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
#include <command.h>
#include <asm/mipsregs.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_board_common.h>
#include <pci.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <miiphy.h>
#include <mtd/cfi_flash.h>
#include <asm/arch/octeon_boot_bus.h>

typedef union {
	uint16_t u16;
	struct {
		uint16_t busy:1, rsv:2, mode:1, op:2, device:5, reg:5;
	} s;
} smi_cmd;

typedef union {
	uint16_t u16;
	struct {
		uint16_t busy:1, op:3, mode:2, rsv:4, ptr:6;
	} s;
} marvel_status_cmd;

DECLARE_GLOBAL_DATA_PTR;

static const char devname[20] = "mdio-octeon0\0";

#if defined(CONFIG_CMD_MII)
#define mdio_write(w,x,y,z) miiphy_write(w,x,y,z)
#define mdio_read(w,x,y,z) miiphy_read(w,x,y,z)

/* In order to determine whether the switch is in single-chip or multi-chip mode, we attempt
 * to read the switch identifier register for port 9.
 *
 * This may wind up making some version of the EBH3100 boot up slowly, as it goes through a timeout cycle.
 */
static int marvell_sense_multi_chip_mode(void)
{
	int result;
	uint16_t value;

	result = mdio_read(devname, 0x19, 3, &value);
	return (value != 0x1a52);
}

void board_mdio_init(void)
{
	uint16_t value;

	if (marvell_sense_multi_chip_mode()) {
		printf("\n\nERROR: marvell switch in multi-chip mode!!!!!\n\n");

	} else {
		/* Force a reset of the switch */
		mdio_read(devname, 0x1B, 4, &value);
		mdio_write(devname, 0x1B, 4, value | 0x8000);
		do {
			mdio_read(devname, 0x1B, 4, &value);
		} while (value & 0x8000);

		/* Force Port9 link speed and state. */
		mdio_write(devname, 0x19, 1, 0x003E);

		/* Turn off PPU. */
		mdio_read(devname, 0x1B, 4, &value);
		mdio_write(devname, 0x1B, 4, value & ~0x4000);

		/* Clear MGMII mode */
		mdio_read(devname, 0x10, 0x00, &value);
		mdio_write(devname, 0x10, 0x00, value & ~0x0040);

		mdio_read(devname, 0x11, 0x00, &value);
		mdio_write(devname, 0x11, 0x00, value & ~0x0040);

		mdio_read(devname, 0x12, 0x00, &value);
		mdio_write(devname, 0x12, 0x00, value & ~0x0040);

		mdio_read(devname, 0x13, 0x00, &value);
		mdio_write(devname, 0x13, 0x00, value & ~0x0040);

		/* Turn PPU back on. */
		mdio_read(devname, 0x1B, 4, &value);
		mdio_write(devname, 0x1B, 4, value | 0x4000);
		cvmx_wait(0x40000);

		if (miiphy_read(devname, 0x04, 27, &value) >= 0) {
			if ((value & 0xf) == 0xf) {
				printf("!!! Configuring Port 0 for MII mode !!!\n");
				cvmx_write_csr(CVMX_GMXX_INF_MODE(0), 0x7);
			}
		}
	}
}

#endif /* MII */


int checkboard(void)
{
	if (octeon_show_info())
		show_pal_info();
	else
		octeon_led_str_write("Boot    ");

	/* Set up switch complex */

	/* Force us into GMII mode, enable interface */
	/* NOTE:  Applications expect this to be set appropriately
	 ** by the bootloader, and will configure the interface accordingly */
	cvmx_write_csr(CVMX_GMXX_INF_MODE(0), 0x3);

	/* Enable SMI to talk with the GMII switch */
	cvmx_write_csr(CVMX_SMI_EN, 0x1);
	cvmx_read_csr(CVMX_SMI_EN);


	return 0;
}

int early_board_init(void)
{
	int cpu_ref = 50;

	memset((void *)&(gd->ogd.mac_desc), 0x0, sizeof(octeon_eeprom_mac_addr_t));
	memset((void *)&(gd->ogd.clock_desc), 0x0,
	       sizeof(octeon_eeprom_clock_desc_t));
	memset((void *)&(gd->ogd.board_desc), 0x0,
	       sizeof(octeon_eeprom_board_desc_t));

	/* NOTE: this is early in the init process, so the serial port is not yet configured */

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

	octeon_board_get_descriptor(CVMX_BOARD_TYPE_CN3010_EVB_HS5, 2, 0);

	/* Even though the CPU ref freq is stored in the clock descriptor, we don't read it here.  The MCU
	 * reads it and configures the clock, and we read how the clock is actually configured.
	 * The bootloader does not need to read the clock descriptor tuple for normal operation on
	 * ebh3100 boards
	 */
	cpu_ref = octeon_mcu_read_cpu_ref();
	gd->ogd.ddr_clock_mhz = octeon_mcu_read_ddr_clock();

	/* CN50XX has a fixed internally generated 50 MHz reference clock */
	if (OCTEON_IS_MODEL(OCTEON_CN50XX)) {
		gd->ogd.ddr_ref_hertz = 50000000;
		gd->ogd.ddr_clock_mhz = gd->ogd.clock_desc.ddr_clock_mhz;
	}

	/* Some sanity checks */
	if (cpu_ref <= 0) {
		/* Default to 50 */
		cpu_ref = 50;
	}
	if (gd->ogd.ddr_clock_mhz <= 0) {
		/* Default to 200 */
		gd->ogd.ddr_clock_mhz = 200;
	}

	octeon_board_get_mac_addr();

	/* Read CPU clock multiplier */
	cvmx_dbg_data_t dbg_data;
	dbg_data.u64 = cvmx_read_csr(CVMX_DBG_DATA);
	int mul = dbg_data.cn30xx.c_mul;
	gd->ogd.cpu_clock_mhz = mul * cpu_ref;

	/* adjust for 33.33 Mhz clock */
	if (cpu_ref == 33)
		gd->ogd.cpu_clock_mhz += (mul) / 4 + mul / 8;

	if (gd->ogd.cpu_clock_mhz < 100 || gd->ogd.cpu_clock_mhz > 900) {
		gd->ogd.cpu_clock_mhz = DEFAULT_ECLK_FREQ_MHZ;
	}
	return 0;
}
