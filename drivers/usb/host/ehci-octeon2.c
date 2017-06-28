/***********************license start************************************
 * Copyright (c) 2010 Cavium Inc. (support@cavium.com). All rights
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
/*define DEBUG*/

#include <common.h>
#include <usb.h>
#include "ehci.h"
#include "ehci-core.h"
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-access.h>
#include <asm/arch/cvmx-asm.h>
#include <asm/arch/cvmx-uahcx-defs.h>

extern void octeon2_usb_clock_start(void);
extern void octeon2_usb_clock_stop(void);
extern int octeon_bist_6XXX_usb(void);

u32 ehci_readl(volatile u32 *addr32)
{
	u64 addr64;
	u32 val;
	addr64  = 0x80016F0000000000ull + (u32)addr32;

	asm volatile ("lw  %[v], 0(%[c])" : [v] "=r" (val) : [c] "r" (addr64));
	debug("%s: read 0x%08x from address %p (0x%llx)\n",
	      __func__, val, addr32, addr64);
	return val;
}

void ehci_writel(volatile u32 *addr32, u32 val)
{
	u64 addr64 = 0x80016F0000000000ull + (u32)addr32;
	u32 val2 = 0;
	asm volatile (	"	sw 	%[vi], 0(%[c])		\n"
			"	lw	%[vo], 0(%[c])		\n"
			: [vo] "=r" (val2) : [vi] "r" (val), [c] "r" (addr64)
	);
	debug("%s: wrote 0x%08x to address %p (0x%llx), read back 0x%08x\n",
	      __func__, val, addr32, addr64, val2);
}

int ehci_hcd_init(void)
{
	union cvmx_uctlx_ehci_ctl		ehci_ctl;

	octeon2_usb_clock_start();

	/* ehci byte swap */
	ehci_ctl.u64 = cvmx_read_csr(CVMX_UCTLX_EHCI_CTL(0));
	/* For 64-bit addressing, we do it here rather than EHCI */
	ehci_ctl.s.ehci_64b_addr_en = 1;    /* Add 64-bit addressing in EHCI */
	ehci_ctl.s.l2c_addr_msb = 0;
	ehci_ctl.s.l2c_buff_emod = 1; /* Byte swapped. */
	ehci_ctl.s.l2c_desc_emod = 1; /* Byte swapped. */
	cvmx_write_csr(CVMX_UCTLX_EHCI_CTL(0), ehci_ctl.u64);

	hccr = NULL;	/* We use the correct address in the access funcs */
	hcor = (struct ehci_hcor *)((u32) hccr +
			HC_LENGTH(ehci_readl(&hccr->cr_capbase)));
	if (octeon_bist_6XXX_usb()) {
		puts("Error: USB failed BIST test!\n");
	}

	return 0;
}

int ehci_hcd_stop(void)
{
	octeon2_usb_clock_stop();

	return 0;
}
