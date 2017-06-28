/*
 * (C) Copyright 2004-2012 Cavium, Inc.
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

/* This file sets up various devices on the boot bus.  The functions are
 * defined as weak so any board may override this functionality.
 */

#include <common.h>
#include <command.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <fdt.h>
#include <malloc.h>
#include <errno.h>
#include <linux/list.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/octeon-model.h>
#include <asm/arch/octeon-feature.h>
#include <asm/arch/octeon_hal.h>
#include <asm/arch/cvmx-gpio-defs.h>
#include <asm/arch/cvmx-mio-defs.h>
#include <asm/arch/octeon_boot_bus.h>
#include <asm/arch/octeon_cf.h>
#include <asm/arch/octeon_boot.h>

#define OCTEON_BOOT_BUS_CS_CONFIG_COMPAT "cavium,octeon-3860-bootbus-config"

#define FLASH_RoundUP(_Dividend, _Divisor) (((_Dividend)+(_Divisor))/(_Divisor))

DECLARE_GLOBAL_DATA_PTR;

typedef int (*boot_device_func_t)(void *fdt_addr, int node_offset,
				  int bootbus_node_offset, void *param,
				  uint64_t *dev_addr);

struct bootbus_fdt_handler_s {
	char *name;
	void *param;
	boot_device_func_t boot_device_func;
	struct list_head list;
};


static struct bootbus_fdt_handler_s bootbus_handlers;

int octeon_boot_bus_add_fdt_handler(const char *name, void *param,
				    int (*callback)(void *fdt_addr,
						    int node_offset,
						    int bootbus_node_offset,
						    void *param,
						    uint64_t *dev_addr))
{
	struct bootbus_fdt_handler_s *handler;

	debug("%s: Adding FDT initializer handler for %s\n", __func__, name);

	handler = (struct bootbus_fdt_handler_s *)
		malloc(sizeof(struct bootbus_fdt_handler_s));
	if (!handler)
		return -ENOMEM;

	handler->name = strdup(name);
	if (!handler->name) {
		free(handler);
		return -ENOMEM;
	}

	handler->param = param;
	handler->boot_device_func = callback;
	list_add_tail(&(handler->list), &(bootbus_handlers.list));
	return 0;
}

/* This function is called before the FDT is parsed for boot bus devices
 * after relocation to RAM.  The purpose of the called function is to call
 * octeon_boot_bus_add_fdt_handlers() to add functions to be called when
 * walking the device tree.
 */
int octeon_add_user_boot_bus_fdt_devices(void)
	__attribute__((__weak__));

int octeon_add_user_boot_bus_fdt_devices(void)
{
	return 0;
}

struct bootbus_fdt_handler_s *boot_bus_get_handler(const char *name)
{
	struct bootbus_fdt_handler_s *handler;
	size_t len;

	debug("%s: Searching for handler for %s\n", __func__, name);
	list_for_each_entry(handler, &bootbus_handlers.list, list) {
		if (!handler->name)
			continue;
		len = strlen(handler->name);
		if (!strncmp(handler->name, name, len) &&
		    ((name[len] == '@') || (name[len] == '\0'))) {
			debug("%s: Found match for %s\n", __func__, name);
			return handler;
		}
	}
	return NULL;
}

/**
 * Converts a time in ns and clock period to the number of clocks.
 *
 * @param time		time to encode in nanoseconds, -1 for maximum
 * @param clock_period	clock period to use for encoding
 *
 * @return number of clock cycles
 *
 * NOTE: This function assumes that the timing multiplier value in
 * MIO_BOOT_REG_CFGX is set to the default value of 3 (8x).
 */
uint32_t boot_bus_time_encode(int64_t time, uint64_t clock_period)
	__attribute__((weak, alias("__boot_bus_time_encode")));

uint32_t __boot_bus_time_encode(int64_t time, uint64_t clock_period)
{
	uint32_t clocks;
	int clock_mult = (OCTEON_IS_MODEL(OCTEON_CN3XXX)
			  || OCTEON_IS_MODEL(OCTEON_CN58XX)) ? 4 : 8;
	if (time < 0)
		clocks = 0x3f;
	if (time == 0)
		return 0;

	clocks = FLASH_RoundUP((FLASH_RoundUP(time * 1000, clock_period) - 1),
			       clock_mult);
	if (clocks > 0x3f) {
		printf("Warning: clock exceeds maximum value, truncating.\n");
		clocks = 0x3f;
	}
	return clocks;
}

/**
 * Macro so clock_period doesn't have to be passed all the time
 */
#define BOOT_BUS_TIME_ENCODE(time) \
	boot_bus_time_encode((time), clock_period)

/**
 * Generic function for initializing boot bus devices.
 *
 * Some boards have devices not included here, such as the Embedded Planet
 * EP6300C Board.  If this is the case, just define this function within the
 * appropriate board file.  This function is weak so the new function will
 * override  this one during the link phase.
 */
void octeon_boot_bus_board_init(void)
{
	return;
}
void octeon_boot_bus_board_init(void) __attribute__ ((weak));

/**
 * Generic function for initializing boot bus devices.
 *
 * Some boards have devices not included here, such as the Embedded Planet
 * EP6300C Board.  If this is the case, just define this function within the
 * appropriate board file.  This function is weak so the new function will
 * override  this one during the link phase.
 *
 * This function is called after relocation to RAM.
 */
void octeon_boot_bus_board_late_init(void)
{
	return;
}
void octeon_boot_bus_board_late_init(void) __attribute__ ((weak));



struct {
	boot_bus_config_t conf;
	const char *str;
	int mode;
} fdt_prop_to_str[] = {
	{ T_ADR,	"cavium,t-adr",		0 },
	{ T_ALE,	"cavium,t-ale",		0 },
	{ T_CE,		"cavium,t-ce",		0 },
	{ T_OE,		"cavium,t-oe",		0 },
	{ T_WE,		"cavium,t-we",		0 },
	{ T_RD_HLD,	"cavium,t-rd-hld",	0 },
	{ T_WR_HLD,	"cavium,t-wr-hld",	0 },
	{ T_PAUSE,	"cavium,t-pause",	0 },
	{ T_WAIT,	"cavium,t-wait",	0 },
	{ T_PAGE,	"cavium,t-page",	0 },
	{ T_RD_DLY,	"cavium,t-rd-dly",	0 },
	{ PAGES,	"cavium,pages",		0 },
	{ BUS_WIDTH,	"cavium,bus-width",	0 },
	{ WAIT_MODE,	"cavium,wait-mode",	1 },
	{ PAGE_MODE,	"cavium,page-mode",	1 },
	{ ALE_MODE,	"cavium,ale-mode",	1 },
	{ UNKNOWN,	NULL,			1 }
};

const char *config_to_prop(boot_bus_config_t conf)
{
	if (conf < UNKNOWN)
		return fdt_prop_to_str[conf].str;
	else
		return NULL;
}

boot_bus_config_t prop_to_config(const char *str)
{
	int i = 0;
	do {
		if (!strcmp(fdt_prop_to_str[i].str, str))
			return (fdt_prop_to_str[i].conf);
		i++;
	} while (fdt_prop_to_str[i].str != NULL);
	return UNKNOWN;
}


int octeon_boot_bus_get_property_node(int node_offset, boot_bus_config_t prop,
				      uint32_t *value)
	__attribute__((weak, alias("__octeon_boot_bus_get_property_node")));

int __octeon_boot_bus_get_property_node(int node_offset,
					boot_bus_config_t prop,
					uint32_t *value)
{
	const void *nodep;
	const char *property;
	int len;

	property = config_to_prop(prop);
	if (!property) {
		debug("Unknown FDT property %d\n", prop);
		return -1;
	}
	nodep = fdt_getprop(gd->fdt_blob, node_offset, property, &len);
	/* The following do not have property values so the value is whether
	 * it is present or not.
	 */
	if (prop == WAIT_MODE || prop == PAGE_MODE || prop == ALE_MODE) {
		*value = (nodep != NULL);
		debug("%s: node offset %d, property: %s returning 0x%x (%u)\n",
		      __func__, node_offset, property, *value, *value);
		return 0;
	}
	if (len <= 0 || nodep == NULL) {
		debug("FDT property %s not found at node offset %d\n",
		      property, node_offset);
		return -1;
	}
	if (len != 4) {
		debug("Unknown FDT property size %d for node offset %d\n",
		      len, node_offset);
		return -1;
	}
	*value =fdt32_to_cpu(*(uint32_t *)nodep);
	debug("%s: node offset %d, property: %s returning 0x%x (%u)\n",
	      __func__, node_offset, property, *value, *value);

	return 0;
}

/**
 * Gets a boot-bus property from the FDT for the specified bus
 * @param chip_sel - chip select of device
 * @param prop - property to get
 * @param[out] value - value of property
 *
 * @return 0 for success, -1 on error
 */
int octeon_boot_bus_get_property(u8 chip_sel, boot_bus_config_t prop,
				 uint32_t *value)
	__attribute__((weak, alias("__octeon_boot_bus_get_property")));
int __octeon_boot_bus_get_property(u8 chip_sel, boot_bus_config_t prop,
				   uint32_t *value)
{
	int node_offset;
	char path[128];

	sprintf(path, "/soc/bootbus/cavium,cs-config@%u", chip_sel);
	node_offset = fdt_path_offset(gd->fdt_blob, path);

	if (node_offset < 0) {
		debug("Could not find \"%s\" in FDT\n", path);
		return -1;
	}
	if (fdt_node_check_compatible(gd->fdt_blob, node_offset,
				      "cavium,octeon-3860-bootbus-config")) {
		printf("Incompatible cs-config type in FDT for chip select %d\n",
		       chip_sel);
		return -1;
	}
	return octeon_boot_bus_get_property_node(node_offset, prop, value);
}

/**
 * Outputs the address range for the specified chip select.
 * @param chip_sel	The chip select number [0-7]
 * @param bootbus_node_offset FDT node offset of /soc/bootbus,
 *			      can be -1 to calculate
 * @param[out] address	Pointer to address to output
 * @param[out] size		Pointer to size to output, can be NULL.
 *
 * @return 0 for success, -1 on error.
 */
int octeon_boot_bus_get_range(u8 chip_sel, int bootbus_node_offset,
			      uint64_t *address, uint32_t *size)
	__attribute__((weak, alias("__octeon_boot_bus_get_range")));
int __octeon_boot_bus_get_range(u8 chip_sel, int bootbus_node_offset,
				uint64_t *address, uint32_t *size)
{
	const void *nodep;
	int len;
	int i;
	uint32_t *p;

	if (bootbus_node_offset < 0)
		bootbus_node_offset =
			fdt_path_offset(gd->fdt_blob, "/soc/bootbus");

	if (bootbus_node_offset < 0) {
		printf("Could not find /soc/bootbus in FDT.\n");
		return -1;
	}

	if (fdt_node_check_compatible(gd->fdt_blob, bootbus_node_offset,
				      "cavium,octeon-3860-bootbus")) {
		printf("Incompatible bootbus in FDT\n");
		return -1;
	}
	nodep = fdt_getprop(gd->fdt_blob, bootbus_node_offset,
			    "ranges", &len);

	if (len < 5 * sizeof(*p)) {
		printf("Invalid range size for /soc/bootbus in FDT.\n");
		return -1;
	}
	p = (uint32_t *)nodep;
	for (i = 0; i < (len * sizeof(*p)); i += 5, p += 5) {
		if (*p == chip_sel)
			break;
	}
	if (*p != chip_sel) {
		printf("Could not find chip select %u in /soc/bootbus ranges\n",
		       chip_sel);
		return -1;
	}
	memcpy(address, &p[2], sizeof(u64));
	if (size != NULL)
		*size = fdt32_to_cpu(p[4]);
	debug("%s: cs: %d, address: 0x%llx, size: 0x%x\n", __func__,
	      chip_sel, *address, fdt32_to_cpu(p[4]));
	return 0;
}

/**
 * Get boot bus information based on the chip select
 *
 * @param[in] chip_sel - chip select to get info for
 * @param[in] clock_period - clock period used for timing information
 * @param[out] reg_cfg - Boot bus config register, may be NULL
 * @param[out] reg_tim - Boot bus timing register
 *
 * This function iterates through the flat device tree and validates
 * the compatible and cavium,cs-index fields and fills in reg_cfg and
 * reg_tim based on the parameters found in the FDT.
 */
int octeon_boot_bus_get_fdt_info(int chip_sel, uint64_t clock_period,
				   cvmx_mio_boot_reg_cfgx_t *reg_cfg,
				   cvmx_mio_boot_reg_timx_t *reg_tim)
	__attribute__((weak, alias("__octeon_boot_bus_get_fdt_info")));

int __octeon_boot_bus_get_fdt_info(int chip_sel, uint64_t clock_period,
				   cvmx_mio_boot_reg_cfgx_t *reg_cfg,
				   cvmx_mio_boot_reg_timx_t *reg_tim)
{
	uint32_t value;
	int node_offset, next_node_offset = -1;
	char path[64];
	uint32_t tag;
	int level = 0;
	int len;
	const struct fdt_property *fdt_prop;
	const void *nodep;
	const char *prop_str;
	boot_bus_config_t prop;
	int compat_found = 0;
	int prop_number = 0;
	int cs_index = 0;

	debug("%s: Getting info for chip select %d\n", __func__, chip_sel);
	sprintf(path, "/soc/bootbus/cavium,cs-config@%u", chip_sel);
	node_offset = fdt_path_offset(gd->fdt_blob, path);

	if (node_offset < 0) {
		debug("Could not find \"%s\" in FDT\n", path);
		return -1;
	}
	if (fdt_node_check_compatible(gd->fdt_blob, node_offset,
				      "cavium,octeon-3860-bootbus-config")) {
		printf("Incompatible bootbus cs-config for cs %d\n", chip_sel);
		return -1;
	}
	/* Iterate through all of the tags for the cs-config entry.  We stop
	 * when we get a FDT_END tag or when the level passes the end of
	 * the cs-config level.
	 */
	while (((tag = fdt_next_tag(gd->fdt_blob, node_offset,
				    &next_node_offset)) != FDT_END) &&
	       (level >= 0)) {
		switch (tag) {
		case FDT_BEGIN_NODE:
			level++;
			debug("begin, level=%d\n", level);
			break;
		case FDT_END_NODE:
			debug("end, level=%d\n", level);
			level--;
			/* Bail out if we reach level 0 again */
			if (level == 0)
				level = -1;
			break;
		case FDT_NOP:
			debug("nop\n");
		default:
			break;
		case FDT_PROP:
			debug("property, level=%d\n", level);
			/* All the properties should be at level 1 */
			if (level == 1) {
				debug("Getting property...\n");
				fdt_prop = fdt_offset_ptr(gd->fdt_blob,
							  node_offset,
							  sizeof(*fdt_prop));
				len = fdt32_to_cpu(fdt_prop->len);
				nodep = fdt_prop->data;
				prop_str = fdt_string(gd->fdt_blob,
						      fdt32_to_cpu(fdt_prop->nameoff));
				debug("Property string: %s\n", prop_str);
				if (len < 0) {
					printf("FDT property error: %s\n",
					       fdt_strerror(len));
					return -1;
				}
				/* Keep track of the property number */
				prop_number++;
				/* NOTE: The compatible field MUST be the first
				 * property.  We skip this comparison if it's
				 * not the first property to speed things up.
				 */
				if (prop_number == 1 &&
				    !strcmp(prop_str, "compatible") &&
				    len > 0) {
					if (strcmp((char *)nodep,
						   OCTEON_BOOT_BUS_CS_CONFIG_COMPAT)) {
						printf("Unknown compatible string \"%s\" for bootbus chip select %d",
						       (char *)nodep, chip_sel);
						return -1;
					}
					compat_found = 1;
					debug("Valid compatible string %s found\n",
					      (char *)nodep);
					break;
				}
				/* Validate the cavium,cs-index parameter.  We
				 * use cs_index to see if we've validated this
				 * or not so we can skip the string compare if
				 * we've already seen it.
				 */
				if (cs_index == 0 &&
				    !strcmp(prop_str, "cavium,cs-index")) {
					if (len == 4) {
						value = fdt32_to_cpu(*(uint32_t *)nodep);
						if (value != chip_sel) {
							printf("%s = <%d> does not match expected chip select %d\n",
							       prop_str, value,
							       chip_sel);
							return -1;
						}
					} else {
						printf("Invalid value type for property %s\n",
						       prop_str);
						return -1;
					}
					cs_index = 1;
					break;
				}

				/* Convert the string to an enumerator.  We
				 * could speed things up a bit here and not
				 * use enumerators and cache already processed
				 * strings.
				 */
				prop = prop_to_config(prop_str);
				debug("prop=%d\n", prop);
				if (len == 4) {
					value = fdt32_to_cpu(*(uint32_t *)nodep);
					debug("property \"%s\"(%d) = %d (0x%x)\n",
					      prop_str, prop, value, value);
					if ((prop != BUS_WIDTH) && (prop != PAGES))
						value = BOOT_BUS_TIME_ENCODE(value);
				} else {
					/* Some properties are modes only.  We
					 * could validate this but don't.
					 */
					value = 0xFFFFFFFF;
				}
				switch (prop) {
				case T_ADR:
					debug("Setting ADR to %d\n", value);
					reg_tim->s.adr = value;
					break;
				case T_ALE:
					debug("Setting ALE to %d\n", value);
					reg_tim->s.ale = value;
					break;
				case T_CE:
					debug("Setting CE to %d\n", value);
					reg_tim->s.ce = value;
					break;
				case T_OE:
					debug("Setting OE to %d\n", value);
					reg_tim->s.oe = value;
					break;
				case T_WE:
					debug("Setting WE to %d\n", value);
					reg_tim->s.we = value;
					break;
				case T_RD_HLD:
					debug("Setting RD_HLD to %d\n", value);
					reg_tim->s.rd_hld = value;
					break;
				case T_WR_HLD:
					debug("Setting WR_HLD to %d\n", value);
					reg_tim->s.wr_hld = value;
					break;
				case T_PAUSE:
					debug("Setting PAUSE to %d\n", value);
					reg_tim->s.pause = value;
					break;
				case T_WAIT:
					debug("Setting WAIT to %d\n", value);
					reg_tim->s.wait = value;
					break;
				case T_PAGE:
					debug("Setting PAGE to %d\n", value);
					reg_tim->s.page = value;
					break;
				case T_RD_DLY:
					debug("Setting RD_DLY to %d\n", value);
					if (reg_cfg)
						reg_cfg->s.rd_dly = value;
					break;
				case PAGES:
					debug("Setting PAGES to %d\n", value);
					reg_tim->s.pages = value;
					break;
				case BUS_WIDTH:
					if (NULL != reg_cfg) {
						reg_cfg->s.width = (value == 16);
						debug("Setting bus width to %d\n",
						      value);
					}
					break;
				case WAIT_MODE:
					debug("Enabling WAIT mode\n");
					reg_tim->s.waitm = 1;
					break;
				case PAGE_MODE:
					debug("Enabling PAGE mode\n");
					reg_tim->s.pagem = 1;
					break;
				case ALE_MODE:
					if (reg_cfg != NULL) {
						debug("Enabling ALE\n");
						reg_cfg->s.ale = 1;
					}
					break;
				case UNKNOWN:
					printf("Unknown FDT property %s\n",
					       prop_str);
				}
			}
		}
		node_offset = next_node_offset;
	}
	if (compat_found)
		return 0;
	else {
		printf("No compatible FDT string found for boot bus chip select %d\n",
		       chip_sel);
		printf("Expected to find %s\n",
			OCTEON_BOOT_BUS_CS_CONFIG_COMPAT);
		return -1;
	}
}

/**
 * Gets the FDT bus width for the specified chip select
 *
 * @param chip_sel - chip select to look up
 * @param[out] width - width of chip select
 * @return 0 for success, -1 on error.
 */
int octeon_boot_bus_get_fdt_bus_width(int chip_sel, int *width)
{
	uint32_t value;
	if (!octeon_boot_bus_get_property(chip_sel, BUS_WIDTH, &value)) {
		*width = value;
		return 0;
	} else
		return -1;
}

/**
 * Gets the ALE mode for the specified chip select
 *
 * @param chip_sel - chip select to look up
 * @param[out] ale_mode - 1 if enabled, 0 otherwise
 *
 * @return 0 for success, -1 on error.
 */
int octeon_boot_bus_get_fdt_ale_mode(int chip_sel, int *ale_mode)
{
	uint32_t value;
	if (!octeon_boot_bus_get_property(chip_sel, BUS_WIDTH, &value)) {
		*ale_mode = value;
		return 0;
	} else
		return -1;
}

/**
 * Outputs device information about the boot bus device specified in the path
 *
 * @param path - path of boot bus device.  Should be /soc/bootbus/devname.
 * @param compatible - compatible string, or NULL to skip compatibility check
 * @param[out] addr - address of device on the boot bus
 * @param[out] size - size of the device address space on the boot bus
 *
 * @return 0 for success, -1 on error
 */
int octeon_boot_bus_get_dev_info(const char *path, const char *compatible,
				 uint64_t *addr, uint32_t *size)
	__attribute__((weak, alias("__octeon_boot_bus_get_dev_info")));

int __octeon_boot_bus_get_dev_info(const char *path, const char *compatible,
				   uint64_t *addr, uint32_t *size)
{
	int node_offset;
	const void *nodep;
	int cs = -1;
	int len;

	node_offset = fdt_path_offset(gd->fdt_blob, path);
	if (node_offset < 0) {
		debug("Device not found in FDT path %s\n", path);
		return -1;
	}

	if (compatible && fdt_node_check_compatible(gd->fdt_blob,
						    node_offset, compatible)) {
		printf("Could not find device compatible with %s in device "
		       "tree path %s\n", compatible, path);
		return -1;
	}

	nodep = fdt_getprop(gd->fdt_blob, node_offset, "reg", &len);
	if (!nodep || len < 4) {
		printf("Could not obtain reg property from %s in FDT\n", path);
		return -1;
	}

	cs = fdt32_to_cpu(*((int *)nodep));
	return octeon_boot_bus_get_range(cs, -1, addr, size);
}

/**
 * Generic boot bus device initialization
 *
 * @param fdt_addr - device tree address
 * @param node_offset - offset of node in device tree for this device
 * @param bootbus_node_offset - node offset of /soc/bootbus, can be -1
 * @param param - NULL terminated array of strings to check
 *                for compatibility, may be NULL for no compatibility checking.
 * @param[out] dev_addr - address of device on the boot bus, can be NULL.
 *
 * @return 0 for success, -1 on error.
 *
 * This is a generic function that can be used to initialize many different
 * devices on the boot bus using information from the flat device tree.
 * It is safe to call this before relocation to RAM.  The function is also
 * weak and can be redefined elsewhere.
 */
int octeon_boot_bus_generic_init(void *fdt_addr, int node_offset,
				 int bootbus_node_offset, void *param,
				 uint64_t *dev_addr)
	__attribute__((weak, alias("__octeon_boot_bus_generic_init")));

int __octeon_boot_bus_generic_init(void *fdt_addr, int node_offset,
				   int bootbus_node_offset, void *param,
				   uint64_t *dev_addr)
{
	cvmx_mio_boot_reg_cfgx_t reg_cfg;
	cvmx_mio_boot_reg_timx_t reg_tim;
	uint64_t clock_mhz;
	uint64_t clock_period;
	uint64_t addr = 0;
	uint32_t size = 0;
	int cs = -1;
	const void *nodep;
	int len = 0;
	int compat_found = 0;
	char **compat_names = (char **)param;
	const char *compat_name = NULL;
	const char *name = fdt_get_name(fdt_addr, node_offset, NULL);

	if (compat_names) {
		while (*compat_names) {
			debug("%s: Checking %s compatiblity with %s\n",
			      __func__, name, *compat_names);
			if (!fdt_node_check_compatible(gd->fdt_blob,
						       node_offset,
						       *compat_names)) {
				debug("%s: Found device in %s compatible with %s\n",
				      __func__, name, *compat_names);
				compat_found = 1;
				compat_name = *compat_names;
				break;
		       }
		       compat_names++;
		}
		if (!compat_found) {
			printf("%s: Could not find boot bus compatible device %s\n",
			       __func__, name);
			return -1;
		}
	}

	nodep = fdt_getprop(gd->fdt_blob, node_offset, "reg", &len);
	if (!nodep || len < 4) {
		printf("Could not read reg property for %s from flat device tree\n",
		       name);
		return -1;
	}
	cs = fdt32_to_cpu(*((int *)nodep));
	octeon_boot_bus_get_range(cs, bootbus_node_offset, &addr, &size);
	if (cs == -1 || addr == 0 || size == 0) {
		printf("Could not obtain range information and/or chip select "
		       "for %s, cs = %d\n", name, cs);
		return -1;
	}
	if (dev_addr)
		*dev_addr = addr;
	reg_cfg.u64 = 0;
	reg_cfg.s.en = 1;
	if (!OCTEON_IS_MODEL(OCTEON_CN3XXX) && !OCTEON_IS_MODEL(OCTEON_CN58XX))
		reg_cfg.s.tim_mult = 3;	/* Set timing multiplier to 8x */
	/* Mask to put physical address in boot bus window */
	reg_cfg.s.base = (addr >> 16) & 0x1fff;
	reg_cfg.s.size = (size >> 16) - 1;

	clock_mhz = cvmx_clock_get_rate(CVMX_CLOCK_SCLK) / 1000000;
	clock_period = 1000000 / clock_mhz;

	reg_tim.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_TIMX(cs));

	octeon_boot_bus_get_fdt_info(cs, clock_period, &reg_cfg, &reg_tim);

	cvmx_write_csr(CVMX_MIO_BOOT_REG_TIMX(cs), reg_tim.u64);
	cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(cs), reg_cfg.u64);
	debug("Initialized timing for %s (compatible with %s) on boot bus\n",
	      name, compat_name ? compat_name : "(unknown)");
	return 0;
}

#ifdef CONFIG_OCTEON_ENABLE_LED_DISPLAY
/**
 * Maps the 8 character LED display on the boot bus.
 *
 * @param fdt_addr - device tree address
 * @param node_offset - offset of node in device tree for this device
 * @param bootbus_node_offset - node offset of /soc/bootbus, can be -1
 * @param param - Set to NULL to support the Avago part or pass in a
 *                NULL terminated array of compatible strings for the FDT.
 * @param[out] dev_addr - address of LED display on the boot bus (can be NULL)
 * @return 0 for success, !0 for error
 *
 * This just ends up calling the generic function with the avago compatibility
 * string.
 */
int octeon_boot_bus_led_init(void *fdt_addr, int node_offset,
			     int bootbus_node_offset, void *param,
			     uint64_t *dev_addr)
	__attribute__ ((weak, alias("__octeon_boot_bus_led_init")));

int __octeon_boot_bus_led_init(void *fdt_addr, int node_offset,
			       int bootbus_node_offset, void *param,
			       uint64_t *dev_addr)
{
	uint64_t addr;
	static const char *led_compat[] = { "avago,hdsp-253x", NULL };
	int rc;


	rc = octeon_boot_bus_generic_init(fdt_addr, node_offset,
					  bootbus_node_offset,
					  param ? param : (void *)led_compat,
					  &addr);
	if (!rc) {
		gd->ogd.led_addr = addr;
		if (dev_addr)
			*dev_addr = addr;
	}
	return rc;
}
#endif /* CONFIG_OCTEON_ENABLE_LED_DISPLAY */

#ifndef CONFIG_SYS_NO_FLASH
/**
 * Maps the NOR flash on the boot bus and configures the timing.
 *
 * @param fdt_addr - device tree address
 * @param node_offset - offset of node in device tree for this device
 * @param bootbus_node_offset - node offset of /soc/bootbus, can be -1
 * @param param - Not used, set to NULL.
 * @param[out] dev_addr - address of flash on the boot bus, can be NULL
 *
 * @return 0 for success, !0 for error
 *
 */
int octeon_boot_bus_flash_init(void *fdt_addr, int node_offset,
			       int bootbus_node_offset, void *param,
			       uint64_t *dev_addr)
	__attribute__ ((weak, alias("__octeon_boot_bus_flash_init")));

int __octeon_boot_bus_flash_init(void *fdt_addr, int node_offset,
				 int bootbus_node_offset, void *param,
				 uint64_t *dev_addr)
{
	cvmx_mio_boot_reg_cfgx_t __attribute__ ((unused)) reg_cfg;
	cvmx_mio_boot_reg_timx_t __attribute__ ((unused)) reg_tim;
	uint64_t clock_mhz;
	uint64_t clock_period;
	uint64_t addr;
	uint32_t len = 0;
	const void *nodep;
	int cs = 0;
#ifdef CONFIG_OCTEON_ENABLE_DT_FLASH_CHECKS
	uint64_t reg_addr;
	uint32_t reg_len;
	int reg_node_len = 0;
	const uint32_t *regp;
#endif

	debug("%s: Initializing flash on boot bus\n", __func__);
#ifdef CONFIG_OCTEON_NAND_STAGE2
	cs = -1;
#endif
	clock_mhz = cvmx_clock_get_rate(CVMX_CLOCK_SCLK) / 1000000;
	clock_period = 1000000 / clock_mhz;	/* clk period (psecs) */

	if (fdt_node_check_compatible(gd->fdt_blob, node_offset,
				      "cfi-flash")) {
		printf("Incompatible NOR flash type in FDT\n");
		return -1;
	}
	nodep = fdt_getprop(fdt_addr, node_offset, "reg", (int *)&len);
	if (nodep && len >= 4) {
		cs = fdt32_to_cpu(*((int *)nodep));
	}
	if (cs < 0) {
		printf("No CFI flash found\n");
		return -1;
	}
	if (octeon_boot_bus_get_range(cs, bootbus_node_offset, &addr, &len)) {
		printf("Could not initialize flash from fdt, invalid range\n");
#if defined(CONFIG_SYS_FLASH_BASE) && defined(CONFIG_SYS_FLASH_SIZE)
		addr = CONFIG_SYS_FLASH_BASE;
		len = CONFIG_SYS_FLASH_SIZE;
#else
		puts("Could not get CFI flash address or length from FDT.\n"
		     "CFI flash not configured.\n");
		return -1;
#endif
	}
	debug("%s: flash range address: 0x%llx, length: 0x%x, cs: %d\n",
	      __func__, addr, len, cs);
	if (dev_addr)
		*dev_addr = addr;
#if defined(CONFIG_OCTEON_ENABLE_DT_FLASH_CHECKS) && CONFIG_SYS_FLASH_BASE
	if (addr != CONFIG_SYS_FLASH_BASE) {
		printf("ERROR! CONFIG_SYS_FLASH_BASE address 0x%x does not "
		       "match device tree range address 0x%llx for cs %d!\n",
		       CONFIG_SYS_FLASH_BASE, addr, cs);
		return -1;
	}
#endif
#if defined(CONFIG_OCTEON_ENABLE_DT_FLASH_CHECKS) && CONFIG_SYS_FLASH_SIZE
	if (len != CONFIG_SYS_FLASH_SIZE + 0x400000) {
		printf("ERROR! CONFIG_SYS_FLASH_SIZE size 0x%x + 0x400000 does "
		       "not match device tree range size 0x%x for cs %d!\n",
		       CONFIG_SYS_FLASH_SIZE, len, cs);
		return -1;
	}
#endif
	/* Validate flash address and length are valid for the 256MB boot bus
	 * window.  If they're invalid then the range in the FDT might be
	 * missing or invalid.
	 */
	if (len < 0x100000 || addr < 0x10000000 || addr >= 0x20000000) {
#if defined(CONFIG_SYS_FLASH_BASE) && defined(CONFIG_SYS_FLASH_SIZE)
		addr = CONFIG_SYS_FLASH_BASE;
		len = CONFIG_SYS_FLASH_SIZE;
		printf("Using hard-coded flash address 0x%llxand length 0x%x\n",
		       addr, len);
#else
		printf("CFI flash length 0x%x and address 0x%llx are out of "
		       "range\n",
		       len, addr);
		puts("CFI flash not configured\n");
		return -1;
#endif
	}
#ifdef CONFIG_OCTEON_ENABLE_DT_FLASH_CHECKS
	/* Validate that the reg field is present and valid */
	regp = (uint32_t *)fdt_getprop(gd->fdt_blob, node_offset,
				       "reg", &reg_node_len);
	if (!regp || reg_node_len < 12) {
		printf("%s: Error reading flash reg parameter in device tree\n",
		       __func__);
		return -1;
	}
	if (cs != regp[0]) {
		printf("%s: Error: invalid chip select in flash reg property, "
		       "read %d, should be %d\n", __func__, regp[0], cs);
		return -1;
	}
	reg_addr = fdt32_to_cpu(regp[1]);
	if (reg_node_len == 16)
		reg_addr = (reg_addr << 32) | fdt32_to_cpu(regp[2]);
	reg_len = fdt32_to_cpu(regp[reg_node_len == 16 ? 3 : 2]);

	debug("Flash cs: %d, reg addr: 0x%llx, reg length: 0x%x\n", regp[0],
	      reg_addr, reg_len);
	if (reg_addr != 0) {
		printf("ERROR! Device tree flash reg addr 0x%llx should be 0 "
		       "for cs %d!\n",
		       reg_addr, cs);
# ifdef CONFIG_SYS_FLASH_BASE
		printf("CONFIG_SYS_FLASH_BASE is set to 0x%x\n",
		       CONFIG_SYS_FLASH_BASE);
# endif
		return -1;
	}

	if (reg_len != len - 0x400000) {
		printf("ERROR! Device tree flash reg size 0x%x does not "
		       "match range size 0x%x - 0x400000 for cs %d!\n",
		       reg_len, len, cs);
# ifdef CONFIG_SYS_FLASH_SIZE
		printf("CONFIG_SYS_FLASH_SIZE is set to 0x%x\n",
		       CONFIG_SYS_FLASH_SIZE);
# endif
		return -1;
	}
#endif
	/* Store flash base address and size in global data */
	gd->ogd.flash_base_address = addr;
	gd->ogd.flash_size = len - 0x400000;

	/* Remap flash part so that it is all addressable on boot bus, with
	 * alias at 0x1fc00000 so that the data mapped at the default location
	 * (0x1fc00000) is still available at the same address.  Note that this
	 * is different if a TLB mapped u-boot (see below.)
	 */
	reg_cfg.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFGX(cs));
	if (cs != 0)
		reg_cfg.s.en = 1;

	/* Don't set enable, as for NAND boot CS0 will be disabled,
	 * and enabling it here complicates things.  We still have to handle
	 * the case of NAND on CS0 and it enabled, as this can happen when
	 * in NAND boot mode but the board is booted over pci/ejtag.
	 */
	if (!OCTEON_IS_MODEL(OCTEON_CN3XXX) && !OCTEON_IS_MODEL(OCTEON_CN58XX))
		reg_cfg.s.tim_mult = 3;		/* Set to 8x */
	reg_cfg.s.rd_dly = 0;
	reg_cfg.s.we_ext = 0;
	reg_cfg.s.oe_ext = 0;
	reg_cfg.s.orbit = 0;
	/* In 64k blocks, + 4MByte alias of low 4Mbytes of flash */
	reg_cfg.s.size = (len >> 16) - 1;
	/* Mask to put physical address in boot bus window */
	reg_cfg.s.base = ((addr >> 16) & 0x1fff);
	if (!(gd->flags & GD_FLG_RAM_RESIDENT) && (cs == 0)) {
		/* If booting from RAM, we want to set the boot bus base to the
		 * normal, aliased value.  Don't do this if NOR is non-bootable.
		 */
		/* Base has previously been adjusted to have the booting u-boot
		 * image starting at 0x1FC00000 so that we can TLB map it with
		 * a single entry.  This will be adjusted to the normal
		 * settings once we are running from RAM and don't need the TLB
		 * mapping to point to flash.
		 */
		cvmx_mio_boot_reg_cfgx_t __attribute__ ((unused)) reg_cfg1;
		reg_cfg1.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFGX(cs));

		reg_cfg.s.base =
			reg_cfg1.s.base - ((len - 0x400000) >> 16);
		debug("Running from RAM, setting base to 0x%x\n",
		      reg_cfg.s.base << 16);
	}
	debug("%s: Writing boot reg: cfg(%d) 0x%llx\n",
	      __func__, cs, reg_cfg.u64);
	cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(cs), reg_cfg.u64);

	/* Set timing to be valid for all CPU clocks up to clock_period */
	reg_tim.u64 = 0;
	reg_tim.s.pagem = 0;
	reg_tim.s.wait = BOOT_BUS_TIME_ENCODE(-1);
	reg_tim.s.adr = BOOT_BUS_TIME_ENCODE(10ULL);
	reg_tim.s.pause = 0;
	reg_tim.s.ce = BOOT_BUS_TIME_ENCODE(50ULL);

	/* If the board uses ALE for flash support then leave the timing
	 * alone. For example, the Embedded Planet EP6300C board needs this.
	 */
# ifndef CONFIG_OCTEON_FLASH_USES_ALE
	if (octeon_is_model(OCTEON_CN31XX))
# endif
	{
		reg_tim.s.ale = BOOT_BUS_TIME_ENCODE(34ULL);
	}

	reg_tim.s.oe = BOOT_BUS_TIME_ENCODE(50ULL);
	reg_tim.s.rd_hld = BOOT_BUS_TIME_ENCODE(25ULL);
	reg_tim.s.wr_hld = BOOT_BUS_TIME_ENCODE(35ULL);
	reg_tim.s.we = BOOT_BUS_TIME_ENCODE(35ULL);
	reg_tim.s.page = BOOT_BUS_TIME_ENCODE(25ULL);

	/* Override timing with FDT settings if present */
	octeon_boot_bus_get_fdt_info(cs, clock_period, NULL, &reg_tim);

	debug("%s: Old timing reg:     0x%llx\n",
	      __func__, cvmx_read_csr(CVMX_MIO_BOOT_REG_TIMX(cs)));
	debug("%s: Writing timing reg: 0x%llx\n", __func__, reg_tim.u64);
	cvmx_write_csr(CVMX_MIO_BOOT_REG_TIMX(cs), reg_tim.u64);
	debug("%s: Done.\n", __func__);
	return 0;
}

/**
 * Enables page mode accesses to the CFI flash.
 *
 * NOTE: Page mode MUST be DISABLED before writing to the flash,
 * erasing the flash, protecting or unprotecting the flash or performing
 * any other operation that makes use of the CFI registers.
 *
 * This is used to speed up read operations.
 */
void __octeon_boot_bus_flash_set_page_mode(int enable)
{
	cvmx_mio_boot_reg_timx_t __attribute__ ((unused)) reg_tim;

	reg_tim.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_TIM0);
	if (enable) {
		reg_tim.s.pagem = 1;
		reg_tim.s.pages = 3;	/* 8 bytes/shorts */
	} else {
		reg_tim.s.pagem = 0;
		reg_tim.s.pages = 0;
	}
}

void octeon_boot_bus_flash_set_page_mode(int enable)
	__attribute__ ((weak, alias("__octeon_boot_bus_flash_set_page_mode")));
#endif /* !CONFIG_SYS_NO_FLASH */

#if defined(CONFIG_CMD_IDE)
/**
 * Initialize the compact flash in true IDE mode on the boot bus
 *
 * @param fdt_addr - device tree address
 * @param node_offset - offset of node in device tree for this device
 * @param bootbus_node_offset - node offset of /soc/bootbus, can be -1
 * @param[out] dev_addr - address of CF CS0 on boot bus, can be NULL
 *
 * @return 0 for success, !0 for error
 *
 */
int octeon_boot_bus_true_ide_init(void *fdt_addr, int node_offset,
				  int bootbus_node_offset, void *param,
				  uint64_t *dev_addr)
	__attribute__ ((weak, alias("__octeon_boot_bus_true_ide_init")));

int __octeon_boot_bus_true_ide_init(void *fdt_addr, int node_offset,
				    int bootbus_node_offset,
				    void *param, uint64_t *dev_addr)
{
	cvmx_mio_boot_reg_cfgx_t __attribute__ ((unused)) reg_cfg;
	cvmx_mio_boot_reg_timx_t __attribute__ ((unused)) reg_tim;
	uint64_t clock_mhz;
	uint64_t clock_period;
	uint64_t cs0_addr, cs1_addr;
	uint32_t cs0_size, cs1_size;
	const void *nodep;
	const uint32_t *reset_nodep;
	int phandle;
	int len;
	int cs0, cs1;
	int rc;
	uint64_t dma_reg;
	int dma_channel;
	int reset_gpio = -1;

	clock_mhz = cvmx_clock_get_rate(CVMX_CLOCK_SCLK) / 1000000;
	clock_period = 1000000 / clock_mhz;	/* clk period (psecs) */

#ifdef OCTEON_CF_TRUE_IDE_CS0_CHIP_SEL
	cs0 = OCTEON_CF_TRUE_IDE_CS0_CHIP_SEL;
#else
	cs0 = -1;
#endif
#ifdef OCTEON_CF_TRUE_IDE_CS1_CHIP_SEL
	cs1 = OCTEON_CF_TRUE_IDE_CS1_CHIP_SEL;
#else
	cs1 = -1;
#endif
#if OCTEON_CF_TRUE_IDE_CS0_ADDR
	cs0_addr = OCTEON_CF_TRUE_IDE_CS0_ADDR;
#else
	cs0_addr = 0;
#endif
#if OCTEON_CF_TRUE_IDE_CS1_ADDR
	cs1_addr = OCTEON_CF_TRUE_IDE_CS1_ADDR;
#else
	cs1_addr = 0;
#endif

	if (fdt_node_check_compatible(fdt_addr, node_offset,
				      "cavium,ebt3000-compact-flash")) {
		printf("Incompatible compact flash type in FDT\n");
		return 0;
	}

	nodep = fdt_getprop(fdt_addr, node_offset,
			    "cavium,true-ide", &len);
	if (!nodep) {
		debug("True IDE mode not enabled for compact flash\n");
		return 0;
	}

	nodep = fdt_getprop(fdt_addr, node_offset, "reg", &len);
	if (nodep && len >= 12) {
		cs0 = fdt32_to_cpu(((int *)nodep)[0]);
		if (octeon_boot_bus_get_range(cs0, bootbus_node_offset,
					      &cs0_addr, &cs0_size) < 0) {
			printf("Error: Cannot get base address for "
			       "compact-flash chip select 0 %d\n", cs0);
		}
		if (len == 24) {
			cs1 = fdt32_to_cpu(((int *)nodep)[3]);
			if (octeon_boot_bus_get_range(cs1, bootbus_node_offset,
						      &cs1_addr,
						      &cs1_size) < 0) {
				printf("Error: Cannot get base address for "
				       "compact-flash chip select 1 %d\n", cs1);
			}
		}
	}
	if (dev_addr)
		*dev_addr = cs0_addr;

	debug("%s: FDT cs0: %d, cs1: %d, cs0 address: 0x%llx, cs0 size: 0x%x, cs1 address: 0x%llx, cs1 size: 0x%x\n",
	      __func__, cs0, cs1, cs0_addr, cs0_size, cs1_addr, cs1_size);

	if (cs0 >= 0) {
		debug("%s: chip select 0 is %d\n", __func__, cs0);
		reg_cfg.u64 = 0;
		reg_cfg.s.en = 1;
		if (!OCTEON_IS_MODEL(OCTEON_CN3XXX)
		    && !OCTEON_IS_MODEL(OCTEON_CN58XX))
			reg_cfg.s.tim_mult = 3;		/* Set to 8x */
		reg_cfg.s.width = 1;
		/* Mask to put physical address in boot bus window */
		reg_cfg.s.base = ((cs0_addr >> 16) & 0x1fff);
		if (cs0_size >= 0x10000)
			cs0_size -= 0x10000;
		reg_cfg.s.size = (cs0_size >> 16);

		reg_tim.u64 = 0;
		reg_tim.s.wait = BOOT_BUS_TIME_ENCODE(-1);
		reg_tim.s.page = BOOT_BUS_TIME_ENCODE(-1);
		reg_tim.s.wr_hld = BOOT_BUS_TIME_ENCODE(-1);
		reg_tim.s.rd_hld = BOOT_BUS_TIME_ENCODE(100);
		reg_tim.s.we = BOOT_BUS_TIME_ENCODE(150);
		reg_tim.s.oe = BOOT_BUS_TIME_ENCODE(125);
		reg_tim.s.ce = BOOT_BUS_TIME_ENCODE(-1);

		octeon_boot_bus_get_fdt_info(cs0, clock_period,
					     &reg_cfg, &reg_tim);

		cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(cs0), reg_cfg.u64);
		cvmx_write_csr(CVMX_MIO_BOOT_REG_TIMX(cs0), reg_tim.u64);
	}


	if (cs1 < 0) {
		debug("Chip select 1 for true IDE mode not configured\n");
		return -1;
	}

	reg_cfg.u64 = 0;
	reg_cfg.s.en = 1;
	if (!OCTEON_IS_MODEL(OCTEON_CN3XXX) && !OCTEON_IS_MODEL(OCTEON_CN58XX))
		reg_cfg.s.tim_mult = 3;		/* Set to 8x */
	reg_cfg.s.width = 1;	/* Always 16-bit */
	/* Mask to put physical address in boot bus window */
	reg_cfg.s.base = ((cs1_addr >> 16) & 0x1fff);
	reg_cfg.s.size = (cs1_size >> 16) - 1;

	reg_tim.u64 = 0;
	reg_tim.s.wait = BOOT_BUS_TIME_ENCODE(30);
	reg_tim.s.page = BOOT_BUS_TIME_ENCODE(-1);
	reg_tim.s.wr_hld = BOOT_BUS_TIME_ENCODE(30);
	reg_tim.s.rd_hld = BOOT_BUS_TIME_ENCODE(100);
	reg_tim.s.we = BOOT_BUS_TIME_ENCODE(150);
	reg_tim.s.oe = BOOT_BUS_TIME_ENCODE(125);
	reg_tim.s.ce = BOOT_BUS_TIME_ENCODE(30);

	octeon_boot_bus_get_fdt_info(cs1, clock_period, &reg_cfg, &reg_tim);

	cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(cs1), reg_cfg.u64);
	cvmx_write_csr(CVMX_MIO_BOOT_REG_TIMX(cs1), reg_tim.u64);

#if OCTEON_CF_RESET_GPIO
	reset_gpio = OCTEON_CF_RESET_GPIO;
#endif

	reset_nodep = fdt_getprop(gd->fdt_blob, node_offset,
				  "cavium,reset", &len);
	if (reset_nodep && len == 12) {
		reset_gpio = fdt32_to_cpu(reset_nodep[1]);
		debug("Compact flash cs %d reset GPIO is %d\n", cs0, reset_gpio);
	}

	/* Get the DMA channel */
	nodep = fdt_getprop(gd->fdt_blob, node_offset,
			    "cavium,dma-engine-handle", NULL);
	if (!nodep) {
		printf("DMA engine not defined for compact flash on chip select %d\n",
		       cs0);
		return -1;
	}

	phandle = fdt32_to_cpu(*(uint32_t *)nodep);
	node_offset = fdt_node_offset_by_phandle(gd->fdt_blob, phandle);
	if (node_offset < 0) {
		printf("Invalid phandle for compact flash DMA engine on chip select %d\n",
		       cs0);
		return -1;
	}

	if (fdt_node_check_compatible(gd->fdt_blob, node_offset,
				      "cavium,octeon-5750-bootbus-dma")) {
		printf("Incompatible DMA type for compact flash on chip select %d\n",
		       cs0);
		return -1;
	}
	nodep = fdt_getprop(gd->fdt_blob, node_offset, "reg", &len);
	if (len < 16) {
		printf("Invalid \"reg\" parameter for DMA channel for compact flash on chip select %d\n",
		       cs0);
		return -1;
	}
	debug("DMA register node address: 0x%p\n", nodep);
	memcpy(&dma_reg, nodep, sizeof(dma_reg));
	dma_reg = fdt64_to_cpu(dma_reg);
	debug("%s: DMA register address: 0x%llx\n", __func__, dma_reg);

	/* Map DMA register address to DMA channel number */
	switch (dma_reg) {
	case 0x1180000000100:
		dma_channel = 0;
		break;
	case 0x1180000000108:
		dma_channel = 1;
		break;
	default:
		printf("%s: Unknown DMA channel address 0x%llx for compact flash on chip select %d\n",
		       __func__, dma_reg, cs0);
		return -1;
	}

	if (!octeon_cf_present(cs0_addr)) {
		debug("Compact flash not present on chip select %d\n", cs0);
		return 0;
	}

	debug("%s: Registering true-IDE device for chip select %d, DMA channel %d\n",
	      __func__, cs0, dma_channel);
	rc = ide_register_device(IF_TYPE_IDE,
				 BLOCK_QUIRK_IDE_CF
				 | BLOCK_QUIRK_IDE_CF_TRUE_IDE
				 | BLOCK_QUIRK_IDE_CF_16BIT_BUS
				 | BLOCK_QUIRK_IDE_CF_TRUE_IDE_DMA,
				 0,	/* Only the primary */
				 MAKE_XKPHYS(cs0_addr),
				 MAKE_XKPHYS(cs0_addr),
				 0, dma_channel, reset_gpio);

	debug("%s: Done. rc=%d\n", __func__, rc);
	return rc >= 0 ? 0 : -1;
}

/**
 * Initialize the compact flash in non-True IDE mode on the boot bus
 *
 * @param fdt_addr - device tree address
 * @param node_offset - offset of node in device tree for this device
 * @param bootbus_node_offset - node offset of /soc/bootbus, can be -1
 * @param param - Not used, set to NULL.
 * @param[out] dev_addr - address of CF common area on boot bus, can be NULL
 *
 * @return 0 for success, !0 for error
 */
int octeon_boot_bus_cf_init(void *fdt_addr, int node_offset,
			    int bootbus_node_offset, void *param,
			    uint64_t *dev_addr)
	__attribute__ ((weak, alias("__octeon_boot_bus_cf_init")));

#if defined(CONFIG_SYS_ATA_REG_OFFSET) || defined(CONFIG_SYS_ATA_DATA_OFFSET)
int __octeon_boot_bus_cf_init(void *fdt_addr, int node_offset,
			      int bootbus_node_offset, void *param,
			      uint64_t *dev_addr)
{
	cvmx_mio_boot_reg_cfgx_t __attribute__ ((unused)) reg_cfg;
	cvmx_mio_boot_reg_timx_t __attribute__ ((unused)) reg_tim;
	uint64_t clock_mhz;
	uint64_t clock_period;
	uint64_t attrib_base_addr;
	uint64_t common_base_addr;
	int len = 0;
	int attr_cs, common_cs;
	const void *nodep = NULL;
	const uint32_t *reset_nodep;
	int bus_16bit;
	int rc;
	int reset_gpio = -1;

	clock_mhz = cvmx_clock_get_rate(CVMX_CLOCK_SCLK) / 1000000;
	clock_period = 1000000 / clock_mhz;	/* clk period (psecs) */

	/* Set up regions for CompactFlash */
	/* Attribute memory region */
#ifdef OCTEON_CF_ATTRIB_CHIP_SEL
	attr_cs = OCTEON_CF_ATTRIB_CHIP_SEL;
#else
	attr_cs = -1;
#endif
#ifdef OCTEON_CF_ATTRIB_BASE_ADDR
	attrib_base_addr = OCTEON_CF_ATTRIB_BASE_ADDR;
#else
	attrib_base_addr = 0;
#endif
#ifdef OCTEON_CF_COMMON_CHIP_SEL
	common_cs = OCTEON_CF_COMMON_CHIP_SEL;
#else
	common_cs = -1;
#endif
#ifdef OCTEON_CF_COMMON_BASE_ADDR
	common_base_addr = OCTEON_CF_COMMON_BASE_ADDR;
#else
	common_base_addr = 0;
#endif

#ifdef OCTEON_CF_16_BIT_BUS
	bus_16bit = 1;
#else
	bus_16bit = 0;
#endif

	if (fdt_node_check_compatible(fdt_addr, node_offset,
				      "cavium,ebt3000-compact-flash")) {
		printf("Incompatible compact flash type in FDT\n");
		return -1;
	}
	/* See if we're in True-IDE mode, if so bail out. */
	nodep = fdt_getprop(fdt_addr, node_offset,
			    "cavium,true-ide", &len);
	if (nodep)
		return octeon_boot_bus_true_ide_init(fdt_addr, node_offset,
						     bootbus_node_offset,
						     param, dev_addr);

	/* Get the chip enables */
	nodep = fdt_getprop(fdt_addr, node_offset, "reg", &len);
	/* If there's 2 registers then both the common and attribute
	 * chip enables are defined, otherwise there's just a common
	 * chip enable.
	 */
	if (nodep && (len == 24)) {
		/* Get attribute chip enable and base address */
		attr_cs = fdt32_to_cpu(((int *)nodep)[3]);
		debug("Compact flash attribute chip enable: %d\n",
		      attr_cs);
		if (octeon_boot_bus_get_range(attr_cs, bootbus_node_offset,
					      &attrib_base_addr,
					      NULL) < 0) {
			printf("Error: Cannot get base address for "
			       "compact-flash attribute chip enable %d\n",
			       attr_cs);
		} else {
			debug("Compat flash attribute address 0x%llx\n",
			      attrib_base_addr);
		}
	}

	if (nodep && (len >= 12)) {
		/* Get common chip enable and base address */
		common_cs = fdt32_to_cpu(((int *)nodep)[0]);
		debug("Compact flash common chip enable: %d\n", common_cs);
		if (octeon_boot_bus_get_range(common_cs, bootbus_node_offset,
					      &common_base_addr, NULL) < 0) {
			printf("Error: cannot get base address for "
			       "compact-flash common chip enable %d\n",
			       common_cs);
		}
		common_base_addr |= (1 << 11);
	} else {
		printf("Invalid register size in device tree for "
		       "compact flash\n");
	}

	if (dev_addr)
		*dev_addr = common_base_addr;

	/* Get bus width */
	nodep = fdt_getprop(fdt_addr, node_offset,
			    "cavium,bus-width", &len);
	if (nodep)
		bus_16bit = (fdt32_to_cpu(*(uint32_t *)nodep) == 16);

	if (common_cs == -1) {
		debug("No compact-flash common chip enable defined.\n");
		return -1;
	}

	/* Attribute chip enable is optional */
	if (attr_cs >= 0) {
		reg_cfg.u64 = 0;
		reg_cfg.s.en = 1;
		if (!OCTEON_IS_MODEL(OCTEON_CN3XXX)
		    && !OCTEON_IS_MODEL(OCTEON_CN58XX))
			reg_cfg.s.tim_mult = 3;		/* Set to 8x */
		reg_cfg.s.width = bus_16bit ? 1 : 0;
		debug("CF attribute CS: %d, width: %d, address: 0x%llx\n",
		      attr_cs, bus_16bit ? 16 : 8, attrib_base_addr);

		/* Mask to put physical address in boot bus window */
		reg_cfg.s.base = ((attrib_base_addr >> 16) & 0x1fff);

		reg_tim.u64 = 0;
		reg_tim.s.wait = BOOT_BUS_TIME_ENCODE(-1);
		reg_tim.s.page = BOOT_BUS_TIME_ENCODE(-1);
		reg_tim.s.wr_hld = BOOT_BUS_TIME_ENCODE(70ULL);
		reg_tim.s.rd_hld = BOOT_BUS_TIME_ENCODE(100ULL);
		reg_tim.s.we = BOOT_BUS_TIME_ENCODE(150ULL);
		reg_tim.s.oe = BOOT_BUS_TIME_ENCODE(270ULL);
		reg_tim.s.ce = BOOT_BUS_TIME_ENCODE(30ULL);

		octeon_boot_bus_get_fdt_info(attr_cs, clock_period,
					     &reg_cfg, &reg_tim);
		cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(attr_cs),
			       reg_cfg.u64);
		cvmx_write_csr(CVMX_MIO_BOOT_REG_TIMX(attr_cs),
			       reg_tim.u64);

	} else {
		debug("Compact flash does not have an attribute chip select.\n");
	}

	/* Common memory region */
	reg_cfg.u64 = 0;
	reg_cfg.s.en = 1;
	if (!OCTEON_IS_MODEL(OCTEON_CN3XXX) && !OCTEON_IS_MODEL(OCTEON_CN58XX))
		reg_cfg.s.tim_mult = 3;			/* Set to 8x */
	reg_cfg.s.width = bus_16bit ? 1 : 0;

	debug("CF common CS: %d, width: %d, address: 0x%llx\n",
	      attr_cs, bus_16bit ? 16 : 8, common_base_addr);

	/* Mask to put physical address in boot bus window */
	reg_cfg.s.base = ((common_base_addr >> 16) & 0x1fff);

	reg_tim.u64 = 0;
	reg_tim.s.wait = BOOT_BUS_TIME_ENCODE(30ULL);
	reg_tim.s.waitm = 1;
	reg_tim.s.page = BOOT_BUS_TIME_ENCODE(-1);
	reg_tim.s.wr_hld = BOOT_BUS_TIME_ENCODE(30ULL);
	reg_tim.s.rd_hld = BOOT_BUS_TIME_ENCODE(100ULL);
	reg_tim.s.we = BOOT_BUS_TIME_ENCODE(150ULL);
	reg_tim.s.oe = BOOT_BUS_TIME_ENCODE(125ULL);
	reg_tim.s.ce = BOOT_BUS_TIME_ENCODE(30ULL);

	if (fdt_addr)
		octeon_boot_bus_get_fdt_info(common_cs, clock_period,
					     &reg_cfg, &reg_tim);

	cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(common_cs), reg_cfg.u64);
	cvmx_write_csr(CVMX_MIO_BOOT_REG_TIMX(common_cs), reg_tim.u64);

	/* No DMA for now */
	if (!octeon_cf_present(common_base_addr + CONFIG_SYS_ATA_REG_OFFSET)) {
		debug("Compact flash card not present on chip select %d\n",
		      common_cs);
		return 0;
	}

#if OCTEON_CF_RESET_GPIO
	reset_gpio = OCTEON_CF_RESET_GPIO;
#endif

	reset_nodep = fdt_getprop(gd->fdt_blob, node_offset,
				  "cavium,reset", &len);
	if (reset_nodep && len == 12) {
		reset_gpio = fdt32_to_cpu(reset_nodep[1]);
		debug("Compact flash cs %d reset GPIO is %d\n",
		      common_cs, reset_gpio);
	}

	debug("%s: Registering IDE device for chip select %d\n",
	      __func__, common_cs);

	rc = ide_register_device(IF_TYPE_IDE,
				 BLOCK_QUIRK_IDE_CF
				 | (bus_16bit ?
				    BLOCK_QUIRK_IDE_CF_16BIT_BUS : 0),
				 0,
				 MAKE_XKPHYS(common_base_addr +
					     CONFIG_SYS_ATA_DATA_OFFSET),
				 MAKE_XKPHYS(common_base_addr +
					     CONFIG_SYS_ATA_REG_OFFSET),
				 0, 0, reset_gpio);

	debug("%s: Done. rc=%d\n", __func__, rc);
	return rc >= 0 ? 0 : -1;
}
#else
int __octeon_boot_bus_cf_init(void *fdt_addr, int node_offset,
			      int bootbus_node_offset, void *param,
			      uint64_t *dev_addr)
{
	const void *nodep;
	int len;

	if (fdt_node_check_compatible(fdt_addr, node_offset,
				      "cavium,ebt3000-compact-flash")) {
		printf("Incompatible compact flash type in FDT\n");
		return -1;
	}
	/* See if we're in True-IDE mode, if so bail out. */
	nodep = fdt_getprop(fdt_addr, node_offset,
			    "cavium,true-ide", &len);
	if (nodep)
		return octeon_boot_bus_true_ide_init(fdt_addr, node_offset,
						     bootbus_node_offset,
						     param, dev_addr);

	printf("Error: CONFIG_SYS_ATA_REG_OFFSET and/or "
	       "CONFIG_SYS_ATA_DATA_OFFSET are not defined in config.h "
	       "or cavium,true-ide is not defined in device tree for "
	       "compact flash!\n");
	return -1;
}
#endif
#endif /* CONFIG_CMD_IDE */

#ifdef CONFIG_OCTEON_ENABLE_PAL
/**
 * Initialize PAL on the boot bus
 *
 * @param fdt_addr - device tree address
 * @param node_offset - offset of node in device tree for this device
 * @param bootbus_node_offset - node offset of /soc/bootbus, can be -1
 * @param param - Not used, set to NULL.
 * @param[out] dev_addr - address of PAL on boot bus, can be NULL
 *
 * @return 0 for success, !0 for error
 */
int octeon_boot_bus_pal_init(void *fdt_addr, int node_offset,
			     int bootbus_node_offset, void *param,
			     uint64_t *dev_addr)
	__attribute__ ((weak, alias("__octeon_boot_bus_pal_init")));
int __octeon_boot_bus_pal_init(void *fdt_addr, int node_offset,
			       int bootbus_node_offset, void *param,
			       uint64_t *dev_addr)
{
	static const char *pal_compat[] = { "cavium,ebt3000-pal", NULL };
	uint64_t addr;
	int rc;

	rc = octeon_boot_bus_generic_init(fdt_addr, node_offset,
					  bootbus_node_offset,
					  (void *)pal_compat, &addr);
	if (!rc) {
		if (dev_addr)
			*dev_addr = addr;
		gd->ogd.pal_addr = addr;
	}
	return rc;
}
#endif /* CONFIG_OCTEON_ENABLE_PAL */

/**
 * Initializes devices on the boot bus and assignes them address space.
 *
 * Note that this function is defined as "weak".  If a board requires
 * separate initialization all that is required is to define this function
 * in the appropriate board file.  See the nicpro2 board for an example.
 */
int octeon_boot_bus_init(void)
	__attribute__((weak, alias("__octeon_boot_bus_init")));

int __octeon_boot_bus_init(void)
{
	cvmx_mio_boot_reg_cfgx_t reg_cfg;
	int node_offset, bootbus_node_offset;
	const char *name;
	int depth = 0;
	int num_devs = 0;
	int total_devs = 0;
#if !defined(CONFIG_SYS_NO_FLASH) && defined(CONFIG_SYS_FLASH_SIZE)
	total_devs++;
#endif
#ifdef CONFIG_OCTEON_ENABLE_PAL
	total_devs++;
#endif
#ifdef CONFIG_OCTEON_ENABLE_LED_DISPLAY
	total_devs++;
#endif

	debug("In %s\n", __func__);
	/* Drive boot bus chip enables [7:4] on gpio [11:8] */
	if (octeon_is_model(OCTEON_CN31XX) || octeon_is_model(OCTEON_CN30XX)
	    || octeon_is_model(OCTEON_CN50XX))
		cvmx_write_csr(CVMX_GPIO_BOOT_ENA, 0xf << 8);

	/* Change default timing to 8x */
	if (!OCTEON_IS_MODEL(OCTEON_CN3XXX)
	    && !OCTEON_IS_MODEL(OCTEON_CN58XX)) {
		int i;
		for (i = 0; i < 8; i++) {
			reg_cfg.u64 = cvmx_read_csr(CVMX_MIO_BOOT_REG_CFGX(i));
			reg_cfg.s.tim_mult = 3;		/* Set to 8x */
			cvmx_write_csr(CVMX_MIO_BOOT_REG_CFGX(i), reg_cfg.u64);
		}
	}

	if (!total_devs) {
		debug("No devices to init on the boot bus, skip device tree\n");
		return 0;
	}

	bootbus_node_offset = fdt_path_offset(gd->fdt_blob, "/soc/bootbus");
	if (bootbus_node_offset < 0) {
		puts("Error: /soc/bootbus not found in device tree\n");
		return -1;
	}
	node_offset = bootbus_node_offset;

	do {
		node_offset = fdt_next_node(gd->fdt_blob, node_offset,
					    &depth);

		if (depth == 1 && node_offset >= 0) {
			name = fdt_get_name(gd->fdt_blob, node_offset, NULL);
			debug("%s: Processing %s\n", __func__, name);
#if !defined(CONFIG_SYS_NO_FLASH) && defined(CONFIG_SYS_FLASH_SIZE)
			if (!strncmp(name, "nor", 3)) {
				debug("Initializing NOR flash from FDT offset %d\n",
				      node_offset);
				octeon_boot_bus_flash_init(gd->fdt_blob,
							   node_offset,
							   bootbus_node_offset,
							   NULL, NULL);
				debug("NOR flash initialization done.\n");
				num_devs++;
				continue;
			}
#endif
#ifdef CONFIG_OCTEON_ENABLE_LED_DISPLAY
			if (!strncmp(name, "led-display", 11)) {
				debug("Initializing LED from FDT offset %d\n",
				      node_offset);
				octeon_boot_bus_led_init(gd->fdt_blob,
							 node_offset,
							 bootbus_node_offset,
							 NULL, NULL);
				num_devs++;
				continue;
			}
#endif
#ifdef CONFIG_OCTEON_ENABLE_PAL
			if (!strncmp(name, "pal", 3)) {
				debug("Initializing PAL from FDT offset %d\n",
				      node_offset);
				octeon_boot_bus_pal_init(gd->fdt_blob,
							 node_offset,
							 bootbus_node_offset,
							 NULL, NULL);
				num_devs++;
				continue;
			}
#endif
		}
		if (num_devs >= total_devs)
			break;
	} while (depth > 0 && node_offset >= 0);

	octeon_boot_bus_board_init();

	debug("%s: Done.\n", __func__);
	return (0);
}

/**
 * Function called after all devices on the boot bus have been initialized.
 *
 * @param fdt_addr - FDT address
 * @return 0 for success
 */
int octeon_boot_bus_board_post_init(void *fdt_addr) __attribute__((weak));

int octeon_boot_bus_board_post_init(void *fdt_Addr)
{
	return 0;
}

/**
 * Initializes devices on the boot bus and assignes them address space.
 *
 * Note that this function is defined as "weak".  If a board requires
 * separate initialization all that is required is to define this function
 * in the appropriate board file.  See the nicpro2 board for an example.
 */
int octeon_boot_bus_late_init(void)
	__attribute__((weak, alias("__octeon_boot_bus_late_init")));

int __octeon_boot_bus_late_init(void)
{
	int rc;
	struct bootbus_fdt_handler_s *handler;
	int depth = 0;
	const char *name;
	int node_offset, bootbus_node_offset;
	static const char *psram_compat[] = { "micron,mt45w8mw16bgx",
					      "micron,mt45w1mw16pd",
					      NULL };

	INIT_LIST_HEAD(&bootbus_handlers.list);

	debug("Calling user function to add user defined FDT initializers\n");
	if ((rc = octeon_add_user_boot_bus_fdt_devices()) != 0) {
		printf("Error adding user device FDT initializer\n");
		return rc;
	}
#if defined(CONFIG_CMD_IDE)
	/* Add compact flash handler */
	octeon_boot_bus_add_fdt_handler("compact-flash", NULL,
					&octeon_boot_bus_cf_init);
#endif
	/* Add psram handler */
	octeon_boot_bus_add_fdt_handler("psram", (void *)psram_compat,
					&octeon_boot_bus_generic_init);

	bootbus_node_offset = fdt_path_offset(gd->fdt_blob, "/soc/bootbus");
	node_offset = bootbus_node_offset;

	if (node_offset < 0) {
		/* Obsolete case where boot bus isn't in the FDT */
		printf("Error: device tree not found for boot bus!\n");
		return -1;
	}

	debug("Calling octeon_boot_bus_board_late_init()\n");
	octeon_boot_bus_board_late_init();

	/* Walk through all the boot bus nodes and call the appropriate handler
	 */
	do {
		node_offset = fdt_next_node(gd->fdt_blob, node_offset,
					    &depth);
		if (depth < 0)
			break;

		name = fdt_get_name(gd->fdt_blob, node_offset, NULL);
		debug("Got node depth %d, name \"%s\"\n", depth, name);
		if (depth == 1) {
			/* We only care about the first level */
			name = fdt_get_name(gd->fdt_blob, node_offset, NULL);
			handler = boot_bus_get_handler(name);
			if (!handler) {
				continue;
			}
			if (handler->boot_device_func) {
				rc = handler->boot_device_func(gd->fdt_blob,
							       node_offset,
							       bootbus_node_offset,
							       handler->param,
							       NULL);
				if (rc != 0) {
					printf("FDT init handler for %s returned error\n",
					       name);
					return rc;
				}
			}
		}
	} while (depth >= 0);

	rc = octeon_boot_bus_board_post_init(gd->fdt_blob);

	debug("octeon_boot_bus_late_init() Done.\n");

	return rc;
}
