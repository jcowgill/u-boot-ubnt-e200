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

#include <common.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <fdt.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/octeon_board_mmc.h>
#include <asm/arch/octeon-feature.h>

#ifndef CONFIG_OCTEON_MAX_MMC_SLOT
# define CONFIG_OCTEON_MAX_MMC_SLOT 4
#endif

DECLARE_GLOBAL_DATA_PTR;

int octeon_boot_bus_mmc_get_info(struct octeon_mmc_info *mmc_info)
	__attribute__((weak, alias("__octeon_boot_bus_mmc_get_info")));

int __octeon_boot_bus_mmc_get_info(struct octeon_mmc_info *mmc_info)
{
	int i;
#ifdef CONFIG_OF_LIBFDT
	int nodeoffset;
	const void *nodep;
	uint32_t *pgpio_handle;
	int gpio_offset;
	int phandle;
#endif

	if (!octeon_has_feature(OCTEON_FEATURE_MMC))
		return -1;

	if (mmc_info == NULL)
		return -1;

	memset(mmc_info, 0, sizeof(mmc_info));
#ifdef CONFIG_OF_LIBFDT
	nodeoffset = fdt_path_offset(gd->fdt_blob, "/soc/mmc");

	if (nodeoffset < 0) {
		debug("MMC interface not found\n");
		return -1;
	}

	if (fdt_node_check_compatible(gd->fdt_blob, nodeoffset,
				      "cavium,octeon-6130-mmc")) {
		puts("Incompatible MMC interface type in FDT\n");
		return -1;
	}
#endif
	memset(mmc_info, 0, sizeof(*mmc_info));

	for (i = 0; i < CONFIG_OCTEON_MAX_MMC_SLOT; i++) {
#ifdef CONFIG_OF_LIBFDT
		int len;
		int slotoffset;
		char name[16];
		int slot;
#endif
		mmc_info->slot[i].chip_sel = -1;
#ifdef CONFIG_OF_LIBFDT
		sprintf(name, "mmc-slot@%i", i);
		slotoffset = fdt_subnode_offset(gd->fdt_blob, nodeoffset,
						name);
		if (slotoffset < 0)
			continue;

		if (fdt_node_check_compatible(gd->fdt_blob, slotoffset,
					      "cavium,octeon-6130-mmc-slot")) {
			printf("Incompatible MMC slot %d\n", i);
			return -1;
		}

		nodep = fdt_getprop(gd->fdt_blob, slotoffset, "reg", &len);
		if (nodep == NULL || len <= 0) {
			printf("Chip select for mmc slot %d missing in FDT\n", i);
			return -1;
		}

		if (len != 4) {
			printf("Unknown FDT property size for MMC slot %d reg\n",
			       i);
			return -1;
		}

		slot = fdt32_to_cpu(*(uint32_t *)nodep);

		if (slot != i) {
			printf("Bad register address %d for MMC slot %d\n",
			       slot, i);
			return -1;
		}

		mmc_info->slot[i].chip_sel = i;

		nodep = fdt_getprop(gd->fdt_blob, slotoffset,
				    "voltage-ranges", &len);

		nodep = fdt_getprop(gd->fdt_blob, slotoffset,
				    "spi-max-frequency", &len);
		if (nodep != NULL && len != 4) {
			printf("Bad max frequency for MMC slot %d in FDT\n", i);
			return -1;
		}
		if (nodep)
			mmc_info->slot[i].max_frequency =
				fdt32_to_cpu(*(uint32_t *)nodep);
		else
			mmc_info->slot[i].max_frequency = 50000000;

		nodep = fdt_getprop(gd->fdt_blob, slotoffset,
				    "cavium,bus-max-width", &len);

		if (nodep && len != 4) {
			printf("Bad bus-max-width for MMC slot %d in FDT\n", i);
			return -1;
		}

		if (nodep)
			mmc_info->slot[i].bus_max_width =
				fdt32_to_cpu(*(uint32_t *)nodep);
		else
			mmc_info->slot[i].bus_max_width = 8;

		mmc_info->slot[i].power_gpio = -1;
		pgpio_handle = (uint32_t *)fdt_getprop(gd->fdt_blob,
						       slotoffset,
						       "power-gpios", &len);
		if (pgpio_handle && len == 12) {
			phandle = fdt32_to_cpu(pgpio_handle[0]);
			gpio_offset = fdt_node_offset_by_phandle(gd->fdt_blob,
							 phandle);
			if (gpio_offset < 0) {
				printf("%s: Cannot access parent GPIO node in DT\n",
				       __func__);
				return -1;
			}
			if (fdt_node_check_compatible(gd->fdt_blob,
						      gpio_offset,
						      "cavium,octeon-3860-gpio")) {
				printf("%s: ERROR: Only native GPIO pins supported\n",
				       __func__);
				return -1;
			}
			mmc_info->slot[i].power_gpio =
					fdt32_to_cpu(pgpio_handle[1]);
			mmc_info->slot[i].power_active_low =
					(fdt32_to_cpu(pgpio_handle[2]) == 1);
			debug("%s: MMC slot %d power GPIO %d active %s\n",
			      __func__, i, mmc_info->slot[i].power_gpio,
			      mmc_info->slot[i].power_active_low ? "low" : "high");
		}

		mmc_info->slot[i].cd_gpio = -1;
		pgpio_handle = (uint32_t *)fdt_getprop(gd->fdt_blob,
						       slotoffset,
						       "cd-gpios", &len);
		if (pgpio_handle && len == 12) {
			phandle = fdt32_to_cpu(pgpio_handle[0]);
			gpio_offset = fdt_node_offset_by_phandle(gd->fdt_blob,
							 phandle);
			if (gpio_offset < 0) {
				printf("%s: Cannot access parent GPIO node in DT\n",
				       __func__);
				return -1;
			}
			if (fdt_node_check_compatible(gd->fdt_blob,
						      gpio_offset,
						      "cavium,octeon-3860-gpio")) {
				printf("%s: ERROR: Only native GPIO pins supported\n",
				       __func__);
				return -1;
			}
			mmc_info->slot[i].cd_gpio =
					fdt32_to_cpu(pgpio_handle[1]);
			mmc_info->slot[i].cd_active_low =
					fdt32_to_cpu(pgpio_handle[2]) == 0;
			debug("%s: MMC slot %d cd GPIO %d active %s\n",
			      __func__, i, mmc_info->slot[i].cd_gpio,
			      mmc_info->slot[i].cd_active_low ? "low" : "high");
		}

		mmc_info->slot[i].wp_gpio = -1;
		pgpio_handle = (uint32_t *)fdt_getprop(gd->fdt_blob,
						       slotoffset,
						       "cd-gpios", &len);
		if (pgpio_handle && len == 12) {
			phandle = fdt32_to_cpu(pgpio_handle[0]);
			gpio_offset = fdt_node_offset_by_phandle(gd->fdt_blob,
							 phandle);
			if (gpio_offset < 0) {
				printf("%s: Cannot access parent GPIO node in DT\n",
				       __func__);
				return -1;
			}
			if (fdt_node_check_compatible(gd->fdt_blob,
						      gpio_offset,
						      "cavium,octeon-3860-gpio")) {
				printf("%s: ERROR: Only native GPIO pins supported\n",
				       __func__);
				return -1;
			}
			mmc_info->slot[i].wp_gpio =
					fdt32_to_cpu(pgpio_handle[1]);
			mmc_info->slot[i].wp_active_low =
					(fdt32_to_cpu(pgpio_handle[2]) == 0);
			debug("%s: MMC slot %d wp GPIO %d active %s\n",
			      __func__, i, mmc_info->slot[i].cd_gpio,
			      mmc_info->slot[i].cd_active_low ? "low" : "high");
		}

#else
		if (CONFIG_OCTEON_AVAIL_MMC_SLOTS & (1 << i))
			mmc_info->slot[i].chip_sel = i;
		mmc_info->slot[i].max_frequency = CONFIG_OCTEON_MMC_MAX_FREQUENCY;
		mmc_info->slot[i].bus_max_width = CONFIG_OCTEON_MMC_MAX_WIDTH;
# ifdef CONFIG_OCTEON_MMC_WP_GPIO
		mmc_info->slot[i].wp_gpio = CONFIG_OCTEON_MMC_WP_GPIO;
# else
		mmc_info->slot[i].wp_gpio = -1;
# endif
# ifdef CONFIG_OCTEON_MMC_CD_GPIO
		mmc_info->slot[i].cd_gpio = CONFIG_OCTEON_MMC_CD_GPIO;
# else
		mmc_info->slot[i].cd_gpio = -1;
# endif
# ifdef CONFIG_OCTEON_MMC_POWER_GPIO
		mmc_info->slot[i].power_gpio = CONFIG_OCTEON_MMC_POWER_GPIO;
# else
		mmc_info->slot[i].power_gpio = -1;
# endif
#endif
		switch (mmc_info->slot[i].bus_max_width) {
		case 1:
		case 4:
		case 8:
			break;
		default:
			printf("Bad bus-max-width %d for MMC slot %d in FDT\n",
			       mmc_info->slot[i].bus_max_width, i);
			return -1;
		}
	}
	return 0;
}