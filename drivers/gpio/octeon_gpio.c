/***********************license start***************
 * Copyright (c) 2011  Cavium Inc. (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *   * Neither the name of Cavium Inc. nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.
 *
 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.
 *
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM INC. MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/

#include <common.h>
#include <asm/gpio.h>
#include <errno.h>
#include <asm/arch/cvmx.h>
#include <asm/arch/cvmx-gpio.h>

static int gpio_in_range(unsigned gpio)
{
	if (OCTEON_IS_MODEL(OCTEON_CN66XX) || OCTEON_IS_MODEL(OCTEON_CN61XX)
	    || OCTEON_IS_MODEL(OCTEON_CNF71XX))
		return gpio < 20;
	else
		return gpio < 16;
}

void gpio_toggle_value(unsigned gpio)
{
	if (!gpio_in_range(gpio))
		return;
	if (cvmx_gpio_read() && (1ull << gpio))
		cvmx_gpio_clear(1ull << gpio);
	else
		cvmx_gpio_set(1ull << gpio);
}

int gpio_direction_input(unsigned gpio)
{
	if (!gpio_in_range(gpio))
		return -1;
	cvmx_gpio_cfg(gpio, 0);
	return 0;
}

int gpio_direction_output(unsigned gpio, int value)
{
	if (!gpio_in_range(gpio))
		return -1;
	cvmx_gpio_cfg(gpio, 1);
	if (value)
		cvmx_gpio_set(1ull << gpio);
	else
		cvmx_gpio_clear(1ull << gpio);
	return 0;
}

int gpio_get_value(unsigned gpio)
{
	if (!gpio_in_range(gpio))
		return -1;
	return (cvmx_gpio_read() & (1ull << gpio)) ? 1 : 0;
}

int gpio_set_value(unsigned gpio, int value)
{
	if (!gpio_in_range(gpio))
		return -1;
	if (value)
		cvmx_gpio_set(1ull << gpio);
	else
		cvmx_gpio_clear(1ull << gpio);
	return 0;
}

int gpio_request(unsigned gpio, const char *label)
{
	if (!gpio_in_range(gpio))
		return -EINVAL;

	return 0;
}

int gpio_free(unsigned gpio)
{
	return 0;
}