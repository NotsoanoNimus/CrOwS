#ifndef CROWS_PORT_IO_H
#define CROWS_PORT_IO_H



unsigned char
inb(
    unsigned short port
);


void
outb(
    unsigned short port,
    unsigned char value
);



#endif   /* CROWS_PORT_IO_H */
