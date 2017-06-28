#ifndef __OCTEON_UBNT_E200_SHARED_H__
#define __OCTEON_UBNT_E200_SHARED_H__

#define OCTEON_UBNT_E200_DRAM_SOCKET_CONFIGURATION0 \
    {{0x53, 0x0}, {NULL, NULL}},{{0x0, 0x0}, {NULL, NULL}}

#define OCTEON_UBNT_E200_BOARD_EEPROM_TWSI_ADDR        (0x56)
  
/* LMC0_MODEREG_PARAMS1 */
#define OCTEON_UBNT_E200_MODEREG_PARAMS1_1RANK_1SLOT      \
    { .cn61xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = 0,                    \
                  .dic_00       = 0,                    \
                  .rtt_nom_00   = rttnom_30ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = 0,                    \
                  .dic_01       = 0,                    \
                  .rtt_nom_01   = 0,                    \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = 0,                    \
                  .dic_10       = 0,                    \
                  .rtt_nom_10   = 0,                    \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = 0,                    \
                  .dic_11       = 0,                    \
                  .rtt_nom_11   = 0,                    \
        }                                               \
    }

#define OCTEON_UBNT_E200_MODEREG_PARAMS1_1RANK_2SLOT      \
    { .cn61xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = rttwr_120ohm,         \
                  .dic_00       = 0,                    \
                  .rtt_nom_00   = rttnom_20ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = 0,                    \
                  .dic_01       = 0,                    \
                  .rtt_nom_01   = 0,                    \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = rttwr_120ohm,         \
                  .dic_10       = 0,                    \
                  .rtt_nom_10   = rttnom_20ohm,         \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = 0,                    \
                  .dic_11       = 0,                    \
                  .rtt_nom_11   = 0                     \
        }                                               \
    }

#define OCTEON_UBNT_E200_MODEREG_PARAMS1_2RANK_1SLOT      \
    { .cn61xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = 0,                    \
                  .dic_00       = 1,                    \
                  .rtt_nom_00   = rttnom_40ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = 0,                    \
                  .dic_01       = 1,                    \
                  .rtt_nom_01   = 0,                    \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = 0,                    \
                  .dic_10       = 0,                    \
                  .rtt_nom_10   = 0,                    \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = 0,                    \
                  .dic_11       = 0,                    \
                  .rtt_nom_11   = 0,                    \
        }                                               \
    }

#define OCTEON_UBNT_E200_MODEREG_PARAMS1_2RANK_2SLOT      \
    { .cn61xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = 0,                    \
                  .dic_00       = 1,                    \
                  .rtt_nom_00   = rttnom_20ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = 0,                    \
                  .dic_01       = 1,                    \
                  .rtt_nom_01   = rttnom_60ohm,         \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = 0,                    \
                  .dic_10       = 1,                    \
                  .rtt_nom_10   = rttnom_20ohm,         \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = 0,                    \
                  .dic_11       = 1,                    \
                  .rtt_nom_11   = rttnom_60ohm,         \
        }                                               \
    }

#define OCTEON_UBNT_E200_MODEREG_PARAMS1_4RANK_1SLOT      \
    { .cn61xx = { .pasr_00      = 0,                    \
                  .asr_00       = 0,                    \
                  .srt_00       = 0,                    \
                  .rtt_wr_00    = rttwr_120ohm,         \
                  .dic_00       = 0,                    \
                  .rtt_nom_00   = rttnom_40ohm,         \
                  .pasr_01      = 0,                    \
                  .asr_01       = 0,                    \
                  .srt_01       = 0,                    \
                  .rtt_wr_01    = rttwr_120ohm,         \
                  .dic_01       = 0,                    \
                  .rtt_nom_01   = rttnom_none,          \
                  .pasr_10      = 0,                    \
                  .asr_10       = 0,                    \
                  .srt_10       = 0,                    \
                  .rtt_wr_10    = rttwr_120ohm,         \
                  .dic_10       = 0,                    \
                  .rtt_nom_10   = rttnom_40ohm,         \
                  .pasr_11      = 0,                    \
                  .asr_11       = 0,                    \
                  .srt_11       = 0,                    \
                  .rtt_wr_11    = rttwr_120ohm,         \
                  .dic_11       = 0,                    \
                  .rtt_nom_11   = rttnom_none,          \
        }                                               \
    }


#define OCTEON_UBNT_E200_CN61XX_DRAM_ODT_1RANK_CONFIGURATION \
    /* DIMMS   reserved  WODT_MASK                LMCX_MODEREG_PARAMS1            RODT_CTL    RODT_MASK    reserved */ \
    /* =====   ======== ============== ========================================== ========= ============== ======== */ \
    /*   1 */ {   0,    0x00000001ULL, OCTEON_UBNT_E200_MODEREG_PARAMS1_1RANK_1SLOT,    3,     0x00000000ULL,  0  },      \
    /*   2 */ {   0,    0x00050005ULL, OCTEON_UBNT_E200_MODEREG_PARAMS1_1RANK_2SLOT,    3,     0x00010004ULL,  0  }

#define OCTEON_UBNT_E200_CN61XX_DRAM_ODT_2RANK_CONFIGURATION \
    /* DIMMS   reserved  WODT_MASK                LMCX_MODEREG_PARAMS1            RODT_CTL    RODT_MASK    reserved */ \
    /* =====   ======== ============== ========================================== ========= ============== ======== */ \
    /*   1 */ {   0,    0x00000101ULL, OCTEON_UBNT_E200_MODEREG_PARAMS1_2RANK_1SLOT,    4,     0x00000000ULL,    0  },    \
    /*   2 */ {   0,    0x09090606ULL, OCTEON_UBNT_E200_MODEREG_PARAMS1_2RANK_2SLOT,    4,     0x01010404ULL,    0  }

#define OCTEON_UBNT_E200_CN61XX_DRAM_ODT_4RANK_CONFIGURATION \
    /* DIMMS   reserved  WODT_MASK                LMCX_MODEREG_PARAMS1            RODT_CTL    RODT_MASK    reserved */ \
    /* =====   ======== ============== ========================================== ========= ============== ======== */ \
    /*   1 */ {   0,    0x01030203ULL, OCTEON_UBNT_E200_MODEREG_PARAMS1_4RANK_1SLOT,    3,     0x01010202ULL,    0  }


#define OCTEON_UBNT_E200_DDR_CONFIGURATION                                \
    /* Interface 0 */                                                   \
    {                                                                   \
        .custom_lmc_config = {                                          \
            .min_rtt_nom_idx	= 4,                                    \
            .max_rtt_nom_idx	= 4,                                    \
            .min_rodt_ctl	= 2,                                    \
            .max_rodt_ctl	= 2,                                    \
            .dqx_ctl		= 7,                                    \
            .ck_ctl		= 7,                                    \
            .cmd_ctl		= 7,                                    \
            .min_cas_latency	= 6,                                    \
            .offset_en 		= 0,                                    \
            .offset_udimm	= 2,                                    \
            .offset_rdimm	= 1,                                    \
            .ddr_rtt_nom_auto	= 1,                                    \
            .ddr_rodt_ctl_auto	= 1,                                    \
            .rlevel_comp_offset	= 2,                                    \
            .rlevel_compute 	= 1,                                    \
            .ddr2t_udimm 	= 1,                                    \
            .ddr2t_rdimm 	= 1,                                    \
            .maximum_adjacent_rlevel_delay_increment = 2,               \
            .fprch2		= 2,                                    \
            .dll_write_offset   = 0,                                    \
            .dll_read_offset    = -13,                                  \
            .parity		= 0,                                    \
            .mode32b		= 0},                                   \
            .dimm_config_table = {                                      \
            OCTEON_UBNT_E200_DRAM_SOCKET_CONFIGURATION0,                  \
            DIMM_CONFIG_TERMINATOR},                                    \
                 .unbuffered = {                                        \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                      .registered = {                                   \
            .ddr_board_delay = 0,                                       \
            .lmc_delay_clk = 0,                                         \
            .lmc_delay_cmd = 0,                                         \
            .lmc_delay_dq = 0},                                         \
                           .odt_1rank_config = {                        \
            OCTEON_UBNT_E200_CN61XX_DRAM_ODT_1RANK_CONFIGURATION},        \
                                .odt_2rank_config = {                   \
            OCTEON_UBNT_E200_CN61XX_DRAM_ODT_2RANK_CONFIGURATION},        \
                                     .odt_4rank_config = {              \
            OCTEON_UBNT_E200_CN61XX_DRAM_ODT_4RANK_CONFIGURATION}         \
    }

#endif   /* __OCTEON_UBNT_E200_SHARED_H__ */
