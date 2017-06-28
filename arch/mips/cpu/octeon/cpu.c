/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, <wd@denx.de>
 * (C) Copyright 2010, 2011
 * Cavium Inc. Inc. <support@cavium.com>
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
#include <command.h>
#include <asm/mipsregs.h>

#include <asm/arch/octeon-boot-info.h>
#include <asm/arch/octeon_boot.h>

#ifdef OCTEON_MGMT_ENET
int octeon_mgmt_eth_initialize(bd_t *);
#endif
#if defined(OCTEON_RGMII_ENET) || defined(OCTEON_XAUI_ENET) || \
	defined(OCTEON_SGMII_ENET)
int octeon_eth_initialize(bd_t *);
#endif
#ifdef OCTEON_SPI4000_ENET
int octeon_spi4000_initialize(bd_t *);
#endif
#ifdef OCTEON_SPI_IXF18201_ENET
int octeon_spi_ixf18201_initialize(bd_t *);
#endif

#ifdef CONFIG_E1000
int e1000_initialize(bd_t *);
#endif
#ifdef CONFIG_CMD_IDE
extern void ide_set_reset(int reset);
#endif
#if !defined(DISABLE_NETWORKING) && !defined(OCTEON_NO_NETWORK) \
    && (defined(CONFIG_MII) || defined(CONFIG_CMD_MII)) \
    && defined(CONFIG_OCTEON_MDIO)
extern int octeon_mdio_initialize(void);
#endif

#if defined(__U_BOOT__)
int do_reset (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{

	CVMX_SYNC;
#ifdef CONFIG_CMD_IDE
	udelay (10000);
	ide_set_reset(1);
	udelay(20000);
	ide_set_reset(0);
	udelay(50000);
#endif
#ifdef CONFIG_HW_WATCHDOG
	extern void hw_watchdog_disable(void);
	hw_watchdog_disable();
#endif
	cvmx_write_csr (CVMX_CIU_SOFT_RST, 1ull);

	fprintf (stderr, "*** reset failed ***\n");
	return 0;
}

void flush_cache (unsigned long start_addr, unsigned long size)
{
	CVMX_SYNC;
	CVMX_ICACHE_INVALIDATE;
	CVMX_DCACHE_INVALIDATE;
}
#endif

void write_one_tlb (int index, u32 pagemask, u32 hi, u32 low0, u32 low1)
{
	write_c0_entrylo0 (low0);
	write_c0_pagemask (pagemask);
	write_c0_entrylo1 (low1);
	write_c0_entryhi (hi);
	write_c0_index (index);
	tlb_write_indexed ();
}

void write_one_tlb64 (u32 index, u32 pagemask, u64 hi, u64 low0, u64 low1)
{
	write_64bit_c0_entrylo0 (low0);
	write_c0_pagemask (pagemask);
	write_64bit_c0_entrylo1 (low1);
	write_64bit_c0_entryhi (hi);
	write_c0_index (index);
	tlb_write_indexed ();
}

int get_num_tlb_entries (void)
{
	int val;
	int num_entries = (read_c0_config1 () >> 25) & 0x3f;

	val = read_c0_config3 ();
	if (val & (1 << 31)) {
		val = read_c0_config4 () & 0xff;
		num_entries |= val << 6;
	}
	return num_entries + 1;
}

void flush_dcache_range(ulong start_addr, ulong stop)
{
	/* Don't need to do anything for OCTEON */
}

void invalidate_dcache_range(ulong start_addr, ulong stop)
{
	/* Don't need to do anything for OCTEON */
}

#ifdef CONFIG_CMD_NET
int cpu_eth_init(bd_t *bis)
{
	if (getenv("disable_networking"))
		return 0;
#if !defined(DISABLE_NETWORKING) && !defined(OCTEON_NO_NETWORK) \
    && (defined(CONFIG_MII) || defined(CONFIG_CMD_MII)) \
    && defined(CONFIG_OCTEON_MDIO)
	debug("Initializing MDIO bus\n");
	octeon_mdio_initialize();
#endif

#ifdef OCTEON_MGMT_ENET
	if (octeon_has_feature(OCTEON_FEATURE_MGMT_PORT))
		octeon_mgmt_eth_initialize(bis);
#endif
#if defined(OCTEON_RGMII_ENET) || defined(OCTEON_XAUI_ENET) \
		|| defined(OCTEON_SGMII_ENET)
	octeon_eth_initialize(bis);
#endif
#ifdef OCTEON_SPI4000_ENET
	if (!getenv("disable_spi"))
		octeon_spi4000_initialize(bis);
#endif
#ifdef OCTEON_SPI_IXF18201_ENET
	if ((!getenv("disable_spi_ixf18201") && !getenv("disable_spi"))
	    || getenv("enable_spi_ixf18201"))
		octeon_spi_ixf18201_initialize(bis);
#endif
#ifdef CONFIG_E1000
	e1000_initialize(bis);
#endif
	return 0;
}
#endif /* CONFIG_CMD_NET */