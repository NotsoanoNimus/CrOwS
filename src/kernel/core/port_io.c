#include "core/port_io.h"



unsigned char
inb(unsigned short port)
{
    unsigned char recv = 0;

    asm volatile ("in %0, %1" : "=a"(recv) : "Nd"(port));

    return recv;
}


void
outb(unsigned short port,
     unsigned char value)
{
    asm volatile ("out %1, %0" :  : "a"(value), "Nd"(port));
}
