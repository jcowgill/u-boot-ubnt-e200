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
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>

DECLARE_GLOBAL_DATA_PTR;

int checkboard(void)
{
	return 0;
}

int early_board_init(void)
{
	int cpu_ref = DEFAULT_CPU_REF_FREQUENCY_MHZ;
	uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH];
	int addr;
	gd->ogd.tlv_addr = OCTEON_EBB6100_BOARD_EEPROM_TWSI_ADDR;

	memset((void *)&(gd->ogd.mac_desc), 0x0,
	       sizeof(octeon_eeprom_mac_addr_t));
	memset((void *)&(gd->ogd.clock_desc), 0x0,
	       sizeof(octeon_eeprom_clock_desc_t));
	memset((void *)&(gd->ogd.board_desc), 0x0,
	       sizeof(octeon_eeprom_board_desc_t));

	/* NOTE: this is early in the init process, so the serial port is not
	 * yet configured
	 */

	/* Determine board type/rev */
	strncpy((char *)(gd->ogd.board_desc.serial_str), "unknown", SERIAL_LEN);
	addr = octeon_tlv_get_tuple_addr(gd->ogd.tlv_addr,
					 EEPROM_BOARD_DESC_TYPE, 0, ee_buf,
					 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	if (addr < 0) {
		gd->ogd.tlv_addr = OCTEON_SFF6100_BOARD_EEPROM_TWSI_ADDR;
		addr = octeon_tlv_get_tuple_addr(gd->ogd.tlv_addr,
						 EEPROM_BOARD_DESC_TYPE, 0,
						 ee_buf,
						 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	}
	if (addr >= 0) {
		memcpy((void *)&(gd->ogd.board_desc), ee_buf,
		       sizeof(octeon_eeprom_board_desc_t));
	} else {
		printf("Unknown board type\n");
		gd->flags |= GD_FLG_BOARD_DESC_MISSING;
		gd->ogd.board_desc.rev_minor = 0;
		gd->ogd.board_desc.board_type = CVMX_BOARD_TYPE_GENERIC;
		/* Try to determine board rev by looking at PAL */
		gd->ogd.board_desc.rev_major = 1;
	}

	/* Populate global data from eeprom */
	addr = octeon_tlv_get_tuple_addr(gd->ogd.tlv_addr,
					 EEPROM_DDR_CLOCK_DESC_TYPE, 0, ee_buf,
					 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	if (addr >= 0) {
		octeon_eeprom_ddr_clock_desc_t *ddr_clock_ptr = (void *)ee_buf;
		gd->ogd.ddr_clock_mhz = ddr_clock_ptr->ddr_clock_hz / 1000000;
	} else {
		gd->ogd.ddr_clock_mhz = GENERIC_EMMC_DEF_DRAM_FREQ;
		gd->flags |= GD_FLG_CLOCK_DESC_MISSING;
	}

	/* CN63XX has a fixed 50 MHz reference clock */
	gd->ogd.ddr_ref_hertz = 50000000;

	if (gd->ogd.ddr_clock_mhz < 100 || gd->ogd.ddr_clock_mhz > 2000) {
		gd->ogd.ddr_clock_mhz = GENERIC_EMMC_DEF_DRAM_FREQ;
	}

	addr = octeon_tlv_get_tuple_addr(gd->ogd.tlv_addr,
					 EEPROM_MAC_ADDR_TYPE, 0, ee_buf,
					 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	if (addr >= 0) {
		memcpy((void *)&(gd->ogd.mac_desc), ee_buf,
		       sizeof(octeon_eeprom_mac_addr_t));
	} else {
		/* Make up some MAC addresses */
                uint16_t psuedorandom = 0;
                int i;

                /* Compute a 16-bit, psuedorandom number from the
                 * board revision and serial number string
		 */
                psuedorandom = (gd->ogd.board_desc.rev_major << 4) | gd->ogd.board_desc.rev_minor;
                for (i=0; i<strlen((const char*)gd->ogd.board_desc.serial_str); ++i) {
			psuedorandom = (psuedorandom << (1&i))
				       + gd->ogd.board_desc.serial_str[i];
                }

		gd->ogd.mac_desc.count = 255;
		gd->ogd.mac_desc.mac_addr_base[0] = 0x00;
		gd->ogd.mac_desc.mac_addr_base[1] = 0xDE;
		gd->ogd.mac_desc.mac_addr_base[2] = 0xAD;
		gd->ogd.mac_desc.mac_addr_base[3] = (psuedorandom>>0) & 0xff;
		gd->ogd.mac_desc.mac_addr_base[4] = (psuedorandom>>8) & 0xff;
		gd->ogd.mac_desc.mac_addr_base[5] = 0x00;
	}

	/* Default if cpu reference clock reading fails. */
	cpu_ref = DEFAULT_CPU_REF_FREQUENCY_MHZ;

	/* Read CPU clock multiplier */
	gd->ogd.cpu_clock_mhz = octeon_get_cpu_multiplier() * cpu_ref;

	return 0;
}

void late_board_init(void)
{
	char buf[256];
	char *board_name;
	board_name = getenv("boardname");
	if (board_name) {
		sprintf(buf, "u-boot-octeon_%s.bin", board_name);
		setenv("octeon_stage3_bootloader", buf);
	}
}