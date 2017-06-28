/***********************license start***************
 * Copyright (c) 2012  Cavium Inc. (support@cavium.com). All rights
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

 *   * Neither the name of Cavium Inc. nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

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

#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include "linux/export.h"
#include "asm/octeon/cvmx.h"
#include "asm/octeon/cvmx-fpa.h"
#include "asm/octeon/cvmx-global-resources.h"
#else
#include "cvmx.h"
#include "cvmx-fpa.h"
#include "cvmx-global-resources.h"
#endif

/** Allocates the pool from global resources and reserves them
  * @param pool	    FPA pool to allocate/reserve. If -1 it
  *                 finds the empty pool to allocate.
  * @return         Allocated pool number OR -1 if fails to allocate
                    the pool
  */
int cvmx_fpa_alloc_pool(int pool)
{
	if (cvmx_create_global_resource_range(CVMX_GR_TAG_FPA, CVMX_FPA_NUM_POOLS)) {
		cvmx_dprintf("\nFailed to create FPA global resource");
		return -1;
	}

	if (pool >= 0)
		pool = cvmx_reserve_global_resource_range(CVMX_GR_TAG_FPA, pool, pool, 1);
	else
		/* Find an empty pool */
		pool = cvmx_allocate_global_resource_range(CVMX_GR_TAG_FPA,(uint64_t)pool, 1, 1);
	if (pool == -1) {
		cvmx_dprintf("Error: FPA pool is not available to use\n");
		return -1;
	}
	//cvmx_dprintf("Error: FPA pool %d is free to use\n", pool);
	return pool;
}
EXPORT_SYMBOL(cvmx_fpa_alloc_pool);

/** Release/Frees the specified pool
  * @param pool	    Pool to free
  * @return         0 for success -1 failure
  */
int cvmx_fpa_release_pool(int pool)
{
	if (cvmx_free_global_resource_range_with_base(CVMX_GR_TAG_FPA, pool, 1) == -1) {
		cvmx_dprintf("\nERROR Failed to release FPA pool %d", (int)pool);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(cvmx_fpa_release_pool);
