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
 * Helper Functions for the Configuration Framework
 *
 * OCTEON_CN68XX introduces a flexible hw interface configuration
 * scheme. To cope with this change and the requirements of
 * configurability for other system resources, e.g., IPD/PIP pknd and
 * PKO ports and queues, a configuration framework for the SDK is
 * designed. It has two goals: first to recognize and establish the
 * default configuration and, second, to allow the user to define key
 * parameters in a high-level language.
 *
 * The helper functions query the QLM setup to help achieving the
 * first goal.
 *
 * The second goal is accomplished by generating
 * cvmx_helper_cfg_init() from a high-level lanaguage.
 *
 * <hr>$Revision: 0 $<hr>
 */

#ifndef __CVMX_HELPER_CFG_H__
#define __CVMX_HELPER_CFG_H__

#define CVMX_HELPER_CFG_MAX_PKO_PORT		128
#define CVMX_HELPER_CFG_MAX_PIP_BPID       	64
#define CVMX_HELPER_CFG_MAX_PIP_PKND       	64
#define CVMX_HELPER_CFG_MAX_PKO_QUEUES     	256
#define CVMX_HELPER_CFG_MAX_PORT_PER_IFACE 	256

#define CVMX_HELPER_CFG_INVALID_VALUE		-1	/* The default return
							 * value upon failure
							 */

#ifdef	__cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define cvmx_helper_cfg_assert(cond)					\
	do {								\
	    if (!(cond))						\
	    {								\
	        cvmx_dprintf("cvmx_helper_cfg_assert (%s) at %s:%d\n",	\
		    #cond, __FILE__, __LINE__);				\
	    }								\
	} while (0)

/*
 * Config Options
 *
 * These options have to be set via cvmx_helper_cfg_opt_set() before calling the
 * routines that set up the hw. These routines process the options and set them
 * correctly to take effect at runtime.
 */
enum cvmx_helper_cfg_option {
	CVMX_HELPER_CFG_OPT_USE_DWB,	/*
					 * Global option to control if
					 * the SDK configures units (DMA,
					 * SSO, and PKO) to send don't
					 * write back (DWB) requests for
					 * freed buffers. Set to 1/0 to
					 * enable/disable DWB.
					 *
					 * For programs that fit inside
					 * L2, sending DWB just causes
					 * more L2 operations without
					 * benefit.
					 */

	CVMX_HELPER_CFG_OPT_MAX
};
typedef enum cvmx_helper_cfg_option cvmx_helper_cfg_option_t;

/*
 * Per physical port
 */
struct cvmx_cfg_port_param {
	int8_t ccpp_pknd;
	int8_t ccpp_bpid;
	int8_t ccpp_pko_port_base;
	int8_t ccpp_pko_num_ports;
};

/*
 * Per pko_port
 */
struct cvmx_cfg_pko_port_param {
	int16_t ccppp_queue_base;
	int16_t ccppp_num_queues;
};

/*
 * A map from pko_port to
 *     interface,
 *     index, and
 *     pko engine id
 */
struct cvmx_cfg_pko_port_map {
	int16_t ccppl_interface;
	int16_t ccppl_index;
	int16_t ccppl_eid;
};

/*
 * This is for looking up pko_base_port and pko_nport for ipd_port
 */
struct cvmx_cfg_pko_port_pair {
	int8_t ccppp_base_port;
	int8_t ccppp_nports;
};

typedef union cvmx_user_static_pko_queue_config
{
	struct
	{
		int pko_queues_per_port_interface[5];
		int pko_queues_per_port_loop;
		int pko_queues_per_port_pci;
	} pknd;
	struct
	{
		int max_ports_per_interface[2];
		int pko_queues_per_port_interface[2];
		int pko_queues_per_port_loop;
		int pko_queues_per_port_pci;
		int pko_queues_per_port_srio[4];
	} non_pknd;
} cvmx_user_static_pko_queue_config_t;

extern CVMX_SHARED cvmx_user_static_pko_queue_config_t __cvmx_pko_queue_static_config;
extern CVMX_SHARED struct cvmx_cfg_pko_port_map cvmx_cfg_pko_port_map[CVMX_HELPER_CFG_MAX_PKO_PORT];
extern CVMX_SHARED struct cvmx_cfg_port_param cvmx_cfg_port [][CVMX_HELPER_CFG_MAX_PORT_PER_IFACE];
extern CVMX_SHARED struct cvmx_cfg_pko_port_param cvmx_pko_queue_table[];
extern CVMX_SHARED int cvmx_enable_helper_flag;
/*
 * @INTERNAL
 * Return configured pknd for the port
 *
 * @param interface the interface number
 * @param index the port's index number
 * @return the pknd
 */
extern int __cvmx_helper_cfg_pknd(int interface, int index);

/*
 * @INTERNAL
 * Return the configured bpid for the port
 *
 * @param interface the interface number
 * @param index the port's index number
 * @return the bpid
 */
extern int __cvmx_helper_cfg_bpid(int interface, int index);

/*
 * @INTERNAL
 * Return the configured pko_port base for the port
 *
 * @param interface the interface number
 * @param index the port's index number
 * @return the pko_port base
 */
extern int __cvmx_helper_cfg_pko_port_base(int interface, int index);

/*
 * @INTERNAL
 * Return the configured number of pko_ports for the port
 *
 * @param interface the interface number
 * @param index the port's index number
 * @return the number of pko_ports
 */
extern int __cvmx_helper_cfg_pko_port_num(int interface, int index);

/*
 * @INTERNAL
 * Return the configured pko_queue base for the pko_port
 *
 * @param pko_port
 * @return the pko_queue base
 */
extern int __cvmx_helper_cfg_pko_queue_base(int pko_port);

/*
 * @INTERNAL
 * Return the configured number of pko_queues for the pko_port
 *
 * @param pko_port
 * @return the number of pko_queues
 */
extern int __cvmx_helper_cfg_pko_queue_num(int pko_port);

/*
 * @INTERNAL
 * Return the interface the pko_port is configured for
 *
 * @param pko_port
 * @return the interface for the pko_port
 */
extern int __cvmx_helper_cfg_pko_port_interface(int pko_port);

/*
 * @INTERNAL
 * Return the index of the port the pko_port is configured for
 *
 * @param pko_port
 * @return the index of the port
 */
extern int __cvmx_helper_cfg_pko_port_index(int pko_port);

/*
 * @INTERNAL
 * Return the pko_eid of the pko_port
 *
 * @param pko_port
 * @return the pko_eid
 */
extern int __cvmx_helper_cfg_pko_port_eid(int pko_port);

/*
 * @INTERNAL
 * Return the max# of pko queues allocated.
 *
 * @return the max# of pko queues
 *
 * Note: there might be holes in the queue space depending on user
 * configuration. The function returns the highest queue's index in
 * use.
 */
extern int __cvmx_helper_cfg_pko_max_queue(void);

/*
 * @INTERNAL
 * Return the max# of PKO DMA engines allocated.
 *
 * @return the max# of DMA engines
 *
 * NOTE: the DMA engines are allocated contiguously and starting from
 * 0.
 */
extern int __cvmx_helper_cfg_pko_max_engine(void);

/*
 * Get the value set for the config option ``opt''.
 *
 * @param opt is the config option.
 * @return the value set for the option
 */
extern uint64_t cvmx_helper_cfg_opt_get(cvmx_helper_cfg_option_t opt);

/*
 * Set the value for a config option.
 *
 * @param opt is the config option.
 * @param val is the value to set for the opt.
 * @return 0 for success and -1 on error
 *
 * Note an option here is a config-time parameter and this means that
 * it has to be set before calling the corresponding setup functions
 * that actually sets the option in hw.
 */
extern int cvmx_helper_cfg_opt_set(cvmx_helper_cfg_option_t opt, uint64_t val);

/*
 * Retrieve the pko_port base given ipd_port.
 *
 * @param ipd_port is the IPD eport
 * @return the corresponding PKO port base for the physical port
 * represented by the IPD eport or CVMX_HELPER_CFG_INVALID_VALUE.
 */
extern int cvmx_helper_cfg_ipd2pko_port_base(int ipd_port);

/*
 * Retrieve the number of pko_ports given ipd_port.
 *
 * @param ipd_port is the IPD eport
 * @return the corresponding number of PKO ports for the physical port
 *  represented by IPD eport or CVMX_HELPER_CFG_INVALID_VALUE.
 */
extern int cvmx_helper_cfg_ipd2pko_port_num(int ipd_port);

/*
 * @INTERNAL
 * The init function
 *
 * @param none
 * @return 0 for success.
 *
 * Note: this function is meant to be called to set the ``configured
 * parameters,'' e.g., pknd, bpid, etc. and therefore should be before
 * any of the corresponding cvmx_helper_cfg_xxxx() functions are
 * called.
 */
extern int __cvmx_helper_init_port_config_data(void);

/*
 * @INTERNAL
 * The local init function
 *
 * @param none
 * @return 0 for success.
 *
 * Note: this function is meant to be called to set the ``configured
 * parameters locally,'' e.g., pknd, bpid, etc. and therefore should be before
 * any of the corresponding cvmx_helper_cfg_xxxx() functions are
 * called.
 */
extern int __cvmx_helper_init_port_config_data_local(void);

/*
 * Set the frame max size and jabber size to 65535.
 *
 */
extern void cvmx_helper_cfg_set_jabber_and_frame_max(void);

/*
 * Enable storing short packets only in the WQE.
 */
extern void cvmx_helper_cfg_store_short_packets_in_wqe(void);

/*
 * Allocated a block of internal ports and queues for the specified
 * interface/port
 *
 * @param  interface  the interface for which the internal ports and queues
 *                    are requested
 * @param  port       the index of the port within in the interface for which
                      the internal ports and queues are requested.
 * @param  pot_count  the number of internal ports requested
 * @param  queue_cnt  the number of queues requested for each of the internal
 *                    port. This call will allocate a total of
 *		      (port_cnt * queue_cnt) queues
 *
 * @return  0 on success
 *         -1 on failure
 */
 int cvmx_pko_alloc_iport_and_queues(int interface, int port, int port_cnt,
				     int queue_cnt);

/*
 * Allocated a block of queues for the specified port.
 *
 * @param  port   the internal port for which the queues are requested
 * @param  count  the number of queues requested
 *
 * @return  0 on success
 *         -1 on failure
 */
int cvmx_pko_queue_alloc(uint64_t port, uint64_t count);

/*
 * Free the queues that are associated with the specified port
 *
 * @param  port   the internal port for which the queues are freed.
 *
 * @return  0 on success
 *         -1 on failure
 */
int cvmx_pko_queue_free(uint64_t port);

/*
 * Initializes the pko queue range data structure.
 * @return  0 on success
 *         -1 on failure
 */
int init_cvmx_pko_que_range(void);

/*
 * Frees up all the allocated ques.
 */
void cvmx_pko_queue_free_all(void);

/*
 * Initializes cvmx with user specified config info.
 */
int cvmx_user_static_config(void);
void cvmx_pko_queue_show(void);
int cvmx_fpa_pool_init_from_cvmx_config(void);
int __cvmx_helper_init_port_valid(void);
#ifdef	__cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif /* __CVMX_HELPER_CFG_H__ */
