#ifndef MFTAH_UEFI_H
#define MFTAH_UEFI_H

/* This assumes the `mftah` header file exists because it needs to use its structures.
    If it's not found on the include path, just copy it locally from the repo at
    https://github.com/NotsoanoNimus/MFTAH. Eventually this will be a local submodule. */
#include <mftah.h>

#include "gnu-efi/inc/efi.h"
#include "gnu-efi/inc/efilib.h"

#include "../crypto/aes.h"
#include "../crypto/sha256.h"



#define MFTAH_CREATOR_ID EFI_SIGNATURE_32('X', 'M', 'I', 'T')   /* Creator: 'XMIT' */
#define MFTAH_OEM_TABLE_ID EFI_SIGNATURE_64('M', 'F', 'T', 'A', 'H', 'E', 'F', 'I')   /* OEM: 'MFTAHEFI' */

/* The GUID that represents the developer (in this case, my personal site). */
#define XMIT_XYZ_VENDOR_GUID \
    { 0xf1338329, 0xb42f, 0x4d88, \
    { 0xd6, 0x3a, 0x96, 0xd4, 0x44, 0x99, 0xbe, 0x5b }}

/* The maximum amount of payloads discoverable by this software. */
#define MFTAH_MAX_PAYLOADS 32

/* The maximum length of the password buffer. */
#define MFTAH_MAX_PW_LEN 32

/* The maximum length of a loadable payload file NAME from the local boot disk. */
#define MFTAH_MAX_FILENAME_LENGTH 64

/* The chunk sizes at which the ramdisk is loaded. */
#define MFTAH_RAMDISK_LOAD_BLOCK_SIZE 16384

/* When set to 1, causes the application to PANIC if EFI variables
 *   hinting toward the loaded ramdisk's location cannot be set. */
#define MFTAH_ENSURE_HINTS 1

/* The path to the executable to load within the decrypted boot image. */
/* TODO: This should probably change with the ARCH selection. */
#define MFTAH_CHAINLOAD_TARGET_PATH     L"EFI\\BOOT\\BOOTX64.EFI"

/* The path to the MFTAH loader executable currently running (at UEFI runtime). */
#define MFTAH_LOADER_EXE_PATH           L"EFI\\BOOT\\BOOTX64.EFI"

/* Colors used by the loader. */
#define MFTAH_COLOR_DEFAULT     (EFI_WHITE      | EFI_BACKGROUND_LIGHTGRAY)
#define MFTAH_COLOR_ASCII_ART   (EFI_CYAN       | EFI_BACKGROUND_LIGHTGRAY)

#define MFTAH_COLOR_DEBUG       (EFI_LIGHTCYAN  | EFI_BACKGROUND_BLACK)
#define MFTAH_COLOR_WARNING     (EFI_YELLOW     | EFI_BACKGROUND_BLACK)
#define MFTAH_COLOR_DANGER      (EFI_BROWN      | EFI_BACKGROUND_BLACK)
#define MFTAH_COLOR_PANIC       (EFI_LIGHTRED   | EFI_BACKGROUND_BLACK)

/*
 * Custom MFTAH UEFI response codes.
 *   These do NOT all indicate an explicit ERROR status. They
 *   are sometimes used to signal the caller to an action.
 */
#define EFI_NO_PAYLOAD_FOUND        0xd000000000000120
#define EFI_SINGLE_PAYLOAD_FOUND    0xd000000000000121
#define EFI_MULTI_PAYLOAD_FOUND     0xd000000000000122

#define EFI_MENU_GO_BACK            0xd000000000000123

#define EFI_INVALID_PASSWORD        0xd000000000000bad


#define CHAR char



/* "Error check" macro. Returns the EFI_STATUS if it's not EFI_SUCCESS. */
#define ERRCHECK(x) \
    { \
    Status = (x); \
    if (EFI_ERROR(Status)) { return Status; } \
    }

/* Same as above, except automatically uses the uefi_call_wrapper construct. */
#define ERRCHECK_UEFI(x,y,...) \
    ERRCHECK(uefi_call_wrapper(x, y, ##__VA_ARGS__))

/* Macro for setting console colors quickly. */
#define EFI_COLOR(x) \
    ST->ConOut->SetAttribute(ST->ConOut, (x));

/* Generic print macro, in case it ever needs to change later. */
#define PRINT(x, ...) \
    Print(x, ##__VA_ARGS__);
/* Same here. */
#define PRINTLN(x, ...) \
    Print(x L"\r\n", ##__VA_ARGS__);

/* Warning wrapper macros. */
#define EFI_WARNING(x, ...) \
    EFI_COLOR(MFTAH_COLOR_WARNING); \
    PRINT(x, ##__VA_ARGS__); \
    EFI_COLOR(MFTAH_COLOR_DEFAULT);
#define EFI_WARNINGLN(x, ...) \
    EFI_COLOR(MFTAH_COLOR_WARNING); \
    PRINTLN(x, ##__VA_ARGS__); \
    EFI_COLOR(MFTAH_COLOR_DEFAULT);

/* Danger wrapper macros. */
#define EFI_DANGER(x, ...) \
    EFI_COLOR(MFTAH_COLOR_DANGER); \
    PRINT(x, ##__VA_ARGS__); \
    EFI_COLOR(MFTAH_COLOR_DEFAULT);
#define EFI_DANGERLN(x, ...) \
    EFI_COLOR(MFTAH_COLOR_DANGER); \
    PRINTLN(x, ##__VA_ARGS__); \
    EFI_COLOR(MFTAH_COLOR_DEFAULT);

#if EFI_DEBUG==1
/* Debug-only. Prints debugging information when enabled by compiler flag. */
#define DPRINT(x, ...) \
    EFI_COLOR(MFTAH_COLOR_DEBUG); \
    Print(L"DEBUG:  " x, ##__VA_ARGS__); \
    EFI_COLOR(MFTAH_COLOR_DEFAULT);
#define DPRINTLN(x, ...) \
    EFI_COLOR(MFTAH_COLOR_DEBUG); \
    Print(L"DEBUG:  " x L"\r\n", ##__VA_ARGS__); \
    EFI_COLOR(MFTAH_COLOR_DEFAULT);
/* Debug-only. Dumps raw memory details when enabled by compiler flag. */
#define MEMDUMP(ptr, len) \
    EFI_COLOR(MFTAH_COLOR_DEBUG); \
    for (int i = 0; i < (len); ++i) { \
      Print(L"%02x%c", *((UINT8 *)(ptr)+i), !((i+1) % 16) ? '\n' : ' '); \
    } \
    if (!(len % 16)) Print(L"\r\n"); \
    EFI_COLOR(MFTAH_COLOR_DEFAULT);
#else
#define DPRINT(x, ...)
#define DPRINTLN(x, ...)
#define MEMDUMP(ptr, len)
#endif   /* #if EFI_DEBUG==1 */


/* pls don't change panic vars ok thx */
static CHAR16 * s_panic_input = NULL;
static CHAR16 s_panic_buffer[512] = {'P', 'A', 'N', 'I', 'C', '!', ' ', 0};
static unsigned int s_panic_buffer_cursor = 7;
#define PANIC(x) \
    { \
    s_panic_input = (x); \
    EFI_COLOR(MFTAH_COLOR_PANIC); \
    do { s_panic_buffer[s_panic_buffer_cursor] = s_panic_input[s_panic_buffer_cursor - 7]; } \
        while (++s_panic_buffer_cursor < 511 && '\0' != s_panic_input[s_panic_buffer_cursor - 7]); \
    s_panic_buffer[s_panic_buffer_cursor] = '\0'; \
    Print(s_panic_buffer); \
    Print(L"  Exit Code: '%d'", Status); \
    while (TRUE); \
    }

#define ABORT(x) PANIC(x)



/* A wrapper data structure for selected payload details. */
typedef
struct {
    mftah_payload_t         *Payload;
    BOOLEAN                 FromMultiSelect;
    CHAR16                  *Name;
    EFI_FILE_PROTOCOL       *VolumeHandle;
    UINT8 VOLATILE          SharedMutex;
} __attribute__((packed)) PAYLOAD;

/**
 * A meta-container for thread objects. These get dynamically assigned to available MPS when started.
 */
typedef
struct S_MFTAH_THREAD {
    UINTN VOLATILE          AssignedProcessorNumber;
    EFI_EVENT VOLATILE      CompletionEvent;
    BOOLEAN VOLATILE        Started;
    BOOLEAN VOLATILE        Finished;
    EFI_STATUS VOLATILE     ExitStatus;
    EFI_AP_PROCEDURE        Method;
    VOID VOLATILE *VOLATILE Context;
} MFTAH_THREAD;

/**
 * Primary structure of decryption thread contexts using the MP library.
 */
typedef
struct {
    MFTAH_THREAD VOLATILE   *Thread;
    UINT64                  CurrentPlace;
    mftah_work_order_t      *WorkOrder;
    mftah_progress_t        *Progress;
    UINT8                   Sha256Key[SIZE_OF_SHA_256_HASH];
    UINT8                   InitializationVector[AES_BLOCKLEN];
} DECRYPT_THREAD_CTX;


/* A canary file helps MFTAH identify the filesystem containing the
      right boot EFI, as opposed to iterating all SFS labels at runtime. */
static const CHAR16 *CanaryFileName = L"CROWS.CANARY";

/* Payload blob files are always suffixed with this extension. */
/*   This must remain upper-cased. */
static const CHAR16 *BootPayloadExtensionPattern = L".CROWS";


/* The Image Handle from EFI_MAIN, in case it's ever used in other modules. */
extern EFI_HANDLE gImageHandle;

/* The vendor GUID for XMIT (developer's affiliation). */
extern EFI_GUID gXmitVendorGuid;

/* The filename of the currently-selected payload being operated on. */
extern PAYLOAD VOLATILE gOperatingPayload;

/* The global MFTAH protocol instance to use. */
extern mftah_protocol_t *MFTAH;

/* A thread pool. */
extern MFTAH_THREAD VOLATILE Threads[MFTAH_MAX_THREAD_COUNT];



#endif   /* MFTAH_UEFI_H */
