/***********************license start************************************
 * Copyright (c) 2004-2012 Cavium Inc. (support@cavium.com). All rights
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

#include <config.h>
#include <common.h>
#include <command.h>
#include <exports.h>
#include <linux/ctype.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/octeon_hal.h>
#include <asm/arch/cvmx-twsi.h>
#include <asm/arch/cvmx-bootloader.h>
#include <asm/arch/cvmx-l2c.h>
#include <asm/arch/cvmx-clock.h>
#include <asm/arch/cvmx-asm.h>
#include <asm/arch/cvmx-gpio.h>
#include <asm/arch/octeon-pci-console.h>
#include <asm/arch/octeon_eeprom_types.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/octeon-boot-info.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>
#include <asm/io.h>

/*#define DEBUG*/
#ifdef DEBUG
#define DEBUG_PRINTF   printf
#else
#define DEBUG_PRINTF(...);
#endif

DECLARE_GLOBAL_DATA_PTR;

extern char tlbwi_and_eret;
extern void asm_launch_linux_entry_point;

#ifndef CONFIG_CMD_OCTEON_CSR
/* Stub this out here, as we don't support real CSR decoding in u-boot due to
 * size limitations
 */
void cvmx_csr_db_decode(int identifier, uint64_t address, uint64_t value)
{
	printf(" 0x%llx: 0x%llx\n", address, value);
}
#endif

/******************  End u-boot eeprom hooks ******************************/

int octeon_show_info(void)
{
	return (!!(cvmx_gpio_read() & OCTEON_GPIO_SHOW_FREQ));
}

void octeon_led_str_write_std(const char *str)
{
#ifdef CONFIG_OCTEON_ENABLE_LED_DISPLAY
	char *ptr;
	int i;

	if (gd->ogd.led_addr) {
		ptr = (char *)((uint32_t)
			MAKE_XKPHYS((uint32_t)gd->ogd.led_addr | 0xf8));
		for (i = 0; i < 8; i++)
			if (*str)
				ptr[i] = *str++;
			else
				ptr[i] = ' ';
	}
#endif
}

int octeon_check_mem_errors (void)
{
	/* Check for DDR/L2C memory errors */
	uint64_t val;
	int errors = 0;
	int ddr_ecc_errors = 0;

	if (OCTEON_IS_OCTEON2()) {
		cvmx_l2c_err_tdtx_t d_err;
		cvmx_l2c_err_ttgx_t t_err;
		int tad;
		for (tad = 0; tad < CVMX_L2C_TADS; tad++) {
			d_err.u64 = cvmx_read_csr(CVMX_L2C_ERR_TDTX(tad));
			if (d_err.s.dbe || d_err.s.sbe ||
			    d_err.s.vdbe || d_err.s.vsbe) {
				printf("WARNING: L2D ECC errors detected in "
				       "TAD%d!(syn: 0x%x, wayidx: 0x%x, "
				       "type: 0x%x\n",
				       tad, t_err.s.syn, t_err.s.wayidx,
				       t_err.s.type);
				cvmx_write_csr(CVMX_L2C_ERR_TDTX(tad), d_err.u64);
				errors++;
			}
			t_err.u64 = cvmx_read_csr(CVMX_L2C_ERR_TTGX(tad));
			if (t_err.s.dbe || t_err.s.sbe) {
				printf("WARNING: L2T ECC errors detected in "
				       "TAD%d!(syn: 0x%x, wayidx: 0x%x, "
				       "type: 0x%x\n",
				       tad, t_err.s.syn, t_err.s.wayidx,
				       t_err.s.type);
				cvmx_write_csr(CVMX_L2C_ERR_TTGX(tad), t_err.u64);
				errors++;
			}
			if (t_err.s.noway)
				printf("WARNING: L2C NOWAY error in TAD%d: "
				       "no way available for allocation\n", tad);
		}
	} else {
		val = cvmx_read_csr(CVMX_L2D_ERR);
		if (val & 0x18ull) {
			printf("WARNING: L2D ECC errors detected!\n");
			cvmx_write_csr(CVMX_L2D_ERR, val);
			errors++;
		}
		val = cvmx_read_csr(CVMX_L2T_ERR);
		if (val & 0x18ull) {
			printf("WARNING: L2T ECC errors detected!\n");
			cvmx_write_csr(CVMX_L2T_ERR, val);
			errors++;
		}
	}

	if (octeon_is_model(OCTEON_CN63XX) || octeon_is_model(OCTEON_CN66XX)) {
		cvmx_lmcx_int_t lmc_int;
		lmc_int.u64 = cvmx_read_csr(CVMX_LMCX_INT (0));
		if (lmc_int.s.sec_err || lmc_int.s.ded_err)
			printf("WARNING: DDR ECC errors detected!\n");
		if (lmc_int.s.nxm_wr_err)
			printf("WARNING: DDR write to nonexistant memory\n");

		cvmx_write_csr (CVMX_LMCX_INT (0), lmc_int.u64);
	} else if (octeon_is_model(OCTEON_CN68XX)) {
		/* TODO_68XX */
	} else if (octeon_is_model(OCTEON_CN56XX)) {
		/* The cn56xx has multiple ddr interfaces that can be enabled
		 * individually. Confirm that the interface is enabled before
		 * checking for ECC errors.  Otherwise the read will timeout
		 * with a bus error.
		 */
		cvmx_l2c_cfg_t l2c_cfg;
		l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);
		if (l2c_cfg.cn56xx.dpres0) {
			val = cvmx_read_csr(CVMX_LMCX_MEM_CFG0 (0));
			if (val & (0xFFull << 21)) {
				cvmx_write_csr(CVMX_LMCX_MEM_CFG0 (0), val);
				errors++;
				ddr_ecc_errors++;
			}
		}
		if (l2c_cfg.cn56xx.dpres1) {
			val = cvmx_read_csr(CVMX_LMCX_MEM_CFG0 (1));
			if (val & (0xFFull << 21)) {
				cvmx_write_csr(CVMX_LMCX_MEM_CFG0 (1), val);
				errors++;
				ddr_ecc_errors++;
			}
		}
	} else {
		val = cvmx_read_csr(CVMX_LMC_MEM_CFG0);
		if (val & (0xFFull << 21)) {
			cvmx_write_csr(CVMX_LMC_MEM_CFG0, val);
			errors++;
			ddr_ecc_errors++;
		}
	}

	if (ddr_ecc_errors) {
		printf ("WARNING: DDR ECC errors detected!\n");
	}
	return errors;
}

/* Checks to see if a boot bus region is enabled for the specified address.
 * This is used to conditionalize at runtime the CFI flash probing, as this is
 * required generic board support.
 */
int octeon_check_nor_flash_enabled(uint32_t addr)
{
	int i;
	cvmx_mio_boot_reg_cfgx_t reg_cfg;
	uint32_t reg_addr;
	uint32_t reg_size;

	addr &= 0x1fffffff;
	for (i = 0; i < 8; i++) {
		reg_cfg.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFGX(i));
		reg_addr = reg_cfg.s.base << 16;
		reg_size = (reg_cfg.s.size + 1) << 16;

		if (addr >= reg_addr && addr < (reg_addr + reg_size)
		    && reg_cfg.s.en)
			return 1;
	}
	return 0;
}

/* Boot bus init for flash and peripheral access */
#define FLASH_RoundUP(_Dividend, _Divisor) (((_Dividend)+(_Divisor))/(_Divisor))
void set_flash_clock(void)
{
	cvmx_mio_boot_reg_timx_t __attribute__((unused)) reg_tim;
	uint64_t clock_mhz, clock_period;

	clock_mhz = cvmx_clock_get_rate(CVMX_CLOCK_SCLK)/1000000;
	clock_period = 1000000 / clock_mhz; /* clk period (psecs) */

	/* Set timing to be valid for all CPU clocks up to clock_period */
	reg_tim.u64 = 0;
	reg_tim.s.pagem = 0;
	reg_tim.s.wait = 0x3f;
	reg_tim.s.adr =
	    FLASH_RoundUP((FLASH_RoundUP (10000ULL, clock_period) - 1), 4);
	reg_tim.s.pause = 0;
	reg_tim.s.ce =
	    FLASH_RoundUP((FLASH_RoundUP (50000ULL, clock_period) - 1), 4);;
#ifndef CONFIG_OCTEON_FLASH_USES_ALE
	if (octeon_is_model(OCTEON_CN31XX))
#endif
		reg_tim.s.ale = 4;	/* Leave ALE at 34 nS */
	reg_tim.s.oe =
	    FLASH_RoundUP((FLASH_RoundUP (50000ULL, clock_period) - 1), 4);
	reg_tim.s.rd_hld =
	    FLASH_RoundUP((FLASH_RoundUP (25000ULL, clock_period) - 1), 4);
	reg_tim.s.wr_hld =
	    FLASH_RoundUP((FLASH_RoundUP (35000ULL, clock_period) - 1), 4);
	reg_tim.s.we =
	    FLASH_RoundUP((FLASH_RoundUP (35000ULL, clock_period) - 1), 4);
	reg_tim.s.page =
	    FLASH_RoundUP((FLASH_RoundUP (25000ULL, clock_period) - 1), 4);
	cvmx_write_csr(CVMX_MIO_BOOT_REG_TIM0, reg_tim.u64);
}


/* Reads the CPU clock multiplier and returns it.  Does
 * the correct thing based on Octeon model
 */
int octeon_get_cpu_multiplier(void)
{

	if (!OCTEON_IS_OCTEON1PLUS()) {
		cvmx_mio_rst_boot_t rst_boot;
		rst_boot.u64 = cvmx_read_csr (CVMX_MIO_RST_BOOT);
		return (rst_boot.s.c_mul);
	} else {
		uint64_t data;
		if (OCTEON_IS_MODEL (OCTEON_CN52XX)
		    || OCTEON_IS_MODEL (OCTEON_CN56XX))
			data = cvmx_read_csr (CVMX_PEXP_NPEI_DBG_DATA);
		else
			data = cvmx_read_csr (CVMX_DBG_DATA);

		data = data >> 18;
		data &= 0x1f;
		return data;
	}
}

uint64_t octeon_get_ioclk_hz(void)
{
	if (!OCTEON_IS_OCTEON1PLUS()) {
		cvmx_mio_rst_boot_t rst_boot;
		rst_boot.u64 = cvmx_read_csr(CVMX_MIO_RST_BOOT);
		return (rst_boot.s.pnr_mul * 50000000);
	} else {
		/* For older chips, this is the same as the core clock */
		return (octeon_get_cpu_multiplier () * 50000000);
	}

}

uint32_t calculate_header_crc(const bootloader_header_t * header);

/**
 * Validate image header. Intended for actual header discovery.
 * Not NAND or NOR specific
 *
 * @param header     Address of expected image header.
 *
 * @return 0  on success
 *         1  on failure
 */
/* something way too large, but it should not hopefully run over memory end */
#define BOOTLOADER_MAX_SIZE 0x200000
int validate_header(const bootloader_header_t * header);

extern uint32_t get_image_size(const bootloader_header_t * header);

int validate_bootloader_header(const bootloader_header_t * header)
{
	if (header->magic == BOOTLOADER_HEADER_MAGIC) {
		/* Check the CRC of the header */
		if (calculate_header_crc(header) == header->hcrc)
			return 1;
		else
			return 0;
	}
	return 0;
}

/**
 * This function is called by the failsafe bootloader to find and validate
 * a normal bootloader image to boot.  It does this by scanning for a header
 * (using the magic number.)  If it finds a header, it checks the CRC of the
 * header. If the CRC is valid, then it checks the CRC of the entire image.
 * If the image CRC is valid, then the normal image is booted.
 * If a valid normal image is not found, then the failsafe image is booted.
 *
 * @param reloc_offset
 *               Relocation offset as computed by the assembly code.  This
 * 		 represents the offset from the address at which the image
 *		 was linked at to the address it is being run from.
 *
 * @return Address of the valid bootloader to jump to, or 0 if no valid
 *         bootloader is found.
 */
extern int _start;		/* symbol at start of code */
#ifndef CONFIG_SYS_NO_FLASH


uint32_t octeon_find_and_validate_normal_bootloader(int rescan_flag)
{

	bootloader_header_t *header;
	unsigned char *scan_ptr;
	unsigned char *scan_end;

	if (!rescan_flag && (gd->bd->bi_bootflags & OCTEON_BD_FLAG_BOOT_FLASH)) {
		/* Speed up flash */
		set_flash_clock();

		printf ("\nLooking for valid bootloader image....\n");
		/* Read our header to figure out where to start scanning */
		header = (void *)&_start;
		/* In TLB mode, this will be a virtual address, but we need to
		 * start execution of the normal bootloader from the physical
		 * address space.
		 */
		uint64_t addr = uboot_tlb_ptr_to_phys(header);
		header = (void *)(0xA0000000 | (uint32_t) addr);
	} else if (!rescan_flag &&
		   (gd->bd->bi_bootflags & OCTEON_BD_FLAG_BOOT_CACHE)) {
			header = (void *)0xBFC00000;
#if 0
	} else if (rescan_flag &&
		   (gd->bd->bi_bootflags & OCTEON_BD_FLAG_BOOT_CACHE)) {
#endif
	} else {
		header = (void *)(CONFIG_SYS_FLASH_BASE);
	}

	debug("%s: Header at 0x%p\n", __func__, header);
	/* Give up if we can't read our own header - something is really
	 * wrong
	 */
	if (!validate_bootloader_header (header) && !rescan_flag) {
		printf("ERROR: can't validate our own header.\n");
		return 0;
	}

	debug("%s: Header is valid\n", __func__);
	scan_ptr = (void *)header;
	scan_end = scan_ptr + MAX_NOR_SEARCH_ADDR;

#ifndef CONFIG_OCTEON_NO_FAILSAFE
	/* Scan for header starting at end of failsafe image, if we expect
	 * one to exist.
	 */
	if (rescan_flag)
		scan_ptr += 0x30000;
	else
		scan_ptr +=
		    ((header->dlen + header->hlen +
		      LOOKUP_STEP) / LOOKUP_STEP) * LOOKUP_STEP;
#endif
	debug("%s: Scanning from 0x%p to 0x%p\n", __func__, scan_ptr, scan_end);

	for (; scan_ptr <= scan_end; scan_ptr += LOOKUP_STEP) {
		header = (bootloader_header_t *) scan_ptr;
		debug("%s: Testing header at 0x%p\n", __func__, header);
		if (validate_bootloader_header(header)) {

			/* We have a valid header, now see if the image is
			 * valid
			 */
			if (header->dcrc ==
			    crc32(0, scan_ptr + header->hlen, header->dlen)) {
				/* We have found a valid image, so boot it */
				return (uint32_t)scan_ptr;
			} else {
				printf("WARNING: Ignoring corrupt image at "
				       "addres %p\n", scan_ptr);
			}
		}
	}

	debug("%s: Normal bootloader not found\n", __func__);
	return 0;

}
#endif

#ifndef OCTEON_GPIO_FAILSAFE_BIT
# define OCTEON_GPIO_FAILSAFE_BIT	0
#endif
/* Scan the NOR flash for a valid 'normal' (non-failsafe) image to boot.
 * This does it's own serial port initialization since we want to call it
 * very early in the boot process.
 */
int failsafe_scan(void)
{
	uint32_t __attribute__ ((unused))addr;
	uint32_t data;

	/* Manually set up the uart here so that we can output status.
	 * The uart will be re-initialized later in boot as well.
	 */
	octeon_uart_setup2(0, octeon_get_ioclk_hz(), 115200);

	/* Determine where we are booting from.  Could be one of
	 * 0x1fc00000 - base of flash, so we could be the failsafe image.
	 * 0x1fcxxxxx - non-base of flash, we are not the failsafe image
	 * RAM        - we are booting from DRAM,
	 */
#ifndef CONFIG_SYS_NO_FLASH
	uint64_t phy_addr = uboot_tlb_ptr_to_phys (&_start);
	debug("%s: our physical start address: 0x%llx\n", __func__, phy_addr);
	if (phy_addr >= 0x10000000ull && phy_addr < 0x20000000ull) {
		cvmx_mio_boot_reg_cfgx_t __attribute__((unused)) reg_cfg;
		reg_cfg.u64 = cvmx_read_csr (CVMX_MIO_BOOT_REG_CFG0);
		gd->ogd.uboot_flash_address = CONFIG_SYS_FLASH_BASE +
					      ((0x1fc0 - reg_cfg.s.base) << 16);
		gd->ogd.uboot_flash_size =
			get_image_size((bootloader_header_t *)(0x1fc00000));
# ifndef CONFIG_OCTEON_NO_FAILSAFE
		if (reg_cfg.s.base == 0x1fc0)
			gd->flags |= GD_FLG_FAILSAFE_MODE;
# endif
		debug("%s: U-Boot flash address: 0x%lx\n",
		      __func__, gd->ogd.uboot_flash_address);
	} else if (gd->bd->bi_bootflags & OCTEON_BD_FLAG_BOOT_CACHE) {
		cvmx_mio_boot_reg_cfgx_t __attribute__((unused)) reg_cfg;
		debug("%s: Booting out of cache\n", __func__);
		reg_cfg.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFG0);
		gd->ogd.uboot_flash_address = CONFIG_SYS_FLASH_BASE +
					      ((0x1fc0 - reg_cfg.s.base) << 16);
		gd->ogd.uboot_flash_size =
			get_image_size((bootloader_header_t *)(0x1fc00000));
		debug("%s: MIO_BOOT_REG_CFG0 address: 0x%x0000\n",
		      __func__, reg_cfg.s.base);
# ifndef CONFIG_OCTEON_NO_FAILSAFE
		if (reg_cfg.s.base == 0x1fc0)
			gd->flags |= GD_FLG_FAILSAFE_MODE;
# endif
		debug("flash address: 0x%lx, size: 0x%lx, failsafe: %s\n",
		      gd->ogd.uboot_flash_address, gd->ogd.uboot_flash_size,
		      gd->flags & GD_FLG_FAILSAFE_MODE ? "yes" : "no");

	} else
#endif
	{
		debug("%s: Booting out of RAM\n", __func__);
		gd->flags |= GD_FLG_RAM_RESIDENT;
		gd->flags |= (GD_FLG_DDR0_CLK_INITIALIZED);
		gd->flags |= (GD_FLG_DDR1_CLK_INITIALIZED);
		return 0;
	}

	if (!(gd->flags & GD_FLG_FAILSAFE_MODE))
		return 0;
#if 0
	data = cvmx_gpio_read();
	if (data & (1 << OCTEON_GPIO_FAILSAFE_BIT)) {
		printf ("GPIO bit %d set, booting failsafe image\n",
			OCTEON_GPIO_FAILSAFE_BIT);
		return 0;
	}
#endif
#ifndef CONFIG_SYS_NO_FLASH
	debug("%s: Searching for normal bootloader\n", __func__);
	addr = octeon_find_and_validate_normal_bootloader(0);
	if (addr) {
		printf ("Jumping to start of image at address 0x%08x\n", addr);
		((void (*)(void))addr)();
	}
#endif
	printf ("No valid bootloader found, booting failsafe image.\n");
	return 0;
}

char *lowcase(char *str_arg)
{
	char *str = str_arg;
	while (*str) {
		*str = tolower(*str);
		str++;
	}
	return str_arg;
}

uint64_t octeon_get_io_clock_rate(void)
{
	return cvmx_clock_get_rate(CVMX_CLOCK_SCLK);
}

/*
 * Set the current core's cvmcount counter to the value of the
 * IPD_CLK_COUNT.  We do this on all cores as they are brought
 * on-line.  This allows for a read from a local cpu register to
 * access a synchronized counter.
 *
 * On CPU_CAVIUM_OCTEON2 the IPD_CLK_COUNT is scaled by rdiv/sdiv.
 */
void octeon_init_cvmcount(void)
{
        unsigned long flags;
        unsigned loops = 2;
	uint64_t rdiv, sdiv;
	uint64_t f;

	if (!OCTEON_IS_OCTEON1PLUS()) {
		cvmx_mio_rst_boot_t rst_boot;
		rst_boot.u64 = cvmx_read_csr(CVMX_MIO_RST_BOOT);
		rdiv = rst_boot.s.c_mul;	/* CPU clock */
		sdiv = rst_boot.s.pnr_mul;	/* I/O clock */
		f = (0x8000000000000000ull / sdiv) * 2;
	} else {
		rdiv = 0;
		f = 0;
	}

        /* Clobber loops so GCC will not unroll the following while loop. */
        asm("" : "+r" (loops));

        /*
         * Loop several times so we are executing from the cache,
         * which should give more deterministic timing.
         */
        while (loops--) {
                uint64_t ipd_clk_count = cvmx_read_csr(CVMX_IPD_CLK_COUNT);
                if (rdiv != 0) {
                        ipd_clk_count = ipd_clk_count * rdiv;
                        if (f != 0) {
                                asm("dmultu\t%[cnt],%[f]\n\t"
                                    "mfhi\t%[cnt]"
                                    : [cnt] "+r" (ipd_clk_count)
                                    :  [f] "r" (f)
                                    :  "hi", "lo");
                        }
                }
                write_64bit_c0_cvmcount(ipd_clk_count);
	}
}
