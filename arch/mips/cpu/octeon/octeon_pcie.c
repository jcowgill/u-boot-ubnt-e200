/***********************license start************************************
 * Copyright (c) 2005-2011 Cavium Inc. (support@cavium.com). All rights
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
#include <nios2-io.h>
#include <pci.h>
#include <asm/addrspace.h>

#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_hal.h>
#include <asm/arch/octeon_pci.h>
#include <asm/arch/cvmx-pcie.h>
#define OCTEON_PCIE_MAX_PORTS	2

DECLARE_GLOBAL_DATA_PTR;

typedef struct {
	int	pcie_port;
} octeon_pci_priv_data_t;

/* Initialize IO and Memory base address for PCIE */
#define PCIE_PNP_IO0_BASE    0x00001000
#define PCIE_PNP_IO0_SIZE    0x000ff000
#define PCIE_PNP_IO1_BASE    0x00100000
#define PCIE_PNP_IO1_SIZE    0xffffffff
#define PCIE_PNP_MEM0_BASE   OCTEON_PCI_SLOT0_BAR_ADDR
#define PCIE_PNP_MEM0_SIZE   OCTEON_PCI_SLOT0_BAR_SIZE
#define PCIE_PNP_MEM1_BASE   OCTEON_PCI_SLOT1_BAR_ADDR
#define PCIE_PNP_MEM1_SIZE   OCTEON_PCI_SLOT1_BAR_SIZE

static struct pci_config_table pci_board_config_table[] = {
	/* vendor, device, class */
	/* bus, dev, func */
	{}
};

typedef struct {
	u32 mem_base;
	u32 mem_size;
	u32 io_base;
	u32 io_size;
} octeon_pcie_region_info_t;


static struct pci_controller hose_pcie[OCTEON_PCIE_MAX_PORTS];
static octeon_pci_priv_data_t oct_pcie_data[OCTEON_PCIE_MAX_PORTS];

static const octeon_pcie_region_info_t octeon_pcie_region_info[] = {
	{
		PCIE_PNP_MEM0_BASE, PCIE_PNP_MEM0_SIZE,
		PCIE_PNP_IO0_BASE, PCIE_PNP_IO0_SIZE,
	},
	{
		PCIE_PNP_MEM1_BASE, PCIE_PNP_MEM1_SIZE,
		PCIE_PNP_IO1_BASE, PCIE_PNP_IO1_SIZE,
	},
	{
		0, 0, 0, 0
	}
};

static const int num_pcie_ports = OCTEON_PCIE_MAX_PORTS;

#ifdef CONFIG_PCI_PNP
void pciauto_config_init(struct pci_controller *hose);
#endif



int cvmx_pcie_rc_initialize(int pcie_port);
uint8_t cvmx_pcie_config_read8(int pcie_port, int bus, int dev, int fn,
			       int reg);
uint16_t cvmx_pcie_config_read16(int pcie_port, int bus, int dev, int fn,
				 int reg);
uint32_t cvmx_pcie_config_read32(int pcie_port, int bus, int dev, int fn,
				 int reg);
void cvmx_pcie_config_write8(int pcie_port, int bus, int dev, int fn, int reg,
			     uint8_t val);
void cvmx_pcie_config_write16(int pcie_port, int bus, int dev, int fn, int reg,
			      uint16_t val);
void cvmx_pcie_config_write32(int pcie_port, int bus, int dev, int fn, int reg,
			      uint32_t val);

static inline phys_addr_t octeon_gen1_phys_to_dma(phys_addr_t paddr)
{
	if (paddr >= 0x410000000ull && paddr < 0x420000000ull) {
		paddr -= 0x400000000ull;
	}
	return paddr;
}

static inline int octeon_get_pcie_port(struct pci_controller *hose)
{
	octeon_pci_priv_data_t *priv = (octeon_pci_priv_data_t *)hose->priv_data;
	return priv->pcie_port;
}

/*
 *	Access Octeon PCIe Config / IACK / Special Space
 */

static int octeon_pcie_write_config_byte(struct pci_controller *hose,
					 pci_dev_t dev, int reg, u8 val)
{
	cvmx_pcie_config_write8(octeon_get_pcie_port(hose),
				PCI_BUS(dev) - hose->first_busno + 1,
				PCI_DEV(dev), PCI_FUNC(dev), reg, val);
	return 0;

}

static int octeon_pcie_write_config_word(struct pci_controller *hose,
					 pci_dev_t dev, int reg, u16 val)
{
	cvmx_pcie_config_write16(octeon_get_pcie_port(hose),
				 PCI_BUS(dev) - hose->first_busno + 1,
				 PCI_DEV(dev), PCI_FUNC(dev), reg, val);
	return 0;

}

static int octeon_pcie_write_config_dword(struct pci_controller *hose,
					  pci_dev_t dev, int reg, u32 val)
{
	cvmx_pcie_config_write32(octeon_get_pcie_port(hose),
				 PCI_BUS(dev) - hose->first_busno + 1,
				 PCI_DEV(dev), PCI_FUNC(dev), reg, val);
	return 0;

}

static int octeon_pcie_read_config_byte(struct pci_controller *hose,
					pci_dev_t dev, int reg, u8 * val)
{
	if (((PCI_BUS(dev) - hose->first_busno) == 0) && (PCI_DEV(dev) != 0)) {
		*val = 0xff;
	} else {
		*val = (u8) cvmx_pcie_config_read8(octeon_get_pcie_port(hose),
						   PCI_BUS(dev) -
						   hose->first_busno + 1,
						   PCI_DEV(dev),
						   PCI_FUNC(dev),
						   reg);
	}
	return 0;
}

static int octeon_pcie_read_config_word(struct pci_controller *hose,
					pci_dev_t dev, int reg, u16 * val)
{
	if (((PCI_BUS(dev) - hose->first_busno) == 0) && (PCI_DEV(dev) != 0)) {
		*val = 0xffff;
	} else {
		*val = (u16) cvmx_pcie_config_read16(octeon_get_pcie_port(hose),
						     PCI_BUS(dev) -
						     hose->first_busno + 1,
						     PCI_DEV(dev),
						     PCI_FUNC(dev),
						     reg);
	}
	return 0;
}

static int octeon_pcie_read_config_dword(struct pci_controller *hose,
					 pci_dev_t dev, int reg, u32 * val)
{
	if (((PCI_BUS(dev) - hose->first_busno) == 0) && (PCI_DEV(dev) != 0)) {
		*val = 0xffffffff;
	} else {
		*val = (u32) cvmx_pcie_config_read32(octeon_get_pcie_port(hose),
						     PCI_BUS(dev) -
						     hose->first_busno + 1,
						     PCI_DEV(dev),
						     PCI_FUNC(dev),
						     reg);
	}
	return 0;
}

/*
 *	Initialize OCTEON PCIE
 */

/*
 *	Initialize Module
 */

/* Only needed for NPI register access */
#define SWAP_ADDR32_LE_TO_BE(x)         ((x)^ 0x4ull)

void init_octeon_pcie(void)
{
	int rc = 0;
	struct pci_controller *hose;

	/* Multi-Hose */
	int first_busno;
	int i;

	debug("Starting PCIE\n");
	first_busno = OCTEON_FIRST_PCIE_BUSNO;
	memset(&hose_pcie[0], 0, sizeof(hose_pcie[0]) * num_pcie_ports);

	for (i = 0; i < num_pcie_ports; i++) {

		rc = cvmx_pcie_rc_initialize(i);

		if (rc != 0)
			continue;

		udelay(100000);		/* Some devices need extra time */

		hose = &hose_pcie[i];
		hose->priv_data = (void *)&oct_pcie_data[i];
		oct_pcie_data[i].pcie_port = i;

		hose->config_table = pci_board_config_table;

		hose->first_busno = first_busno;
		hose->last_busno = 0xff;


		/* PCI I/O space (Sub-DID == 2) */
		pci_set_region(hose->regions + 0,
			       octeon_pcie_region_info[i].io_base,
			       octeon_pcie_region_info[i].io_base,
			       octeon_pcie_region_info[i].io_size,
			       PCI_REGION_IO);
		/* PCI memory space (Sub-DID == 3) */
		pci_set_region(hose->regions + 1,
			       octeon_pcie_region_info[i].mem_base,
			       octeon_pcie_region_info[i].mem_base,
			       octeon_pcie_region_info[i].mem_size,
			       PCI_REGION_MEM);

		hose->region_count = 2;

		pci_set_ops(hose,
			    octeon_pcie_read_config_byte,
			    octeon_pcie_read_config_word,
			    octeon_pcie_read_config_dword,
			    octeon_pcie_write_config_byte,
			    octeon_pcie_write_config_word,
			    octeon_pcie_write_config_dword);

		pci_register_hose(hose);

		hose->last_busno = pci_hose_scan(hose);

		debug("PCIe: port=%d, first_bus=%d, last_bus=%d,\n\t"
		      "mem_base=0x%x, mem_size=0x%x, io_base=0x%x, io_size=0x%x\n",
		      octeon_get_pcie_port(hose),
		      hose->first_busno, hose->last_busno,
		      octeon_pcie_region_info[i].mem_base,
		      octeon_pcie_region_info[i].mem_size,
		      octeon_pcie_region_info[i].io_base,
		      octeon_pcie_region_info[i].io_size);

		first_busno = hose->last_busno + 1;
		if (OCTEON_IS_MODEL(OCTEON_CN52XX)
		    || OCTEON_IS_MODEL(OCTEON_CN56XX)) {
#if 0 /* Does not work! */
			uint64_t bar1_reg_addr;

			/* Setup BAR1 to map bus address 0x0 to the base of
			 * u-boot's TLB mapping.  This allows us to have u-boot
			 * located anywhere in memory (including above 32 bit
			 * addressable space), and still have 32 bit PCI devices
			 * have access to memory that is statically allocated or
			 * malloced u-boot, both of which are TLB mapped.
			 */
			cvmx_write_csr(CVMX_PESCX_P2N_BAR1_START(i), 0);

			/* Disable bar0/bar2, as we are not using them here */
			cvmx_write_csr(CVMX_PESCX_P2N_BAR0_START(i), -1);
			cvmx_write_csr(CVMX_PESCX_P2N_BAR2_START(i), -1);

			/* Select 64 MByte mapping size for bar 1 on both ports */
			cvmx_npei_ctl_status2_t ctls2;
			ctls2.u64 = cvmx_read_csr(CVMX_NPEI_CTL_STATUS2);
			ctls2.s.c0_b1_s = 1;
			ctls2.s.c1_b1_s = 1;
			cvmx_write_csr(CVMX_NPEI_CTL_STATUS2, ctls2.u64);

			/* Configure the regions in bar 1 to map to the DRAM used
			 * by u-boot.
			 */
			int base_entry = 16 * i;
			int j;
			cvmx_npei_bar1_indexx_t npei_bar1_indexx;
			npei_bar1_indexx.u32 = 0;
			npei_bar1_indexx.s.addr_v = 1;
			npei_bar1_indexx.s.end_swp = 1;
			npei_bar1_indexx.s.ca = 1;
			/* Round down to 4MByte boundary to meet BAR mapping
			 * requirements
			 */
			uint64_t bar_base = gd->bd->bi_uboot_ram_addr & ~0x3fffffull;

			debug("pcie: port %d, setting BAR base to 0x%llx\n",
			      i, bar_base);

			for (j = 0; j < 16; j++) {
				npei_bar1_indexx.s.addr_idx =
					(bar_base + 4 * 1024 * 1024 * j) >> 22;
				bar1_reg_addr =
					SWAP_ADDR32_LE_TO_BE(CVMX_PEXP_NPEI_BAR1_INDEXX(base_entry + j));
				debug("BAR1 index %d (addr: 0x%llx): 0x%llx (0x%x)\n",
				      base_entry + j, bar1_reg_addr,
				      (bar_base + 4 * 1024 * 1024 * j),
					npei_bar1_indexx.s.addr_idx);
				cvmx_write64_uint32(bar1_reg_addr,
						    npei_bar1_indexx.u32);
			}
#endif
#if CONFIG_OCTEON_PCI_ENABLE_32BIT_MAPPING
		} else if (OCTEON_IS_MODEL(OCTEON_CN6XXX)
			   || OCTEON_IS_MODEL(OCTEON_CNF7XXX)) {
			/* Setup BAR1 to map bus address 0x0 to the base of
			 * u-boot's TLB mapping.  This allows us to have u-boot
			 * located anywhere in memory (including above 32 bit
			 * addressable space) and still have 32 bit PCI devices
			 * have access to memory that is statically allocated
			 * or malloced by u-boot, both of which are TLB mapped.
			*/
			cvmx_write_csr(CVMX_PEMX_P2N_BAR1_START(i), 0);

			/* Disable bar0/bar2, as we are not using them here */
			cvmx_write_csr(CVMX_PEMX_P2N_BAR0_START(i), -1);
			cvmx_write_csr(CVMX_PEMX_P2N_BAR2_START(i), -1);

			/* Select 64 MByte mapping size for bar 1 on
			 * all ports
			 */
			cvmx_pemx_bar_ctl_t bar_ctl;
			bar_ctl.u64 = cvmx_read_csr(CVMX_PEMX_BAR_CTL(i));
			bar_ctl.s.bar1_siz = 1;	/* 64MB */
			bar_ctl.s.bar2_enb = 0;
			cvmx_write_csr(CVMX_PEMX_BAR_CTL(i), bar_ctl.u64);
			/* Configure the regions in bar 1 to map to the
			 * DRAM used by u-boot.
			*/
			/* Round down to 4MByte boundary to meet BAR mapping
			 * requirements
			 */
			uint64_t bar_base = gd->bd->bi_uboot_ram_addr & ~0x3fffffull;

			debug("pcie: port %d, setting BAR base to 0x%llx\n",
			      i, bar_base);

			int j;
			cvmx_pemx_bar1_indexx_t pemx_bar1_indexx;
			pemx_bar1_indexx.u64 = 0;
			pemx_bar1_indexx.s.addr_v = 1;
			pemx_bar1_indexx.s.end_swp = 1;
			pemx_bar1_indexx.s.ca = 1;

			for (j = 0; j < 16; j++) {
				pemx_bar1_indexx.s.addr_idx =
					(bar_base + 4 * 1024 * 1024 * j) >> 22;
				cvmx_write64_uint64(CVMX_PEMX_BAR1_INDEXX(j, i),
						    pemx_bar1_indexx.u64);
			}
#endif /* CONFIG_OCTEON_PCI_ENABLE_32BIT_MAPPING */
		}
	}
}

int octeon_pci_port_num(uint32_t addr)
{
	return (addr >= PCIE_PNP_IO1_BASE);
}

/* Do physical/virtual to bus mapping.  This mapping assumes that all 'physical'
 * addresses come from u-boot data or heap.
 */
uint32_t octeon_pci_phys_to_bus(uint32_t mem_addr)
{
	uint64_t phys_addr;
	uint64_t bus_addr;
	uint64_t bar_base = gd->bd->bi_uboot_ram_addr & ~0x3fffffull;
	phys_addr = cvmx_ptr_to_phys((void *)mem_addr);

	/* We only have a limited amount of DRAM mapped with BAR1, so this
	 * translation is only valid for that range.
	 */
	if (phys_addr < bar_base
	    || phys_addr > gd->bd->bi_uboot_ram_addr + gd->bd->bi_uboot_ram_used_size) {
		printf("ERROR: Invalid address 0x%x (0x%llx physical) in "
		       "octeon_pci_phys_to_bus\n",
		       mem_addr, phys_addr);
		return (0xdeadbeef);
	}

	bus_addr = phys_addr - bar_base;

	debug("mem: 0x%x, phys: 0x%llx, ub-phys: 0x%llx, bus64: 0x%llx, "
	      "bus32: 0x%x\n",
	      mem_addr, phys_addr, gd->bd->bi_uboot_ram_addr, bus_addr,
	      (uint32_t) bus_addr);

	return ((uint32_t) bus_addr);
}

pci_addr_t pci_hose_phys_to_bus(struct pci_controller *hose,
				phys_addr_t phys_addr,
     				unsigned long flags)
{
	uint64_t bar_base = gd->bd->bi_uboot_ram_addr & ~0x3fffffull;
	uint64_t bus_addr;

	/* Handle OCTEON/OCTEON+ hole remapping */
	/* Adjust the address for memory used for the 2nd 256MB moved by the
	 * boot bus hole.
	 */
#if 1
	if (!octeon_has_feature(OCTEON_FEATURE_PCIE) &&
	    (OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN5XXX))) {
		debug("%s: Phys address 0x%llx in hole, changing to ",
		      __func__, phys_addr);
		phys_addr = octeon_gen1_phys_to_dma(phys_addr);
		debug("0x%llx, bar_base: 0x%llx\n", phys_addr, bar_base);
	}
#endif
	/* Not all Octeon devices can perform this mapping. */
#ifdef CONFIG_OCTEON_PCI_ENABLE_32BIT_MAPPING
	if (!OCTEON_IS_MODEL(OCTEON_CN6XXX)
	    && !OCTEON_IS_MODEL(OCTEON_CNF7XXX)) {
		debug("%s: returning physical address 0x%llx\n",
		      __func__, phys_addr);
		return phys_addr;
	}
#else
	return phys_addr;
#endif
	/* We only have a limited amount of DRAM mapped with BAR1, so this
	 * translation is only valid for that range.
	 */
	if ((phys_addr >= bar_base) &&
	    (phys_addr <= gd->bd->bi_uboot_ram_addr + gd->bd->bi_uboot_ram_used_size)) {
		bus_addr = phys_addr - bar_base;
	} else if (phys_addr < 0x400000ull) {
		printf("%s: ERROR: Invalid physical address 0x%llx in PCI mapped area\n",
		       __func__, phys_addr);
		return (0xdeadbeef);
	}

	debug("phys: 0x%llx, ub-phys: 0x%llx, bus64: 0x%llx, "
	      "bus32: 0x%x\n",
	      phys_addr, gd->bd->bi_uboot_ram_addr, bus_addr,
	      (uint32_t) bus_addr);

	return ((pci_addr_t)bus_addr);
}

/* We expect all bus addresses to correspond to the TLB mapped u-boot
* data and heap area.  If it is outside of this region, it is an error.
*/
uint32_t octeon_pci_bus_to_phys(uint32_t bus_addr)
{
	/* Extra offset to acount for the 4 MByte PCI BAR alignment
	 * restriciton - the u-boot base may not be aligned properly
	 */
	uint32_t align_offset =
	    (gd->bd->bi_uboot_ram_addr & 0x3fffff) ? 0x400000 -
	    (gd->bd->bi_uboot_ram_addr & 0x3fffff) : 0;
	if (bus_addr > gd->bd->bi_uboot_ram_used_size + align_offset) {
		printf("ERROR: Invalid address 0x%x octeon_pci_bus_to_phys\n",
		       bus_addr);
		return (0xdeadbeef);
	}

	uint64_t phys_addr =
	    bus_addr + gd->bd->bi_uboot_ram_addr - align_offset;

	return ((uint32_t) cvmx_phys_to_ptr(phys_addr));

}

extern int __pci_hose_bus_to_phys(struct pci_controller *hose,
				  pci_addr_t bus_addr,
				  unsigned long flags,
				  unsigned long skip_mask,
				  phys_addr_t *pa);

phys_addr_t pci_hose_bus_to_phys(struct pci_controller *hose,
				 pci_addr_t bus_addr,
				 unsigned long flags)
{
	phys_addr_t phys_addr = 0;
	int ret;

	if (!hose) {
		puts("pci_hose_bus_to_phys: invalid hose\n");
		return phys_addr;
	}
	/* Note that bus_addr is really a virtual address and not a physical
	 * address!
	 */

	/* if PCI_REGION_MEM is set we do a two pass search with preference
	 * on matches that don't have PCI_REGION_SYS_MEMORY set
	 */
	if ((flags & PCI_REGION_MEM) == PCI_REGION_MEM) {
		ret = __pci_hose_bus_to_phys(hose, bus_addr,
				flags, PCI_REGION_SYS_MEMORY, &phys_addr);

		if (!ret) {
			debug("%s: Flags: 0x%lx, bus addr: 0x%llx, "
			      "phys addr: 0x%llx (pass 1)\n",
			      __func__, flags, bus_addr, phys_addr);
			return phys_addr;
		}
	}

	ret = __pci_hose_bus_to_phys(hose, bus_addr, flags, 0, &phys_addr);
	if (!ret) {
		debug("%s: Flags: 0x%lx, bus addr: 0x%llx, "
		      "phys addr: 0x%llx (pass 1)\n",
		      __func__, flags, bus_addr, phys_addr);
		return phys_addr;
	}

	phys_addr = (phys_addr_t)
		cvmx_phys_to_ptr(octeon_pci_bus_to_phys((uint32_t)bus_addr));
	debug("%s: Flags: 0x%lx, bus addr: 0x%llx, "
	      "phys addr: 0x%llx (bus to virt ptr)\n",
	      __func__, flags, bus_addr, phys_addr);
	return phys_addr;
}


/* We don't skip anything on Octeon */
int pci_skip_dev(struct pci_controller *hose, pci_dev_t dev)
{
	return 0;
}

int pci_print_dev(struct pci_controller *hose, pci_dev_t dev)
{
	return 1;
}
