/*
 * Copyright (C) 2013 Ubiquiti Networks, Inc.
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
#include <miiphy.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/cvmx-helper-jtag.h>
#include <asm/arch/cvmx-mdio.h>
#include <asm/arch/cvmx-gpio.h>

DECLARE_GLOBAL_DATA_PTR;

int board_early_init_f(void)
{
	return 0;
}

#define BOARD_E200_MAJOR 0
#define BOARD_E201_MAJOR 1
#define BOARD_E202_MAJOR 2

int board_fixup_fdt(void)
{
	const char *key = "0,sfp";
	if (gd->ogd.board_desc.board_type == CVMX_BOARD_TYPE_UBNT_E200) {
		if (gd->ogd.board_desc.rev_major == BOARD_E201_MAJOR) {
			key = "0,nosfp";
		}
	}
	octeon_fdt_patch(working_fdt, key, NULL);
	return 0;
}

void octeon_fixup_fdt(void)
{
	const char *p;
	int pip, interface, ethernet;
	int i, e;
	uint64_t mac;
	char name[20];

	__octeon_fixup_fdt();

	if (!working_fdt)
		return;

	mac = (gd->ogd.mac_desc.mac_addr_base[5] & 0xffull) |
		((gd->ogd.mac_desc.mac_addr_base[4] & 0xffull) << 8) |
		((gd->ogd.mac_desc.mac_addr_base[3] & 0xffull) << 16) |
		((gd->ogd.mac_desc.mac_addr_base[2] & 0xffull) << 24) |
		((gd->ogd.mac_desc.mac_addr_base[1] & 0xffull) << 32) |
		((gd->ogd.mac_desc.mac_addr_base[0] & 0xffull) << 40);

	p = fdt_get_alias(working_fdt, "pip");
	if (!p)
		return;

	pip = fdt_path_offset(working_fdt, p);
	if (pip <= 0)
		return;

	for (i = 1; i >= 0; i--) {
		sprintf(name, "interface@%d", i);
		interface = fdt_subnode_offset(working_fdt, pip, name);
		if (interface <= 0)
			continue;
		for (e = 0; e < 4; e++) {
			sprintf(name, "ethernet@%d", e);
			ethernet = fdt_subnode_offset(working_fdt, interface,
						      name);
			if (ethernet <= 0)
				break;
			octeon_set_one_fdt_mac(ethernet, &mac);
		}
	}
}

void board_mdio_init(void)
{
}

int checkboard(void)
{
	cvmx_mpi_cfg_t mpi_cfg;
	mpi_cfg.u64 = 0;
	mpi_cfg.cn61xx.clkdiv = 32;
	mpi_cfg.cn61xx.csena0 = 1;
	mpi_cfg.cn61xx.clk_cont = 0;
	mpi_cfg.cn61xx.enable = 1;
	cvmx_write_csr(CVMX_MPI_CFG, mpi_cfg.u64);

	cvmx_write_csr(CVMX_SMIX_EN(0), 1);
	cvmx_write_csr(CVMX_SMIX_EN(1), 1);

	return 0;
}

static void *translate_flash_addr(void *base)
{
	void *ptr = base;

	if (!(gd->flags & GD_FLG_RAM_RESIDENT)) {
		cvmx_mio_boot_reg_cfgx_t __attribute__((unused)) reg_cfg;
		uint32_t normal_base = ((CONFIG_SYS_FLASH_BASE >> 16)
		                        & 0x1fff);
		uint32_t offset;

		reg_cfg.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFG0);
		offset = (normal_base - reg_cfg.s.base) << 16;
		ptr = (void *)((char *) base - offset);
	}

	return ptr;
}

static void *get_ubnt_gd_addr(void)
{
	return translate_flash_addr(CONFIG_UBNT_GD_ADDR);
}

static void *get_ubnt_eeprom_addr(void)
{
	return translate_flash_addr(CONFIG_UBNT_EEPROM_ADDR);
}

#define UBNT_MAX_DESC_LEN 64

static int validate_and_fix_gd_entry(octeon_eeprom_header_t *hdr)
{
	int i;
	uint16_t csum = 0;
	uint8_t *p = (uint8_t *) hdr;

	be16_to_cpus(hdr->type);
	switch (hdr->type) {
	case EEPROM_MAC_ADDR_TYPE:
		break;
	case EEPROM_BOARD_DESC_TYPE:
		{
			octeon_eeprom_board_desc_t *bd
				= (octeon_eeprom_board_desc_t *) hdr;
			be16_to_cpus(bd->board_type);
		}
		break;
	default:
		return 0;
	}

	be16_to_cpus(hdr->length);
	if (hdr->length > UBNT_MAX_DESC_LEN)
		return 0;

	be16_to_cpus(hdr->checksum);
	for (i = 0; i < hdr->length; i++)
		csum += p[i];

	csum -= (hdr->checksum & 0xff);
	csum -= (hdr->checksum >> 8);
	if (csum != hdr->checksum)
		return 0;

	return 1;
}

void print_mpr()
{
	void *eptr = get_ubnt_eeprom_addr();
	uint8_t edata[32];
	uint32_t mpt;

	memcpy(edata, eptr, 32);
	mpt = (edata[16] << 16) + (edata[17] << 8) + edata[18];
	printf("MPR 13-%05u-%02u\n", mpt, edata[19]);
}

int early_board_init(void)
{
	void *gdptr = get_ubnt_gd_addr();

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

	memcpy((void *)&(gd->ogd.mac_desc),
	       gdptr + CONFIG_UBNT_GD_MAC_DESC_OFF,
	       sizeof(octeon_eeprom_mac_addr_t));
	memcpy((void *)&(gd->ogd.board_desc),
	       gdptr + CONFIG_UBNT_GD_BOARD_DESC_OFF,
	       sizeof(octeon_eeprom_board_desc_t));
	memset((void *)&(gd->ogd.clock_desc), 0x0,
	       sizeof(octeon_eeprom_clock_desc_t));

	if (!validate_and_fix_gd_entry(&(gd->ogd.mac_desc.header))
	    || gd->ogd.mac_desc.header.type != EEPROM_MAC_ADDR_TYPE) {
		gd->ogd.mac_desc.count = 8;
		gd->ogd.mac_desc.mac_addr_base[0] = 0x00;
		gd->ogd.mac_desc.mac_addr_base[1] = 0xbe;
		gd->ogd.mac_desc.mac_addr_base[2] = 0xef;
		gd->ogd.mac_desc.mac_addr_base[3] = 0x10;
		gd->ogd.mac_desc.mac_addr_base[4] = 0x00;
		gd->ogd.mac_desc.mac_addr_base[5] = 0x00;
	}

	if (!validate_and_fix_gd_entry(&(gd->ogd.board_desc.header))
	    || gd->ogd.board_desc.header.type != EEPROM_BOARD_DESC_TYPE) {
		uint32_t g = cvmx_gpio_read();
		strncpy((char *)(gd->ogd.board_desc.serial_str),
		        "fffffffffffffffff", SERIAL_LEN);
		gd->ogd.board_desc.rev_minor = 0xff;
		gd->ogd.board_desc.board_type = CVMX_BOARD_TYPE_UBNT_E200;
		switch ((g >> 17) & 0x7) {
		case 1:
			gd->ogd.board_desc.rev_major = BOARD_E200_MAJOR;
			break;
		case 2:
			gd->ogd.board_desc.rev_major = BOARD_E201_MAJOR;
			break;
		case 4:
			gd->ogd.board_desc.rev_major = BOARD_E202_MAJOR;
			break;
		default:
			gd->ogd.board_desc.rev_major = 0xff;
			break;
		}
	}

	gd->ogd.ddr_clock_mhz = UBNT_E200_DEF_DRAM_FREQ;
	gd->ogd.ddr_ref_hertz = 50000000;
	gd->ogd.cpu_clock_mhz = octeon_get_cpu_multiplier()
	                        * DEFAULT_CPU_REF_FREQUENCY_MHZ;

	{
		cvmx_mio_qlmx_cfg_t mio_qlmx;
		mio_qlmx.u64 = 0;
		mio_qlmx.cn61xx.qlm_cfg = 0x2;
		mio_qlmx.cn61xx.qlm_spd = 0x3;
		cvmx_write_csr(CVMX_MIO_QLMX_CFG(2), mio_qlmx.u64);
		cvmx_write_csr(CVMX_MIO_QLMX_CFG(0), mio_qlmx.u64);
	}

	return 0;
}


/****** ubntw ******/

static void fill_checksum(octeon_eeprom_header_t *hdr)
{
	int i;
	uint16_t csum = 0;
	uint8_t *p = (uint8_t *) hdr;
	unsigned int len = ntohs(hdr->length);

	for (i = 0; i < len; i++)
		csum += p[i];

	hdr->checksum = htons(csum);
}

static int write_ubnt_gd_entries(octeon_eeprom_header_t **hdr)
{
	int i, r, ret = 0;
	unsigned int gdoff;
	void *gdptr = get_ubnt_gd_addr();
	void *buf;

	if (!hdr) {
		printf("Invalid GD entry\n");
		return 1;
	}

	if (!(buf = (void *) malloc(CONFIG_UBNT_GD_SIZE))) {
		printf("Failed to allocate memory\n");
		return 1;
	}

	memcpy(buf, gdptr, CONFIG_UBNT_GD_SIZE);
	for (i = 0; hdr[i]; i++) {
		unsigned int len = ntohs(hdr[i]->length);

		fill_checksum(hdr[i]);
		switch (ntohs(hdr[i]->type)) {
		case EEPROM_MAC_ADDR_TYPE:
			gdoff = CONFIG_UBNT_GD_MAC_DESC_OFF;
			break;
		case EEPROM_BOARD_DESC_TYPE:
			gdoff = CONFIG_UBNT_GD_BOARD_DESC_OFF;
			break;
		default:
			printf("Unsupported type\n");
			ret = 1;
			goto out;
		}

		memcpy(buf + gdoff, hdr[i], len);
	}

	if ((r = flash_sect_protect(0, gdptr,
	                            gdptr + CONFIG_UBNT_GD_SIZE - 1))) {
		flash_perror(r);
		ret = 1;
		goto out;
	}
	if ((r = flash_sect_erase(gdptr, gdptr + CONFIG_UBNT_GD_SIZE - 1))) {
		flash_perror(r);
		ret = 1;
		goto out;
	}
	if ((r = flash_write(buf, gdptr, CONFIG_UBNT_GD_SIZE))) {
		flash_perror(r);
		ret = 1;
		goto out;
	}

out:
	free(buf);
	printf("\n");
	return ret;
}

struct board_type_info {
	const char *type;
	uint8_t major;
	uint8_t dr_id[4];
};

static struct board_type_info board_types[] = {
	{"e200", BOARD_E200_MAJOR, {0xee, 0x11, 0x07, 0x77}},
	{"e201", BOARD_E201_MAJOR, {0xee, 0x12, 0x07, 0x77}},
	{"e202", BOARD_E202_MAJOR, {0xee, 0x13, 0x07, 0x77}},
	{NULL, 0xff, {0xff, 0xff, 0xff, 0xff}}
};

static struct board_type_info *find_board_type(const char *btype)
{
	int i;

	for (i = 0; board_types[i].type; i++)
		if (strcmp(btype, board_types[i].type) == 0)
			return &(board_types[i]);

	return NULL;
}

static int write_ubnt_eeprom(struct board_type_info *binfo,
			     octeon_eeprom_mac_addr_t *mac,
			     uint8_t *mpr)
{
	int i, r;
	uint8_t data[32];
	uint8_t *d = binfo->dr_id;
	void *eptr = get_ubnt_eeprom_addr();

	memset(data, 0, 32);
	for (i = 0; i < 6; i++) {
		data[i] = mac->mac_addr_base[i];
		data[i + 6] = mac->mac_addr_base[i];
	}
	data[6] |= 0x02;
	for (i = 0; i < 4; i++) {
		data[(12 + i)] = d[i];
		data[(16 + i)] = mpr[i];
	}

	if ((r = flash_sect_protect(0, eptr,
	                            eptr + CONFIG_UBNT_EEPROM_SIZE - 1))) {
		flash_perror(r);
		return 1;
	}
	if ((r = flash_sect_erase(eptr, eptr + CONFIG_UBNT_EEPROM_SIZE - 1))) {
		flash_perror(r);
		return 1;
	}
	if ((r = flash_write(data, eptr, 32))) {
		flash_perror(r);
		return 1;
	}
	return 0;
}

static void fill_mac_desc(octeon_eeprom_mac_addr_t *mac, const char *mstr,
			  const char *mcount)
{
	int i;
	uint64_t maddr;

	mac->header.type = htons(EEPROM_MAC_ADDR_TYPE);
	mac->header.length = htons(sizeof(octeon_eeprom_mac_addr_t));
	mac->header.minor_version = 0;
	mac->header.major_version = 1;
	mac->header.checksum = 0;

	maddr = simple_strtoull(mstr, NULL, 16);
	cpu_to_be64s(maddr);
	mac->count = simple_strtoul(mcount, NULL, 10);
	for (i = 0; i < 6; i++) {
		mac->mac_addr_base[i] = (maddr >> ((5 - i) * 8)) & 0xff;
	}
}

static void fill_board_desc(octeon_eeprom_board_desc_t *bd,
			    struct board_type_info *binfo,
			    const char *b_minor, const char *serial)
{
	bd->header.type = htons(EEPROM_BOARD_DESC_TYPE);
	bd->header.length = htons(sizeof(octeon_eeprom_board_desc_t));
	bd->header.minor_version = 0;
	bd->header.major_version = 1;
	bd->header.checksum = 0;

	bd->board_type = htons(CVMX_BOARD_TYPE_UBNT_E200);
	bd->rev_major = binfo->major;
	bd->rev_minor = simple_strtoul(b_minor, NULL, 10);
	strncpy((char *) (bd->serial_str), serial, SERIAL_LEN);
	(bd->serial_str)[SERIAL_LEN - 1] = 0;
}

static void show_mac_desc(octeon_eeprom_mac_addr_t *mac)
{
	if (!mac) {
		return;
	}

	printf("MAC base: %02x:%02x:%02x:%02x:%02x:%02x, count: %u\n",
	       mac->mac_addr_base[0], mac->mac_addr_base[1],
	       mac->mac_addr_base[2], mac->mac_addr_base[3],
	       mac->mac_addr_base[4], mac->mac_addr_base[5], mac->count);
}

static void show_board_desc(octeon_eeprom_board_desc_t *bd)
{
	if (!bd) {
		return;
	}

	printf("Board type: %s (%u.%u)\nSerial number: %s\n",
	       cvmx_board_type_to_string(ntohs(bd->board_type)),
	       bd->rev_major, bd->rev_minor, bd->serial_str);
}

int do_ubntw(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char op;

	if (!argv[1]) {
		printf("Operation required\n");
		return 1;
	}

	op = argv[1][0];
	if (op == 'a') {
		octeon_eeprom_mac_addr_t mac;
		octeon_eeprom_board_desc_t bd;
		octeon_eeprom_header_t *hdrs[] = {
			((octeon_eeprom_header_t *) &mac),
			((octeon_eeprom_header_t *) &bd),
			NULL
		};
		struct board_type_info *binfo;
		uint8_t mpr[4] = {0xff, 0xff, 0xff, 0xff};

		if (argc < 7) {
			printf("Required arguments missing\n");
			return 1;
		}

		if (!(binfo = find_board_type(argv[4]))) {
			printf("Invalid type\n");
			return 1;
		}

		fill_mac_desc(&mac, argv[2], argv[3]);
		fill_board_desc(&bd, binfo, argv[5], argv[6]);

		mpr[3] = simple_strtoul(argv[5], NULL, 10);
		if (argc == 8) {
			uint32_t pt = simple_strtoul(argv[7], NULL, 10);
			mpr[0] = (pt >> 16) & 0xff;
			mpr[1] = (pt >> 8) & 0xff;
			mpr[2] = pt & 0xff;
		}

		if (write_ubnt_gd_entries(hdrs) != 0) {
			printf("Write data failed\n");
			return 1;
		}
		if (write_ubnt_eeprom(binfo, &mac, mpr) != 0) {
			printf("Write failed\n");
			return 1;
		}

		show_mac_desc(&mac);
		show_board_desc(&bd);
		return 0;
	}

	printf("Invalid operation\n");
	return 1;
}

U_BOOT_CMD(ubntw, 8, 1, do_ubntw,
	"ubntw      - ubntw command\n",
	"ubntw all <mac> <count> <type> <minor> <serial> [<pt>] - write all\n");

static void _set_led(int l)
{
	int mbus = 0;
	int maddr = 7;
	uint16_t val = 0;
	if (gd->ogd.board_desc.board_type == CVMX_BOARD_TYPE_UBNT_E200
	    && (gd->ogd.board_desc.rev_major == BOARD_E200_MAJOR
		|| gd->ogd.board_desc.rev_major == BOARD_E202_MAJOR)) {
		mbus = 1;
		maddr = 1;
	}
	switch (l) {
	case 0:
		val = 0x10;
		break;
	case 1:
		val = 0x8;
		break;
	default:
		break;
	}
	cvmx_mdio_write(mbus, maddr, 0x10, val);
}

void board_blink_led(int ms)
{
	int i;
	for (i = 0; i < (ms / 125); i++) {
		_set_led(i % 2);
		udelay(125000);
	}
}

void board_set_led_on()
{
	_set_led(0);
}

void board_set_led_off()
{
	_set_led(1);
}

void board_set_led_normal()
{
	_set_led(2);
}
