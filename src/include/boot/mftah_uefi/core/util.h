#ifndef MFTAH_UTIL_H
#define MFTAH_UTIL_H

#include "core/mftah_uefi.h"
#include "drivers/threading.h"


#define MAX(x,y) \
    (((x) >= (y)) ? (x) : (y))
#define MIN(x,y) \
    (((x) <= (y)) ? (x) : (y))


/**
 * Print out a progress message. Updates the current cursor line.
 * 
 * @param[in] CurrentValue    A pointer to the current value.
 * @param[in] OutOfValue      A pointer to the maximum value.
 * @param[in] ExtraInfo       A useless value used to conform with the PROGRESS_HOOK interface.
 */
VOID
EFIAPI
PrintProgress(
    IN CONST UINT64 *CurrentValue,
    IN CONST UINT64 *OutOfValue,
    IN VOID         *ExtraInfo      OPTIONAL
);


/**
 * Calculate the 8-bit checksum of a buffer.
 * 
 * @param[in] Buffer   A pointer to the base of the input buffer.
 * @param[in] Length   The length of the buffer to be summed.
 * 
 * @returns An appropriate 8-bit checksum value.
 */
UINT8
EFIAPI
CalculateCheckSum8(
    IN CONST UINT8 *Buffer,
    IN CONST UINTN  Length
);



/**
 * Decrypt a specified region of memory with the given key.
 * 
 * @param[in]  MFTAH         A MFTAH protocol instance to use.
 * @param[in]  InputBuffer  The base of the raw buffer to decrypt.
 * @param[in]  InputLength  The length of the raw buffer to decrypt.
 * @param[in]  InitializationVector  The base of the IV used to encrypt/decrypt the buffer.
 * @param[in]  DecryptionKey  The base of the known decryption key data.
 * @param[in]  ProgressMethod  A function pointer to call when issuing incremental progress updates.
 * @param[in]  ProgressExtra  Any extra information or context to be passed to a PROGRESS_HOOK callback.
 */
EFI_STATUS
EFIAPI
DecryptWithKey(
    IN mftah_immutable_protocol_t    *MFTAH,
    IN UINT8                        *InputBuffer,
    IN UINT64                       InputLength,
    IN UINT8                        *InitializationVector   OPTIONAL,
    IN UINT8                        *DecryptionKey,
    IN mftah_fp__progress_hook_t     ProgressMethod          OPTIONAL,
    IN VOID                         *ProgressExtra          OPTIONAL
);


/**
 * TODO!
 */
mftah_status_t
SpawnDecryptionWorker(
    IN mftah_immutable_protocol_t    MFTAH,
    IN mftah_work_order_t            *WorkOrder,
    IN immutable_ref_t              Sha256Key,
    IN immutable_ref_t              InitializationVector,
    IN mftah_progress_t              *ProgressMeta           OPTIONAL
);

/**
 * TODO!
 */
void
UefiSpin(
    IN UINT64 *QueuedBytes
);


/**
 * Decrypts a set of data from a given context (threaded). This function is less
 *  of a utility and more of a specific operation in tandem with DecryptWithKey.
 * 
 * @param[in]  Context  The decryption context to use.
 */
VOID
EFIAPI
DecryptByContext(
    IN VOID *Context
);


/**
 * Get the size of the file from the given handle.
 *
 * @param[in] FileHandle    A pointer to a valid system file handle.
 *
 * @returns The length of the file given by the handle; 0 on failure.
 */
UINT64
EFIAPI
FileSize(
    IN CONST EFI_FILE_HANDLE *FileHandle
);


/**
 * Generic shutdown function. Panics if a shutdown could not be completed.
 * 
 * @param  Reason  Set to EFI_SUCCESS to indicate a normal shutdown. Otherwise, provide a status code to the ResetSystem call.
 * 
 * @returns Nothing. This is supposed to terminate the application (and stop the system).
 */
VOID
EFIAPI
Shutdown(
    IN CONST EFI_STATUS Reason
);



#endif   /* MFTAH_UTIL_H */