#ifndef CROWS_H
#define CROWS_H

#include "multiboot.h"



struct crows_context
{
    multiboot_info_tag_memory_map *memory_map;
    void *kmem_base;
    unsigned long long kmem_len;
};



#endif   /* CROWS_H */
