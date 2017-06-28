/***********************license start************************************
 * Copyright (c) 2005-2012 Cavium, Inc. (support@cavium.com). All rights
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
 * marketing@cavium.com
 *
 ***********************license end**************************************/

/**
 *
 * $Id: octeon_eth.c 84350 2013-05-16 19:08:32Z cchavva $
 *
 */

#include <common.h>
#include <exports.h>
#include <malloc.h>
#include <linux/ctype.h>
#include <asm/arch/lib_octeon.h>
#include <asm/arch/octeon_eeprom_types.h>
#include <net.h>
#include <phy.h>
#include <miiphy.h>
#include <asm/arch/octeon_eth.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/octeon_eeprom_types.h>
#include <asm/arch/lib_octeon_shared.h>
#include <asm/arch/cvmx-config.h>
#include <asm/arch/cvmx-wqe.h>
#include <asm/arch/cvmx-pow.h>
#include <asm/arch/cvmx-pko.h>
#include <asm/arch/cvmx-ipd.h>
#include <asm/arch/cvmx-pip.h>
#include <asm/arch/cvmx-spi.h>
#include <asm/arch/cvmx-mdio.h>
#include <asm/arch/cvmx-helper.h>
#include <asm/arch/cvmx-mgmt-port.h>
#include <asm/arch/cvmx-helper-cfg.h>
#include <asm/arch/cvmx-helper-util.h>
#include <asm/arch/cvmx-bootmem.h>
#include <asm/arch/cvmx-global-resources.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * Enables RX packet debugging if octeon_debug_rx_packets is set in the
 * environment.
 */
#define DEBUG_RX_PACKET

/**
 * Enables TX packet debugging if octeon_debug_tx_packets is set in the
 * environment.
 */
#define DEBUG_TX_PACKET


/* Global flag indicating common hw has been set up */
int octeon_global_hw_inited = 0;

int octeon_eth_send(struct eth_device *dev, void *packet, int length);
int octeon_ebt3000_rgmii_init(struct eth_device *dev, bd_t * bis);
void octeon_eth_rgmii_enable(struct eth_device *dev);
int octeon_eth_write_hwaddr(struct eth_device *dev);

/************************************************************************/
#if  defined(OCTEON_INTERNAL_ENET) || defined(OCTEON_MGMT_ENET)

/* Variable shared between ethernet routines. */
static int card_number;
static int available_mac_count = -1;
static uint8_t mac_addr[6];
#if defined(DEBUG_RX_PACKET)  || defined(DEBUG_TX_PACKET)
static int packet_rx_debug = 0, packet_tx_debug = 0;
#endif

/* Make sure that we have enough buffers to keep prefetching blocks happy.
 * Absolute minimum is probably about 200.
 */
#define NUM_PACKET_BUFFERS  1000

#define PKO_SHUTDOWN_TIMEOUT_VAL     100
/****************************************************************/

/**
 * Searches for an ethernet device based on interface and index.
 *
 * @param interface - interface number to search for
 * @param index - index to search for
 *
 * @returns pointer to ethernet device or NULL if not found.
 */
struct eth_device *octeon_find_eth_by_interface_index(int interface, int index)
{
	struct eth_device *ethdev, *start;
	struct octeon_eth_info *oct_eth_info;

	start = eth_get_dev();
	if (!start)
		return NULL;
	ethdev = start;
	do {
		oct_eth_info = (struct octeon_eth_info *)ethdev->priv;
		if ((oct_eth_info->interface == interface) &&
		    (oct_eth_info->index == index))
			return oct_eth_info->ethdev;
		ethdev = ethdev->next;
	} while (ethdev && ethdev != start);
	return NULL;
}

static void cvm_oct_fill_hw_memory(uint64_t pool, uint64_t size,
				   uint64_t elements)
{

	int64_t memory;
	static int alloc_count = 0;
	char tmp_name[64];

	debug("cvm_oct_fill_hw_memory: pool: 0x%llx, size: 0xx%llx, "
	      "count: 0x%llx\n", pool, size, elements);
	sprintf(tmp_name, "%s_fpa_alloc_%d",
		OCTEON_BOOTLOADER_NAMED_BLOCK_TMP_PREFIX, alloc_count++);
	memory = cvmx_bootmem_phy_named_block_alloc(size * elements, 0,
						    0x40000000, 128, tmp_name,
						    CVMX_BOOTMEM_FLAG_END_ALLOC);
	if (memory < 0) {
		printf("Unable to allocate %llu bytes for FPA pool %llu\n",
		       elements * size, pool);
		return;
	}
	while (elements--)
		cvmx_fpa_free((void *)(uint32_t) (memory + elements * size),
			      pool, 0);
}

/**
 * Converts the interface mode to the phy mode
 *
 * @param if_mode - interface mode
 *
 * @return phy mode
 */
static phy_interface_t
cvm_if_mode_to_phy_mode(cvmx_helper_interface_mode_t if_mode)
{
	switch (if_mode) {
	case CVMX_HELPER_INTERFACE_MODE_RGMII:
		return PHY_INTERFACE_MODE_RGMII;
	case CVMX_HELPER_INTERFACE_MODE_GMII:
		return PHY_INTERFACE_MODE_GMII;
	case CVMX_HELPER_INTERFACE_MODE_SGMII:
		return PHY_INTERFACE_MODE_SGMII;
	case CVMX_HELPER_INTERFACE_MODE_RXAUI:
	case CVMX_HELPER_INTERFACE_MODE_XAUI:
		return PHY_INTERFACE_MODE_XGMII;
	default:
		return PHY_INTERFACE_MODE_NONE;
	}
}

/**
 * Set the hardware MAC address for a device
 *
 * @param interface    interface of port to set
 * @param index    index of port to set MAC address for
 * @param addr   Address structure to change it too.
 * @return Zero on success
 */
static int cvm_oct_set_mac_address(int interface, int index, void *addr)
{
	cvmx_gmxx_prtx_cfg_t gmx_cfg;

	int i;
	uint8_t *ptr = addr;
	uint64_t mac = 0;
	for (i = 0; i < 6; i++)
		mac = (mac << 8) | (uint64_t) (ptr[i]);

	/* Disable interface */
	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface),
		       gmx_cfg.u64 & ~1ull);

	cvmx_write_csr(CVMX_GMXX_SMACX(index, interface), mac);
	cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM0(index, interface), ptr[0]);
	cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM1(index, interface), ptr[1]);
	cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM2(index, interface), ptr[2]);
	cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM3(index, interface), ptr[3]);
	cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM4(index, interface), ptr[4]);
	cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM5(index, interface), ptr[5]);

	cvmx_gmxx_rxx_adr_ctl_t control;
	control.u64 = 0;
	control.s.bcst = 1;	/* Allow broadcast MAC addresses */
	control.s.mcst = 1;	/* Force reject multicast packets */
	control.s.cam_mode = 1;	/* Filter packets based on the CAM */
	cvmx_write_csr(CVMX_GMXX_RXX_ADR_CTL(index, interface), control.u64);

	cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM_EN(index, interface), 1);

	/* Return interface to previous enable state */
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64);

	return 0;
}

/**
 * Configure common hardware for all interfaces
 */
static void cvm_oct_configure_common_hw(void)
{

	if (getenv("disable_spi")) {

		/* Do this here so that we can disabled this after boot but
		 * before we use networking.
		 */
		cvmx_spi_callbacks_t spi_callbacks_struct;
		cvmx_spi_get_callbacks(&spi_callbacks_struct);
		spi_callbacks_struct.reset_cb = 0;
		spi_callbacks_struct.calendar_setup_cb = 0;
		spi_callbacks_struct.clock_detect_cb = 0;
		spi_callbacks_struct.training_cb = 0;
		spi_callbacks_struct.calendar_sync_cb = 0;
		spi_callbacks_struct.interface_up_cb = 0;
		printf("SPI interface disabled with 'disable_spi' environment "
		       "variable\n");
		cvmx_spi_set_callbacks(&spi_callbacks_struct);
	}
	/* Setup the FPA */
	cvmx_fpa_enable();

	cvm_oct_fill_hw_memory(CVMX_FPA_WQE_POOL, CVMX_FPA_WQE_POOL_SIZE,
			       NUM_PACKET_BUFFERS);
#if CVMX_FPA_OUTPUT_BUFFER_POOL != CVMX_FPA_PACKET_POOL
	cvm_oct_fill_hw_memory(CVMX_FPA_OUTPUT_BUFFER_POOL,
			       CVMX_FPA_OUTPUT_BUFFER_POOL_SIZE, 128);
#endif
	cvm_oct_fill_hw_memory(CVMX_FPA_PACKET_POOL, CVMX_FPA_PACKET_POOL_SIZE,
			       NUM_PACKET_BUFFERS);

	cvmx_helper_initialize_packet_io_global();
	cvmx_helper_initialize_packet_io_local();

	/* Set POW get work timeout to maximum value */
	if (octeon_has_feature(OCTEON_FEATURE_CN68XX_WQE))
		cvmx_write_csr(CVMX_SSO_NW_TIM, 0x3ff);
	else
		cvmx_write_csr(CVMX_POW_NW_TIM, 0x3ff);
}

/**
 * Packet transmit
 *
 * @param skb    Packet to send
 * @param dev    Device info structure
 * @return Always returns zero
 */
static int cvm_oct_xmit(struct eth_device *dev, void *packet, int len)
{
	struct octeon_eth_info *priv = (struct octeon_eth_info *) dev->priv;
	cvmx_pko_command_word0_t pko_command;
	cvmx_buf_ptr_t hw_buffer;
	int rv;
	int queue = cvmx_pko_get_base_queue(priv->port);

	debug("cvm_oct_xmit addr: %p, len: %d\n", packet, len);

	/* Build the PKO buffer pointer */
	hw_buffer.u64 = 0;
	hw_buffer.s.addr = cvmx_ptr_to_phys(packet);
	hw_buffer.s.pool = CVMX_FPA_PACKET_POOL;
	hw_buffer.s.size = CVMX_FPA_PACKET_POOL_SIZE;

	/* Build the PKO command */
	pko_command.u64 = 0;
	pko_command.s.subone0 = 1;
	pko_command.s.dontfree = 0;
	pko_command.s.segs = 1;
	pko_command.s.total_bytes = len;
	/* Send the packet to the output queue */

	debug("cvm_oct_xmit port: %d, queue: %d\n", priv->port, queue);
	cvmx_pko_send_packet_prepare(priv->port, queue, 0);
	rv = cvmx_pko_send_packet_finish (priv->port, queue, pko_command,
					  hw_buffer, 0);
	if (rv) {
		printf("Failed to send the packet rv=%d\n", rv);
	}
	return 0;
}

#ifdef CONFIG_PHYLIB_10G
extern int octeon_vitesse_sfp_config(struct eth_device *dev);
#endif
static void cvm_oct_phy_port_check(struct eth_device *dev)
{
#ifdef CONFIG_PHYLIB_10G
	struct octeon_eth_info *oct_eth_info;

	oct_eth_info = (struct octeon_eth_info *)dev->priv;
	if (oct_eth_info->phy_device_type == VITESSE)
		octeon_vitesse_sfp_config(dev);
#endif
}

/**
 * Configure the RGMII port for the negotiated speed
 *
 * @param dev    Linux device for the RGMII port
 */
static void cvm_oct_configure_rgmii_speed(struct eth_device *dev)
{
	struct octeon_eth_info *priv = (struct octeon_eth_info *)(dev->priv);
	int port = priv->port;

	cvmx_helper_link_info_t link_state = cvmx_helper_link_get(port);

	/* If the port is down some PHYs we need to check modules, etc. */
	if (!link_state.s.link_up)
		cvm_oct_phy_port_check(dev);

	if (link_state.u64 != priv->link_state) {

		cvmx_helper_interface_mode_t mode;
		printf("%s: ", dev->name);
		if (!link_state.s.link_up)
			puts("Down ");
		else {
			cvm_oct_phy_port_check(dev);
			printf("Up %d Mbps ", link_state.s.speed);
			if (link_state.s.full_duplex)
				puts("Full duplex ");
			else
				puts("Half duplex ");
		}
		mode = cvmx_helper_interface_get_mode(priv->interface);
		printf("(port %2d) (%s)\n", port,
			cvmx_helper_interface_mode_to_string(mode));
		cvmx_helper_link_set(priv->port, link_state);
		priv->link_state = link_state.u64;
	}
}

#if defined(OCTEON_MGMT_ENET)
/**
 * Resets the specified management port
 *
 * @param port - port to reset
 *
 * @return 0 for success, -1 on error.
 */
static int octeon_mgmt_port_reset_port(int port)
{
	cvmx_mixx_ctl_t mix_ctl;
	if (OCTEON_IS_MODEL(OCTEON_CN52XX) && (port < 0 || port > 1))
		return -1;
	else if (OCTEON_IS_MODEL(OCTEON_CN56XX) && (port < 0 || port > 0))
		return -1;
	else if (OCTEON_IS_MODEL(OCTEON_CN61XX) && (port < 0 || port > 1))
		return -1;
	else if (OCTEON_IS_MODEL(OCTEON_CN63XX) && (port < 0 || port > 1))
		return -1;
	else if (OCTEON_IS_MODEL(OCTEON_CN66XX) && (port < 0 || port > 1))
		return -1;
	else if (OCTEON_IS_MODEL(OCTEON_CN68XX) && (port < 0 || port > 0))
		return -1;

	mix_ctl.u64 = cvmx_read_csr(CVMX_MIXX_CTL(port));
	mix_ctl.s.en = 0;
	cvmx_write_csr(CVMX_MIXX_CTL(port), mix_ctl.u64);
	do {
		mix_ctl.u64 = cvmx_read_csr(CVMX_MIXX_CTL(port));
	} while (mix_ctl.s.busy);
	mix_ctl.s.reset = 1;
	cvmx_write_csr(CVMX_MIXX_CTL(port), mix_ctl.u64);
	cvmx_read_csr(CVMX_MIXX_CTL(port));
	return 0;
}

/**
 * Really shutdown the management ports, including resetting the hardware
 * units.  This is needed for NAND boot stage2 -> stage3 handoff.  The MGMT
 * ports need to be reset before they are handed off to stage 3.
 * This is not needed with newer MGMT init code that properly handles this
 * case at init time.
 */
int octeon_mgmt_port_shutdown(void)
{
	octeon_mgmt_port_reset_port(0);
	octeon_mgmt_port_reset_port(1);
	cvmx_bootmem_free_named("cvmx_mgmt_port");
	return 0;
}
#endif

/**
 * Shuts down the network hardware before loading new applications
 * or operating systems.
 */
int octeon_network_hw_shutdown(void)
{
	int retval = 0;
	int i, count;

	if (octeon_global_hw_inited) {
		retval = cvmx_helper_shutdown_packet_io_global();
		cvmx_helper_shutdown_packet_io_local();
		/* Free FPA buffers from all the pools before disabling */
		for (i = 0; i < 8; i++) {
			count = 0;
			while (cvmx_fpa_alloc(i))
				count++;
			debug("Freed %d buffers from pool %d\n", count, i);
		}
		/* Now disable the FPA, except on cn68xx, causes SS0 to
                   malfunction on subsequent initialization. */
	        if (!octeon_has_feature(OCTEON_FEATURE_CN68XX_WQE))
		    cvmx_fpa_disable();
		/* Spurious FPE errors will happen doing this cleanup. */
		if (OCTEON_IS_MODEL(OCTEON_CN68XX)) {
			cvmx_sso_err_t sso_err;
			sso_err.u64 = cvmx_read_csr(CVMX_SSO_ERR);
			sso_err.s.fpe = 1;
			cvmx_write_csr(CVMX_SSO_ERR, sso_err.u64);
		}
	}

	free_global_resources();
	/* Free command queue named block, as we have reset the hardware */
	cvmx_bootmem_phy_named_block_free("cvmx_cmd_queues", 0);
	if (retval < 0) {
		/* Make this a fatal error since the error message is easily
		 * missed and ignoring it can lead to very strange networking
		 * behavior in the application.
		 */
		printf("FATAL ERROR: Network shutdown failed.  "
		       "Please reset the board.\n");
		while (1)
			;
	}
	return retval;
}

/*******************  Octeon networking functions ********************/
int octeon_ebt3000_rgmii_init(struct eth_device *dev, bd_t * bis)
{				/* Initialize the device  */
	struct octeon_eth_info *priv = (struct octeon_eth_info *)(dev->priv);
	int interface = priv->interface;
	debug("%s(), dev_ptr: %p, dev: %s, port: %d\n",
	      __func__, dev, dev->name, priv->port);

	if (priv->initted_flag) {
		debug("%s already initialized\n", dev->name);
		return 1;
	}

	if (!octeon_global_hw_inited) {
		debug("Initializing common hardware\n");
		cvm_oct_configure_common_hw();
	}

	/* Ignore backpressure on RGMII ports */
	cvmx_write_csr(CVMX_GMXX_TX_OVR_BP(interface), 0xf << 8 | 0xf);

	cvm_oct_set_mac_address(interface, priv->port & 0x3, dev->enetaddr);

	if (!octeon_global_hw_inited) {
		debug("Enabling packet input\n");
		cvmx_helper_ipd_and_packet_input_enable();
		octeon_global_hw_inited = 1;
	}
	priv->enabled = 0;
	priv->initted_flag = 1;

	return (1);
}

#ifdef OCTEON_SPI4000_ENET
/**
 * Initializes the SPI4000 interface and devices
 *
 * @param dev - ethernet device to init
 *
 * @param bis - pointer to bd_t data structure
 *
 * @return 1 for success, 0 for error
 */
int octeon_spi4000_init(struct eth_device *dev, bd_t * bis)
{				/* Initialize the device  */
	struct octeon_eth_info *priv = (struct octeon_eth_info *)(dev->priv);
	static char spi4000_inited[2] = { 0 };
	debug("octeon_spi4000_init(), dev_ptr: %p, port: %d\n",
	      dev, priv->port);

	if (priv->initted_flag)
		return 1;

	if (!octeon_global_hw_inited) {
		cvm_oct_configure_common_hw();
	}

	if (!spi4000_inited[priv->interface]) {
		spi4000_inited[priv->interface] = 1;
		if (__cvmx_helper_spi_enable(priv->interface) < 0) {
			printf("ERROR initializing spi4000 on Octeon "
			       "Interface %d\n", priv->interface);
			return 0;
		}
	}

	if (!octeon_global_hw_inited) {
		cvmx_helper_ipd_and_packet_input_enable();
		octeon_global_hw_inited = 1;
	}
	priv->initted_flag = 1;
	return 1;
}
#endif

/**
 * Enables a SGMII interface
 *
 * @param dev - Ethernet device to initialize
 */
static void octeon_eth_sgmii_enable(struct eth_device *dev)
{
	struct octeon_eth_info *oct_eth_info;
	cvmx_gmxx_prtx_cfg_t gmx_cfg;
	int index, interface;
	cvmx_helper_interface_mode_t if_mode;

	oct_eth_info = (struct octeon_eth_info *) dev->priv;

	interface = oct_eth_info->interface;

	index = oct_eth_info->index;

	if_mode = cvmx_helper_interface_get_mode(interface);
	/* Normal operating mode. */

	if (if_mode == CVMX_HELPER_INTERFACE_MODE_SGMII) {
		cvmx_pcsx_miscx_ctl_reg_t pcsx_miscx_ctl_reg;
		pcsx_miscx_ctl_reg.u64 =
			cvmx_read_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface));
		pcsx_miscx_ctl_reg.s.gmxeno = 0;
		cvmx_write_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface),
			       pcsx_miscx_ctl_reg.u64);
	} else {
		cvmx_pcsxx_misc_ctl_reg_t pcsxx_misc_ctl_reg;
		pcsxx_misc_ctl_reg.u64 =
			cvmx_read_csr(CVMX_PCSXX_MISC_CTL_REG(interface));
		pcsxx_misc_ctl_reg.s.gmxeno = 0;
		cvmx_write_csr(CVMX_PCSXX_MISC_CTL_REG(interface),
			       pcsxx_misc_ctl_reg.u64);
	}

	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
	gmx_cfg.s.en = 1;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64);
	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
}

/**
 * Enables an RGMII interface
 *
 * @param dev - Ethernet device to enable
 */
void octeon_eth_rgmii_enable(struct eth_device *dev)
{
	struct octeon_eth_info *oct_eth_info;
	uint64_t tmp;
	int interface;
	cvmx_helper_interface_mode_t if_mode;

	oct_eth_info = (struct octeon_eth_info *) dev->priv;

	interface = oct_eth_info->interface;
	if_mode = cvmx_helper_interface_get_mode(interface);

	switch (if_mode) {
	case CVMX_HELPER_INTERFACE_MODE_RGMII:
	case CVMX_HELPER_INTERFACE_MODE_GMII:
		tmp = cvmx_read_csr(CVMX_ASXX_RX_PRT_EN(interface));
		tmp |= (1ull << (oct_eth_info->port & 0x3));
		cvmx_write_csr(CVMX_ASXX_RX_PRT_EN(interface), tmp);
		tmp = cvmx_read_csr(CVMX_ASXX_TX_PRT_EN(interface));
		tmp |= (1ull << (oct_eth_info->port & 0x3));
		cvmx_write_csr(CVMX_ASXX_TX_PRT_EN(interface), tmp);
		octeon_eth_write_hwaddr(dev);
		break;

	case CVMX_HELPER_INTERFACE_MODE_SGMII:
	case CVMX_HELPER_INTERFACE_MODE_XAUI:
	case CVMX_HELPER_INTERFACE_MODE_RXAUI:
		octeon_eth_sgmii_enable(dev);
		octeon_eth_write_hwaddr(dev);
		break;

	default:
		return;
	}
}

/**
 * Keep track of packets sent on each interface so we can
 * autonegotiate before we send the first one.  This is cleared in
 * eth_try_another.
 */
int packets_sent;

#if defined(DEBUG_TX_PACKET) || defined(DEBUG_RX_PACKET)
static void print_mac(const char *label, const uint8_t *mac_addr)
{
	printf("%s: %02x:%02x:%02x:%02x:%02x:%02x", label,
	       mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
	       mac_addr[5]);
}

static void print_ip(const void *packet)
{
	uint8_t *p = (uint8_t *)packet;
	uint16_t length;
	uint8_t hdr_len;

	puts("IP Header:\n");
	if ((p[0] & 0xF0) != 0x40) {
		printf("Invalid IP version %d\n", *p >> 4);
		return;
	}
	hdr_len = *p & 0x0F;
	if (hdr_len < 5)
		printf("Invalid IP header length %d\n", hdr_len);
	printf("\tVersion: 4, Header length: %d\n", hdr_len);
	length = (p[2] << 8) | p[3];
	printf("\tTOS: 0x%02x, length: %d\n", p[1], length);
	printf("\tID: %d, %s%s%s fragment offset: %d\n", (p[4] << 8) | p[5],
	       p[6] & 0x80 ? "congested, " : "", p[6] & 0x40 ? "DF, " : "",
	       p[6] & 0x20 ? "MF, " : "", ((p[6] & 0x1F) << 8) | p[7]);
	printf("\tTTL: %d, Protocol: %d, Header Checksum: 0x%x\n", p[8], p[9],
	       (p[10] << 8) | p[11]);
	printf("\tSource IP: %d.%d.%d.%d\n\tDestination IP: %d.%d.%d.%d\n\n",
	       p[12], p[13], p[14], p[15], p[16], p[17], p[18], p[19]);
}

/**
 * Prints out a packet for debugging purposes
 *
 * @param[in]	packet - pointer to packet data
 * @param	length - length of packet in bytes
 */
static void print_packet(const void *packet, int length)
{
	int i, j;
	const unsigned char *up = packet;
	uint16_t type = (up[12] << 8 | up[13]);
	int start = 14;

	print_mac("DMAC", &up[0]);
	puts("    ");
	print_mac("SMAC", &up[6]);
	printf("    TYPE: %04x\n", type);

	if (type == 0x0800)
		print_ip(&up[start]);

	for (i = start; (i + 16) < length; i += 16) {
		printf("%04x ", i);
		for (j = 0; j < 16; ++j)
			printf("%02x ", up[i + j]);

		printf("    ");
		for (j = 0; j < 16; ++j)
			printf("%c", ((up[i + j] >= ' ')
				      && (up[i + j] <= '~')) ? up[i + j] : '.');
		printf("\n");
	}
	printf("%04x ", i);
	for (j = 0; i + j < length; ++j)
		printf("%02x ", up[i + j]);

	for (; j < 16; ++j)
		printf("   ");

	printf("    ");
	for (j = 0; i + j < length; ++j)
		printf("%c", ((up[i + j] >= ' ')
			      && (up[i + j] <= '~')) ? up[i + j] : '.');

	printf("\n");
}
#endif

/**
 * Send a packet out an Ethernet device
 *
 * @param dev - Ethernet device to send on
 * @param packet - pointer to packet to send
 * @param length - length of packet to send
 *
 * @return 0 for success
 */
int octeon_eth_send(struct eth_device *dev, void *packet, int length)
{				/* Send a packet  */
	struct octeon_eth_info *oct_eth_info =
					 (struct octeon_eth_info *)dev->priv;

	debug("ethernet TX! ptr: %p, len: %d\n", packet, length);
	/* If we haven't been initialized, do it now. */
	if (!oct_eth_info->initted_flag) {
		debug("Initializing %s in send\n", dev->name);
		dev->init(dev, gd->bd);
	}
	if (!oct_eth_info->enabled) {
		oct_eth_info->enabled = 1;
		octeon_eth_rgmii_enable(dev);
	}
	/* We need to copy this to a FPA buffer, then give that to TX */

	if (oct_eth_info->packets_sent == 0)
		cvm_oct_configure_rgmii_speed(dev);

	void *fpa_buf = cvmx_fpa_alloc(CVMX_FPA_PACKET_POOL);
	if (!fpa_buf) {
		printf("ERROR allocating buffer for packet!\n");
		return -1;
	}
	memcpy(fpa_buf, (void *)packet, length);
#ifdef DEBUG_TX_PACKET
	if (packet_tx_debug) {
		printf("\nTX packet: interface: %d, index: %d\n",
		       oct_eth_info->interface, oct_eth_info->index);
		print_packet(packet, length);
	}
#endif
	cvm_oct_xmit(dev, fpa_buf, length);
	oct_eth_info->packets_sent++;
	return 0;
}

/**
 * Called to receive a packet
 *
 * @param dev - device to receive on
 *
 * @return - length of packet
 *
 * This function is used to poll packets.  In turn it calls NetReceive
 * to process the packets.
 */
int octeon_eth_recv(struct eth_device *dev)
{				/* Check for received packets     */
	cvmx_wqe_t *work = cvmx_pow_work_request_sync(1);
	struct octeon_eth_info *oct_eth_info =
					(struct octeon_eth_info *)dev->priv;

	/* If we haven't been initialized, do it now. */
	if (!oct_eth_info->initted_flag) {
		debug("Initializing %s in recv\n", dev->name);
		dev->init(dev, gd->bd);
	}

	if (!oct_eth_info->enabled) {
		oct_eth_info->enabled = 1;
		octeon_eth_rgmii_enable(dev);
	}
	if (!work) {
		/* Poll for link status change, only if work times out.
		 * On some interfaces this is really slow.
		 * getwork timeout is set to maximum.
		 */
		cvm_oct_configure_rgmii_speed(dev);
		return 0;
	}
	if (work->word2.s.rcv_error) {
		/* Work has error, so drop */
		printf("Error packet received (code %d), dropping\n",
		       work->word2.s.err_code);
		cvmx_helper_free_packet_data(work);
		cvmx_fpa_free(work, CVMX_FPA_WQE_POOL, 0);
		return 0;
	}

	void *packet_data =
		    cvmx_phys_to_ptr(work->packet_ptr.u64 & 0xffffffffffull);
	int length = work->word1.len;

	oct_eth_info->packets_received++;
	debug("############# got work: %p, len: %d, packet_ptr: %p\n",
	      work, length, packet_data);
#ifdef DEBUG_RX_PACKET
	if (packet_rx_debug) {
		printf("\nRX packet: interface: %d, index: %d\n",
		       oct_eth_info->interface, oct_eth_info->index);
		print_packet(packet_data, length);
	}
#endif
	NetReceive(packet_data, length);
	cvmx_helper_free_packet_data(work);
	cvmx_fpa_free(work, CVMX_FPA_WQE_POOL, 0);
	/* Free WQE and packet data */
	return length;
}

/**
 * Halts the specified Ethernet interface preventing it from receiving any more
 * packets.
 *
 * @param dev - Ethernet device to shut down.
 */
void octeon_eth_halt(struct eth_device *dev)
{
	uint64_t tmp;
	cvmx_gmxx_prtx_cfg_t gmx_cfg;
	struct octeon_eth_info *oct_eth_info =
				(struct octeon_eth_info *)dev->priv;
	int index = oct_eth_info->index;

	oct_eth_info->enabled = 0;
	/* stop SCC                       */
	/* Disable reception on this port at the GMX block */

	switch (cvmx_helper_interface_get_mode(oct_eth_info->interface)) {
	case CVMX_HELPER_INTERFACE_MODE_RGMII:
	case CVMX_HELPER_INTERFACE_MODE_GMII:
		tmp =
		    cvmx_read_csr(CVMX_ASXX_RX_PRT_EN(oct_eth_info->interface));
		tmp &= ~(1ull << index);
		/* Disable the RGMII RX ports */
		cvmx_write_csr(CVMX_ASXX_RX_PRT_EN(oct_eth_info->interface),
			       tmp);
		tmp =
		    cvmx_read_csr(CVMX_ASXX_TX_PRT_EN(oct_eth_info->interface));
		tmp &= ~(1ull << index);
		/* Disable the RGMII TX ports */
		cvmx_write_csr(CVMX_ASXX_TX_PRT_EN(oct_eth_info->interface),
			       tmp);
		/* No break! */
	case CVMX_HELPER_INTERFACE_MODE_SGMII:
	case CVMX_HELPER_INTERFACE_MODE_XAUI:
	case CVMX_HELPER_INTERFACE_MODE_RXAUI:
		/* Disable MAC filtering */
		index = oct_eth_info->index;

		gmx_cfg.u64 =
		    cvmx_read_csr(CVMX_GMXX_PRTX_CFG
				  (index, oct_eth_info->interface));
		cvmx_write_csr(CVMX_GMXX_PRTX_CFG
			       (index, oct_eth_info->interface),
			       gmx_cfg.u64 & ~1ull);
		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CTL
			       (index, oct_eth_info->interface), 1);
		cvmx_write_csr(CVMX_GMXX_RXX_ADR_CAM_EN
			       (index, oct_eth_info->interface), 0);
		cvmx_write_csr(CVMX_GMXX_PRTX_CFG
			       (index, oct_eth_info->interface), gmx_cfg.u64);
		break;
	default:
		break;
	}
}

/**
 * Sets the hardware MAC address of the Ethernet device
 *
 * @param dev - Ethernet device
 *
 * @return 0 for success
 */
int octeon_eth_write_hwaddr(struct eth_device *dev)
{
	struct octeon_eth_info *oct_eth_info;
	oct_eth_info = (struct octeon_eth_info *)dev->priv;

	/* Skip if the interface isn't yet enabled */
	if (!oct_eth_info->enabled) {
		debug("%s: Interface not enabled, not setting MAC address\n",
		      __func__);
		return 0;
	}
	debug("%s: Setting %s address to %02x:%02x:%02x:%02x:%02x:%02x\n",
	      __func__, dev->name,
	      dev->enetaddr[0], dev->enetaddr[1], dev->enetaddr[2],
	      dev->enetaddr[3], dev->enetaddr[4], dev->enetaddr[5]);
	return cvm_oct_set_mac_address(oct_eth_info->interface,
				       oct_eth_info->index,
				       dev->enetaddr);
}

/**
 * Get the PHY information from the FDT
 *
 * @param[in,out]	dev	- Ethernet device
 * @param		mgmt	- 1 for management port
 *
 * @return 0 for success, -1 for error.
 *
 */
static int octeon_eth_get_phy_info(struct eth_device *dev, int mgmt)
{
	uint32_t *phy_handle;
	struct octeon_eth_info *oct_eth_info =
					 (struct octeon_eth_info *)dev->priv;
	int aliases, eth, phy, phandle;
	const char *phy_compatible_str;
	char name_buffer[20];
	const char *path;

	oct_eth_info->phy_node_offset = -1;
	aliases = fdt_path_offset(gd->fdt_blob, "/aliases");
	if (aliases < 0) {
		printf("%s: No /aliases node in device tree\n", __func__);
		return -1;
	}

	if (mgmt) {
		snprintf(name_buffer, sizeof(name_buffer), "mix%d",
			 oct_eth_info->port);
		path = fdt_getprop(gd->fdt_blob, aliases, name_buffer, NULL);
		if (!path) {
			printf("%s: ERROR: mix%d path not found in device tree\n",
			       __func__, oct_eth_info->port);
			return -1;
		}

		eth = fdt_path_offset(gd->fdt_blob, path);
	} else {
		int pip, iface;

		path = fdt_getprop(gd->fdt_blob, aliases, "pip", NULL);
		if (!path) {
			printf("%s: ERROR: pip path not found in device tree\n",
			       __func__);
			return -1;
		}
		pip = fdt_path_offset(gd->fdt_blob, path);
		if (pip < 0) {
			printf("%s: ERROR: pip not found in device tree\n",
			       __func__);
			return -1;
		}
		snprintf(name_buffer, sizeof(name_buffer), "interface@%d",
			 oct_eth_info->interface);
		iface = fdt_subnode_offset(gd->fdt_blob, pip, name_buffer);
		if (iface < 0) {
			printf("%s: ERROR: pip intf %d not found in device tree\n",
			       __func__, oct_eth_info->interface);
			return -1;
		}
		debug("%s: Looking for ethernet@%x\n", __func__,
		      oct_eth_info->index);
		snprintf(name_buffer, sizeof(name_buffer), "ethernet@%x",
			 oct_eth_info->index);
		eth = fdt_subnode_offset(gd->fdt_blob, iface, name_buffer);
	}
	oct_eth_info->fdt_offset = eth;

	if (eth < 0) {
		printf("%s: ERROR: could not find ethernet port in device tree for interface %d, port %d\n",
		       __func__, oct_eth_info->interface, oct_eth_info->index);
		return -1;
	}

	phy_handle = (uint32_t *)fdt_getprop(gd->fdt_blob, eth,
					     "phy-handle", NULL);
	/* Not all interfaces have a PHY */
	if (!phy_handle) {
		oct_eth_info->phy_fdt_offset = -1;
		return 0;
	}

	phandle = fdt32_to_cpu(*phy_handle);
	phy = fdt_node_offset_by_phandle(gd->fdt_blob, phandle);
	oct_eth_info->phy_fdt_offset = phy;

	if (phy < 0) {
		printf("%s: ERROR: cannot find phy for interface %d, index %d\n",
		       __func__, oct_eth_info->interface, oct_eth_info->index);
		return -1;
	}

	phy_compatible_str = (const char *)fdt_getprop(gd->fdt_blob, phy,
						       "compatible", NULL);
	if (!phy_compatible_str) {
		printf("%s: ERROR: no compatible prop in phy\n", __func__);
		return -1;
	}
	if (!memcmp("vitesse", phy_compatible_str, strlen("vitesse"))) {
		oct_eth_info->phy_device_type = VITESSE;
	} else if (!memcmp("marvell", phy_compatible_str, strlen("marvell"))) {
		oct_eth_info->phy_device_type = MARVELL;
	} else if (!memcmp("cortina", phy_compatible_str, strlen("cortina"))) {
		oct_eth_info->phy_device_type = CORTINA;
	} else if (!memcmp("broadcom", phy_compatible_str, strlen("broadcom"))) {
		oct_eth_info->phy_device_type = BROADCOM;
	} else {
		printf("%s: Unknown PHY compatible string %s\n", __func__,
		       phy_compatible_str);
		return -1;
	}
	oct_eth_info->phy_node_offset = phy;

	return 0;
}

/**
 * Configure PHY information for the specified port and interface mode
 *
 * @param[in,out] dev          - ethernet device
 * @param         if_mode      - mode of port
 * @param         mgmt         - set if management port
 *
 * @return PHY MII address for success, -1 on error
 */
static int octeon_setup_phy(struct eth_device *dev,
			    cvmx_helper_interface_mode_t if_mode,
			    int mgmt)
{
	cvmx_phy_info_t phy_info;
	struct octeon_eth_info *oct_eth_info =
					 (struct octeon_eth_info *)dev->priv;
	char mii_name[MDIO_NAME_LEN];
	int port_offset = mgmt ? CVMX_HELPER_BOARD_MGMT_IPD_PORT : 0;
	phy_interface_t phy_mode;

	if (mgmt) {
		phy_mode = PHY_INTERFACE_MODE_MII;
	} else {
		phy_mode = cvm_if_mode_to_phy_mode(if_mode);
		if (phy_mode == PHY_INTERFACE_MODE_NONE) {
			debug("No phy for interface: %d, port: %d, dev: %s\n",
			      oct_eth_info->interface, oct_eth_info->port,
			      dev->name);
			oct_eth_info->mii_addr = -2;
			oct_eth_info->phydev = NULL;
		}
	}
	debug("interface: %d, index: %d phy mode: %s (%s)\n",
	      oct_eth_info->interface, oct_eth_info->index,
	      cvmx_helper_interface_mode_to_string(if_mode),
	      phy_string_for_interface(phy_mode));
	if (cvmx_helper_board_get_phy_info(&phy_info,
					   oct_eth_info->port + port_offset)) {
		debug("%s: No phy info for ipd port %d\n", __func__,
		      oct_eth_info->port + port_offset);
		return 0;
	}

	oct_eth_info->mii_addr = phy_info.phy_addr;

	if (phy_info.direct_connect)
		snprintf(mii_name, sizeof(mii_name), "mdio-octeon%d",
			 oct_eth_info->mii_addr >> 8);
	else
		snprintf(mii_name, sizeof(mii_name), "mdio-mux-octeon%d",
			 phy_info.gpio_value);


	oct_eth_info->mii_bus = miiphy_get_dev_by_name(mii_name);
	if (!oct_eth_info->mii_bus) {
		printf("Could not find MDIO bus for %s, MII address 0x%x, interface %d, port %d\n",
		       mii_name, oct_eth_info->mii_addr,
		       oct_eth_info->interface, oct_eth_info->port);
		oct_eth_info->phydev = NULL;
#ifdef CONFIG_PHYLIB
	} else {
		debug("Connecting MII phy bus: %s (%d), mode: %s, address: %d to %s\n",
		      mii_name,
		      oct_eth_info->mii_addr >> 8,
		      phy_string_for_interface(phy_mode),
		      oct_eth_info->mii_addr & 0xff,
		      dev->name);

		oct_eth_info->phydev = phy_connect(oct_eth_info->mii_bus,
						   oct_eth_info->mii_addr & 0xff,
						   dev, phy_mode);

		/* We need to call phy_startup for the Cortina PHY. For other
		 * PHY devices it just adds annoying messages and delays if
		 * they are not connected.
		 */
		if (octeon_eth_get_phy_info(dev, mgmt) == 0) {
			switch (oct_eth_info->phy_device_type) {
			case VITESSE:	debug("Vitesse");	break;
			case MARVELL:	debug("Marvell");	break;
			case BROADCOM:	debug("Broadcom");	break;
			case CORTINA:	debug("Cortina");	break;
			default:
				debug("UNKNOWN (%d)",
				      oct_eth_info->phy_device_type);
				break;
			}
			debug(" phy reported for interface %d, index %d, port %d\n",
			      oct_eth_info->interface, oct_eth_info->index,
			      oct_eth_info->port);
			if (oct_eth_info->phydev)
				phy_config(oct_eth_info->phydev);
			if (oct_eth_info->phy_device_type == CORTINA) {
				debug("Starting Cortina PHY at %d\n",
				      oct_eth_info->phydev->addr);
				phy_startup(oct_eth_info->phydev);
			}
		} else {
			debug("No PHY information available for interface %d, index %d\n",
			      oct_eth_info->interface, oct_eth_info->index);
		}
#endif
	}
	return oct_eth_info->mii_addr;
}

/*********************************************************
Only handle built-in RGMII/SGMII/XAUI/GMII ports here
**********************************************************/
int octeon_eth_initialize(bd_t * bis)
{
	struct eth_device *dev;
	struct octeon_eth_info *oct_eth_info;
	int port;
	int interface;
	uint32_t *mac_inc_ptr = (uint32_t *) (&mac_addr[2]);
	int num_ports;
	int num_ints;
	cvmx_helper_interface_mode_t if_mode;

	debug("%s called\n", __func__);
	if (available_mac_count == -1) {
		available_mac_count = gd->ogd.mac_desc.count;
		memcpy(mac_addr,
		       (uint8_t *) (gd->ogd.mac_desc.mac_addr_base), 6);
	}
	if (getenv("disable_networking")) {
		printf("Networking disabled with 'disable_networking' "
		       "environment variable, skipping RGMII interface\n");
		return 0;
	}
	if (available_mac_count <= 0) {
		printf("No available MAC addresses for RGMII interface, "
		       "skipping\n");
		return 0;
	}
	cvmx_user_static_config();
	__cvmx_helper_init_port_valid();

	/* Do board specific init in early_board_init or checkboard if
	 * possible.
	 */

	/* NOTE: on 31XX based boards, the GMXX_INF_MODE register must be set
	 * appropriately before this code is run (in checkboard for instance).
	 * The hardware is configured based on the settings of GMXX_INF_MODE.
	 */

	/* We only care about the first two (real packet interfaces)
	 * This will ignore PCI, loop, etc interfaces.
	 */
	num_ints = cvmx_helper_get_number_of_interfaces();
	debug("Num interfaces: %d\n", num_ints);

	/* Check to see what interface and ports we should use */
	for (interface = 0; interface < num_ints; interface++) {
		if_mode = cvmx_helper_interface_get_mode(interface);
		if (CVMX_HELPER_INTERFACE_MODE_RGMII == if_mode ||
		    CVMX_HELPER_INTERFACE_MODE_GMII  == if_mode ||
		    CVMX_HELPER_INTERFACE_MODE_SGMII == if_mode ||
		    CVMX_HELPER_INTERFACE_MODE_RXAUI == if_mode ||
		    CVMX_HELPER_INTERFACE_MODE_XAUI  == if_mode) {
			cvmx_helper_interface_probe(interface);
			num_ports = cvmx_helper_ports_on_interface(interface);
			debug("Interface %d is %s, ports: %d\n", interface,
			      cvmx_helper_interface_mode_to_string(if_mode),
			      num_ports);
			/* RGMII or SGMII */
			int index = 0;
			int pknd = cvmx_helper_get_ipd_port(interface, 0);
			int incr = 1;
			if (octeon_has_feature(OCTEON_FEATURE_PKND))
				incr = 16;

			num_ports = pknd + (num_ports * incr);
			for (index = 0, port = pknd;
			     (port < num_ports);
			     port += incr, index++) {
				if (!cvmx_helper_is_port_valid(interface, index))
					continue;

				available_mac_count--;
				dev = (struct eth_device *)
							calloc(sizeof(*dev), 1);

				oct_eth_info = (struct octeon_eth_info *)
					calloc(sizeof(struct octeon_eth_info),
					       1);

				oct_eth_info->port = port;
				oct_eth_info->index = index;
				oct_eth_info->interface = interface;
				oct_eth_info->initted_flag = 0;
				/* This is guaranteed to force the link state
				 * to be printed out.
				 */
				oct_eth_info->link_state = 0xffffffffffffffffULL;
				oct_eth_info->ethdev = dev;

				debug("Setting up port: %d, int: %d, index: %d, device: octeth%d\n",
				      oct_eth_info->port, oct_eth_info->interface,
				      oct_eth_info->index, card_number);
				sprintf(dev->name, "octeth%d", card_number);
				card_number++;

				dev->priv = (void *)oct_eth_info;
				dev->iobase = 0;
				dev->init = octeon_ebt3000_rgmii_init;
				dev->halt = octeon_eth_halt;
				dev->send = octeon_eth_send;
				dev->recv = octeon_eth_recv;
				dev->write_hwaddr = octeon_eth_write_hwaddr;
				memcpy(dev->enetaddr, mac_addr, 6);
				(*mac_inc_ptr)++;	/* increment MAC address */

				eth_register(dev);
				/* Link the MDIO bus and PHY to this Ethernet
				 * port
				 */
				octeon_setup_phy(dev, if_mode, 0);
			}
		}
	}
	packet_rx_debug = (getenv("octeon_debug_rx_packets") != NULL);
	packet_tx_debug = (getenv("octeon_debug_tx_packets") != NULL);

	return card_number;
}

#ifdef OCTEON_SPI4000_ENET
/**
 * Writes MAC address to the hardware for SPI4000 (NOP)
 *
 * @param dev - SPI4000 device
 *
 * @return 0
 */
int octeon_spi4000_write_hwaddr(struct eth_device *dev)
{
	return 0;
}

/**
 * Initializes all SPI4000 interfaces
 *
 * @param bis - Board data structure pointer
 */
int octeon_spi4000_initialize(bd_t * bis)
{
	struct eth_device *dev;
	struct octeon_eth_info *oct_eth_info;
	int port;
	int interface = 0;
	uint32_t *mac_inc_ptr = (uint32_t *) (&mac_addr[2]);
	cvmx_spi_callbacks_t spi_callbacks_struct;

	debug("%s: Entry\n", __func__);

	if (available_mac_count == -1) {
		available_mac_count = gd->ogd.mac_desc.count;
		memcpy(mac_addr, (void *)(gd->ogd.mac_desc.mac_addr_base), 6);
	}
	if (getenv("disable_networking")) {
		printf("Networking disabled with 'disable_networking' "
		       "environment variable, skipping SPI interface\n");
		return 0;
	}

	/* Re-setup callbacks here, as the initialization done at compile time
	 * points to the link addresses in flash.
	 */
	cvmx_spi_get_callbacks(&spi_callbacks_struct);
	spi_callbacks_struct.reset_cb = cvmx_spi_reset_cb;
	spi_callbacks_struct.calendar_setup_cb = cvmx_spi_calendar_setup_cb;
	spi_callbacks_struct.clock_detect_cb = cvmx_spi_clock_detect_cb;
	spi_callbacks_struct.training_cb = cvmx_spi_training_cb;
	spi_callbacks_struct.calendar_sync_cb = cvmx_spi_calendar_sync_cb;
	spi_callbacks_struct.interface_up_cb = cvmx_spi_interface_up_cb;
	cvmx_spi_set_callbacks(&spi_callbacks_struct);

	if (!cvmx_spi4000_is_present(interface)) {
		debug("%s: interface %d not present\n", __func__, interface);
		return 0;
	}
	if (available_mac_count <= 0) {
		printf("No available MAC addresses for SPI interface, "
		       "skipping\n");
		return 0;
	}

	for (port = 0; port < 10 && available_mac_count-- > 0; port++) {
		dev = (struct eth_device *)calloc(sizeof(*dev), 1);
		oct_eth_info = (struct octeon_eth_info *)
			calloc(sizeof(struct octeon_eth_info), 1);
		oct_eth_info->port = port;
		oct_eth_info->interface = interface;
		oct_eth_info->initted_flag = 0;

		debug("Setting up SPI4000 port: %d, int: %d\n",
		      oct_eth_info->port, oct_eth_info->interface);
		sprintf(dev->name, "octspi%d", port);
		card_number++;

		dev->priv = (void *)oct_eth_info;
		dev->iobase = 0;
		dev->init = octeon_spi4000_init;
		dev->halt = octeon_eth_halt;
		dev->send = octeon_eth_send;
		dev->recv = octeon_eth_recv;
		dev->write_hwaddr = octeon_spi4000_write_hwaddr;
		memcpy(dev->enetaddr, mac_addr, 6);
		(*mac_inc_ptr)++;	/* increment MAC address */

		eth_register(dev);
		octeon_spi4000_init(dev, gd->bd);
	}
	return card_number;
}
#endif /* OCTEON_SPI4000_ENET */

#ifdef OCTEON_SPI_IXF18201_ENET
int octeon_spi_ixf18201_init(struct eth_device *dev, bd_t * bis)
{				/* Initialize the device  */
	struct octeon_eth_info *priv = (struct octeon_eth_info *) dev->priv;
	static char spi_inited[2] = { 0 };
	debug("octeon_spi_ixf18201_init(), dev_ptr: %p, port: %d\n",
	      dev, priv->port);

	if (priv->initted_flag)
		return 1;

	if (!octeon_global_hw_inited) {
		cvm_oct_configure_common_hw();
	}

	if (!spi_inited[priv->interface]) {
		spi_inited[priv->interface] = 1;
		if (__cvmx_helper_spi_enable(priv->interface) < 0) {
			printf("ERROR initializing spi on Octeon Interface %d\n",
			       priv->interface);
			return 0;
		}
	}

	if (!octeon_global_hw_inited) {
		cvmx_helper_ipd_and_packet_input_enable();
		octeon_global_hw_inited = 1;
	}
	priv->initted_flag = 1;
	return (1);
}

int octeon_spi_ixf18201_write_hwaddr(struct eth_device *dev)
{
	debug("%s: Setting %s address to %02x:%02x:%02x:%02x:%02x:%02x\n",
	      __func__, dev->name,
	      dev->enetaddr[0], dev->enetaddr[1], dev->enetaddr[2],
	      dev->enetaddr[3], dev->enetaddr[4], dev->enetaddr[5]);
	return 0;
}

int octeon_spi_ixf18201_initialize(bd_t * bis)
{
	struct eth_device *dev;
	struct octeon_eth_info *oct_eth_info;
	int port;
	int interface = 0;
	uint32_t *mac_inc_ptr = (uint32_t *) (&mac_addr[2]);
	cvmx_spi_callbacks_t spi_callbacks_struct;

	if (available_mac_count == -1) {
		available_mac_count = gd->ogd.mac_desc.count;
		memcpy(mac_addr, (void *)(gd->ogd.mac_desc.mac_addr_base), 6);
	}
	if (getenv("disable_networking")) {
		printf("Networking disabled with 'disable_networking' "
		       "environment variable, skipping SPI interface\n");
		return 0;
	}

	/* Re-setup callbacks here, as the initialization done at compile time
	 * points to the link addresses in flash.
	 */
	cvmx_spi_get_callbacks(&spi_callbacks_struct);
	spi_callbacks_struct.reset_cb = cvmx_spi_reset_cb;
	spi_callbacks_struct.calendar_setup_cb = cvmx_spi_calendar_setup_cb;
	spi_callbacks_struct.clock_detect_cb = cvmx_spi_clock_detect_cb;
	spi_callbacks_struct.training_cb = cvmx_spi_training_cb;
	spi_callbacks_struct.calendar_sync_cb = cvmx_spi_calendar_sync_cb;
	spi_callbacks_struct.interface_up_cb = cvmx_spi_interface_up_cb;
	cvmx_spi_set_callbacks(&spi_callbacks_struct);

	if (available_mac_count <= 0) {
		printf("No available MAC addresses for SPI interface, "
		       "skipping\n");
		return 0;
	}

	int eth_num = 0;
	for (interface = 0; interface < 2; interface++) {
		int ports = 1;	/* We only want to set up the real ports here */
		for (port = 0; port < ports && available_mac_count-- > 0;
		     port++) {
			dev = (struct eth_device *)calloc(sizeof(*dev), 1);
			oct_eth_info = (struct octeon_eth_info *)
			    calloc(sizeof(struct octeon_eth_info), 1);

			oct_eth_info->port = port;
			oct_eth_info->interface = interface;
			oct_eth_info->initted_flag = 0;

			debug("Setting up SPI port: %d, int: %d\n",
			      oct_eth_info->port, oct_eth_info->interface);
			sprintf(dev->name, "octspi%d", eth_num++);
			card_number++;

			dev->priv = (void *)oct_eth_info;
			dev->iobase = 0;
			dev->init = octeon_spi_ixf18201_init;
			dev->halt = octeon_eth_halt;
			dev->send = octeon_eth_send;
			dev->recv = octeon_eth_recv;
			dev->write_hwaddr = octeon_spi_ixf18201_write_hwaddr;
			memcpy(dev->enetaddr, mac_addr, 6);
			(*mac_inc_ptr)++;	/* increment MAC address */

			eth_register(dev);
			octeon_spi_ixf18201_init(dev, gd->bd);
		}
	}
	return card_number;
}
#endif /* OCTEON_SPI_IXF18201_ENET */

#endif /* OCTEON_INTERNAL_ENET */

#if defined(OCTEON_MGMT_ENET) && defined(CONFIG_CMD_NET)

int octeon_mgmt_eth_init(struct eth_device *dev, bd_t * bis)
{				/* Initialize the device  */
	struct octeon_eth_info *priv = (struct octeon_eth_info *) dev->priv;
	debug("%s(), dev_ptr: %p\n", __func__, dev);

	if (priv->initted_flag)
		return 1;

	priv->enabled = 0;

	priv->initted_flag = 1;
	return 1;
}

int octeon_mgmt_eth_send(struct eth_device *dev, void *packet, int length)
{				/* Send a packet  */
	int retval;
	struct octeon_eth_info *priv = (struct octeon_eth_info *) dev->priv;

	if (!priv->enabled) {
		priv->enabled = 1;
		cvmx_mgmt_port_enable(priv->port);
	}
	debug("%s: called for %s, sending %d bytes\n",
	      __func__, dev->name, length);
	retval = cvmx_mgmt_port_send(priv->port, length, (void *)packet);

	if (packets_sent++ == 0)
		cvmx_mgmt_port_link_set(priv->port,
					cvmx_mgmt_port_link_get(priv->port));

	return retval;

}

#define MGMT_BUF_SIZE   1700
int octeon_mgmt_eth_recv(struct eth_device *dev)
{				/* Check for received packets     */
	uchar buffer[MGMT_BUF_SIZE];
	struct octeon_eth_info *priv = (struct octeon_eth_info *) dev->priv;

	if (!priv->enabled) {
		priv->enabled = 1;
		cvmx_mgmt_port_enable(priv->port);
	}
	int length = cvmx_mgmt_port_receive(priv->port, MGMT_BUF_SIZE, buffer);

	if (length > 0) {
#if defined(DEBUG) && 0
		/* Dump out packet contents */
		{
			int i, j;
			unsigned char *up = buffer;

			for (i = 0; (i + 16) < length; i += 16) {
				printf("%04x ", i);
				for (j = 0; j < 16; ++j) {
					printf("%02x ", up[i + j]);
				}
				printf("    ");
				for (j = 0; j < 16; ++j) {
					printf("%c", ((up[i + j] >= ' ')
						      && (up[i + j] <=
							  '~')) ? up[i +
								     j] : '.');
				}
				printf("\n");
			}
			printf("%04x ", i);
			for (j = 0; i + j < length; ++j) {
				printf("%02x ", up[i + j]);
			}
			for (; j < 16; ++j) {
				printf("   ");
			}
			printf("    ");
			for (j = 0; i + j < length; ++j) {
				printf("%c", ((up[i + j] >= ' ')
					      && (up[i + j] <=
						  '~')) ? up[i + j] : '.');
			}
			printf("\n");
		}
#endif
		debug("MGMT port %s got %d byte packet\n", dev->name, length);
		NetReceive(buffer, length);
	} else if (length < 0) {
		printf("MGMT port %s rx error: %d\n", dev->name, length);
	}

	return (length);
}

/**
 * Shuts down receiving packets on a management interface
 *
 * @param dev - Management Ethernet device
 */
void octeon_mgmt_eth_halt(struct eth_device *dev)
{				/* stop SCC                       */
	struct octeon_eth_info *priv = (struct octeon_eth_info *) dev->priv;
	priv->enabled = 0;
	cvmx_mgmt_port_disable(priv->port);
}

/**
 * Writes the MAC address to the hardware for management devices
 *
 * @param dev - Management Ethernet device
 *
 * @return 0 for success
 */
int octeon_mgmt_eth_write_hwaddr(struct eth_device *dev)
{
	struct octeon_eth_info *oct_eth_info;
	oct_eth_info = (struct octeon_eth_info *)dev->priv;
	u64 mac = 0;

	/* Don't do anything if the interface is disabled. */
	if (!oct_eth_info->enabled) {
		debug("%s: Interface disabled\n", __func__);
		return 0;
	}

	memcpy(((char *)&mac) + 2, dev->enetaddr, 6);
	debug("%s: Setting %s address to %02x:%02x:%02x:%02x:%02x:%02x\n",
	      __func__, dev->name,
	      dev->enetaddr[0], dev->enetaddr[1], dev->enetaddr[2],
	      dev->enetaddr[3], dev->enetaddr[4], dev->enetaddr[5]);
	return cvmx_mgmt_port_set_mac(oct_eth_info->port, mac);
}

/**
 * Initializes management Ethernet devices
 *
 * @param bis - Board specific data structure pointer
 *
 * @return current card number
 */
int octeon_mgmt_eth_initialize(bd_t * bis)
{
	struct eth_device *dev;
	struct octeon_eth_info *oct_eth_info;
	uint32_t *mac_inc_ptr = (uint32_t *) (&mac_addr[2]);
	int mgmt_port_count;
	int cur_port;
	int retval;
	int aliases;

	debug("%s called\n", __func__);
	/* Figure out how many management ports there are from
	 * the device tree, otherwise assume number available to chip.
	 */
	aliases = fdt_path_offset(gd->fdt_blob, "/aliases");
	if (aliases >= 0) {
		if (fdt_getprop(gd->fdt_blob, aliases, "mix1", NULL))
			mgmt_port_count = 2;
		else if (fdt_getprop(gd->fdt_blob, aliases, "mix0", NULL))
			mgmt_port_count = 1;
		else
			mgmt_port_count = 0;
	} else {
		if (OCTEON_IS_MODEL(OCTEON_CN52XX)
		    || OCTEON_IS_MODEL(OCTEON_CN63XX)
		    || OCTEON_IS_MODEL(OCTEON_CN66XX)
		    || OCTEON_IS_MODEL(OCTEON_CN61XX))
			mgmt_port_count = 2;
		else
			mgmt_port_count = 1;
	}

	if (available_mac_count == -1) {
		available_mac_count = gd->ogd.mac_desc.count;
		memcpy(mac_addr,
		       (uint8_t *) (gd->ogd.mac_desc.mac_addr_base), 6);
	}
	if (getenv("disable_networking")) {
		printf("Networking disabled with 'disable_networking' "
		       "environment variable, skipping RGMII interface\n");
		return 0;
	}

	if (available_mac_count <= 0) {
		printf("No available MAC addresses for Management "
		       "interface(s), skipping\n");
		return 0;
	}

	for (cur_port = 0;
	     (cur_port < mgmt_port_count) && available_mac_count-- > 0;
	     cur_port++) {
		uint64_t mac = 0x0;
		/* Initialize port state array to invalid values */
		dev = (struct eth_device *)malloc(sizeof(*dev));

		oct_eth_info = (struct octeon_eth_info *)
				calloc(sizeof(struct octeon_eth_info), 1);

		oct_eth_info->port = cur_port;
		/* These don't apply to the management ports */
		oct_eth_info->interface = ~0;
		oct_eth_info->initted_flag = 0;

		oct_eth_info->link_state = 0xffffffffffffffffULL;

		debug("Setting up mgmt eth port %d\n", cur_port);
		sprintf(dev->name, "octmgmt%d", cur_port);

		dev->priv = (void *)oct_eth_info;
		dev->iobase = 0;
		dev->init = octeon_mgmt_eth_init;
		dev->halt = octeon_mgmt_eth_halt;
		dev->send = octeon_mgmt_eth_send;
		dev->recv = octeon_mgmt_eth_recv;
		dev->write_hwaddr = octeon_mgmt_eth_write_hwaddr;
		memcpy(dev->enetaddr, mac_addr, 6);

		(*mac_inc_ptr)++;	/* increment MAC address */

		eth_register(dev);
		if (CVMX_MGMT_PORT_SUCCESS !=
		    (retval = cvmx_mgmt_port_initialize(oct_eth_info->port))) {
			printf("ERROR: cvmx_mgmt_port_initialize() failed: %d\n",
			       retval);
		}
		memcpy(((char *)&mac) + 2, dev->enetaddr, 6);
		cvmx_mgmt_port_set_mac(oct_eth_info->port, mac);
		cvmx_helper_link_info_t link =
		    cvmx_mgmt_port_link_get(oct_eth_info->port);
		if (octeon_setup_phy(dev,
				     CVMX_HELPER_INTERFACE_MODE_GMII, 1) < 0)
			printf("Error configuring PHY for management port %d\n",
			       cur_port);

		debug("Port: %d, speed: %d, up: %d, duplex: %d\n", cur_port,
		      link.s.speed, link.s.link_up, link.s.full_duplex);
		cvmx_mgmt_port_link_set(oct_eth_info->port, link);
	}

	return card_number;

}
#endif /* defined(OCTEON_MGMT_ENET) && defined(CONFIG_CMD_NET) */
