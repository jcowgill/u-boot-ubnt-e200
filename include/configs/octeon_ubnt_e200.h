/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 * 
 * Copyright (C) 2004 - 2012 Cavium, Inc. <support@cavium.com>
 *
 * Copyright (C) 2013 Ubiquiti Networks, Inc.
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifndef CONFIG_OCTEON_PCI_HOST
# define CONFIG_OCTEON_PCI_HOST		1
#endif

#define CONFIG_OCTEON_USB_OCTEON2

#include "octeon_common.h"

#define UBNT_E200_DEF_DRAM_FREQ		533

#define CONFIG_OCTEON_MMC
#define CONFIG_CMD_MMC
#define CONFIG_MMC

#define CONFIG_SYS_SPD_BUS_NUM                  0

#define UBNT_BOOT_SIZE_KB 640
#define UBNT_EEPROM_SIZE_KB 64

#define _FLASH_PARTS _fparts(UBNT_BOOT_SIZE_KB, UBNT_EEPROM_SIZE_KB)
#define _fparts(bsize, esize) __fparts(bsize, esize)
#define __fparts(bsize, esize) \
    "phys_mapped_flash:" #bsize "k(boot0)," #bsize "k(boot1)," \
        #esize "k(eeprom)"

#define CONFIG_UBNT_EEPROM_ADDR	\
    (CONFIG_SYS_FLASH_BASE + (UBNT_BOOT_SIZE_KB * 2 * 1024))
#define CONFIG_UBNT_EEPROM_SIZE	(UBNT_EEPROM_SIZE_KB * 1024)

#define CFG_PRINT_MPR

#define CONFIG_BOOTDELAY	0
#define CONFIG_ZERO_BOOTDELAY_CHECK
#undef	CONFIG_BOOTARGS
#define CONFIG_BOOTCOMMAND \
    "fatload mmc 0 $(loadaddr) vmlinux.64;" \
    "bootoctlinux $(loadaddr) numcores=2 endbootargs " \
    "mem=0 root=/dev/mmcblk0p2 rootdelay=10 rw " \
    "rootsqimg=squashfs.img rootsqwdir=w " \
    "mtdparts=" _FLASH_PARTS

#define CONFIG_FACTORY_RESET
#define CONFIG_FACTORY_RESET_GPIO      0
#define CONFIG_FACTORY_RESET_TIME      3
#define CONFIG_FACTORY_RESET_BOOTCMD   CONFIG_BOOTCOMMAND " resetsqimg"

#ifndef __ASSEMBLY__
extern void board_blink_led(int ms);
extern void board_set_led_on(void);
extern void board_set_led_off(void);
extern void board_set_led_normal(void);
#endif

#define OCTEON_SGMII_ENET
#define OCTEON_INTERNAL_ENET

#include "octeon_cmd_conf.h"

#ifdef  CONFIG_CMD_NET
# include "octeon_network.h"
#endif

#define CONFIG_CMD_FAT
#define CONFIG_CMD_FLASH

/*
 * Miscellaneous configurable options
 */
/* Environment variables that will be set by default */
#define	CONFIG_EXTRA_ENV_SETTINGS \
    "nuke_env=protect off $(env_addr) +$(env_size);" \
        "erase $(env_addr) +$(env_size)\0" \
    "mtdparts=" _FLASH_PARTS "\0" \
    "autoload=n\0" \
    ""

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */
#define CONFIG_SYS_FLASH_SIZE	        (8*1024*1024)	/* Flash size (bytes) */
#define CONFIG_SYS_MAX_FLASH_BANKS	1	/* max number of memory banks */
#define CONFIG_SYS_MAX_FLASH_SECT	(512)	/* max number of sectors on one chip */

/* Width of CFI bus to start scanning */
#define CONFIG_SYS_FLASH_CFI_WIDTH	FLASH_CFI_8BIT

/* Enable extra elements in CFI driver for storing flash geometry */
#define CONFIG_SYS_FLASH_CFI  		1
/* Enable CFI flash driver */
#define CONFIG_FLASH_CFI_DRIVER		1

/* We're not RAM_RESIDENT so CONFIG_ENV_IS_IN_FLASH will be set. */
#if CONFIG_RAM_RESIDENT
# define	CONFIG_ENV_IS_NOWHERE	1
#else
# define	CONFIG_ENV_IS_IN_FLASH	1
#endif

/* Address and size of Primary Environment Sector	*/
#define CONFIG_ENV_SIZE		(8*1024)
#define CONFIG_UBNT_GD_SIZE	CONFIG_ENV_SIZE

/*
 * jcowgill:
 * The flash layout is roughly:
 *  0          = U-Boot
 *   ..
 *  size - 24k = Chainloaded Environment
 *   ..
 *  size - 16k = "gd" block (contains immutable config like MAC addresses)
 *
 *  size - 8k  = Old Environment
 *    ..
 *  size - 1
 *
 * If CONFIG_ENV_CHAINLOAD is set, we use the chainloaded environment area,
 * otherwize we use the "normal" environment area.
 */
#define CONFIG_ENV_CHAINLOAD

#ifdef CONFIG_ENV_CHAINLOAD
# define CONFIG_ENV_ADDR		(CONFIG_SYS_FLASH_BASE + CONFIG_SYS_FLASH_SIZE - CONFIG_ENV_SIZE * 2 - CONFIG_UBNT_GD_SIZE)
#else
# define CONFIG_ENV_ADDR		(CONFIG_SYS_FLASH_BASE + CONFIG_SYS_FLASH_SIZE - CONFIG_ENV_SIZE)
#endif

#define CONFIG_UBNT_GD_ADDR	(CONFIG_SYS_FLASH_BASE + CONFIG_SYS_FLASH_SIZE - CONFIG_ENV_SIZE - CONFIG_UBNT_GD_SIZE)
#define CONFIG_UBNT_GD_MAC_DESC_OFF	0
#define CONFIG_UBNT_GD_BOARD_DESC_OFF	(CONFIG_UBNT_GD_MAC_DESC_OFF \
					 + sizeof(octeon_eeprom_mac_addr_t))

/*-----------------------------------------------------------------------
 * Cache Configuration
 */
#define CONFIG_SYS_DCACHE_SIZE		(32 * 1024)
#define CONFIG_SYS_ICACHE_SIZE		(37 * 1024)

/* Include shared board configuration, consisting mainly of DRAM details. */
#include "octeon_ubnt_e200_shared.h"

#undef CONFIG_SHA1
#undef CONFIG_SHA256
#undef CONFIG_MD5
#undef CONFIG_BZIP2
#undef CONFIG_OCTEON_EEPROM_TUPLES
#define CONFIG_OCTEON_EEPROM_TUPLES 0
#undef CONFIG_PCI
#undef CONFIG_PCI_PNP
#undef CONFIG_PCIAUTO_SKIP_HOST_BRIDGE
#undef CONFIG_SYS_PCI_64BIT
#undef CONFIG_CMD_PCI

#undef CONFIG_CMD_IMPORTENV
#undef CONFIG_CMD_EXPORTENV
#undef CONFIG_CMD_ASKENV
#undef CONFIG_CMD_EDITENV

#undef CONFIG_CMD_IMLS
#undef CONFIG_CMD_ELF
#undef CONFIG_CMD_BOOTM

#endif	/* __CONFIG_H__ */
