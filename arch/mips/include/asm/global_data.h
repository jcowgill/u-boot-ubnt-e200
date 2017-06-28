/*
 * (C) Copyright 2002-2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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

#ifndef	__ASM_GBL_DATA_H
#define __ASM_GBL_DATA_H
#ifdef __U_BOOT__
# include <config.h>
# include <asm/types.h>
#endif
#include <asm/regdef.h>
#ifdef CONFIG_OCTEON
# include <asm/octeon_global_data.h>
# include <asm/u-boot.h>
#endif
#ifndef __U_BOOT__
# include <asm/octeon_global_data.h>
# include <asm/u-boot.h>
#endif

/*
 * The following data structure is placed in some memory wich is
 * available very early after boot (like DPRAM on MPC8xx/MPC82xx, or
 * some locked parts of the data cache) to allow for a minimum set of
 * global variables during system initialization (until we have set
 * up the memory controller so that we can use RAM).
 *
 * Keep it *SMALL* and remember to set GENERATED_GBL_DATA_SIZE > sizeof(gd_t)
 */

typedef	struct	global_data {
	bd_t		*bd;
	unsigned long	flags;
#ifdef CONFIG_JZSOC
	/* There are other clocks in the jz4740 */
	unsigned long	cpu_clk;	/* CPU core clock */
	unsigned long	sys_clk;	/* System bus clock */
	unsigned long	per_clk;	/* Peripheral bus clock */
	unsigned long	mem_clk;	/* Memory bus clock */
	unsigned long	dev_clk;	/* Device clock */
	/* "static data" needed by most of timer.c */
	unsigned long	tbl;
	unsigned long	lastinc;
#endif
	unsigned long	baudrate;
	unsigned long	have_console;	/* serial_init() was called */
#ifdef CONFIG_PRE_CONSOLE_BUFFER
	unsigned long	precon_buf_idx;	/* Pre-Console buffer index */
#endif
	phys_size_t	ram_size;	/* RAM size */
	phys_size_t	reloc_off;	/* Relocation Offset */
	unsigned long	env_addr;	/* Address  of Environment struct */
	unsigned long	env_valid;	/* Checksum of Environment valid? */
	void		**jt;		/* jump table */
	void		*fdt_blob;	/* Our device tree, NULL if none */
	char		env_buf[512];	/* buffer for getenv() before reloc. */
#ifdef CONFIG_OCTEON
	octeon_global_data_t	ogd;	/* Octeon specific global data */
#endif
} gd_t;

/*
 * Global Data Flags
 */
#define	GD_FLG_RELOC		0x00001	/* Code was relocated to RAM		*/
#define	GD_FLG_DEVINIT		0x00002	/* Devices have been initialized	*/
#define	GD_FLG_SILENT		0x00004	/* Silent mode				*/
#define	GD_FLG_POSTFAIL		0x00008	/* Critical POST test failed		*/
#define	GD_FLG_POSTSTOP		0x00010	/* POST seqeunce aborted		*/
#define	GD_FLG_LOGINIT		0x00020	/* Log Buffer has been initialized	*/
#define GD_FLG_DISABLE_CONSOLE	0x00040	/* Disable console (in & out)		*/
#define GD_FLG_ENV_READY	0x00080	/* Environment imported into hash table	*/

#ifdef CONFIG_OCTEON
#define GD_FLG_FAILSAFE_MODE    	0x01000 /* Failsafe mode */
/* Note that the DDR clock initialized flags must be contiguous */
#define GD_FLG_DDR0_CLK_INITIALIZED	0x02000 /* Clock for DDR 0 initialized */
#define GD_FLG_DDR1_CLK_INITIALIZED	0x04000 /* Clock for DDR 1 initialized */
#define GD_FLG_DDR2_CLK_INITIALIZED	0x08000 /* Clock for DDR 2 initialized */
#define GD_FLG_DDR3_CLK_INITIALIZED	0x10000 /* Clock for DDR 3 initialized */
#define GD_FLG_RAM_RESIDENT		0x20000 /* Loaded into RAM externally */
#define GD_FLG_DDR_VERBOSE		0x40000 /* Verbose DDR information */
#define GD_FLG_DDR_DEBUG		0x80000 /* Check env. for DDR variables */
#define GD_FLG_DDR_TRACE_INIT		0x100000
#define GD_FLG_MEMORY_PRESERVED		0x200000
#define GD_FLG_DFM_VERBOSE		0x400000
#define GD_FLG_DFM_TRACE_INIT		0x800000
#define GD_FLG_DFM_CLK_INITIALIZED	0x1000000 /* DFM memory clock initialized */
#define GD_FLG_CLOCK_DESC_MISSING	0x2000000 /* EEPROM clock descr. missing */
#define GD_FLG_BOARD_DESC_MISSING	0x4000000 /* EEPROM board descr. missing */
#define GD_FLG_DDR_PROMPT		0x8000000
#endif /* CONFIG_OCTEON */

#define DECLARE_GLOBAL_DATA_PTR     register volatile gd_t *gd asm ("k0")

#endif /* __ASM_GBL_DATA_H */
