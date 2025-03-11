#include "drivers/uart.h"

#include "core/port_io.h"


static int
uart_enabled = 0;


int
uart_init(void)
{
    /* That's right: shamelessly using OSDev's content to get up and going. */
    outb(SERIAL_COM1 + 1, 0x00);    /* Disable all interrupts */
    outb(SERIAL_COM1 + 3, 0x80);    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_COM1 + 0, 0x01);    /* Set divisor to 1 (lo byte): 115200 baud */
    outb(SERIAL_COM1 + 1, 0x00);    /*                  (hi byte) */
    outb(SERIAL_COM1 + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(SERIAL_COM1 + 2, 0xC7);    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(SERIAL_COM1 + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */

    // outb(SERIAL_COM1 + 4, 0x1E);    /* Set in loopback mode, test the serial chip */
    // outb(SERIAL_COM1 + 0, 0xAE);    /* Test serial chip (send byte 0xAE and check if serial returns same byte) */

    // /* Check if serial is faulty (i.e: not same byte as sent) */
    // if (0xAE != inb(SERIAL_COM1 + 0)) {
    //     return -1;
    // }

    /* If serial is not faulty set it in normal operation mode
        (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled). */
    // outb(SERIAL_COM1 + 4, 0x0F);

    uart_enabled = 1;
    return 0;
}


static inline
int
uart_recv_ready(void)
{
    return inb(SERIAL_COM1 + 5) & 0x01;
}

unsigned char
uart_get(void)
{
    if (!uart_enabled) return 0;

    /* Block until the port is available for receiving. */
    while (0 == uart_recv_ready());

    return inb(SERIAL_COM1);
}


static inline
int
uart_send_ready(void)
{
    return inb(SERIAL_COM1 + 5) & 0x20;
}

void
uart_putc(unsigned char value)
{
    if (!uart_enabled) return;

    while (0 == uart_send_ready());

    outb(SERIAL_COM1, value);
}


void
uart_puts(const char *string)
{
    if (
        (void *)0 == (void *)string
        || !uart_enabled
    ) return;

    for (; *string; ++string) {
        uart_putc((unsigned char)(*string));
    }
}
