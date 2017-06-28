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
 * This file contains defines for the ILK interface

 * <hr>$Revision: 49448 $<hr>
 *
 *
 */
#ifndef __CVMX_ILK_H__
#define __CVMX_ILK_H__

#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include <asm/octeon/cvmx.h>
#endif

#ifdef	__cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* CSR typedefs have been moved to cvmx-ilk-defs.h */

#define CVMX_ILK_GBL_BASE  5
#define CVMX_ILK_QLM_BASE  1

typedef struct {
	int intf_en:1;
	int la_mode:1;
	int reserved:6;		/* unused */
	int lane_en_mask:8;
	int lane_speed:16;
	/* add more here */
} cvmx_ilk_intf_t;

#define CVMX_NUM_ILK_INTF  2
#define CVMX_ILK_MAX_LANES 8
CVMX_SHARED extern unsigned char cvmx_ilk_lane_mask[CVMX_NUM_ILK_INTF];

typedef struct {
	unsigned int pipe;
	unsigned int chan;
} cvmx_ilk_pipe_chan_t;

#define CVMX_ILK_MAX_PIPES 45
/* Max number of channels allowed */
#define CVMX_ILK_MAX_CHANS 8

extern CVMX_SHARED unsigned char cvmx_ilk_chans[CVMX_NUM_ILK_INTF];
extern unsigned char cvmx_ilk_chan_map[CVMX_NUM_ILK_INTF][CVMX_ILK_MAX_CHANS];

typedef struct {
	unsigned int chan;
	unsigned int pknd;
} cvmx_ilk_chan_pknd_t;

#define CVMX_ILK_MAX_PKNDS 8	/* must be <45 */

typedef struct {
	unsigned int *chan_list;	/* for discrete channels. or, must be null */
	unsigned int num_chans;

	unsigned int chan_start;	/* for continuous channels */
	unsigned int chan_end;
	unsigned int chan_step;

	unsigned int clr_on_rd;
} cvmx_ilk_stats_ctrl_t;

#define CVMX_ILK_MAX_CAL      288
#define CVMX_ILK_TX_MIN_CAL   1
#define CVMX_ILK_RX_MIN_CAL   1
#define CVMX_ILK_CAL_GRP_SZ   8
#define CVMX_ILK_PIPE_BPID_SZ 7
#define CVMX_ILK_ENT_CTRL_SZ  2
#define CVMX_ILK_RX_FIFO_WM   0x200

typedef enum {
	PIPE_BPID = 0,
	LINK,
	XOFF,
	XON
} cvmx_ilk_cal_ent_ctrl_t;

typedef struct {
	unsigned char pipe_bpid;
	cvmx_ilk_cal_ent_ctrl_t ent_ctrl;
} cvmx_ilk_cal_entry_t;

/** Callbacks structure to customize ILK initialization sequence */
typedef struct {
    /** Called to setup rx calendar */
	int (*calendar_setup_rx) (int interface, int cal_depth, cvmx_ilk_cal_entry_t * pent, int hi_wm, unsigned char cal_ena);

    /** add more here */
} cvmx_ilk_callbacks_t;

typedef enum {
	CVMX_ILK_LPBK_DISA = 0,
	CVMX_ILK_LPBK_ENA
} cvmx_ilk_lpbk_ena_t;

typedef enum {
	CVMX_ILK_LPBK_INT = 0,
	CVMX_ILK_LPBK_EXT
} cvmx_ilk_lpbk_mode_t;

/**
 * This header is placed in front of all received ILK look-aside mode packets
 */
typedef union {
	uint64_t u64;

	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint32_t reserved_63_57:7;	// bits 63...57
		uint32_t nsp_cmd:5;	// bits 56...52
		uint32_t nsp_flags:4;	// bits 51...48
		uint32_t nsp_grp_id_upper:6;	// bits 47...42
		uint32_t reserved_41_40:2;	// bits 41...40
		uint32_t la_mode:1;	// bit  39      /* Protocol type, 1 for LA mode packet */
		uint32_t nsp_grp_id_lower:2;	// bits 38...37
		uint32_t nsp_xid_upper:4;	// bits 36...33
		uint32_t ilk_channel:1;	// bit  32      /* ILK channel number, 0 or 1 */
		uint32_t nsp_xid_lower:8;	// bits 31...24
		uint32_t reserved_23_0:24;	// bits 23...0  /* Unpredictable, may be any value */
#else
		uint32_t reserved_23_0:24;	// bits 23...0
		uint32_t nsp_xid_lower:8;	// bits 31...24
		uint32_t ilk_channel:1;	// bit  32
		uint32_t nsp_xid_upper:4;	// bits 36...33
		uint32_t nsp_grp_id_lower:2;	// bits 38...37
		uint32_t la_mode:1;	// bit  39
		uint32_t reserved_41_40:2;	// bits 41...40
		uint32_t nsp_grp_id_upper:6;	// bits 47...42
		uint32_t nsp_flags:4;	// bits 51...48
		uint32_t nsp_cmd:5;	// bits 56...52
		uint32_t reserved_63_57:7;	// bits 63...57
#endif
	} s;
} cvmx_ilk_la_nsp_compact_hdr_t;

typedef struct cvmx_ilk_LA_mode_struct
{
	int ilk_LA_mode;
	int ilk_LA_mode_cal_ena;
} cvmx_ilk_LA_mode_t;

void cvmx_ilk_config_set_LA_mode(int interface, int mode);
void cvmx_ilk_config_set_LA_mode_cal(int interface, int mode);
void cvmx_ilk_config_set_max_channels(int interface, unsigned char channels);
void cvmx_ilk_config_set_lane_mask(int interface, unsigned char mask);

extern CVMX_SHARED cvmx_ilk_LA_mode_t cvmx_ilk_LA_mode[CVMX_NUM_ILK_INTF];

extern void cvmx_ilk_get_callbacks(cvmx_ilk_callbacks_t * callbacks);
extern void cvmx_ilk_set_callbacks(cvmx_ilk_callbacks_t * new_callbacks);
extern int cvmx_ilk_use_la_mode(int interface, int channel);

extern int cvmx_ilk_start_interface(int interface, unsigned char num_lanes);
extern int cvmx_ilk_start_interface_la(int interface, unsigned char num_lanes);
extern int cvmx_ilk_set_pipe(int interface, int pipe_base, unsigned int pipe_len);
extern int cvmx_ilk_tx_set_channel(int interface, cvmx_ilk_pipe_chan_t * pch, unsigned int num_chs);
extern int cvmx_ilk_rx_set_pknd(int interface, cvmx_ilk_chan_pknd_t * chpknd, unsigned int num_pknd);
extern int cvmx_ilk_calendar_setup_cb(int interface, int num_ports);
extern int cvmx_ilk_calendar_sync_cb(int interface, int timeout);
extern int cvmx_ilk_enable(int interface);
extern int cvmx_ilk_disable(int interface);
extern int cvmx_ilk_get_intf_ena(int interface);
extern unsigned char cvmx_ilk_bit_count(unsigned char uc);
extern unsigned char cvmx_ilk_get_intf_ln_msk(int interface);
extern int cvmx_ilk_get_chan_info(int interface, unsigned char **chans, unsigned char *num_chan);
extern cvmx_ilk_la_nsp_compact_hdr_t cvmx_ilk_enable_la_header(int port, int mode);
extern void cvmx_ilk_show_stats(int interface, cvmx_ilk_stats_ctrl_t * pstats);
extern int cvmx_ilk_cal_setup_rx(int interface, int cal_depth, cvmx_ilk_cal_entry_t * pent, int hi_wm, unsigned char cal_ena);
extern int cvmx_ilk_cal_setup_tx(int interface, int cal_depth, cvmx_ilk_cal_entry_t * pent, unsigned char cal_ena);
extern int cvmx_ilk_lpbk(int interface, cvmx_ilk_lpbk_ena_t enable, cvmx_ilk_lpbk_mode_t mode);
extern int cvmx_ilk_la_mode_enable_rx_calendar(int interface);
#ifdef	__cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif /* __CVMX_ILK_H__ */
