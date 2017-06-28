/*
 * (C) Copyright 2012 Cavium, Inc.
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

/* This file sets up the boot bus moveable region. */
#include <common.h>
#include <asm/arch/octeon_boot_bus.h>
#include <asm/arch/cvmx-mio-defs.h>

/**
 * NOTE: This is called when the Watchdog is disabled
 * since the watchdog overrides the first moveable region.
 */
int octeon_boot_bus_moveable_init(void)
	__attribute__((weak, alias("__octeon_boot_bus_moveable_init")));
int __octeon_boot_bus_moveable_init(void)
{
	extern void debugHandler_entrypoint(void);
	extern void SecondaryCoreInit(void);
	const uint64_t *handler_code;
	int count;

	debug("%s: Installing moveable regions\n", __func__);
	/* Install the 2nd moveable region to the debug exception main entry
	 * point. This way the debugger will work even if there isn't any
	 * flash.
	 */

	handler_code = (const uint64_t *)debugHandler_entrypoint;
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_ADR, 0x80);
	for (count = 0; count < 128 / 8; count++)
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_DAT, *handler_code++);
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_CFGX(1),
		       (1ull << 31) | (0x1fc00480 >> 4));

	/* Install the first movable region at the reset vector so we can boot
	 * other cores
	 */
	/* Auto increments after write */
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_ADR, 0x0);
	handler_code = (const uint64_t *)SecondaryCoreInit;
	for (count = 0; count < 128 / 8; count++) {
		debug("0x%016llx\n", *handler_code);
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_DAT, *handler_code++);
	}
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_CFGX(0),
		       (1ull << 31) | (0x1fc00000 >> 4));
	debug("%s: Done.\n", __func__);
	return 0;
}
