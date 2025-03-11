#ifndef CROWS_UART_H
#define CROWS_UART_H



#define SERIAL_COM1     0x3F8


int
uart_init(void);

unsigned char
uart_get(void);

// TODO: gets (needs heap)

void
uart_putc(
    unsigned char value
);

void
uart_puts(
    const char *string
);



#endif   /* CROWS_UART_H */
