/*
 *    Copyright (C) 2012 Cavium, Inc.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 */
#ifndef __CORTINA_CS4321_H__
#define __CORTINA_CS4321_H__

/*
 * The Cortina CS4321 PHY needs to know what mode the host is in.  Currently,
 * this PHY supports XAUI, RXAUI, SGMII and several FC types (which we won't
 * bother with yet).
 *
 * In order to initialize the PHY properly we need to be able to pass it
 * the mode.  Since the phy_device flags field is not used, we will pass
 * this info there.
 */

enum cortina_cs4321_host_mode {
	CS4321_HOST_MODE_UNKNOWN,
	CS4321_HOST_MODE_SGMII,
	CS4321_HOST_MODE_XAUI,
	CS4321_HOST_MODE_RXAUI
};

/**
 * User-defined function used to figure out the host mode to use for a
 * phy device.
 *
 * @param[in] phydev - PHY device data structure to check
 *
 * @return mode to be used for the specified PHY device
 */
enum cortina_cs4321_host_mode
cortina_cs4321_get_host_mode(const struct phy_device *phydev);
#endif