#include "core/port_io.h"
#include "core/str.h"

#include "drivers/uart.h"

#include "multiboot.h"
#include "crows.h"

#include <stddef.h>
#include <stdint.h>



/* Because I really don't like CC warnings. */
void
kmain(void);

/* Main kernel init function. This does return eventually. */
int
kinit(
    struct crows_context *kernel_ctx,
    multiboot_info_header *info_tag
);


#pragma pack(push, 1)
struct crows_multiboot2_header
{
    multiboot_header            header;
    multiboot_tag_header        requests_header;
    unsigned int                requested_info_tag_types[5];
};

/* Define a statically-compiled Multiboot2 header which will be linked near or within
    the first 4K of the kernel. I decided to use a custom-defined structure for this
    because I didn't want to deal with jamming different types into the generic multiboot
    header file. This seemed to be the cleanest route and doesn't take much to update. */
const
struct crows_multiboot2_header
multiboot2_header
__attribute__((unused, aligned(16), section(".multiboot"))) = {
    .header = {
        .magic = MULTIBOOT_MAGIC_HEADER,
        .header_length = sizeof(struct crows_multiboot2_header),
        .architecture = MULTIBOOT_ARCHITECTURE_I386,
        .checksum = (0x100000000 - (MULTIBOOT_ARCHITECTURE_I386 + sizeof(struct crows_multiboot2_header) + MULTIBOOT_MAGIC_HEADER)),
    },
    .requests_header = {
        .type = MB_INFO_REQ,
        .flags = 0,
        .size = sizeof(multiboot_tag_header) + (5 * sizeof(unsigned int)),
    },
    .requested_info_tag_types = {
        MBI_CMD_LINE,           /* For obvious reasons. */
        MBI_LOADER_NAME,        /* This really shouldn't be required, but we want it. */
        MBI_FRAMEBUFFER,        /* This is a must for early DWM setup. */
        MBI_MEMORY_MAP,         /* This is also a must for obvious reasons. */
        MBI_EFI_ST_64,          /* This is a must for ACPI and Runtime Services, among other things. */
    },
};
#pragma pack(pop)


/* Calculate the minimum size expected of the info block. Of course, this
    doesn't guarantee these tags were included until the kernel checks. */
static const
int
expected_minimum_multiboot_info_size = (
    sizeof(multiboot_info_header)
    + sizeof(multiboot_info_tag_cmdline)
    + sizeof(multiboot_info_tag_loader_name)
    + sizeof(multiboot_info_tag_framebuffer)
    + sizeof(multiboot_info_tag_pointer_64)
    + sizeof(multiboot_info_tag_efi_memory_map)
);


/* Main ELF64 entrypoint from the bootloader. */
void
__attribute__((unused))
kmain(void)
{
    /* Disable all interrupts. The machine is ours. */
    asm("cli");

    /* We assume the kernel has been pre-loaded by a multiboot2-compliant
        loader. We check EAX and EBX to verify this is true. If the multiboot2
        header is invalid or otherwise not found, do not boot. We leave this
        all up to the loader (MFTAH-UEFI or GRUB2 or [Other]) to do properly. */
    unsigned long long rax, rbx;
    char message[128];

    /* Capture the values immediately. */
    asm("" : "=a"(rax), "=b"(rbx));
    rax &= ((unsigned long long)(1ULL << 32) - 1);
    rbx &= ((unsigned long long)(1ULL << 32) - 1);

    /* Initialize the serial port very, very early. This is better than combing the
        Multiboot2 struct right away because we'll be able to tell what goes wrong.
        NOTE: If the serial port can't init, then we'll just have to stay in the dark. */
    uart_init();
    uart_puts("\r\n\r\nWelcome to CrOwS.\r\n\t  CAW CAW CAW!!!\r\n\r\n");
    uart_puts("Crude serial UART driver initialized (duh 😎).\r\n");

    // char TESTSTRING[48];
    // for (int i = 0; i < 48; ++i) TESTSTRING[i] = 0;
    // snprintf(TESTSTRING, 48, "test '%% %%' '%d' '0x%x' '%s'...\n", strlen("abc"), (1<<8), "Two", "garbage garbage", 1, 2, 3);
    // uart_puts(TESTSTRING);

    if (MULTIBOOT_INFO_MAGIC != rax || NULL == (void *)rbx) {
        uart_puts("PANIC:  Multiboot2 Info structure not found. Cannot boot.\r\n");
        return;
    }

    snprintf(message, 128, "Got Multiboot2 info block at physical address 0x%X.\r\n", rbx);
    uart_puts(message); for (int i = 0; i < strlen(message); ++i) message[i] = 0x00;

    /* Get all Multiboot-provided information from the info structure. */
    multiboot_info_header *info_tag = (multiboot_info_header *)rbx;
    if (info_tag->total_size < expected_minimum_multiboot_info_size) {
        uart_puts("PANIC:  Invalid Multiboot2 Info structure size. Cannot boot.\r\n");
        return;
    }

    /* Initialize the rest of the kernel's early drivers with these details. This
        will include the GDT, IDT, framebuffer access, paging setup, crypto, etc. */
    struct crows_context kernel_ctx;
    if (kinit(&kernel_ctx, info_tag) < 0) {
        uart_puts("PANIC:  Unable to initialize the CrOwS kernel.\r\n");
        return;
    }

    /* Take on the modular micro-kernel role and chill while waiting for user
        applications to make syscalls and perform other setup operations. */
    //if (enter_userspace() < 0) {}

    return;
}


int
kinit(struct crows_context *kernel_ctx,
      multiboot_info_header *info_tag)
{
    if (NULL == (void *)info_tag) return -1;

    /* Find a physical memory hole to use for kmalloc calls. This is used temporarily
        during initialization until the kernel relocates itself. */
    multiboot_info_tag_memory_map *mem_map = (multiboot_info_tag_memory_map *)
        multiboot2_seek_tag(MBI_MEMORY_MAP, info_tag);
    if (NULL == mem_map) {
        uart_puts("Missing Multiboot2 Memory Map.\r\n");
        return -1;
    }
    kernel_ctx->memory_map = mem_map;

    char memmap_line[256];
    snprintf(
        memmap_line, 256,
        "Iterating Memory Map provided by Multiboot2 (%u bytes, %u entries).\r\n",
        mem_map->Header.size,
        (mem_map->Header.size / mem_map->EntrySize)
    );
    uart_puts(memmap_line);

    for (
        uint64_t p = ((uint64_t)mem_map + sizeof(multiboot_info_tag_memory_map));
        p < ((uint64_t)mem_map + mem_map->Header.size);
        p += mem_map->EntrySize
    ) {
        multiboot_memory_map_entry *entry = (multiboot_memory_map_entry *)p;

        for (int i = 0 ; i < strlen(memmap_line); ++i) memmap_line[i] = 0x00;
        snprintf(memmap_line, 256, "   Base 0x%X -- Length 0x%X -- Type %d\r\n",
            entry->Base, entry->Length, entry->Type);
        uart_puts(memmap_line);

        /* If we already found a memory hole, don't check anything below, just keep printing. */
        if (NULL != kernel_ctx->kmem_base) continue;

        /* Do not accept spaces under 2MiB, or non-free spaces. */
        if (entry->Base < 0x200000 || 1 != entry->Type) continue;

        /* We require a pool of at least 1MiB. */
        if (entry->Length < 0x100000) continue;

        /* Set the pool information. */
        kernel_ctx->kmem_base = (void *)(entry->Base);
        kernel_ctx->kmem_len = entry->Length;

        /* Mark the area as "reserved" in the temporary memory mapping. */
        entry->Type = 2;
    }

    if (NULL == kernel_ctx->kmem_base) {
        uart_puts("Cannot find a suitable temporary memory hole (>=2MiB @1MiB).");
        return -1;
    } else {
        char *into = (char *)(kernel_ctx->kmem_base);
        snprintf(into, kernel_ctx->kmem_len, "Using temporary memory hole at '0x%X'.\r\n", (uint64_t)into);
        uart_puts(into);

        /* Zero out the whole region. */
        for (unsigned long long i = 0; i < kernel_ctx->kmem_len; ++i) into[i] = 0x00;
    }

    return 0;
}
