#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H



#define MULTIBOOT_MAGIC_HEADER          0xe85250d6
#define MULTIBOOT_INFO_MAGIC            0x36d76289

#define MULTIBOOT_SEARCH_LIMIT          (1 << 15)   /* 32,768 bytes */
#define MULTIBOOT_SEARCH_ALIGNMENT      (1 << 3)   /* 8 bytes */

#define MULTIBOOT_EMPTY_TAG             { 0, 0, 8 }
#define MULTIBOOT_INFO_EMPTY_TAG        { 0, 8 }

#define MULTIBOOT_ARCHITECTURE_I386     0
#define MULTIBOOT_ARCHITECTURE_MIPS32   4

#define MULTIBOOT_RELOC_PREFERENCE_NONE 0
#define MULTIBOOT_RELOC_PREFERENCE_LOW  1
#define MULTIBOOT_RELOC_PREFERENCE_HIGH 2

#define MULTIBOOT_FLAG_TAG_OPTIONAL     1

#define MULTIBOOT_MEM_AVAILABLE         1
#define MULTIBOOT_MEM_RESERVED          2
#define MULTIBOOT_MEM_ACPI_RECLAIMABLE  3
#define MULTIBOOT_MEM_NVS               4
#define MULTIBOOT_MEM_BADRAM            5

#define MULTIBOOT_FB_TYPE_INDEXED       0
#define MULTIBOOT_FB_TYPE_RGB           1
#define MULTIBOOT_FB_TYPE_EGA_TEXT      2


// TODO: Convert typedef names to capitalized snake_case
typedef
enum {
    MB_END                  = 0,
    MB_INFO_REQ,
    MB_ADDR,
    MB_ENTRY,
    MB_FLAGS,
    MB_FRAMEBUFFER,
    MB_MOD_ALIGN,
    MB_EFI_BS,
    MB_I386_ENTRY,
    MB_AMD64_ENTRY,
    MB_RELOCATABLE,
} multiboot_tag_type;

typedef
enum {
    MBI_END                 = 0,
    MBI_CMD_LINE,
    MBI_LOADER_NAME,
    MBI_MODULES,
    MBI_BASIC_MEM,
    MBI_BIOS_BOOT_DEV,
    MBI_MEMORY_MAP,
    MBI_VBE_INFO,
    MBI_FRAMEBUFFER,
    MBI_ELF_SYMBOLS,
    MBI_APM,
    MBI_EFI_ST_32,
    MBI_EFI_ST_64,
    MBI_SMBIOS,
    MBI_ACPI_10,
    MBI_ACPI_20,
    MBI_NET,
    MBI_EFI_MEMORY_MAP,
    MBI_NO_EXIT_BOOT_SVCS,
    MBI_IMG_HND_32,
    MBI_IMG_HND_64,
    MBI_IMG_LOAD_BASE,
} multiboot_info_tag_type;


/* Multiboot Headers & Tags (within the target kernel). */
typedef
struct {
    unsigned int    magic;
    unsigned int    architecture;
    unsigned int    header_length;
    unsigned int    checksum;
} multiboot_header;

typedef
struct {
    unsigned short  type;
    unsigned short  flags;
    unsigned int    size;
} multiboot_tag_header;

/* Serves as a tag whose existence alone within Multiboot2 headers
    signals a boot loader to take a certain action or provide extra info. */
typedef
struct {
    multiboot_tag_header    header;
} multiboot_tag_boolean;

/* Multiboot tag types. */
typedef
struct {
    multiboot_tag_header    header;
    unsigned int            requested_info_tag_types[];
} multiboot_tag_info_request;

typedef
struct {
    multiboot_tag_header    header;
    unsigned int            header_phys_addr;
    unsigned int            text_load_phys_addr;
    unsigned int            load_end_phys_addr;
    unsigned int            bss_end_phys_addr;
} multiboot_tag_address;

/* This type works for MB_ENTRY, as well as arch-specific EFI ones. */
typedef
struct {
    multiboot_tag_header    header;
    unsigned int            entry_phys_addr;
} multiboot_tag_entry;

typedef
struct {
    multiboot_tag_header    header;
    unsigned int            flags;
} multiboot_tag_flags;

typedef
struct {
    multiboot_tag_header    header;
    unsigned int            width;
    unsigned int            height;
    unsigned int            depth;
} multiboot_tag_framebuffer;

typedef
struct {
    multiboot_tag_header    header;
    unsigned int            min_phys_addr;
    unsigned int            max_phys_addr;
    unsigned int            align;
    unsigned int            preference;
} multiboot_tag_relocatable;


/* Multiboot Information Structure Headers & Tags (provided by loaders). */
typedef
struct {
    unsigned int    total_size;
    unsigned int    reserved;
} multiboot_info_header;

typedef
struct {
    unsigned int    type;
    unsigned int    size;
} multiboot_info_tag_header;

/* Generic structure used for types consisting of just a pointer. */
typedef
struct {
    multiboot_info_tag_header   header;
    unsigned int                physical_address;
} multiboot_info_tag_pointer_32;
typedef
struct {
    multiboot_info_tag_header   header;
    unsigned long long          physical_address;
} multiboot_info_tag_pointer_64;

/* Generic structure used for boolean tags. */
typedef
struct {
    multiboot_info_tag_header   header;
} multiboot_info_tag_boolean;

/* Multiboot Info structure tag types. */
typedef
struct {
    multiboot_info_tag_header   header;
    unsigned char               command_line_string_data[];
} multiboot_info_tag_cmdline;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned char                   BootLoaderNameStringData[];
} multiboot_info_tag_loader_name;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned int                  ModulePhysAddrStart;
    unsigned int                  ModulePhysAddrEnd;
    unsigned char                   ModuleStringData[];
} MultibootInfoTagModule;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned int                  MemoryLower;
    unsigned int                  MemoryUpper;
} MultibootInfoTagBasicMemoryInfo;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned int                  BiosDeviceNumber;
    unsigned int                  Parition;
    unsigned int                  SubPartition;
} MultibootInfoTagBiosBootDevice;

typedef
struct {
    unsigned long long      Base;
    unsigned long long      Length;
    unsigned int      Type;
    unsigned int      Reserved;
} multiboot_memory_map_entry;
typedef
struct {
    multiboot_info_tag_header   Header;
    unsigned int                EntrySize;
    unsigned int                EntryVersion;
} multiboot_info_tag_memory_map;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned short                  VbeMode;
    unsigned short                  VbeIfaceSegment;
    unsigned short                  VbeIfaceOffset;
    unsigned short                  VbeIfaceLength;
    unsigned char                   VbeControlInfo[512];
    unsigned char                   VbeModeInfo[256];
} MultibootInfoTagVbeInfo;

typedef
struct {
    unsigned char       Red;
    unsigned char       Green;
    unsigned char       Blue;
} MultibootColor;
typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned long long                  FramebufferPhysAddr;
    unsigned int                  Pitch;
    unsigned int                  Width;
    unsigned int                  Height;
    unsigned char                   BitsPerPixel;
    unsigned char                   Type;
    unsigned short                  Reserved;
    union {
        struct {
            unsigned int          NumberOfColors;
            MultibootColor  Colors[];
        };
        struct {
            unsigned char           RedFieldPosition;
            unsigned char           RedMaskSize;
            unsigned char           GreenFieldPosition;
            unsigned char           GreenMaskSize;
            unsigned char           BlueFieldPosition;
            unsigned char           BlueMaskSize;
        };
    };
} multiboot_info_tag_framebuffer;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned short                  NumberOfSections;
    unsigned short                  EntrySize;
    unsigned short                  SectionHeaderIndex;
    unsigned short                  Reserved;
    unsigned char                   SectionHeaders[];
} MultibootInfoTagElfSymbols;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned short                  Version;
    unsigned short                  CodeSegment;
    unsigned int                  Offset;
    unsigned short                  CodeSegment16;
    unsigned short                  DataSegment16;
    unsigned short                  Flags;
    unsigned short                  CodeSegmentLength;
    unsigned short                  CodeSegment16Length;
    unsigned short                  DataSegment16Length;
} MultibootInfoTagApm;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned char                   Major;
    unsigned char                   Minor;
    unsigned char                   Reserved[6];
    unsigned char                   Tables[];
} MultibootInfoTagSmbios;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned char                   Rsdp10[];
} MultibootInfoTagAcpi10;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned char                   Rsdp20[];
} MultibootInfoTagAcpi20;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned char                   DhcpAck[];
} MultibootInfoTagNetwork;

typedef
struct {
    multiboot_info_tag_header  Header;
    unsigned int                  DescriptorSize;
    unsigned int                  DescriptorVersion;
    unsigned char                   EfiMemoryMap[];
} multiboot_info_tag_efi_memory_map;



multiboot_info_tag_header *
multiboot2_seek_tag(
    multiboot_info_tag_type of_type,
    multiboot_info_header *head
);



#endif   /* MULTIBOOT2_H */
