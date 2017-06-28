/***********************license start************************************
 * Copyright (c) 2008-2011  Cavium Inc. (support@cavium.com).
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

#ifdef __U_BOOT__
#include <config.h>
#endif
#include <common.h>
#include <command.h>
#include <linux/stddef.h>

#ifdef CONFIG_SYS_CMD_OCTEON_BOOTLOADER_UPDATE

#include <watchdog.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <asm/addrspace.h>
#include <octeon_boot.h>
#include <octeon_nand.h>
#endif

#include "cvmx.h"
#include "cvmx-nand.h"
#include "cvmx-bootloader.h"

/**
 * Helpers
 */

uint32_t calculate_header_crc(const bootloader_header_t * header);
uint32_t calculate_image_crc(const bootloader_header_t * header);

#ifdef CONFIG_SYS_CMD_OCTEON_BOOTLOADER_UPDATE
uint32_t get_image_size(const bootloader_header_t * header);
int validate_header(const bootloader_header_t * header);
int validate_data(const bootloader_header_t * header);

#if (!defined(CONFIG_SYS_NO_FLASH))
extern flash_info_t flash_info[];
#endif

//#define DEBUG

#ifdef DEBUG
#define DBGUPD printf
#else
#define DBGUPD(_args...)
#endif

/* This needs to be overridden for some boards, but this is correct for most.
 * This is only used for dealing with headerless images (and headerless failsafe
 * images currently on boards.)
 */
#ifndef CONFIG_SYS_NORMAL_BOOTLOADER_BASE
#define CONFIG_SYS_NORMAL_BOOTLOADER_BASE 0x1fd00000
#endif
/**
 * Prepares the update of the bootloader in NOR flash.  Performs any
 * NOR specific checks (block alignment, etc). Patches up the image for 
 * relocation and prepares the environment for bootloader_flash_update command.
 *
 * @param image_addr Address of image in DRAM.  May not have a bootloader header
 *                   in it.
 * @param length     Length of image in bytes if the image does not have a header.
 *                   0 if image has a hader.
 * @param burn_addr  Address in flash (not remapped) to programm the image to
 * @param failsafe   Flag to allow failsafe image burning.
 *
 * @return 0 on success
 *         1 on failure
 */
#define ERR_ON_OLD_BASE
int do_bootloader_update_nor(uint32_t image_addr, int length,
			     uint32_t burn_addr, int failsafe)
{
#if defined(CONFIG_SYS_NO_FLASH)
	printf("ERROR: Bootloader not compiled with NOR flash support\n");
	return 1;
#else
	uint32_t failsafe_size, failsafe_top_remapped;
	uint32_t burn_addr_remapped, image_size, normal_top_remapped;
	flash_info_t *info;
	char tmp[16] __attribute__ ((unused));	/* to hold 32 bit numbers in hex */
	int sector = 0;
	bootloader_header_t *header;
	header = cvmx_phys_to_ptr(image_addr);

	extern flash_info_t flash_info[];

	DBGUPD("LOOKUP_STEP                0x%x\n", LOOKUP_STEP);
	DBGUPD("CFG_FLASH_BASE             0x%x\n", CONFIG_SYS_FLASH_BASE);

	/* File with rev 1.1 headers are not relocatable, so _must_ be burned
	 * at the address that they are linked at.
	 */
	if (header->maj_rev == 1 && header->min_rev == 1) {
		if (burn_addr && burn_addr != header->address) {
			printf("ERROR: specified address (0x%x) does not match "
			       "required burn address (0x%llx\n)\n",
			       burn_addr, header->address);
			return 1;
		}
		burn_addr = header->address;
	}

	/* If we have at least one bank of non-zero size, we have some NOR */
	if (!flash_info[0].size) {
		printf("ERROR: No NOR Flash detected on board, can't burn NOR "
		       "bootloader image\n");
		return 1;
	}

	/* check the burn address allignement */
	if ((burn_addr & (LOOKUP_STEP - 1)) != 0) {
		printf("Cannot programm normal image at 0x%x: address must be\n"
		       " 0x%x bytes alligned for normal boot lookup\n",
		       burn_addr, LOOKUP_STEP);
		return 1;
	}

	/* for failsage checks are easy */
	if ((failsafe) && (burn_addr != FAILSAFE_BASE)) {
		printf("ERROR: Failsafe image must be burned to address 0x%x\n",
		       FAILSAFE_BASE);
		return 1;
	}

	if (burn_addr && (burn_addr < FAILSAFE_BASE)) {
		printf("ERROR: burn address 0x%x out of boot range\n",
		       burn_addr);
		return 1;
	}

	if (!failsafe) {
#ifndef CONFIG_OCTEON_NO_FAILSAFE
		/* find out where failsafe ends */
		failsafe_size =
		    get_image_size((bootloader_header_t *)
				   CONFIG_SYS_FLASH_BASE);

		if (failsafe_size == 0) {
			/* failsafe does not have header - assume fixed size
			 * old image
			 */
			printf("Failsafe has no valid header, assuming old image. "
			       "Using default failsafe size\n");
			failsafe_size =
			    CONFIG_SYS_NORMAL_BOOTLOADER_BASE - FAILSAFE_BASE;

			/* must default to CONFIG_SYS_NORMAL_BOOTLOADER_BASE */
			if (!burn_addr)
				burn_addr = CONFIG_SYS_NORMAL_BOOTLOADER_BASE;
			else if (CONFIG_SYS_NORMAL_BOOTLOADER_BASE != burn_addr) {
				printf("WARNING: old failsafe image will not be able to start\n"
				       "image at any address but 0x%x\n",
				       CONFIG_SYS_NORMAL_BOOTLOADER_BASE);
#ifdef ERR_ON_OLD_BASE
				return 1;
#endif
			}
		}		/* old failsafe */
#else
		failsafe_size = 0;
#endif		/* CONFIG_OCTEON_NO_FAILSAFE */

		DBGUPD("failsafe size is 0x%x\n", failsafe_size);
		/* Locate the next flash sector */
		failsafe_top_remapped = CONFIG_SYS_FLASH_BASE + failsafe_size;
		DBGUPD("failsafe_top_remapped 0x%x\n", failsafe_top_remapped);
		info = &flash_info[0];	/* no need to look into any other banks */
		/* scan flash bank sectors */
		for (sector = 0; sector < info->sector_count; ++sector) {
			DBGUPD("%d: 0x%x\n", sector, info->start[sector]);
			if (failsafe_top_remapped <= info->start[sector])
				break;
		}

		if (sector == info->sector_count) {
			printf("Failsafe takes all the flash?? Can not burn normal image\n");
			return 1;
		}

		/* Move failsafe top up to the sector boundary */
		failsafe_top_remapped = info->start[sector];

		DBGUPD("Found next sector after failsafe is at remapped addr 0x%x\n",
		       failsafe_top_remapped);
		failsafe_size = failsafe_top_remapped - CONFIG_SYS_FLASH_BASE;
		DBGUPD("Alligned up failsafe size is 0x%x\n", failsafe_size);

		/* default to the first sector after the failsafe */
		if (!burn_addr)
			burn_addr = FAILSAFE_BASE + failsafe_size;
		/* check for overlap */
		else if (FAILSAFE_BASE + failsafe_size > burn_addr) {
			printf("ERROR: can not burn: image overlaps with failsafe\n");
			printf("burn address is 0x%x, in-flash failsafe top is 0x%x\n",
			       burn_addr, FAILSAFE_BASE + failsafe_size);
			return 1;
		}
		/* done with failsafe checks */
	}

	if (length)
		image_size = length;
	else
		image_size = get_image_size((bootloader_header_t *) image_addr);
	if (!image_size) {
		/* this is wierd case. Should never happen with good image */
		printf("ERROR: image has size field set to 0??\n");
		return 1;
	}

	/* finally check the burn address' CKSSEG limit */
	if ((burn_addr + image_size) >= (uint64_t) CKSSEG) {
		printf("ERROR: can not burn: image exceeds KSEG1 area\n");
		printf("burnadr is 0x%x, top is 0x%x\n", burn_addr,
		       burn_addr + image_size);
		return 1;
	}

	/* Look up the last sector to use by the new image */
	burn_addr_remapped = burn_addr - FAILSAFE_BASE + CONFIG_SYS_FLASH_BASE;
	DBGUPD("burn_addr_remapped 0x%x\n", burn_addr_remapped);
	normal_top_remapped = burn_addr_remapped + image_size;
	/* continue flash scan - now for normal image top */
	if (failsafe)
		sector = 0;	/* is failsafe, we start from first sector here */
	for (; sector < info->sector_count; ++sector) {
		DBGUPD("%d: 0x%x\n", sector, info->start[sector]);
		if (normal_top_remapped <= info->start[sector])
			break;
	}
	if (sector == info->sector_count) {
		printf("ERROR: not enough room in flash bank for the image??\n");
		return 1;
	}
	/* align up for environment variable set up */
	normal_top_remapped = info->start[sector];

	DBGUPD("normal_top_remapped 0x%x\n", normal_top_remapped);
	/* if there is no header (length != 0) - check burn address and
	 * give warning
	 */
	if (length && CONFIG_SYS_NORMAL_BOOTLOADER_BASE != burn_addr) {
#ifdef ERR_ON_OLD_BASE
		printf("ERROR: burning headerless image at other that defailt address\n"
		       "Image look up will not work.\n");
		printf("Default burn address: 0x%x requested burn address: 0x%x\n",
		       CONFIG_SYS_NORMAL_BOOTLOADER_BASE, burn_addr);
		return 1;
#else
		printf("WARNING: burning headerless image at other that defailt address\n"
		       "Image look up will not work.\n");
		printf
		    ("Default burn address: 0x%x requested burn address: 0x%x\n",
		     CONFIG_SYS_NORMAL_BOOTLOADER_BASE, burn_addr);
#endif
	}

	printf("Image at 0x%x is ready for burning\n", image_addr);
	printf("           Header version: %d.%d\n", header->maj_rev,
	       header->min_rev);
	printf("           Header size %d, data size %d\n", header->hlen,
	       header->dlen);
	printf("           Header crc 0x%x, data crc 0x%x\n", header->hcrc,
	       header->dcrc);
	printf("           Image link address is 0x%llx\n", header->address);
	printf("           Image burn address on flash is 0x%x\n", burn_addr);
	printf("           Image size on flash 0x%x\n",
	       normal_top_remapped - burn_addr_remapped);

	DBGUPD("burn_addr_remapped 0x%x normal_top_remapped 0x%x\n",
	       burn_addr_remapped, normal_top_remapped);
	if (flash_sect_protect(0, burn_addr_remapped, normal_top_remapped - 1)) {
		printf("Flash unprotect failed\n");
		return 1;
	}
	if (flash_sect_erase(burn_addr_remapped, normal_top_remapped - 1)) {
		printf("Flash erase failed\n");
		return 1;
	}

	int rc;
	puts("Copy to Flash... ");
	/* Note: Here we copy more than we should - whatever is after the image
	 * in memory gets copied to flash.
	 */
	rc = flash_write((char *)image_addr, burn_addr_remapped,
			 normal_top_remapped - burn_addr_remapped);
	if (rc != 0) {
		flash_perror(rc);
		return 1;
	}
	puts("done\n");

#ifndef CONFIG_ENV_IS_IN_NAND
	/* Erase the environment so that older bootloader will use its default
	 * environment.  This will ensure that the default 
	 * 'bootloader_flash_update' macro is there.  HOWEVER, this is only
	 * useful if a legacy sized failsafe u-boot image is present.
	 * If a new larger failsafe is present, then that macro will be incorrect
	 * and will erase part of the failsafe.
	 * The 1.9.0 u-boot needs to have its link address and
	 * normal_bootloader_size/base modified to work with this...
	 */
	if (header->maj_rev == 1 && header->min_rev == 1) {
		printf("Erasing environment due to u-boot downgrade.\n");
		flash_sect_protect(0, CONFIG_ENV_ADDR,
				   CONFIG_ENV_ADDR + CONFIG_ENV_SIZE - 1);
		if (flash_sect_erase
		    (CONFIG_ENV_ADDR, CONFIG_ENV_ADDR + CONFIG_ENV_SIZE - 1)) {
			printf("Environment erase failed\n");
			return 1;
		}

	}
#endif
	return 0;
#endif
}

#ifdef CONFIG_CMD_OCTEON_NAND

/**
 * NAND specific update routine.  Handles erasing the previous
 * image if it exists.
 *
 * @param image_addr Address of image in DRAM.  Always
 *                   has an image header.
 *
 * @return 0 on success
 *         1 on failure
 */
int do_bootloader_update_nand(uint32_t image_addr)
{
	const bootloader_header_t *new_header;
	const bootloader_header_t *header;
	new_header = (void *)image_addr;
	int chip = oct_nand_get_cur_chip();
	int page_size = cvmx_nand_get_page_size(chip);
	int oob_size = octeon_nand_get_oob_size(chip);
	int pages_per_block = cvmx_nand_get_pages_per_block(chip);
	int bytes;

	uint64_t block_size = page_size * pages_per_block;

	uint64_t nand_addr = block_size;
	uint64_t buf_storage[2200 / 8] = { 0 };
	unsigned char *buf = (unsigned char *)buf_storage;
	int read_size = CVMX_NAND_BOOT_ECC_BLOCK_SIZE + 8;
	header = (void *)buf;
	uint64_t old_image_nand_addr = 0;
	int old_image_size = 0;

	if (!cvmx_nand_get_active_chips()) {
		printf("ERROR: No NAND Flash detected on board, can't burn "
		       "NAND bootloader image\n");
		return 1;
	}

	/* Find matching type (failsafe/normal, stage2/stage3) of image that
	 * is currently in NAND, if present.  Save location for later erasing
	 */
	while ((nand_addr =
		oct_nand_image_search(nand_addr, MAX_NAND_SEARCH_ADDR,
				      new_header->image_type))) {
		/* Read new header */
		bytes =
		    cvmx_nand_page_read(chip, nand_addr, cvmx_ptr_to_phys(buf),
					read_size);
		if (bytes != read_size) {
			printf("Error reading NAND addr 0x%llx (bytes_read: %d, expected: %d)\n",
			       nand_addr, bytes, read_size);
			return 1;
		}
		/* Check a few more fields from the headers */

		if (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_GENERIC
		    && header->board_type != CVMX_BOARD_TYPE_GENERIC) {
			/* If the board type of the running image is generic,
			 * don't do any board matching.  When looking for images
			 * in NAND to overwrite, treat generic board type images
			 * as matching all board types.
			 */
			if (new_header->board_type != header->board_type) {
				printf("WARNING: A bootloader for a different "
				       "board type was found and skipped (not erased.)\n");
				/* Different board type, so skip (this is
				 * strange to find.....
				 */
				nand_addr +=
				    ((header->hlen + header->dlen + page_size - 1)
				    & ~(page_size - 1));
				continue;
			}
		}
		if ((new_header->flags & BL_HEADER_FLAG_FAILSAFE) !=
		    (new_header->flags & BL_HEADER_FLAG_FAILSAFE)) {
			/* Not a match, so skip */
			nand_addr +=
			    ((header->hlen + header->dlen + page_size -
			      1) & ~(page_size - 1));
			continue;
		}

		/* A match, so break out */
		old_image_nand_addr = nand_addr;
		old_image_size = header->hlen + header->dlen;
		printf("Found existing bootloader image of same type at NAND addr: 0x%llx\n",
		       old_image_nand_addr);
		break;
	}
	/* nand_addr is either 0 (no image found), or has the address of the
	 * image we will delete after the write of the new image.
	 */
	if (!nand_addr)
		printf("No existing matching bootloader found in flash\n");

	/* Find a blank set of _blocks_ to put the new image in.  We want
	 * to make sure that we don't put any part of it in a block with
	 * something else, as we want to be able to erase it later.
	 */
	int required_len = new_header->hlen + new_header->dlen;
	int required_blocks = (required_len + block_size - 1) / block_size;

	int conseq_blank_blocks = 0;
	read_size = page_size + oob_size;
	for (nand_addr = block_size; nand_addr < MAX_NAND_SEARCH_ADDR;
	     nand_addr += block_size) {
		if (oct_nand_block_is_blank(nand_addr)) {
			conseq_blank_blocks++;
			if (conseq_blank_blocks == required_blocks) {
				/* We have a large enough blank spot */
				nand_addr -=
				    (conseq_blank_blocks - 1) * block_size;
				break;
			}
		} else
			conseq_blank_blocks = 0;
	}

	if (nand_addr >= MAX_NAND_SEARCH_ADDR) {
		printf("ERROR: unable to find blank space for new bootloader\n");
		return 1;
	}
	printf("New bootloader image will be written at blank address 0x%llx, length 0x%x\n",
	       nand_addr, required_len);

	/* Write the new bootloader to blank location. */
	if (0 > oct_nand_boot_write(nand_addr, (void *)image_addr, required_len, 0)) {
		printf("ERROR: error while writing new image to flash.\n");
		return 1;
	}

	/* Now erase the old bootloader of the same type.
	 * We know these are not bad NAND blocks since they have valid data
	 * in them.
	 */
	uint64_t erase_base = old_image_nand_addr & ~(block_size - 1);
	uint64_t erase_end =
	    ((old_image_nand_addr + old_image_size + block_size -
	      1) & ~(block_size - 1));
	for (nand_addr = erase_base; nand_addr < erase_end;
	     nand_addr += block_size) {
		if (cvmx_nand_block_erase(chip, nand_addr)) {
			printf("cvmx_nand_block_erase() failed, addr 0x%08llx\n",
			       nand_addr);
			return 1;
		}

	}

	printf("Bootloader update in NAND complete.\n");
	return 0;
}
#endif		/* CONFIG_CMD_OCTEON_NAND */

/**
 * Command for updating a bootloader image in flash.  This function
 * parses the arguments, and validates the header (if header exists.)
 * Actual flash updating is done by flash type specific functions.
 *
 * @return 0 on success
 *         1 on failure
 */
int do_bootloader_update(cmd_tbl_t * cmdtp, int flag, int argc,
			 char *const argv[])
{
	uint32_t image_addr = 0;
	uint32_t image_len = 0;
	uint32_t burn_addr = 0;
	int failsafe = 0;
	const bootloader_header_t *header;

	if (argc >= 2)
		image_addr = simple_strtoul(argv[1], NULL, 16);
	if (argc >= 3)
		image_len = simple_strtoul(argv[2], NULL, 16);
	if (argc >= 4)
		burn_addr = simple_strtoul(argv[3], NULL, 16);
	if ((argc >= 5) && (strcmp("failsafe", argv[4]) == 0))
		failsafe = 1;

/* If we don't support failsafe images, we need to put the image at the
   base of flash, so we treat all images like failsafe image in this case. */
#ifdef CONFIG_OCTEON_NO_FAILSAFE
	failsafe = 1;
	burn_addr = 0x1fc00000;
#endif

	if (!burn_addr)
		if (getenv("burnaddr"))
			burn_addr =
			    simple_strtoul(getenv("burnaddr"), NULL, 16);

	if (!image_addr) {
		if (getenv("fileaddr")) {
			image_addr =
			    simple_strtoul(getenv("fileaddr"), NULL, 16);
		} else if (load_addr) {
			image_addr = load_addr;
		} else {
			printf("ERROR: Unable to get image address from "
			       "'fileaddr' environment variable\n");
			return 1;
		}
	}

	/* Figure out what kind of flash we are going to update.  This will
	 * typically come from the bootloader header.  If the bootloader does
	 * not have a header, then it is assumed to be a legacy NOR image, and
	 * a destination NOR flash address must be supplied.  NAND images
	 * _must_ have a header.
	 */

	header = (void *)image_addr;

	if (header->magic != BOOTLOADER_HEADER_MAGIC) {
		/* No header, assume headerless NOR bootloader image */
		printf("No valid bootloader header found. Assuming old headerless image\n"
		       "Image checks cannot be performed\n");

		if (!burn_addr) {
			burn_addr = CONFIG_SYS_NORMAL_BOOTLOADER_BASE;
			DBGUPD("Unable to get burn address from 'burnaddr' environment variable,\n");
			DBGUPD(" using default 0x%x\n", burn_addr);
		}
		/* We only need image length for the headerless case */
		if (!image_len) {
			if (getenv("filesize")) {
				image_len =
				    simple_strtoul(getenv("filesize"), NULL, 16);
			} else {
				printf("ERROR: Unable to get image size from "
				       "'filesize' environment variable\n");
				return 1;
			}
		}

		return (do_bootloader_update_nor
			(image_addr, image_len, burn_addr, failsafe));
	} else {
		/* We have a header, so validate image */
		if (validate_header(header)) {
			printf("ERROR: Image header has invalid CRC.\n");
			return 1;
		}
		if (validate_data(header))	/* Errors printed */
			return 1;

		/* We now have a valid image, so determine what to do with it. */
		printf("Valid bootloader image found.\n");

		/* Check to see that it is for the board we are running on */
		if (header->board_type != cvmx_sysinfo_get()->board_type) {
			printf("Current board type: %s (%d), image board type: %s (%d)\n",
			       cvmx_board_type_to_string(cvmx_sysinfo_get()->board_type),
			       cvmx_sysinfo_get()->board_type,
			       cvmx_board_type_to_string(header->board_type),
			       header->board_type);
			if (cvmx_sysinfo_get()->board_type !=
			    CVMX_BOARD_TYPE_GENERIC
			    && header->board_type != CVMX_BOARD_TYPE_GENERIC) {
				printf("ERROR: Bootloader image is not for this board type.\n");
				return 1;
			} else {
				printf("Loading mismatched image since current "
				       "or new bootloader is generic.\n");
			}
		}

		/* SDK 1.9.0 NOR images have rev of 1.1 and unkown image_type */
		if (header->image_type == BL_HEADER_IMAGE_NOR
		    || (header->image_type == BL_HEADER_IMAGE_UNKNOWN
			&& header->maj_rev == 1 && header->min_rev == 1)) {
			return (do_bootloader_update_nor
				(image_addr, 0, burn_addr, failsafe));
		} else if (header->image_type == BL_HEADER_IMAGE_STAGE2
			   || header->image_type == BL_HEADER_IMAGE_STAGE3) {

#ifdef CONFIG_CMD_OCTEON_NAND
			return (do_bootloader_update_nand(image_addr));
#else
			printf("ERROR: This bootloader not compiled with Octeon NAND support\n");
			return 1;
#endif
		}

	}

	return 1;
}

/**
 * Validate image header. Intended for actual header discovery.
 * Not NAND or NOR specific
 *
 * @param header     Address of expected image header.
 *
 * @return 0  on success
 *         1  on failure
 */
#define BOOTLOADER_MAX_SIZE 0x200000	/* something way too large, but it 
					 * should not hopefully run ower
					 * memory end
					 */
int validate_header(const bootloader_header_t * header)
{
	if (header->magic == BOOTLOADER_HEADER_MAGIC) {
		/* check if header length field valid */
		if (header->hlen > BOOTLOADER_HEADER_MAX_SIZE) {
			printf("Corrupted header length field\n");
			return 1;
		}

		if ((header->maj_rev == 1) && (header->min_rev == 0)) {
			printf("Image header version 1.0, relocation not supported\n");
			return 1;
		}
		/* Check the CRC of the header */
		if (calculate_header_crc(header) == header->hcrc)
			return 0;
		else {
			printf("Header crc check failed\n");
			return 1;
		}
	}

	return 1;
}

/**
 * Validate image data.
 * Not NAND or NOR specific
 *
 * @param header     Address of expected image header.
 *
 * @return 0  on success
 *         1  on failure
 */
int validate_data(const bootloader_header_t * header)
{
	uint32_t image_size, crc;

	if ((image_size = get_image_size(header)) > BOOTLOADER_MAX_SIZE) {
		printf("Image has length %d - too large?\n", image_size);
		return 1;
	}

	crc = calculate_image_crc(header);
	if (crc != header->dcrc) {
		printf("Data crc failed: header value 0x%x calculated value 0x%x\n",
		       header->dcrc, crc);
		return 1;
	}
	return 0;
}

/**
 *  Given valid header returns image size (data + header); or 0
 */
uint32_t get_image_size(const bootloader_header_t * header)
{
	uint32_t isize = 0;
	if (!validate_header(header))
		/* failsafe has valid header - get the image size */
		isize = header->hlen + header->dlen;

	return isize;
}

int do_bootloader_validate(cmd_tbl_t * cmdtp, int flag, int argc,
			   char *const argv[])
{
	uint32_t image_addr = 0;
	const bootloader_header_t *header;

	if (argc >= 2)
		image_addr = simple_strtoul(argv[1], NULL, 16);

	if (!image_addr) {
		if (getenv("fileaddr")) {
			image_addr =
			    simple_strtoul(getenv("fileaddr"), NULL, 16);
		} else if (load_addr) {
			image_addr = load_addr;
		} else {
			printf("ERROR: Unable to get image address from "
			       "'fileaddr' environment variable\n");
			return 1;
		}
	}

	header = (void *)image_addr;
	if (validate_header(header)) {
		printf("Image does not have valid header\n");
		return 1;
	}

	if (validate_data(header))
		return 1;

	printf("Image validated. Header size %d, data size %d\n", header->hlen,
	       header->dlen);
	printf("                 Header crc 0x%x, data crc 0x%x\n",
	       header->hcrc, header->dcrc);
	printf("                 Image link address is 0x%llx\n",
	       header->address);

	return 0;
}

U_BOOT_CMD(bootloaderupdate, 5, 0, do_bootloader_update,
	   "Update the bootloader in flash",
	   "[image_address] [image_length]\n"
	   "Updates the the bootloader in flash.  Uses bootloader header if present\n"
	   "to validate image.\n");

U_BOOT_CMD(bootloadervalidate, 2, 0, do_bootloader_validate,
	   "Validate the bootloader image",
	   "[image_address]\n"
	   "Validates the bootloader image.  Image must have bootloader header.\n"
	   "Validates header and image crc32\n");

#endif	/* CONFIG_SYS_CMD_OCTEON_BOOTLOADER_UPDATE */

int do_nmi(cmd_tbl_t * cmdtp, int flag, int argc, char *const argv[])
{
	uint64_t coremask = simple_strtoull(argv[1], NULL, 0);
	cvmx_write_csr(CVMX_CIU_NMI, coremask);
	return 0;
}

U_BOOT_CMD(nmi, 2, 0, do_nmi,
	   "Generate a non-maskable interrupt",
	   "Generate a non-maskable interrupt on core 0");

/* These functions are used by failsafe bootloader. */
uint32_t calculate_header_crc(const bootloader_header_t * header)
{
	uint32_t crc;
	uint32_t hcrc_p;

	hcrc_p = offsetof(bootloader_header_t, hcrc);

	crc = crc32(0, (void *)header, hcrc_p);
	hcrc_p += sizeof(header->hcrc);	/* step up pass header crc field */
	crc =
	    crc32(crc, (void *)(((uint32_t) header) + hcrc_p),
		  header->hlen - hcrc_p);

	return crc;
}

uint32_t calculate_image_crc(const bootloader_header_t * header)
{
	return crc32(0, (void *)(((uint32_t) header) + header->hlen),
		     header->dlen);
}
