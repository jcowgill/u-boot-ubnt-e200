/*
 * (C) Copyright 2012 Cavium, Inc. <support@cavium.com>
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
#include <miiphy.h>
#include <phy.h>
#include <net.h>
#include <fdt.h>
#include <fdtdec.h>
#include <libfdt.h>
#ifdef CONFIG_PHYLIB_10G
#include <i2c.h>
#endif
#include <asm/arch/octeon_eth.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-qlm.h>

DECLARE_GLOBAL_DATA_PTR;

extern int octeon_i2c_reg_addr_to_bus(uint64_t addr);

/**
 * Maps GPIO lines to the global GPIO config registers.
 *
 * Please see the data sheet since the configuration for each GPIO line is
 * different.
 */
static const struct {
	uint32_t config1_status_reg;
	uint32_t config2_reg;
} vcs848x_gpio_to_reg[12] = {
	{ 0x1e0100, 0x1e0101 },	/* 0 */
	{ 0x1e0102, 0x1e0103 },	/* 1 */
	{ 0x1e0104, 0x1e0105 },	/* 2 */
	{ 0x1e0106, 0x1e0107 },	/* 3 */
	{ 0x1e0108, 0x1e0109 },	/* 4 */
	{ 0x1e010A, 0x1e010B },	/* 5 */
	{ 0x1e0124, 0x1e0125 },	/* 6 */
	{ 0x1e0126, 0x1e0127 },	/* 7 */
	{ 0x1e0128, 0x1e0129 },	/* 8 */
	{ 0x1e012a, 0x1e012b },	/* 9 */
	{ 0x1e012c, 0x1e012d },	/* 10 */
	{ 0x1e012e, 0x1e012f },	/* 11 */
};

/**
 * @INTERNAL
 * Configures a Marvell PHY based on the device tree
 *
 * @param ethdev - Ethernet device to configure
 *
 * @return 0 for success, error otherwise
 */
static int octeon_fdt_marvell_config(const struct eth_device *ethdev)
{
	struct octeon_eth_info *oct_eth_info =
		 (struct octeon_eth_info *)ethdev->priv;
		uint32_t page, reg, mask, value;
	int last_page = -1;
	int len;
	uint32_t *init_seq;
	int seq_len;
	int phy_node_offset = oct_eth_info->phy_node_offset;

	debug("%s(%s)\n", __func__, ethdev->name);

	/* Check for at least one compatible Marvell PHY */
	if (fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "marvell,88e1118") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "marvell,88e1149") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "marvell,88e1149r") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "marvell,88e1145") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "marvell,88e1240") ) {
		printf("%s: Warning: unknown Marvell PHY for %s\n",
		       __func__, ethdev->name);
		return 0;
	}

	init_seq = (uint32_t *)fdt_getprop(gd->fdt_blob, phy_node_offset,
					   "marvell,reg-init", &len);
	if (!init_seq)
		return 0;

	if (len % 16 != 0) {
		printf("Invalid init sequence in DT for Marvell PHY for %s\n",
		       ethdev->name);
		return -1;
	}

	seq_len = len / 16;
	while (seq_len--) {
		page = fdt32_to_cpu(*init_seq++);
		reg = fdt32_to_cpu(*init_seq++);
		mask = fdt32_to_cpu(*init_seq++);
		if (page != last_page) {
			phy_write(oct_eth_info->phydev, MDIO_DEVAD_NONE,
				  0x16, page);
			last_page = page;
		}

		if (mask) {
			value = phy_read(oct_eth_info->phydev, MDIO_DEVAD_NONE,
					 reg);
			value &= mask;
			value |= fdt32_to_cpu(*init_seq++);
		} else {
			value = fdt32_to_cpu(*init_seq++);
		}
		debug("Marvell init for %s addr: %d: page: %d, reg: %d, mask: 0x%x, value: 0x%x\n",
		      ethdev->name, oct_eth_info->phydev->addr, page, reg,
		      mask, value);

		phy_write(oct_eth_info->phydev, MDIO_DEVAD_NONE, reg, value);
	}
	/* Reset to page 0 */
	if (last_page != 0)
		phy_write(oct_eth_info->phydev, MDIO_DEVAD_NONE, 0x16, 0);

	return 0;
}

/**
 * @INTERNAL
 * Configures a Broadcom PHY based on the device tree
 *
 * @param ethdev - Ethernet device to configure
 *
 * @return 0 for success, error otherwise
 */
static int octeon_fdt_broadcom_config(const struct eth_device *ethdev)
{
	struct octeon_eth_info *oct_eth_info =
		 (struct octeon_eth_info *)ethdev->priv;
	uint32_t devid = MDIO_DEVAD_NONE;
	uint32_t reg, mask;
	int value;
	int c45 = 0;	/* True if clause-45 mode */
	int len;
	uint32_t *init_seq;
	int seq_len;
	int phy_node_offset = oct_eth_info->phy_node_offset;

	debug("%s(%s)\n", __func__, ethdev->name);

	/* Check for at least one compatible Broadcom PHY */
	if (fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "broadcom,bcm5466") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "broadcom,bcm5482") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "broadcom,bcm5464r") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "broadcom,bcm5464") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "broadcom,bcm5241") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "broadcom,bcm7806") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "broadcom,bcm8706") &&
	    fdt_node_check_compatible(gd->fdt_blob, phy_node_offset,
				      "broadcom,bcm8727") ) {

		printf("%s: Unknown broadcom phy for %s\n",
		       __func__, ethdev->name);
		return 0;
	}

	init_seq = (uint32_t *)fdt_getprop(gd->fdt_blob, phy_node_offset,
					   "broadcom,c45-reg-init", &len);
	if (init_seq)
		c45 = 1;
	else
		init_seq = (uint32_t *)fdt_getprop(gd->fdt_blob,
						   phy_node_offset,
						   "broadcom,reg-init", &len);
	if (!init_seq)
		return 0;

	if ((c45 && (len % 16 != 0)) || (!c45 && (len % 12 != 0))) {
		printf("Invalid init sequence in DT for Broadcom PHY for %s (c45: %d, len: %d)\n",
		       ethdev->name, c45, len);
		return -1;
	}

	seq_len = len / (c45 ? 16 : 12);
	while (seq_len--) {
		devid = (c45) ? fdt32_to_cpu(*init_seq++) : MDIO_DEVAD_NONE;
		reg = fdt32_to_cpu(*init_seq++);
		mask = fdt32_to_cpu(*init_seq++);

		if (mask) {
			value = phy_read(oct_eth_info->phydev, devid, reg);
			if (value < 0) {
				printf("%s: Error reading from phy for %s at %d:%d\n",
				       __func__, ethdev->name, devid, reg);
				return -1;
			}
			debug("%s: Read: 0x%x, mask: 0x%x\n", __func__, value, mask);
			value &= mask;
			value |= fdt32_to_cpu(*init_seq++);
		} else {
			value = fdt32_to_cpu(*init_seq++);
		}
		debug("Broadcom init for %s addr: %d: dev: %d, reg: 0x%x, mask: 0x%x, value: 0x%x\n",
		      ethdev->name, oct_eth_info->phydev->addr, devid, reg,
		      mask, value);

		if (phy_write(oct_eth_info->phydev, devid, reg, value) < 0) {
			printf("%s: Error writing 0x%x to phy for %s at %d:%d\n",
			       __func__, value, ethdev->name, devid, reg);
			return -1;
		}
	}

	return 0;
}

/**
 * @INTERNAL
 * Configures the Vitesse VCS8488 for use on boards like the SNIC10xxx based
 * boards using the values in the flat device tree.
 *
 * @param ethdev - ethernet device to initialize PHY for
 *
 * @returns 0 for success, error otherwise
 */
static int octeon_fdt_vitesse_config(const struct eth_device *ethdev)
{
	const void *fdt = gd->fdt_blob;
	struct phy_device *phydev;
	int phy_node_offset;
	uint32_t txon_gpio;
	uint32_t rx_eq;
	uint32_t tx_preemph;
	uint32_t tx_out_driver_ctl1;
	uint32_t tx_out_driver_ctl2;
	const uint32_t *nodep;
	uint32_t devid = MDIO_DEVAD_NONE;
	uint32_t reg, mask;
	int value;
	int c45 = 0;	/* True if clause-45 mode */
	int len;
	uint32_t *init_seq;
	int seq_len;
	int vsc8488 = 0;
	const struct octeon_eth_info *oct_eth_info =
				 (struct octeon_eth_info *)ethdev->priv;

	debug("%s(%s)\n", __func__, ethdev->name);

	phydev = oct_eth_info->phydev;

	if (!phydev) {
		printf("%s: Error: could not get phy device for %s\n",
		       __func__, ethdev->name);
		return -1;
	}

	phy_node_offset = oct_eth_info->phy_node_offset;
	if (phy_node_offset < 0) {
		printf("%s: Error: could not find phy info for phy device\n",
		       __func__);
		return -1;
	}
	if (!fdt_node_check_compatible(fdt, phy_node_offset,
				       "vitesse,vsc8488") ||
	    !fdt_node_check_compatible(fdt, phy_node_offset,
				       "vitesse,vsc8488-15") ||
	    !fdt_node_check_compatible(fdt, phy_node_offset,
				       "vitesse,vsc8487") ||
	    !fdt_node_check_compatible(fdt, phy_node_offset,
				       "vitesse,vsc8486") ||
	    !fdt_node_check_compatible(fdt, phy_node_offset,
				       "vitesse,vsc8486-15") ||
	    !fdt_node_check_compatible(fdt, phy_node_offset,
				       "vitesse,vsc8484") ) {
		vsc8488 = 1;
		c45 = 1;
	}

	init_seq = (uint32_t *)fdt_getprop(gd->fdt_blob, phy_node_offset,
					   "vitesse,c45-reg-init", &len);
	if (init_seq)
		c45 = 1;
	else
		init_seq = (uint32_t *)fdt_getprop(gd->fdt_blob,
						   phy_node_offset,
						   "vitesse,reg-init", &len);

	if (init_seq) {
		if ((c45 && (len % 16 != 0)) || (len % 12 != 0)) {
			printf("Invalid init sequence in DT for Broadcom PHY for %s\n",
			       ethdev->name);
			return -1;
		}

		seq_len = len / (c45 ? 16 : 12);
		while (seq_len--) {
			devid = (c45) ?
				fdt32_to_cpu(*init_seq++) : MDIO_DEVAD_NONE;
			reg = fdt32_to_cpu(*init_seq++);
			mask = fdt32_to_cpu(*init_seq++);

			if (mask) {
				value = phy_read(oct_eth_info->phydev, devid, reg);
				if (value < 0) {
					printf("%s: Error reading from phy for %s at %d:%d\n",
					       __func__, ethdev->name, devid, reg);
					return -1;
				}
				debug("%s: Read: 0x%x, mask: 0x%x\n", __func__,
				      value, mask);
				value &= mask;
				value |= fdt32_to_cpu(*init_seq++);
			} else {
				value = fdt32_to_cpu(*init_seq++);
			}
			debug("Vitesse init for %s addr: %d: dev: %d, reg: 0x%x, mask: 0x%x, value: 0x%x\n",
			      ethdev->name, oct_eth_info->phydev->addr, devid,
			      reg, mask, value);

			if (phy_write(oct_eth_info->phydev, devid,
				      reg, value) < 0) {
				printf("%s: Error writing 0x%x to phy for %s at %d:%d\n",
				       __func__, value, ethdev->name, devid, reg);
				return -1;
			}
		}
	}

	if (!vsc8488) {
		debug("Vitesse vsc848x not found for %s\n", ethdev->name);
		return 0;
	}

	/* The following are only supported on the VCS8488 for now */
	tx_preemph = getenv_ulong("vcs8488_preemph", 16, 0xffffffff);
	if (tx_preemph != 0xffffffff) {
		printf("Overriding PHY XS XAUI TX Pre-emphasis Control register to 0x%x\n",
		       tx_preemph);
		phy_write(phydev, 4, 0x8011, tx_preemph);
	} else {
		nodep = fdt_getprop(fdt, phy_node_offset,
				    "vitesse,tx_preemphasis",
				    NULL);
		if (nodep) {
			tx_preemph = fdt32_to_cpu(*nodep);
			debug("%s: Writing 0x%x to TX pre-emphasis\n",
			      __func__, tx_preemph);
			phy_write(phydev, 4, 0x8011, tx_preemph);
		}
	}

	rx_eq = getenv_ulong("vcs8488_rxeq", 16, 0xffffffff);
	if (rx_eq != 0xffffffff) {
		printf("Overriding PHY XS XAUI Rx Equalization Control with 0x%x\n",
		       rx_eq);
		phy_write(phydev, 4, 0x8010, rx_eq);
	} else {
		nodep = fdt_getprop(fdt, phy_node_offset,
				    "vitesse,rx_equalization", NULL);
		if (nodep) {
			rx_eq = fdt32_to_cpu(*nodep);
			debug("%s: Writing 0x%x to RX equalization\n",
			      __func__, rx_eq);
			phy_write(phydev, 4, 0x8010, rx_eq);
		}
	}

	tx_out_driver_ctl1 = getenv_ulong("vcs8488_txdrvctrl1", 16, 0xffffffff);
	if (tx_out_driver_ctl1 != 0xffffffff) {
		printf("Overriding TX Output Driver Control 1 with 0x%x\n",
		       tx_out_driver_ctl1);
		phy_write(phydev, 1, 0x8014, tx_out_driver_ctl1);
	} else {
		nodep = fdt_getprop(fdt, phy_node_offset,
				    "vitesse,txout_driver_ctrl1", NULL);
		if (nodep) {
			tx_out_driver_ctl1 = fdt32_to_cpu(*nodep);
			debug("%s: Writing 0x%x to TX Output driver control 1\n",
			      __func__, tx_out_driver_ctl1);
			phy_write(phydev, 1, 0x8013, tx_out_driver_ctl1);
		}
	}

	tx_out_driver_ctl2 = getenv_ulong("vcs8488_txdrvctrl2", 16, 0xffffffff);
	if (tx_out_driver_ctl2 != 0xffffffff) {
		printf("Overriding TX Output Driver Control 2 with 0x%x\n",
		       tx_out_driver_ctl2);
		phy_write(phydev, 1, 0x8014, tx_out_driver_ctl2);
	} else {
		nodep = fdt_getprop(fdt, phy_node_offset,
				    "vitesse,txout_driver_ctrl2", NULL);
		if (nodep) {
			tx_out_driver_ctl2 = fdt32_to_cpu(*nodep);
			debug("%s: Writing 0x%x to TX Output driver control 2\n",
			      __func__, tx_out_driver_ctl2);
			phy_write(phydev, 1, 0x8014, tx_out_driver_ctl2);
		}
	}

	nodep = fdt_getprop(fdt, phy_node_offset, "txon", NULL);
	if (nodep) {
		txon_gpio = fdt32_to_cpu(*nodep);
		debug("%s: Turning on transmit with GPIO %d for %s\n",
		      __func__, txon_gpio, ethdev->name);
		if (txon_gpio >= 12) {
			printf("Error: txon gpio %d is out of range on %s\n",
			       txon_gpio, ethdev->name);
			return -1;
		}
		value = vcs848x_gpio_to_reg[txon_gpio].config1_status_reg;

		phy_write(phydev, value >> 16, value & 0xffff, 0);
	} else {
		printf("Could not find txon GPIO for %s\n", ethdev->name);
	}
	return 0;
}

#ifdef CONFIG_PHYLIB_10G

#define VITESSE_FUNC_MODE_LIMITING	2	/* Optical */
#define VITESSE_FUNC_MODE_COPPER	3	/* Copper */
#define VITESSE_FUNC_MODE_LINEAR	4
#define VITESSE_FUNC_MODE_KR		5
#define VITESSE_FUNC_MODE_ZR		7
#define VITESSE_FUNC_MODE_1G		8

#ifdef DEBUG
/**
 * This reads a global variable as described in the VSC848X EDC Firmware
 * Configuration Application Note.
 *
 * @param phydev[in] - phy device to read from
 * @param channel - channel to read
 * @param addr - global variable address
 *
 * @returns 0 for success, -1 on error.
 */
static int vitesse_read_global_var(struct phy_device *phydev, uint8_t channel,
				   uint8_t addr)
{
	int timeout = 10000;
	uint16_t val;
	do {
		val = phy_read(phydev, 0x1e, 0x7fe3);
		if (val == 0)
			break;
		udelay(1);
	} while (timeout-- > 0);
	if (timeout <= 0) {
		printf("%s: Timeout waiting to read global\n", __func__);
		return -1;
	}
	phy_write(phydev, 0x1e, 0x7fe3, 0x4000 | ((channel & 3) << 8) | addr);
	timeout = 10000;
	do {
		val = phy_read(phydev, 0x1e, 0x7fe3);
		if (val == 0)
			break;
		udelay(1);
	} while (timeout-- > 0);
	if (timeout <= 0) {
		printf("%s: Timeout waiting to read global\n", __func__);
		return -1;
	}
	val = phy_read(phydev, 0x1e, 0x7fe4);
	debug("%s: Read 0x%04x from channel %d, address 0x%x\n", __func__, val,
	      channel, addr);
	return val;
}
#endif

/**
 * This writes a global variable as described in the VSC848X EDC Firmware
 * Configuration Application Note.
 *
 * @param phydev[in] - phy device to write to
 * @param channel - channel to write
 * @param addr - global variable address
 * @param value - value to write
 *
 * @returns 0 for success, -1 on error.
 */
static int vitesse_write_global_var(struct phy_device *phydev, uint8_t channel,
				    uint8_t addr, uint16_t value)
{
	int timeout = 100000;
	uint16_t val;

	/* 1. Read control register until 0 */
	do {
		val = phy_read(phydev, 0x1e, 0x7fe3);
		if (val == 0)
			break;
		udelay(100);
	} while (timeout-- > 0);
	if (timeout <= 0) {
		printf("%s: Timeout waiting to read global 1, val=0x%x\n",
		       __func__, val);
		return -1;
	}
	/* 2. Write data register */
	phy_write(phydev, 0x1e, 0x7fe4, value);
	/* 3. Write control register */
	phy_write(phydev, 0x1e, 0x7fe3,
		  0x8000 | ((channel & 3) << 8) | addr);
	debug("%s: Wrote 0x%04x to channel %d, address 0x%x\n", __func__, value,
	      channel, addr);
	/* 4. Wait until control reg cleared */
	timeout = 100000;
	do {
		val = phy_read(phydev, 0x1e, 0x7fe3);
		if (val == 0)
			break;
		udelay(100);
	} while (timeout-- > 0);
	if (timeout <= 0) {
		printf("%s: Timeout waiting to read global 2, val=0x%x\n",
		       __func__, val);
		return -1;
	}
	return 0;
}

/**
 * This function configures the Vitesse PHY based on information read from the
 * SFP+ module SFF EEPROM
 *
 * @param ethdev - ethernet device to configure
 *
 * @returns 0 for success, error otherwise
 */
int octeon_vitesse_sfp_config(struct eth_device *ethdev)
{
	struct octeon_eth_info *oct_eth_info;
	uint32_t *sfp_handle;
	uint32_t phandle;
	uint32_t *value;
	int phy_offset;
	int sfp_offset;
	uint8_t sfp_buffer[256];
	int i2c_bus, i2c_addr;
	int offset;
	int len;
	int mode = VITESSE_FUNC_MODE_LIMITING;	/* Default for optical */
	uint64_t tx_id;
	uint8_t csum;
	int i;
	const char *mode_str = "Unknown";
	struct phy_device *phydev;
	int timeout;
	uint16_t val;
 	uint8_t addr_mask = 0;
        uint32_t old_stop_delay;

	oct_eth_info = (struct octeon_eth_info *)ethdev->priv;

	phydev = oct_eth_info->phydev;

	if (!is_10g_interface(oct_eth_info->phydev->interface))
		return 0;

	/* This is a lengthy process to parse and validate the device tree.
	 * The SPD EEPROM address is linked from the PHY.  We must obtain both
	 * the TWSI bus number and the address (which should be 0x50) from the
	 * device tree before we can actually read it.
	 */
	phy_offset = oct_eth_info->phy_fdt_offset;

	sfp_handle = (uint32_t *)fdt_getprop(gd->fdt_blob, phy_offset,
					     "sfp-eeprom", NULL);
	if (!sfp_handle) {
		printf("Error: cannot find SFP for PHY on %s\n", ethdev->name);
		return -1;
	}

	phandle = fdt32_to_cpu(*sfp_handle);
	sfp_offset = fdt_node_offset_by_phandle(gd->fdt_blob, phandle);
	if (sfp_offset < 0) {
		printf("Error: cannot find SFP for PHY on %s\n", ethdev->name);
		return -1;
	}

	if (fdt_node_check_compatible(gd->fdt_blob, sfp_offset, "atmel,24c01")) {
		printf("Error: Unknown SFP EEPROM type for %s\n", ethdev->name);
		return -1;
	}

	value = (uint32_t *)fdt_getprop(gd->fdt_blob, sfp_offset, "reg", NULL);
	if (!value) {
		printf("Error: could not get SFP EEPROM address for %s\n",
		       ethdev->name);
		return -1;
	}
	i2c_addr = fdt32_to_cpu(*value);

	offset = fdt_parent_offset(gd->fdt_blob, sfp_offset);
	if (fdt_node_check_compatible(gd->fdt_blob, offset,
				      "cavium,octeon-3860-twsi")) {
		printf("Error: incompatible TWSI bus type for %s\n",
		       ethdev->name);
		return -1;
	}

	value = (uint32_t *)fdt_getprop(gd->fdt_blob, offset, "reg", &len);

	if (len != 16)  {
		printf("Error: invalid register length for TWSI bus for SFP for %s\n",
		       ethdev->name);
		return -1;
	}
	if (!value) {
		printf("Error: could not obtain TWSI bus information for SFP for %s\n",
		       ethdev->name);
		return -1;
	}
	i2c_bus = octeon_i2c_reg_addr_to_bus(((uint64_t)fdt32_to_cpu(value[0]) << 32)
					   | fdt32_to_cpu(value[1]));

	if (i2c_bus < 0) {
		printf("Error: Unknown register address for TWSI bus for SPD for %s\n",
		       ethdev->name);
		return -1;
	}

	i2c_set_bus_num(i2c_bus);
	/* We need to increase the delay between transactions for the SFP+
	 * module to 26usec minimum.
	 */
extern uint32_t i2c_get_stop_delay(void);
extern int i2c_set_stop_delay(uint32_t delay);
        old_stop_delay = i2c_get_stop_delay();
        i2c_set_stop_delay(26);

	if (i2c_read(i2c_addr, 0, 1, sfp_buffer, 64)) {
		/* The SPD might not be present, i.e. if nothing is plugged in */
		debug("Error reading from spd on I2C bus %d, address 0x%x for %s\n",
		      i2c_bus, i2c_addr, ethdev->name);
		return 0;
	}

	i2c_set_stop_delay(old_stop_delay);

	/* Validate the checksum */
	csum = 0;
	for (i = 0; i < 63; i++)
		csum += sfp_buffer[i];
	if (csum != sfp_buffer[63]) {
		printf("Warning: SFP checksum information is incorrect for %s.\n"
		       "Checksum is 0x%x, expected 0x%x\n",
		       ethdev->name, sfp_buffer[63], csum);
	}

	/* Check transceiver type.  See the SFF-8472 standard. */
	if (sfp_buffer[0] != 3) {	/* SFP or SFP+ */
		printf("Error: Unknown SFP transceiver type 0x%x for %s\n",
		       sfp_buffer[0], ethdev->name);
		return -1;
	}

	if (sfp_buffer[1] < 1 || sfp_buffer[1] > 7) {	/* Extended ID */
		printf("Error: Unknown SFP extended identifier 0x%x for %s\n",
		       sfp_buffer[1], ethdev->name);
		return -1;
	}

	switch (sfp_buffer[2]) {
	case 0x01:
		mode = VITESSE_FUNC_MODE_LIMITING;
		mode_str = "SC";
		break;
	case 0x07:
		mode = VITESSE_FUNC_MODE_LIMITING;
		mode_str = "LC";
		break;
	case 0x0B:
		mode = VITESSE_FUNC_MODE_LIMITING;
		mode_str = "Optical Pigtail";
		break;
	case 0x21:
		mode = VITESSE_FUNC_MODE_COPPER;
		mode_str = "Copper Pigtail";
		break;
	default:
		printf("Error: Unknown SFP module type 0x%x.  Please see SFF-8472 table 3.4 for definition\n",
		       sfp_buffer[2]);
		return -1;
	}
	memcpy(&tx_id, &sfp_buffer[3], sizeof(tx_id));
	if (mode == VITESSE_FUNC_MODE_LIMITING) {
		if (sfp_buffer[3] & 0x80)
			debug("%s SFP cap 10G Base-ER\n", ethdev->name);
		if (sfp_buffer[3] & 0x40)
			debug("%s SFP cap 10G Base-LRM\n", ethdev->name);
		if (sfp_buffer[3] & 0x20)
			debug("%s SFP cap 10G Base-LR\n", ethdev->name);
		if (sfp_buffer[3] & 0x10)
			debug("%s SFP cap 10G Base-SR\n", ethdev->name);
		if (!(sfp_buffer[3] & 0xF0)) {
			printf("Error: unknown SFP ID 0x%016llx for %s\n",
			       tx_id, ethdev->name);
			puts("SFP+ does not support Ethernet.\n");
			return -1;
		}
	} else if (mode == VITESSE_FUNC_MODE_COPPER) {
		if (sfp_buffer[8] & 0x4) {
			debug("%s SFP is passive copper\n", ethdev->name);
			if (sfp_buffer[6] & 0x4) {
				debug("%s SFP+ supports Twinax\n", ethdev->name);
			} else if (sfp_buffer[6] & 0x8) {
				debug("%s SFP+ supports 1000Base-T", ethdev->name);
			}
		} else if(sfp_buffer[8] & 0x8) {
			debug("%s SFP is active copper\n", ethdev->name);
			mode = 2;	/* Active copper uses optical mode */
		} else {
			printf("Error: Cannot detect if copper SFP+ is active or passive for %s\n",
			       ethdev->name);
			printf("SFP+ id is 0x%016llx\n", tx_id);
			return -1;
		}
	} else {
		printf("Unknown mode %d for %s\n", mode, ethdev->name);
		printf("SFP+ id is 0x%016llx\n", tx_id);
		return -1;
	}

	debug("%s: Setting PHY to mode %d (%s) on %s\n",
	      __func__, mode, mode_str, ethdev->name);

	/* Wait for firmware download to complete */
	timeout = 100000;
	do {
		val = phy_read(phydev, 0x1e, 0x7fe0);
		if (val == 3)
			break;
		udelay(100);
	} while (timeout-- > 0);
	if (timeout <= 0) {
		printf("Error waiting for PHY firmware download to complete on %s, reg: 0x%04x\n",
		       ethdev->name, val);
		return -1;
	}

	/* Program the mode into the PHY */
	if (!fdt_node_check_compatible(gd->fdt_blob, phy_offset,
	    "vitesse,vsc8488"))
		addr_mask = 1;
	if (!fdt_node_check_compatible(gd->fdt_blob, phy_offset,
	    "vitesse,vsc8484"))
		addr_mask = 3;

	vitesse_write_global_var(phydev, phydev->addr & addr_mask, 0x94, mode);

	/* Adjust PMA_TXOUTCTRL2 based on cable length for copper */
	if (mode == VITESSE_FUNC_MODE_COPPER) {
		if (sfp_buffer[18] >= 5) {
			/* Optimize for 5M and longer cables */
			phy_write(phydev, 1, 0x8014, 0x1606);
		}
	}

	/* Reset the state machine */
	phy_write(phydev, 1, 0x8034, 0x11);


#ifdef DEBUG
	for (i = 0; i < 4; i++)
		vitesse_read_global_var(phydev, i, 0x94);
#endif

	return 0;
}
#endif
/**
 * Configures all Vitesse VCS8488 devices found.
 *
 * @return 0 for success, error otherwise.
 */
int octeon_vcs8488_config(void)
{
	struct eth_device *start = eth_get_dev();
	struct eth_device *ethdev = start;
	struct octeon_eth_info *oct_eth_info;

	if (!start) {
		debug("%s: No Ethernet devices found\n", __func__);
		return 0;
	}
	do {
		oct_eth_info = (struct octeon_eth_info *)ethdev->priv;
		if (oct_eth_info->phy_device_type == VITESSE) {
			if (octeon_fdt_vitesse_config(ethdev)) {
				printf("%s: Error configuring Ethernet device %s\n",
				       __func__, ethdev->name);
				return -1;
			}
		}
		ethdev = ethdev->next;
	} while (ethdev && ethdev != start);
	return 0;
}

/**
 * This function tunes the QLM for use by the Vitesse VCS8488 PHY.
 *
 * @param qlm - qlm number to tune
 */
void octeon_board_vcs8488_qlm_tune(int qlm)
{
	if (!OCTEON_IS_MODEL(OCTEON_CN66XX) &&
	    !OCTEON_IS_MODEL(OCTEON_CN61XX)) {
		printf("%s: Not supported on this OCTEON model yet\n", __func__);
		return;
	}
	cvmx_qlm_jtag_set(qlm, -1, "tcoeff_hf_ls_byp", 0);
	cvmx_qlm_jtag_set(qlm, -1, "tcoeff_hf_byp", 0);
	cvmx_qlm_jtag_set(qlm, -1, "tcoeff_lf_ls_byp", 0);
	cvmx_qlm_jtag_set(qlm, -1, "tcoeff_lf_byp", 0);
	cvmx_qlm_jtag_set(qlm, -1, "rx_eq_gen1", 0);
	cvmx_qlm_jtag_set(qlm, -1, "rx_eq_gen2", 0);
	cvmx_qlm_jtag_set(qlm, -1, "rx_cap_gen1", 0);
	cvmx_qlm_jtag_set(qlm, -1, "rx_cap_gen2", 0);
	cvmx_qlm_jtag_set(qlm, -1, "biasdrv_hs_ls_byp", 31);
	cvmx_qlm_jtag_set(qlm, -1, "biasdrv_hf_byp", 31);
	cvmx_qlm_jtag_set(qlm, -1, "biasdrv_lf_ls_byp", 31);
	cvmx_qlm_jtag_set(qlm, -1, "biasdrv_lf_byp", 31);
	/* Assert serdes_tx_byp to force the new settings to override the
	   QLM default. */
	cvmx_qlm_jtag_set(qlm, -1, "serdes_tx_byp", 1);
}

/**
 * Initializes all of the PHYs on a board using the device tree information.
 *
 * @return 0 for success, error otherwise
 */
int octeon_board_phy_init(void)
{
	struct eth_device *ethdev, *start;
	struct octeon_eth_info *oct_eth_info;
	int err = 0;

	start = eth_get_dev();
	if (!start)
		return 0;

	ethdev = start;
	do {
		oct_eth_info = (struct octeon_eth_info *)ethdev->priv;

		debug("%s: Initializing %s\n", __func__, ethdev->name);
		if (oct_eth_info->phydev) {
			switch (oct_eth_info->phy_device_type) {
			case MARVELL:
				if ((err |= octeon_fdt_marvell_config(ethdev)))
					printf("Error configuring Marvell PHY for %s\n",
					       ethdev->name);
				break;

			case BROADCOM:
				if ((err |= octeon_fdt_broadcom_config(ethdev)))
					printf("Error configuring Broadcom PHY for %s\n",
					       ethdev->name);
				break;

			case VITESSE:
				err |= octeon_fdt_vitesse_config(ethdev);
#ifdef CONFIG_PHYLIB_10G
				err |= octeon_vitesse_sfp_config(ethdev);
#endif
				if (err)
					printf("Error configuring Vitesse PHY for %s\n",
					       ethdev->name);
				break;

			default:
				break;
			}
		}
		ethdev = ethdev->next;
	} while (ethdev != start);

	return err;
}
