/*
 * Copyright (c) 2026 HakiOS project.
 *
 * Polled console for the Google Tensor gs201 (Pixel 7) USI UART.
 *
 * The gs201 USI block in UART mode presents the Samsung S3C-style register
 * interface (32-bit accesses only; no 16550 LSR/THR at 0x14/0x00).  The
 * register map below was taken from the working Linux driver for this
 * hardware, drivers/tty/serial/samsung_tty.c in
 * github.com/derekross/linux-omarchy-mobile (compatible
 * "google,gs201-uart"/"google,gs101-uart", iotype UPIO_MEM32), whose
 * earlycon path is exactly this driver's transmit algorithm:
 *
 *   0x00 ULCON   line control (bootloader-configured, left alone)
 *   0x04 UCON    uart control    (bootloader-configured, left alone)
 *   0x08 UFCON   fifo control    (bit 0: fifo mode enabled)
 *   0x10 UTRSTAT tx/rx status
 *                  bit 0 RXDR  receive data ready
 *                  bit 1 TXFE  transmit buffer/fifo empty (can accept a byte)
 *                  bit 2 TXE   transmitter completely idle
 *   0x18 UFSTAT  fifo status    (bit 9: tx fifo full)
 *   0x20 UTXH    transmit hold register (byte, 32-bit access)
 *   0x24 URXH    receive hold register  (byte, 32-bit access)
 *
 * The bootloader has already brought the line up (USI UART mode, baud,
 * fifos), so this driver only ever reads status registers and writes UTXH;
 * it cannot disturb the configuration.  There is no interrupt support and
 * no baud setup: transmit is polled (wait until the fifo is not full, or,
 * with fifos off, until the transmit buffer is empty), receive is polled
 * for uart_getc only.
 *
 * Selection: serial_init() (pe_serial.c) calls pe_serial_gs201_init(),
 * which claims a device-tree child of /arm-io that is named "uart0" and
 * carries compatible "google,gs201-uart".  The node's reg is the offset
 * from the arm-io base; for the Pixel 7's USI0 UART at PA 0x10a00000 with
 * the loader's arm-io base of 0x10000000 that is:
 *
 *   uart0 { compatible = "google,gs201-uart"; reg = <0x00a00000 0x1000>; }
 *
 * If no such node exists the driver stays out of the way and the other
 * console drivers (bcm2837 mini-uart, QEMU PL011) keep working unchanged.
 */

#include <string.h>

#include <libkern/section_keywords.h>
#include <machine/machine_routines.h>
#include <pexpert/device_tree.h>
#include <pexpert/pexpert.h>
#include <pexpert/protos.h>

#include "pe_serial.h"

/* Samsung S3C-style register offsets, 32-bit accesses (UPIO_MEM32). */
#define GS201_UART_UFCON                0x08u
#define GS201_UART_UTRSTAT              0x10u
#define GS201_UART_UFSTAT               0x18u
#define GS201_UART_UTXH                 0x20u
#define GS201_UART_URXH                 0x24u

#define GS201_UART_UFCON_FIFOMODE       (1u << 0)
#define GS201_UART_UTRSTAT_RXDR         (1u << 0)
#define GS201_UART_UTRSTAT_TXFE         (1u << 1)
#define GS201_UART_UFSTAT_TXFULL        (1u << 9)

static vm_offset_t gs201_uart_base_vaddr = 0;

static uint32_t
gs201_uart_read(uint32_t reg)
{
	return *(volatile uint32_t *) (gs201_uart_base_vaddr + reg);
}

static void
gs201_uart_write(uint32_t reg, uint32_t value)
{
	*(volatile uint32_t *) (gs201_uart_base_vaddr + reg) = value;
}

static void
gs201_uart_init(void)
{
	/*
	 * Nothing to do: the bootloader configured the line, and this driver
	 * must not touch its configuration registers.
	 */
}

static unsigned int
gs201_uart_transmit_ready(void)
{
	if (gs201_uart_read(GS201_UART_UFCON) & GS201_UART_UFCON_FIFOMODE) {
		/* Fifo mode: ready while the Tx fifo is not full. */
		return !(gs201_uart_read(GS201_UART_UFSTAT) & GS201_UART_UFSTAT_TXFULL);
	}

	/* Non-fifo mode: ready when the transmit buffer is empty. */
	return gs201_uart_read(GS201_UART_UTRSTAT) & GS201_UART_UTRSTAT_TXFE;
}

static void
gs201_uart_transmit_data(uint8_t c)
{
	gs201_uart_write(GS201_UART_UTXH, c);
}

static unsigned int
gs201_uart_receive_ready(void)
{
	return gs201_uart_read(GS201_UART_UTRSTAT) & GS201_UART_UTRSTAT_RXDR;
}

static uint8_t
gs201_uart_receive_data(void)
{
	return (uint8_t) gs201_uart_read(GS201_UART_URXH);
}

SECURITY_READ_ONLY_LATE(static struct pe_serial_functions) gs201_uart_serial_functions =
{
	.init = gs201_uart_init,
	.transmit_ready = gs201_uart_transmit_ready,
	.transmit_data = gs201_uart_transmit_data,
	.receive_ready = gs201_uart_receive_ready,
	.receive_data = gs201_uart_receive_data,
	/* No IRQ support: enable_irq/disable_irq/acknowledge_irq stay NULL. */
	.device = SERIAL_GS201_UART
};

/*
 * Claims the /arm-io child named "uart0" if it carries compatible
 * "google,gs201-uart", maps its register window and registers the driver
 * with the serial multiplexer.  Called once from serial_init().
 */
void
pe_serial_gs201_init(void)
{
	DTEntry entryP = NULL;
	uintptr_t const *reg_prop;
	uint32_t prop_size;
	char const *uart_compat = NULL;
	vm_offset_t soc_base = pe_arm_get_soc_base_phys();

	if (soc_base == 0) {
		return;
	}

	if (SecureDTFindEntry("name", "uart0", &entryP) != kSuccess) {
		return;
	}

	SecureDTGetProperty(entryP, "compatible", (void const **) &uart_compat, &prop_size);
	if (uart_compat == NULL || strcmp(uart_compat, "google,gs201-uart") != 0) {
		return;
	}

	SecureDTGetProperty(entryP, "reg", (void const **) &reg_prop, &prop_size);
	if (prop_size < 2 * sizeof(uintptr_t)) {
		/* reg must be a full <address size> pair */
		return;
	}

	gs201_uart_base_vaddr = ml_io_map(soc_base + *reg_prop, *(reg_prop + 1));
	if (gs201_uart_base_vaddr == 0) {
		return;
	}

	register_serial_functions(&gs201_uart_serial_functions);
}
