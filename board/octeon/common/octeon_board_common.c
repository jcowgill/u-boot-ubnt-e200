/***********************license start************************************
 * Copyright (c) 2012 Cavium Inc. (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Cavium Inc. nor the names of
 *       its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 *
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM INC. MAKES NO PROMISES, REPRESENTATIONS
 * OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 * RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 * REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 * DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 * PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 * POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 * OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 * For any questions regarding licensing please contact
 * support@cavium.com
 *
 ***********************license end**************************************/

#include <common.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/octeon-model.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/cvmx-asm.h>
#include <asm/arch/cvmx-mio-defs.h>
#include <asm/arch/octeon_boot_bus.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/octeon_board_phy.h>
#ifdef CONFIG_OCTEON_ENABLE_PAL
#  include <asm/arch/octeon_boot.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

/**
 * Dynamically adjust the board name, used for prompt generation
 * @param name - name string to be adjusted
 * @param max_len - maximum length of name
 *
 * This function can overwrite the name of a board, for example based on
 * the processor installed.
 */
void octeon_adjust_board_name(char *name, size_t max_len) __attribute__((weak));
void octeon_adjust_board_name(char *name, size_t max_len)
{
	return;
}

#ifdef CONFIG_OCTEON_ENABLE_PAL
void show_pal_info(void) __attribute__((weak, alias("__show_pal_info")));
void __show_pal_info(void)
{
	int pal_rev_maj = 0;
	int pal_rev_min = 0;
	int voltage_100ths = 0;
	int voltage_1s = 0;
	int mcu_rev_maj = 0;
	int mcu_rev_min = 0;
	char tmp[10];
	uint64_t pal_addr;

	if (octeon_boot_bus_get_dev_info("/soc/bootbus/pal",
					 "cavium,ebt3000-pal", &pal_addr, NULL))
		pal_addr = gd->ogd.pal_addr;

	if (!pal_addr) {
		debug("%s: PAL not found\n", __func__);
		octeon_led_str_write("Boot    ");
		return;
	}

	if (octeon_read64_byte(pal_addr) == 0xa5
	    && octeon_read64_byte(pal_addr + 1) == 0x5a) {
		pal_rev_maj = octeon_read64_byte(pal_addr + 2);
		pal_rev_min = octeon_read64_byte(pal_addr + 3);
		if ((octeon_read64_byte(pal_addr + 4)) > 0xf) {
			voltage_1s = 0;
			voltage_100ths =
			    (600 + (31 - octeon_read64_byte(pal_addr + 4)) * 25);
		} else {
			voltage_1s = 1;
			voltage_100ths =
			    ((15 - octeon_read64_byte(pal_addr + 4)) * 5);
		}
	}

	if (twsii_mcu_read(0x00) == 0xa5
	    && twsii_mcu_read(0x01) == 0x5a) {
		gd->ogd.mcu_rev_maj = mcu_rev_maj = twsii_mcu_read(2);
		gd->ogd.mcu_rev_min = mcu_rev_min = twsii_mcu_read(3);
	}

	printf("PAL rev: %d.%02d, MCU rev: %d.%02d, CPU voltage: %d.%02d\n",
	       pal_rev_maj, pal_rev_min, mcu_rev_maj, mcu_rev_min,
	       voltage_1s, voltage_100ths);

	/* Display CPU speed on display */
	sprintf(tmp, "%d %.1d.%.2d ", gd->ogd.cpu_clock_mhz,
		voltage_1s, voltage_100ths);
	octeon_led_str_write(tmp);
}

#endif /* CONFIG_OCTEON_ENABLE_PAL */

/**
 * Generate a random MAC address
 */
void octeon_board_create_random_mac_addr(void)
	__attribute__((weak,alias("__octeon_board_create_random_mac_addr")));

void __octeon_board_create_random_mac_addr(void)
{
	uint8_t fuse_buf[128];
	uint8_t md5_buf[16];
	uint64_t *md5_buf64 = (uint64_t *)md5_buf;
	cvmx_mio_fus_rcmd_t fus_rcmd;
	uint64_t *ptr;
	int ser_len = strlen((char *)gd->ogd.board_desc.serial_str);
	int i;

	memset(fuse_buf, 0, sizeof(fuse_buf));
	fuse_buf[0] = gd->ogd.board_desc.board_type;
	fuse_buf[1] = (gd->ogd.board_desc.rev_major << 4)
			| gd->ogd.board_desc.rev_minor;
	fuse_buf[2] = ser_len;
	strncpy((char*)(fuse_buf+3), (char*)gd->ogd.board_desc.serial_str, sizeof(fuse_buf)-3);

	/* For a random number we perform a MD5 hash using the board type,
	 * revision, serial number length, serial number and for OCTEON 2 and 3
	 * the fuse settings.
	 */

	/* Initialize MD5 hash */
	CVMX_MT_HSH_IV(0x0123456789abcdefull, 0);
	CVMX_MT_HSH_IV(0xfedcba9876543210ull, 1);

	ptr = (uint64_t *)fuse_buf;
	for (i = 0; i < sizeof(fuse_buf); i += 64) {
		CVMX_MT_HSH_DAT(*ptr++, 0);
		CVMX_MT_HSH_DAT(*ptr++, 1);
		CVMX_MT_HSH_DAT(*ptr++, 2);
		CVMX_MT_HSH_DAT(*ptr++, 3);
		CVMX_MT_HSH_DAT(*ptr++, 4);
		CVMX_MT_HSH_DAT(*ptr++, 5);
		CVMX_MT_HSH_DAT(*ptr++, 6);
		CVMX_MT_HSH_STARTMD5(*ptr++);
	}

	if (!OCTEON_IS_OCTEON1PLUS()) {
		fus_rcmd.u64 = 0;
		fus_rcmd.s.pend = 1;
		for (i = 0; i < sizeof(fuse_buf); i++) {
			do {
				fus_rcmd.u64 = cvmx_read_csr(CVMX_MIO_FUS_RCMD);
			} while (fus_rcmd.s.pend == 1);
			fuse_buf[i] = fus_rcmd.s.dat;
		}
		for (i = 0; i < sizeof(fuse_buf); i += 64) {
			CVMX_MT_HSH_DAT(*ptr++, 0);
			CVMX_MT_HSH_DAT(*ptr++, 1);
			CVMX_MT_HSH_DAT(*ptr++, 2);
			CVMX_MT_HSH_DAT(*ptr++, 3);
			CVMX_MT_HSH_DAT(*ptr++, 4);
			CVMX_MT_HSH_DAT(*ptr++, 5);
			CVMX_MT_HSH_DAT(*ptr++, 6);
			CVMX_MT_HSH_STARTMD5(*ptr++);
		}
	}
	/* Get the final MD5 */
	CVMX_MF_HSH_IV(md5_buf64[0], 0);
	CVMX_MF_HSH_IV(md5_buf64[1], 1);

	gd->ogd.mac_desc.count = 255;
	gd->ogd.mac_desc.mac_addr_base[0] = 0x02;	/* locally administered */
	gd->ogd.mac_desc.mac_addr_base[1] = md5_buf[0];
	gd->ogd.mac_desc.mac_addr_base[2] = md5_buf[1];
	gd->ogd.mac_desc.mac_addr_base[3] = md5_buf[2];
	gd->ogd.mac_desc.mac_addr_base[4] = md5_buf[3];
	gd->ogd.mac_desc.mac_addr_base[5] = 0;
}

/**
 * Gets the MAC address for a board from the TLV EEPROM.  If not present a
 * random MAC address is generated.
 */
void octeon_board_get_mac_addr(void)
	__attribute__((weak, alias("__octeon_board_get_mac_addr")));

void __octeon_board_get_mac_addr(void)
{
#if defined(CONFIG_SYS_I2C_EEPROM_ADDR) || defined(CONFIG_SYS_DEF_EEPROM_ADDR)
	uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH];
	int addr;

	addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
					 EEPROM_MAC_ADDR_TYPE, 0, ee_buf,
					 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	if (addr >= 0)
		memcpy((void *)&(gd->ogd.mac_desc), ee_buf,
		       sizeof(octeon_eeprom_mac_addr_t));
	else
#endif
		octeon_board_create_random_mac_addr();
}

/**
 * Gets the board clock info from the TLV EEPROM or uses the default value
 * if not available.
 *
 * @param def_ddr_clock_mhz	Default DDR clock speed in MHz
 */
void octeon_board_get_clock_info(uint32_t def_ddr_clock_mhz)
	__attribute((weak, alias("__octeon_board_get_clock_info")));

void __octeon_board_get_clock_info(uint32_t def_ddr_clock_mhz)
{
	uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH];
	int addr;

	/* Assume no descriptor is present */
	gd->ogd.ddr_clock_mhz = def_ddr_clock_mhz;

#if defined(CONFIG_SYS_I2C_EEPROM_ADDR) || defined(CONFIG_SYS_DEF_EEPROM_ADDR)
	/* OCTEON I and OCTEON Plus use the old clock descriptor of which
	 * there are two versions.  OCTEON II uses a dedicated DDR clock
	 * descriptor instead.
	 */
	if (OCTEON_IS_OCTEON1PLUS()) {
		octeon_eeprom_header_t *header;
		struct octeon_eeprom_clock_desc_v1 *clock_v1;
		struct octeon_eeprom_clock_desc_v2 *clock_v2;

		addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
						 EEPROM_CLOCK_DESC_TYPE, 0,
						 ee_buf,
						 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
		if (header < 0)
			return;

		header = (octeon_eeprom_header_t *)ee_buf;
		switch (header->major_version) {
		case 1:
			clock_v1 = (struct octeon_eeprom_clock_desc_v1 *)ee_buf;
			gd->ogd.ddr_clock_mhz = clock_v1->ddr_clock_mhz;
			break;
		case 2:
			clock_v2 = (struct octeon_eeprom_clock_desc_v2 *)ee_buf;
			gd->ogd.ddr_clock_mhz = clock_v2->ddr_clock_mhz;
			break;
		default:
			printf("Unknown TLV clock header version %d.%d\n",
			       header->major_version, header->minor_version);
		}
	} else {
		addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
						 EEPROM_DDR_CLOCK_DESC_TYPE, 0,
						 ee_buf,
						 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
		if (addr < 0)
			return;


		octeon_eeprom_ddr_clock_desc_t *ddr_clock_ptr =
				 (octeon_eeprom_ddr_clock_desc_t *)ee_buf;
		gd->ogd.ddr_clock_mhz = ddr_clock_ptr->ddr_clock_hz / 1000000;
	}
#endif

	if (gd->ogd.ddr_clock_mhz < 100 || gd->ogd.ddr_clock_mhz > 2000)
		gd->ogd.ddr_clock_mhz = def_ddr_clock_mhz;
}

/**
 * Reads the board descriptor from the TLV EEPROM or fills in the default
 * values.
 *
 * @param type		board type
 * @param rev_major	board major revision
 * @param rev_minor	board minor revision
 */
void octeon_board_get_descriptor(enum cvmx_board_types_enum type,
				 int rev_major, int rev_minor)
	__attribute__((weak, alias("__octeon_board_get_descriptor")));

void __octeon_board_get_descriptor(enum cvmx_board_types_enum type,
				   int rev_major, int rev_minor)
{
#if defined(CONFIG_SYS_I2C_EEPROM_ADDR) || defined(CONFIG_SYS_DEF_EEPROM_ADDR)
	uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH];
	int addr;
	/* Determine board type/rev */
	strncpy((char *)(gd->ogd.board_desc.serial_str), "unknown", SERIAL_LEN);
	addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
					 EEPROM_BOARD_DESC_TYPE, 0, ee_buf,
					 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
	if (addr >= 0) {
		memcpy((void *)&(gd->ogd.board_desc), ee_buf,
		       sizeof(octeon_eeprom_board_desc_t));
	} else
#endif
	{
		gd->flags |= GD_FLG_BOARD_DESC_MISSING;
		gd->ogd.board_desc.rev_major = rev_major;
		gd->ogd.board_desc.rev_minor = rev_minor;
		gd->ogd.board_desc.board_type = type;
	}
}


/**
 * Function to write string to LED display
 * @param str - string up to 8 characters to write.
 */
void octeon_led_str_write(const char *str)
	__attribute__((weak, alias("__octeon_led_str_write")));

void __octeon_led_str_write(const char *str)
{
#ifdef CONFIG_OCTEON_ENABLE_LED_DISPLAY
	octeon_led_str_write_std(str);
#endif
}

void __board_mdio_init(void)
{
#ifdef CONFIG_OF_LIBFDT
	octeon_board_phy_init();
#endif
}

void board_mdio_init(void) __attribute__((weak, alias("__board_mdio_init")));


