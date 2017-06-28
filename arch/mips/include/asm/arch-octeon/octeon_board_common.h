/***********************license start************************************
 * Copyright (c) 2012 Cavium Inc. (support@cavium.com). All rights
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
 * support@cavium.com
 *
 ***********************license end**************************************/
#ifndef __OCTEON_BOARD_COMMON_H__
#define __OCTEON_BOARD_COMMON_H__

#include <asm/arch/cvmx-app-init.h>

/* NOTE: All of these functions are weak and may be overwritten */

/**
 * Dynamically adjust the board name, used for prompt generation
 * @param name - name string to be adjusted
 * @param max_len - maximum length of name
 *
 * This function can overwrite the name of a board, for example based on
 * the processor installed.
 */
void octeon_adjust_board_name(char *name, size_t max_len);

/**
 * Generates a random MAC address using the board revision and serial string
 *
 * NOTE: Only the 5th byte is random.
 */
void octeon_board_create_random_mac_addr(void);

/**
 * Extracts and fills in the GD MAC address from the TLV EEPROM.  If not
 * found in the TLV EEPROM then it calls octeon_create_random_mac_addr().
 */
void octeon_board_get_mac_addr(void);

/**
 * Gets the board clock info from the TLV EEPROM or uses the default value
 * if not available.
 *
 * @param def_ddr_clock_mhz	Default DDR clock speed in MHz
 */
void octeon_board_get_clock_info(uint32_t def_ddr_clock_mhz);

/**
 * Reads the board descriptor from the TLV EEPROM or fills in the default
 * values.
 *
 * @param type		board type
 * @param rev_major	board major revision
 * @param rev_minor	board minor revision
 */
void octeon_board_get_descriptor(enum cvmx_board_types_enum type,
				 int rev_major, int rev_minor);

/**
 * Function to write string to LED display
 * @param str - string up to 8 characters to write.
 */
void octeon_led_str_write(const char *str);

/**
 * Initializes basic MDIO support for OCTEON.  This function
 * must be called before using any miiphy_read/write calls.
 *
 * @return 0 for success, otherwise error
 */
int octeon_mdio_initialize(void);

#ifdef CONFIG_OCTEON_ENABLE_PAL
/**
 * Display information provided by the Cavium on-board PAL device.
 */
void show_pal_info(void);
#endif

/**
 * User-defined function called just before initializing networking support
 */
void board_net_preinit(void);

/**
 * User-defined function called just after initializing networking and MDIO
 * support
 */
void board_net_postinit(void);

/**
 * User-defined function called to initialize the PHY devices for a board.
 */
void board_mdio_init(void);

#endif /* __OCTEON_BOARD_COMMON_H__ */