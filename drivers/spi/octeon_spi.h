/*
 * SH SPI driver
 *
 * Copyright (C) 2011 Renesas Solutions Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __OCTEON_SPI_H__
#define __OCTEON_SPI_H__

#include <spi.h>
#include <asm/arch/cvmx.h>
#include <mtd/cfi_flash.h>

#define OCTEON_SPI_CFG	0
#define OCTEON_SPI_STS	0x08
#define OCTEON_SPI_TX	0x10
#define OCTEON_SPI_DAT0	0x80

#define OCTEON_SPI_MAX_BYTES	9
#define OCTEON_SPI_MAX_CLOCK_HZ	16000000
#define OCTEON_SPI_MAX_CHIP_SEL	1

struct octeon_spi {
	struct spi_slave slave;
	u32 max_speed_hz;
	u8 mode;
	u8 bytes_per_word;
};

struct octeon_spi

#endif

