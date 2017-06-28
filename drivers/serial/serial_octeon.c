/***********************license start************************************
 * Copyright (c) 2003-2011 Cavium Inc. (support@cavium.com). All rights
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
#undef DEBUG
#include <config.h>

#include <common.h>
#include <serial.h>
#include <watchdog.h>
#include <asm/arch/octeon_boot.h>
#include <asm/arch/lib_octeon.h>

int octeon_serial_init(void);
int octeon_serial_uninit(void);
void octeon_serial_setbrg(void);
void octeon_serial_putc(const char c);
void octeon_serial_puts(const char *s);
int octeon_serial_getc(void);
int octeon_serial_tstc(void);

#ifdef CONFIG_SYS_PCI_CONSOLE
int octeon_serial_pci_init(void);
int octeon_serial_pci_uninit(void);
void octeon_serial_pci_setbrg(void);
void octeon_serial_pci_putc(const char c);
void octeon_serial_pci_puts(const char *s);
int octeon_serial_pci_getc(void);
int octeon_serial_pci_tstc(void);
#endif

DECLARE_GLOBAL_DATA_PTR;

struct serial_device octeon_serial0_device = {
	.name = "serial",
	.init = serial_init,
	.uninit = NULL,
	.setbrg = serial_setbrg,
	.getc = serial_getc,
	.tstc = serial_tstc,
	.putc = serial_putc,
	.puts = serial_puts,
	.next = NULL
};

struct serial_device *default_serial_console(void)
{
	return &octeon_serial0_device;
}


/******************************************************************************
*
* serial_init - initialize a channel
*
* This routine initializes the number of data bits, parity
* and set the selected baud rate. Interrupts are disabled.
* Set the modem control signals if the option is selected.
*
* RETURNS: N/A
*/

static uint16_t compute_divisor(uint32_t eclock_hz, uint32_t baud)
{
	uint16_t divisor;
#if CONFIG_OCTEON_SIM_SPEED
	divisor = 1;		/* We're simulating, go fast! */
#else
	divisor =
	    ((unsigned long)(eclock_hz +
			     8 * baud) / (unsigned long)(16 * baud));
#endif

	return (divisor);
}

#define LOG_REGS    \
    asm volatile ("\
        .set push\n\
        .set mips64\n\
        dadd $1, $0, $2\n\
        dadd $1, $0, $3\n\
        dadd $1, $0, $4\n\
        dadd $1, $0, $5\n\
        dadd $1, $0, $6\n\
        dadd $1, $0, $7\n\
        dadd $1, $0, $8\n\
        dadd $1, $0, $9\n\
        dadd $1, $0, $10\n\
        dadd $1, $0, $11\n\
        dadd $1, $0, $12\n\
        dadd $1, $0, $13\n\
        dadd $1, $0, $14\n\
        dadd $1, $0, $15\n\
        dadd $1, $0, $16\n\
        dadd $1, $0, $17\n\
        dadd $1, $0, $18\n\
        dadd $1, $0, $19\n\
        dadd $1, $0, $20\n\
        dadd $1, $0, $21\n\
        dadd $1, $0, $22\n\
        dadd $1, $0, $23\n\
        dadd $1, $0, $24\n\
        dadd $1, $0, $25\n\
        dadd $1, $0, $26\n\
        dadd $1, $0, $27\n\
        dadd $1, $0, $28\n\
        dadd $1, $0, $29\n\
        dadd $1, $0, $30\n\
        dadd $1, $0, $31\n\
        .set pop\n\
        " : : );

/**
 * Function that does the real work of setting up the Octeon uart.
 * Takes all parameters as arguments, so it does not require gd
 * structure to be set up.
 *
 * @param uart_index Index of uart to configure
 * @param cpu_clock_hertz
 *                   CPU clock frequency in Hz
 * @param baudrate   Baudrate to configure
 *
 * @return 0 on success
 *         !0 on error
 */
int octeon_uart_setup2(int uart_index, int cpu_clock_hertz, int baudrate)
{
	uint16_t divisor;
	cvmx_uart_fcr_t fcrval;
	cvmx_uart_mcr_t mcrval;
	cvmx_uart_lcr_t lcrval;

	fcrval.u64 = 0;
	fcrval.s.en = 1;	/* enable the FIFO's */
	fcrval.s.rxfr = 1;	/* reset the RX fifo */
	fcrval.s.txfr = 1;	/* reset the TX fifo */

	divisor = compute_divisor(cpu_clock_hertz, baudrate);

	cvmx_write_csr(CVMX_MIO_UARTX_FCR(uart_index), fcrval.u64);

	mcrval.u64 = 0;
#if CONFIG_OCTEON_SIM_SETUP
	if (uart_index == 1)
		mcrval.s.afce = 1;	/* enable auto flow control for
					 * simulator. Needed for gdb
					 * regression callfuncs.exp.
					 */
	else
		mcrval.s.afce = 0;	/* disable auto flow control so board
					 * can power on without serial port
					 * connected
					 */
#else
	mcrval.s.afce = 0;	/* disable auto flow control so board can power
				 * on without serial port connected
				 */
#endif
	mcrval.s.rts = 1;	/* looks like this must be set for auto flow
				 * control to work
				 */

	cvmx_read_csr(CVMX_MIO_UARTX_LSR(uart_index));

	lcrval.u64 = 0;
	lcrval.s.cls = CVMX_UART_BITS8;
	lcrval.s.stop = 0;	/* stop bit included? */
	lcrval.s.pen = 0;	/* no parity? */
	lcrval.s.eps = 1;	/* even parity? */
	lcrval.s.dlab = 1;	/* temporary to program the divisor */
	cvmx_write_csr(CVMX_MIO_UARTX_LCR(uart_index), lcrval.u64);

	cvmx_write_csr(CVMX_MIO_UARTX_DLL(uart_index), divisor & 0xff);
	cvmx_write_csr(CVMX_MIO_UARTX_DLH(uart_index), (divisor >> 8) & 0xff);

	/* divisor is programmed now, set this back to normal */
	lcrval.s.dlab = 0;
	cvmx_write_csr(CVMX_MIO_UARTX_LCR(uart_index), lcrval.u64);

#if !CONFIG_OCTEON_SIM_SPEED
	/* spec says need to wait after you program the divisor */
	uint64_t read_cycle = octeon_get_cycles() + (2 * divisor * 16) + 10000;
	while (octeon_get_cycles() < read_cycle) {
		/* Spin */
	}
#endif

	/* Don't enable flow control until after baud rate is configured. - we
	 * don't want to allow characters in until after the baud rate is
	 * fully configured
	 */
	cvmx_write_csr(CVMX_MIO_UARTX_MCR(uart_index), mcrval.u64);
	return 0;

}

/*****************************************************************************/
/**
 * Setup a uart for use
 *
 * @param uart_index Uart to setup (0 or 1)
 * @return Zero on success
 */
int octeon_uart_setup(int uart_index)
{
	return octeon_uart_setup2(uart_index, octeon_get_ioclk_hz(),
				  gd->baudrate);
}

void octeon_set_baud(uint32_t uart_index, uint32_t baud)
{
	uint16_t divisor;
	divisor = compute_divisor(octeon_get_ioclk_hz(), baud);

	cvmx_uart_lcr_t lcrval;
	lcrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LCR(uart_index));

	cvmx_wait((2 * divisor * 16) + 10000);

	lcrval.s.dlab = 1;	/* temporary to program the divisor */
	cvmx_write_csr(CVMX_MIO_UARTX_LCR(uart_index), lcrval.u64);

	cvmx_write_csr(CVMX_MIO_UARTX_DLL(uart_index), divisor & 0xff);
	cvmx_write_csr(CVMX_MIO_UARTX_DLH(uart_index), (divisor >> 8) & 0xff);
	/* divisor is programmed now, set this back to normal */
	lcrval.s.dlab = 0;
	cvmx_write_csr(CVMX_MIO_UARTX_LCR(uart_index), lcrval.u64);

	/* spec says need to wait after you program the divisor */
	cvmx_wait((2 * divisor * 16) + 10000);
	/* Wait a little longer..... */
	udelay(500000);

}

/**
 * Put a single byte to uart port.
 *
 * @param uart_index Uart to write to (0 or 1)
 * @param ch         Byte to write
 */
static inline void uart_write_byte(int uart_index, uint8_t ch)
{
	cvmx_uart_lsr_t lsrval;

	/* Spin until there is room */
	do {
		lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(uart_index));
	}
	while (lsrval.s.thre == 0);
	WATCHDOG_RESET();
	/* Write the byte */
	cvmx_write_csr(CVMX_MIO_UARTX_THR(uart_index), ch);
}

/**
 * Get a single byte from serial port.
 *
 * @param uart_index Uart to read from (0 or 1)
 * @return The byte read
 */
static inline uint8_t uart_read_byte(int uart_index)
{
	cvmx_uart_lsr_t lsrval;

	/* Spin until data is available */
	do {
		lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(uart_index));
		WATCHDOG_RESET();
	} while (!lsrval.s.dr);

	/* Read and return the data */
	return cvmx_read_csr(CVMX_MIO_UARTX_RBR(uart_index));
}

// modified for Octane.....

int serial_init(void)
{
	char *env_str;
	int i;

	gd->ogd.console_uart = CONFIG_OCTEON_DEFAULT_CONSOLE_UART;
	if ((env_str = getenv("console_uart"))) {
		if (1 == simple_strtoul(env_str, NULL, 10))
			gd->ogd.console_uart = 1;
	}

	/*
	 * Initialize all UARTS even though we only use one for the
	 * console.  Some applications (Linux second kernel) expect both
	 * to work.
	 */
	for (i = CONFIG_OCTEON_MIN_CONSOLE_UART;
	     i <= CONFIG_OCTEON_MAX_CONSOLE_UART; i++)
		octeon_uart_setup(i);

	return 0;
}

void serial_setbrg(void)
{
	octeon_set_baud(gd->ogd.console_uart, gd->baudrate);
}

void serial_putc(const char c)
{
#if !CONFIG_OCTEON_SIM_HW_DIFF
	if (c == '\n') {
		uart_write_byte(gd->ogd.console_uart, '\r');
	}
#endif

	uart_write_byte(gd->ogd.console_uart, c);
}

void serial_puts(const char *s)
{
	while (*s) {
		serial_putc(*s++);
	}
}

int serial_getc(void)
{
	return (uart_read_byte(gd->ogd.console_uart));
}

int serial_tstc(void)
{
	cvmx_uart_lsr_t lsrval;

	lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(gd->ogd.console_uart));
	return (lsrval.s.dr);
}

#if 0
void serial_reinit_all(void)
{
	struct serial_device *s;

	for (s = serial_devices; s; s = s->next)
		s->init();
}
#endif
