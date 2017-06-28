/*
 * (C) Copyright 2011 Cavium Inc.
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

/*
 * This file contains functions that tend to be board specific related to IDE
 * and compact flash.  Two modes are supported, both True IDE mode and
 * non-True IDE mode.
 *
 * All of the functions are defined as weak.  Rather than changing these
 * functions they should be re-implemented in the board specific files and
 * the linker will pick up those instead.
 */

#include <common.h>
#include <asm/global_data.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/lib_octeon_shared.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CFG_PRINT_MPR
extern void print_mpr(void);
#else
void print_mpr(void)
{
}
#endif

/**
 * Prints out board information during bootup.
 */
int display_board_info(void)
{
	printf("%s r1:%d, r2:%d, serial #: %s\n",
	       cvmx_board_type_to_string(gd->ogd.board_desc.board_type),
	       gd->ogd.board_desc.rev_major,
	       gd->ogd.board_desc.rev_minor, gd->ogd.board_desc.serial_str);
	print_mpr();

	printf("Core clock: %lld MHz",
	       divide_nint(cvmx_clock_get_rate(CVMX_CLOCK_CORE), 1000000));

	if (OCTEON_IS_MODEL(OCTEON_CN6XXX) || OCTEON_IS_MODEL(OCTEON_CNF7XXX))
		printf(", IO clock: %lld MHz",
		       divide_nint(cvmx_clock_get_rate(CVMX_CLOCK_SCLK),
				   1000000));

#if 1
	printf(", DDR clock: %d MHz (%d Mhz DDR)", gd->ogd.ddr_clock_mhz,
	       gd->ogd.ddr_clock_mhz * 2);
#else
	/* FIXME: This should be computed with hertz and proper rounding as
	 * follows.  The change would have to ripple through gd_t and
	 * eeprom_tuples.
	 */
	printf(", DDR clock: %d MHz (%d Mhz DDR)",
	       divide_nint(gd->ogd.ddr_clock_hertz, 1000000),
	       divide_nint(gd->ogd.ddr_clock_hertz * 2, 1000000));
#endif

        if (OCTEON_IS_MODEL(OCTEON_CN63XX) || OCTEON_IS_MODEL(OCTEON_CN66XX)) {
            cvmx_lmcx_ddr_pll_ctl_t ddr_pll_ctl;

            uint64_t calculated_dfm_hertz;
            static const unsigned _en[] = {1, 2, 3, 4, 6, 8, 12};

            ddr_pll_ctl.u64 = cvmx_read_csr(CVMX_LMCX_DDR_PLL_CTL(0));
            calculated_dfm_hertz = (ddr_pll_ctl.cn63xx.clkf * 50 * 1000000)
                / _en[ddr_pll_ctl.cn63xx.dfm_ps_en];

            printf(", DFM clock: %lld MHz",
                   divide_nint(calculated_dfm_hertz, 1000000));
        }

	printf("\n");
	return 0;
}

int display_board_info(void) __attribute__((__weak__));
