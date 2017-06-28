/***********************license start************************************
 * Copyright (c) 2009-2011 Cavium, Inc. (support@cavium.com). All rights
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
 * marketing@cavium.com
 *
 ***********************license end**************************************/

#include <common.h>
#include <cvmx.h>
#include <nand.h>
#include <exports.h>
#include <watchdog.h>
#include <asm/arch/cvmx-nand.h>
#include <asm/arch/octeon_nand.h>
#include <fdt.h>

#if defined(CONFIG_CMD_OCTEON_NAND) || defined(CONFIG_CMD_NAND)

DECLARE_GLOBAL_DATA_PTR;

struct octeon_nand_priv {
	int selected_chip;
	int selected_page;
	int data_len;
	int data_index;
	__attribute__ ((aligned(8))) uint8_t data[5000];
};

uint8_t octeon_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *nand = mtd->priv;
	struct octeon_nand_priv *priv = nand->priv;

	if (priv->data_index < priv->data_len) {
		return priv->data[priv->data_index++];
	} else {
		printf("error: No data to read\n");
		return 0xff;
	}
	return (0xff);
}

uint16_t octeon_read_word(struct mtd_info * mtd)
{
	struct nand_chip *nand = mtd->priv;
	struct octeon_nand_priv *priv = nand->priv;

	if (priv->data_index + 1 < priv->data_len) {
		uint16_t result = le16_to_cpup((uint16_t *) (priv->data +
							     priv->data_index));
		priv->data_index += 2;
		return result;
	} else {
		printf("error: No data to read\n");
		return 0xffff;
	}
	return 0xffff;
}

/* Write data to software buffer that will be used for later commands */
void octeon_write_buf(struct mtd_info *mtd, const uint8_t * buf, int len)
{
	struct nand_chip *nand = mtd->priv;
	struct octeon_nand_priv *priv = nand->priv;

	/* printf("%s called, len: %d\n", __FUNCTION__, len); */

	WATCHDOG_RESET();
	memcpy(priv->data + priv->data_index, buf, len);
	priv->data_index += len;
	priv->data_len += len;
	/* Linux sometimes thinks there is less OOB data than the chip really
	   has. Make sure all OOB is set to 0xff */
	memset(priv->data + priv->data_index, 0xff,
	       sizeof(priv->data) - priv->data_index);
	WATCHDOG_RESET();
}

void octeon_read_buf(struct mtd_info *mtd, uint8_t * buf, int len)
{
	struct nand_chip *nand = mtd->priv;
	struct octeon_nand_priv *priv = nand->priv;
	/* printf("%s called, len: %d, index: %d\n", __FUNCTION__, len,
	 *        priv->data_index);
	 */
	WATCHDOG_RESET();
	if (len <= priv->data_len - priv->data_index) {
		memcpy(buf, priv->data + priv->data_index, len);
		priv->data_index += len;
	} else {
		printf("octeon_read_buf: Not enough data for read of %d bytes.\n"
		       "data length: %d, data index: %d\n",
		       len, priv->data_len, priv->data_index);
		priv->data_len = 0;
	}
	WATCHDOG_RESET();
}

void octeon_select_chip(struct mtd_info *mtd, int chip)
{
	/* We don't need to do anything here */
}

int octeon_block_bad(struct mtd_info *mtd, loff_t ofs, int getchip)
{
	struct nand_chip *chip = mtd->priv;
	struct octeon_nand_priv *priv = chip->priv;
	int chip_select = priv->selected_chip;

	/* Check to see if the blockis marked bad, taking into account
	 * that some blocks are written with the Octeon NAND boot specific ECC
	 * that can look like a bad block (but isn't)
	 */
	WATCHDOG_RESET();
	return (oct_nand_block_is_bad(chip_select, ofs, 1));
}


#ifndef cmdprint
# ifdef DEBUG
#  define cmdprint printf
# else
#  define cmdprint(...)
# endif
#endif
void octeon_cmdfunc(struct mtd_info *mtd, unsigned command, int column,
		    int page_addr)
{
	cmdprint("%s called, cmd: 0x%x, col: 0x%x, page_addr: 0x%x\n",
		 __FUNCTION__, command, column, page_addr);

	int status;
	struct nand_chip *nand = mtd->priv;
	struct octeon_nand_priv *priv = nand->priv;
	int buffer_length;

	WATCHDOG_RESET();
	switch (command) {
	case NAND_CMD_PAGEPROG:
		cmdprint("cmdfunc: PAGEPROG\n");
		status = cvmx_nand_page_write(priv->selected_chip,
					      priv->selected_page << nand->
					      page_shift,
					      cvmx_ptr_to_phys(priv->data));
		if (status)
			cmdprint("PAGEPROG failed with %d\n", status);
		break;

	case NAND_CMD_SEQIN:
		cmdprint("SEQIN column=%d page_addr=0x%x\n", column, page_addr);
		/* If we don't seem to be doing sequential writes then erase
		 * all data assuming it is old
		 */
		if (priv->data_index != column)
			memset(priv->data, 0xff, sizeof(priv->data));
		priv->data_index = column;
		priv->data_len = column;
		priv->selected_page = page_addr;
		break;
	case NAND_CMD_STATUS:
		cmdprint("cmdfunc: STATUS\n");
		priv->data_index = 0;
		priv->data_len = 2;
		priv->data[0] = cvmx_nand_get_status(priv->selected_chip);
		priv->data[1] = priv->data[0];
		break;

	case NAND_CMD_READID:
		cmdprint("READID, chip: %d, data_buf: %p, data_addr: 0x%llx\n",
			 priv->selected_chip, priv->data,
			 cvmx_ptr_to_phys(priv->data));
		priv->data_index = 0;
		/* Read length must be a multiple of 8, so read a little more
		 * than we require.
		 */
		priv->data_len = cvmx_nand_read_id(priv->selected_chip, column,
						   cvmx_ptr_to_phys(priv->data),
						   16);
		if (priv->data_len < 16) {
			cmdprint("READID failed with %d\n", priv->data_len);
			priv->data_len = 0;
		}
		status = cvmx_nand_reset(priv->selected_chip);
		if (status)
			cmdprint("RESET failed with %d\n", status);
		break;

	case NAND_CMD_READOOB:
		cmdprint("READOOB page_addr=0x%x\n", page_addr);
		priv->data_index = 8;
		/* Read length must be a multiple of 8, so we start reading
		 * 8 bytes from the end of page.
		 */
		priv->data_len = cvmx_nand_page_read(priv->selected_chip,
						     (page_addr << nand->page_shift) +
						     (1 << nand->page_shift) -
						     priv->data_index,
						     cvmx_ptr_to_phys(priv->data),
						     mtd->oobsize +
						     priv->data_index);
		if (priv->data_len < mtd->oobsize + priv->data_index) {
			cmdprint("READOOB failed with %d\n", priv->data_len);
			priv->data_len = 0;
		}
		break;

	case NAND_CMD_RNDOUT:
		/* Don't do anything. */
		cmdprint("RNDOUT column %d\n", column);
		priv->data_index = column;
		break;
	case NAND_CMD_READ0:
		cmdprint("READ0 page_addr=0x%x\n", page_addr);
		priv->data_index = 0;
		/* Make sure buffer length is rounded up to multiple of 8 */
		buffer_length = (1 << nand->page_shift) + mtd->oobsize;
		buffer_length = (buffer_length + 7) & ~7;
		/* Here mtd->oobsize _must_ already be a multiple of 8 */
		priv->data_len = cvmx_nand_page_read(priv->selected_chip,
						     column +
						     (page_addr << nand->page_shift),
						     cvmx_ptr_to_phys(priv->data),
						     buffer_length);
		if (priv->data_len < (1 << nand->page_shift) + mtd->oobsize) {
			cmdprint("READ0 failed with %d\n", priv->data_len);
			priv->data_len = 0;
		}
		cmdprint("READ0 length %d\n", priv->data_len);
		break;

	case NAND_CMD_ERASE1:
		cmdprint("ERASE1 page_addr=0x%x\n", page_addr);
		if (cvmx_nand_block_erase(priv->selected_chip,
					  page_addr << nand->page_shift)) {
			cmdprint("ERASE1 failed\n");
		}
		break;
	case NAND_CMD_ERASE2:
		/* We do all erase processing in the first command, so ignore
		 * this one
		 */
		break;
	case NAND_CMD_RESET:
		cmdprint("RESET\n");
		priv->data_index = 0;
		priv->data_len = 0;
		memset(priv->data, 0xff, sizeof(priv->data));
		status = cvmx_nand_reset(priv->selected_chip);
		if (status)
			cmdprint("RESET failed with %d\n", status);
		break;

	case NAND_CMD_PARAM:
		cmdprint("PARAM");
		priv->data_len =
			cvmx_nand_read_param_page(priv->selected_chip,
						  cvmx_ptr_to_phys(priv->data),
						  2048);
		priv->data_index = column;
		break;
	default:
		printf("Octeon NAND octeon_cmdfunc: unsupported command 0x%x\n",
		       command);
		break;
	}
	WATCHDOG_RESET();

	cmdprint("\n");
	return;
}

int octeon_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{

	/* Don't do anything here, as we have already waited for busy */
	return 0;
}

int octeon_scan_bbt(struct mtd_info *mtd)
{
	/* Don't do anything here - we don't want to scan. */
	return 0;
}

int board_nand_init(struct nand_chip *chip)
{
	struct octeon_nand_priv *nand_priv;
	static int cur_chip_select = 0;
	int i;
#ifdef CONFIG_OF_LIBFDT
	int offset;
	char fdt_path[64];
	const char *ecc_mode;
	uint32_t *prop;
#endif
#if 0
	/* Don't do this here, as in some cases we need to do this before
	 * probing for NOR devices.
	 */
	if (cur_chip_select == 0) {
		/* Do cvmx_nand_stuff init stuff here */
		oct_nand_probe();
	}
#endif

	/* Find next probed NAND chip */
	for (i = cur_chip_select; i < 8; i++) {
		debug("%s: Looking for NAND chip select %d\n", __func__, i);
		WATCHDOG_RESET();
		if ((1 << i) & cvmx_nand_get_active_chips()) {
			debug("NAND chip found at cs %d\n", i);
			WATCHDOG_RESET();
			cur_chip_select = i;
			break;
		}
	}

	/* We have no more NAND chips */
	if (i == 8) {
		debug("No NAND chips found\n");
		return (-1);
	}

	/* We have found a NAND chip, so set it up.  oct_nand_probe()
	 * has done all the chip specific stuff behind the scenes.
	 * (it needs to be able to correctly identify the device paramaters,
	 * or nothing will work.)
	 */

	/* Do stuff that used to be in octeon_nand_scan */
	chip->priv = nand_priv = malloc(sizeof(struct octeon_nand_priv));
	if (!nand_priv) {
		printf("ERROR allocating memory for NAND structures\n");
		return -1;
	}
	memset(nand_priv, 0, sizeof(struct octeon_nand_priv));
	nand_priv->selected_chip = cur_chip_select;
	chip->numchips = 1;
#ifdef CONFIG_NAND_ECC_BCH
	chip->ecc.mode = NAND_ECC_SOFT_BCH;
#else
	chip->ecc.mode = NAND_ECC_SOFT;
#endif
#ifdef CONFIG_OCTEON_NAND_ECC_SIZE
	chip->ecc.size = CONFIG_OCTEON_NAND_ECC_SIZE;
#endif
#if CONFIG_OCTEON_NAND_ECC_BYTES
	chip->ecc.bytes = CONFIG_OCTEON_NAND_ECC_BYTES;
#endif

#ifdef CONFIG_OF_LIBFDT
	offset = fdt_path_offset(gd->fdt_blob, "/soc/nand-flash-interface");
	if ((offset >= 0) &&
	    (!fdt_node_check_compatible(gd->fdt_blob, offset,
					"cavium,octeon-5230-nand"))) {
		snprintf(fdt_path, sizeof(fdt_path), "flash@%d", cur_chip_select);
		offset = fdt_subnode_offset(gd->fdt_blob, offset, fdt_path);
		if (offset >= 0) {
			ecc_mode = (const char *)fdt_getprop(gd->fdt_blob,
							     offset,
							     "nand-ecc-mode",
							     NULL);
			if (ecc_mode && !strcmp(ecc_mode, "soft_bch")) {
				chip->ecc.mode = NAND_ECC_SOFT_BCH;
				prop = (uint32_t *)fdt_getprop(gd->fdt_blob,
							       offset,
							       "nand-ecc-size",
							       NULL);
				if (prop)
					chip->ecc.size = fdt32_to_cpu(*prop);
				prop = (uint32_t *)fdt_getprop(gd->fdt_blob,
							       offset,
							       "nand-ecc-bytes",
							       NULL);
				if (prop)
					chip->ecc.bytes = fdt32_to_cpu(*prop);
				debug("NAND ecc mode %s, ecc size: %d, ecc bytes: %d\n",
				      ecc_mode, chip->ecc.size, chip->ecc.bytes);
			} else if (ecc_mode && !strcmp(ecc_mode, "soft")) {
				chip->ecc.mode = NAND_ECC_SOFT;
				debug("NAND ecc mode %s\n", ecc_mode);
			} else if (ecc_mode && !strcmp(ecc_mode, "none")) {
				chip->ecc.mode = NAND_ECC_NONE;
				debug("NAND ecc mode %s\n", ecc_mode);
			} else if (ecc_mode) {
				printf("ERROR : Unknown or unsupported NAND ECC mode %s\n",
				       ecc_mode);
				return -1;
			}
		}
	}
#endif
	/* Functions that we define with something useful */
	chip->read_byte = octeon_read_byte;
	chip->read_word = octeon_read_word;
	chip->write_buf = octeon_write_buf;
	chip->read_buf = octeon_read_buf;
	chip->block_bad = octeon_block_bad;	/* May not be needed */
	chip->cmdfunc = octeon_cmdfunc;

	/* Some functions we override, but they end up being no-ops. */
	chip->select_chip = octeon_select_chip;
	chip->waitfunc = octeon_waitfunc;

	/* We generally don't want to scan the whole flash
	 * to generate the BBT, so we have a dummy function that does
	 * nothing.
	 * The BBT scan functions do funky things to read the OOB which
	 * currently are not supported.
	 */
	chip->scan_bbt = octeon_scan_bbt;

	/* Move on to next chip on next invocation */
	cur_chip_select++;
	return 0;
}

int octeon_nand_get_oob_size(int nand_chip)
{

	return (cvmx_nand_get_oob_size(nand_chip) & ~0x7);
}

int oct_nand_probe(void)
{
	int i;
	int active_nand_mask;
	int chip = -1;
	int debug_flag;

#if 0
	/* Set default values.  This can be done to allow unrecognizable parts
	 * to be configured.  If a part cannot be autoconfigured, then these
	 * values are used.
	 */
	/*  For Numonyx NAND08GW3B2CN6 part */
	cvmx_nand_set_defaults(2048, 64, 64, 8192, 2);
#endif

	/* Detect NAND flash */
	WATCHDOG_RESET();
	debug("%s: Calling cvmx_nand_initialize()\n", __func__);

	if (getenv("octeon_debug_nand")) {
		debug_flag = CVMX_NAND_INITIALIZE_FLAGS_DEBUG;
		printf("OCTEON NAND debugging enabled.\n");
	} else
		debug_flag = 0;

	if (cvmx_nand_initialize(debug_flag, 0xff)) {
		debug("%s: Could not initialize NAND\n", __func__);
		return OCT_NAND_ERROR;
	}
	WATCHDOG_RESET();
	/* Find the first nand chip and use it */
	debug("%s: Calling cvmx_nand_get_active_chips()\n", __func__);
	active_nand_mask = cvmx_nand_get_active_chips();
	debug("%s: Active nand chip mask: 0x%x\n", __func__, active_nand_mask);

	for (i = 0; i < 8; i++) {
		if ((1 << i) & active_nand_mask) {
			chip = i;
			break;
		}
	}

#if 1
	/* The ebt5200 board has both NAND and NOR, and they can be swapped
	 * between chip selects 0/1.  We can't tell which way it is set up other
	 * than probing to see what is there.  Older bootloader init code
	 * unconditionally enabled CS0, even in NAND boot mode (stage 0 clears
	 * it), so we handle two cases:
	 *  - CS0 enabled so NAND not detected.  This happens when a board
	 *    configured for NAND boot is booted over EJTAG/PCI.
	 *  - CS0 disnabled, NAND detected.  This is normal when booted via
	 *    stage1.
	 * In both cases we then want to set the base/size/enable on CS1 for
	 * the NOR chip.  Note that bad things seem to happen if we try CFI
	 * probing of a NAND chip.
	 * Try this for both ebt5200 boards and the generic board type.
	 */
	if (chip <= 0) {
		cvmx_mio_boot_reg_cfgx_t reg_cfg;
		cvmx_mio_boot_reg_cfgx_t reg_cfg_orig_cs0;
		reg_cfg_orig_cs0.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFGX(0));
		if (chip < 0) {
			reg_cfg.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFGX(0));
			reg_cfg.s.en = 0;
			cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(0), reg_cfg.u64);

			/* Detect NAND flash */
			debug("%s: Calling cvmx_nand_initialize()\n", __func__);
			if (cvmx_nand_initialize(0 & CVMX_NAND_INITIALIZE_FLAGS_DEBUG, 0xff)) {
				printf("ERROR initializing NAND\n");
				return OCT_NAND_ERROR;
			}
			WATCHDOG_RESET();

			/* Find the first nand chip and use it */
			debug("%s: Calling cvmx_nand_get_active_chips()\n",
			      __func__);
			active_nand_mask = cvmx_nand_get_active_chips();
			for (i = 0; i < 8; i++) {
				if ((1 << i) & active_nand_mask) {
					chip = i;
					break;
				}
			}

		}
		if (chip == 0) {
			/* NAND is on CS 0, rather than boot flash.  Copy CS 0
			 * config to CS1, as the early boot code set up CS 0 for
			 * boot flash.  Only do this if CS1 is not alread
			 * enabled, and CS1 doesn't have a NAND chip detected on
			 * it.
			 */
			if (!(((cvmx_mio_boot_reg_cfgx_t)
				cvmx_read_csr(CVMX_MIO_BOOT_REG_CFGX(1))).s.en)
			      && !(active_nand_mask & 0x2)) {
				reg_cfg.u64 =
				    cvmx_read_csr(CVMX_MIO_BOOT_REG_CFGX(0));
				reg_cfg.s.en = 1;
				cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(1),
					       reg_cfg.u64);
				cvmx_write_csr(CVMX_MIO_BOOT_REG_TIMX(1),
					       cvmx_read_csr
					       (CVMX_MIO_BOOT_REG_TIMX(0)));
			}
		} else {
			/* Just restore CS0 config */
			cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(0),
				       reg_cfg_orig_cs0.u64);
		}
	}
#endif

	WATCHDOG_RESET();
	debug("%s: Calling cvmx_nand_get_blocks(%d)\n", __func__, chip);
	if (cvmx_nand_get_blocks(chip) < 1)
		return OCT_NAND_ERROR;

	WATCHDOG_RESET();
#ifdef CONFIG_CMD_OCTEON_NAND
	debug("%s: Calling oct_nand_set_cur_chip(%d)\n", __func__, chip);
	oct_nand_set_cur_chip(chip);
#endif
	return chip;
}
#endif
