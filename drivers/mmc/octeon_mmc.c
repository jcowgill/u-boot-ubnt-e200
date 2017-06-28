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
#include <command.h>
#include <mmc.h>
#include <part.h>
#include <malloc.h>
#include <errno.h>
#include <asm/arch/octeon_mmc.h>
#include <asm/io.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/octeon-feature.h>
#include <asm/arch/cvmx-access.h>
#include <asm/arch/cvmx-mio-defs.h>
#include <asm/arch/octeon_board_mmc.h>
#include <linux/list.h>
#include <div64.h>
#include <watchdog.h>
#include <asm/gpio.h>

/* Enable support for SD as well as MMC */
#define CONFIG_OCTEON_MMC_SD

/*
 * Due to how MMC is implemented in the OCTEON processor it is not
 * possible to use the generic MMC support.  However, this code
 * implements all of the MMC support found in the generic MMC driver
 * and should be compatible with it for the most part.
 *
 * Currently both MMC and SD/SDHC are supported.
 */

#define WATCHDOG_COUNT		(1000)	/* in msecs */

#ifndef CONFIG_OCTEON_MAX_MMC_SLOT
# define CONFIG_OCTEON_MAX_MMC_SLOT		4
#endif

#ifndef CONFIG_OCTEON_MIN_BUS_SPEED_HZ
# define CONFIG_OCTEON_MIN_BUS_SPEED_HZ		400000
#endif

#ifndef CONFIG_OCTEON_MMC_SD
# undef IS_SD
# define IS_SD(x) (0)
#endif

#ifndef CONFIG_SYS_MMC_MAX_BLK_COUNT
#define CONFIG_SYS_MMC_MAX_BLK_COUNT	16384
#else
#error CONFIG_SYS_MMC_MAX_BLK_COUNT already defined!
#endif

#define MMC_CMD_FLAG_CTYPE_XOR(x)	(((x) & 3) << 16)
#define MMC_CMD_FLAG_RTYPE_XOR(x)	(((x) & 7) << 20)
#define MMC_CMD_FLAG_OFFSET(x)		(((x) & 0x3f) << 24)
#define MMC_CMD_FLAG_IGNORE_CRC_ERR	(1 << 30)
#define MMC_CMD_FLAG_STRIP_CRC		(1 << 31)

DECLARE_GLOBAL_DATA_PTR;

static LIST_HEAD(mmc_devices);
static struct mmc	mmc_dev[CONFIG_OCTEON_MAX_MMC_SLOT];
static struct mmc_host	mmc_host[CONFIG_OCTEON_MAX_MMC_SLOT];
static struct octeon_mmc_info mmc_info;

static int cur_dev_num = -1;
static int init_time = 1;

int mmc_set_blocklen(struct mmc *mmc, int len);

static int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd,
			struct mmc_data *data);

static int mmc_send_acmd(struct mmc *mmc, struct mmc_cmd *cmd,
			 struct mmc_data *data);

static void mmc_set_ios(struct mmc *mmc);

#ifdef CONFIG_OCTEON_MMC_SD
static int sd_set_ios(struct mmc *mmc);
#endif

#ifdef DEBUG
static void mmc_print_status(uint32_t status)
{
	static const char *state[] = {
		"Idle",		/* 0 */
		"Ready",	/* 1 */
		"Ident",	/* 2 */
		"Standby",	/* 3 */
		"Tran",		/* 4 */
		"Data",		/* 5 */
		"Receive",	/* 6 */
		"Program",	/* 7 */
		"Dis",		/* 8 */
		"Btst",		/* 9 */
		"Sleep",	/* 10 */
		"reserved",	/* 11 */
		"reserved",	/* 12 */
		"reserved",	/* 13 */
		"reserved",	/* 14 */
		"reserved"	/* 15 */ };
	if (status & R1_APP_CMD)
		puts("MMC ACMD\n");
	if (status & R1_SWITCH_ERROR)
		puts("MMC switch error\n");
	if (status & R1_READY_FOR_DATA)
		puts("MMC ready for data\n");
	printf("MMC %s state\n", state[R1_CURRENT_STATE(status)]);
	if (status & R1_ERASE_RESET)
		puts("MMC erase reset\n");
	if (status & R1_WP_ERASE_SKIP)
		puts("MMC partial erase due to write protected blocks\n");
	if (status & R1_CID_CSD_OVERWRITE)
		puts("MMC CID/CSD overwrite error\n");
	if (status & R1_ERROR)
		puts("MMC undefined device error\n");
	if (status & R1_CC_ERROR)
		puts("MMC device error\n");
	if (status & R1_CARD_ECC_FAILED)
		puts("MMC internal ECC failed to correct data\n");
	if (status & R1_ILLEGAL_COMMAND)
		puts("MMC illegal command\n");
	if (status & R1_COM_CRC_ERROR)
		puts("MMC CRC of previous command failed\n");
	if (status & R1_LOCK_UNLOCK_FAILED)
		puts("MMC sequence or password error in lock/unlock device command\n");
	if (status & R1_CARD_IS_LOCKED)
		puts("MMC device locked by host\n");
	if (status & R1_WP_VIOLATION)
		puts("MMC attempt to program write protected block\n");
	if (status & R1_ERASE_PARAM)
		puts("MMC invalid selection of erase groups for erase\n");
	if (status & R1_ERASE_SEQ_ERROR)
		puts("MMC error in sequence of erase commands\n");
	if (status & R1_BLOCK_LEN_ERROR)
		puts("MMC block length error\n");
	if (status & R1_ADDRESS_ERROR)
		puts("MMC address misalign error\n");
	if (status & R1_OUT_OF_RANGE)
		puts("MMC address out of range\n");
}
#endif

#ifdef CONFIG_PARTITIONS
block_dev_desc_t *mmc_get_dev(int dev)
{
	struct mmc *mmc = find_mmc_device(dev);

	if (mmc)
		debug("%s: Found mmc device %d\n", __func__, dev);
	else
		debug("%s: mmc device %d not found\n", __func__, dev);

	return mmc ? &mmc->block_dev : NULL;
}
#endif

struct mmc *find_mmc_device(int dev_num)
{
	struct mmc *m;
	struct list_head *entry;

	/* If nothing is available, try and initialize */
	if (list_empty(&mmc_devices)) {
		if (mmc_initialize(gd->bd))
			return NULL;
	}

	list_for_each(entry, &mmc_devices) {
		m = list_entry(entry, struct mmc, link);

		if (m->block_dev.dev == dev_num)
			return m;
	}

	printf("MMC Device %d not found\n", dev_num);

	return NULL;
}

int mmc_read(struct mmc *mmc, u64 src, uchar *dst, int size)
{
	uint64_t dma_addr;
	cvmx_mio_emm_dma_t emm_dma;
	cvmx_mio_ndf_dma_cfg_t ndf_dma;
	cvmx_mio_ndf_dma_int_t ndf_dma_int;
	cvmx_mio_emm_rsp_sts_t rsp_sts;
	cvmx_mio_emm_int_t emm_int;
	cvmx_mio_emm_sts_mask_t emm_sts_mask;
	int timeout;
	int dma_retry_count = 0;
	struct mmc_cmd cmd;

	debug("%s(src: 0x%llx, dst: 0x%p, size: %d)\n", __func__, src, dst, size);
#ifdef DEBUG
	memset(dst, size * mmc->read_bl_len, 0);
#endif
	debug("Setting block length to %d\n", mmc->read_bl_len);
	if (!IS_SD(mmc) || (IS_SD(mmc) && mmc->high_capacity))
		mmc_set_blocklen(mmc, mmc->read_bl_len);

	/* Clear DMA interrupt */
	ndf_dma_int.u64 = 0;
	ndf_dma_int.s.done = 1;
	cvmx_write_csr(CVMX_MIO_NDF_DMA_INT, ndf_dma_int.u64);

	/* Enable appropriate errors */
	emm_sts_mask.u64 = 0;
	emm_sts_mask.s.sts_msk = R1_BLOCK_READ_MASK;
	cvmx_write_csr(CVMX_MIO_EMM_STS_MASK, emm_sts_mask.u64);

	dma_addr = cvmx_ptr_to_phys(dst);

	ndf_dma.u64 = 0;
	ndf_dma.s.en = 1;
	ndf_dma.s.clr = 0;
	ndf_dma.s.size = ((uint64_t)(size * mmc->read_bl_len) / 8) - 1;
	ndf_dma.s.adr = dma_addr;
	debug("%s: Writing 0x%llx to mio_ndf_dma_cfg\n", __func__, ndf_dma.u64);
	cvmx_write_csr(CVMX_MIO_NDF_DMA_CFG, ndf_dma.u64);

	emm_dma.u64 = 0;
	emm_dma.s.dma_val = 1;
	emm_dma.s.sector = mmc->high_capacity ? 1 : 0;
	/* NOTE: For SD we can only support multi-block transfers if
	 * bit 33 (CMD_SUPPORT) is set in the SCR register.
	 */
	if ((size > 1) && ((IS_SD(mmc) && (mmc->scr[1] & 2)) || !IS_SD(mmc)))
		emm_dma.s.multi = 1;
	else
		emm_dma.s.multi = 0;

	emm_dma.s.block_cnt = size;
	if (!mmc->high_capacity)
		src *= mmc->read_bl_len;
	emm_dma.s.card_addr = src;
	/* Clear interrupt */
	emm_int.u64 = cvmx_read_csr(CVMX_MIO_EMM_INT);
	cvmx_write_csr(CVMX_MIO_EMM_INT, emm_int.u64);

	debug("%s: Writing 0x%llx to mio_emm_dma\n", __func__, emm_dma.u64);
	cvmx_write_csr(CVMX_MIO_EMM_DMA, emm_dma.u64);

retry_dma:
	timeout = 500000 + 2000 * size;

	do {
		ndf_dma_int.u64 = cvmx_read_csr(CVMX_MIO_NDF_DMA_INT);

		if (ndf_dma_int.s.done) {
			cvmx_write_csr(CVMX_MIO_NDF_DMA_INT, ndf_dma_int.u64);
			break;
		}

		WATCHDOG_RESET();
		udelay(1);
	} while (timeout-- > 0);

	rsp_sts.u64 = cvmx_read_csr(CVMX_MIO_EMM_RSP_STS);
	if (!ndf_dma_int.s.done || timeout <= 0 || rsp_sts.s.dma_val
	    || rsp_sts.s.dma_pend) {

		emm_dma.u64 = cvmx_read_csr(CVMX_MIO_EMM_DMA);
		emm_int.u64 = cvmx_read_csr(CVMX_MIO_EMM_INT);
		debug("%s: Error detected: MIO_EMM_RSP_STS: 0x%llx, MIO_EMM_DMA: 0x%llx, MIO_EMM_INT: 0x%llx\n",
		      __func__, rsp_sts.u64, emm_dma.u64, emm_int.u64);
		if (timeout <= 0) {
			printf("%s(mmc, 0x%llx, %p, %d)\n",
			       __func__, src, dst, size);
			printf("MMC read DMA timeout, status: 0x%llx, interrupt status: 0x%llx\n",
			       rsp_sts.u64, emm_int.u64);
			printf("\tMIO_EMM_DMA: 0x%llx, last command: %d\n",
			       emm_dma.u64, rsp_sts.s.cmd_idx);
		} else {
			if (rsp_sts.s.blk_timeout)
				printf("Block timeout error detected\n");
			if (rsp_sts.s.blk_crc_err)
				printf("Block CRC error detected\n");
		}
		if (dma_retry_count++ < 3) {
			/* DMA still pending, terminate it */
			emm_dma.s.dma_val = 1;
			timeout = 2000 * size;
			cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
			cmd.cmdarg = 0;
			cmd.flags = 0;
			cmd.resp_type = MMC_RSP_R1b;
			if (mmc_send_cmd(mmc, &cmd, NULL))
				printf("Error sending stop transmission cmd\n");
#ifdef DEBUG
			int i;
			for (i = 0; i < 512; i++) {
				if (i % 16 == 0)
					printf("\n0x%03x: ", i);
				if (i % 16 == 8)
					puts("- ");
				printf("%02x ", (uint32_t)dst[i]);
			}
			puts("\n");
#endif
			cvmx_write_csr(CVMX_MIO_EMM_DMA, emm_dma.u64);
			debug("Retrying MMC read DMA\n");
			goto retry_dma;
		} else {
			cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
			cmd.cmdarg = 0;
			cmd.flags = 0;
			cmd.resp_type = MMC_RSP_R1b;
			if (mmc_send_cmd(mmc, &cmd, NULL))
				printf("Error sending stop transmission cmd\n");
			printf("mmc read block %llu, size %d DMA failed, terminating...\n",
			       src, size);
			emm_dma.s.dma_val = 1;
			emm_dma.s.dat_null = 1;
			cvmx_write_csr(CVMX_MIO_EMM_DMA, emm_dma.u64);
			timeout = 2000 * size;
			do {
				ndf_dma_int.u64 =
					cvmx_read_csr(CVMX_MIO_NDF_DMA_INT);
				if (ndf_dma_int.s.done)
					break;
				udelay(1);
			} while (timeout-- > 0);
			if (timeout < 0)
				puts("Error: MMC read DMA failed to terminate!\n");
			return 0;
		}
	}

	if (dma_retry_count)
		debug("Success after %d DMA retries\n", dma_retry_count);

	if (timeout <= 0) {
		printf("MMC read block %llu timed out\n", src);
		debug("Read status 0x%llx\n", rsp_sts.u64);
		emm_dma.u64 = cvmx_read_csr(CVMX_MIO_EMM_DMA);
		debug("EMM_DMA: 0x%llx\n", emm_dma.u64);

		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.flags = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (mmc_send_cmd(mmc, &cmd, NULL))
			printf("Error sending stop transmission cmd\n");
		return 0;
	}
	debug("Read status 0x%llx\n", rsp_sts.u64);

	emm_dma.u64 = cvmx_read_csr(CVMX_MIO_EMM_DMA);
	debug("EMM_DMA: 0x%llx\n", emm_dma.u64);
#ifdef DEBUG
	int i;
	for (i = 0; i < 512; i++) {
		if (i % 16 == 0)
			printf("\n0x%03x: ", i);
		if (i % 16 == 8)
			puts("- ");
		printf("%02x ", (uint32_t)dst[i]);
	}
	puts("\n");
#endif
	return size - emm_dma.s.block_cnt;
}

/**
 * Writes sectors to MMC device
 *
 * @param[in,out] mmc - MMC device to write to
 * @param start - starting sector to write to
 * @param size - number of sectors to write
 * @param src - pointer to source address of buffer containing sectors
 *
 * @return number of sectors or 0 if none.
 *
 * NOTE: This checks the GPIO write protect if it is present
 */
static ulong
mmc_write(struct mmc *mmc, ulong start, int size, const void *src)
{
	uint64_t dma_addr;
	cvmx_mio_emm_dma_t emm_dma;
	cvmx_mio_ndf_dma_cfg_t ndf_dma;
	cvmx_mio_ndf_dma_int_t ndf_dma_int;
	cvmx_mio_emm_rsp_sts_t rsp_sts;
	cvmx_mio_emm_int_t emm_int;
	cvmx_mio_emm_sts_mask_t emm_sts_mask;
	cvmx_mio_emm_buf_idx_t emm_buf_idx;
	struct mmc_cmd cmd;
	int timeout;
	int dma_retry_count = 0;
	int rc;
	int multi;

	debug("%s(start: %lu, size: %d, src: 0x%p)\n", __func__, start,
	      size, src);

	/* Poll for ready status */
	timeout = 10000;	/* 10 seconds */
	udelay(100);
	do {
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmdidx = MMC_CMD_SEND_STATUS;
		cmd.cmdarg = mmc->rca << 16;
		cmd.resp_type = MMC_RSP_R1;
		rc = mmc_send_cmd(mmc, &cmd, NULL);
		if (rc) {
			printf("%s: Error getting device status\n", __func__);
			return 0;
		} else if (cmd.response[0] & R1_READY_FOR_DATA) {
			break;
		}
		udelay(1000);
	} while (--timeout > 0);
	debug("%s: Device status: 0x%x\n", __func__, cmd.response[0]);
	if (timeout == 0) {
		printf("%s: Device timed out waiting for empty buffer, response: 0x%x\n",
		       __func__, cmd.response[0]);
		return 0;
	}

	/* NOTE: For SD we can only support multi-block transfers if
	 * bit 33 (CMD_SUPPORT) is set in the SCR register.
	 */
	multi = ((size > 1)
		 && ((IS_SD(mmc) && (mmc->scr[1] & 2)) || !IS_SD(mmc)));
	emm_buf_idx.u64 = 0;
	emm_buf_idx.s.inc = 1;
	cvmx_write_csr(CVMX_MIO_EMM_BUF_IDX, emm_buf_idx.u64);

	/* Clear DMA interrupt */
	ndf_dma_int.u64 = 0;
	ndf_dma_int.s.done = 1;
	cvmx_write_csr(CVMX_MIO_NDF_DMA_INT, ndf_dma_int.u64);

	/* Enable appropriate errors */
	emm_sts_mask.u64 = 0;
	emm_sts_mask.s.sts_msk = R1_BLOCK_WRITE_MASK;
	cvmx_write_csr(CVMX_MIO_EMM_STS_MASK, emm_sts_mask.u64);

	mmc_set_blocklen(mmc, mmc->write_bl_len);

	dma_addr = cvmx_ptr_to_phys((uchar *)src);

	ndf_dma.u64 = 0;
	ndf_dma.s.en = 1;
	ndf_dma.s.rw = 1;
	ndf_dma.s.clr = 0;
	ndf_dma.s.size = ((uint64_t)(size * 512) / 8) - 1;
	ndf_dma.s.adr = dma_addr;
	debug("%s: Writing 0x%llx to mio_ndf_dma_cfg\n", __func__, ndf_dma.u64);
	debug("DMA address: 0x%llx\n", dma_addr);

	emm_dma.u64 = 0;
	emm_dma.s.dma_val = 1;
	emm_dma.s.sector = mmc->high_capacity ? 1 : 0;
	emm_dma.s.rw = 1;
	emm_dma.s.rel_wr = 0;
	emm_dma.s.multi = multi;

	if (!mmc->high_capacity)
		start *= mmc->write_bl_len;

	emm_dma.s.block_cnt = size;
	emm_dma.s.card_addr = start;
	/* Clear interrupt */
	ndf_dma_int.u64 = 0;
	ndf_dma_int.s.done = 1;
	cvmx_write_csr(CVMX_MIO_NDF_DMA_INT, ndf_dma_int.u64);
	emm_int.u64 = 0;
	emm_int.s.dma_done = 1;
	emm_int.s.cmd_err = 1;
	emm_int.s.dma_err = 1;
	cvmx_write_csr(CVMX_MIO_EMM_INT, emm_int.u64);
	debug("%s: Writing 0x%llx to mio_emm_dma\n", __func__, emm_dma.u64);
	cvmx_write_csr(CVMX_MIO_NDF_DMA_CFG, ndf_dma.u64);
	cvmx_write_csr(CVMX_MIO_EMM_DMA, emm_dma.u64);

retry_dma:
	timeout = 500000 + 2000 * size;

	do {
#ifdef DEBUG
		if (ctrlc()) {
			printf("Interrupted by user\n");
			break;
		}
#endif

		rsp_sts.u64 = cvmx_read_csr(CVMX_MIO_EMM_RSP_STS);
		if (((rsp_sts.s.dma_val == 0) || (rsp_sts.s.dma_pend == 1))
		    && rsp_sts.s.cmd_done)
			break;

		WATCHDOG_RESET();
		udelay(1);
	} while (--timeout > 0);
	if (timeout <= 0) {
		printf("%s: write command completion timeout for cmd %d\n",
		       __func__, rsp_sts.s.cmd_idx);
	}
	/*rsp_sts.u64 = cvmx_read_csr(CVMX_MIO_EMM_RSP_STS);*/
	debug("emm_rsp_sts: 0x%llx, cmd: %d, response: 0x%llx\n",
	      rsp_sts.u64, rsp_sts.s.cmd_idx,
	      cvmx_read_csr(CVMX_MIO_EMM_RSP_LO));
	if (rsp_sts.s.cmd_val || timeout <= 0 || rsp_sts.s.dma_val
	    || rsp_sts.s.dma_pend) {
		emm_dma.u64 = cvmx_read_csr(CVMX_MIO_EMM_DMA);
		emm_int.u64 = cvmx_read_csr(CVMX_MIO_EMM_INT);
		printf("%s: Error detected: MIO_EMM_RSP_STS: 0x%llx, "
		       "MIO_EMM_DMA: 0x%llx,\n\tMIO_NDF_DMA_CFG: 0x%llx, timeout: %d\n",
		       __func__, rsp_sts.u64, emm_dma.u64,
		       cvmx_read_csr(CVMX_MIO_NDF_DMA_CFG), timeout);
		printf("Last command index: %d\n", rsp_sts.s.cmd_idx);
		printf("emm_int: 0x%llx\n", emm_int.u64);
		udelay(10000);
		rsp_sts.u64 = cvmx_read_csr(CVMX_MIO_EMM_RSP_STS);
		printf("Re-read rsp_sts: 0x%llx, cmd_idx: %d\n", rsp_sts.u64,
		       rsp_sts.s.cmd_idx);
		printf("  RSP_LO: 0x%llx\n", cvmx_read_csr(CVMX_MIO_EMM_RSP_LO));
		if (timeout <= 0) {
			printf("%s(mmc, 0x%lx, %d, 0x%p)\n",
			       __func__, start, size, src);
			printf("MMC write DMA timeout, status: 0x%llx, interrupt status: 0x%llx\n",
			       rsp_sts.u64, emm_int.u64);
			printf("\tMIO_EMM_DMA: 0x%llx, last command: %d\n",
			       emm_dma.u64, rsp_sts.s.cmd_idx);
		} else {
			if (rsp_sts.s.blk_timeout)
				printf("Block timeout error detected\n");
			if (rsp_sts.s.blk_crc_err)
				printf("Block CRC error detected\n");
			if (rsp_sts.s.dma_val) {
				printf("DMA still valid\n");
			}
		}

		if (dma_retry_count++ < 3 && rsp_sts.s.dma_pend) {
			/* DMA still pending, terminate it */
			emm_dma.s.dma_val = 1;
			timeout = 2000 * size;
			cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
			cmd.cmdarg = 0;
			cmd.flags = 0;
			cmd.resp_type = MMC_RSP_R1b;
			mmc_send_cmd(mmc, &cmd, NULL);
			cvmx_write_csr(CVMX_MIO_EMM_DMA, emm_dma.u64);
			debug("Retrying MMC write DMA\n");
			goto retry_dma;
		} else {
			emm_dma.s.dma_val = 1;
			emm_dma.s.dat_null = 1;
			cvmx_write_csr(CVMX_MIO_EMM_DMA, emm_dma.u64);
			timeout = 2000 * size;
			do {
				ndf_dma_int.u64 =
					cvmx_read_csr(CVMX_MIO_NDF_DMA_INT);
				if (ndf_dma_int.s.done)
					break;
				udelay(1);
			} while (--timeout > 0);
			if (timeout <= 0)
				puts("Error: MMC write DMA failed to terminate!\n");
			return 0;
		}
	}

	if (dma_retry_count)
		debug("Success after %d DMA retries\n", dma_retry_count);

	if (timeout <= 0) {
		printf("MMC write block %lu timed out\n", start);
		debug("Write status 0x%llx\n", rsp_sts.u64);
		emm_dma.u64 = cvmx_read_csr(CVMX_MIO_EMM_DMA);
		debug("EMM_DMA: 0x%llx\n", emm_dma.u64);

		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.flags = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (mmc_send_cmd(mmc, &cmd, NULL))
			printf("Error sending stop transmission cmd\n");
		return 0;
	}
	debug("Write status 0x%llx\n", rsp_sts.u64);

#if 1
	/* Poll status if we can't send data right away */
	if (!((timeout > 0)
	     && (rsp_sts.s.cmd_idx == MMC_CMD_SEND_STATUS)
	     && rsp_sts.s.cmd_done
	     && ((cvmx_read_csr(CVMX_MIO_EMM_RSP_LO) >> 8) & R1_READY_FOR_DATA))) {
		/* Poll for ready status */
		timeout = 10000;	/* 10 seconds */
		do {
			memset(&cmd, 0, sizeof(cmd));
			cmd.cmdidx = MMC_CMD_SEND_STATUS;
			cmd.cmdarg = mmc->rca << 16;
			cmd.resp_type = MMC_RSP_R1;
			rc = mmc_send_cmd(mmc, &cmd, NULL);
			if (rc) {
				printf("%s: Error getting post device status\n",
				       __func__);
				return 0;
			}
			if (cmd.response[0] & R1_READY_FOR_DATA)
				break;
			udelay(1000);
		} while (--timeout > 0);
		if (timeout == 0) {
			printf("%s: Device timed out waiting for empty buffer\n",
			       __func__);
			return 0;
		}
	}
#endif
	emm_dma.u64 = cvmx_read_csr(CVMX_MIO_EMM_DMA);
	debug("EMM_DMA: 0x%llx\n", emm_dma.u64);

	return size - emm_dma.s.block_cnt;
}

static ulong mmc_erase_t(struct mmc *mmc, ulong start, lbaint_t blkcnt)
{
	struct mmc_cmd cmd;
	ulong end;
	int err, start_cmd, end_cmd;

	if (mmc->high_capacity)
		end = start + blkcnt - 1;
	else {
		end = (start + blkcnt - 1) * mmc->write_bl_len;
		start *= mmc->write_bl_len;
	}

	if (IS_SD(mmc)) {
		start_cmd = SD_CMD_ERASE_WR_BLK_START;
		end_cmd = SD_CMD_ERASE_WR_BLK_END;
	} else {
		start_cmd = MMC_CMD_ERASE_GROUP_START;
		end_cmd = MMC_CMD_ERASE_GROUP_END;
	}

	cmd.cmdidx = start_cmd;
	cmd.cmdarg = start;
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		goto err_out;

	cmd.cmdidx = end_cmd;
	cmd.cmdarg = end;
	cmd.resp_type = MMC_RSP_R1b;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		goto err_out;

	return 0;

err_out:
	puts("mmc erase failed\n");
	return err;
}

static ulong mmc_bread(int dev_num, ulong start, lbaint_t blkcnt, void *dst)
{
	lbaint_t cur, blocks_todo = blkcnt;
	struct mmc *mmc = find_mmc_device(dev_num);
	unsigned char bounce_buffer[4096];

	debug("%s(%d, %lu, %llu, %p)\n", __func__, dev_num, start,
	      (uint64_t)blkcnt, dst);
	if (!mmc)
		return 0;

	if (blkcnt == 0)
		return 0;

	if ((start + blkcnt) > mmc->block_dev.lba) {
		printf("MMC: block number 0x%llx exceeds max(0x%llx)\n",
		       (uint64_t)(start + blkcnt),
		       (uint64_t)mmc->block_dev.lba);
		return 0;
	}

	if (((ulong)dst) & 7) {
		debug("%s: Using bounce buffer due to alignment\n", __func__);
		do {
			if (mmc_read(mmc, start, bounce_buffer, 1) != 1)
				return 0;
			memcpy(dst, bounce_buffer, mmc->read_bl_len);
			WATCHDOG_RESET();
			dst += mmc->read_bl_len;
			start++;
			blocks_todo--;
		} while (blocks_todo > 0);
	} else {
		do {
			cur = min(blocks_todo, mmc->b_max);
			if (mmc_read(mmc, start, dst, cur) != cur)
				return 0;
			WATCHDOG_RESET();
			blocks_todo -= cur;
			start += cur;
			dst += cur * mmc->read_bl_len;
		} while (blocks_todo > 0);
	}

	return blkcnt;
}

static ulong mmc_bwrite(int dev_num, ulong start, lbaint_t blkcnt,
			const void *src)
{
	lbaint_t cur, blocks_todo = blkcnt;
	struct mmc *mmc = find_mmc_device(dev_num);
	unsigned char bounce_buffer[4096];
	struct mmc_host *host;

	debug("%s(%d, %lu, %llu, %p)\n", __func__, dev_num, start,
	      (uint64_t)blkcnt, src);
	if (!mmc) {
		printf("MMC Write: device %d not found\n", dev_num);
		return 0;
	}
	host = (struct mmc_host *)(mmc->priv);
	if (blkcnt == 0)
		return 0;
	if ((start + blkcnt) > mmc->block_dev.lba) {
		printf("MMC: block number 0x%llx exceeds max(0x%llx)\n",
		       (uint64_t)(start + blkcnt),
		       (uint64_t)mmc->block_dev.lba);
		return 0;
	}
	if (host->cd_gpio != -1) {
		int cd_val = gpio_get_value(host->cd_gpio);
		if (cd_val != host->cd_active_low) {
			printf("%s: ERROR: Card not detected\n", __func__);
			return 0;
		}
	}

	if (host->wp_gpio != -1) {
		int wp_val = gpio_get_value(host->wp_gpio);
		if (wp_val != host->wp_active_low) {
			printf("%s: Failed due to write protect switch\n",
			       __func__);
			return 0;
		}
	}
	if (((ulong)src) & 7) {
		debug("%s: Using bounce buffer due to alignment\n", __func__);
		do {
			memcpy(bounce_buffer, src, mmc->write_bl_len);
			if (mmc_write(mmc, start, 1, bounce_buffer) != 1)
				return 0;
			WATCHDOG_RESET();
			src += mmc->write_bl_len;
			start++;
			blocks_todo--;
		} while (blocks_todo > 0);
	} else {
		do {
			cur = min(blocks_todo, mmc->b_max);
			if (mmc_write(mmc, start, cur, src) != cur)
				return 0;
			WATCHDOG_RESET();
			blocks_todo -= cur;
			start += cur;
			src += cur * mmc->write_bl_len;
		} while (blocks_todo > 0);
	}
	return blkcnt;
}

static ulong
mmc_berase(int dev_num, ulong start, lbaint_t blkcnt)
{
	int err = 0;
	struct mmc *mmc = find_mmc_device(dev_num);
	lbaint_t blk = 0, blk_r = 0;

	if (!mmc)
		return 0;

	if ((start % mmc->erase_grp_size) || (blkcnt % mmc->erase_grp_size))
		printf("\n\nCaution!  Your device's erase group is 0x%x\n"
		       "The erase range would be changed to 0x%lx~0x%x\n\n",
		       mmc->erase_grp_size, start & ~(mmc->erase_grp_size - 1),
		       (uint32_t)((start + blkcnt + mmc->erase_grp_size)
		       & ~(mmc->erase_grp_size - 1)) - 1);

	while (blk < blkcnt) {
		blk_r = ((blkcnt - blk) > mmc->erase_grp_size) ?
			mmc->erase_grp_size : (blkcnt - blk);
		err = mmc_erase_t(mmc, start + blk, blk_r);
		if (err)
			break;

		blk += blk_r;
	}

	return blk;
}

int mmc_register(struct mmc *mmc)
{
	mmc->block_dev.if_type = IF_TYPE_MMC;
	mmc->block_dev.dev = cur_dev_num++;
	mmc->block_dev.removable = 1;
	mmc->block_dev.block_read = mmc_bread;
	mmc->block_dev.block_write = mmc_bwrite;
	mmc->block_dev.block_erase = mmc_berase;
	if (!mmc->b_max)
		mmc->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;

	INIT_LIST_HEAD(&mmc->link);

	list_add_tail(&mmc->link, &mmc_devices);

	return 0;
}

void print_mmc_devices(char separator)
{
	struct mmc *m;
	struct list_head *entry;

        /* If nothing is available, try and initialize */
        if (list_empty(&mmc_devices))
                mmc_initialize(gd->bd);

        list_for_each(entry, &mmc_devices) {
		m = list_entry(entry, struct mmc, link);
		printf("%s: %d", m->name, m->block_dev.dev);
		if (entry->next != &mmc_devices)
			printf("%c ", separator);
	}
	printf("\n");
}

void print_mmc_device_info(int dev_num)
{
	struct mmc *mmc;
	struct mmc_host *host;
	const char *type;
	const char *version;
	uint32_t card_type;
	int prev = 0;
	int i;
	static const char *cbx_str[4] = {
		"Card (removable)",
		"BGA (Discrete embedded)",
		"POP",
		"Reserved"
	};

	mmc = find_mmc_device(dev_num);
	if (!mmc) {
		printf("MMC device %d not found\n", dev_num);
		return;
	}
	host = (struct mmc_host *)mmc->priv;
	card_type = host->ext_csd[EXT_CSD_CARD_TYPE];
	if (IS_SD(mmc)) {
		if (mmc->high_capacity)
			type = "SDHC or SDXC";
		else
			type = "SD";
	} else
		type = "MMC";

	switch (mmc->version) {
#ifdef CONFIG_OCTEON_MMC_SD
	case SD_VERSION_2:
		if (((mmc->scr[0] >> 24) & 0xf) == 2)
			version = "SD 3.0X or later";
		else
			version = "SD 2.00";
		break;
	case SD_VERSION_1_10:		version = "SD 1.10";		break;
	case SD_VERSION_1_0:		version = "SD 1.0";		break;
#endif
	case MMC_VERSION_4:
		switch (host->ext_csd[EXT_CSD_REV]) {
			case 0:		version = "MMC v4.0";		break;
			case 1:		version = "MMC v4.1";		break;
			case 2:		version = "MMC v4.2";		break;
			case 3:		version = "MMC v4.3";		break;
			case 4:		version = "MMC v4.4 (obsolete)";break;
			case 5:		version = "MMC v4.41";		break;
			case 6:		version = "MMC v4.5";		break;
			default:	version = "MMC > v4.5";		break;
		}
		break;
	case MMC_VERSION_3:		version = "MMC 3";		break;
	case MMC_VERSION_2_2:		version = "MMC 2.2";		break;
	case MMC_VERSION_1_4:		version = "MMC 1.4";		break;
	case MMC_VERSION_1_2:		version = "MMC 1.2";		break;
	case MMC_VERSION_UNKNOWN:	version = "MMC Unknown";	break;
	default:			version = "Unknown";		break;
	}

	printf("Name:                  %s\n", mmc->name);
	printf("Type:                  %s\n", type);
	printf("Version:               %s\n", version);
	printf("Manufacturer ID:       0x%02x\n", (mmc->cid[0] >> 24) & 0xff);
	printf("Device Type:           %s\n", cbx_str[(mmc->cid[0] >> 16) & 3]);
	printf("OEM ID:                0x%02x\n", (mmc->cid[0] >> 8) & 0xff);
	printf("Vendor:                %s\n", mmc->block_dev.vendor);
	printf("Product:               %s\n", mmc->block_dev.product);
	printf("Revision:              %s\n", mmc->block_dev.revision);
	printf("Manufacturing Date:    %d/%d\n",
	       (mmc->cid[3] >> 12) & 0xf, ((mmc->cid[3] >> 8) & 0xf) + 1997);
	printf("Capacity:              %llu bytes\n", mmc->capacity);
	printf("Read block length:     %u\n", mmc->read_bl_len);
	printf("Write block length:    %u\n", mmc->write_bl_len);
	printf("High capacity:         %s\n", mmc->high_capacity ? "yes" : "no");
	printf("Bus width:             %u bits\n", mmc->bus_width);
	printf("Bus frequency:         %u\n", mmc->clock);
	if (!mmc->card_caps & MMC_MODE_HS)
		printf("Transfer frequency:    %u\n", mmc->tran_speed);
	printf("Bus DDR:               %s\n",
	       ((mmc->host_caps & mmc->card_caps) & MMC_MODE_DDR) ? "yes" : "no");
	if (!IS_SD(mmc))
		printf("Erase group size:      %u\n", mmc->erase_grp_size);
	printf("Relative Card Address: 0x%x\n", mmc->rca);
	if (IS_SD(mmc)) {
		const char *sd_security;
		switch((mmc->scr[0] >> 20) & 7) {
		case 0:
			sd_security = "None";
			break;
		case 1:
			sd_security = "Not Used";
			break;
		case 2:
			sd_security = "SDSC Card (Security Version 1.01)";
			break;
		case 3:
			sd_security = "SDHC Card (Security Version 2.00)";
			break;
		case 4:
			sd_security = "SDXC Card (Security Version 3.xx)";
			break;
		default:
			sd_security = "Reserved/Unknown";
			break;
		}
		printf("SD Security:           %s\n", sd_security);
		if ((mmc->scr[0] >> 11) & 7)
			printf("SD Extended Security supported\n");

		if (mmc->scr[0] & 2)
			printf("SD Set Block Count (CMD23) supported\n");
		if (mmc->scr[0] & 1)
			printf("SDXC Speed Class Control supported\n");
	}
	if (card_type != 0) {
		puts("Supported bus speeds:    ");
		if (card_type & EXT_CSD_CARD_TYPE_26) {
			puts(" 26MHz");
			prev = 1;
		}
		if (card_type & EXT_CSD_CARD_TYPE_52) {
			if (prev)
				putc(',');
			puts(" 52MHz");
			prev = 1;
		}
		if (card_type & EXT_CSD_CARD_TYPE_DDR_1_8V) {
			if (prev)
				putc(',');
			puts(" DDR 1.8V");
			prev = 1;
		}
		if (card_type & EXT_CSD_CARD_TYPE_DDR_1_2V) {
			if (prev)
				putc(',');
			puts(" DDR 1.2V");
			prev = 1;
		}
	}
	if (!IS_SD(mmc)) {
		puts("\nExtended CSD register");
		for (i = 0; i < 512; i++) {
			if (i % 16 == 0)
				printf("\n0x%03x: ", i);
			if (i % 16 == 8)
				puts("- ");
			printf("%02x ", (uint32_t)host->ext_csd[i]);
		}
		puts("\n");
	}
}

int mmc_set_blocklen(struct mmc *mmc, int len)
{
	struct mmc_cmd cmd;
	int err;

	if (mmc->card_caps & MMC_MODE_DDR) {
		debug("MMC set block length skipped in DDR mode\n");
		return 0;
	}

	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = len;
	cmd.flags = 0;

	debug("%s: Setting block length to %d\n", __func__, len);
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err) {
		printf("%s: Error setting block length to %d\n", __func__, len);
	}
	return err;
}

int mmc_set_dev(int dev_num)
{
	cur_dev_num = dev_num;
	return 0;
}

int __board_mmc_getcd(struct mmc *mmc)
{
	return -1;
}
int board_mmc_getcd(struct mmc *mmc) __attribute__((weak,
	alias("__board_mmc_getcd")));

int mmc_legacy_init(int dev_num)
{
	struct mmc *mmc;
	int rc;

	mmc = find_mmc_device(dev_num);
	if (mmc == NULL) {
		printf("Error: could not find MMC device %d\n", dev_num);
		return -1;
	}
	rc = mmc_init(mmc);
	if (rc) {
		printf("Error: could not initialize MMC device %d\n", dev_num);
	}
	return rc;
}

/* Wait for a command to respond */
static int
oct_mmc_wait_cmd(int bus_id, int cmd_idx, int flags)
{
	cvmx_mio_emm_rsp_sts_t emm_rsp_sts;
	unsigned long base_time = get_timer(0);
	debug("%s: Waiting for bus_id %d, cmd idx %d\n",
	      __func__, bus_id, cmd_idx);
	ulong time = 0;

	do {
		emm_rsp_sts.u64 = cvmx_read_csr(CVMX_MIO_EMM_RSP_STS);
		if (emm_rsp_sts.s.cmd_done || emm_rsp_sts.s.rsp_timeout)
			break;
		WATCHDOG_RESET();
		udelay(1);
		time+=1;
	} while (get_timer(base_time) < WATCHDOG_COUNT);

	debug("%s: Response is 0x%llx after %lu ms (%lu loops)\n",
	      __func__, emm_rsp_sts.u64, get_timer(base_time), time);
	if (!emm_rsp_sts.s.cmd_done) {
		debug("%s: Wait for command index %d timed out\n",
		      __func__, cmd_idx);
		return TIMEOUT;
	}
	if (bus_id >= 0 && emm_rsp_sts.s.bus_id != bus_id) {
		debug("%s: Mismatch bus_id, expected %d for cmd idx %d, got %d\n",
		      __func__, bus_id, cmd_idx, emm_rsp_sts.s.bus_id);
		return -1;	/* Wrong bus ID */
	}
	if (emm_rsp_sts.s.rsp_timeout
	    || (emm_rsp_sts.s.rsp_crc_err &&
		!(flags & MMC_CMD_FLAG_IGNORE_CRC_ERR))
	    || emm_rsp_sts.s.rsp_bad_sts) {
		uint64_t status = cvmx_read_csr(CVMX_MIO_EMM_RSP_LO) >> 8;
		debug("%s: Bad response for bus id %d, cmd id %d:\n"
		      "\trsp_timeout: %d\n"
		      "\trsp_bad_sts: %d\n"
		      "\trsp_crc_err: %d\n",
		     __func__, bus_id, cmd_idx,
		     emm_rsp_sts.s.rsp_timeout,
		     emm_rsp_sts.s.rsp_bad_sts,
	             emm_rsp_sts.s.rsp_crc_err);
		if (emm_rsp_sts.s.rsp_type == 1 && emm_rsp_sts.s.rsp_bad_sts) {
			debug("\tResponse status: 0x%llx\n", status & 0xffffffff);
#ifdef DEBUG
			mmc_print_status(status);
#endif
		}
		return -1;
	}
	if (emm_rsp_sts.s.cmd_idx != cmd_idx) {
		debug("%s: response for bus id %d, cmd idx %d mismatch command index %d\n",
		      __func__, bus_id, cmd_idx, emm_rsp_sts.s.cmd_idx);
		return -1;
	}
	return 0;
}

#if 0
static int
mmc_wait_cmd(int bus_id, int cmd_idx)
{
	return oct_mmc_wait_cmd(bus_id, cmd_idx, 0);
}
#endif

static int
mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
	struct mmc_host *host = (struct mmc_host *)mmc->priv;
	cvmx_mio_emm_cmd_t emm_cmd;
	cvmx_mio_emm_buf_idx_t emm_buf_idx;
	cvmx_mio_emm_buf_dat_t emm_buf_dat;
	u64 resp_lo;
	u64 resp_hi;
	int i;
	int bus_id = host->bus_id;

	emm_cmd.u64 = 0;
	emm_cmd.s.cmd_val = 1;
	emm_cmd.s.bus_id = bus_id;
	emm_cmd.s.cmd_idx = cmd->cmdidx;
	emm_cmd.s.arg = cmd->cmdarg;
	emm_cmd.s.ctype_xor = (cmd->flags >> 16) & 3;
	emm_cmd.s.rtype_xor = (cmd->flags >> 20) & 7;
	emm_cmd.s.offset = (cmd->flags >> 24) & 0x3f;

	debug("mmc cmd: %d, arg: 0x%x flags: 0x%x reg: 0x%llx\n",
	      cmd->cmdidx, cmd->cmdarg, cmd->flags, emm_cmd.u64);

	if (data && data->flags & MMC_DATA_WRITE) {
		const char *src = data->src;
		if (!src) {
			printf("%s: Error, source buffer is NULL\n", __func__);
			return -1;
		}
		if (data->blocksize > 512) {
			printf("%s: Error: data size %u exceeds 512\n",
			       __func__, data->blocksize);
		}
		emm_buf_idx.u64 = 0;
		emm_buf_idx.s.inc = 1;
		cvmx_write_csr(CVMX_MIO_EMM_BUF_IDX, emm_buf_idx.u64);
		for (i = 0; i < (data->blocksize + 7) / 8; i++) {
			memcpy(&emm_buf_dat.u64, src, sizeof(emm_buf_dat));
			cvmx_write_csr(CVMX_MIO_EMM_BUF_DAT, emm_buf_dat.u64);
			debug("mmc cmd: buffer 0x%x: 0x%llx\n",
			      i*8, emm_buf_dat.u64);
			src += sizeof(emm_buf_dat);
		}
		debug("mmc cmd: wrote %d 8-byte blocks to buffer\n", i);
	}
	cvmx_write_csr(CVMX_MIO_EMM_CMD, emm_cmd.u64);

	if (oct_mmc_wait_cmd(bus_id, cmd->cmdidx, cmd->flags)) {
		if (!init_time)
			debug("mmc cmd: Error waiting for bus %d, command index %d to complete\n",
			       bus_id, cmd->cmdidx);
		return TIMEOUT;
	}
	debug("%s: Response flags: 0x%x\n", __func__, cmd->resp_type);
	if (!cmd->resp_type & MMC_RSP_PRESENT) {
		debug("%s: no response expected for command index %d, returning\n",
		      __func__, cmd->cmdidx);
		return 0;
	}

	resp_lo = cvmx_read_csr(CVMX_MIO_EMM_RSP_LO);
	if (cmd->resp_type & MMC_RSP_136) {
		resp_hi = cvmx_read_csr(CVMX_MIO_EMM_RSP_HI);
		debug("mmc cmd: response hi: 0x%016llx\n", resp_hi);
		cmd->response[0] = resp_hi >> 32;
		cmd->response[1] = resp_hi & 0xffffffff;
		cmd->response[2] = resp_lo >> 32;
		cmd->response[3] = resp_lo & 0xffffffff;
	} else if ((cmd->resp_type & MMC_RSP_CRC)
		   || (cmd->flags & MMC_CMD_FLAG_STRIP_CRC)) {	/* No CRC */
		cmd->response[0] = (resp_lo >> 8) & 0xffffffff;
	} else {
		cmd->response[0] = resp_lo & 0xffffffff;
	}
	debug("mmc cmd: response lo: 0x%016llx\n", resp_lo);
	if (data && data->flags & MMC_DATA_READ) {
		char *dest = data->dest;

		if (!dest) {
			printf("%s: Error, destination buffer NULL!\n",
			       __func__);
			return -1;
		}
		if (data->blocksize > 512) {
			printf("%s: Error: data size %u exceeds 512\n",
			       __func__, data->blocksize);
		}
		emm_buf_idx.u64 = 0;
		emm_buf_idx.s.inc = 1;
		cvmx_write_csr(CVMX_MIO_EMM_BUF_IDX, emm_buf_idx.u64);
		for (i = 0; i < (data->blocksize + 7) / 8; i++) {
			emm_buf_dat.u64 = cvmx_read_csr(CVMX_MIO_EMM_BUF_DAT);
			memcpy(dest, &emm_buf_dat.u64, sizeof(emm_buf_dat));
			       dest += sizeof(emm_buf_dat);
		}
	}
	return 0;
}

static int mmc_send_acmd(struct mmc *mmc, struct mmc_cmd *cmd,
			 struct mmc_data *data)
{
	struct mmc_cmd acmd;
	int err;

	acmd.cmdidx = MMC_CMD_APP_CMD;
	acmd.cmdarg = mmc->rca << 16;
	acmd.flags = 0;
	acmd.resp_type = MMC_RSP_R1;

	if (!IS_SD(mmc)) {
		debug("%s: Error, not SD card\n", __func__);
		return -1;
	}
	err = mmc_send_cmd(mmc, &acmd, NULL);
	if (err) {
		printf("%s: Error sending ACMD to SD card\n", __func__);
		return err;
	}
	if (cmd)
		return mmc_send_cmd(mmc, cmd, data);
	else
		return 0;
}

/* Change the bus width */
static void mmc_set_bus_width(struct mmc *mmc, uint width)
{
	mmc->bus_width = width;
}

static int mmc_go_idle(struct mmc* mmc)
{
	struct mmc_cmd cmd;
	int err;
	int i;

	for (i = 0; i < 5; i++) {
		/* Do this 5 times to clear the bus */
		cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_NONE;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;
	}
	udelay(20000);	/* Wait 20ms */
	return 0;
}

#ifdef CONFIG_OCTEON_MMC_SD
static int sd_send_relative_addr(struct mmc *mmc)
{
	int err;
	struct mmc_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_R6;
	cmd.flags = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err) {
		printf("%s: error sending command\n", __func__);
		return err;
	}
	mmc->rca = cmd.response[0] >> 16;
	debug("%s: SD RCA is %d (0x%x)\n", __func__, mmc->rca, mmc->rca);
	return 0;
}
#endif

static int mmc_set_relative_addr(struct mmc *mmc)
{
	struct mmc_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.cmdidx = MMC_CMD_SET_RELATIVE_ADDR;
	cmd.cmdarg = mmc->rca << 16;
	cmd.resp_type = MMC_RSP_R1;
	cmd.flags = 0;
	return mmc_send_cmd(mmc, &cmd, NULL);
}

static int mmc_select_card(struct mmc *mmc)
{
	struct mmc_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1b;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

static int mmc_all_send_cid(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));

	debug("%s: Getting CID\n", __func__);
	cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;
	cmd.flags = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err) {
		printf("%s: Error getting all CID\n", __func__);
		return err;
	}

	memcpy(mmc->cid, &cmd.response[0], 16);
#ifdef DEBUG
	debug("\tManufacturer: 0x%x\n", mmc->cid[0] >> 24);
	debug("\tDevice/BGA:   ");
	switch ((mmc->cid[0] >> 16) & 3) {
	case 0:
		debug("Device (removable)\n");
		break;
	case 1:
		debug("BGA (Discrete embedded)\n");
		break;
	case 2:
		debug("POP\n");
		break;
	default:
		debug("Reserved\n");
		break;
	}
	debug("\tOID:          0x%x\n", (mmc->cid[0] >> 8) & 0xFF);
	debug("\tProduct Name: %c%c%c%c%c%c\n", mmc->cid[0] & 0xff,
	      (mmc->cid[1] >> 24) & 0xff, (mmc->cid[1] >> 16) & 0xff,
	      (mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff,
	      (mmc->cid[2] >> 24) & 0xff);
	debug("\tProduct Revision: %d.%d\n",
	      (mmc->cid[2] >> 20) & 0xf, (mmc->cid[2] >> 16) & 0xf);
	debug("\tProduct Serial Number: 0x%x\n",
	      ((mmc->cid[2] & 0xffff) << 16) | ((mmc->cid[3] >> 16) & 0xffff));
	debug("\tManufacturing Date: %d/%d\n", (mmc->cid[3] >> 12) & 0xf,
	      ((mmc->cid[3] >> 8) & 0xf) + 1997);
#endif
	return 0;
}

static int mmc_get_csd(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));

	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err) {
		printf("%s: Error getting CSD\n", __func__);
		return err;
	}
	mmc->csd[0] = cmd.response[0];
	mmc->csd[1] = cmd.response[1];
	mmc->csd[2] = cmd.response[2];
	mmc->csd[3] = cmd.response[3];
	debug("%s: CSD: 0x%08x 0x%08x 0x%08x 0x%08x\n", __func__,
	      mmc->csd[0], mmc->csd[1], mmc->csd[2], mmc->csd[3]);
	return 0;
}

#ifdef CONFIG_OCTEON_MMC_SD
static int sd_set_bus_width_speed(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int err;

	if (mmc->card_caps & MMC_MODE_4BIT) {
		/* Set SD bus width to 4 */
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 2;
		cmd.flags = 0;

		err = mmc_send_acmd(mmc, &cmd, NULL);
		if (err) {
			printf("%s: Error setting bus width\n", __func__);
			return err;
		}
		mmc_set_bus_width(mmc, 4);
	}
	if (mmc->card_caps & MMC_MODE_HS)
		mmc_set_clock(mmc, 50000000);
	else
		mmc_set_clock(mmc, 25000000);
	return sd_set_ios(mmc);
}
#endif

static int mmc_set_bus_width_speed(struct mmc *mmc)
{
		debug("%s: card caps: 0x%x\n", __func__, mmc->card_caps);
		if (mmc->card_caps & MMC_MODE_8BIT) {
			debug("%s: Set bus width to 8 bits\n", __func__);
			mmc_set_bus_width(mmc, 8);
		} else if (mmc->card_caps & MMC_MODE_4BIT) {
			debug("%s: Set bus width to 4 bits\n", __func__);
			/* Set the card to use 4 bit */
			mmc_set_bus_width(mmc, 4);
		}
		if (mmc->card_caps & MMC_MODE_HS) {
			debug("%s: Set high-speed mode\n", __func__);
			if (mmc->card_caps & MMC_MODE_HS_52MHz) {
				debug("%s: Set clock speed to 52MHz\n",
				      __func__);
				mmc_set_clock(mmc, 52000000);
			} else {
				mmc_set_clock(mmc, 26000000);
				debug("%s: Set clock speed to 26MHz\n",
				      __func__);
			}
		} else {
			debug("%s: Set clock speed to 20MHz\n", __func__);
			mmc_set_clock(mmc, 20000000);
		}
		mmc_set_ios(mmc);
		return 0;
}

#ifdef CONFIG_OCTEON_MMC_SD
int sd_send_op_cond(struct mmc *mmc)
{
	int timeout = 1000;
	int err;
	struct mmc_cmd cmd;

	debug("In %s\n", __func__);
	do {
		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.flags = MMC_CMD_FLAG_RTYPE_XOR(3) | MMC_CMD_FLAG_STRIP_CRC;
		/*
		 * Most cards do not answer if some reserved bits
		 * in the ocr are set.  However, some controllers
		 * can set bit 7 (reserved low voltages), but
		 * how to manage low voltage SD cards is not yet
		 * specified.
		 */
		cmd.cmdarg = mmc->voltages & 0xff8000;

		if (mmc->version == SD_VERSION_2) {
			cmd.cmdarg |= OCR_HCS;
			debug("%s: SD 2.0 compliant card\n", __func__);
		}

		err = mmc_send_acmd(mmc, &cmd, NULL);
		if (err) {
			debug("%s: Error sending SD command, might be MMC\n",
			      __func__);
			return err;
		}

		debug("%s response: 0x%x\n", __func__, cmd.response[0]);
		udelay(1000);
	} while ((!(cmd.response[0] & OCR_BUSY)) && timeout--);

	if (timeout <= 0) {
		printf("%s: Timed out\n", __func__);
		return TIMEOUT;
	}

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	mmc->ocr = cmd.response[0];
	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 0;

	debug("%s: MMC high capacity mode %sdetected.\n",
	      __func__, mmc->high_capacity ? "" : "NOT ");
	return 0;
}
#endif

int mmc_send_op_cond(struct mmc *mmc)
{
	int timeout = 2000;
	struct mmc_cmd cmd;
	int err;

	mmc_go_idle(mmc);
	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
	cmd.resp_type = MMC_RSP_R3;
	cmd.cmdarg = 0;
	cmd.flags = MMC_CMD_FLAG_STRIP_CRC;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	udelay(20000);

	do {
		cmd.cmdidx = MMC_CMD_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = OCR_HCS | mmc->voltages;
		cmd.flags = MMC_CMD_FLAG_STRIP_CRC;

		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err) {
			if (!init_time)
				debug("%s: Returned %d\n", __func__, err);
			return err;
		}
		debug("%s: response 0x%x\n", __func__, cmd.response[0]);
		if (cmd.response[0] & OCR_BUSY)
			break;
		udelay(1000);
	} while (timeout--);

	if (timeout <= 0) {
		printf("%s: Timed out!", __func__);
		return TIMEOUT;
	}

	mmc->version = MMC_VERSION_UNKNOWN;
	mmc->ocr = cmd.response[0];

	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);

#ifdef DEBUG
	debug("%s: OCR: 0x%x\n", __func__, mmc->ocr);
	if (mmc->ocr & 0x80)
		debug("\t1.70-1.95V\n");
	if (mmc->ocr & 0x3f00)
		debug("\t2.0-2.6V\n");
	if (mmc->ocr & 0x007f8000)
		debug("\t2.7-3.6V\n");
	debug("\tAccess Mode: %s\n",
	      (mmc->ocr & 0x40000000) == 0x40000000 ? "sector" : "byte");
#endif
	return 0;
}

/* Get the extended CSD register */
int mmc_send_ext_csd(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	struct mmc_host *host = (struct mmc_host *)mmc->priv;
	int err;
	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;
	if (host->have_ext_csd) {
		debug("%s: Already have ext_csd\n", __func__);
#ifdef DEBUG
		int i;
		for (i = 0; i < 512; i++) {
			if (i % 16 == 0)
				printf("\n0x%03x: ", i);
			if (i % 16 == 8)
				puts("- ");
			printf("%02x ", (uchar)host->ext_csd[i]);
		}
		puts("\n");
#endif
		return 0;
	}
	mmc_set_blocklen(mmc, 512);
	data.dest = (char *)host->ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;
	debug("In %s\n", __func__);

	err = mmc_send_cmd(mmc, &cmd, &data);

	if (err) {
		printf("%s: Error getting extended CSD\n", __func__);
	} else {
		debug("%s: Got good response\n", __func__);
		host->have_ext_csd = 1;
#ifdef DEBUG
		int i;
		for (i = 0; i < 512; i++) {
			if (i % 16 == 0)
				printf("\n0x%03x: ", i);
			if (i % 16 == 8)
				puts("- ");
			printf("%02x ", (uchar)host->ext_csd[i]);
		}
		puts("\n");
#endif
	}
	return err;
}

/* NOTE: We don't use this function since OCTEON handles this in hardware */
int mmc_switch(struct mmc *mmc, u8 set, u8 index, u8 value)
{
	struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_SWITCH;
	cmd.resp_type = MMC_RSP_R1b;
	cmd.cmdarg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		(index << 16) |
		(value << 8);
	cmd.flags = 0;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

int mmc_switch_part(int dev_num, unsigned int part_num)
{
	struct mmc *mmc = find_mmc_device(dev_num);
	if (!mmc)
		return -1;

	return mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PART_CONF,
			  (mmc->part_config & ~PART_ACCESS_MASK)
			  | (part_num & PART_ACCESS_MASK));
}
#ifdef CONFIG_OCTEON_MMC_SD
static int sd_set_ios(struct mmc *mmc)
{
	cvmx_mio_emm_switch_t emm_switch;
	struct mmc_host *host = (struct mmc_host *)mmc->priv;
	int clock = mmc->clock;
	int clk_period;

	debug("%s: clock: %d (max %d), width %d\n",
	      __func__, clock, mmc->f_max, mmc->bus_width);
	if (mmc->bus_width > 4)
		mmc->bus_width = 4;
	clk_period = (cvmx_clock_get_rate(CVMX_CLOCK_SCLK) + clock - 1) / clock;
	emm_switch.u64 = 0;
	emm_switch.s.bus_id = host->bus_id;
	emm_switch.s.hs_timing = (mmc->card_caps & MMC_MODE_HS) ? 1 : 0;
	/* No DDR for now */
	emm_switch.s.bus_width = (mmc->bus_width == 4) ? 1 : 0;
	emm_switch.s.clk_hi = (clk_period + 1) / 2;
	emm_switch.s.clk_lo = (clk_period + 1) / 2;
	emm_switch.s.power_class = 10;
	debug("%s: Writing emm_switch value 0x%llx\n",
	      __func__, emm_switch.u64);
	cvmx_write_csr(CVMX_MIO_EMM_SWITCH, emm_switch.u64);
	udelay(20000);
#ifdef DEBUG
	emm_switch.u64 = cvmx_read_csr(CVMX_MIO_EMM_SWITCH);
	debug("%s: emm_switch now 0x%llx\n", __func__, emm_switch.u64);
#endif
	return 0;
}
#endif

static void mmc_set_ios(struct mmc *mmc)
{
	cvmx_mio_emm_switch_t emm_switch;
	cvmx_mio_emm_rsp_sts_t emm_sts;
	cvmx_mio_emm_wdog_t emm_wdog;
	int switch_timeout_ms = 2550;
	struct mmc_host *host = (struct mmc_host *)mmc->priv;
	int err;
	int timeout = 2000;
	int clk_period;
	char cardtype;
	int hs_timing = 0;
	int ddr = 0;
	int bus_width;
	int power_class = 10;
	int clock = mmc->clock;

	debug("In %s\n", __func__);
	debug("Starting clock is %uHz\n", clock);
	mmc->card_caps = 0;

	/* Only version 4 supports high speed */
	if (mmc->version < MMC_VERSION_4)
		return;

	if (clock == 0) {
		puts("mmc switch: Error, clock is zero!\n");
		return;
	}

	err = mmc_send_ext_csd(mmc);
	if (err) {
		printf("%s: Error getting ext csd\n", __func__);
		return;
	}
	cardtype = host->ext_csd[EXT_CSD_CARD_TYPE] & 0x3f;
	debug("%s: card type flags (device_type): 0x%x\n", __func__, cardtype);
	if (cardtype & EXT_CSD_CARD_TYPE_26) {
		hs_timing = 1;
		debug("\tHigh-Speed eMMC 26MHz at rated device voltage(s)\n");
	}
	if (cardtype & EXT_CSD_CARD_TYPE_52) {
		debug("\tHigh-Speed eMMC 52MHz at rated device voltage(s)\n");
		mmc->card_caps |= MMC_MODE_HS_52MHz;
		hs_timing = 1;
	}
	if (cardtype & EXT_CSD_CARD_TYPE_DDR_1_8V) {
		debug("\tHigh-Speed DDR eMMC 52MHz at 1.8V or 3V I/O\n");
		hs_timing = 1;
		if ((mmc->voltages & MMC_VDD_165_195)
		    || getenv("octeon_mmc_ddr"))
			ddr = 1;
	}
	if (cardtype & EXT_CSD_CARD_TYPE_DDR_1_2V) {
		debug("\tHigh-Speed DDR eMMC 52MHz at 1.2V I/O\n");
		hs_timing = 1;
		/* DDR only works at 1.2V which OCTEON doesn't support */
		ddr = 0;
	}
	if (cardtype & (1 << 4))
		debug("\tHS200 Single Data Rate eMMC 200MHz at 1.8V I/O\n");
	if (cardtype & (1 << 5))
		debug("\tHS200 Single Data Rate eMMC 200MHz at 1.2V I/O\n");

	switch (mmc->bus_width) {
	case 8:
		if (ddr)
			bus_width = EXT_CSD_DDR_BUS_WIDTH_8;
		else
			bus_width = EXT_CSD_BUS_WIDTH_8;
		break;
	case 4:
		if (ddr)
			bus_width = EXT_CSD_DDR_BUS_WIDTH_4;
		else
			bus_width = EXT_CSD_BUS_WIDTH_4;
		break;
	case 1:
		bus_width = EXT_CSD_BUS_WIDTH_1;
		break;
	default:
		printf("%s: Unknown bus width %d\n", __func__, mmc->bus_width);
		return;
	}

	if (hs_timing) {
		if (cardtype & MMC_HS_52MHZ) {
			mmc->card_caps |= MMC_MODE_HS_52MHz | MMC_MODE_HS;
			clock = 52000000;
			debug("High-speed 52MHz timing mode detected\n");
		} else {
			mmc->card_caps |= MMC_MODE_HS;
			clock = 26000000;
			debug("High-speed 26MHz timing mode detected\n");
		}
		if (ddr)
			mmc->card_caps |= MMC_MODE_DDR;
	} else {
		if (mmc->tran_speed)
			clock = min(clock, mmc->tran_speed);
		else
			clock = min(clock, 20000000);
		debug("High-speed clock mode NOT detected, setting to %dhz\n",
		      clock);
	}
	mmc_set_clock(mmc, clock);
	debug("%s: Clock set to %u Hz\n", __func__, mmc->clock);

	switch_timeout_ms = host->ext_csd[EXT_CSD_GENERIC_CMD6_TIME] * 10;
	if (switch_timeout_ms == 0) {
		switch_timeout_ms = 2550;
		debug("extended CSD generic cmd6 timeout not specified\n");
	} else
		debug("extended CSD generic cmd6 timeout %d ms\n",
		      switch_timeout_ms);
	clk_period =
		(cvmx_clock_get_rate(CVMX_CLOCK_SCLK) + mmc->clock - 1) / mmc->clock;

	debug("%s: Setting clock period to %d for MMC clock: %d, hs: %d\n",
	      __func__, clk_period, mmc->clock, hs_timing);

again:
	emm_switch.u64 = 0;
	emm_switch.s.bus_id = host->bus_id;
	emm_switch.s.switch_exe = 1;
	emm_switch.s.hs_timing = hs_timing;
	emm_switch.s.bus_width = bus_width;
	emm_switch.s.power_class = power_class;
	emm_switch.s.clk_hi = (clk_period + 1) / 2;
	emm_switch.s.clk_lo = (clk_period + 1) / 2;

	debug("%s: Writing 0x%llx to mio_emm_switch\n",
	      __func__, emm_switch.u64);
	cvmx_write_csr(CVMX_MIO_EMM_SWITCH, emm_switch.u64);
	do {
		emm_sts.u64 = cvmx_read_csr(CVMX_MIO_EMM_RSP_STS);
		if (!emm_sts.s.switch_val)
			break;
		udelay(100);
	} while (timeout-- > 0);
	if (timeout <= 0) {
		printf("%s: switch command timed out, status=0x%llx\n",
		       __func__, emm_sts.u64);
		return;
	}

	emm_switch.u64 = cvmx_read_csr(CVMX_MIO_EMM_SWITCH);
	debug("Switch command response: 0x%llx, switch: 0x%llx\n",
	      emm_sts.u64, emm_switch.u64);
#if defined(DEBUG)
	cvmx_mio_emm_rsp_lo_t emm_rsp_lo;
	emm_rsp_lo.u64 = cvmx_read_csr(CVMX_MIO_EMM_RSP_LO);
	debug("Switch response lo: 0x%llx\n", emm_rsp_lo.u64);
#endif

	if (emm_switch.s.switch_err0) {
		/* Error while performing POWER_CLASS switch */
		if (power_class > 0) {
			power_class--;
			debug("%s: Trying power class %d\n",
			      __func__, power_class);
			goto again;
		}
		printf("%s: Error setting power class\n", __func__);
		return;
	}
	if (emm_switch.s.switch_err1) {
		/* Error while performing HS_TIMING switch */
		if (ddr) {
			ddr = 0;
			debug("%s: Turning off DDR mode\n", __func__);
			mmc->card_caps &= ~MMC_MODE_DDR;
			if (bus_width == EXT_CSD_DDR_BUS_WIDTH_8)
				bus_width = EXT_CSD_BUS_WIDTH_8;
			else if (bus_width == EXT_CSD_DDR_BUS_WIDTH_4)
				bus_width = EXT_CSD_BUS_WIDTH_4;
			goto again;
		}
		if (hs_timing) {
			if (clock > 26000000) {
				clock = 26000000;
				debug("%s: Reducing clock to 26MHz\n", __func__);
			} else {
				hs_timing = 0;
				debug("%s: Turning off high-speed timing\n",
				      __func__);
			}
			goto again;
		}
		printf("%s: Error setting hs timing\n", __func__);
		return;
	}

	/* Test bus width */
	if (!emm_switch.s.switch_err2 &&
	    (bus_width != EXT_CSD_DDR_BUS_WIDTH_8) &&
	    (bus_width != EXT_CSD_DDR_BUS_WIDTH_4)) {
		/* Width succeeded, test the bus */
		struct mmc_cmd mmc_cmd;
		struct mmc_data mmc_data;
		uint8_t buffer[8];
		debug("Testing bus width %d (%d)\n", mmc->bus_width, bus_width);
		mmc_data.src = (char *)buffer;
		mmc_data.blocks = 1;
		mmc_data.blocksize = sizeof(buffer);
		mmc_data.flags = MMC_DATA_WRITE;

		switch (mmc->bus_width) {
		case 8:
			buffer[0] = 0x55;
			buffer[1] = 0xaa;
			buffer[2] = 0x00;
			buffer[3] = 0x00;
			buffer[4] = 0x00;
			buffer[5] = 0x00;
			buffer[6] = 0x00;
			buffer[7] = 0xff;
			break;
		case 4:
			buffer[0] = 0x5a;
			buffer[1] = 0x00;
			buffer[2] = 0x00;
			buffer[3] = 0x00;
			buffer[4] = 0x00;
			buffer[5] = 0x00;
			buffer[6] = 0x00;
			buffer[7] = 0x0f;
			break;
		case 1:
			buffer[0] = 0x80;
			buffer[1] = 0x00;
			buffer[2] = 0x00;
			buffer[3] = 0x00;
			buffer[4] = 0x00;
			buffer[5] = 0x00;
			buffer[6] = 0x00;
			buffer[7] = 0x01;
			break;
		default:
			printf("Unknown bus width %d\n", mmc->bus_width);
			return;
		}

#ifdef DEBUG
		int i;
		for (i = 0; i < sizeof(buffer); i++) {
			if (i % 16 == 0)
				printf("\n0x%03x: ", i);
			if (i % 16 == 8)
				puts("- ");
			printf("%02x ", buffer[i]);
		}
		puts("\n");
#endif
		mmc_cmd.cmdarg = 0;
		mmc_cmd.cmdidx = 19;	/* BUSTEST_W */
		mmc_cmd.resp_type = MMC_RSP_R1;
		mmc_cmd.flags = MMC_CMD_FLAG_CTYPE_XOR(0);
		mmc_cmd.flags |= MMC_CMD_FLAG_OFFSET(63);
		if (mmc_send_cmd(mmc, &mmc_cmd, &mmc_data) != 0) {
			printf("Warning: problem sending BUSTEST_W command\n");
		}
		debug("BUSTEST_W response is 0x%x 0x%x 0x%x 0x%x\n",
		      mmc_cmd.response[0], mmc_cmd.response[1],
		      mmc_cmd.response[2], mmc_cmd.response[3]);
		memset(buffer, 0, sizeof(buffer));
		mmc_cmd.cmdarg = 0;
		mmc_cmd.cmdidx = 14;	/* BUSTEST_R */
		mmc_cmd.resp_type = MMC_RSP_R1;
		mmc_cmd.flags = MMC_CMD_FLAG_OFFSET(63);
		memset(buffer, 0, sizeof(buffer));
		mmc_data.dest = (char *)buffer;
		mmc_data.blocks = 1;
		mmc_data.blocksize = sizeof(buffer);
		mmc_data.flags = MMC_DATA_READ;
		if (mmc_send_cmd(mmc, &mmc_cmd, &mmc_data) != 0) {
			printf("Warning: problem sending BUSTEST_R command\n");
		}
		debug("BUSTEST_R response is 0x%x %x %x %x\n",
		      mmc_cmd.response[0], mmc_cmd.response[1],
		      mmc_cmd.response[2], mmc_cmd.response[3]);
#ifdef DEBUG
		for (i = 0; i < sizeof(buffer); i++) {
			if (i % 16 == 0)
				printf("\n0x%03x: ", i);
			if (i % 16 == 8)
				puts("- ");
			printf("%02x ", buffer[i]);
		}
		puts("\n");
#endif
		switch (bus_width) {
		case EXT_CSD_DDR_BUS_WIDTH_8:
			if (buffer[0] != 0xaa || buffer[1] != 0x55) {
				debug("DDR Bus width 8 test failed, returned "
				      "0x%02x%02x, expected 0xAA55, trying "
				      "bus width 8\n",
				      buffer[0], buffer[1]);
				bus_width = EXT_CSD_DDR_BUS_WIDTH_4;
				mmc->bus_width = 4;
				goto again;
			}
			break;
		case EXT_CSD_DDR_BUS_WIDTH_4:
			if (buffer[0] != 0xa5) {
				debug("DDR Bus width 4 test failed, returned "
				      "0x%02x%02x, expected 0xA5, trying "
				      "bus width %d\n",
				      buffer[0], buffer[1], host->max_width);
				bus_width = (host->max_width == 8) ?
					EXT_CSD_BUS_WIDTH_8
					: EXT_CSD_BUS_WIDTH_4;
				mmc->bus_width = host->max_width;
				goto again;
			}
			break;
		case EXT_CSD_BUS_WIDTH_8:
			if (buffer[0] != 0xaa || buffer[1] != 0x55) {
				debug("Bus width 8 test failed, returned "
				      "0x%02x%02x, expected 0xAA55, trying bus width 4\n",
				      buffer[0], buffer[1]);
				bus_width = EXT_CSD_BUS_WIDTH_4;
				mmc->bus_width = 4;
				goto again;
			}
			break;
		case EXT_CSD_BUS_WIDTH_4:
			if (buffer[0] != 0xa5) {
				debug("DDR bus width 4 test failed, returned "
				      "0x%02x, expected 0xA5, trying bus width 1\n",
				      buffer[0]);
				bus_width = EXT_CSD_BUS_WIDTH_1;
				mmc->bus_width = 1;
				goto again;
			}
			break;
		case EXT_CSD_BUS_WIDTH_1:
			if ((buffer[0] & 0xc0) != 0x40) {
				debug("DDR bus width 1 test failed, returned "
				      "0x%02x, expected 0x4x, trying bus width 1\n",
				      buffer[0]);
				return;
			}
			break;
		default:
			break;
		}
	}

	if (emm_switch.s.switch_err2) {
		/* Error while performing BUS_WIDTH switch */
		switch (bus_width) {
		case EXT_CSD_DDR_BUS_WIDTH_8:
			debug("DDR bus width 8 failed, trying DDR bus width 4\n");
			bus_width = EXT_CSD_DDR_BUS_WIDTH_4;
			goto again;
		case EXT_CSD_DDR_BUS_WIDTH_4:
			debug("DDR bus width 4 failed, trying bus width %d\n",
			      host->max_width);
			bus_width = (host->max_width == 8) ?
				EXT_CSD_BUS_WIDTH_8 : EXT_CSD_BUS_WIDTH_4;
			goto again;
		case EXT_CSD_BUS_WIDTH_8:
			debug("Bus width 8 failed, trying bus width 4\n");
			bus_width = EXT_CSD_BUS_WIDTH_4;
			goto again;
		case EXT_CSD_BUS_WIDTH_4:
			debug("Bus width 4 failed, trying bus width 1\n");
			bus_width = EXT_CSD_BUS_WIDTH_1;
			goto again;
		default:
			printf("%s: Could not set bus width\n", __func__);
			return;
		}
	}

	if (ddr)
		mmc->card_caps |= MMC_MODE_DDR;
	/* Set watchdog for command timeout */
	emm_wdog.u64 = 0;
	emm_wdog.s.clk_cnt = 256000 + (8 * mmc->clock) / 10;
	cvmx_write_csr(CVMX_MIO_EMM_WDOG, emm_wdog.u64);

	debug("%s: Set hs: %d, clock: %u, bus width: %d, power class: %d, watchdog: %u\n",
	      __func__, hs_timing, mmc->clock, bus_width, power_class,
	      emm_wdog.s.clk_cnt);
	return;
}

/* Set the clock speed.
 *
 * NOTE: The clock speed will be limited to the maximum supported clock speed.
 */
void mmc_set_clock(struct mmc *mmc, uint clock)
{
	debug("%s: min: %u, max: %u, trans: %u, hs: %u, set: %u\n",
	      __func__, mmc->f_min, mmc->f_max, mmc->tran_speed,
	      (mmc->card_caps & MMC_MODE_HS) ? 1 : 0, clock);
	clock = min(clock, mmc->f_max);
	clock = max(clock, mmc->f_min);
	if (mmc->tran_speed && !(mmc->card_caps & MMC_MODE_HS)) {
		clock = min(clock, mmc->tran_speed);
		debug("%s: Limiting clock to trans speed %u\n",
		      __func__, mmc->tran_speed);
	}
	debug("%s: Setting clock to %uHz\n", __func__, clock);
	mmc->clock = clock;
}

#ifdef CONFIG_OCTEON_MMC_SD
int sd_switch(struct mmc *mmc, int mode, int group, u8 value, u8 *resp)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int err;

	/* Switch the frequency */
	cmd.cmdidx = SD_CMD_SWITCH_FUNC;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = (mode << 31) | 0xffffff;
	cmd.cmdarg &= ~(0xf << (group * 4));
	cmd.cmdarg |= value << (group * 4);
	cmd.flags = MMC_CMD_FLAG_CTYPE_XOR(1);

	data.dest = (char *)resp;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);
	if (err) {
		printf("%s: failed, rc: %d\n", __func__, err);
		return err;
	}
	memcpy(resp, &cmd.response[0], sizeof(cmd.response));
	return 0;
}

int sd_change_freq(struct mmc *mmc)
{
	int err;
	struct mmc_cmd cmd;
	uint32_t scr[2];
	uint32_t switch_status[16];
	struct mmc_data data;
	int timeout;

	debug("In %s\n", __func__);

#ifdef DEBUG
	memset(scr, 0x55, sizeof(scr));
	memset(switch_status, 0xaa, sizeof(switch_status));
#endif
	mmc->card_caps = 0;

	cmd.cmdidx = SD_CMD_APP_SEND_SCR;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;
	cmd.flags = MMC_CMD_FLAG_RTYPE_XOR(1) | MMC_CMD_FLAG_CTYPE_XOR(1)
		    | MMC_CMD_FLAG_OFFSET(63);

	timeout = 3;

retry_scr:
	data.dest = (char *)&scr;
	data.blocksize = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_acmd(mmc, &cmd, &data);

	if (err) {
		debug("Retrying send SCR\n");
		if (timeout--)
			goto retry_scr;

		return err;
	}

	mmc->scr[0] = __be32_to_cpu(scr[0]);
	mmc->scr[1] = __be32_to_cpu(scr[1]);

	debug("%s: SCR=0x%08x 0x%08x\n", __func__, mmc->scr[0], mmc->scr[1]);
	switch ((mmc->scr[0] >> 24) & 0xf) {
		case 0:
			mmc->version = SD_VERSION_1_0;
			break;
		case 1:
			mmc->version = SD_VERSION_1_10;
			break;
		case 2:
			mmc->version = SD_VERSION_2;
			break;
		default:
			mmc->version = SD_VERSION_1_0;
			break;
	}

	/* Version 1.0 doesn't support switching */
	if (mmc->version == SD_VERSION_1_0) {
		debug("%s: Returning for SD version 1.0\n", __func__);
		return 0;
	}

	timeout = 4;
	while (timeout--) {
		err = sd_switch(mmc, SD_SWITCH_CHECK, 0, 1,
				(u8 *)&switch_status);

		if (err) {
			debug("%s: Error calling sd_switch\n", __func__);
			return err;
		}

		/* The high-speed function is busy.  Try again */
		if (!(__be32_to_cpu(switch_status[7]) & SD_HIGHSPEED_BUSY)) {
			debug("%s: high speed function is !busy, done\n", __func__);
			break;
		}
		udelay(1000);
	}

#ifdef DEBUG
	int i;
	for (i = 0; i < 16; i++) {
		if (!(i & 3))
			debug("\n%02x: ", i * 4);
		debug("%08x ", switch_status[i]);
	}
	puts("\n");
#endif

	if (mmc->scr[0] & SD_DATA_4BIT) {
		mmc->card_caps |= MMC_MODE_4BIT;
		debug("%s: SD 4 bit mode detected\n", __func__);
	}
#ifdef DEBUG
	debug("\tmax current: %u ma\n", switch_status[0] >> 16);
	if (switch_status[0] & 0xffff) {
		debug("\tGroup 6 functions supported: ");
		for (i = 0; i < 16; i++)
			if ((switch_status[0] >> i) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[1] & 0xffff0000) {
		debug("\tGroup 5 functions supported: ");
		for (i = 0; i < 16; i++)
			if ((switch_status[1] >> (i + 16)) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[1] & 0xffff) {
		debug("\tGroup 4 functions supported: ");
		for (i = 0; i < 16; i++)
			if ((switch_status[1] >> i) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[2] & 0xffff0000) {
		debug("\tGroup 3 functions supported: ");
		for (i = 0; i < 16; i++)
			if ((switch_status[2] >> (i + 16)) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[2] & 0x0000ffff) {
		debug("\tGroup 2 functions supported: ");
		for (i = 0; i < 16; i++)
			if ((switch_status[1] >> i) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[3] & 0xffff0000) {
		debug("\tGroup 1 functions supported: ");
		for (i = 0; i < 16; i++)
			if ((switch_status[2] >> (i + 16)) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[3] & 0x0000f000) {
		debug("\tGroup 6 functions selected: ");
		for (i = 0; i < 4; i++)
			if ((switch_status[2] >> (i+12)) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[3] & 0x00000f00) {
		debug("\tGroup 5 functions selected: ");
		for (i = 0; i < 4; i++)
			if ((switch_status[2] >> (i + 8)) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[3] & 0x000000f0) {
		debug("\tGroup 4 functions selected: ");
		for (i = 0; i < 4; i++)
			if ((switch_status[2] >> (i+4)) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[3] & 0x0000000f) {
		debug("\tGroup 3 functions selected: ");
		for (i = 0; i < 4; i++)
			if ((switch_status[2] >> i) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[4] & 0xf0000000) {
		debug("\tGroup 2 functions selected: ");
		for (i = 0; i < 4; i++)
			if ((switch_status[4] >> (i+28)) & 1)
				debug("%i ", i);
		debug("\n");
	}
	if (switch_status[4] & 0x0f000000) {
		debug("\tGroup 1 functions selected: ");
		for (i = 0; i < 4; i++)
			if ((switch_status[4] >> (i+24)) & 1)
				debug("%i ", i);
		debug("\n");
	}
	debug("\tData structure version: %d\n", (switch_status[4] >> 16) & 0xff);
#endif
	if (!(__be32_to_cpu(switch_status[3] & SD_HIGHSPEED_SUPPORTED))) {
		debug("%s: high speed mode not supported\n", __func__);
	} else {
		debug("%s: high speed mode supported\n", __func__);
	}

	err = sd_switch(mmc, SD_SWITCH_SWITCH, 0, 1, (u8 *)&switch_status);

	if (err) {
		debug("%s: switch failed\n", __func__);
		return err;
	}

#ifdef DEBUG
	for (i = 0; i < 16; i++) {
		if (!(i & 3))
			debug("\n%02x: ", i * 4);
		debug("%08x ", switch_status[i]);
	}
	puts("\n");
#endif
	if ((__be32_to_cpu(switch_status[4]) & 0x0f000000) == 0x01000000) {
		mmc->card_caps |= MMC_MODE_HS;
		debug("%s: High speed mode supported\n", __func__);
	}

	return 0;
}
#endif

int mmc_send_if_cond(struct mmc *mmc)
{
#ifdef CONFIG_OCTEON_MMC_SD
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	/* We set the bit if the host supports voltages between 2.7 and 3.6 V */
	cmd.cmdarg = ((mmc->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;
	cmd.flags = MMC_CMD_FLAG_CTYPE_XOR(1) | MMC_CMD_FLAG_RTYPE_XOR(2);

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err) {
		debug("%s failed\n", __func__);
		return err;
	}

	if ((cmd.response[0] & 0xff) != 0xaa) {
		debug("%s: Unusable error, response is 0x%x\n",
		      __func__, cmd.response[0]);
		return UNUSABLE_ERR;
	} else {
		mmc->version = SD_VERSION_2;
		debug("%s: SD version 2 detected\n", __func__);
	}
#endif
	return 0;
}

void mmc_reset_bus(struct mmc *mmc, int preserve_switch)
{
	struct mmc_host *host = (struct mmc_host *)mmc->priv;
	cvmx_mio_emm_cfg_t emm_cfg;
	cvmx_mio_emm_switch_t emm_switch;
	cvmx_mio_emm_sts_mask_t emm_sts_mask;

	emm_cfg.u64 = cvmx_read_csr(CVMX_MIO_EMM_CFG);
	if (preserve_switch)
		emm_switch.u64 = cvmx_read_csr(CVMX_MIO_EMM_SWITCH);

	/* Reset the bus */
	emm_cfg.u64 &= ~(1 << host->bus_id);
	cvmx_write_csr(CVMX_MIO_EMM_CFG, emm_cfg.u64);
	udelay(20000);	/* Wait 20ms */
	emm_cfg.u64 |= 1 << host->bus_id;
	cvmx_write_csr(CVMX_MIO_EMM_CFG, emm_cfg.u64);

	udelay(20000);

	emm_sts_mask.u64 = 0;
	emm_sts_mask.s.sts_msk = 0x80;
	cvmx_write_csr(CVMX_MIO_EMM_STS_MASK, emm_sts_mask.u64);

	/* Restore switch settings */
	if (preserve_switch) {
		emm_switch.s.switch_exe = 0;
		emm_switch.s.switch_err0 = 0;
		emm_switch.s.switch_err1 = 0;
		emm_switch.s.switch_err2 = 0;
		cvmx_write_csr(CVMX_MIO_EMM_SWITCH, emm_switch.u64);
		udelay(100000);
	}
}

int mmc_startup(struct mmc *mmc)
{
	struct mmc_host *host = (struct mmc_host *)mmc->priv;
	u64 cmult, csize, capacity;
	int err;
	uint mult, freq;
	cvmx_mio_emm_switch_t emm_switch;
	cvmx_mio_emm_wdog_t emm_wdog;

	/* frequency bases */
	/* divided by 10 to be nice to platforms without floating point */
	static const int fbase[] = {
		10000,
		100000,
		1000000,
		10000000,
		0, 0, 0, 0
	};

	/* Multiplier values for TRAN_SPEED.  Multiplied by 10 to be nice
	* to platforms without floating point.
	*/
	static const int multipliers[] = {
		0,	/* reserved */
		10, 12, 13, 15, 20, 26, 30, 35,
		40, 45, 50, 52, 55, 60, 70, 80 };

	debug("In %s\n", __func__);

	mmc->rca = 0;
	/* Program initial clock speed and power */
	emm_switch.u64 = 0;
	emm_switch.s.bus_id = host->bus_id;
	emm_switch.s.power_class = 10;
	/* Set clock period */
	emm_switch.s.clk_hi = ((host->sclock / mmc->clock) + 1) / 2;
	emm_switch.s.clk_lo = ((host->sclock / mmc->clock) + 1) / 2;
	debug("%s: Set clock period to %d clocks, sclock: %u\n", __func__,
	      emm_switch.s.clk_hi, host->sclock);
	cvmx_write_csr(CVMX_MIO_EMM_SWITCH, emm_switch.u64);

	/* Set watchdog for command timeout */
	emm_wdog.u64 = 0;
	emm_wdog.s.clk_cnt = 256000 + (8 * mmc->clock) / 10;
	cvmx_write_csr(CVMX_MIO_EMM_WDOG, emm_wdog.u64);
	udelay(10000);	/* Wait 10ms */

	/* Reset the card */
	debug("Resetting card\n");
	err = mmc_go_idle(mmc);
	if (err) {
		if (!init_time)
			printf("%s: Could not reset MMC card\n", __func__);
		return err;
	}

#ifdef CONFIG_OCTEON_MMC_SD
	/* Note that this doesn't work yet on the CN61XX pass 1.0.
	 * The CN61XX pass 1.0 has an errata where only 8-bit wide buses are
	 * supported due to checksum errors on narrower busses.
	 */
	debug("Testing for SD version 2\n");
	/* Test for SD version 2 */
	err = mmc_send_if_cond(mmc);

	debug("Getting SD card operating condition\n");
#endif
	/* Now try to get the SD card's operating condition */
	if (IS_SD(mmc)) {
#ifdef CONFIG_OCTEON_MMC_SD
		err = sd_send_op_cond(mmc);
		if (err == TIMEOUT) {
			err = mmc_send_op_cond(mmc);
			if (err) {
				printf("Card did not respond to voltage select!\n");
				return UNUSABLE_ERR;
			}
		}

		if (err) {
			mmc_reset_bus(mmc, TRUE);
			err = mmc_send_op_cond(mmc);
			if (err) {
				puts("MMC Init: Error recovering after SD Version 2 test\n");
				return UNUSABLE_ERR;
			}
		}
#endif
	} else {
		err = mmc_send_op_cond(mmc);
		if (err == TIMEOUT) {
			err = mmc_send_op_cond(mmc);
			if (err) {
				debug("MMC Init: Card did not respond to voltage select\n");
				return UNUSABLE_ERR;
			}
		} else if (err) {
			mmc_reset_bus(mmc, TRUE);
			err = mmc_send_op_cond(mmc);
			if (err) {
				puts("MMC Init: Error recovering after SD Version 2 test\n");
				return UNUSABLE_ERR;
			}
		}
	}


	err = mmc_all_send_cid(mmc);
	if (err) {
		printf("%s: Error getting CID\n", __func__);
		return err;
	}

	/* For MMC cards, set the Relative Address.
	 * For SD cards, get the Relative address.
	 * This also puts the cards into Standby State.
	 */
	if (IS_SD(mmc)) {
#ifdef CONFIG_OCTEON_MMC_SD
		err = sd_send_relative_addr(mmc);
		if (err) {
			printf("%s: Error getting RCA\n", __func__);
			return err;
		}
#endif
	} else {
		mmc->rca = host->bus_id + 1;
		err = mmc_set_relative_addr(mmc);
		if (err) {
			printf("%s: Error setting MMC RCA to %d\n",
			       __func__, mmc->rca);
			return err;
		}
	}

	err = mmc_get_csd(mmc);
	if (err) {
		printf("%s: Error getting CSD\n", __func__);
		return err;
	}
	if (mmc->version == MMC_VERSION_UNKNOWN) {
		int version = (mmc->csd[0] >> 26) & 0xf;

		switch (version) {
			case 0:
				mmc->version = MMC_VERSION_1_2;
				debug("MMC version 1.2 detected\n");
				break;
			case 1:
				mmc->version = MMC_VERSION_1_4;
				debug("MMC version 1.4 detected\n");
				break;
			case 2:
				mmc->version = MMC_VERSION_2_2;
				debug("MMC version 2.2 detected\n");
				break;
			case 3:
				mmc->version = MMC_VERSION_3;
				debug("MMC version 3 detected\n");
				break;
			case 4:
				mmc->version = MMC_VERSION_4;
				debug("MMC version 4 detected\n");
				break;
			default:
				mmc->version = MMC_VERSION_1_2;
				debug("MMC version 1.2 (unknown) detected\n");
				break;
		}
	}

	freq = fbase[(mmc->csd[0] & 7)];
	mult = multipliers[((mmc->csd[0] >> 3) & 0xf)];

#ifdef DEBUG
	int i, classes;
	i = 0;
	classes = (mmc->csd[1] >> 20) & 0x7ff;
	debug("Classes supported: ");
	while (classes) {
		if (classes & 1)
			debug("%i ", i);
		classes >>= 1;
		i++;
	}
	debug("\n%s: Read block length: %u\n", __func__,
	      1 << ((mmc->csd[1] >> 16) & 0xF));
#endif

	if (IS_SD(mmc))
		switch (mmc->csd[0] & 0x7f) {
		case 0x0b:	mmc->tran_speed = 50000000;	break;
		case 0x2b:	mmc->tran_speed = 200000000;	break;
		case 0x32:	mmc->tran_speed = 25000000;	break;
		case 0x5a:	mmc->tran_speed = 50000000;	break;
		default:
			printf("Unknown tran_speed value 0x%x in SD CSD register\n",
			       mmc->csd[0] & 0x7f);
			mmc->tran_speed = 25000000;
			break;
		}
	else
		mmc->tran_speed = freq * mult;
	debug("%s CSD tran_speed: %u\n",
	      IS_SD(mmc) ? "SD" : "MMC", mmc->tran_speed);

	mmc->read_bl_len = 1 << ((mmc->csd[3] >> 22) & 0xf);

	if (IS_SD(mmc))
		mmc->write_bl_len = mmc->read_bl_len;
	else
		mmc->write_bl_len = 1 << ((mmc->csd[3] >> 22) & 0xf);

	if (mmc->high_capacity) {
		csize = (mmc->csd[1] & 0x3f) << 16
			| (mmc->csd[2] & 0xffff0000) >> 16;
		cmult = 8;
	} else {
		csize = (mmc->csd[1] & 0x3ff) << 2
			| (mmc->csd[2] & 0x00038000) >> 16;
		cmult = (mmc->csd[2] & 0x00038000) >> 15;
	}

	mmc->capacity = (csize + 1) << (cmult + 2);
	mmc->capacity *= mmc->read_bl_len;
	debug("%s: capacity: %llu bytes\n", __func__, mmc->capacity);
	debug("%s: read to write program time factor: %d\n", __func__,
	      1 << ((mmc->csd[0] >> 26) & 7));

	mmc->erase_grp_size = 1;
	mmc->read_bl_len = min(mmc->read_bl_len, 512);
	mmc->write_bl_len = min(mmc->write_bl_len, 512);

	/* Select the card and put it into Transfer Mode */

	debug("%s: Selecting card to rca %d\n", __func__, mmc->rca);

	err = mmc_select_card(mmc);
	if (err) {
		printf("%s: Error selecting card\n", __func__);
		return err;
	}

	if (IS_SD(mmc)) {
		debug("SD version: ");
		switch(mmc->version) {
		case SD_VERSION_2:	debug("2\n");		break;
		case SD_VERSION_1_0:	debug("1.0\n");		break;
		case SD_VERSION_1_10:	debug("1.10\n");	break;
		default:		debug("Undefined\n");	break;
		}

		debug("sd version 0x%x\n", mmc->version);
	} else {
		debug("MMC version: ");
		switch (mmc->version) {
		case MMC_VERSION_UNKNOWN:	debug("UNKNOWN\n");	break;
		case MMC_VERSION_1_2:		debug("1.2\n");		break;
		case MMC_VERSION_1_4:		debug("1.4\n");		break;
		case MMC_VERSION_2_2:		debug("2.2\n");		break;
		case MMC_VERSION_3:		debug("3\n");		break;
		case MMC_VERSION_4:		debug("4\n");		break;
		default:			debug("Undefined\n");	break;
		}

		debug("mmc version 0x%x\n", mmc->version);
		if (mmc->version >= MMC_VERSION_4) {
		/* Check ext_csd version and capacity */
			err = mmc_send_ext_csd(mmc);
			if (err) {
				printf("%s: Error: cannot read extended CSD\n",
				       __func__);
				return -1;
			}
			if (host->ext_csd[EXT_CSD_REV] >= 2) {
				capacity = host->ext_csd[EXT_CSD_SEC_CNT] << 0
					| host->ext_csd[EXT_CSD_SEC_CNT + 1] << 8
					| host->ext_csd[EXT_CSD_SEC_CNT + 2] << 16
					| host->ext_csd[EXT_CSD_SEC_CNT + 3] << 24;
				debug("MMC EXT CSD reports capacity of %llu sectors (0x%llx bytes)\n",
				      capacity, capacity * 512);
				capacity *= 512;
				if (((capacity >> 20) > 2 * 1024)
				    && mmc->high_capacity)
					mmc->capacity = capacity;
			}
			if (host->ext_csd[EXT_CSD_ERASE_GROUP_DEF]) {
				mmc->erase_grp_size =
					host->ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]
						* 512 * 1024;
			} else {
				int erase_gsz, erase_gmul;
				erase_gsz = (mmc->csd[2] & 0x00007c00) >> 10;
				erase_gmul = (mmc->csd[2] & 0x000003e0) >> 5;
				mmc->erase_grp_size = (erase_gsz + 1)
							* (erase_gmul + 1);
			}
			debug("%s: erase group size %d\n",
			      __func__, mmc->erase_grp_size);
			if (host->ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & PART_SUPPORT) {
				mmc->part_config = host->ext_csd[EXT_CSD_PART_CONF];
				debug("%s: partitioning config 0x%x\n", __func__,
				      mmc->part_config);
			}
			debug("%s: part config: 0x%x\n", __func__,
			      mmc->part_config);
			mmc_set_bus_width(mmc, 8);
			if (host->ext_csd[EXT_CSD_CARD_TYPE]
			    & EXT_CSD_CARD_TYPE_26) {
				mmc->card_caps |= MMC_MODE_HS;
				debug("%s: high-speed mode supported\n",
				      __func__);
			}
			if (host->ext_csd[EXT_CSD_CARD_TYPE]
			    & EXT_CSD_CARD_TYPE_52) {
				mmc->card_caps |=
					MMC_MODE_HS_52MHz | MMC_MODE_HS;
				debug("%s: high-speed 52MHz mode supported\n",
				      __func__);
			}
		}
	}

	debug("%s: Changing frequency\n", __func__);
#ifdef CONFIG_OCTEON_MMC_SD
	if (IS_SD(mmc))
		err = sd_change_freq(mmc);
	else
#endif
	{
		mmc_set_ios(mmc);
		err = 0;
	}
	if (err) {
		printf("%s: Error changing frequency\n", __func__);
		return err;
	}

	/* Restrict card's capabilities by what the host can do. */
	debug("%s: MMC card caps: 0x%x, host caps: 0x%x\n",
	      __func__, mmc->card_caps, mmc->host_caps);
	mmc->card_caps &= mmc->host_caps;


	if (IS_SD(mmc)) {
#ifdef CONFIG_OCTEON_MMC_SD
		err = sd_set_bus_width_speed(mmc);
		if (err) {
			printf("%s: Error setting SD bus width and/or speed\n",
			       __func__);
			return err;
		}
#endif
	} else {
		err = mmc_set_bus_width_speed(mmc);
		if (err) {
			printf("%s: Error setting MMC bus width and/or speed\n",
			       __func__);
			return err;
		}
	}

	err = mmc_set_blocklen(mmc, mmc->read_bl_len);
	if (err) {
		printf("%s: Error setting block length to %d\n",
		       __func__, mmc->read_bl_len);
		return err;
	}

	/* Set watchdog for command timeout again */
	emm_wdog.u64 = 0;
	emm_wdog.s.clk_cnt = 256000 + (10 * mmc->clock) / 8;
	cvmx_write_csr(CVMX_MIO_EMM_WDOG, emm_wdog.u64);

	/* Fill in device description */
	debug("%s: Filling in block descriptor\n",  __func__);
	mmc->block_dev.lun = 0;
	mmc->block_dev.type = 0;
	mmc->block_dev.blksz = mmc->read_bl_len;
	mmc->block_dev.lba = lldiv(mmc->capacity, mmc->read_bl_len);
	sprintf(mmc->block_dev.vendor, "Man %06x Snr %08x", mmc->cid[0] >> 8,
		(mmc->cid[2] << 8) | (mmc->cid[3] >> 24));
	debug("%s: %s\n", __func__, mmc->block_dev.vendor);
	sprintf(mmc->block_dev.product, "%c%c%c%c%c%c", mmc->cid[0] & 0xff,
		mmc->cid[1] >> 24, (mmc->cid[1] >> 16) & 0xff,
		(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff,
		(mmc->cid[2] >> 24) & 0xff);
	debug("%s: %s\n", __func__, mmc->block_dev.product);
	sprintf(mmc->block_dev.revision,"%d.%d", (mmc->cid[2] >> 20) & 0xf,
		(mmc->cid[2] >> 16) & 0xf);
	debug("%s: %s\n", __func__, mmc->block_dev.revision);

	return 0;
}

int mmc_initialize(bd_t *bis)
{
	struct mmc *mmc;
	struct mmc_host *host;
	int bus_id = 0;
	int rc;
	cvmx_mio_emm_cfg_t emm_cfg;
	int gpio_val;

	/* Get MMC information from the FDT */
	if ((rc = octeon_boot_bus_mmc_get_info(&mmc_info)) != 0)
		goto out;

	init_time = 1;	/* Suppress error messages */
	INIT_LIST_HEAD(&mmc_devices);
	cur_dev_num = 0;

	/* Disable all MMC slots */
	emm_cfg.u64 = 0;
	cvmx_write_csr(CVMX_MIO_EMM_CFG, emm_cfg.u64);
	udelay(100000);

	rc = -1;
	for (bus_id = 0; bus_id < CONFIG_OCTEON_MAX_MMC_SLOT; bus_id++) {
		/* Skip slot if not in device tree */
		if (mmc_info.slot[bus_id].chip_sel < 0) {
			debug("%s: Skipping MMC bus %d\n", __func__, bus_id);
			continue;
		}
		debug("Initializing MMC bus %d\n", bus_id);
		if (mmc_info.slot[bus_id].cd_gpio != -1) {
			gpio_direction_input(mmc_info.slot[bus_id].cd_gpio);
			gpio_val = gpio_get_value(mmc_info.slot[bus_id].cd_gpio);
			debug("%s: Slot 0 cd gpio reports %d\n",
			      __func__, gpio_val);
			if (gpio_val == mmc_info.slot[bus_id].cd_active_low) {
				debug("%s: Skipping empty bus %d\n",
				      __func__, bus_id);
				continue;
			}
		}

		mmc = &mmc_dev[bus_id];
		host = &mmc_host[bus_id];
		memset(mmc, 0, sizeof(*mmc));
		memset(host, 0, sizeof(*host));
		mmc->priv = (void *)host;
		sprintf(mmc->name, "Octeon MMC/SD%d", bus_id);
		mmc->version = MMC_VERSION_UNKNOWN;
		mmc->rca = bus_id + 1;
		host->bus_id = bus_id;
		host->mmc = mmc;
		host->dev_index = bus_id;
		host->sector_mode = 0;
		host->cd_gpio = mmc_info.slot[bus_id].cd_gpio;
		host->cd_active_low = mmc_info.slot[bus_id].cd_active_low;
		if (host->cd_gpio != -1)
			gpio_direction_input(host->cd_gpio);

		host->wp_gpio = mmc_info.slot[bus_id].wp_gpio;
		host->wp_active_low = mmc_info.slot[bus_id].wp_active_low;
		if (host->wp_gpio != -1)
			gpio_direction_input(host->wp_gpio);

		host->power_gpio = mmc_info.slot[bus_id].power_gpio;
		host->power_active_low = mmc_info.slot[bus_id].power_active_low;
		if (host->power_gpio != -1) {
			debug("%s: Turning on power for MMC slot %d\n",
			      __func__, bus_id);
			gpio_direction_output(host->power_gpio,
					      !host->power_active_low);
		}

		debug("%s: Calling mmc_init for %s\n", __func__, mmc->name);
		rc = mmc_init(mmc);
		if (!rc) {
			debug("Registering MMC device %d\n", bus_id);
			mmc_register(mmc);
			init_part(&mmc->block_dev);
		} else {
			debug("MMC device %d initialization failed\n", bus_id);
		}
	}
	init_time = 0;

out:
	if (rc == 0)
		print_mmc_devices(',');
	else
		printf("not available\n");
	return rc;
}

int mmc_init(struct mmc *mmc)
{
	struct mmc_host *host = (struct mmc_host *)(mmc->priv);
	cvmx_mio_emm_cfg_t emm_cfg;
	int rc;
	int cs;

	if (!octeon_has_feature(OCTEON_FEATURE_MMC))
		return 0;

	debug("%s: Entry\n", __func__);

	cs = host->bus_id;
	mmc_reset_bus(mmc, FALSE);

	mmc->f_min = CONFIG_OCTEON_MIN_BUS_SPEED_HZ;
	mmc->clock = mmc->f_min;
	mmc->f_max = mmc_info.slot[cs].max_frequency;
#ifdef CONFIG_OCTEON_MMC_MAX_FREQUENCY
	mmc->f_max = min(mmc->f_max, CONFIG_OCTEON_MMC_MAX_FREQUENCY);
#endif
	mmc->f_max = min(mmc->f_max, getenv_ulong("mmc_max_freq", 10, 52000000));
	mmc->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;
	host->sclock = cvmx_clock_get_rate(CVMX_CLOCK_SCLK);
	debug("%s: sclock: %u\n", __func__, host->sclock);

	/* Set up the host capabilities */
	if (mmc->f_max > 20000000)
		mmc->host_caps |= MMC_MODE_HS;
	if (mmc->f_max >= 52000000)
		mmc->host_caps |= MMC_MODE_HS_52MHz;
	if (mmc_info.slot[cs].bus_max_width >= 4)
		mmc->host_caps |= MMC_MODE_4BIT;
	if (mmc_info.slot[cs].bus_max_width == 8)
		mmc->host_caps |= MMC_MODE_8BIT;
	mmc->host_caps |= MMC_MODE_DDR;
	host->max_width = mmc_info.slot[cs].bus_max_width;
	mmc->send_cmd = mmc_send_cmd;
	mmc->set_ios = mmc_set_ios;
	mmc->init = NULL;
	mmc->voltages = getenv_ulong("mmc_voltages", 16, 0);
	if (!mmc->voltages)
		mmc->voltages =  MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30 |
				 MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33 |
				 MMC_VDD_33_34 | MMC_VDD_34_35 | MMC_VDD_35_36;

	mmc_set_bus_width(mmc, 1);
	mmc_set_clock(mmc, mmc->f_min);

	rc = mmc_startup(mmc);
	if (rc != 0) {
		debug("Could not start up mmc bus %d\n", cs);
		emm_cfg.u64 = cvmx_read_csr(CVMX_MIO_EMM_CFG);
		emm_cfg.u64 &= ~(1 << host->bus_id);
		cvmx_write_csr(CVMX_MIO_EMM_CFG, emm_cfg.u64);
	}
	return rc;
}

int get_mmc_num(void)
{
	return cur_dev_num;
}