/***********************license start************************************
 * Copyright (c) 2003-2012 Cavium (support@cavium.com). All rights
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
 * For any questions regarding licensing please contact support@cavium.com
 *
 ***********************license end**************************************/

/**
 * @file
 *
 * $Id: cmd_octeon.c 79682 2013-01-04 21:22:55Z cchavva $
 *
 */

#if !defined (__U_BOOT__)
#include "bootoct.h"
#else
#include <common.h>
#include <command.h>
#include <exports.h>
#include <linux/ctype.h>
#include <net.h>
#include <elf.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>
#include <asm/arch/cvmx-bootmem.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon-boot-info.h>
#include <asm/arch/lib_octeon.h>

extern char uboot_start;
extern char tlbwi_and_eret;

extern boot_info_block_t boot_info_block_array[];

DECLARE_GLOBAL_DATA_PTR;
#endif

/*#define DEBUG*/

#ifdef DEBUG
#define dprintf printf
#else
#define dprintf(...)
#endif

int forceboot = 0;
uint32_t boot_flags = 0;
#if defined(__U_BOOT__)
static int load_elf(uint32_t elf_addr, int argc, char *const argv[],
		    uint32_t stack_size, uint32_t heap_size, uint32_t core_mask,
		    int app_index);
#else
int load_elf(void *elf_addr, int argc, char *const argv[],
	     uint32_t stack_size, uint32_t heap_size, uint32_t core_mask,
	     int app_index);
#endif

#if defined(__U_BOOT__)

void setup_se_app_boot_info(uint32_t se_core_mask)
{
	uint32_t fuse_coremask = octeon_get_available_coremask();
	uint32_t app_coremask = fuse_coremask & ~se_core_mask;
	linux_app_boot_info_t *labi =
	    (linux_app_boot_info_t *) (0x80000000 + LABI_ADDR_IN_BOOTLOADER);
	labi->labi_signature = LABI_SIGNATURE;
	if (labi->avail_coremask)
		labi->avail_coremask &= app_coremask & coremask_from_eeprom;
	else
		labi->avail_coremask = app_coremask & coremask_from_eeprom;
	labi->cur_exception_base = cur_exception_base;
	octeon_translate_gd_to_linux_app_global_data(&labi->gd);
}

/* ======================================================================
 * Interpreter command to boot an Octeon simple exec ELF image from memory.
 * ====================================================================== */

int do_bootocteon(cmd_tbl_t * cmdtp, int flag, int argc, char *const argv[])
{
	unsigned long addr;	/* Address of the ELF image     */
	uint32_t stack_size = DEFAULT_STACK_SIZE;
	uint32_t heap_size = DEFAULT_HEAP_SIZE;
	int64_t core_mask = 1;	/* default to 1 */
	int i;
	int num_cores = 0;
	int skip_cores = 0;
	/* Use coremask_override to restrict cores used on bringup boards
	 * This value is anded with coremask to determine allowed cores
	 */

#if CONFIG_OCTEON_SIM_SW_DIFF
	/* Default is to run on all cores on simulator */
	core_mask = CVMX_COREMASK_MAX & cvmx_read_csr(CVMX_CIU_FUSE);
#endif

	/* -------------------------------------------------- */
	int rcode = 0;

	if (argc < 2) {
		addr = load_addr;
	} else {
		addr = simple_strtoul(argv[1], NULL, 16);
		if (!addr)
			addr = load_addr;
	}

	/* Last arg seems to get a \n on it, so clean it up */
	if (argv[argc - 1][strlen(argv[argc - 1]) - 1] == '\n')
		argv[argc - 1][strlen(argv[argc - 1]) - 1] = '\0';

	for (i = 2; i < argc; i++) {
#if 0
		printf("argv[%d]: >%s<\n", i, argv[i]);
#endif
		if (!strncmp(argv[i], "stack=", 6)) {
			stack_size = simple_strtoul(argv[i] + 6, NULL, 0);
		} else if (!strncmp(argv[i], "heap=", 5)) {
			heap_size = simple_strtoul(argv[i] + 5, NULL, 0);
		} else if (!strncmp(argv[i], "debug", 5)) {
			printf("setting debug flag!\n");
			boot_flags |= OCTEON_BL_FLAG_DEBUG;
		}
#if OCTEON_APP_INIT_H_VERSION >= 3
		else if (!strncmp(argv[i], "break", 5)) {
			printf("setting break flag!\n");
			boot_flags |= OCTEON_BL_FLAG_BREAK;
		}
#endif
		else if (!strncmp(argv[i], "coremask=0x", 11)) {
			core_mask = simple_strtoull(argv[i] + 9, NULL, 0);
		} else if (!strncmp(argv[i], "coremask=", 9)) {
			char tmp[20] = "0x";
			strncat(tmp, argv[i] + 9, 10);
			core_mask = simple_strtoull(tmp, NULL, 0);
		} else if (!strncmp(argv[i], "forceboot", 9)) {
			forceboot = 1;
		} else if (!strncmp(argv[i], "numcores=", 9)) {
			num_cores = simple_strtoul(argv[i] + 9, NULL, 0);
		} else if (!strncmp(argv[i], "skipcores=", 10)) {
			skip_cores = simple_strtoul(argv[i] + 10, NULL, 0);
		} else if (!strncmp(argv[i], "endbootargs", 12)) {
			argc -= i + 1;
			argv = &argv[i + 1];
			break;	/* stop processing argument */
		}
	}

	/* numcores specification overrides a coremask on the same command line */
	if (num_cores) {
		core_mask = octeon_coremask_num_cores(num_cores + skip_cores)
			    & ~octeon_coremask_num_cores(skip_cores);
	}

	/* Remove cores from coremask based on environment variable stored in
	 * flash
	 */
	core_mask = validate_coremask(core_mask);
	if (!core_mask) {
		printf("Coremask is empty after coremask_override mask.  "
		       "Nothing to do.\n");
		return (0);
	} else if (core_mask < 0) {
		printf("Invalid coremask.\n");
		return (1);
	}

	/* Check environment for forceboot flag */
	if (getenv("forceboot"))
		forceboot |= 1;

	if (core_mask & coremask_to_run) {
		printf("ERROR: Can't load code on core twice! (provided "
		       "coremask(0x%llx) overlaps previously loaded coremask(0x%x))\n",
		       core_mask, coremask_to_run);
		return (-1);
	}

	setup_se_app_boot_info(core_mask);

	dprintf("Bootloader: Booting Octeon Executive application at 0x%08lx, "
	        "core mask: 0x%llx, stack size: 0x%x, heap size: 0x%x\n",
	        addr, core_mask, stack_size, heap_size);

	rcode = load_elf(addr, argc, argv,
			 stack_size, heap_size, core_mask, -1);

#if CONFIG_OCTEON_SIM_HW_DIFF
	if (rcode < 0) {
		/* If we have an error loading the ELF file, we want to abort
		 * the simulation
		 */
		printf("Aborting simulation - ignore errors below this....\n");
		/* Hack to get simulator to exit on error */
		char *hack_ptr = (void *)0xbfc00000;
		/* Put break instruction at reset vector */
		*hack_ptr++ = 0x0;
		*hack_ptr++ = 0x0;
		*hack_ptr++ = 0x0;
		*hack_ptr++ = 0xd;
		OCTEON_SYNC;
		/* take rest of cores out of reset */
		/* TODO: this seems to cause simulator assertion, need to fix
		 * for multicore exiting
		 */
		cvmx_write_csr(CVMX_CIU_PP_RST, 0x0);
		OCTEON_SYNC;
		asm volatile ("break\n");
	}
#endif
	if (rcode < 0) {
		printf("ERROR: Unable to properly load application\n");
	}

	return rcode;
}

/* ====================================================================== */
U_BOOT_CMD(bootoct, 128, 0, do_bootocteon,
	   "Boot from an Octeon Executive ELF image in memory",
	   " [elf_address [stack=stack_size] [heap=heap_size] [coremask=mask_to_run | numcores=core_cnt_to_run] [forceboot] [debug] [break] [endbootargs] [app name] [app_args ...] .\n"
	   "elf_address - address of ELF image to load. defaults to $(loadaddr). If 0, default load address used.\n"
	   "stack       - size of stack in bytes.  Defaults to 1 megabyte\n"
	   "heap        - size of heap in bytes.  Defaults to 3 megabytes\n"
	   "coremask    - mask of cores to run on.  Anded with coremask_override environment\n"
	   "              variable to ensure only working cores are used\n"
	   "numcores    - number of cores to run on.  Runs on specified number of cores, taking into\n"
	   "              account the coremask_override.\n"
	   "skipcores   - only meaningful with numcores.  Skips this many cores (starting from 0) when\n"
	   "              loading the numcores cores. For example, setting skipcores to 1 will skip core 0\n"
	   "              and load the application starting at the next available core.\n"
	   "debug       - if present, bootloader passes debug flag to application which will cause\n"
	   "              the application to stop at a breakpoint on startup\n"
	   "break       - if present, exit program when control-c is received on the console\n"
	   "forceboot   - if set, boots application even if core 0 is not in mask\n"
	   "endbootargs - if set, bootloader does not process any further arguments and only passes\n"
	   "              the arguments that follow to the application.  If not set, the application\n"
	   "              gets the entire commnad line as arguments.\n"
	   "app name    - U-boot has no way of knowing the name of the application so this must be the first argument.\n"
	   "\n");
#endif

struct elf64headertype {
	char ident[16];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint64_t entry;
	uint64_t phoff;
	uint64_t shoff;
	uint32_t flags;
	uint16_t ehsize;
	uint16_t phentsize;
	uint16_t phnum;
	uint16_t shentsize;
	uint16_t shnum;
	uint16_t shstrndx;
};
struct elf64programheadertype {
	uint32_t type;
	uint32_t flags;
	uint64_t offset;
	uint64_t vaddr;
	uint64_t paddr;
	uint64_t filesz;
	uint64_t memsz;
	uint64_t align;
};
struct elf64sectionheadertype {
	uint32_t name;
	uint32_t type;
	uint64_t flags;
	uint64_t addr;
	uint64_t offset;
	uint64_t size;
	uint32_t link;
	uint32_t info;
	uint64_t addralign;
	uint64_t entsize;
};

struct elf32headertype {
	char ident[16];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint32_t entry;
	uint32_t phoff;
	uint32_t shoff;
	uint32_t flags;
	uint16_t ehsize;
	uint16_t phentsize;
	uint16_t phnum;
	uint16_t shentsize;
	uint16_t shnum;
	uint16_t shstrndx;
};
struct elf32programheadertype {
	uint32_t type;
	uint32_t offset;
	uint32_t vaddr;
	uint32_t paddr;
	uint32_t filesz;
	uint32_t memsz;
	uint32_t flags;
	uint32_t align;
};
struct elf32sectionheadertype {
	uint32_t name;
	uint32_t type;
	uint32_t flags;
	uint32_t addr;
	uint32_t offset;
	uint32_t size;
	uint32_t link;
	uint32_t info;
	uint32_t addralign;
	uint32_t entsize;
};

typedef struct {
	uint32_t mem_addr;
	uint32_t mem_size;
	uint32_t file_offset;
	uint32_t file_size;
	uint32_t flags;
	uint32_t type;

} segment_desc_t;

typedef struct mem_list_entry {
	uint64_t start_offset;
	uint64_t size;
	struct mem_list_entry *next;
} mem_list_entry_t;

/* Global phy mem list head */
mem_list_entry_t *phy_mem_list_head;

mem_list_entry_t *alloc_mem_list_entry(void)
{
	mem_list_entry_t *tmp;
	tmp = malloc(sizeof(mem_list_entry_t));
	tmp->start_offset = 0;
	tmp->size = 0;
	tmp->next = NULL;

	return (tmp);
}

void print_oct_tlb_entry(octeon_tlb_entry_t_bl * te)
{
	printf("p1=%16llx p2=%16llx  v=%16llx  sz=%8x sh=%d \n", te->phy_page0,
	       te->phy_page1, te->virt_base, te->page_size, te->flags.seg_type);
}

void print_boot_block(boot_info_block_t * i)
{
	printf("e=%16llx b=%16llx s=%16llx ex=%8x cvmx=%16llx flags=%16llx\n",
	       i->entry_point, i->boot_desc_addr, i->stack_top,
	       i->exception_base, i->cvmx_desc_addr, i->flags);
}

void print_boot_array(uint32_t core_mask)
{
	int core, i;
	for (core = coremask_iter_init(core_mask);
	     core >= 0;
	     core = coremask_iter_next()) {
		printf("c=%02d ", core);
		print_boot_block(&boot_info_block_array[core]);
	}
	for (core = coremask_iter_init(core_mask);
	     core >= 0;
	     core = coremask_iter_next()) {
		for (i = 0; i < NUM_OCTEON_TLB_ENTRIES; i++) {
			if (boot_info_block_array[core].tlb_entries[i].virt_base != 0) {
				printf("c=%02d %02d ", core, i);
				print_oct_tlb_entry(&boot_info_block_array[core].tlb_entries[i]);
			}
		}
	}
}

void print_tlb_array(octeon_tlb_entry_t_bl * tlb_array, int num_entries)
{
	int i;

	printf("TLB addr: %p\n", tlb_array);
	for (i = 0; i < num_entries; i++) {
		int shared = tlb_array[i].flags.seg_type;;
		if (!tlb_array[i].page_size)
			break;
		printf("TLB[%02d] page0: 0x%010llx", i, tlb_array[i].phy_page0);
		printf(" page1: 0x%010llx", tlb_array[i].phy_page1);
		printf(" pagesize:      0x%08lx (%010d)",
		       TLB_PAGE_SIZE_FROM_EXP(tlb_array[i].page_size),
		       tlb_array[i].page_size);
		printf(" virt : 0x%llx - 0x%llx seg_type=%d \n",
		       tlb_array[i].virt_base,
		       tlb_array[i].virt_base +
		       (1ULL << (tlb_array[i].page_size + 1)), shared);
#if 0
		printf("TLB[%02d] flags   : 0x%x\n", i, tlb_array[i].flags);
#endif
	}

}

/**
 * Computes the required TLB mapping size for a given region.  Now limits
 * mappings to a max size, and returns the max size if the request is larger
 * than that.
 *
 * @param vaddr_start
 *                  start virtual address
 * @param vaddr_end end virtual address
 * @param num_tlb_entries
 *                  number of TLB entries to use.  MUST BE 1.
 * @param alignment_exp
 *                  variable to put page (NOT TLB entry/VADDR) alignment
 *                  exponent in
 *
 * @return Mapping size in bytes on success
 *         0 on failures
 */
uint64_t get_tlb_mapping_size(uint64_t vaddr_start, uint64_t vaddr_end,
			      uint32_t num_tlb_entries,
			      uint32_t * alignment_exp)
{

	uint32_t tlb_page_size_exp;
	uint32_t tlb_page_size;

	/* assume only one TLB entry per range for now.... */
	if (num_tlb_entries != 1 || !alignment_exp) {
		return (0);
	}

	/* cycle through all page sizes, from smallest to largest allowed.
	 * We limit ourselves to 16 MByte pages (32 Mbytes per tlb entry.
	 */
	for (tlb_page_size_exp = 12; tlb_page_size_exp < 24;
	     tlb_page_size_exp += 2) {
		tlb_page_size = 0x1UL << tlb_page_size_exp;
		if (2 * tlb_page_size +
		    ALIGN_ADDR_DOWN(vaddr_start,
				    ALIGN_MASK(tlb_page_size_exp)) > vaddr_end) {
			*alignment_exp = tlb_page_size_exp;
			return (2 * tlb_page_size);
		}
		/* we align the start/end addresses to page alignments, and
		 * then see if the range is smaller that two pages.
		 */
	}

	/* Return max size if won't fit in one  */
	tlb_page_size = 0x1UL << tlb_page_size_exp;
	*alignment_exp = tlb_page_size_exp;
	return (2 * tlb_page_size);

	return (0);		/* range not mappable */
}

/* Returns 0 if range is not already mapped, !0 if mapped */
static int is_range_tlb_mapped(octeon_tlb_entry_t_bl * tlb_array,
			       uint64_t vaddr_low, uint64_t vaddr_high)
{

	int tlb_index = 0;

	dprintf("checking range: 0x%llx - 0x%llx\n", vaddr_low, vaddr_high);
	for (tlb_index = 0; tlb_index < get_num_tlb_entries(); tlb_index++) {
		if (tlb_array[tlb_index].page_size != 0) {
			uint64_t entry_base = tlb_array[tlb_index].virt_base;
			uint64_t entry_top = entry_base +
			    (1ull << (tlb_array[tlb_index].page_size + 1)) - 1;

			dprintf("checking entry %d: 0x%llx - 0x%llx\n",
				tlb_index, entry_base, entry_top);
			if (vaddr_low >= entry_base && vaddr_low <= entry_top)
				return 1;
			if (vaddr_high >= entry_base && vaddr_high <= entry_top)
				return 1;
			/* New region does not start or end within block, see
			 * if it contains the block.
			 */
			if (vaddr_low <= entry_base && vaddr_high >= entry_top)
				return 1;
		}
	}
	dprintf("Range check passed!\n");

	return 0;

}

/* Add a single tlb entry, and allocate memory for it. */
static int add_tlb_mapping_single(octeon_tlb_entry_t_bl * tlb_array,
				  uint64_t vaddr_low, uint64_t vaddr_high,
				  tlb_flags_t tlb_flags,
				  mem_list_entry_t ** phy_mem_list)
{
	int64_t page0, page1;
	uint32_t alignment_exp;
	uint64_t map_size;
	int tlb_index = 0;
	uint64_t vbase;

#if defined(__U_BOOT__)
	int use_hi_mem_for_app = 0;
	/* For non-63XX chips, if env variable set, use the high 256 meg block
	 * for TLB mapped program code/data.  This leaves more low memory
	 * available for the application.  This may be useful for some
	 * 32 bit applications since the high 256 Meg block is not 32 bit
	 * addressable.
	 */
	if (!octeon_is_model(OCTEON_CN6XXX)
	    && !octeon_is_model(OCTEON_CNF7XXX)
	    && getenv("octeon_load_app_hi"))
		use_hi_mem_for_app = 1;
#endif

	map_size =
	    get_tlb_mapping_size(vaddr_low, vaddr_high, 1, &alignment_exp);
	if (!map_size)
		return (-1);

	dprintf("add_tlb_mapping_single: vaddr: 0x%llx - 0x%llx, "
	        "mapping size: 0x%llx, alignment exp: %d, align_mask: 0x%llx\n",
	        vaddr_low, vaddr_high, map_size, alignment_exp,
	        ALIGN_MASK(alignment_exp));
	vbase = vaddr_low & ALIGN_MASK(alignment_exp + 1);

	if (is_range_tlb_mapped(tlb_array, vbase, vbase + map_size - 1)) {
		printf("ERROR: TLB overlap - out of virtual memory in "
		       "0x10000000 - 0x20000000 range\n");
		return -1;
	}
#if defined(__U_BOOT__)
	/* Try to allocate from high 256 meg block first if requested */
	if (use_hi_mem_for_app)
		page0 = cvmx_bootmem_phy_alloc(map_size >> 1,
					       0x410000000ull,
					       0x41FFFFFFFull,
					       1 << alignment_exp,
					       0);
	else
		page0 = -1;
	if (page0 < 0)
		page0 = cvmx_bootmem_phy_alloc(map_size >> 1, 0, 0,
					       1 << alignment_exp, 0);
#else
	page0 =  cvmx_bootmem_phy_alloc(map_size >> 1, 0, 0xFFFFFFFF,
				        1 << alignment_exp, 0);
#endif

	/* Check to see if we can use just a single page.  If so, we will map
	 * one page twice.
	 */
	if (vbase + (map_size >> 1) > vaddr_high) {
		/* A half mapping will work, so re-use page 0.  The app should
		 * never use the upper half of the virt. range, and it if does
		 * it will only mess itself up.
		 */
		page1 = page0;
		dprintf("Using a half mapping!\n");

	} else {
		/* Map a real page 1 */
#if defined(__U_BOOT__)
		if (use_hi_mem_for_app)
			page1 = cvmx_bootmem_phy_alloc(map_size >> 1,
						       0x410000000ull,
						       0x41FFFFFFFull,
						       1 << alignment_exp, 0);
		else
			page1 = -1;
		if (page1 < 0)
			page1 = cvmx_bootmem_phy_alloc(map_size >> 1, 0, 0,
						       1 << alignment_exp, 0);
#else
		page1 = cvmx_bootmem_phy_alloc(map_size >> 1, 0, 0xFFFFFFFF,
					       1 << alignment_exp, 0);
#endif
	}

	if (page0 < 0 || page1 < 0) {
		printf("Bootloader error: out of memory.\n");
		return (-1);
	}

	dprintf("add_tlb_mapping: phy blocks used: 0x%llx, 0x%llx\n", page0,
		page1);

	/* Find first free TLB entry */

	while (tlb_array[tlb_index].page_size != 0
	       && tlb_index < get_num_tlb_entries())
		tlb_index++;

	if (tlb_index == get_num_tlb_entries())
		return (-1);

	/* Only support 1 TLB entry/mapping currently */
	/* page size used as valid indicator */
	tlb_array[tlb_index].page_size = alignment_exp;
	tlb_array[tlb_index].phy_page0 = page0;
	tlb_array[tlb_index].phy_page1 = page1;
	tlb_array[tlb_index].virt_base = vbase;
	tlb_array[tlb_index].flags = tlb_flags;

	return (tlb_index);
}

/* Add a tlb mapping for a specified region.  Uses TLB mapping sizes up to
 * 32 MBytes (16 MByte pages) - uses multiple TLB entries for large regions
 */
int add_tlb_mapping(octeon_tlb_entry_t_bl * tlb_array, uint64_t vaddr_low,
		    uint64_t vaddr_high, int num_entries, tlb_flags_t tlb_flags,
		    mem_list_entry_t ** phy_mem_list)
{
	uint32_t alignment_exp;
	uint64_t map_size;
	uint64_t vbase;
	int tlb_index = -1;

	map_size = get_tlb_mapping_size(vaddr_low, vaddr_high, 1, &alignment_exp);
	if (!map_size) {
		dprintf("Can't get mapping size!\n");
		return (-1);
	}
	dprintf("add_tlb_mapping: vaddr: 0x%llx - 0x%llx, mapping size: 0x%llx, "
	        "alignment exp: %d, align_mask: 0x%llx\n",
	         vaddr_low, vaddr_high, map_size, alignment_exp,
	         ALIGN_MASK(alignment_exp));

	/* Align low address to match TLB size selected. */
	vbase = vaddr_low & ALIGN_MASK(alignment_exp + 1);

	while (num_entries > 0 && vbase < vaddr_high) {
		uint64_t end_addr = vbase + map_size - 1;

		/* Map less then entire mapping if we don't have to */
		if (vaddr_high < vbase + map_size - 1)
			end_addr = vaddr_high;
		tlb_index = add_tlb_mapping_single(tlb_array, vbase, end_addr,
						   tlb_flags, phy_mem_list);
		if (tlb_index < 0) {
			dprintf("Error adding TLB entry.\n");
			return -1;
		}
		vbase += map_size;
		num_entries--;

		/* Figure out mapping size for next segment, as not all
		 * segments need to be 32 MByte
		 */
		map_size = get_tlb_mapping_size(vbase, vaddr_high,
						1, &alignment_exp);
		if (!map_size) {
			dprintf("Can't get mapping size (loop)!\n");
			return (-1);
		}
		if (vbase != (vbase & ALIGN_MASK(alignment_exp + 1))) {
			dprintf("ERROR creating 2nd+ TLB mappings\n");
			return (-1);
		}
	}

	if (num_entries < 0 || vaddr_high > vbase) {
		dprintf("Error generating TLB mappings: entries: %d, vlow: 0x%llx, vhigh: 0x%llx\n",
		        num_entries, vaddr_low, vaddr_high);
		/* We did not complete successfully */
		return -1;
	}

	return tlb_index;
}

uint32_t copy_tlb_entry(octeon_tlb_entry_t_bl * dest_tlb_array,
			octeon_tlb_entry_t_bl * src_entry_ptr)
{
	int32_t i;
	for (i = 0; i < get_num_tlb_entries(); i++) {
		if (!dest_tlb_array[i].page_size)
			break;
	}
	if (i == get_num_tlb_entries())
		return (~0UL);
	memcpy(&dest_tlb_array[i], src_entry_ptr,
	       sizeof(octeon_tlb_entry_t_bl));

	return (i);
}

uint32_t mem_set_tlb(octeon_tlb_entry_t_bl * tlb_array, uint64_t dest_virt,
		     uint8_t val, uint32_t len)
{
	uint32_t cur_len;
	uint64_t dest_phys;
	uint32_t chunk_len;
	int tlb_index;

	cur_len = len;

	for (tlb_index = 0;
	     tlb_index < get_num_tlb_entries() && cur_len > 0;
	     tlb_index++) {
		uint32_t page_size =
		    TLB_PAGE_SIZE_FROM_EXP(tlb_array[tlb_index].page_size);

		/* We found a page0 mapping that contains the start of the block */
		if (dest_virt >= (tlb_array[tlb_index].virt_base)
		    && dest_virt < (tlb_array[tlb_index].virt_base + page_size)) {
			dest_phys =
			    tlb_array[tlb_index].phy_page0 + (dest_virt -
							      tlb_array[tlb_index].virt_base);
			chunk_len =
			    MIN((tlb_array[tlb_index].virt_base + page_size) - dest_virt,
				cur_len);

			dprintf("Zeroing 0x%x bytes at 0x%x (phys), 0x%llx virt\n",
			        chunk_len, dest_phys, dest_virt);
			memset64(dest_phys, val, chunk_len);

			cur_len -= chunk_len;
			dest_virt += chunk_len;
		}
		/* We found a page1 mapping that contains the start of the block */
		if (cur_len > 0
		    && dest_virt >= (tlb_array[tlb_index].virt_base + page_size)
		    && dest_virt < (tlb_array[tlb_index].virt_base + 2 * page_size)) {
			dest_phys =
			    tlb_array[tlb_index].phy_page1 + (dest_virt -
							      (tlb_array[tlb_index].virt_base +
							       page_size));
			chunk_len =
			    MIN((tlb_array[tlb_index].virt_base +
				 2 * page_size) - dest_virt, cur_len);

			dprintf("Zeroing 0x%x bytes at 0x%x (phys), 0x%llx virt\n",
			        chunk_len, dest_phys, dest_virt);
			memset64(dest_phys, val, chunk_len);

			cur_len -= chunk_len;
			dest_virt += chunk_len;
		}
	}
	if (cur_len != 0) {
		printf("ERROR clearing memory using TLB mappings!\n");
		return (0);
	} else {
		return (len);
	}
}

#if defined(__U_BOOT__)
/* Force this function not to be inlined as the local copies
   are read from the inline-asm but are not correctly described
   there. */
static void octeon_setup_crt0_tlb(uint32_t stack_addr_ptr,
				  uint32_t entry_addr_ptr,
				  uint32_t desc_vaddr_ptr) __attribute__((noinline));

static void octeon_setup_crt0_tlb(uint32_t stack_addr_ptr,
				  uint32_t entry_addr_ptr,
				  uint32_t desc_vaddr_ptr)
{
	uint64_t unmapped_ram_addr;

	/* Look this up before we set TLB registers, as uboot_tlb_ptr_to_phys
	 * does a TLB lookup and disturbs the registers we care about
	 */
	unmapped_ram_addr = MAKE_XKPHYS(uboot_tlb_ptr_to_phys(&tlbwi_and_eret));

	/* Set up TLB registers to clear desired entry.  The actual
	 * tlbwi instruction is done in ASM when running from unmapped DRAM
	 */
	write_64bit_c0_entrylo0(0);
	write_c0_pagemask(0);
	write_64bit_c0_entrylo1(0);
	write_64bit_c0_entryhi(0xFFFFFFFF91000000ull);
	write_c0_index(get_num_tlb_entries() - 1);

	/* We need to execute the final instructions that clears the final TLB
	 * entry from unmapped DRAM.  Here we figure out what address we need
	 * to jump to do do that.
	 */

	asm volatile (
		      /* Use $at as temp register */
		      "       .set push                        \n"
		      "       .set mips64                      \n"
		      "       .set noreorder                   \n"
		      "       .set noat                        \n"
		      /* Set stack address */
		      "       ld       $29, 0(%[s_addr])       \n"
		      "       ld       %[e_addr],  0(%[e_addr])\n"
		      /* Clear performance counters */
		      "       dmtc0    $0, $25, 1              \n"
		      "       dmtc0    $0, $25, 3              \n"
		      /* Error EPC */
		      "       dmtc0    %[e_addr], $30, 0       \n"
		      "       dmtc0    %[e_addr], $14, 0       \n"
		      /* Enable unaligned instructions */
		      "       dmfc0    $at, $9,7               \n"
		      "       ori      $at, 0x1000             \n"
		      "       dmtc0    $at, $9,7               \n"
		      "       j        %[ram_addr]             \n"
		      /* Pass descriptor address in r16 */
		      "       ld       $16, 0(%[d_addr])       \n"
		      "       .set pop              \n"
		      ::[s_addr] "r"(stack_addr_ptr),
		      [e_addr] "r"(entry_addr_ptr),
		      [d_addr] "r"(desc_vaddr_ptr),
		      [ram_addr] "r"(unmapped_ram_addr));
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
    __builtin_unreachable ();
#endif

}

#define OCTEON_TLBWI                       asm volatile ("tlbwi" : : )
#define OCTEON_TLBR                        asm volatile ("tlbr" : : )
#define OCTEON_EHB                         asm volatile ("ehb" : : )
#define OCTEON_NOP                         asm volatile ("nop" : : )

#define TLB_DIRTY   (0x1ULL<<2)
#define TLB_VALID   (0x1ULL<<1)
#define TLB_GLOBAL  (0x1ULL<<0)

void bl_write_tlb(uint64_t hi, uint64_t lo0, uint64_t lo1, uint64_t mask,
		  uint64_t index)
{
#if 0
#ifdef DEBUG
	printf("Writing TLB entry:\n index: 0x%llx\n hi: 0x%llx\n lo0: 0x%llx\n"
	       " lo1: 0x%llx\n mask: 0x%llx\n",
	       index, hi, lo0, lo1, mask);
#endif
#endif

	write_64bit_c0_entryhi(hi);
	write_64bit_c0_entrylo0(lo0);
	write_64bit_c0_entrylo1(lo1);
	write_c0_pagemask(mask);
	write_c0_index(index);
	write_c0_wired(index + 1);
	OCTEON_EHB;
	OCTEON_TLBWI;
	OCTEON_EHB;
}

static const char hexchars[] = "0123456789abcdef";
static char *mem2hex(char *mem, char *buf, int count)
{
	unsigned char ch;

	while (count-- > 0) {
		ch = *mem++;
		*buf++ = hexchars[ch >> 4];
		*buf++ = hexchars[ch & 0xf];
	}
	*buf = 0;
	return buf;
}

char *u64_to_hex(uint64_t val)
{
	static char buf[20];
	mem2hex((char *)&val, buf, 8);
	return (buf);
}

/**
 * Processes boot descriptor block, and then starts application.
 * Stack pointer must be valid on entering this function,
 * stack pointer is set to address defined in boot descriptor before app is
 * started.
 * This is run on all cores.
 */
void start_app(void)
{
	int entry;
	int core_num;
	uint64_t val;
	octeon_tlb_entry_t_bl *tlb_entries;
	boot_info_block_t *boot_info_ptr;
	boot_init_vector_t *boot_vect =
	    (boot_init_vector_t *) (BOOT_VECTOR_BASE);
	extern void octeon_sync_cores(void);

	core_num = get_core_num();

	/* Here we look up the boot_info_ptr again.  We only need to do this for
	 * the bootoct Linux utility case, but since u-boot set up
	 * boot_info_addr properly for its own use it works in both cases.
	 */
	boot_info_ptr = (void *)(uint32_t) boot_vect[core_num].boot_info_addr;

	tlb_entries = boot_info_ptr->tlb_entries;

	/* Set local cycle counter based on global counter in IPD */
	octeon_init_cvmcount();

	set_except_base_addr(boot_info_ptr->exception_base);

	/* Check to see if we should enable icache prefetching.  On pass 1
	 * it should always be disabled
	 */
	val = get_cop0_cvmctl_reg();
	if ((boot_info_ptr->flags & BOOT_INFO_FLAG_DISABLE_ICACHE_PREFETCH))
		val |= (1ull << 13);
	else
		val &= ~(1ull << 13);
	/* Disable Fetch under fill on CN63XXp1 due to errata Core-14317 */
	if (octeon_is_model(OCTEON_CN63XX_PASS1_X))
		val |= (1ull << 19);	/*CvmCtl[DEFET] */
	set_cop0_cvmctl_reg(val & 0xfffff3);

	/* Disable core stalls on write buffer full */
	if (octeon_is_model(OCTEON_CN38XX_PASS2)) {
		val = get_cop0_cvmmemctl_reg();
		val |= 1ull << 29;	/* set diswbfst */
		set_cop0_cvmmemctl_reg(val);
	}

	/* Allow use of XI bit in TLB */
	if (!octeon_is_model(OCTEON_CN3XXX)) {
		uint32_t val;
		CVMX_MF_PAGEGRAIN(val);
		val |= 1 << 30;
		CVMX_MT_PAGEGRAIN(val);
	}

	for (entry = 0; entry < 32; entry++) {
		if (tlb_entries[entry].page_size) {
			if (tlb_entries[entry].phy_page0 !=
			    tlb_entries[entry].phy_page1) {
				bl_write_tlb(tlb_entries[entry].virt_base,
					     ((uint64_t) tlb_entries[entry].flags.xi << 62) |
					     tlb_entries[entry].phy_page0 >> 6 |
					     (tlb_entries[entry].flags.cca << 3) | 0x7,
					     ((uint64_t) tlb_entries[entry].flags.xi << 62) |
					     tlb_entries[entry].phy_page1 >> 6 |
					     (tlb_entries[entry].flags.cca << 3) | 0x7,
					     ~(ALIGN_MASK(tlb_entries[entry].page_size) << 1),
					     entry);
			} else {
				/* We used to map both halves of the virtual
				 * space to the same page.
				 * Better to make one page invalid
				 */
				bl_write_tlb(tlb_entries[entry].virt_base,
					     ((uint64_t) tlb_entries[entry].flags.xi << 62) |
					     tlb_entries[entry].phy_page0 >> 6 |
					     (tlb_entries[entry].flags.cca << 3) |
					     0x7,
					     0,	/* Page 1 is invalid */
					     ~(ALIGN_MASK(tlb_entries[entry].page_size) << 1),
					     entry);
			}

		}
	}

	/* Sync up cores before starting main */
	octeon_sync_cores();

	/* Pass address of stack top variable.  Also enables cavium only
	 * unaligned load/store instructions
	 * This function does not return, but performs an eret to the entry
	 * point of the application.
	 */
	/* Make local copies of arguments */
	volatile uint64_t stack_top = boot_info_ptr->stack_top;
	volatile uint64_t entry_point = boot_info_ptr->entry_point;
	volatile uint64_t desc_vaddr = boot_info_ptr->boot_desc_addr;

	/*
	 * Flush i-cache. This is needed if we have shutdown one application
	 * and are launching a new one on the same core, since i-cache may be
	 * out of sync with the new TLB entries we have setup above.
	 */
	__asm__ volatile ("synci 0($0)");

	octeon_setup_crt0_tlb((uint32_t) & stack_top, (uint32_t) & entry_point,
			      (uint32_t) & desc_vaddr);
}
#endif

/* Functions to truncate 64 bit ELF headers to 32 bit for processing */
void copy_elf64_header(struct elf32headertype *dest_eh, void *elf64_eh)
{
	struct elf64headertype *elf64_ptr = elf64_eh;

	dest_eh->type = elf64_ptr->type;
	dest_eh->machine = elf64_ptr->machine;
	dest_eh->version = elf64_ptr->version;
	dest_eh->entry = elf64_ptr->entry;
	dest_eh->phoff = (uint32_t) elf64_ptr->phoff;
	dest_eh->shoff = (uint32_t) elf64_ptr->shoff;
	dest_eh->flags = elf64_ptr->flags;
	dest_eh->ehsize = (uint32_t) elf64_ptr->ehsize;
	dest_eh->phentsize = (uint32_t) elf64_ptr->phentsize;
	dest_eh->phnum = (uint32_t) elf64_ptr->phnum;
	dest_eh->shentsize = (uint32_t) elf64_ptr->shentsize;
	dest_eh->shnum = (uint32_t) elf64_ptr->shnum;
	dest_eh->shstrndx = (uint32_t) elf64_ptr->shstrndx;

}

void copy_elf64_section_header(struct elf32sectionheadertype *dest_sh,
			       void *elf64_sh)
{
	struct elf64sectionheadertype *elf64_sh_ptr = elf64_sh;

	dest_sh->name = elf64_sh_ptr->name;
	dest_sh->type = elf64_sh_ptr->type;
	dest_sh->flags = (uint32_t) elf64_sh_ptr->flags;
	dest_sh->addr = (uint32_t) elf64_sh_ptr->addr;
	dest_sh->offset = (uint32_t) elf64_sh_ptr->offset;
	dest_sh->size = (uint32_t) elf64_sh_ptr->size;
	dest_sh->link = elf64_sh_ptr->link;
	dest_sh->info = elf64_sh_ptr->info;
	dest_sh->addralign = (uint32_t) elf64_sh_ptr->addralign;
	dest_sh->entsize = (uint32_t) elf64_sh_ptr->entsize;

}

void copy_elf64_program_header(struct elf32programheadertype *dest_ph,
			       void *elf64_ph)
{
	struct elf64programheadertype *elf64_ph_ptr = elf64_ph;

	dest_ph->type = elf64_ph_ptr->type;
	dest_ph->offset = (uint32_t) (elf64_ph_ptr->offset);
	dest_ph->vaddr = (uint32_t) (elf64_ph_ptr->vaddr);
	dest_ph->paddr = (uint32_t) (elf64_ph_ptr->paddr);
	dest_ph->filesz = (uint32_t) (elf64_ph_ptr->filesz);
	dest_ph->memsz = (uint32_t) (elf64_ph_ptr->memsz);
	dest_ph->flags = elf64_ph_ptr->flags;
	dest_ph->align = (uint32_t) (elf64_ph_ptr->align);
}

struct elf_info {
#define MAX_ELF_SEGMENTS    (20)
	char *elf_ptr;
	struct elf32headertype *ehp;
	struct elf32headertype eh;
	struct elf32programheadertype ph;
	struct elf32sectionheadertype sh;
	segment_desc_t ro_segments[MAX_ELF_SEGMENTS];
	segment_desc_t rw_segments[MAX_ELF_SEGMENTS];
	segment_desc_t shared_segment;
	uint64_t ro_min_vaddr;
	uint64_t ro_max_vaddr;
	uint64_t rw_min_vaddr;
	uint64_t rw_max_vaddr;
};

#if defined(__U_BOOT__)
int read_elf_info(struct elf_info *ei, uint32_t elf_addr)
#else
int read_elf_info(struct elf_info *ei, void *elf_addr)
#endif
{
	char *strtab_ptr;
	unsigned int strtab_size;
	uint32_t elf_type = 0;
	int h;
	uint32_t shared_data_addr = 0;

#if defined(__U_BOOT__)
	ei->elf_ptr = cvmx_phys_to_ptr(elf_addr);
#else
	ei->elf_ptr = (void *)(elf_addr);	/* For linux bootoct utility */
#endif

	ei->ehp = (void *)ei->elf_ptr;

	if (ei->ehp->ident[0] != 127 || ei->ehp->ident[1] != 'E'
	    || ei->ehp->ident[2] != 'L' || ei->ehp->ident[3] != 'F') {
		printf("Pointer(%p) not to ELF data!\n", ei->elf_ptr);
		return (-1);	// not ELF file
	}
	if (ei->ehp->ident[4] != ELFCLASS64 && ei->ehp->ident[4] != ELFCLASS32) {
		printf("Data isn't an ELF32 or ELF64 file!\n");
		return (-1);	// not ELF file
	}
	elf_type = ei->ehp->ident[4];
	if (ei->ehp->ident[5] != 2) {
		printf("Data isn't in big-endian mode.\n");
		return (-1);	// not ELF file
	}
	if (ei->ehp->type != 2) {
		printf("Data isn't an executable!\n");
		return (-1);	// not ELF file
	}
	if (ei->ehp->machine != 8 && ei->ehp->machine != 9) {
		printf("Data isn't an executable for a MIPS R3000 or R4000!\n");
		return (-1);	// not ELF file
	}

	if (elf_type == ELFCLASS32)
		memcpy(&ei->eh, ei->elf_ptr, sizeof(ei->eh));
	else
		copy_elf64_header(&ei->eh, ei->elf_ptr);

	dprintf("Entry point for ELF executable(q): 0x%x\n", ei->eh.entry);

	/* Need to find the string table section entry so that we can
	 * get names of sections
	 */
	for (h = ei->eh.shnum - 1; h >= 0; h--) {
		if (elf_type == ELFCLASS32)
			memcpy(&ei->sh,
			       ei->elf_ptr + ei->eh.shoff +
			       ei->eh.shentsize * h, sizeof(ei->sh));
		else
			copy_elf64_section_header(&ei->sh,
						  ei->elf_ptr + ei->eh.shoff +
						  ei->eh.shentsize * h);

		if (ei->sh.type == 3)	// I have a string section
		{

			strtab_ptr = ei->elf_ptr + ei->sh.offset;
			strtab_size = ei->sh.size;

			if (0 ==
			    strncmp(".shstrtab", &strtab_ptr[ei->sh.name],
				    200)) {
				dprintf("Found .shstrtab at offset 0x%x\n",
					ei->sh.offset);
				break;
			}
		}
	}
	if (h < 0) {
		printf("ERROR: unable to find section header string table, aborting\n");
		return (-1);	/* I couldn't find a section header string table */
	}

	/* I want to find the stack and heap */
	for (h = 0; h < ei->eh.shnum; h++) {
		if (elf_type == ELFCLASS32)
			memcpy(&ei->sh,
			       ei->elf_ptr + ei->eh.shoff + ei->eh.shentsize * h,
			       sizeof(ei->sh));
		else
			copy_elf64_section_header(&ei->sh,
						  ei->elf_ptr + ei->eh.shoff +
						  ei->eh.shentsize * h);

		/* printf("segment: %s found, addr: 0x%llx\n",
		 *        &strtab_ptr[ei->sh.name], ei->sh.addr);
		 */

		if (ei->sh.name < (unsigned)strtab_size &&
		    (strcmp(&strtab_ptr[ei->sh.name], ".cvmx_shared_hw") == 0)) {
			printf("Error: cvmx_shared_hw is no longer supported.\n");
			return (-1);
		}
		if (ei->sh.name < (unsigned)strtab_size
		    && (strncmp(&strtab_ptr[ei->sh.name], ".cvmx_shared", 12) == 0)) {
			/* Only make note of first entry, as we want this to
			 * match the start of the segment
			 */
			if (!shared_data_addr) {
				shared_data_addr = ei->sh.addr;
				dprintf("found shared data at: 0x%x\n",
					shared_data_addr);
			}
		}
	}
#define MAX_ELF_SEGMENTS    (20)
	int ro_seg_index = 0;
	int rw_seg_index = 0;

	/*  Now process the program headers. */
	for (h = 0; h < ei->eh.phnum; h++) {
		if (elf_type == ELFCLASS32)
			memcpy(&ei->ph,
			       ei->elf_ptr + ei->eh.phoff + ei->eh.phentsize * h,
			       sizeof(ei->ph));
		else
			copy_elf64_program_header(&ei->ph,
						  ei->elf_ptr + ei->eh.phoff +
						  ei->eh.phentsize * h);

		/* I have a loadable segment that has a non-zero memory size */
		if (ei->ph.type == 1 && ei->ph.memsz) {
			/* Check to see if the shared address is anywhere in
			 * this segment
			 */
			if (shared_data_addr >= ei->ph.vaddr
			    && shared_data_addr <= (ei->ph.vaddr + ei->ph.memsz)) {
				dprintf("Shared segment : 0x%x bytes to address 0x%x\n",
				       ei->ph.filesz, ei->ph.vaddr);
				ei->shared_segment.file_offset = ei->ph.offset;
				ei->shared_segment.file_size = ei->ph.filesz;
				ei->shared_segment.mem_addr = ei->ph.vaddr;
				ei->shared_segment.mem_size = ei->ph.memsz;
			} else if (!(ei->ph.flags & PF_W)) {
				dprintf("Readonly segment: 0x%x bytes to address 0x%x\n",
				       ei->ph.filesz, ei->ph.vaddr);
				/* Read only segment */
				ei->ro_segments[ro_seg_index].file_offset =
				    ei->ph.offset;
				ei->ro_segments[ro_seg_index].file_size =
				    ei->ph.filesz;
				ei->ro_segments[ro_seg_index].mem_addr =
				    ei->ph.vaddr;
				ei->ro_segments[ro_seg_index].mem_size =
				    ei->ph.memsz;
				ro_seg_index++;
			} else {
				dprintf("Writable segment: 0x%x bytes to address 0x%x\n",
				        ei->ph.filesz, ei->ph.vaddr);
				/* writable segment */
				ei->rw_segments[rw_seg_index].file_offset =
				    ei->ph.offset;
				ei->rw_segments[rw_seg_index].file_size =
				    ei->ph.filesz;
				ei->rw_segments[rw_seg_index].mem_addr =
				    ei->ph.vaddr;
				ei->rw_segments[rw_seg_index].mem_size =
				    ei->ph.memsz;
				rw_seg_index++;
			}
		}
	}

	/*  determine address range for each type */
	if (ro_seg_index) {
		int i;
		ei->ro_min_vaddr = ei->ro_segments[0].mem_addr;
		ei->ro_max_vaddr =
		    ei->ro_segments[0].mem_addr + ei->ro_segments[0].mem_size;
		for (i = 1; i < ro_seg_index; i++) {
			if (ei->ro_min_vaddr > ei->ro_segments[i].mem_addr)
				ei->ro_min_vaddr = ei->ro_segments[i].mem_addr;
			if (ei->ro_max_vaddr < (ei->ro_segments[i].mem_addr +
						ei->ro_segments[i].mem_size))
				ei->ro_max_vaddr = (ei->ro_segments[i].mem_addr
						    + ei->ro_segments[i].mem_size);
		}
		ei->ro_max_vaddr--;
	}

	if (rw_seg_index) {
		int i;
		ei->rw_min_vaddr = ei->rw_segments[0].mem_addr;
		ei->rw_max_vaddr =
		    ei->rw_segments[0].mem_addr + ei->rw_segments[0].mem_size;
		for (i = 1; i < rw_seg_index; i++) {
			if (ei->rw_min_vaddr > ei->rw_segments[i].mem_addr)
				ei->rw_min_vaddr = ei->rw_segments[i].mem_addr;
			if (ei->rw_max_vaddr < (ei->rw_segments[i].mem_addr +
						ei->rw_segments[i].mem_size))
				ei->rw_max_vaddr = (ei->rw_segments[i].mem_addr +
						    ei->rw_segments[i].mem_size);
		}
		ei->rw_max_vaddr--;
	}

	dprintf(" ro range: 0x%llx - 0x%llx\n rw range: 0x%llx - 0x%llx\n"
	        " shared range: 0x%llx - 0x%llx\n",
	        ei->ro_min_vaddr, ei->ro_max_vaddr, ei->rw_min_vaddr,
	        ei->rw_max_vaddr, ei->shared_segment.mem_addr,
	        ei->shared_segment.mem_addr + ei->shared_segment.mem_size - 1);

	return 0;
}

#define OCTEON_BL_FLAG_HPLUG_CORES (1 << 6)
#if defined(__U_BOOT__)
int load_elf_additional_cores(uint32_t elf_addr, int argc, char *const argv[],
			      uint32_t stack_size, uint32_t heap_size,
			      uint32_t core_mask, uint32_t new_core_mask)
#else
int load_elf_additional_cores(void *elf_addr, int argc, char *const argv[],
			      uint32_t stack_size, uint32_t heap_size,
			      uint32_t core_mask, uint32_t new_core_mask)
#endif
{
	uint32_t stack_heap_base_address = 0;
	uint32_t config_flags = 0;
	uint32_t new_cores = new_core_mask ^ core_mask;
	int first_core = get_first_core(core_mask);
	int core;
	tlb_flags_t tlb_flags = { 0, 0, 0, 0 };
	int tlb_index = 0;
	int segment;

	struct elf_info elfinfo;
	struct elf_info *ei = &elfinfo;

	memset(ei, 0, sizeof(elfinfo));

	read_elf_info(ei, elf_addr);

	if (heap_size < sizeof(boot_desc[0])) {
		printf("ERROR: heap too small: 0x%x, must be at least 0x%x "
		       "bytes for temp boot descriptor storage\n",
		       heap_size, sizeof(boot_desc[0]));
		return (-1);
	}
	/* printf("new cores=%08x core_mask=%08x new_core_mask=%08x\n",
	 *        new_cores, core_mask, new_core_mask);
	 */

	for (core = coremask_iter_init(new_cores); core >= 0;
	     core = coremask_iter_next()) {
		int i;
		for (i = 0; i < NUM_OCTEON_TLB_ENTRIES; i++) {
			if (boot_info_block_array[first_core].tlb_entries[i].virt_base == 0)
				continue;
			if ((boot_info_block_array[first_core].tlb_entries[i].flags.seg_type == RO_TLB_SEGMENT)
			    || (boot_info_block_array[first_core].tlb_entries[i].flags.seg_type == SHARED_TLB_SEGMENT)) {
				copy_tlb_entry(boot_info_block_array[core].tlb_entries,
					       &boot_info_block_array[first_core].tlb_entries[i]);
			}
		}
	}

	/* Here we 'mark' the stack, heap, and data segment for the application.
	 * This allows us to use the SYNCWS instruction.  This is new is Octeon
	 * pass2, see the HW manual for more information.
	 */
#if defined(__U_BOOT__)
	if (!getenv("no_mark_private_data"))
#else
	if (!global_linux_boot_info->labi.no_mark_private_data)
#endif
		tlb_flags.cca = 0x7;
	else
		tlb_flags.cca = 0x0;

	if (ei->rw_min_vaddr < 0x10000000 || ei->rw_max_vaddr > 0x1fffffff) {
		config_flags |= CVMX_BOOTINFO_CFG_FLAG_OVERSIZE_TLB_MAPPING;
		dprintf("rw segment too big\n");
	}

	for (core = coremask_iter_init(new_cores);
	     core >= 0;
	     core = coremask_iter_next()) {
		dprintf("core: %d\n", core);
		tlb_flags.seg_type = RW_TLB_SEGMENT;
		tlb_index =
		    add_tlb_mapping(boot_info_block_array[core].tlb_entries,
				    ei->rw_min_vaddr, ei->rw_max_vaddr, 28,
				    tlb_flags, &phy_mem_list_head);
		dprintf("RW tlb_index = %d\n\n", tlb_index);
		if (tlb_index < 0) {
			printf("ERROR CREATING TLB MAPPING (rw)\n");
			return (-1);
		}
	}

	/* Put stack/heap base above cvmx shared segment end, and align so that
	 * we won't overlap the shared mapping
	 */
	stack_heap_base_address =
	    ei->shared_segment.mem_addr + ei->shared_segment.mem_size - 1;
	stack_heap_base_address = (stack_heap_base_address +
		(1 << 25)) & ~((1 << 25) - 1);	/* 32 MByte alignment */

	if (stack_heap_base_address < 0x10000000
	    || stack_heap_base_address + stack_size + heap_size > 0x1fffffff) {
		config_flags |= CVMX_BOOTINFO_CFG_FLAG_OVERSIZE_TLB_MAPPING;
		dprintf("stack/heap segment too big\n");
	}
	for (core = coremask_iter_init(new_cores);
	     core >= 0;
	     core = coremask_iter_next()) {
		/* Determine base address of stack/heap mapping based on size,
		 * just use fake addresses for calculations
		 */
		uint32_t alignment_exp;
		uint32_t map_size =
		    get_tlb_mapping_size(stack_heap_base_address,
					 stack_heap_base_address + stack_size +
					 heap_size, 1, &alignment_exp);
		if (!map_size) {
			printf("ERROR CREATING TLB MAPPING for stack/heap\n");
			return (-1);
		}

		/* Adjust stack/heap to start at bottom of mapping, as this
		 * makes half-mappings possible
		 */
		stack_heap_base_address =
		    stack_heap_base_address & ALIGN_MASK(alignment_exp + 1);

		/* We now have the mapping size, so now we can compute the base
		 * address for the mapping
		 */
		dprintf("core: %d\n", core);
		tlb_flags.seg_type = STACK_HEAP_TLB_SEGMENT;
		tlb_index =
		    add_tlb_mapping(boot_info_block_array[core].tlb_entries,
				    stack_heap_base_address,
				    stack_heap_base_address + stack_size +
				    heap_size - 1, 28, tlb_flags,
				    &phy_mem_list_head);
		dprintf("Stack/heap tlb_index = %d\n\n", tlb_index);
		if (tlb_index < 0) {
			printf("ERROR CREATING TLB MAPPING for stack/heap\n");
			return (-1);
		}
	}

	/* Copy the writable segments for each core. */
	for (core = coremask_iter_init(new_cores); core >= 0;
	     core = coremask_iter_next()) {
		for (segment = 0; segment < MAX_ELF_SEGMENTS; segment++) {
			if (ei->rw_segments[segment].file_size) {
				mem_copy_tlb(boot_info_block_array[core].tlb_entries,
					     ei->rw_segments[segment].mem_addr,
#if defined(__U_BOOT__)
					     MAKE_XKPHYS(ei->rw_segments[segment].file_offset +
							 elf_addr),
#else
					     ei->rw_segments[segment].file_offset + (uint64_t) elf_addr,
#endif
					     ei->rw_segments[segment].file_size);
				mem_set_tlb(boot_info_block_array[core].tlb_entries,
					    ei->rw_segments[segment].mem_addr +
					    ei->rw_segments[segment].file_size,
					    0x0,
					    ei->rw_segments[segment].mem_size -
					    ei->rw_segments[segment].file_size);
			}
		}
	}
	boot_flags |= OCTEON_BL_FLAG_HPLUG_CORES;
	if (octeon_setup_boot_desc_block(core_mask, argc, argv, ei->eh.entry,
					 stack_size, heap_size, boot_flags,
					 stack_heap_base_address, 0,
					 config_flags, new_core_mask, -1)) {
		printf("ERROR setting up boot descriptor block, core_mask: 0x%x 0x%x \n",
		       core_mask, new_core_mask);
		return (-1);
	}
#ifdef DEBUG_TLB
	for (core = coremask_iter_init(new_core_mask); core >= 0;
	     core = coremask_iter_next()) {
		printf("\nCore %d:\n", core);
		print_tlb_array(boot_info_block_array[core].tlb_entries, 32);
		printf("----------------------------------\n");
	}
#endif

	return 0;
}

#if defined(__U_BOOT__)
static
int load_elf(uint32_t elf_addr, int argc, char *const argv[],
	     uint32_t stack_size, uint32_t heap_size, uint32_t core_mask,
	     int app_index)
#else
int load_elf(void *elf_addr, int argc, char *const argv[],
	     uint32_t stack_size, uint32_t heap_size, uint32_t core_mask,
	     int app_index)
#endif
{
	uint32_t stack_heap_base_address = 0;
	uint32_t config_flags = 0;

	struct elf_info elfinfo;
	struct elf_info *ei = &elfinfo;

	memset(ei, 0, sizeof(elfinfo));

	read_elf_info(ei, elf_addr);

	if (heap_size < sizeof(boot_desc[0])) {
		printf("ERROR: heap too small: 0x%x, must be at least 0x%x "
		       "bytes for temp boot descriptor storage\n",
		       heap_size, sizeof(boot_desc[0]));
		return (-1);
	}

	/* We have 3 regions to map:
	 *   code/readonly data
	 *   shared data
	 *   private data (per core)
	 */

	{
		int tlb_index = 0;
		tlb_flags_t tlb_flags = { 0, 0, 0, 0 };
		int prev_tlb_index = 0;	/* Used to track entries to copy */
		int i;

		int core;

		/* We need these mapped the same on all cores, and we only want
		 * to allocate physical memory once
		 */
		if (ei->ro_min_vaddr < 0x10000000
		    || ei->ro_max_vaddr > 0x1fffffff) {
			config_flags |=
			    CVMX_BOOTINFO_CFG_FLAG_OVERSIZE_TLB_MAPPING;
			dprintf("ro segment too big\n");
		}

		for (core = coremask_iter_init(core_mask); core >= 0;
		     core = coremask_iter_next()) {
			tlb_flags.seg_type = RO_TLB_SEGMENT;
			if (coremask_iter_get_first_core() == core) {
				tlb_index =
				    add_tlb_mapping(boot_info_block_array[core].tlb_entries,
						    ei->ro_min_vaddr,
						    ei->ro_max_vaddr, 28,
						    tlb_flags,
						    &phy_mem_list_head);
				dprintf("RO tlb_index = %d\n\n", tlb_index);
				if (tlb_index < 0) {
					printf("ERROR CREATING TLB MAPPING (read only)\n");
					return (-1);
				}
			} else {
				/* We may need to copy more than one TLB entry.
				 * (older bootloaders only allowed 1 TLB entry
				 * per segment)
				 */
				for (i = prev_tlb_index; i <= tlb_index; i++)
					copy_tlb_entry(boot_info_block_array[core].tlb_entries,
						       &boot_info_block_array[coremask_iter_get_first_core()].tlb_entries[i]);
			}
		}
		prev_tlb_index = tlb_index + 1;

		/* Enable XI bit usage if supported.  Mark data, cvmx_shared,
		 * stack, heap as not executable
		 */
		if (!octeon_is_model(OCTEON_CN3XXX))
			tlb_flags.xi = 0x1;

		if (ei->shared_segment.mem_addr < 0x10000000 ||
		    (ei->shared_segment.mem_addr +
		     ei->shared_segment.mem_size - 1) > 0x1fffffff) {
			config_flags |=
			    CVMX_BOOTINFO_CFG_FLAG_OVERSIZE_TLB_MAPPING;
			dprintf("Shared segment too big: start addr: 0x%lx, end: 0x%lx\n",
			        ei->shared_segment.mem_addr,
			        (ei->shared_segment.mem_addr +
			         ei->shared_segment.mem_size - 1));
		}

		if (ei->shared_segment.mem_size) {
			for (core = coremask_iter_init(core_mask); core >= 0;
			     core = coremask_iter_next()) {
				tlb_flags.seg_type = SHARED_TLB_SEGMENT;
				if (coremask_iter_get_first_core() == core) {
					tlb_index =
					    add_tlb_mapping(boot_info_block_array[core].tlb_entries,
							    ei->shared_segment.mem_addr,
							    ei->shared_segment.mem_addr +
							    ei->shared_segment.mem_size - 1,
							    28,
							    tlb_flags,
							    &phy_mem_list_head);
					dprintf("Shared tlb_index = %d\n\n", tlb_index);
					if (tlb_index < 0) {
						printf("ERROR CREATING TLB MAPPING (CVMX_SHARED)\n");
						return (-1);
					}
				} else {
					for (i = prev_tlb_index; i <= tlb_index;
					     i++)
						copy_tlb_entry(boot_info_block_array[core].tlb_entries,
							       &boot_info_block_array[coremask_iter_get_first_core()].tlb_entries[i]);
				}
			}
			prev_tlb_index = tlb_index + 1;
		}

		/* Here we 'mark' the stack, heap, and data segment for the
		 * application.  This allows us to use the SYNCWS instruction.
		 *  This is new is Octeon pass2, see the HW manual for more
		 * information.
		 */
#if defined(__U_BOOT__)
		if (!getenv("no_mark_private_data"))
#else
		if (!global_linux_boot_info->labi.no_mark_private_data)
#endif
			tlb_flags.cca = 0x7;
		else
			tlb_flags.cca = 0x0;

		if (ei->rw_min_vaddr < 0x10000000
		    || ei->rw_max_vaddr > 0x1fffffff) {
			config_flags |=
			    CVMX_BOOTINFO_CFG_FLAG_OVERSIZE_TLB_MAPPING;
			dprintf("rw segment too big\n");
		}

		for (core = coremask_iter_init(core_mask); core >= 0;
		     core = coremask_iter_next()) {

			dprintf("core: %d\n", core);
			tlb_flags.seg_type = RW_TLB_SEGMENT;
			tlb_index =
			    add_tlb_mapping(boot_info_block_array[core].tlb_entries,
					    ei->rw_min_vaddr,
					    ei->rw_max_vaddr, 28, tlb_flags,
					    &phy_mem_list_head);
			dprintf("RW tlb_index = %d\n\n", tlb_index);
			if (tlb_index < 0) {
				printf("ERROR CREATING TLB MAPPING (rw)\n");
				return (-1);
			}
		}

		/* Need to allow stack to be anywhere now that we allow mappings
		 * outside of the boot bus window
		 */

		/* Put stack/heap base above cvmx shared segment end, and align
		 * so that we won't overlap the shared mapping
		 */
		stack_heap_base_address =
		    ei->shared_segment.mem_addr + ei->shared_segment.mem_size -
		    1;
		stack_heap_base_address = (stack_heap_base_address +
			(1 << 25)) & ~((1 << 25) - 1);	/* 32 MByte alignment */

		if (stack_heap_base_address < 0x10000000
		    || stack_heap_base_address + stack_size + heap_size >
		    0x1fffffff) {
			config_flags |=
			    CVMX_BOOTINFO_CFG_FLAG_OVERSIZE_TLB_MAPPING;
			dprintf("stack/heap segment too big\n");
		}
		for (core = coremask_iter_init(core_mask);
		     core >= 0;
		     core = coremask_iter_next()) {
			/* Determine base address of stack/heap mapping based
			 * on size, just use fake addresses for calculations
			 */
			uint32_t alignment_exp;
			uint32_t map_size =
			    get_tlb_mapping_size(stack_heap_base_address,
						 stack_heap_base_address +
						 stack_size + heap_size,
						 1, &alignment_exp);
			if (!map_size) {
				printf("ERROR CREATING TLB MAPPING for stack/heap\n");
				return (-1);
			}

			/* Adjust stack/heap to start at bottom of mapping, as
			 * this makes half-mappings possible
			 */
			stack_heap_base_address =
			    stack_heap_base_address & ALIGN_MASK(alignment_exp + 1);

			/* We now have the mapping size, so now we can compute
			 * the base address for the mapping
			 */
			dprintf("core: %d\n", core);
			tlb_flags.seg_type = STACK_HEAP_TLB_SEGMENT;
			tlb_index =
			    add_tlb_mapping(boot_info_block_array[core].tlb_entries,
					    stack_heap_base_address,
					    stack_heap_base_address +
					    stack_size + heap_size - 1, 28,
					    tlb_flags, &phy_mem_list_head);
			dprintf("Stack/heap tlb_index = %d\n\n", tlb_index);
			if (tlb_index < 0) {
				printf("ERROR CREATING TLB MAPPING for stack/heap\n");
				return (-1);
			}
		}
		/* Set TLB flags back to default value */
		tlb_flags.cca = 0x0;
		tlb_flags.xi = 0x0;
		tlb_flags.ri = 0x0;
		tlb_flags.seg_type = 0;

		dprintf("Done with TLB mappings, now copying program data\n");

#ifdef DEBUG_TLB
		for (core = coremask_iter_init(core_mask); core >= 0;
		     core = coremask_iter_next()) {
			printf("\nCore %d:\n", core);
			print_tlb_array(boot_info_block_array[core].tlb_entries,
					32);
			printf("----------------------------------\n");
		}
#endif

		/* copy memory segments.... */
		{
			int segment, core;
			coremask_iter_init(core_mask);

			/* Make one copy of the readonly segments, shared by
			 * all cores
			 */
			for (segment = 0; segment < MAX_ELF_SEGMENTS; segment++) {
				if (ei->ro_segments[segment].file_size) {
					mem_copy_tlb(boot_info_block_array[coremask_iter_get_first_core()].tlb_entries,
						     ei->ro_segments[segment].mem_addr,
#if defined(__U_BOOT__)
						     MAKE_XKPHYS(ei->ro_segments[segment].file_offset +
								 elf_addr),
#else
						     ei->ro_segments[segment].file_offset + (uint64_t) elf_addr,
#endif
						     ei->ro_segments[segment].file_size);
					mem_set_tlb(boot_info_block_array[coremask_iter_get_first_core()].tlb_entries,
						    ei->ro_segments[segment].mem_addr +
						    ei->ro_segments[segment].file_size,
						    0x0,
						    ei->ro_segments[segment].mem_size -
						    ei->ro_segments[segment].file_size);
				}

			}
			/* Copy the shared segment */
			if (ei->shared_segment.mem_size) {
				mem_copy_tlb(boot_info_block_array[coremask_iter_get_first_core()].tlb_entries,
					     ei->shared_segment.mem_addr,
#if defined(__U_BOOT__)
					     MAKE_XKPHYS(ei->shared_segment.file_offset +
							 elf_addr),
#else
					     ei->shared_segment.file_offset + (uint64_t) elf_addr,
#endif
					     ei->shared_segment.file_size);
				mem_set_tlb(boot_info_block_array[coremask_iter_get_first_core()].tlb_entries,
					    ei->shared_segment.mem_addr +
					    ei->shared_segment.file_size,
					    0x0,
					    ei->shared_segment.mem_size -
					    ei->shared_segment.file_size);
			}

			/* Copy the writable segments for each core. */
			for (core = coremask_iter_init(core_mask); core >= 0;
			     core = coremask_iter_next()) {
				for (segment = 0; segment < MAX_ELF_SEGMENTS;
				     segment++) {
					if (ei->rw_segments[segment].file_size) {
						mem_copy_tlb(boot_info_block_array[core].tlb_entries,
							     ei->rw_segments[segment].mem_addr,
#if defined(__U_BOOT__)
							     MAKE_XKPHYS(ei->rw_segments[segment].file_offset +
									 elf_addr),
#else
							     ei->rw_segments[segment].file_offset + (uint64_t) elf_addr,
#endif
							     ei->rw_segments[segment].file_size);
						mem_set_tlb(boot_info_block_array[core].tlb_entries,
							    ei->rw_segments[segment].mem_addr +
							    ei->rw_segments[segment].file_size,
							    0x0,
							    ei->rw_segments[segment].mem_size -
							    ei->rw_segments[segment].file_size);
					}
				}
			}
		}
	}

	if (config_flags & CVMX_BOOTINFO_CFG_FLAG_OVERSIZE_TLB_MAPPING)
		printf("NOTICE: oversize TLB mappings for application used, "
		       "application can't use 1-1 TLB mappings.\n");

	if (octeon_setup_boot_desc_block(core_mask, argc, argv, ei->eh.entry,
					 stack_size, heap_size, boot_flags,
					 stack_heap_base_address, 0,
					 config_flags, 0, app_index)) {
		printf("ERROR setting up boot descriptor block, core_mask: 0x%x\n",
		       core_mask);
		return (-1);
	}

#if defined(__U_BOOT__)
	if (octeon_setup_boot_vector((uint32_t) start_app, core_mask)) {
		printf("ERROR setting boot vectors, core_mask: 0x%x\n",
		       core_mask);
		return (-1);
	}

	/* Add coremask to global mask of cores that have been set up and are
	 * runable
	 */
	coremask_to_run |= core_mask;

	printf("Bootloader: Done loading app on coremask: 0x%x\n", core_mask);

	/* Start other cores, but only if core zero is in mask */
	if ((core_mask & 1) || forceboot) {
		octeon_bootloader_shutdown();
		start_cores(coremask_to_run);	/* does not return */
	}
#endif

	return (0);
}

/**
 * Idle cores come here and will spin on a wait loop conserving power until
 * NMI is received.
 */
void idle_core_start_app(void)
{
	uint64_t addr;
	cvmx_bootmem_named_block_desc_t *block_desc;

	addr =
	    cvmx_bootmem_phy_named_block_find(IDLE_CORE_BLOCK_NAME,
					      CVMX_BOOTMEM_FLAG_NO_LOCKING);

	if (!addr) {
		printf("Named block(%s) is not created\n",
		       IDLE_CORE_BLOCK_NAME);
		/* loop here, should not happen */
		while (1) ;
	}

	block_desc = (cvmx_bootmem_named_block_desc_t *) cvmx_phys_to_ptr(addr);
	asm volatile ("       .set push                \n"
		      "       .set mips64              \n"
		      "       .set noreorder           \n"
		      "       move	$2, %[addr]    \n"
		      "       jr	$2             \n"
		      "       nop                      \n"
		      "       .set pop "
		      :: [addr] "r"(block_desc->base_addr)
		      : "$2");
	while (1) ;
}
