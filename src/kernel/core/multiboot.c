#include "multiboot.h"

#include <stddef.h>
#include <stdint.h>



multiboot_info_tag_header *
multiboot2_seek_tag(multiboot_info_tag_type of_type,
                    multiboot_info_header *head)
{
    if (NULL == head) return NULL;

    for (
        void *p = (void *)((uint64_t)head + sizeof(multiboot_info_header));
        p < (void *)((uint64_t)head + head->total_size);
    ) {
        if (of_type != ((multiboot_info_tag_header *)p)->type) {
            p = (void *)((uint64_t)p + ((multiboot_info_tag_header *)p)->size);
            continue;
        }

        return (multiboot_info_tag_header *)p;
    }

    return NULL;
}
