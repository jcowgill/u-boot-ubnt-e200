/***********************license start************************************
 * Copyright (c) 2011 Cavium, Inc. (support@cavium.com). All rights reserved.
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
 * $Id: octeon_exec.c 69988 2012-02-15 18:55:21Z cchavva $
 *
 */
/*
 * Platform specific override functions used for the go and various exec
 * commands
 */
#undef DEBUG
#include <common.h>
#include <config.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-access-native.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>

extern void asm_launch_linux_entry_point(uint64_t arg0, uint64_t arg1,
					 uint64_t arg2, uint64_t arg3,
					 void (*entry) (int, char *const[]));

unsigned long do_go_exec(ulong (*entry) (int, char *const[]), int argc,
			 char *const argv[])
{
	uint64_t arg0, arg1, arg4;
	uint64_t asm_addr;

	arg0 = argc;
	/* Cast away const.... */
	arg1 = MAKE_XKPHYS(uboot_tlb_ptr_to_phys((void *)&argv[0]));
	/* want sign extension... */
	arg4 = (int32_t) MAKE_KSEG0((uint32_t) entry);
	asm_addr =
	    MAKE_XKPHYS(uboot_tlb_ptr_to_phys(&asm_launch_linux_entry_point));

	octeon_restore_std_mips_config();

	/* Set up the environment in RAM to pass off to next application
	 * or bootloader.  Required for NAND boot.
	 */
	octeon_setup_ram_env();
	/* Set up TLB registers to clear desired entry.  The actual tlbwi
	 * instruction is done in ASM when running from unmapped DRAM
	 * Don't use write_one_tlb64, as this does the actual TLB update,
	 * which will remove the mapping we are currently running from.
	 */
	write_64bit_c0_entrylo0(0);
	write_c0_pagemask(0);
	write_64bit_c0_entrylo1(0);
	write_64bit_c0_entryhi(0xFFFFFFFF91000000ull);
	write_c0_index(get_num_tlb_entries() - 1);

	asm volatile ("	.set push		\n"
		      "	.set mips64		\n"
		      "	.set noreorder		\n"
		      "	move	$4, %[arg0]	\n"
		      "	move	$5, %[arg1]	\n"
		      " move	$2, $0		\n" /* Big endian */
		      "	j	%[addr]		\n"
		      "	move	$8, %[arg4]	\n"
		      "	.set pop		\n"
		      : :
		      [arg0] "r"(arg0),
		      [arg1] "r"(arg1),
		      [arg4] "r"(arg4),
		      [addr] "r"(asm_addr)
		      : "$2", "$4", "$5", "$8");
	printf("### Application terminated, rc not available\n");
	return entry(argc, argv);
}

/**
 * Override for do_bootelf_exec in order to handle platform specific 
 * settings.
 */
unsigned long do_bootelf_exec (ulong (*entry) (int, char *const[]),
			       int argc, char *const argv[])
{
	unsigned long ret;

	/* Restore to a sane MIPS configuration, i.e. disable 
	 * watchdog, USB, networking and other cleanup.
	 */
	octeon_restore_std_mips_config();
	/*
	 * pass address parameter as argv[0] (aka command name),
	 * and all remaining args
	 */
	ret = entry(argc, argv);

	return ret;
}
