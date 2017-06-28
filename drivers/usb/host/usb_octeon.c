/***********************license start***************
 * Copyright (c) 2008-2011 Cavium, inc. (support@cavium.com).  All rights
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
 * This Software, including technical data, may be subject to U.S.  export
 * control laws, including the U.S.  Export Administration Act and its
 * associated regulations, and may be subject to export or import regulations
 * in other countries.  You warrant that You will comply strictly in all
 * respects with all such regulations and acknowledge that you have the
 * responsibility to obtain licenses to export, re-export or import the
 * Software.
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

#include <common.h>
#include <asm/arch/octeon_boot.h>
#include <usb.h>
#include <asm/arch/cvmx-usb.h>
#include <mtd/cfi_flash.h>
#include <exports.h>
#include <watchdog.h>

static cvmx_usb_state_t *cvmx_usb_state_array;
static int usb_port;
static int usb_initialized;
static int speed_hack_done;

static void msleep(uint32_t ms)
{
	while (ms-- > 0) {
		udelay(1000);
	}
	WATCHDOG_RESET();
}

#define PIPE_DEBUG_FLAGS    0	// CVMX_USB_PIPE_FLAGS_DEBUG_TRANSFERS

/* Keep the u-boot pipe value for each cvmx pipe structure opened */
#define PIPE_LIST_SIZE 32
unsigned int pipe_handle_list[PIPE_LIST_SIZE];

int lookup_pipe_handle(uint32_t uboot_pipe)
{
	int i;
	for (i = 0; i < PIPE_LIST_SIZE; i++) {
		if (uboot_pipe == pipe_handle_list[i])
			return i;
	}
	return -1;
}

int usb_lowlevel_init(void)
{
	cvmx_usb_status_t status;
	char *eptr;
	int64_t addr;

	if (!cvmx_usb_get_num_ports()) {
		printf("ERROR: This device does not support USB\n");
		return -1;
	}

	if ((eptr = getenv("usb_host_port"))) {
		usb_port = simple_strtol(eptr, NULL, 10);
		if (usb_port >= cvmx_usb_get_num_ports()) {
			printf("Invalid usb port selected for host mode, "
			       "ignoring\n");
			usb_port = 0;
		}
	} else
		usb_port = 0;

	if (usb_port < 0) {
		printf("Skipping USB initialization due to negative "
		       "usb_host_port value\n");
		return -1;
	}

	if (!cvmx_usb_state_array) {
		addr =
		    cvmx_bootmem_phy_named_block_alloc(cvmx_usb_get_num_ports()
						       *
						       sizeof(cvmx_usb_state_t),
						       0, 0x7fffffff, 128,
						       "__tmp_bootloader_usb",
						       CVMX_BOOTMEM_FLAG_END_ALLOC);
		if (addr < 0) {
			printf("ERROR: Unable to allocate memory for USB "
			       "structures.... (1)\n");
			return -1;
		}
		cvmx_usb_state_array = cvmx_phys_to_ptr(addr);
		if (!cvmx_usb_state_array) {
			printf("ERROR: Unable to allocate memory for USB "
			       "structures.... (2)\n");
			return -1;
		}
	}

	printf("(port %d) ", usb_port);

	status = cvmx_usb_initialize(&cvmx_usb_state_array[usb_port],
				     usb_port,
				     CVMX_USB_INITIALIZE_FLAGS_CLOCK_AUTO
#if 0
//                                 | CVMX_USB_INITIALIZE_FLAGS_DEBUG_CSRS
				     | CVMX_USB_INITIALIZE_FLAGS_DEBUG_INFO
				     | CVMX_USB_INITIALIZE_FLAGS_DEBUG_CALLS
				     | CVMX_USB_INITIALIZE_FLAGS_DEBUG_TRANSFERS
				     | CVMX_USB_INITIALIZE_FLAGS_DEBUG_CALLBACKS
#endif
	    );

	if (status != CVMX_USB_SUCCESS) {
		printf("ERROR: cvmx_usb_initialize failed, status: %d\n",
		       status);
		msleep(2000);
		if (CVMX_USB_SUCCESS !=
		    cvmx_usb_shutdown(&cvmx_usb_state_array[usb_port]))
			printf("#### ERROR: Unable to shutdown usb block\n");
		return -1;
	}
	msleep(2000);

	status = cvmx_usb_enable(&cvmx_usb_state_array[usb_port]);
	if (CVMX_USB_SUCCESS != status) {
		if (CVMX_USB_SUCCESS !=
		    cvmx_usb_shutdown(&cvmx_usb_state_array[usb_port])) {
			printf("#### ERROR: Unable to shutdown usb block\n");
			return -1;
		}
		/* Don't return error if we can't enable the port. */
		return 0;
	}
	usb_initialized = 1;
	speed_hack_done = 0;
	return 0;
}

int usb_close_all_pipes(void)
{
	int retval = 0;
	int i;
	for (i = 0; i < PIPE_LIST_SIZE; i++) {
		if (pipe_handle_list[i]) {
			if (CVMX_USB_SUCCESS !=
			    cvmx_usb_close_pipe(&cvmx_usb_state_array[usb_port],
						i)) {
				printf("#### ERROR: Unable to close pipe "
				       "handle: %d\n", i);
				retval = -1;
			}
			pipe_handle_list[i] = 0;
		}
	}

	return retval;
}

int usb_lowlevel_stop(void)
{
	/* Go through all cached pipes and shut them down. */
	int retval = 0;

	if (!usb_initialized || !cvmx_usb_state_array)
		return 0;

	retval = usb_close_all_pipes();

	if (CVMX_USB_SUCCESS !=
	    cvmx_usb_disable(&cvmx_usb_state_array[usb_port])) {
		printf("#### ERROR: Unable to disable usb port\n");
		retval = -1;
	}
	if (CVMX_USB_SUCCESS !=
	    cvmx_usb_shutdown(&cvmx_usb_state_array[usb_port])) {
		printf("#### ERROR: Unable to shutdown usb block\n");
		retval = -1;
	}
	cvmx_bootmem_free_named("__tmp_bootloader_usb");
	usb_initialized = 0;

	return retval;
}

static int callback_done = 0;

void control_msg_callback(cvmx_usb_state_t * state,
			  cvmx_usb_callback_t reason,
			  cvmx_usb_complete_t status,
			  int pipe_handle, int submit_handle,
			  int bytes_transferred, void *user_data)
{
#if 0
	printf("####### %s: state: %p, reason: %d, status: %d, pipe handle: %d, "
	       "submit_handle: %d, bytes: %d, user: %p\n",
	       __FUNCTION__, state, reason, status, pipe_handle, submit_handle,
	       bytes_transferred, user_data);
#endif
	struct usb_device *dev = user_data;

	if (CVMX_USB_CALLBACK_TRANSFER_COMPLETE == reason) {
#if 0
		printf("Transfer complete on pipe handle %d, bytes trans: %d\n",
		       pipe_handle, bytes_transferred);
#endif
		dev->status &= ~USB_ST_NOT_PROC;/* Request has been processed */
		callback_done = 1;
		dev->act_len = bytes_transferred;
	}
}

int submit_bulk_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		    int transfer_len)
{
	int pipe_handle;
	int device_addr = usb_pipedevice(pipe);
	int endpoint_num = usb_pipeendpoint(pipe);
	cvmx_usb_direction_t transfer_dir =
	    usb_pipeout(pipe) ? CVMX_USB_DIRECTION_OUT : CVMX_USB_DIRECTION_IN;
	cvmx_usb_transfer_t transfer_type = CVMX_USB_TRANSFER_BULK;
	int max_packet = usb_maxpacket(dev, pipe);
	int retval;
	int pipe_speed;

	if (!usb_initialized || !cvmx_usb_state_array)
		return -1;

#if 0
	printf("%s: dev: %p, pipe: 0x%08x, buf: %p, len: %d\n", __FUNCTION__,
	       dev, pipe, buffer, transfer_len);
	printf("Dev addr: %d, endp: %d, dir: %d, trans len: %dmax: %d\n",
	       device_addr, endpoint_num, transfer_dir, transfer_len,
	       max_packet);
#endif

	if (!buffer || !cvmx_ptr_to_phys(buffer)) {
		printf("ERROR: Buffer NULL in submit_bulk_message, len: %d, "
		       "ptr: %p, addr: 0x%llx\n",
		       transfer_len, buffer, cvmx_ptr_to_phys(buffer));
	}

	if (dev->speed == USB_SPEED_HIGH)
		pipe_speed = CVMX_USB_SPEED_HIGH;
	else if (dev->speed == USB_SPEED_LOW)
		pipe_speed = CVMX_USB_SPEED_LOW;
	else
		pipe_speed = CVMX_USB_SPEED_FULL;

	if ((pipe_handle = lookup_pipe_handle(pipe)) < 0) {

		pipe_handle =
		    cvmx_usb_open_pipe(&cvmx_usb_state_array[usb_port],
				       PIPE_DEBUG_FLAGS, device_addr,
				       endpoint_num, pipe_speed, max_packet,
				       transfer_type, transfer_dir, 0, 0,
				       dev->split_hub_id, dev->split_hub_port);

		if (pipe_handle < 0) {
			printf("ERROR: Unable to open pipe!(%d)\n",
			       pipe_handle);
			return -1;
		}
		pipe_handle_list[pipe_handle] = pipe;
//        printf("pipe handle (BULK): %d\n", pipe_handle);
	}

	/* Now send message */
	retval =
	    cvmx_usb_submit_bulk(&cvmx_usb_state_array[usb_port], pipe_handle,
				 cvmx_ptr_to_phys(buffer), transfer_len,
				 control_msg_callback, dev);
	if (retval < 0) {
		printf("#### ERROR: cvmx_usb_submit_bulk() failed: %d\n",
		       retval);
		return -1;
	}

	cvmx_usb_status_t status;
	int timeout = 3000000;
	callback_done = 0;
	status = cvmx_usb_poll(&cvmx_usb_state_array[usb_port]);
	while (timeout-- > 0 && !callback_done && status == CVMX_USB_SUCCESS) {
		status = cvmx_usb_poll(&cvmx_usb_state_array[usb_port]);
		WATCHDOG_RESET();
	}

	if (status < 0)
		printf("#### ERROR: USB bulk polling returned error: %d!\n",
		       status);
	if (timeout < 0)
		printf("#### ERROR: USB bulk polling timed out!\n");

	return -1;
}

int submit_control_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		       int transfer_len, struct devrequest *setup)
{
	int pipe_handle;
	int device_addr = usb_pipedevice(pipe);
	int endpoint_num = usb_pipeendpoint(pipe);
	cvmx_usb_direction_t transfer_dir =
	    usb_pipeout(pipe) ? CVMX_USB_DIRECTION_OUT : CVMX_USB_DIRECTION_IN;
	cvmx_usb_transfer_t transfer_type = CVMX_USB_TRANSFER_CONTROL;
	int max_packet = usb_maxpacket(dev, pipe);
	int retval;
	int pipe_speed;

	if (!usb_initialized || !cvmx_usb_state_array)
		return -1;
#if 0
	printf("%s: dev: %p, pipe: 0x%08x, buf: %p, len: %d, devreq: %p\n",
	       __FUNCTION__, dev, pipe, buffer, transfer_len, setup);
	printf("Dev addr: %d, endp: %d, dir: %d, trans len: %dmax: %d\n",
	       device_addr, endpoint_num, transfer_dir, transfer_len,
	       max_packet);
#endif

	if (!speed_hack_done) {
		/* Set the speed of the root hub port when we send the first
		 * control message.  This is all we need to do with with root
		 * hub, and avoid the complexity of doing a virtual root hub
		 * implementation.
		 */
		struct usb_device *root_dev = usb_get_dev_index(0);

		cvmx_usb_port_status_t port_status =
		    cvmx_usb_get_status(&cvmx_usb_state_array[usb_port]);

		if (port_status.port_speed == CVMX_USB_SPEED_HIGH)
			root_dev->speed = USB_SPEED_HIGH;
		else if (port_status.port_speed == CVMX_USB_SPEED_LOW)
			root_dev->speed = USB_SPEED_LOW;
		else
			root_dev->speed = USB_SPEED_FULL;
		speed_hack_done = 1;
	}

	if (dev->speed == USB_SPEED_HIGH)
		pipe_speed = CVMX_USB_SPEED_HIGH;
	else if (dev->speed == USB_SPEED_LOW)
		pipe_speed = CVMX_USB_SPEED_LOW;
	else
		pipe_speed = CVMX_USB_SPEED_FULL;

	/* Open a pipe to send this message with */
	if ((pipe_handle = lookup_pipe_handle(pipe)) < 0) {
		pipe_handle =
		    cvmx_usb_open_pipe(&cvmx_usb_state_array[usb_port],
				       PIPE_DEBUG_FLAGS, device_addr,
				       endpoint_num, pipe_speed, max_packet,
				       transfer_type, transfer_dir, 0, 0,
				       dev->split_hub_id, dev->split_hub_port);

		if (pipe_handle < 0) {
			printf("Unable to upen pipe!\n");
			return -1;
		}
		/* Don't cache device 0 pipes */
		if (device_addr)
			pipe_handle_list[pipe_handle] = pipe;
	}

	/* Buffer is NULL for some transactions, handle this way to avoid
	 * ptr_to_phys warnings
	 */
	uint64_t buffer_address = 0;
	if (buffer)
		buffer_address = cvmx_ptr_to_phys(buffer);

	/* Now send message */
	retval = cvmx_usb_submit_control(&cvmx_usb_state_array[usb_port],
					 pipe_handle, cvmx_ptr_to_phys(setup),
					 buffer_address, transfer_len,
					 control_msg_callback, dev);

	cvmx_usb_status_t status;
	int timeout = 3000000;
	callback_done = 0;
	status = cvmx_usb_poll(&cvmx_usb_state_array[usb_port]);
	while (timeout-- > 0 && !callback_done && status == CVMX_USB_SUCCESS) {
		status = cvmx_usb_poll(&cvmx_usb_state_array[usb_port]);
		WATCHDOG_RESET();
	}
	if (status < 0)
		printf("#### ERROR: USB control polling returned error: %d!\n",
		       status);
	if (timeout < 0)
		printf("#### ERROR: USB control polling timed out!\n");

	/* Close uncached pipes to device 0 */
	if (!device_addr) {
		/* Close the pipe */
		if (CVMX_USB_SUCCESS !=
		    cvmx_usb_close_pipe(&cvmx_usb_state_array[usb_port],
					pipe_handle))
			printf("#### ERROR: Unable to close pipe handle "
			       "(control): %d\n", pipe_handle);
	}

	return 0;		/* Return value ignored by caller */
}

int submit_int_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		   int transfer_len, int interval)
{
	printf("%s: dev: %p, pipe: 0x%08lx, buf: %p, len: %d, interval: %d\n",
	       __FUNCTION__, dev, pipe, buffer, transfer_len, interval);
	printf("ERROR:  *****   Interrupt messages not supported *******\n");
	return -1;
}

