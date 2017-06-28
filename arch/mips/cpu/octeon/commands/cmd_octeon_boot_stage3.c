/***********************license start************************************
 * Copyright (c) 2011  Cavium Inc. (support@cavium.com).
 * All rights reserved.
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
 ***********************license end**************************************/

#include <common.h>
#include <command.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-gpio.h>
#include <ata.h>
#include <part.h>
#include <fat.h>
#include <../disk/part_dos.h>

extern unsigned long do_go_exec(ulong (*entry)(int, char * const []), int argc,
				char * const argv[]);

/**
 * Search for a bootable FAT partition
 */
static int find_bootable_fat_partition(block_dev_desc_t *dev_desc)
{
	dos_partition_t *pt;
	int i;

	ALLOC_CACHE_ALIGN_BUFFER(uint8_t, buffer, dev_desc->blksz);

	if ((dev_desc->block_read(dev_desc->dev, 0, 1, (ulong *)buffer) != 1) ||
	    (buffer[DOS_PART_MAGIC_OFFSET + 0] != 0x55) ||
	    (buffer[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) ) {
		printf("Could not read DOS partition table\n");
		return -1;
	}

	pt = (dos_partition_t *)(buffer + DOS_PART_TBL_OFFSET);

	for (i = 0; i < 4; i++) {
		if (pt->boot_ind != 0x80)
			continue;
		switch (pt->sys_ind) {
		case 0x4:		/* FAT16 < 32M */
		case 0x6:		/* FAT16 */
		case 0x14:		/* FAT16 < 32M */
		case 0x16:		/* FAT16 */
		case 0xb:		/* FAT32 */
		case 0xc:		/* FAT32 */
		case 0xe:		/* LBA FAT16 */
		case 0x1b:		/* FAT32 */
		case 0x1c:		/* LBA FAT32 */
		case 0x1e:		/* LBA FAT16 */
			return i+1;
		default:
			break;
		}
	}
	return -1;
}

int do_octbootstage3(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	uint32_t addr, rc;
	int rcode = 0;
	char *filename;
	char *failsafe_filename;
	const char *dev_name;
	int failsafe;
	int size = 0;
	int max_size = 0;
	int part_no;
	int dev_no = 0;
	block_dev_desc_t *dev_desc = NULL;

	filename = getenv("octeon_stage3_bootloader");
	addr = getenv_ulong("octeon_stage3_load_addr", 16,
			    CONFIG_OCTEON_STAGE3_LOAD_ADDR);
	max_size = getenv_ulong("octeon_stage3_max_size", 0,
				CONFIG_OCTEON_STAGE3_MAX_SIZE);
	dev_no = getenv_ulong("octeon_stage3_devno", 0,
			   CONFIG_OCTEON_STAGE3_DEVNO);
	dev_name = getenv("octeon_stage3_devname");

	if (!dev_name)
		dev_name = CONFIG_OCTEON_STAGE3_DEVNAME;

	dev_desc = get_dev(dev_name, dev_no);
	if (!dev_desc) {
		printf("Could not find device %s %d\n", dev_name, dev_no);
		return -1;
	}

	part_no = find_bootable_fat_partition(dev_desc);
	if (part_no < 0) {
		printf("No bootable FAT partition found\n");
		return -1;
	}

	if (fat_register_device(dev_desc, part_no) != 0) {
		printf("Unable to use %s %d:%d for FAT partition\n", dev_name,
		       dev_no, part_no);
	}

#ifndef CONFIG_OCTEON_NO_STAGE3_FAILSAFE
	failsafe = !!(cvmx_gpio_read() & (1 << CONFIG_OCTEON_FAILSAFE_GPIO));

	if (!filename) {
		failsafe = 1;
	}

	if (!failsafe) {
		size = file_fat_read(filename, (unsigned char *)addr, max_size);
		if (size <= 0) {
			printf("Could not read %s, trying failsafe\n", filename);
			goto failsafe;
		}
		do_go_exec((void *)addr, argc - 1, argv + 1);
	}

failsafe:
	failsafe_filename = getenv("octeon_stage3_failsafe_bootloader");
	if (!failsafe_filename) {
		failsafe_filename = CONFIG_OCTEON_STAGE3_FAILSAFE_FILENAME;
		printf("Error: environment variable octeon_stage3_failsafe_bootloader is not set.\n");
		return 0;
	}
	if (failsafe_filename) {
		size = file_fat_read(filename, (unsigned char *)addr,
				     max_size);
		if (size <= 0) {
			printf("Could not read failsafe bootloader %s, "
			       "trying %s\n", failsafe_filename,
			       CONFIG_OCTEON_STAGE3_FAILSAFE_FILENAME);
			failsafe_filename = CONFIG_OCTEON_STAGE3_FAILSAFE_FILENAME;
		}
	} else {
		printf("No failsafe available!\n");
		return -1;
	}
#else
	if (!filename)
		filename = CONFIG_OCTEON_STAGE3_FILENAME;

	size = file_fat_read(filename, (unsigned char *)addr, max_size);
	if (size <= 0) {
		printf("Could not open stage 3 bootloader %s\n", filename);
		return -1;
	}
	do_go_exec((void *)addr, argc - 1, argv + 1);
#endif
	return -1;
}

U_BOOT_CMD(bootstage3, 1, 0, do_octbootstage3,
	   "Load and execute the stage 3 bootloader",
	   "Load and execute the stage 3 bootloader");