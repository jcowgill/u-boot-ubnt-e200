/***********************license start************************************
 * Copyright (c) 2011 Cavium, Inc. (support@cavium.com).  All rights
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
 * $Id: octeon_fdt.c 78682 2012-11-27 08:41:30Z awilliams $
 *
 */

#include <common.h>
#include <fdt.h>
#include <fdt_support.h>
#include <addr_map.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-clock.h>
#include <asm/arch/octeon-model.h>
#include <cvmx-wqe.h>
#include <asm/arch/octeon_eth.h>

DECLARE_GLOBAL_DATA_PTR;

/**
 * Trims nodes from the flat device tree.
 *
 * @param fdt - pointer to working FDT, usually in gd->fdt_blob
 * @param fdt_key - key to preserve.  All non-matching keys are removed
 * @param trim_name - name of property to look for.  If NULL use
 * 		      'cavium,qlm-trim'
 *
 * The key should look something like device #, type where device # is a
 * number from 0-9 and type is a string describing the type.  For QLM
 * operations this would typically contain the QLM number followed by
 * the type in the device tree, like "0,xaui", "0,sgmii", etc.  This function
 * will trim all items in the device tree which match the device number but
 * have a type which does not match.  For example, if a QLM has a xaui module
 * installed on QLM 0 and "0,xaui" is passed as a key, then all FDT nodes that
 * have "0,xaui" will be preserved but all others, i.e. "0,sgmii" will be
 * removed.
 *
 * Note that the trim_name must also match.  If trim_name is NULL then it
 * looks for the property "cavium,qlm-trim".
 *
 * Also, when the trim_name is "cavium,qlm-trim" or NULL that the interfaces
 * will also be renamed based on their register values.
 *
 * For example, if a PIP interface is named "interface@W" and has the property
 * reg = <0> then the interface will be renamed after this function to
 * interface@0.
 *
 * @return 0 for success.
 */
int octeon_fdt_patch(void *fdt, const char *fdt_key, const char *trim_name)
	__attribute__((weak, alias("__octeon_fdt_patch")));

int __octeon_fdt_patch(void *fdt, const char *fdt_key, const char *trim_name)
{
	int fdt_key_len;
	int offset, next_offset;

	if (trim_name == NULL)
		trim_name = "cavium,qlm-trim";

	debug("In %s: Patching FDT header at 0x%p with key \"%s\"\n",
	      __func__, fdt, fdt_key);
	if (!fdt || fdt_check_header(fdt) != 0)
		return -1;

	fdt_key_len = strlen(fdt_key) + 1;

	/* Prune out the unwanted parts based on the QLM mode.  */
	offset = 0;
	for (offset = fdt_next_node(fdt, offset, NULL);
	     offset >= 0;
	     offset = next_offset) {
		int len;
		const char *val;

		next_offset = fdt_next_node(fdt, offset, NULL);

		val = fdt_getprop(fdt, offset, trim_name, &len);
		if (!val)
			continue;

		debug("fdt found trim name %s, comparing key \"%s\"(%d) with \"%s\"(%d)\n",
		      trim_name, fdt_key, fdt_key_len, val, len);
		if (memcmp(val, fdt_key, 2) != 0)
			continue; /* Not this QLM. */

		debug("fdt key number matches\n");

		if (memcmp(val, fdt_key, fdt_key_len) != 0) {
			/* This QLM, but wrong mode.  Delete it. */
			debug("fdt trimming matching key %s\n", fdt_key);
			next_offset = fdt_parent_offset(fdt, offset);
			fdt_nop_node(fdt, offset);
		}
	}
	/* Second pass: Rewrite names and remove key properties.  */
	offset = 0;
	for (offset = fdt_next_node(fdt, offset, NULL);
	     offset >= 0;
	     offset = next_offset) {
		int len;
		const char *val = fdt_getprop(fdt, offset,
					      trim_name, &len);

		next_offset = fdt_next_node(fdt, offset, NULL);

		if (!val)
			continue;
		if (memcmp(val, fdt_key, fdt_key_len) == 0) {
			char new_name[20];
			const char *name;
			const char *at;
			const uint32_t *reg;

			debug("Found key %s at offset 0x%x\n", fdt_key, offset);
			fdt_nop_property(fdt, offset, trim_name);

			/* We only do renaming of interfaces for QLM trims */
			if (!strcmp(trim_name, "cavium,qlm-trim")) {
				name = fdt_get_name(fdt, offset, NULL);
				if (name == NULL)
					continue;
				at = strchr(name, '@');
				if (at == NULL)
					continue;

				reg = fdt_getprop(fdt, offset, "reg", &len);
				if (reg == NULL || len != 4)
					continue;

				len = at - name + 1;
				if (len + 9 >= sizeof(new_name))
					continue;

				memcpy(new_name, name, len);
				sprintf(new_name + len, "%x",
					fdt32_to_cpu(*reg));
				fdt_set_name(fdt, offset, new_name);
			}
			/* Structure may have changed, start at the beginning. */
			next_offset = 0;
		}
	}

	return 0;
}

void octeon_set_one_fdt_mac(int node, uint64_t *mac)
{
	uint8_t mac_addr[6];
	int r;

	mac_addr[5] = *mac & 0xff;
	mac_addr[4] = (*mac >> 8) & 0xff;
	mac_addr[3] = (*mac >> 16) & 0xff;
	mac_addr[2] = (*mac >> 24) & 0xff;
	mac_addr[1] = (*mac >> 32) & 0xff;
	mac_addr[0] = (*mac >> 40) & 0xff;

	r = fdt_setprop_inplace(working_fdt, node, "local-mac-address",
				mac_addr, 6);
	if (r == 0)
		*mac = *mac + 1;
}

void octeon_fixup_fdt(void)
	__attribute__((weak, alias("__octeon_fixup_fdt")));

void __octeon_fixup_fdt(void)
{
	const char *p;
	int node, pip, interface, ethernet;
	int i, e;
	uint64_t mac;
	char name[20];
	uint32_t clk;
	u64 sizes[3], addresses[3];
	u64 size_left = gd->ram_size;
	int num_addresses = 0;
	int rc;

	if (!working_fdt)
		return;

	mac = (gd->ogd.mac_desc.mac_addr_base[5] & 0xffull) |
		((gd->ogd.mac_desc.mac_addr_base[4] & 0xffull) << 8) |
		((gd->ogd.mac_desc.mac_addr_base[3] & 0xffull) << 16) |
		((gd->ogd.mac_desc.mac_addr_base[2] & 0xffull) << 24) |
		((gd->ogd.mac_desc.mac_addr_base[1] & 0xffull) << 32) |
		((gd->ogd.mac_desc.mac_addr_base[0] & 0xffull) << 40);

	for (i = 0; i < 2; i++) {
		sprintf(name, "mix%x", i);
		p = fdt_get_alias(working_fdt, name);
		if (p) {
			node = fdt_path_offset(working_fdt, p);
			if (node <= 0)
				continue;
			octeon_set_one_fdt_mac(node, &mac);
		}
	}

	p = fdt_get_alias(working_fdt, "pip");
	if (!p)
		return;

	pip = fdt_path_offset(working_fdt, p);
	if (pip <= 0)
		return;

	for (i = 0; i < 8; i++) {
		sprintf(name, "interface@%d", i);
		interface = fdt_subnode_offset(working_fdt, pip, name);
		if (interface <= 0)
			continue;
		for (e = 0; e < 16; e++) {
			sprintf(name, "ethernet@%d", e);
			ethernet = fdt_subnode_offset(working_fdt, interface,
						      name);
			if (ethernet <= 0)
				break;
			octeon_set_one_fdt_mac(ethernet, &mac);
		}
	}

	clk = cvmx_clock_get_rate(CVMX_CLOCK_SCLK);

	for (i = 0; i < 2; i++) {
		sprintf(name, "uart%x", i);
		p = fdt_get_alias(working_fdt, name);
		if (p) {
			node = fdt_path_offset(working_fdt, p);
			if (node <= 0)
				continue;
			fdt_setprop_inplace_cell(working_fdt, node,
						 "clock-frequency", clk);
		}
	}
	size_left = gd->ram_size;
	sizes[num_addresses] = min(size_left, 256 * 1024 * 1024);
	size_left -= sizes[num_addresses];
	addresses[num_addresses] = 0;
	num_addresses++;

	if (size_left > 0) {
		if (OCTEON_IS_MODEL(OCTEON_CN3XXX) ||
		    OCTEON_IS_MODEL(OCTEON_CN5XXX)) {
			sizes[num_addresses] =
					min(256 * 1024 * 1024, size_left);
			addresses[num_addresses] = 0x410000000ULL;
			size_left -= sizes[num_addresses];
			num_addresses++;

			if (size_left > 0) {
				sizes[num_addresses] = size_left;
				addresses[num_addresses] = 0x20000000ULL;
				size_left = 0;
				num_addresses++;
			}
		} else {
			sizes[num_addresses] = size_left;
			addresses[num_addresses] = 0x20000000ULL;
			num_addresses++;
		}
	}

	node = fdt_path_offset(working_fdt, "/memory");
	if (node < 0)
		node = fdt_add_subnode(working_fdt, 0, "memory");
	if (node < 0) {
		printf("Could not add memory section to fdt: %s\n",
		       fdt_strerror(node));
		return;
	}
	rc = fdt_fixup_memory_banks(working_fdt, addresses, sizes,
				    num_addresses);
	if (rc != 0) {
		printf("%s: fdt_fixup_memory_banks returned %d when adding %d addresses\n",
		       __func__, rc, num_addresses);
	}
}

int __board_fixup_fdt(void)
{
	/*
	 * Nothing to do in this dummy implementation
	 */
	return 0;
}
int board_fixup_fdt(void)
	__attribute__((weak, alias("__board_fixup_fdt")));

/**
 * This is a helper function to find the offset of a PHY device given
 * an Ethernet device.
 *
 * @param[in] eth - Ethernet device to search for PHY offset
 *
 * @returns offset of phy info in device tree or -1 if not found
 */
int octeon_fdt_find_phy(const struct eth_device *eth)
{
	int aliases;
	void *fdt = gd->fdt_blob;
	const char *pip_path;
	int pip;
	char buffer[64];
	struct octeon_eth_info *oct_eth_info =
				 (struct octeon_eth_info *)eth->priv;
	int interface, index;
	int phandle;
	int phy;
	uint32_t *phy_handle;

	aliases = fdt_path_offset(fdt, "/aliases");
	if (aliases < 0) {
		puts("/aliases not found in device tree!\n");
		return -1;
	}
	pip_path = fdt_getprop(fdt, aliases, "pip", NULL);
	if (!pip_path) {
		puts("pip not found in aliases in device tree\n");
		return -1;
	}
	pip = fdt_path_offset(fdt, pip_path);
	if (pip < 0) {
		puts("pip not found in device tree\n");
		return -1;
	}
	snprintf(buffer, sizeof(buffer), "interface@%d"
	, oct_eth_info->interface);
	interface = fdt_subnode_offset(fdt, pip, buffer);
	if (interface < 0) {
		printf("%s: interface@%d not found in device tree for %s\n",
		       __func__, oct_eth_info->interface, eth->name);
		return -1;
	}
	snprintf(buffer, sizeof(buffer), "ethernet@%x", oct_eth_info->index);
	index = fdt_subnode_offset(fdt, interface, buffer);
	if (index < 0) {
		printf("%s: ethernet@%x not found in device tree for %s\n",
		       __func__, oct_eth_info->index, eth->name);
		return -1;
	}
	phy_handle = (uint32_t *)fdt_getprop(fdt, index, "phy-handle", NULL);
	if (phy_handle < 0) {
		printf("%s: phy-handle not found for %s\n",
		       __func__, eth->name);
		return -1;
	}
	phandle = fdt32_to_cpu(*phy_handle);
	phy = fdt_node_offset_by_phandle(fdt, phandle);
	if (phy < 0) {
		printf("%s: phy not found for %s\n", __func__, eth->name);
		return -1;
	}

	return phy;
}
