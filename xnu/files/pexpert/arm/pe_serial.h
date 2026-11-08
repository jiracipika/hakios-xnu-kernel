/*
 * Copyright (c) 2026 HakiOS project.
 *
 * Private interface shared between pexpert/arm/pe_serial.c (the serial
 * multiplexer) and the low-level serial drivers that live in their own
 * files. Not part of the exported pexpert headers.
 */
#ifndef _PEXPERT_ARM_PE_SERIAL_H
#define _PEXPERT_ARM_PE_SERIAL_H

#include <pexpert/arm/protos.h>

struct pe_serial_functions {
	/* Initialize the underlying serial hardware. */
	void (*init) (void);

	/* Return a non-zero value if the serial interface is ready to send more data. */
	unsigned int (*transmit_ready) (void);

	/* Write a single byte of data to serial. */
	void (*transmit_data) (uint8_t c);

	/* Return a non-zero value if there's a byte of data available. */
	unsigned int (*receive_ready) (void);

	/* Read a single byte from serial. */
	uint8_t (*receive_data) (void);

	/* Enables IRQs from this device. */
	void (*enable_irq) (void);

	/* Disables IRQs from this device and reports whether IRQs were enabled. */
	bool (*disable_irq) (void);

	/* Clears this device's IRQs targeting this agent, returning true if at least one IRQ was cleared. */
	bool (*acknowledge_irq) (void);

	/**
	 * Whether this serial driver can handle irqs. This value should be set by
	 * querying the device tree to see if the serial device has interrupts
	 * associated with it.
	 *
	 * For a device to support IRQs:
	 *   - enable_irq, disable_irq, and acknowledge_irq must be non-null
	 *   - The AppleSerialShim kext must be able to match to the serial device
	 *     in the IORegistry and call serial_enable_irq with the proper
	 *     serial_device_t
	 *   - The device tree entry for the serial device should have an interrupt
	 *     associated with it.
	 */
	bool has_irq;

	/* enum identifying which serial device these functions belong to. */
	serial_device_t device;

	/* Pointer to the next serial interface in the linked-list. */
	struct pe_serial_functions *next;
};

/*
 * Adds a serial driver to the multiplexer's list. Called from serial_init()
 * (pe_serial.c) and from the drivers it dispatches to.
 */
void register_serial_functions(struct pe_serial_functions *fns);

#ifdef GS201_UART
/*
 * Google Tensor gs201 (Pixel 7) USI UART, selected by a device-tree node
 * named "uart0" with compatible "google,gs201-uart"; see
 * pe_serial_gs201.c.
 */
void pe_serial_gs201_init(void);
#endif /* GS201_UART */

#endif /* _PEXPERT_ARM_PE_SERIAL_H */
