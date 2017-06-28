/***********************license start************************************
 * Copyright (c) 2011 Cavium Inc. (support@cavium.com). All rights
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

/**
 *
 * $Id: octeon_wd.c 75238 2012-07-25 02:37:58Z awilliams $
 *
 * Watchdog support imported from the Linux kernel driver
 */

#include <common.h>
#include <config.h>
#include <watchdog.h>
#include <asm/mipsregs.h>
#include <asm/uasm.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-access.h>
#include <asm/arch/cvmx-ciu-defs.h>
#include <asm/arch/cvmx-mio-defs.h>
#ifndef CONFIG_SYS_NO_FLASH
#include <mtd/cfi_flash.h>
#endif
#include <asm/arch/octeon_boot_bus.h>

DECLARE_GLOBAL_DATA_PTR;

#define K0		26
#define C0_CVMMEMCTL	11, 7
#define C0_STATUS	12, 0
#define C0_EBASE	15, 1
#define C0_DESAVE	31, 0

static u32 nmi_stage1_insns[64];
static struct uasm_label labels[5];
static struct uasm_reloc relocs[5];

enum label_id {
	label_enter_bootloader = 1
};

extern u64 octeon_get_io_clock_rate(void);

/* Defined in start.S */
extern void nmi_exception_handler(void);
extern void asm_reset(void);

static int octeon_watchdog_build_stage1(void);

static uint32_t octeon_watchdog_get_period(unsigned int msecs)
{
	uint32_t timeout_cnt;
	/* Divide by 2 since the first interrupt is ignored in U-Boot */
	msecs >>= 1;
	if (msecs > 30000)
		msecs = 30000;
	do {
		timeout_cnt = (((octeon_get_io_clock_rate() >> 8) / 1000) * msecs) >> 8;
		msecs -= 500;
		/* CN68XX seems to run at half the rate */
		if (OCTEON_IS_MODEL(OCTEON_CN68XX))
			timeout_cnt >>= 1;
	} while (timeout_cnt > 65535);
	debug("%s: Setting timeout count to %d\n", __func__, timeout_cnt);
	return timeout_cnt;
}

/**
 * Starts the watchdog with the specified interval.
 * 
 * @param msecs - time until NMI is hit.  If zero then use watchdog_timeout
 *                environment variable or default value.
 * 
 * NOTE: if the time exceeds the maximum supported by the hardware then it
 * will be limited to the maximum supported by the hardware.
 */
void hw_watchdog_start(int msecs)
{
	union cvmx_ciu_wdogx ciu_wdog;
	cvmx_mio_boot_loc_cfgx_t reg_boot_loc_cfg;
	uint64_t *ptr;
	int i;
	static int initialized = 0;
	char *e;

	debug("%s(%d)\n", __func__, msecs);

	if (!initialized) {
		/* Initialize the stage1 code */
		if (octeon_watchdog_build_stage1()) {
			/* Something bad happened */
			return;
		}
		initialized = 1;
	}
	
	if (!msecs) {
		if ((e = getenv("watchdog_timeout")) != NULL) {
			msecs = simple_strtoul(e, NULL, 0);
		}
		if (!msecs)
			msecs = CONFIG_OCTEON_WD_TIMEOUT * 1000;
	}

	cvmx_write_csr(CVMX_CIU_PP_POKEX(0), 1);

	/* Copy the instructions to the first local region.  Note that each
	 * local region is 128 bytes which holds a maximum of 32 instructions.
	 */
	ptr = (uint64_t *)nmi_stage1_insns;
	for (i = 0; i < (128 / 8); i++) {
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_ADR, i * 8);
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_DAT, ptr[i]);
	}
	/* Map it to the reset vector location */
	reg_boot_loc_cfg.u64 = 0;
	reg_boot_loc_cfg.s.en = 1;
	reg_boot_loc_cfg.s.base = 0x1fc00000 >> 7;
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_CFGX(0), reg_boot_loc_cfg.u64);

	ciu_wdog.u64 = 0;
	ciu_wdog.s.len = octeon_watchdog_get_period(msecs);
	ciu_wdog.s.mode = 3;	/* Interrupt + NMI + soft-reset */

	cvmx_write_csr(CVMX_CIU_WDOGX(0), ciu_wdog.u64);
	cvmx_write_csr(CVMX_CIU_PP_POKEX(0), 1);
	
}

/**
 * Puts the dog to sleep
 */
void hw_watchdog_disable(void)
{
	union cvmx_ciu_wdogx ciu_wdog;

	/* Poke the watchdog to clear out its state */
	cvmx_write_csr(CVMX_CIU_PP_POKEX(0), 1);

	/* Disable the hardware. */
	ciu_wdog.u64 = 0;
	cvmx_write_csr(CVMX_CIU_WDOGX(0), ciu_wdog.u64);

	/* Restore the moveable regions */
	octeon_boot_bus_moveable_init();
	
	debug("%s: Watchdog stopped\n", __func__);
}

/**
 * Pets the watchdog
 */
void hw_watchdog_reset(void)
{
	/* Pet the dog.  Good dog! */
	cvmx_write_csr(CVMX_CIU_PP_POKEX(0), 1);
}
/**
 * Build stage 1 for the NMI handler.
 *
 * This must be built dynamically and will be installed at the reset vector 
 * location.  It checks for a valid watchdog NMI then branches to the code
 * in start.S that will dump the registers.  The next time the watchdog is 
 * hit it will force a reset.
 */
static int octeon_watchdog_build_stage1(void)
{
#ifdef DEBUG
	int i;
#endif
	int len;
	int is_ciu2 = octeon_has_feature(OCTEON_FEATURE_CIU2);
	u32 *p = nmi_stage1_insns;
	struct uasm_label *l = labels;
	struct uasm_reloc *r = relocs;
	/*
	 * For the next few instructions running the debugger may
	 * cause corruption of k0 in the saved registers. Since we're
	 * about to crash, nobody probably cares.
	 *
	 * Save K0 into the debug scratch register
	 */
	uasm_i_dmtc0(&p, K0, C0_DESAVE);

	uasm_i_mfc0(&p, K0, C0_STATUS);
	/* Force 64-bit addressing enabled */
	uasm_i_ori(&p, K0, K0, 0xE0);	/* UX, SX, KX */
	uasm_i_mtc0(&p, K0, C0_STATUS);

	/*
	 * If octeon_bootloader_entry_addr is set, we can transfer
	 * control to the bootloader if it is not a watchdog related
	 * NMI.
	 */
	uasm_il_bbit0(&p, &r, K0, 19, label_enter_bootloader);
	uasm_i_mfc0(&p, K0, C0_EBASE);
	/* Coreid number in K0 */
	uasm_i_andi(&p, K0, K0, is_ciu2 ? 0x1f : 0xf);
	/* 8 * coreid in bits 16-31 */
	uasm_i_dsll_safe(&p, K0, K0, 3 + 16);
	uasm_i_ori(&p, K0, K0, 0x8001);
	uasm_i_dsll_safe(&p, K0, K0, 16);
	if (is_ciu2) {
		uasm_i_ori(&p, K0, K0, 0x0701);
		uasm_i_dsll_safe(&p, K0, K0, 16);
		uasm_i_ori(&p, K0, K0, 0x0010);
		uasm_i_drotr_safe(&p, K0, K0, 48);
		/* Should result in: 0x8001,0701,0010,8*coreid which is
		 * CVMX_CIU2_WDOGX(coreid)
		 */
		uasm_i_ld(&p, K0, 0, K0);
	} else {
		uasm_i_ori(&p, K0, K0, 0x0700);
		uasm_i_drotr_safe(&p, K0, K0, 32);
		/*
		 * Should result in: 0x8001,0700,0000,8*coreid which is
		 * CVMX_CIU_WDOGX(coreid) - 0x0500
		 *
		 * Now ld K0, CVMX_CIU_WDOGX(coreid)
		 */
		uasm_i_ld(&p, K0, 0x500, K0);
	}
	/*
	 * If bit one set handle the NMI as a watchdog event.
	 * otherwise transfer control to bootloader.
	 */
	uasm_il_bbit0(&p, &r, K0, 1, label_enter_bootloader);
	uasm_i_nop(&p);

	/* Clear Dcache so cvmseg works right. */
	uasm_i_cache(&p, 1, 0, 0);

	/* Use K0 to do a read/modify/write of CVMMEMCTL */
	uasm_i_dmfc0(&p, K0, C0_CVMMEMCTL);
	/* Clear out the size of CVMSEG	*/
	uasm_i_dins(&p, K0, 0, 0, 6);
	/* Set CVMSEG to its largest value */
	uasm_i_ori(&p, K0, K0, 0x1c0 | 54);
	/* Store the CVMMEMCTL value */
	uasm_i_dmtc0(&p, K0, C0_CVMMEMCTL);

	/* Load the address of the second stage handler */
	UASM_i_LA(&p, K0, (long)nmi_exception_handler);
	uasm_i_jr(&p, K0);
	uasm_i_dmfc0(&p, K0, C0_DESAVE);

	uasm_build_label(&l, p, label_enter_bootloader);

	if (gd->ogd.uboot_flash_address) {
		/* Jump to the bootloader and restore K0 */
		UASM_i_LA(&p, K0, (long)gd->ogd.uboot_flash_address);
	} else {
		UASM_i_LA(&p, K0, (long)asm_reset);
	}
	uasm_i_jr(&p, K0);
	uasm_i_dmfc0(&p, K0, C0_DESAVE);
	uasm_resolve_relocs(relocs, labels);

	len = (int)(p - nmi_stage1_insns);
#ifdef DEBUG
	debug("Synthesized NMI stage 1 handler (%d instructions).\n", len);

	debug("\t.set push\n");
	debug("\t.set noreorder\n");
	for (i = 0; i < len; i++)
		debug("\t.word 0x%08x\n", nmi_stage1_insns[i]);
	debug("\t.set pop\n");
#endif
	if (len > 32) {
		printf("NMI stage 1 handler exceeds 32 instructions, was %d\n",
		       len);
		return -1;
	}
	return 0;
}
