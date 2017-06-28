/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2004-2012
 * Cavium Inc., Inc. <support@cavium.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
#include <malloc.h>
#include <stdio_dev.h>
#include <timestamp.h>
#include <version.h>
#include <net.h>
#include <environment.h>
#include <nand.h>
#include <flash.h>
#include <nand.h>
#include <addr_map.h>
#include <errno.h>
#include <watchdog.h>
#include <asm/mipsregs.h>
#include <sata.h>
#include <miiphy.h>
#include <linux/ctype.h>
#ifdef CONFIG_CMD_USB
# include <usb.h>
#endif
#ifdef CONFIG_CMD_DTT
# include <dtt.h>
#endif
#include <search.h>
#include <asm/addrspace.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-core.h>
#include <asm/arch/cvmx-bootloader.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon-boot-info.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/octeon-pci-console.h>
#include <asm/arch/cvmx-bootmem.h>
#include <asm/arch/cvmx-qlm.h>
#include <asm/arch/cvmx-adma.h>
#include <asm/arch/cvmx-helper-jtag.h>
#include <asm/arch/cvmx-l2c.h>
#include <asm/arch/octeon_cf.h>
#include <asm/arch/octeon_boot_bus.h>
#include <asm/arch/octeon_fdt.h>
#include <asm/arch/octeon_board_common.h>
#include <asm/arch/octeon_mdio.h>

DECLARE_GLOBAL_DATA_PTR;

#define DRAM_LATE_ZERO_OFFSET  0x100000ull

/** Short cut to convert a number to megabytes */
#define MB(X)	((uint64_t)(X) * (uint64_t)(1024 * 1024))

#if ( !defined(CONFIG_ENV_IS_NOWHERE) && \
      ((((CONFIG_ENV_ADDR+CONFIG_ENV_SIZE) < CONFIG_SYS_MONITOR_BASE) || \
       (CONFIG_ENV_ADDR >= (CONFIG_SYS_MONITOR_BASE + CONFIG_SYS_MONITOR_LEN)) ) \
       || defined(CONFIG_ENV_IS_IN_NVRAM)))
#  define	TOTAL_MALLOC_LEN	(CONFIG_SYS_MALLOC_LEN + CONFIG_ENV_SIZE)
#else
#  define	TOTAL_MALLOC_LEN	CONFIG_SYS_MALLOC_LEN
#endif

/* #undef DEBUG */

extern ulong uboot_end_data;
extern ulong uboot_end;
extern char uboot_start;

extern char uboot_prompt[];
#ifdef CONFIG_SYS_HUSH_PARSER
extern char uboot_prompt_hush_ps2[];
#endif

extern volatile uint64_t pci_mem1_base;
extern volatile uint64_t pci_mem2_base;

#ifdef CONFIG_OF_LIBFDT
/* Device tree */
extern char __dtb_begin;
extern char __dtb_end;
#endif

ulong monitor_flash_len;

const char version_string[] =
    U_BOOT_VERSION " (Build time: " U_BOOT_DATE " - " U_BOOT_TIME ")";

extern struct serial_device octeon_serial0_device;
#ifdef CONFIG_SYS_PCI_CONSOLE
extern struct serial_device octeon_pci0_serial_device;
#endif

/*
 * mips_io_port_base is the begin of the address space to which x86 style
 * I/O ports are mapped.
 */
const unsigned long mips_io_port_base = 0;

/* Used for error checking on load commands */
ulong load_reserved_addr = 0;	/* Base address of reserved memory for loading */
ulong load_reserved_size = 0;	/* Size of reserved memory for loading */

extern int timer_init(void);
extern int octeon_bist(void);

#ifdef CONFIG_CMD_IDE
/* defined in octeon_board_cf.c or board specific file. */
extern int octeon_board_ide_init(void);
#endif
/* Defined in board/octeon/common/octeon_board_display.c */
extern int display_board_info(void);

#if defined(CONFIG_OCTEON_FLASH) && !defined(CONFIG_ENV_IS_IN_NAND)
/* Defined in octeon_flash.c */
extern void octeon_fix_env_reloc(void);
#endif

#define SET_K0(val)	\
	asm volatile ("add $26, $0, %[rt]" :: [rt] "d" (val):"memory")

#define STACK_SIZE	(16*1024UL + CONFIG_SYS_BOOTPARAMS_LEN)

int __board_early_init_f(void)
{
	/*
	 * Nothing to do in this dummy implementation
	 */
	return 0;
}
int board_early_init_f(void)
	__attribute__((weak, alias("__board_early_init_f")));

#ifdef CONFIG_OF_LIBFDT
/**
 * Perform pre-relocation FDT initialization.
 */
int __board_fdt_init_f(void)
{
	gd->fdt_blob = (void *)MAKE_KSEG0((uint32_t)(&__dtb_begin));
	return fdt_check_header(gd->fdt_blob);
}

int board_fdt_init_f(void)
	__attribute__((weak, alias("__board_fdt_init_f")));

void __board_fdt_get_limits(char **begin, char **end)
{
	*begin = &__dtb_begin;
	*end = &__dtb_end;
}

void board_fdt_get_limits(char **begin, char **end)
	__attribute__((weak, alias("__board_fdt_get_limits")));
#endif

#ifdef CONFIG_CMD_NET
void __board_net_preinit(void)
{
	return;
}

void board_net_preinit(void)
	__attribute__((weak, alias("__board_net_preinit")));

void __board_net_postinit(void)
{
	return;
}

void board_net_postinit(void)
	__attribute__((weak, alias("__board_net_postinit")));
#endif

#if 0
static int init_func_ram(void)
{
	puts("DRAM:  ");
	if ((gd->ram_size) > 0) {
		print_size(gd->ram_size, "\n");
		return 0;
	}
	puts("*** failed ***\n");
	return 1;
}
#endif

static int display_banner(void)
{

	printf("\n\n%s\n\n", version_string);
	return (0);
}

#ifndef CONFIG_SYS_NO_FLASH
static void display_flash_config(ulong size)
{
	puts("Flash: ");
	print_size(size, "\n");
}
#endif

static int init_baudrate(void)
{
	char tmp[64];		/* long enough for environment variables */
	int i = getenv_f("baudrate", tmp, sizeof(tmp));

	gd->baudrate = (i > 0)
	    ? (int)simple_strtoul(tmp, NULL, 10)
	    : CONFIG_BAUDRATE;

	return 0;
}

/* Import any environment variables passed via a remote bootloader.  It is
 * only active if U-Boot was already loaded into RAM.
 */
static int import_ram_env(void)
{
	env_t *ep = (env_t *)cvmx_phys_to_ptr(U_BOOT_RAM_ENV_ADDR);
	char *s;
	uint32_t crc;
	int i;
	int old_env_size;

	if ((gd->flags & GD_FLG_RAM_RESIDENT) &&
	    !(gd->bd->bi_bootflags & OCTEON_BD_FLAG_BOOT_CACHE)) {
		/* Import any environment variables passed by the remote
		 * bootloader.
		 * Make a copy of the passed-in environment variables and
		 * append the default environment variables after them.
		 * Later on the passed-in environment variables will be
		 * re-imported to override any that are stored in flash.
		 */
		debug("Importing environment from RAM address 0x%x\n",
		      U_BOOT_RAM_ENV_ADDR);
		crc = crc32(0, ep->data, U_BOOT_RAM_ENV_SIZE - U_BOOT_RAM_ENV_CRC_SIZE);
		if (ep->crc != crc) {
			printf("Environment passed by remote boot loader has "
			       "a bad CRC!\n");
			return 0;
		}
		/* Copy the environment before modifying */
		memcpy((char *)cvmx_phys_to_ptr(U_BOOT_RAM_ENV_ADDR_2), ep,
		       U_BOOT_RAM_ENV_SIZE);
		ep = (env_t *)cvmx_phys_to_ptr(U_BOOT_RAM_ENV_ADDR_2);
		/* Get size of environment minus last terminator*/
		s = (char *)&ep->data[0];
		for (i = 0; s[i] != '\0' || s[i+1] != '\0'; i++)
			;
		i++;
		debug("RAM environment is %u bytes\n", i);
		/* Get size of existing environment */
		s = (char *)gd->env_addr;
		if (s) {
			for (old_env_size = 0;
			     s[old_env_size] != '\0' ||
		             s[old_env_size+1] != '\0';
			     old_env_size++)
				;
			/* Append the old environment to the end of the new one */
			old_env_size = min(old_env_size, U_BOOT_RAM_ENV_SIZE - i);
			if (old_env_size >= U_BOOT_RAM_ENV_SIZE - i) {
				printf("Truncating old environment from %u to %u bytes\n",
				old_env_size, U_BOOT_RAM_ENV_SIZE - i);
				old_env_size = U_BOOT_RAM_ENV_SIZE - i;
			}

			if (old_env_size > 0)
				memcpy(&ep->data[i], (char *)gd->env_addr,
				       old_env_size + 1);
		}
		gd->env_addr =
			(unsigned long)cvmx_phys_to_ptr(U_BOOT_RAM_ENV_ADDR_2 +
							U_BOOT_RAM_ENV_CRC_SIZE);
		gd->env_valid = 1;
#if 0
		/* Print out environment variables */
		s = (char *)ep->data;
		puts("Early Environment:\n");
		while (*s) {
			putc(*s++);
			if (*s == '\0') {
				s++;
				puts("\n");
			}
		}
#endif
	}
	return 0;
}

int init_dram(void)
{
	int mem_mbytes = 0;
	u32 measured_ddr_hertz;
	u32 ddr_config_valid_mask;
	char *eptr;
	u32 ddr_hertz;
	u32 ddr_ref_hertz;
	int ram_resident = 0;
	u32 cpu_id = cvmx_get_proc_id();
	const ddr_configuration_t *ddr_config_ptr;
	debug("Initializing DRAM\n");
#if CONFIG_OCTEON_SIM_NO_DDR
# if defined(CONFIG_OCTEON_NAND_STAGE2) || defined(CONFIG_OCTEON_EMMC_STAGE2) \
     || defined(CONFIG_OCTEON_AUTHENTIK_STAGE2)
	/* For simulating NAND boot, we don't use normal oct-sim helper, so we
	 * don't get the DRAM size in the special location
	 */
	gd->ram_size = MB(128);
# else
	gd->ram_size = MB(*((uint32_t *) 0x9ffffff8));
# endif
	return (0);
#endif

	if (gd->flags & GD_FLG_RAM_RESIDENT)
		ram_resident = 1;

	if (ram_resident && getenv("dram_size_mbytes")) {
		debug("U-Boot is RAM resident\n");
		/* DRAM size has been based by the remote entity that configured
		 * the DRAM controller, so we use that size rather than
		 * re-computing by reading the DIMM SPDs. */
		mem_mbytes = simple_strtoul(getenv("dram_size_mbytes"),
					    NULL, 0);
		printf("Using DRAM size from environment: %d MBytes\n",
		       mem_mbytes);
		int ddr_int = 0;
		if (OCTEON_IS_MODEL(OCTEON_CN56XX)) {
			/* Check to see if interface 0 is enabled */
			cvmx_l2c_cfg_t l2c_cfg;
			l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);
			if (!l2c_cfg.s.dpres0) {
				debug("DDR interface 0 disabled, checking 1\n");
				if (l2c_cfg.s.dpres1) {
					debug("DDR interface 1 enabled\n");
					ddr_int = 1;
				} else
					printf("ERROR: no DRAM controllers "
					       "initialized\n");
			}
		}
		gd->ogd.ddr_clock_mhz =
		    divide_nint(measure_octeon_ddr_clock(cpu_id,
							 NULL,
							 gd->ogd.cpu_clock_mhz *
							 1000 * 1000,
							 0, 0,
							 ddr_int, 0), 1000000);
		debug("DDR clock is %uMHz\n", gd->ogd.ddr_clock_mhz);
	} else {	/* Not RAM-resident case */
		debug("U-Boot is not RAM-resident\n");
		ddr_config_ptr = lookup_ddr_config_structure(cpu_id,
							     gd->ogd.board_desc.board_type,
							     gd->ogd.board_desc.rev_major,
							     gd->ogd.board_desc.rev_minor,
							     &ddr_config_valid_mask);
		if (!ddr_config_ptr) {
			printf("ERROR: unable to determine DDR configuration "
			       "for board type: %s (%d)\n",
			       cvmx_board_type_to_string(gd->ogd.board_desc.board_type),
			       gd->ogd.board_desc.board_type);
			return (-1);
		}
		/* Check for special case of mismarked 3005 samples,
		 * and adjust cpuid
		 */
		if ((cpu_id == OCTEON_CN3010_PASS1) &&
		    (cvmx_read_csr(CVMX_L2D_FUS3) & (1ull << 34)))
			cpu_id |= 0x10;

		ddr_hertz = gd->ogd.ddr_clock_mhz * 1000000;
		if ((eptr = getenv("ddr_clock_hertz")) != NULL) {
			ddr_hertz = simple_strtoul(eptr, NULL, 0);
			gd->ogd.ddr_clock_mhz = divide_nint(ddr_hertz, 1000000);
			printf("Parameter found in environment.  "
			       "ddr_clock_hertz = %d\n", ddr_hertz);
		}

		ddr_ref_hertz = getenv_ulong("ddr_ref_hertz", 10,
					     gd->ogd.ddr_ref_hertz);
		debug("Initializing DDR, clock = %uhz, reference = %uhz\n",
		      ddr_hertz, ddr_ref_hertz);
		debug("LMC0_DCLK_CNT: 0x%llx\n",
		      cvmx_read_csr(CVMX_LMCX_DCLK_CNT(0)));

		mem_mbytes = octeon_ddr_initialize(cpu_id,
						   cvmx_clock_get_rate(CVMX_CLOCK_CORE),
						   ddr_hertz,
						   ddr_ref_hertz,
						   ddr_config_valid_mask,
						   ddr_config_ptr,
						   &measured_ddr_hertz,
						   gd->ogd.board_desc.board_type,
						   gd->ogd.board_desc.rev_major,
						   gd->ogd.board_desc.rev_minor);

		gd->ogd.ddr_clock_mhz =
		    divide_nint(measured_ddr_hertz, 1000000);

		debug("Measured DDR clock %d Hz\n", measured_ddr_hertz);
		debug("Mem size in MBYTES: %u\n", mem_mbytes);

		if (measured_ddr_hertz != 0) {
			if (!gd->ogd.ddr_clock_mhz) {
				/* If ddr_clock not set, use measured clock
				 * and don't warn
				 */
				gd->ogd.ddr_clock_mhz =
				    divide_nint(measured_ddr_hertz, 1000000);
			} else if ((measured_ddr_hertz > ddr_hertz + 3000000) ||
				   (measured_ddr_hertz < ddr_hertz - 3000000)) {
				printf("\nWARNING:\n");
				printf("WARNING: Measured DDR clock mismatch!  "
				       "expected: %lld MHz, measured: %lldMHz, "
				       "cpu clock: %d MHz\n",
				       divide_nint(ddr_hertz, 1000000),
				       divide_nint(measured_ddr_hertz, 1000000),
				       gd->ogd.cpu_clock_mhz);
				if (!(gd->flags & GD_FLG_RAM_RESIDENT))
					printf("WARNING: Using measured clock "
					       "for configuration.\n");
				printf("WARNING:\n\n");
				gd->ogd.ddr_clock_mhz =
				    divide_nint(measured_ddr_hertz, 1000000);
			}
		}

		if (gd->flags & GD_FLG_CLOCK_DESC_MISSING)
			printf("Warning: Clock descriptor tuple not found in "
			       "eeprom, using defaults\n");

		if (gd->flags & GD_FLG_BOARD_DESC_MISSING)
			printf("Warning: Board descriptor tuple not found in "
			       "eeprom, using defaults\n");

		if (mem_mbytes > 0 && getenv("dram_size_mbytes")) {
			/* Override the actual DRAM size. */
			uint32_t new_mem_mbytes;
			new_mem_mbytes = simple_strtoul(getenv("dram_size_mbytes"),
						        NULL, 0);
			if (new_mem_mbytes < mem_mbytes) {
				printf("!!! Overriding DRAM size: "
				       "dram_size_mbytes = %d MBytes !!!\n",
				       mem_mbytes);
				mem_mbytes = new_mem_mbytes;
			} else {
				printf("!!! Memory size override failed due to "
				       "actual memory size being smaller than "
				       "requested size !!!\n");
			}
		}
	}

	if (mem_mbytes > 0) {
		gd->ram_size = MB(mem_mbytes);

		/* For 6XXX generate a proper error when reading/writing
		 * non-existant memory locations.
		 */
		cvmx_l2c_set_big_size((uint64_t)mem_mbytes, 0);

		debug("Ram size %uMiB (0x%llx)\n", mem_mbytes, gd->ram_size);

		/* If already running in memory don't zero it. */
		/* Zero the low Megabyte of DRAM used by bootloader.
		 * The rest is zeroed later when running from DRAM.
		 */
#if !defined(CONFIG_NO_CLEAR_DDR)
		/* Preserve environment in RAM if it exists */
		if (!(gd->flags & GD_FLG_MEMORY_PRESERVED)) {
			if (gd->flags & GD_FLG_RAM_RESIDENT) {
				debug("Preserving environment in RAM\n");
				octeon_bzero64_pfs(0, U_BOOT_RAM_ENV_ADDR);
				octeon_bzero64_pfs(U_BOOT_RAM_ENV_ADDR_2 + U_BOOT_RAM_ENV_SIZE,
						   DRAM_LATE_ZERO_OFFSET - (U_BOOT_RAM_ENV_ADDR_2 + U_BOOT_RAM_ENV_SIZE));
			} else {
				debug("Clearing memory from 0 to %llu\n",
				      DRAM_LATE_ZERO_OFFSET);
				octeon_bzero64_pfs(0, DRAM_LATE_ZERO_OFFSET);
			}
		}
		debug("Done clearing memory\n");
#endif /* !CONFIG_NO_CLEAR_DDR */
		return 0;
	} else
		return -1;
}

#define DO_SIMPLE_DRAM_TEST_FROM_FLASH  0
#if DO_SIMPLE_DRAM_TEST_FROM_FLASH
/* Very simple DRAM test to run from flash for cases when bootloader boot
 * can't complete.
 */
static int dram_test(void)
{
	u32 *addr;
	u32 *start = (void *)0x80400000;
	u32 *end = (void *)0x81400000;
	u32 val, incr, readback, pattern;
	int error_limit;
	int i;

	if (gd->flags & GD_FLG_RAM_RESIDENT) {
		printf("Skipping DDR test in ram-resident mode.\n");
		return 0;
	}
	pattern = 0;
	printf("Performing simple DDR test from flash.\n");

#define DRAM_TEST_ERROR_LIMIT 200

	incr = 0x4321;
	for (i = 0; i < 10; i++) {
		error_limit = DRAM_TEST_ERROR_LIMIT;

		printf("\rPattern %08X  Writing..."
		       "%12s" "\b\b\b\b\b\b\b\b\b\b", pattern, "");

		for (addr = start, val = pattern; addr < end; addr++) {
			*addr = val;
			val += incr;
		}

		puts("Reading...");

		for (addr = start, val = pattern; addr < end; addr++) {
			readback = *addr;
			if (readback != val && error_limit-- > 0) {
				if (error_limit + 1 == DRAM_TEST_ERROR_LIMIT)
					puts("\n");
				printf("Mem error @ 0x%08X: "
				       "found %08X, expected %08X, "
				       "xor %08X\n",
				       (uint) addr, readback, val,
				       readback ^ val);
			}
			val += incr;
		}

		/*
		 * Flip the pattern each time to make lots of zeros and
		 * then, the next time, lots of ones.  We decrement
		 * the "negative" patterns and increment the "positive"
		 * patterns to preserve this feature.
		 */
		if (pattern & 0x80000000)
			pattern = -pattern;	/* complement & increment */
		else
			pattern = ~pattern;
		if (error_limit <= 0)
			printf("Too many errors, printing stopped\n");
		incr = -incr;
	}

	puts("\n");
	return 0;

}
#endif

#if OCTEON_CONFIG_DFM && !CONFIG_OCTEON_SIM_NO_DDR
int init_dfm(void)
{
	int mem_mbytes = 0;

	/* Check if HFA is present */
	if (!octeon_has_feature(OCTEON_FEATURE_HFA))
		return 0;

	if (getenv("disable_dfm")) {
		printf("disable_dfm is set, skipping DFM initialization\n");
		return 0;
	}
	mem_mbytes = octeon_dfm_initialize();

	if (mem_mbytes >= 0) {
		gd->ogd.dfm_ram_size = MB(mem_mbytes);
		return 0;
	} else {
		return -1;
	}
}
#endif

/* Clears DRAM, while taking care to not overwrite DRAM already in use
 * by u-boot
 */
void octeon_clear_mem_pfs(u64 base_addr, u64 size)
{
	u64 ub_base = gd->bd->bi_uboot_ram_addr;
	u64 ub_size = gd->bd->bi_uboot_ram_used_size;

	debug("Clearing base address: 0x%llx, size: 0x%llx, "
	      "ub_base: 0x%llx, ub_size: 0x%llx\n",
	      base_addr, size, ub_base, ub_size);
	debug("Stack: 0x%p\n", &ub_size);

	if (ub_base >= base_addr && ub_base < (base_addr + size)) {
		/* We need to avoid overwriting the u-boot section */
		octeon_bzero64_pfs(MAKE_XKPHYS(base_addr), ub_base - base_addr);
		octeon_bzero64_pfs(MAKE_XKPHYS(ub_base + ub_size),
				   size - (ub_base - base_addr) - (ub_size));
	} else {
		/* just clear it */
		octeon_bzero64_pfs(MAKE_XKPHYS(base_addr), size);
	}
	debug("Done clearing memory, ub_base: 0x%llx\n", ub_base);
}

/*
 * Breath some life into the board...
 *
 * The first part of initialization is running from Flash memory;
 * its main purpose is to initialize the RAM so that we
 * can relocate the monitor code to RAM.
 */
#ifndef CONFIG_OCTEON_NO_BOOT_BUS
int octeon_boot_bus_init_board(void);
#endif
int octeon_boot_bus_moveable_init(void);
int early_board_init(void);
int failsafe_scan(void);
int octeon_bist(void);
int late_board_init(void);

/*
 * All attempts to come up with a "common" initialization sequence
 * that works for all boards and architectures failed: some of the
 * requirements are just _too_ different. To get rid of the resulting
 * mess of board dependend #ifdef'ed code we now make the whole
 * initialization sequence configurable to the user.
 *
 * The requirements for any new initalization function is simple: it
 * receives a pointer to the "global data" structure as it's only
 * argument, and returns an integer return code, where 0 means
 * "continue" and != 0 means "fatal error, hang the system".
 */
typedef int (init_fnc_t) (void);

init_fnc_t *init_sequence[] = {
	console_init_f,
	board_early_init_f,
#ifdef CONFIG_OF_LIBFDT
	board_fdt_init_f,
#endif
#if (!CONFIG_OCTEON_SIM_NO_FLASH && !defined(CONFIG_OCTEON_NAND_STAGE2) && \
     !defined(CONFIG_OCTEON_EMMC_STAGE2) && \
     !defined(CONFIG_OCTEON_AUTHENTIK_STAGE2)) \
    || defined(CONFIG_OCTEON_DISABLE_FAILSAFE_SCAN)
	failsafe_scan,
#endif
#if !defined(CONFIG_OCTEON_SIM_NO_FLASH) && !defined(CONFIG_OCTEON_NO_BOOT_BUS)
	octeon_boot_bus_init,
#endif
	octeon_boot_bus_moveable_init,
	timer_init,
	early_board_init,
	env_init,		/* initialize environment */
	import_ram_env,		/* import env if booting from RAM */
	init_baudrate,		/* initialze baudrate settings */
	serial_init,		/* serial communications setup */
	console_init_f,
	display_banner,		/* say that we are here */
#if !CONFIG_OCTEON_SIM_HW_DIFF
	octeon_bist,
#endif
	init_dram,
#if DO_SIMPLE_DRAM_TEST_FROM_FLASH
	dram_test,
#endif
#if OCTEON_CONFIG_DFM
	init_dfm,
#endif
	checkboard,
	display_board_info,
	NULL,
};

void board_init_f(ulong bootflag)
{
	u64 new_gd_addr;
	gd_t gd_data;
	bd_t bd;		/* Local copy */
	init_fnc_t **init_fnc_ptr;
	ulong len = (ulong) & uboot_end - CONFIG_SYS_MONITOR_BASE;
	u64 addr;
	u64 u_boot_mem_top;	/* top of memory, as used by u-boot */
	int init_func_count = 0;
	u64 heap_base_addr;
	u64 map_size;
	u64 u_boot_base_phys;	/* Physical address of u-boot code */
	u64 addr_sp_phys;	/* Stack pointer physical address */
	u64 addr_bd_phys;	/* Address of board data structure in RAM */

	/* Offsets from u-boot base for stack/heap.  These offsets will be
	 * the same for both physical and virtual maps.
	 */

	SET_K0(&gd_data);

	/* Pointer is writeable since we allocated a register for it */
	memset((void *)gd, 0, sizeof(gd_t));
	memset((void *)&bd, 0, sizeof(bd));

	bd.bi_bootflags = bootflag;

	gd->bd = &bd;
	/* Round u-boot length up to a nice boundary */
	len = (len + 0xFFFF) & ~0xFFFF;

	extern int init_twsi_bus(void);
	init_twsi_bus();	/* Slow down TWSI bus */
	mdelay(100);


	/* Enable soft BIST so that BIST is run on soft resets. */
	if (!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2) &&
	    !OCTEON_IS_MODEL(OCTEON_CN31XX))
		cvmx_write_csr(CVMX_CIU_SOFT_BIST, 1);

	/* Change default setting for CN58XX pass 2 */
	if (OCTEON_IS_MODEL(OCTEON_CN58XX_PASS2))
		cvmx_write_csr(CVMX_MIO_FUS_EMA, 2);

#if !CONFIG_OCTEON_SIM_HW_DIFF
	/* Disable the watchdog timer for core 0 since it may still be running
	 * after a soft reset as a PCI target.
	 */
	cvmx_write_csr(CVMX_CIU_WDOGX(0), 0);
#endif
#ifdef ENABLE_BOARD_DEBUG
	int gpio_status_data = 1;
#endif
	/* Initialize the serial port early */
	gd->baudrate = 115200;
	octeon_uart_setup(0);

	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
#ifdef ENABLE_BOARD_DEBUG
		gpio_status(gpio_status_data++);
#endif
		debug("Calling %d: 0x%p\n", init_func_count, *init_fnc_ptr);
		debug("%s: Boot flag: 0x%lx\n", __func__, bootflag);
		if ((*init_fnc_ptr) () != 0) {
			printf("hanging, init function: %d\n", init_func_count);
			if (OCTEON_IS_MODEL(OCTEON_CN63XX)) {
				/* 63XX_HACK sometimes dimms are not detected */
				printf("AUTO resetting board due to init failing!\n");
				do_reset(NULL, 0, 0, NULL);
			}
			hang();
		}
		init_func_count++;
	}

	if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X)) {
		/* Disable Fetch under fill on CN63XXp1 due to errata
		 * Core-14317
		 */
		u64 val;
		val = read_64bit_c0_cvmctl();
		val |= (1ull << 19);	/* CvmCtl[DEFET] */
		write_64bit_c0_cvmctl(val);
	}

	/* Apply workaround for Errata L2C-16862 */
	if (OCTEON_IS_MODEL(OCTEON_CN68XX_PASS1_X) ||
	      OCTEON_IS_MODEL(OCTEON_CN68XX_PASS2_0) ||
	      OCTEON_IS_MODEL(OCTEON_CN68XX_PASS2_1)) {
		cvmx_l2c_ctl_t l2c_ctl;
		cvmx_iob_to_cmb_credits_t iob_cmb_credits;
		cvmx_iob1_to_cmb_credits_t iob1_cmb_credits;

		l2c_ctl.u64 = cvmx_read_csr(CVMX_L2C_CTL);
		l2c_ctl.s.maxlfb = 6;
		cvmx_write_csr(CVMX_L2C_CTL, l2c_ctl.u64);

		iob_cmb_credits.u64 = cvmx_read_csr(CVMX_IOB_TO_CMB_CREDITS);
		iob_cmb_credits.s.ncb_rd = 6;
		cvmx_write_csr(CVMX_IOB_TO_CMB_CREDITS, iob_cmb_credits.u64);

		iob1_cmb_credits.u64 = cvmx_read_csr(CVMX_IOB1_TO_CMB_CREDITS);
		iob1_cmb_credits.s.pko_rd = 6;
		cvmx_write_csr(CVMX_IOB1_TO_CMB_CREDITS, iob1_cmb_credits.u64);
#if 0
		l2c_ctl.u64 = cvmx_read_csr(CVMX_L2C_CTL);
		iob_cmb_credits.u64 = cvmx_read_csr(CVMX_IOB_TO_CMB_CREDITS);
		iob1_cmb_credits.u64 = cvmx_read_csr(CVMX_IOB1_TO_CMB_CREDITS);
		printf("L2C_CTL[MAXLFB] = %d, IOB_TO_CMB_CREDITS[ncb_rd] = %d, IOB1_TO_CMB_CREDITS[pko_rd] = %d\n",
			l2c_ctl.s.maxlfb, iob_cmb_credits.s.ncb_rd,
			iob1_cmb_credits.s.pko_rd);
#endif
	}

	/*
	 * Now that we have DRAM mapped and working, we can
	 * relocate the code and continue running from DRAM.
	 */

	/* Put u-boot at the highest address available in DRAM.
	 * For 3XXX/5XXX chips, this will likely be in the range
	 * 0x410000000-0x41fffffff
	 */
	if (gd->ram_size <= MB(256)) {
		addr = MIN(gd->ram_size, MB(256));
		/* We need to leave some room for the named block structure that
		 * will be allocated when the bootmem allocator
		 * is initialized.
		 */
		addr -= CVMX_BOOTMEM_NUM_NAMED_BLOCKS *
			sizeof(cvmx_bootmem_named_block_desc_t) + 128;
	} else {
		if (OCTEON_IS_OCTEON1PLUS()) {
			u64 offset = MIN(MB(256), gd->ram_size - MB(256));
			addr = 0x410000000ULL + offset;
		} else {
			/* CN63XX and newer */
			u64 offset = gd->ram_size - MB(256);
			addr = 0x20000000ULL + offset;
		}
	}

	u_boot_mem_top = addr;

	/* Print out any saved error messages that were logged during startup
	 * before the serial port was enabled.  This only works if the
	 * error message is a string constant in flash, but most things
	 * early on will be.
	 */
	if (gd->ogd.err_msg)
		printf("Error message from before serial ready: %s\n",
		       gd->ogd.err_msg);

	/* We can reserve some RAM "on top" here.
	 */

	debug("U-Boot link addr: 0x%x\n", CONFIG_SYS_MONITOR_BASE);

	/* round down to next 4 kB limit.
	 */
	addr &= ~(4096 - 1);
	debug("Top of RAM usable for U-Boot at: %08llx\n", addr);

	/* Reserve memory for U-Boot code, data & bss */
	addr = (uint64_t) ((uint64_t) addr - (uint64_t) MAX(len, 512 * 1024));

	/* In TLB mapped case, stack/heap go above (higher address) u-boot in
	 * DRAM as the base address of u-boot has alignment constraints due to
	 * the TLB usage.
	 * Reserve space for them here (before u-boot base address is
	 * determined).
	 */
	addr -= STACK_SIZE;
	addr -= TOTAL_MALLOC_LEN;

	/* Determine alignment required to use a single TLB entry, and
	 * round the DRAM address down to that alignment.   The minimum
	 * alignment is 4 MBytes, as this is required for PCI BAR
	 * mappings.
	 */
	map_size = 1;
	while (map_size < u_boot_mem_top - addr)
		map_size = map_size << 1;
	map_size = map_size >> 1;	/* TLB page size is 1/2 of mapping
					 * alignment
					 */
	debug("Using TLB mapping size of 0x%llx bytes for DRAM mapping.\n",
	      map_size);
	addr &= ~(map_size - 1);
	u_boot_base_phys = addr;

	debug("Reserving %ldk for U-Boot at: %08llx\n", (len + 1023) / 1024, addr);
#if !CONFIG_OCTEON_SIM_SPEED
	debug("Clearing 0x%llx bytes at addr 0x%llx\n",
	      u_boot_mem_top - u_boot_base_phys,
	      u_boot_base_phys);
	octeon_bzero64_pfs(MAKE_XKPHYS(u_boot_base_phys),
			   (u_boot_mem_top - u_boot_base_phys +
			    OCTEON_BZERO_PFS_STRIDE_MASK) &
			   ~OCTEON_BZERO_PFS_STRIDE_MASK);
#endif

	/* Put heap after code/data when using TLB */
	heap_base_addr = u_boot_base_phys + len;
	debug("u_boot_base_phys: 0x%llx, len: 0x%lx\n", u_boot_base_phys, len);
	/* For the TLB case we want the stack to be TLB mapped.  Place it
	 * after the u-boot code/data/heap.
	 */
	addr_sp_phys = heap_base_addr + TOTAL_MALLOC_LEN + STACK_SIZE;
	debug("Reserving %dk for malloc() at: %08llx\n",
	      TOTAL_MALLOC_LEN >> 10, heap_base_addr);

	debug("Stack top physical address: 0x%llx\n", addr_sp_phys);
	/*
	 * (permanently) allocate a Board Info struct
	 * and a permanent copy of the "global" data
	 */

	addr_sp_phys -= sizeof(bd_t);
	addr_bd_phys = addr_sp_phys;
	gd->bd =
	    (bd_t *) (uint32_t) (CONFIG_SYS_MONITOR_BASE +
				 (addr_bd_phys - u_boot_base_phys));
	debug("Reserving %zu bytes for Board Info at: 0x%08llx\n", sizeof(bd_t),
	      addr_bd_phys);
	addr_sp_phys -= sizeof(gd_t);
	new_gd_addr = addr_sp_phys;
	debug("Reserving %zu Bytes for Global Data at: 0x%08llx\n",
	      sizeof(gd_t), new_gd_addr);

	/* Reserve memory for boot params.
	 */
	addr_sp_phys -= CONFIG_SYS_BOOTPARAMS_LEN;
	bd.bi_boot_params = addr_sp_phys;
	debug("Reserving %dk for boot params() at: %08llx\n",
	      CONFIG_SYS_BOOTPARAMS_LEN >> 10, addr_sp_phys);

	/*
	 * Finally, we set up a new (bigger) stack.
	 *
	 * Leave some safety gap for SP, force alignment on 16 byte boundary
	 * Clear initial stack frame
	 */
	addr_sp_phys -= 16;
	addr_sp_phys &= ~0xF;
	memset64(MAKE_XKPHYS(addr_sp_phys), 0, 8);

	bd.bi_uboot_ram_addr = u_boot_base_phys;
	bd.bi_uboot_ram_used_size = u_boot_mem_top - bd.bi_uboot_ram_addr;

	debug("Stack Pointer at:   %08llx, stack size: 0x%08lx\n",
	      addr_sp_phys, STACK_SIZE);
	printf("Base DRAM address used by u-boot: 0x%08llx, size: 0x%lx\n",
	       bd.bi_uboot_ram_addr, bd.bi_uboot_ram_used_size);
        puts("DRAM: ");
        print_size(gd->ram_size, "\n");
	debug("Top of fixed address reserved memory: 0x%08x\n",
	      BOOTLOADER_END_RESERVED_SPACE);

#ifdef DEBUG
#define PRINT_RESERVED_ADDR(X)	printf("Reserved address %s: 0x%08x\n",\
					       #X, X)
	PRINT_RESERVED_ADDR(EXCEPTION_BASE_BASE);
	PRINT_RESERVED_ADDR(BOOTLOADER_DEBUG_REG_SAVE_BASE);
	PRINT_RESERVED_ADDR(BOOTLOADER_DEBUG_STACK_BASE);
	PRINT_RESERVED_ADDR(BOOTLOADER_PCI_READ_BUFFER_BASE);
	PRINT_RESERVED_ADDR(BOOTLOADER_BOOTMEM_DESC_ADDR);
	PRINT_RESERVED_ADDR(BOOTLOADER_END_RESERVED_SPACE);
#undef PRINT_RESERVED_ADDR
#endif

	if ((BOOTLOADER_END_RESERVED_SPACE & 0x7fffffff) >=
	    (bd.bi_uboot_ram_addr & 0x7fffffff)) {
		printf("\n\n*************************************\n");
		printf("ERROR: U-boot memory usage overlaps fixed address "
		       "reserved area!\n");
		printf("U-boot base address: 0x%08qx, reserved top: 0x%08x\n",
		       bd.bi_uboot_ram_addr, BOOTLOADER_END_RESERVED_SPACE);
		printf("*************************************\n\n");
	}

	/*
	 * Save local variables to board info struct
	 */
	bd.bi_memstart = CONFIG_SYS_SDRAM_BASE;	/* start of  DRAM memory */
	bd.bi_memsize = gd->ram_size;		/* size  of  DRAM memory in bytes */
	bd.bi_baudrate = gd->baudrate;		/* Console Baudrate */
	bd.bi_uboot_map_size = map_size;	/* Size of TLB mapping */

	debug("gd address: %p, new_addr: 0x%llx\n", gd, new_gd_addr);
	/* Use 64-bit copy here, as new stack could be outside of the
	 * 32-bit address space (physically addressed).
	 * gd pointer is kseg0 address in scratch, so casting it to an
	 * int32 is proper.
	 */
	debug("Relocating bd from: %p to 0x%08llx, size: 0x%x\n",
	      &bd, MAKE_XKPHYS(addr_bd_phys), sizeof(bd));
	memcpy64(MAKE_XKPHYS(addr_bd_phys), (int32_t) (&bd), sizeof(bd));

	debug("Relocating gd from: %p to 0x%08llx, size: 0x%x\n",
	      gd, MAKE_XKPHYS(new_gd_addr), sizeof(*gd));
	memcpy64(MAKE_XKPHYS(new_gd_addr), (int32_t) gd, sizeof(*gd));
#ifdef DEBUG
	extern void *_start;
	debug("current address: 0x%llx\n", cvmx_ptr_to_phys(&_start));
	debug("cache data: 0x%llx\n", *(uint64_t *)(0x8f400000));
#endif

	debug("relocating and jumping to code in DRAM at addr: 0x%llx\n", addr);
	u32 stack_virt = CONFIG_SYS_MONITOR_BASE + addr_sp_phys -
	    u_boot_base_phys;
	u32 gd_virt = CONFIG_SYS_MONITOR_BASE + new_gd_addr - u_boot_base_phys;

	debug("Virtual stack top: 0x%x, virt new gd: 0x%x\n",
	      stack_virt, gd_virt);

	relocate_code_octeon(stack_virt, (void *)(u32) gd_virt,
			     MAKE_XKPHYS(addr), map_size);

	/* NOTREACHED - relocate_code() does not return */
}

/************************************************************************
 *
 * This is the next part if the initialization sequence: we are now
 * running from RAM and have a "normal" C environment, i. e. global
 * data can be written, BSS has been cleared, the stack size in not
 * that critical any more, etc.
 *
 ************************************************************************
 */

void board_init_r(gd_t * id, u64 dest_addr)
{
#ifdef CONFIG_OF_LIBFDT
	char *dtb_begin, *dtb_end;
#endif
	int64_t idle_loop;
	ulong size;
	char *s, *e;
	bd_t *bd;
	int i;
	int rc;

	/* Convince GCC to really do what we want, otherwise the old value is
	 * still used.
	 */
	SET_K0(id);

	gd = id;
	gd->flags |= GD_FLG_RELOC;	/* tell others: relocation done */

	debug("Now running in RAM - U-Boot at: %08llx\n", dest_addr);

	gd->reloc_off =
	    (dest_addr & 0xFFFFFFFF) - (u64) CONFIG_SYS_MONITOR_BASE;

	monitor_flash_len = (ulong) & uboot_end_data - dest_addr;

	debug("Relocation offset: 0x%llx, monitor base: 0x%08llx\n",
	      gd->reloc_off, (u64) CONFIG_SYS_MONITOR_BASE);

#if !defined(CONFIG_SYS_NO_FLASH)
	/* Here we need to update the boot bus mappings to put the flash
	 * boot bus mappings back at the expected locations.  We
	 * no longer need the adjusted mappings for the TLB usage.
	 */
	{
		cvmx_mio_boot_reg_cfgx_t __attribute__ ((unused)) reg_cfg;
		reg_cfg.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFG0);
		if (reg_cfg.s.en != 0) {
			reg_cfg.s.base = ((CONFIG_SYS_FLASH_BASE >> 16) & 0x1fff);
			cvmx_write_csr(CVMX_MIO_BOOT_REG_CFG0, reg_cfg.u64);
		}
	}
#endif

	bd = gd->bd;
	debug("U-Boot memory end: %p\n", &uboot_end);
	debug("Setting heap memory to start at 0x%x, size: 0x%x\n",
	      ((uint32_t)(&uboot_end) + 0xFFFF) & ~0xFFFF, TOTAL_MALLOC_LEN);

	mem_malloc_init(((u32)(&uboot_end) + 0xFFFF) & ~0xFFFF,
			TOTAL_MALLOC_LEN);
	debug("Adding map entry for 0x%x, address: 0x%llx, size: 0x%x, index: %d\n",
	      CONFIG_SYS_TEXT_BASE, bd->bi_uboot_ram_addr,
	      bd->bi_uboot_map_size, CONFIG_ADDR_MAP_CODE_IDX);

	/* Fix the gd environment address */
#if !defined(CONFIG_ENV_IS_IN_RAM) && !defined(CONFIG_SYS_NO_FLASH) &&	\
	defined(CONFIG_OCTEON_FLASH) && 				\
	!defined(CONFIG_ENV_IS_IN_NAND)
	octeon_fix_env_reloc();
#endif
	/* Map u-boot's virtual to physical mapping */
	addrmap_set_entry(CONFIG_SYS_TEXT_BASE, bd->bi_uboot_ram_addr,
			  bd->bi_uboot_map_size * 2, CONFIG_ADDR_MAP_CODE_IDX);

	/* Map the first 256MB of memory 1:1 */
	size = MIN(gd->ram_size, MB(256));
	addrmap_set_entry(0, 0, size, CONFIG_ADDR_MAP_FIRST_256MB_IDX);

	/* Map the boot bus 1:1 */
	addrmap_set_entry(0x10000000, 0x10000000, 0x10000000,
			  CONFIG_ADDR_MAP_BOOT_BUS_IDX);
	if (gd->ram_size > MB(256)) {
		/* The CN3XXX and CN5XXX place the second 256MB of memory
		 * at address 0x410000000 to 0x41FFFFFFF but this isn't really
		 * accessible to U-Boot so we don't worry about it.
		 */
		if (OCTEON_IS_OCTEON1PLUS()) {
			if (gd->ram_size > MB(512)) {
				size = min(0x60000000, gd->ram_size - MB(512));
				addrmap_set_entry(0x20000000, 0x20000000,
						  size,
						  CONFIG_ADDR_MAP_REST_MEM_IDX);
			}
		} else {
			/* CN63XX and newer */
			/* The CN6XXX and newer are different.  All memory above
			 * 256MB has it's address shifted by 256MB by the boot
			 * bus.
			 * U-Boot only cares about the other 1.5GB in KUSEG.
			 */
			size = gd->ram_size - MB(256);
			size = min(size, 0x60000000);
			addrmap_set_entry(0x20000000, 0x20000000, size,
					  CONFIG_ADDR_MAP_REST_MEM_IDX);
		}
	}

#if CONFIG_OCTEON_PCI_HOST
	/* Add BAR address mapping.  Note that this must match the code in
	 * octeon_pci.c
	 */
	addrmap_set_entry(0xe0000000, XKPHYS_TO_PHYS(pci_mem1_base + 0xe0000000),
			  0x10000000,
			  CONFIG_ADDR_MAP_PCI0_BASE);
	if (!OCTEON_IS_OCTEON1PLUS())
		addrmap_set_entry(0xf0000000,
				  XKPHYS_TO_PHYS(pci_mem2_base + 0xf0000000),
				  0x10000000, CONFIG_ADDR_MAP_PCI1_BASE);
#endif /* CONFIG_OCTEON_PCI_HOST */
#if !CONFIG_OCTEON_SIM_HW_DIFF
	/* Disable the watchdog timers, as these are still running after a soft
	 * reset as a PCI target
	 */
	int num_cores = cvmx_octeon_num_cores();
	for (i = 0; i < num_cores; i++)
		cvmx_write_csr(CVMX_CIU_WDOGX(i), 0);

	debug("About to flush L2 cache\n");
	/* Flush and unlock L2 cache (soft reset does not unlock locked lines)
	 */
	cvmx_l2c_flush();

#ifdef CONFIG_OCTEON_EMMC_STAGE2
	/* Unlock memory regions locked by the stage 1 bootloader */
	cvmx_l2c_unlock_mem_region(CONFIG_OCTEON_EMMC_STAGE1_ADDR,
				   CONFIG_OCTEON_EMMC_STAGE1_SIZE);
	cvmx_l2c_unlock_mem_region(CONFIG_OCTEON_EMMC_STAGE2_CACHE_ADDR,
				   CONFIG_OCTEON_EMMC_STAGE2_CACHE_SIZE);
	cvmx_l2c_unlock_mem_region(U_BOOT_RAM_ENV_ADDR, 2 * U_BOOT_RAM_ENV_SIZE);
#endif
#ifdef CONFIG_OCTEON_AUTHENTIK_STAGE2
	/* Unlock memory regions locked by the stage 1 bootloader */
	cvmx_l2c_unlock_mem_region(CONFIG_OCTEON_AUTHENTIK_STAGE1_ADDR,
				   CONFIG_OCTEON_AUTHENTIK_STAGE1_SIZE);
	cvmx_l2c_unlock_mem_region(CONFIG_OCTEON_AUTHENTIK_STAGE2_CACHE_ADDR,
				   CONFIG_OCTEON_AUTHENTIK_STAGE2_CACHE_SIZE);
#endif
#endif /* !CONFIG_OCTEON_SIM_HW_DIFF */

#if !CONFIG_OCTEON_SIM_SPEED && !defined(CONFIG_NO_CLEAR_DDR)
	if (!(gd->flags & GD_FLG_MEMORY_PRESERVED)) {
		/* Zero all of memory above DRAM_LATE_ZERO_OFFSET */
		debug("ram size: 0x%llx (%llu), sizeof=%u\n",
		      gd->ram_size, gd->ram_size, sizeof(gd->ram_size));
		printf("Clearing DRAM.....");
		octeon_clear_mem_pfs(DRAM_LATE_ZERO_OFFSET,
				     (u64)MIN(gd->ram_size,
					      MB(256) - DRAM_LATE_ZERO_OFFSET));
		puts(".");

		if (!OCTEON_IS_OCTEON1PLUS()) {
			if (gd->ram_size > MB(256))
				octeon_clear_mem_pfs(0x20000000ull,
						     (u64)(gd->ram_size - MB(256)));
		} else {
			if (gd->ram_size > MB(256)) {
				octeon_clear_mem_pfs(0x410000000ull,
						     (u64)MIN(gd->ram_size - MB(256),
							      MB(256)));
				puts(".");
			}

			if (gd->ram_size > MB(512)) {
				octeon_clear_mem_pfs(0x20000000ull,
						     gd->ram_size - MB(512));
				puts(".");
			}

		}
		/* Make sure all writes are complete before we continue */
		CVMX_SYNC;
		printf(" done\n");
	}
        else
        {
            printf("DRAM contents preserved, not clearing.\n");
        }
#endif

	/* Clear DDR/L2C ECC error bits */
	if (!OCTEON_IS_OCTEON1PLUS()) {
		int tad;
		cvmx_write_csr(CVMX_L2C_INT_REG,
			       cvmx_read_csr(CVMX_L2C_INT_REG));
		for (tad = 0; tad < CVMX_L2C_TADS; tad++) {
			cvmx_write_csr(CVMX_L2C_ERR_TDTX(tad),
				       cvmx_read_csr(CVMX_L2C_ERR_TDTX(tad)));
			cvmx_write_csr(CVMX_L2C_ERR_TTGX(tad),
				       cvmx_read_csr(CVMX_L2C_ERR_TTGX(tad)));
		}
	} else {
		cvmx_write_csr(CVMX_L2D_ERR, cvmx_read_csr(CVMX_L2D_ERR));
		cvmx_write_csr(CVMX_L2T_ERR, cvmx_read_csr(CVMX_L2T_ERR));
	}
	if (octeon_is_model(OCTEON_CN56XX)) {
		cvmx_l2c_cfg_t l2c_cfg;
		l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);
		if (l2c_cfg.cn56xx.dpres0) {
			cvmx_write_csr(CVMX_LMCX_MEM_CFG0(0),
				       cvmx_read_csr(CVMX_LMCX_MEM_CFG0(0)));
		}
		if (l2c_cfg.cn56xx.dpres1) {
			cvmx_write_csr(CVMX_LMCX_MEM_CFG0(1),
				       cvmx_read_csr(CVMX_LMCX_MEM_CFG0(1)));
		}
	} else if (octeon_is_model(OCTEON_CN63XX)
		   || octeon_is_model(OCTEON_CN66XX)
		   || octeon_is_model(OCTEON_CN61XX)
		   || octeon_is_model(OCTEON_CNF71XX)) {
		cvmx_write_csr(CVMX_LMCX_INT(0),
			       cvmx_read_csr(CVMX_LMCX_INT(0)));
	} else if (octeon_is_model(OCTEON_CN68XX)) {
		int lmc;
		for (lmc = 0; lmc < 4; lmc++)
		{
			cvmx_lmcx_dll_ctl2_t ctl;
			ctl.u64 = cvmx_read_csr(CVMX_LMCX_DLL_CTL2(lmc));
			if (ctl.s.intf_en)
			{
				cvmx_write_csr(CVMX_LMCX_INT(lmc),
					cvmx_read_csr(CVMX_LMCX_INT(lmc)));
			}
		}
	} else {
		cvmx_write_csr(CVMX_LMC_MEM_CFG0,
			       cvmx_read_csr(CVMX_LMC_MEM_CFG0));
	}

	debug("Initializing phy mem list: ram_size: 0x%llx, low reserved size: 0x%x\n",
	      gd->ram_size, OCTEON_RESERVED_LOW_MEM_SIZE);
	if (!cvmx_bootmem_phy_mem_list_init(gd->ram_size,
		    OCTEON_RESERVED_LOW_MEM_SIZE,
		    (void *)MAKE_KSEG0(BOOTLOADER_BOOTMEM_DESC_SPACE))) {
		printf("FATAL: Error initializing free memory list\n");
		hang();
	}
	debug("Bootloader boot memory descriptor at %p\n",
	      (void *)MAKE_KSEG0(BOOTLOADER_BOOTMEM_DESC_SPACE));
	/* Here we allocate the memory that u-boot is _already_ using.  Note
	 * that if the location of u-boot changes, this may need to be done
	 * differently.  In particular, this _won't_ work if u-boot is
	 * placed at the beginning of a DRAM region, as that is where the free
	 * block size/length is stored, and u-boot will be corrupted when
	 * the bootmem list is created.
	 */
	debug("Allocating named block for used memory.\n");
	int64_t __attribute__ ((unused)) bm_val;
	if (0 >
	    (bm_val =
	     cvmx_bootmem_phy_named_block_alloc(gd->bd->bi_uboot_ram_used_size,
						gd->bd->bi_uboot_ram_addr,
						gd->bd->bi_uboot_ram_addr +
						gd->bd->bi_uboot_ram_used_size,
						0, "__uboot_code_data", 0))) {
		printf("ERROR: Unable to allocate DRAM for u-boot from "
		       "bootmem allocator.\n");
		debug("ub used_size: 0x%lx, addr: 0x%llx, "
		      "bootmem_retval: 0x%llx\n",
		      gd->bd->bi_uboot_ram_used_size,
		      gd->bd->bi_uboot_ram_addr, bm_val);
	}
#ifdef CONFIG_OF_LIBFDT
	board_fdt_get_limits(&dtb_begin, &dtb_end);
	debug("Copying FDT from 0x%p to 0x%x, %u bytes\n",
	      dtb_begin, OCTEON_FDT_BASE, dtb_end - dtb_begin);
	bm_val = cvmx_bootmem_phy_named_block_alloc(OCTEON_FDT_MAX_SIZE,
						    OCTEON_FDT_BASE,
						    0,
						    0,
						    OCTEON_FDT_BLOCK_NAME,
						    0);
	if (bm_val < 0) {
		printf("Error: Could not reserve %u bytes for FDT at address 0x%x\n",
		       OCTEON_FDT_MAX_SIZE, OCTEON_FDT_BASE);
	}
	bm_val = cvmx_bootmem_phy_named_block_alloc(OCTEON_FDT_LE_MAX_SIZE,
						    OCTEON_FDT_LE_BASE,
						    0,
						    0,
						    OCTEON_FDT_LE_BLOCK_NAME,
						    0);
	if (bm_val < 0) {
		printf("Error: Could not reserve %u bytes for little-endian FDT "
		       "at address 0x%x\n",
		       OCTEON_FDT_LE_MAX_SIZE, OCTEON_FDT_LE_BASE);
	}
	/* Copy the initial FDT to its final location. */
	if (dtb_end - dtb_begin > OCTEON_FDT_MAX_SIZE) {
		printf("Embedded DTB image too large: %d\n",
		       dtb_end - dtb_begin);
	} else {
		memcpy((char *)OCTEON_FDT_BASE, dtb_begin,
		       dtb_end - dtb_begin);
		working_fdt = (struct fdt_header *)OCTEON_FDT_BASE;
		gd->fdt_blob = (void *)OCTEON_FDT_BASE;
		/* Reserve space to grow */
		debug("Increase FDT size to 0x%x\n", OCTEON_FDT_MAX_SIZE);
		rc = fdt_open_into(working_fdt, working_fdt,
				   OCTEON_FDT_MAX_SIZE);
		if (rc) {
			printf("Error: Cannot increase size of FDT: %s\n",
			       fdt_strerror(rc));
		}
		debug("Performing board FDT fixups\n");
		board_fixup_fdt();
	}
#endif
	debug("BOOTLOADER_BOOTMEM_DESC_ADDR: 0x%x\n",
	      BOOTLOADER_BOOTMEM_DESC_ADDR);
	debug("BOOTLOADER_BOOTMEM_DESC_SPACE: 0x%x\n",
	      BOOTLOADER_BOOTMEM_DESC_SPACE);
	debug("size of bootmem desc: %d, location: %p\n",
	      sizeof(cvmx_bootmem_desc_t),
	      (void *)MAKE_KSEG0(BOOTLOADER_BOOTMEM_DESC_SPACE));

	debug("Setting up simple executive support\n");
	/* setup cvmx_sysinfo structure so we can use simple executive
	 * functions.
	 */
	octeon_setup_cvmx_sysinfo();

	/* Take all cores out of reset to reduce power usage. The secondary
	 * cores will spin on a wait loop conserving power until a NMI is
	 * received.
	 */
	debug("Taking all cores out of reset.\n");
	cvmx_write_csr(CVMX_CIU_PP_RST, 0);

#ifndef CONFIG_ENV_IS_NOWHERE
	debug("Relocating the environment function pointers.\n");
# if !defined(CONFIG_ENV_IS_IN_RAM) && !defined(CONFIG_ENV_IS_IN_NAND)
	if (gd->flags & GD_FLG_RAM_RESIDENT) {
		/* Restore to FLASH location */
		env_t *ep = (env_t *)CONFIG_ENV_ADDR;
		gd->env_addr = CONFIG_ENV_ADDR;
		debug("Setting gd environment address to 0x%lx\n", gd->env_addr);
		if (ep->crc == crc32(0, ep->data, CONFIG_ENV_SIZE - 4)) {
			debug("Found good CRC for environment.\n");
			gd->env_valid = 1;
		} else
			debug("CRC mismatch for environment at 0x%p\n", ep);
	}
# endif
#endif
#if (defined(CONFIG_CMD_OCTEON_NAND) || defined(CONFIG_CMD_NAND)) \
	&& !CONFIG_OCTEON_SIM_NO_FLASH
	/* NAND must be done after bootmem allocator is set up, and before
	 * NOR  probing to handle some corner cases.
	 */
	int oct_nand_probe(void);
	if (!getenv("disable_nand")) {
		debug("Probing NAND memory\n");
		WATCHDOG_RESET();
		oct_nand_probe();
		WATCHDOG_RESET();
	}

	/* Initialize NAND flash */
	if (!getenv("disable_nand")) {
		puts("NAND:  ");
		nand_init();	/* go init the NAND (do this after flash init...) */
		WATCHDOG_RESET();
	}
#endif

	/* Relocate environment function pointers */
	env_relocate();

#ifndef CONFIG_OCTEON_SIM
	if (gd->flags & GD_FLG_RAM_RESIDENT) {
		/* If we're loaded by an external boot loader then we may be
		 * passed environment variables.  In this case we first load
		 * the environment, if any, from flash and the default
		 * environment, then overwrite any entries passed from the
		 * external bootloader.
		 *
		 * The difference between this and a standard import is that
		 * import clears out the environment variables from the flash
		 * first, which we do not want.
		 */
		env_t *ep = (env_t *)U_BOOT_RAM_ENV_ADDR;
		uint32_t crc;
		debug("Importing environment from remote boot loader.\n");

		memcpy(&crc, &ep->crc, sizeof(crc));

		if ((crc == crc32(0, ep->data,
				 U_BOOT_RAM_ENV_SIZE - U_BOOT_RAM_ENV_CRC_SIZE))
		    && (!himport_r(&env_htab, (char *)ep->data,
				   U_BOOT_RAM_ENV_SIZE - U_BOOT_RAM_ENV_CRC_SIZE,
				   '\0', H_NOCLEAR))) {
				printf("Error: could not import environment "
				       "from boot loader\n");
		}
# if !CONFIG_NO_CLEAR_DDR
		else {
			/* We can safely clear the passed-in environment
			 * variables now.
			 */
			memset((char *)U_BOOT_RAM_ENV_ADDR, 0,
			       U_BOOT_RAM_ENV_SIZE);
			memset((char *)U_BOOT_RAM_ENV_ADDR_2, 0,
			       U_BOOT_RAM_ENV_SIZE);
		}
# endif
	}
#endif
	/* For the Octeon simulator we need to be able to import the environment
	 */
#if defined(CONFIG_OCTEON_SIM) && !defined(CONFIG_OCTEON_NO_BOOT_BUS)
#define OCTEON_ENV_FLASH_ADDR (0x9FE00000UL)
#define OCTEON_ENV_FLASH_SIZE (0x10000UL)
	debug("Looking for environment at %p\n",
	      (char *)(OCTEON_ENV_FLASH_ADDR));
	if (*(char *)(OCTEON_ENV_FLASH_ADDR) != '\0') {
		printf("Copying user supplied environment from file flash\n");
		if (!env_import((char *)(OCTEON_ENV_FLASH_ADDR - ENV_HEADER_SIZE), 0)) {
			printf("Error %d importing user supplied environment\n", errno);
		}
	} else {
		debug("No user supplied environment found\n");
	}
#endif

	/* Create a named block for idle cores to loop */
	idle_loop = cvmx_bootmem_phy_named_block_alloc(IDLE_CORE_BLOCK_SIZE,
						       OCTEON_IDLE_CORE_MEM,
						       0, 0,
						       IDLE_CORE_BLOCK_NAME, 0);
	if (0 > idle_loop)
		printf("ERROR: Unable to allocate DRAM for idle loop from "
		       "bootmem allocator.\n");

#if !CONFIG_OCTEON_SIM_MEM_LAYOUT || defined(CONFIG_NET_MULTI) || \
	defined(OCTEON_RGMII_ENET) || defined(OCTEON_MGMT_ENET) || \
	defined(OCTEON_SGMII_ENET) || defined(OCTEON_XAUI_ENET)
	{
		char *eptr;
		u32 addr;
		u32 size;
		eptr = getenv("octeon_reserved_mem_load_size");

		if (!eptr || !strcmp("auto", eptr)) {
			/* Pick a size that we think is appropriate.
			 * Please note that for small memory boards this guess
			 * will likely not be ideal.
			 * Please pick a specific size for boards/applications
			 * that require it.
			 */
			if (OCTEON_IS_OCTEON1PLUS()) {
				if (gd->ram_size <= MB(256))
					size = ((gd->ram_size * 2) / 5) & ~0xFFFFF;
				else if (gd->ram_size <= MB(512))
					size = min(MB(128),
						   (((gd->ram_size - MB(256)) * 2) / 5) & ~0xFFFFF);
			else
					size = min(MB(256),
						   (((gd->ram_size - MB(512)) * 2) / 3) & ~0xFFFFF);
			} else {
				if (gd->ram_size <= MB(256))
					size = min(MB(128),
						   ((gd->ram_size * 2) / 5) & ~0xFFFFF);
			else
					size = min(MB(256),
						   ((gd->ram_size - MB(256)) / 3) & ~0xFFFFF);
			}
		} else {
			size = simple_strtol(eptr, NULL, 16);
			debug("octeon_reserved_mem_load_size=0x%08x\n", size);
		}
		if (size) {
			debug("Linux reserved load size 0x%08x\n", size);
			eptr = getenv("octeon_reserved_mem_load_base");
			if (!eptr || !strcmp("auto", eptr)) {
				u64 mem_top;
				/* Leave some room for previous allocations that
				 * are made starting at the top of the low
				 * 256 Mbytes of DRAM
				 */
				int adjust = MB(1);
				if (gd->ram_size <= MB(512))
					adjust = MB(16);
				/* Put block at the top of DDR0, or bottom of
				 * DDR2
				 */
				if ((gd->ram_size <= MB(256)) ||
				    (size > (gd->ram_size - MB(256))))
					mem_top = MIN(gd->ram_size - adjust,
					      MB(256) - adjust);
				else if ((gd->ram_size <= MB(512)) ||
					 (size > (gd->ram_size - MB(512))))
					mem_top = MIN(gd->ram_size - adjust,
						      MB(512) - adjust);
				else
					/* We have enough room, so set
					 * mem_top so that the block is
					 * at the base of the DDR2
					 * segment
					 */
					mem_top = MB(512) + size;

				/* Adjust for boot bus memory hole on OCTEON II
				 * and later.
				 */
				if (!OCTEON_IS_OCTEON1PLUS() &&
				    (gd->ram_size > MB(256)))
					mem_top += MB(256);
				debug("Adjusted memory top is 0x%llx\n", mem_top);
				addr = mem_top - size;
				if (addr > MB(512))
					addr = MB(512);
				if ((addr >= MB(256)) && addr < MB(512)) {
					/* The address landed in the boot-bus
					 * memory hole.  Dig it out of the hole.
					 */
					if (OCTEON_IS_OCTEON1PLUS())
						addr = MB(256) - size - MB(16);
					else
						addr = MB(512);
				}
			} else {
				addr = simple_strtol(eptr, NULL, 16);
			}

			if (0 > cvmx_bootmem_phy_named_block_alloc(size, addr,
								   addr + size,
								   0,
								   OCTEON_BOOTLOADER_LOAD_MEM_NAME,
								   0)) {
				printf("ERROR: Unable to allocate bootloader "
				       "reserved memory	(addr: 0x%x, "
				       "size: 0x%x).\n", addr, size);
			} else {
				/* Set default load address to base of memory
				 * reserved for loading. The setting of the
				 * env. variable also sets the load_addr global
				 * variable.
				 * This environment variable is overridden each
				 * boot if a reserved block is created.
				 */
				char str[20];
				snprintf(str, sizeof(str), "0x%x", addr);
				setenv("loadaddr", str);
				load_reserved_addr = addr;
				load_reserved_size = size;
				debug("Setting load address to 0x%08x, size 0x%x\n",
				      addr, size);
			}
		} else
			printf("WARNING: No reserved memory for image "
			       "loading.\n");
	}
#endif

#if !CONFIG_OCTEON_SIM_MEM_LAYOUT
	/* Reserve memory for Linux kernel.  The kernel requires specific
	 * physical addresses, so by reserving this here we can load simple
	 * exec applications and do other memory allocation without
	 * interfering with loading a kernel image.  This is freed and used
	 * when loading a Linux image.  If a Linux image is not loaded, then
	 * this is freed just before the applications start.
	 */
	{
		char *eptr;
		u32 addr;
		u32 size;
		eptr = getenv("octeon_reserved_mem_linux_size");
		if (!eptr || !strcmp("auto", eptr)) {
			if (OCTEON_IS_OCTEON1PLUS() &&
			    (gd->ram_size >= MB(512)))
				size = MIN((gd->ram_size - MB(256)) / 2, MB(192));
			else if (gd->ram_size >= MB(1024))
				size = MIN(gd->ram_size / 2, MB(240));
			else
				size = MIN(gd->ram_size / 2, MB(192));
		} else
			size = simple_strtol(eptr, NULL, 16);
		if (size) {
			eptr = getenv("octeon_reserved_mem_linux_base");
			if (!eptr || !strcmp("auto", eptr))
				/* Start right after the bootloader */
				addr = OCTEON_LINUX_RESERVED_MEM;
			else
				addr = simple_strtol(eptr, NULL, 16);
			debug("Reserving 0x%x bytes of memory at 0x%x for the Linux kernel\n",
			      size, addr);
			/* Allocate the memory, and print message if we fail */
			if (0 > cvmx_bootmem_phy_named_block_alloc(size, addr,
								   addr + size,
								   0,
								   OCTEON_LINUX_RESERVED_MEM_NAME,
								   0)) {
				printf("ERROR: Unable to allocate linux "
				       "reserved memory (addr: 0x%x, size: 0x%x).\n",
				       addr, size);
			}
		}
	}
#endif
	/* Initialize the named block with the following code so that cores will
	 * spin on a    wait loop conserving power until a NMI is received.
	 *
	 * 42000020  1: wait
	 * 1000fffe     b 1b
	 * 00000000     nop
	 */
	memset(cvmx_phys_to_ptr(idle_loop), 0, IDLE_CORE_BLOCK_SIZE);
	*(int64_t *) cvmx_phys_to_ptr(idle_loop) = 0x420000201000fffeull;
	CVMX_SYNCW;
	/* Put bootmem descriptor address in known location for host */
	{
		/* Make sure it is not in kseg0, as we want physical addresss */
		u64 *u64_ptr = (void *)(0x80000000 |
					BOOTLOADER_BOOTMEM_DESC_ADDR);
		*u64_ptr = ((u32) __cvmx_bootmem_internal_get_desc_ptr()) &
		    0x7fffffffull;
		debug("bootmem descriptor address (looked up): 0x%llx\n",
		      ((u32) __cvmx_bootmem_internal_get_desc_ptr())
		      & 0x7fffffffull);
	}

	/* Set up prompt */
	debug("Setting up U-Boot prompt for board type %u.\n",
	      cvmx_sysinfo_get()->board_type);

	/* Generate the u-boot prompt based on various conditions */
	if (!getenv("prompt")) {
		char prompt[CONFIG_SYS_MAX_PROMPT_LEN];
		debug("prompt environment variable not set.\n");
#ifdef CONFIG_SYS_PROMPT
		debug("Setting prompt from defined %s\n", CONFIG_SYS_PROMPT);
		strncpy(prompt, CONFIG_SYS_PROMPT,
			CONFIG_SYS_MAX_PROMPT_LEN - 12);
#else
		char board_name[CONFIG_SYS_MAX_PROMPT_LEN];
		strncpy(board_name,
			cvmx_board_type_to_string(cvmx_sysinfo_get()->board_type),
			CONFIG_SYS_MAX_PROMPT_LEN - 20);
		octeon_adjust_board_name(board_name, sizeof(board_name)-1);
		lowcase(board_name);
		snprintf(prompt, sizeof(prompt), "Octeon %s", board_name);
#endif
		if (gd->flags & GD_FLG_FAILSAFE_MODE)
			snprintf(uboot_prompt, sizeof(uboot_prompt),
				 "%s(Failsafe)", prompt);
		else if (gd->flags & GD_FLG_RAM_RESIDENT)
			snprintf(uboot_prompt, sizeof(uboot_prompt),
				 "%s(ram)", prompt);
		else
			snprintf(uboot_prompt, sizeof(uboot_prompt),
				 "%s", prompt);
		strncpy(prompt, uboot_prompt, sizeof(prompt));
#ifdef CONFIG_SYS_HUSH_PARSER
		snprintf(uboot_prompt, sizeof(uboot_prompt), "%s=>", prompt);
		snprintf(uboot_prompt_hush_ps2, sizeof(uboot_prompt_hush_ps2),
			 "%s>", prompt);
#else
		snprintf(uboot_prompt, sizeof(uboot_prompt), "%s# ", prompt);
#endif

		debug("Setting prompt to %s\n", uboot_prompt);
	} else
		debug("prompt environment set, using that.\n");

#if defined(CONFIG_HW_WATCHDOG)
	extern void hw_watchdog_start(int msecs);
	if (getenv("watchdog_enable")) {
		char *e;
		int msecs = 0;
		if ((e = getenv("watchdog_timeout")) != NULL) {
			msecs = simple_strtoul(e, NULL, 0);
		}
		if (!msecs)
			msecs = CONFIG_OCTEON_WD_TIMEOUT * 1000;
		debug("Starting hardware watchdog with NMI timeout %d ms\n",
		      msecs);
		hw_watchdog_start(msecs);
	}
#endif
	/* Initialize the rest of the devices on the boot bus like
	 * Compact Flash, PSRAM, etc.
	 */
#ifndef CONFIG_OCTEON_NO_BOOT_BUS
	debug("Performing late boot bus initialization.\n");
	octeon_boot_bus_late_init();
#endif
#if CONFIG_OCTEON_SIM_NO_FLASH
	{
		/* Configure boot bus region 1 to allow entire 256 MByte region
		 * to be accessed during simulation.
		 */
		uint64_t flash_size = MB(256);
		cvmx_mio_boot_reg_cfgx_t reg_cfg;
		printf("Configuring boot bus for full 256MiB access\n");
		reg_cfg.u64 = 0;
		cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(0), reg_cfg.u64);
		reg_cfg.s.en = 1;
		/* In 64k blocks, +4MByte alias of low 4MBytes of flash */
		reg_cfg.s.size = (flash_size >> 16) - 1;
		/* Mask to put physical address in boot bus window */
		reg_cfg.s.base = ((0x10000000 >> 16) & 0x1fff);
		cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(1), reg_cfg.u64);
	}
#endif
#if !defined(CONFIG_SYS_NO_FLASH) && !CONFIG_OCTEON_SIM_NO_FLASH
	/* Do flash init _after_ NAND probing, as we may have done some chip
	 * select enable fixups as part of the NAND probing.
	 */
	debug("Checking for NOR flash...\n");
	WATCHDOG_RESET();
	if (octeon_check_nor_flash_enabled(gd->ogd.flash_base_address)) {
		int size;
		debug("%s: NOR flash detected at 0x%x\n",
		      __func__, gd->ogd.flash_base_address);
		size = flash_init();
		display_flash_config(size);
		if (bd->bi_bootflags & OCTEON_BD_FLAG_BOOT_CACHE) {
			gd->ogd.flash_base_address = 0x1fc00000 - size;
			gd->ogd.flash_size = size;
		}
		bd->bi_flashstart = gd->ogd.flash_base_address;
		bd->bi_flashsize = size;
#if CONFIG_SYS_MONITOR_BASE == CONFIG_SYS_FLASH_BASE
		/* reserved area for U-Boot */
		bd->bi_flashoffset = monitor_flash_len;
#else
		bd->bi_flashoffset = 0;
#endif
	} else {
		printf("Flash boot bus region not enabled, skipping NOR flash "
		       "config\n");
		bd->bi_flashstart = 0;
		bd->bi_flashsize = 0;
	}
	WATCHDOG_RESET();
#endif /* CONFIG_SYS_NO_FLASH */

#ifdef DEBUG
	cvmx_bootmem_phy_list_print();
#endif

#if (CONFIG_OCTEON_EEPROM_TUPLES)
	{
		WATCHDOG_RESET();
		debug("Reading eeprom...\n");
		/* Read coremask from eeprom, and set environment variable if
		 * present.  Print warning if not present in EEPROM.
		 * Currently ignores voltage/freq fields, and just uses first
		 * capability tuple.
		 * This is only valid for certain parts.
		 */
		int addr;
		uint8_t ee_buf[OCTEON_EEPROM_MAX_TUPLE_LENGTH];
		octeon_eeprom_chip_capability_t *cc_ptr = (void *)ee_buf;
		addr = octeon_tlv_get_tuple_addr(CONFIG_SYS_DEF_EEPROM_ADDR,
						 EEPROM_CHIP_CAPABILITY_TYPE,
						 0, ee_buf,
						 OCTEON_EEPROM_MAX_TUPLE_LENGTH);
		if (addr >= 0 &&
		    ((octeon_is_model(OCTEON_CN38XX)
		      && ((cvmx_read_csr(CVMX_CIU_FUSE) & 0xffff) == 0xffff)
		      && !cvmx_octeon_fuse_locked())
		     || octeon_is_model(OCTEON_CN56XX)
		     || octeon_is_model(OCTEON_CN52XX)
		     || OCTEON_IS_OCTEON2())) {
			char tmp[10];
			snprintf(tmp, sizeof(tmp), "0x%04x", cc_ptr->coremask);
			setenv("coremask_override", tmp);
			/* Save copy for later verification */
			coremask_from_eeprom = cc_ptr->coremask;
		} else {
			/* No chip capability tuple found, so we need to check
			 * if we expect one.
			 * CN31XX chips will all have fuses blown
			 * appropriately.
			 * CN38XX chips may have fuses blown, but may not.  We
			 * will check to see if
			 * we need a chip capability tuple and only warn if we
			 * need it but don't have it.
			 */
			if (octeon_is_model(OCTEON_CN38XX)) {
				/* Here we only need it if no core fuses are
				 * blown and the lockdown fuse is not blown.
				 * In all other cases the cores fuses are
				 * definitive and we don't need a coremask
				 * override.
				 */
				if (((cvmx_read_csr(CVMX_CIU_FUSE) & 0xffff) ==
				     0xffff)
				    && !cvmx_octeon_fuse_locked())
					printf("Warning: No chip capability "
					       "tuple found in eeprom, "
					       "coremask_override may be set "
					       "incorrectly\n");
				else
					/* Clear coremask_override as we have a
					 * properly fused part, and don't need
					 * it.
					 */
					setenv("coremask_override", NULL);
			} else {
				/* 31xx and 30xx will always have core fuses
				 * blown appropriately
				 */
				setenv("coremask_override", NULL);
			}
		}
	}
#endif
#ifdef CONFIG_OF_LIBFDT
	debug("Performing OCTEON FDT fixups\n");
	octeon_fixup_fdt();
	debug("Packing flat device tree\n");
	fdt_pack(working_fdt);
#endif
	/* Set numcores env variable to indicate the number of cores available
	 */
	char tmp[10];
	snprintf(tmp, sizeof(tmp), "%d", octeon_get_available_core_count());
	debug("Setting numcores to %s\n", tmp);
	setenv("numcores", tmp);

	/* board MAC address */
	s = getenv("ethaddr");
	if (s) {
		debug("Setting ethernet address to %s...\n", s);
		for (i = 0; i < 6; ++i) {
			bd->bi_enetaddr[i] = s ? simple_strtoul(s, &e, 16) : 0;
			if (s)
				s = (*e) ? e + 1 : e;
		}
	} else
		debug("ethaddr not found in environment\n");

	WATCHDOG_RESET();
	debug("Setting IP address...\n");
	/* IP Address */
	bd->bi_ip_addr = getenv_IPaddr("ipaddr");
	debug("Done.\n");

	debug("Reserving first 1MB of memory\n");
	rc = cvmx_bootmem_reserve_memory(0, OCTEON_RESERVED_LOW_BOOT_MEM_SIZE,
					 "__low_reserved", 0);
	if (!rc) {
		printf("Error reserving low 1MB of memory\n");
	}

        /* Initialize QLM */
        cvmx_qlm_init();

#if defined(CONFIG_PCI)
	/*
	 * Do pci configuration
	 */
	if (getenv("disable_pci"))
		printf("Skipping PCI due to 'disable_pci' environment "
		       "variable\n");
	else {
		debug("Initializing PCI\n");
		pci_init();
	}
#endif

	/** leave this here (after malloc(), environment and PCI are working)
	 **/
	WATCHDOG_RESET();
	debug("Initializing stdio...\n");
	/* Initialize stdio devices */
	stdio_init();

#ifdef CONFIG_SYS_PCI_CONSOLE
	/* Check to see if we need to set up a PCI console block. Set up
	 * structure if either count is set (and greater than 0) or
	 * pci_console_active is set
	 */
extern int uboot_octeon_pci_console_init(void);

	debug("Setting up PCI console\n");
	if (uboot_octeon_pci_console_init())
		puts("WARNING: Could not initialize PCI console support!\n");
	debug("PCI read buffer base at 0x%x\n",
	      MAKE_KSEG0(BOOTLOADER_PCI_READ_BUFFER_BASE));
#endif

	/* leave this here (after malloc(), environment and PCI are working) */
	debug("Initializing jump table...\n");
	jumptable_init();

	debug("Initializing console...\n");
	/* Initialize the console (after the relocation and devices init) */
	/* Must be done after environment variables are set */

	/* stdin, stdout and stderr must be set to serial before calling
	 * console_init_r()
	 */
	setenv("stdin", "serial");
	setenv("stdout", "serial");
	setenv("stderr", "serial");
	/* We can't fully initialize stdin, stdout and stderr at this time
	 * since console_init_r() will switch it back to serial.
	 */
	console_init_r();

	/* Must change std* variables after devices_init()
	 * Force bootup settings based on pci_console_active, overriding the
	 * std* variables
	 */
#ifdef CONFIG_SYS_PCI_CONSOLE
# ifdef CONFIG_OCTEON_BOOTCMD
	debug("Setting stdin, stdout and stderr with serial, pci and "
	      "bootcmd input...\n");
# else
	debug("Setting stdin, stdout and stderr with serial and pci input...\n");
# endif
	if (getenv("pci_console_active")) {
		if (getenv("serial_console_active")) {
			printf("Using PCI console, serial port also enabled.\n");
#ifdef CONFIG_OCTEON_BOOTCMD
			setenv("stdin", "serial,pci,bootcmd");
#else
			setenv("stdin", "serial,pci");
#endif
			setenv("stdout", "serial,pci");
			setenv("stderr", "serial,pci");
		} else {
			printf("Using PCI console, serial port disabled.\n");
# ifdef CONFIG_OCTEON_BOOTCMD
			setenv("stdin", "pci,bootcmd");
# else
			setenv("stdin", "pci");
# endif
			setenv("stdout", "pci");
			setenv("stderr", "pci");
		}
		octeon_led_str_write("PCI CONS");
	} else {
# ifdef CONFIG_OCTEON_BOOTCMD
		setenv("stdin", "serial,pci,bootcmd");
# else
		setenv("stdin", "serial,pci");
# endif
		setenv("stdout", "serial");
		setenv("stderr", "serial");
	}
#else
	debug("Setting stdin, stdout and stderr to serial port\n");
	setenv("stdin", "serial");
	setenv("stdout", "serial");
	setenv("stderr", "serial");
#endif
	/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/
	WATCHDOG_RESET();

	/* We want to print out the board/chip info again, as this is very
	 * useful for automated testing. and board identification.
	 */
	if (getenv("pci_console_active"))
		display_board_info();

	/* Initialize from environment. Note that in most cases the environment
	 * variable will have been set from the reserved memory block allocated
	 * above.
	 * By also checking this here, this allows the load address to be set in
	 * cases where there is no reserved block configured.
	 * If loadaddr is not set, set it to something.  This default should
	 * never really be used,
	 * but we need to handle this case.
	 */
	debug("Setting load address...\n");
	if ((s = getenv("loadaddr")) != NULL) {
		load_addr = simple_strtoul(s, NULL, 16);
	} else {
		u64 addr = 0x100000;
		char tmp[20];
#ifdef CONFIG_SYS_LOAD_ADDR
		addr = CONFIG_SYS_LOAD_ADDR;
#endif
		snprintf(tmp, sizeof(tmp), "0x%llx", addr);
#if !CONFIG_OCTEON_SIM_MEM_LAYOUT
		printf("WARNING: loadaddr not set, defaulting to %s.  Please "
		       "either define a load reserved block or set the "
		       "loadaddr environment variable.\n", tmp);
#endif
		setenv("loadaddr", tmp);
	}

#if !CONFIG_OCTEON_SIM_NO_FLASH && !defined(CONFIG_SYS_NO_FLASH) && \
    !defined(CONFIG_OCTEON_NAND_STAGE2) && \
    !defined(CONFIG_OCTEON_EMMC_STAGE2) && \
    !defined(CONFIG_OCTEON_AUTHENTIK_STAGE2)
	{
		u32 octeon_find_and_validate_normal_bootloader(int rescan_flag);
		u32 get_image_size(const bootloader_header_t * header);

		char tmp[30];
		extern flash_info_t flash_info[];
		flash_info_t *info;
		int sector;
		unsigned long flash_unused_address, flash_unused_size;
		unsigned long uboot_flash_address, uboot_flash_size;
		/* Set up a variety of environment variables for use by u-boot
		 * and applications (through environment named block).
		 */

		/* Set env_addr and env_size variables to the address and size
		 * of the u-boot environment.
		 */
		snprintf(tmp, sizeof(tmp), "0x%x", CONFIG_ENV_ADDR);
		setenv("env_addr", tmp);
		snprintf(tmp, sizeof(tmp), "0x%x", CONFIG_ENV_SIZE);
		setenv("env_size", tmp);

		/* Describe physical flash */
		snprintf(tmp, sizeof(tmp), "0x%x", CONFIG_SYS_FLASH_BASE);
		setenv("flash_base_addr", tmp);
		snprintf(tmp, sizeof(tmp), "0x%x", CONFIG_SYS_FLASH_SIZE);
		setenv("flash_size", tmp);

		/* Here we want to set some environment variables that describe
		 * how the flash is currently used, and protect the
		 * region that has u-boot code.
		 */
		if (gd->flags & GD_FLG_FAILSAFE_MODE) {
			uboot_flash_size = gd->ogd.uboot_flash_size;
			uboot_flash_address = gd->ogd.uboot_flash_address;
		} else {
			uboot_flash_address =
			    octeon_find_and_validate_normal_bootloader(1);
			uboot_flash_size =
			    get_image_size((bootloader_header_t *)
					   (uboot_flash_address));
		}
		flash_unused_address = uboot_flash_address + uboot_flash_size;

		/* Align this to the correct sector */
		info = &flash_info[0];
		for (sector = 0; sector < info->sector_count; ++sector)
			if (flash_unused_address <= info->start[sector])
				break;

		if (sector == info->sector_count)
			printf("ERROR: No unused memory available in flash\n");

		flash_unused_address = info->start[sector];

		flash_unused_size = CONFIG_SYS_FLASH_SIZE -
		    (flash_unused_address - CONFIG_SYS_FLASH_BASE);
		/* Set addr size for flash space that is available for
		 * application use
		 */
		snprintf(tmp, sizeof(tmp), "0x%lx", uboot_flash_address);
		setenv("uboot_flash_addr", tmp);
		snprintf(tmp, sizeof(tmp), "0x%lx",
			flash_unused_address - uboot_flash_address);
		setenv("uboot_flash_size", tmp);

		snprintf(tmp, sizeof(tmp), "0x%lx", flash_unused_address);
		setenv("flash_unused_addr", tmp);
		snprintf(tmp, sizeof(tmp), "0x%lx", flash_unused_size);
		setenv("flash_unused_size", tmp);

		/* We now want to protect the flash from the base to the unused
		 * address
		 */
		flash_protect(FLAG_PROTECT_SET,
			      CONFIG_SYS_FLASH_BASE,
			      flash_unused_address - 1, info);

		/* Set obsolete bootloader_flash_update to do the new
		 * bootloaderupdate command.
		 */
		setenv("bootloader_flash_update", "bootloaderupdate");
	}
#endif

#ifdef CONFIG_CMD_NET
	debug("Getting bootfile...\n");
	if ((s = getenv("bootfile")) != NULL)
		copy_filename(BootFile, s, sizeof(BootFile));
#endif /* CFG_COMMAND_NET */
	{
		char boardname[256];
		char *s = boardname;
		strcpy(boardname,
		       cvmx_board_type_to_string(cvmx_sysinfo_get()->board_type));
		while (*s) {
			*s = tolower(*s);
			s++;
		}

		setenv("boardname", boardname);
	}

	WATCHDOG_RESET();
#if defined(CONFIG_CMD_NET) && defined(CONFIG_NET_MULTI) \
	&& !defined(DISABLE_NETWORKING)
	puts("Net:   ");
	board_net_preinit();
	eth_initialize(gd->bd);
	debug("Net configured.\n");
	debug("Initializing board MDIO interfaces/PHYs\n");
	board_mdio_init();
	board_net_postinit();
	WATCHDOG_RESET();
#endif

#ifdef CONFIG_CMD_SPI
	puts("SPI:   ");
	spi_init();		/* go init the SPI */
	puts("ready\n");
#endif

#if defined(CONFIG_MISC_INIT_R)
	/* miscellaneous platform dependent initialisations */
	debug("Configuring misc...\n");
	misc_init_r();
	WATCHDOG_RESET();
#endif

#ifdef  CONFIG_CMD_IDE
	debug("Configuring IDE\n");
#if !defined(CONFIG_PCI) && 0
	if (octeon_cf_present())
#endif
	{
#if 0
		octeon_board_ide_init();
		WATCHDOG_RESET();
#endif
#ifdef CONFIG_CMD_IDE

		/* Always do ide_init if PCI enabled to allow for PCI IDE
		 * devices
		 */
		ide_init();
		WATCHDOG_RESET();
#endif
	}
#endif
#ifdef CONFIG_PCI
# if defined(CONFIG_CMD_SATA)
	sata_initialize();
# endif
#endif
#ifdef CONFIG_CMD_DTT
	debug("Initializing temperature sensor...\n");
	extern int dtt_init(void);
	dtt_init();
	WATCHDOG_RESET();
#endif

#ifdef CONFIG_OCTEON_MMC
	extern int mmc_initialize(bd_t *bis);
	if (!getenv("disable_mmc")) {
		puts("MMC:   ");
		mmc_initialize(gd->bd);
	}
#endif
#ifdef CONFIG_CMD_USB
	/* Scan for USB devices automatically on boot.
	 * The host port used can be selected with the 'usb_host_port' env
	 * variable, which can also disable the scanning completely.
	 */
	if (!getenv("disable_usb_scan")) {
		usb_init();
	}
# ifdef CONFIG_USB_EHCI_OCTEON2
	else if (getenv("enable_usb_ehci_clock")) {
		extern int ehci_hcd_init(void);
		ehci_hcd_init();
	}
# endif
	WATCHDOG_RESET();
# ifdef CONFIG_USB_STORAGE
	puts("Type the command \'usb start\' to scan for USB storage devices.\n\n");
# endif
#endif

	/* verify that boot_init_vector type is the correct size */
	if (BOOT_VECTOR_NUM_WORDS * 4 != sizeof(boot_init_vector_t)) {
		printf("ERROR: boot_init_vector_t size mismatch: "
		       "define: %d, type: %d\n", BOOT_VECTOR_NUM_WORDS * 4,
		       sizeof(boot_init_vector_t));
		while (1) ;
	}
	debug("Doing late board init...\n");
	late_board_init();
	WATCHDOG_RESET();

	/* Set the serial # */
	setenv("serial#", (char *)gd->ogd.board_desc.serial_str);

	/* Set a few environment flags based on the U-Boot mode */
	setenv("octeon_failsafe_mode",
	       (gd->flags & GD_FLG_FAILSAFE_MODE) ? "1" : "0");
	setenv("octeon_ram_mode",
	       (gd->flags & GD_FLG_RAM_RESIDENT) ? "1" : "0");

#ifdef CONFIG_OCTEON_BOOTCMD
	debug("Unlocking PCI boot command console\n");
	/* Unlock the bootcmd console */
	int octeon_pci_bootcmd_unlock(void);
	octeon_pci_bootcmd_unlock();
#endif

	/* Check if NULL address is still zero */
	debug("Checking for NULL pointer writes\n");
	if (*(uint64_t *)MAKE_KSEG0(0) != 0ull) {
		printf("Error: detected write to NULL pointer.\n");
	}
	debug("End of NULL pointer check.\n");
	debug("Entering main loop.\n");
	/* main_loop() can return to retry autoboot, if so just run it again. */
	for (;;) {
		main_loop();
	}
	/* NOTREACHED - no way out of command loop except booting */
}

void hang(void)
{
	puts("### ERROR ### Please RESET the board ###\n");
	for (;;) ;
}
