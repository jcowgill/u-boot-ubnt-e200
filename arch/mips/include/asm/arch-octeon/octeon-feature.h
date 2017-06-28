/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Inc. (support@cavium.com). All rights
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

/**
 * @file
 *
 * File defining checks for different Octeon features.
 *
 * <hr>$Revision: 30468 $<hr>
 */

#ifndef __OCTEON_FEATURE_H__
#define __OCTEON_FEATURE_H__

#ifdef	__cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/*
 * Errors
 */
typedef enum {
	OCTEON_FEATURE_SUCCESS = 0,
	OCTEON_FEATURE_MAP_OVERFLOW = -1,
} octeon_feature_result_t;

/*
 * Octeon attributes are per-core, initialized once, and referenced at
 * runtime. It provides a unified way to answer yes-or-no quetions.
 */
typedef enum {
	OCTEON_ATTR_FIRST_CORE,	/* first core in the boot coremask */
	OCTEON_ATTR_TRACED,	/* the core is traced */
	OCTEON_ATTR_NO_IOCFG,	/* skip IO config in cvmx_user_app_init() */
	OCTEON_ATTR_MAX
} octeon_attr_t;
#define OCTEON_IS_FIRST_CORE()	octeon_has_attr(OCTEON_ATTR_FIRST_CORE)
#define OCTEON_IS_TRACED()	octeon_has_attr(OCTEON_ATTR_TRACED)
#define OCTEON_IS_NO_IOCFG()	octeon_has_attr(OCTEON_ATTR_NO_IOCFG)

/*
 * Octeon models are declared after the macros in octeon-model.h with the
 * suffix _FEATURE. The individual features are declared with the
 * _FEATURE_ infix.
 */
typedef enum {
	/*
	 * Checks on the critical path are moved to the top (8 positions)
	 * so that the compiler generates one less insn than for the rest
	 * of the checks.
	 */
	OCTEON_FEATURE_PKND,
				/**<  CN68XX uses port kinds for packet interface */
	OCTEON_FEATURE_CN68XX_WQE,
				/**<  CN68XX has different fields in word0 - word2 */

	/*
	 * Features
	 */
	OCTEON_FEATURE_SAAD,
				/**<  Octeon models in the CN5XXX family and higher support atomic add instructions to memory (saa/saad) */
	OCTEON_FEATURE_ZIP,
				/**<  Does this Octeon support the ZIP offload engine? */
	OCTEON_FEATURE_CRYPTO,
				/**<  Does this Octeon support crypto acceleration using COP2? */
	OCTEON_FEATURE_DORM_CRYPTO,
				/**<  Can crypto be enabled by calling cvmx_crypto_dormant_enable()? */
	OCTEON_FEATURE_PCIE,
				/**<  Does this Octeon support PCI express? */
	OCTEON_FEATURE_SRIO,
				/**<  Does this Octeon support SRIO */
	OCTEON_FEATURE_ILK,
				/**<  Does this Octeon support Interlaken */
	OCTEON_FEATURE_KEY_MEMORY,
				/**<  Some Octeon models support internal memory for storing cryptographic keys */
	OCTEON_FEATURE_LED_CONTROLLER,
				   /**<  Octeon has a LED controller for banks of external LEDs */
	OCTEON_FEATURE_TRA,
				/**<  Octeon has a trace buffer */
	OCTEON_FEATURE_MGMT_PORT,
				/**<  Octeon has a management port */
	OCTEON_FEATURE_RAID,
				/**<  Octeon has a raid unit */
	OCTEON_FEATURE_USB,
				/**<  Octeon has a builtin USB */
	OCTEON_FEATURE_NO_WPTR,
				/**<  Octeon IPD can run without using work queue entries */
	OCTEON_FEATURE_DFA,
				/**<  Octeon has DFA state machines */
	OCTEON_FEATURE_MDIO_CLAUSE_45,
				       /**<  Octeon MDIO block supports clause 45 transactions for 10 Gig support */
	OCTEON_FEATURE_NPEI,
				/**<  CN52XX and CN56XX used a block named NPEI for PCIe access. Newer chips replaced this with SLI+DPI */
	OCTEON_FEATURE_HFA,
				/**<  Octeon has DFA/HFA */
	OCTEON_FEATURE_DFM,
				/**<  Octeon has DFM */
	OCTEON_FEATURE_CIU2,
				/**<  Octeon has CIU2 */
	OCTEON_FEATURE_DICI_MODE,
				/**<  Octeon has DMA Instruction Completion Interrupt mode */
	OCTEON_FEATURE_BIT_EXTRACTOR,
				     /**<  Octeon has Bit Select Extractor schedulor */
	OCTEON_FEATURE_NAND,
				/**<  Octeon has NAND */
	OCTEON_FEATURE_MMC,
				/**<  Octeon has built-in MMC support */
	OCTEON_FEATURE_ROM,
				/**<  Octeon has built-in ROM support */
	OCTEON_FEATURE_AUTHENTIK,
				/**<  Octeon has Authentik ROM support */
	OCTEON_MAX_FEATURE
} octeon_feature_t;

static inline int octeon_has_feature_OCTEON_FEATURE_SAAD(void)
{
	return !OCTEON_IS_MODEL(OCTEON_CN3XXX);
}

static inline int octeon_has_feature_OCTEON_FEATURE_ZIP(void)
{
	if (OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)
	    || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CNF71XX))
		return 0;
	else
		return !cvmx_fuse_read(121);
}

static inline int octeon_has_feature_OCTEON_FEATURE_CRYPTO(void)
{
	if (!OCTEON_IS_OCTEON1PLUS()) {	/* OCTEON II and later */
		cvmx_mio_fus_dat2_t fus_2;
		fus_2.u64 = cvmx_read_csr(CVMX_MIO_FUS_DAT2);
		if (fus_2.s.nocrypto || fus_2.s.nomul) {
			return 0;
		} else if (!fus_2.s.dorm_crypto) {
			return 1;
		} else {
			cvmx_rnm_ctl_status_t st;
			st.u64 = cvmx_read_csr(CVMX_RNM_CTL_STATUS);
			return st.s.eer_val;
		}
	} else {
		return !cvmx_fuse_read(90);
	}
}

static inline int octeon_has_feature_OCTEON_FEATURE_DORM_CRYPTO(void)
{
	if (!OCTEON_IS_OCTEON1PLUS()) {	/* OCTEON II and later */
		cvmx_mio_fus_dat2_t fus_2;
		fus_2.u64 = cvmx_read_csr(CVMX_MIO_FUS_DAT2);
		return !fus_2.s.nocrypto && !fus_2.s.nomul && fus_2.s.dorm_crypto;
	} else {
		return 0;
	}
}

static inline int octeon_has_feature_OCTEON_FEATURE_PCIE(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN56XX)
		|| OCTEON_IS_MODEL(OCTEON_CN52XX)
		|| !OCTEON_IS_OCTEON1PLUS());	/* OCTEON II and later have PCIe */
}

static inline int octeon_has_feature_OCTEON_FEATURE_SRIO(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN63XX) || OCTEON_IS_MODEL(OCTEON_CN66XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_ILK(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN68XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_KEY_MEMORY(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN38XX)
		|| OCTEON_IS_MODEL(OCTEON_CN58XX)
		|| OCTEON_IS_MODEL(OCTEON_CN56XX)
		|| !OCTEON_IS_OCTEON1PLUS());	/* OCTEON II or later */

}

static inline int octeon_has_feature_OCTEON_FEATURE_LED_CONTROLLER(void)
{
	return OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)
	       || OCTEON_IS_MODEL(OCTEON_CN56XX);
}

static inline int octeon_has_feature_OCTEON_FEATURE_TRA(void)
{
	return !(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN50XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_MGMT_PORT(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN56XX)
		|| OCTEON_IS_MODEL(OCTEON_CN52XX)
		|| !OCTEON_IS_OCTEON1PLUS());	/* OCTEON II or later */
}

static inline int octeon_has_feature_OCTEON_FEATURE_RAID(void)
{
	return OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX)
	       || !OCTEON_IS_OCTEON1PLUS();	/* OCTEON II or later */
}

static inline int octeon_has_feature_OCTEON_FEATURE_USB(void)
{
	return !(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_NO_WPTR(void)
{
	return ((OCTEON_IS_MODEL(OCTEON_CN56XX)
		 || OCTEON_IS_MODEL(OCTEON_CN52XX)
		 || !OCTEON_IS_OCTEON1PLUS())	/* OCTEON II or later */
		&& !OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X)
		&& !OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X));
}

static inline int octeon_has_feature_OCTEON_FEATURE_DFA(void)
{
	if (!OCTEON_IS_MODEL(OCTEON_CN38XX) && !OCTEON_IS_MODEL(OCTEON_CN31XX)
	    && !OCTEON_IS_MODEL(OCTEON_CN58XX))
		return 0;
	else if (OCTEON_IS_MODEL(OCTEON_CN3020))
		return 0;
	else
		return !cvmx_fuse_read(120);
}

static inline int octeon_has_feature_OCTEON_FEATURE_HFA(void)
{
	if (OCTEON_IS_OCTEON1PLUS())
		return 0;
	else
		return !cvmx_fuse_read(90);
}

static inline int octeon_has_feature_OCTEON_FEATURE_DFM(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX) || OCTEON_IS_MODEL(OCTEON_CN66XX)))
		return 0;
	else
		return !cvmx_fuse_read(90);
}

static inline int octeon_has_feature_OCTEON_FEATURE_MDIO_CLAUSE_45(void)
{
	return !(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN58XX)
		 || OCTEON_IS_MODEL(OCTEON_CN50XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_NPEI(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_PKND(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN68XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_CN68XX_WQE(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN68XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_CIU2(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN68XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_NAND(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN52XX)
		|| OCTEON_IS_MODEL(OCTEON_CN63XX)
		|| OCTEON_IS_MODEL(OCTEON_CN66XX)
		|| OCTEON_IS_MODEL(OCTEON_CN68XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_DICI_MODE(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN68XX_PASS2_X)
		|| OCTEON_IS_MODEL(OCTEON_CN61XX)
		|| OCTEON_IS_MODEL(OCTEON_CNF71XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_BIT_EXTRACTOR(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN68XX_PASS2_X)
		|| OCTEON_IS_MODEL(OCTEON_CN61XX)
		|| OCTEON_IS_MODEL(OCTEON_CNF71XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_MMC(void)
{
	return (OCTEON_IS_MODEL(OCTEON_CN61XX)
		|| OCTEON_IS_MODEL(OCTEON_CNF71XX));
}

static inline int octeon_has_feature_OCTEON_FEATURE_ROM(void)
{
	return OCTEON_IS_MODEL(OCTEON_CN66XX)
	       || OCTEON_IS_MODEL(OCTEON_CN61XX)
	       || OCTEON_IS_MODEL(OCTEON_CNF71XX);
}

static inline int octeon_has_feature_OCTEON_FEATURE_AUTHENTIK(void)
{
	if (OCTEON_IS_MODEL(OCTEON_CN66XX)
	    || OCTEON_IS_MODEL(OCTEON_CN61XX)
	    || OCTEON_IS_MODEL(OCTEON_CNF71XX)) {
		cvmx_mio_fus_dat2_t fus_2;
		fus_2.u64 = cvmx_read_csr(CVMX_MIO_FUS_DAT2);
		return (fus_2.s.nocrypto == 1) && (fus_2.s.dorm_crypto == 1);
	}
	return 0;
}
/*
 * bit map for octeon features
 */
#define FEATURE_MAP_SIZE	128
CVMX_SHARED extern uint8_t octeon_feature_map[FEATURE_MAP_SIZE];

/*
 * Answer ``Is the bit for feature set in the bitmap?''
 * @param feature
 * @return 1 when the feature is present and 0 otherwise, -1 in case of error.
 */
#if defined(CVMX_BUILD_FOR_LINUX_HOST) || defined(CVMX_BUILD_FOR_TOOLCHAIN) || defined(__U_BOOT__)
#define octeon_has_feature(feature_x) octeon_has_feature_##feature_x()
#else
#if defined(USE_RUNTIME_MODEL_CHECKS)
static inline int octeon_has_feature(octeon_feature_t feature)
{
	int byte, bit;

	byte = feature >> 3;
	bit = feature & 0x7;
	if (byte >= FEATURE_MAP_SIZE) {
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
		printk("ERROR: octeon_feature_map: Invalid Octeon Feature 0x%x\n", feature);
#else
		printf("ERROR: octeon_feature_map: Invalid Octeon Feature 0x%x\n", feature);
#endif
		return -1;
	}

	return (octeon_feature_map[byte] & ((1 << bit))) ? 1 : 0;
}
#else
#define octeon_has_feature(feature_x) octeon_has_feature_##feature_x()
#endif
#endif

/*
 * initialize octeon_feature_map[]
 */
extern void octeon_feature_init(void);

/*
 * initialize octeon_attr_map[]
 */
extern void octeon_attr_init(void);

/*
 * Query attribute
 *
 * @param attr is the per-core attribute to query.
 * @return 1 if the core has the attribute, 0 if not, and -1 for error.
 */
extern int octeon_has_attr(octeon_attr_t attr);

/*
 * Set attribute
 *
 * @param attr is the per-core attribute.
 * @return 0 for success and -1 when attr is out of the defined range.
 */
extern int octeon_set_attr(octeon_attr_t attr);

/*
 * Clear attribute
 *
 * @param attr is the per-core attribute to clear.
 * @return 0 for success and -1 when attr is out of the defined range.
 */
extern int octeon_clear_attr(octeon_attr_t attr);

#ifdef	__cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* __OCTEON_FEATURE_H__ */
