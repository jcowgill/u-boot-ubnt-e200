/***********************license start************************************
 * Copyright (c) 2011  Cavium, Inc. (support@cavium.com).
 * All rights reserved.
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
 ***********************license end**************************************/

#include <common.h>
#include <environment.h>
#include <asm/global_data.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-app-init.h>
#include <asm/arch/cvmx-sysinfo.h>
#include <cvmx-core.h>

DECLARE_GLOBAL_DATA_PTR;

/* This hook is called whenever an environment variable is set. */
env_set_hook_rc_t setenv_arch(const char *var, const char *old_value,
                              const char *new_value)
{
	debug("In %s for var %s\n", __func__, var);
	debug("old=%s, new=%s\n", old_value ? old_value : "(none)",
	      new_value ? new_value : "(none)");
	if (!strcmp(var, "console_uart") && new_value) {
		unsigned console_uart = simple_strtoul(new_value, NULL, 0);
		debug("Changing console UART from %d to %d\n",
		      gd->ogd.console_uart, console_uart);
		if ((console_uart >= CONFIG_OCTEON_MIN_CONSOLE_UART) &&
		    (console_uart <= CONFIG_OCTEON_MAX_CONSOLE_UART)) {
			gd->ogd.console_uart = console_uart;
			printf("Console UART changed to %u\n", console_uart);
			return ENV_NORMAL;
		} else {
			printf("Console UART %u out of range\n", console_uart);
			return ENV_ERROR;
		}
	} else if (!strcmp(var, "pci_console_active")) {
		/* Change console based on changing pci_console_active */
		char output[40];
		char *env_str;
		if (new_value) {
			/* Setting it */
			env_str = getenv("stdin");
			if (!env_str)
				sprintf(output, "pci");
			else if (!strstr(env_str, "pci") && (strlen(env_str) < 36))
				sprintf(output, "%s,pci", env_str);
			else
				sprintf(output, "%s", env_str);
			debug("Changing stdin from %s to %s\n", env_str, output);
			setenv("stdin", output);

			env_str = getenv("stdout");
			if (!env_str)
				sprintf(output, "pci");
			else if (!strstr(env_str, "pci") && (strlen(env_str) < 36))
				sprintf(output, "%s,pci", env_str);
			else
				sprintf(output, "%s", env_str);
			debug("Changing stdout from %s to %s\n", env_str, output);
			setenv("stdout", output);

			env_str = getenv("stderr");
			if (!env_str)
				sprintf(output, "pci");
			else if (!strstr(env_str, "pci") && (strlen(env_str) < 36))
				sprintf(output, "%s,pci", env_str);
			else
				sprintf(output, "%s", env_str);
			debug("Changing stderr from %s to %s\n", env_str, output);
			setenv("stderr", output);
			printf("PCI console output has been enabled.\n");
		} else {
			/* Clearing it */
			char *start, *end;
			debug("Disabling pci console\n");
			/* Disable PCI console access */
			env_str = getenv("stdout");
			/* There are three cases:
			 * 1. pci could not be present (unlikely)
			 * 2. pci could be at the beginning, so we remove pci,
			 * 3. pci is not at the beginning so we remove ,pci
			 */
			if (env_str && strstr(env_str, "pci")) {
				sprintf(output, "%s", env_str);
				start = strstr(output, ",pci");
				if (!start)	/* case 3 */
					start = strstr(output, "pci,");
				if (!start)	/* case 1 */
					output[0] = '\0';
				else {		/* case 2 or 3 */
					end = &start[4];
					/* Remove 4 characters from string */
					do {
						*start++ = *end;
					} while (*end++);
				}
				setenv("stdout", output);
			}
			env_str = getenv("stderr");
			if (env_str && strstr(env_str, "pci")) {
				sprintf(output, "%s", env_str);
				start = strstr(output, ",pci");
				if (!start)
					start = strstr(output, "pci,");
				if (!start)
					output[0] = '\0';
				else {
					end = &start[4];
					do {
						*start++ = *end;
					} while (*end++);
				}
				setenv("stderr", output);
			}
			printf("PCI console output has been disabled.\n");
		}
	} else if (!strcmp(var, "console_uart")) {
		int uart;
		if (new_value)
			uart = simple_strtoul(new_value, NULL, 10);
		else
			uart = 0;
		if (uart == 0 || uart == 1) {
			gd->ogd.console_uart = uart;
			int octeon_uart_setup(int uart_index);
			octeon_uart_setup(uart);
		}
	}
#if defined(CONFIG_HW_WATCHDOG)
	else if (!strcmp(var, "watchdog_enable")) {
		if (new_value) {
			char *e;
			int msecs = 0;
			if ((e = getenv("watchdog_timeout")) != NULL) {
				msecs = simple_strtoul(e, NULL, 0);
			}
			if (!msecs)
				msecs = CONFIG_OCTEON_WD_TIMEOUT * 1000;
			extern void hw_watchdog_start(int msecs);
			printf("Starting watchdog with %d ms timeout\n",
			       msecs);
			hw_watchdog_start(msecs);
		} else {
			extern void hw_watchdog_disable(void);
			hw_watchdog_disable();
		}
	} else if (!strcmp(var, "watchdog_timeout") &&
		   getenv("watchdog_enable")) {
		int msecs = 0;
		if (new_value)
			msecs = simple_strtol(new_value, NULL, 0);
		else
			msecs = CONFIG_OCTEON_WD_TIMEOUT * 1000;
		extern void hw_watchdog_start(int msecs);
		printf("Changing watchdog timeout to %d ms.\n",
		       msecs);
		hw_watchdog_start(msecs);
	}
#endif

	return ENV_NORMAL;
}
