#include "core/util.h"
#include "drivers/threading.h"



UINT8
EFIAPI
CalculateCheckSum8(IN CONST UINT8 *Buffer,
                   IN CONST UINTN Length)
{
    UINT8 Checksum;
    UINTN Count;

    if (NULL == Buffer ||
        Length > (MAX_ADDRESS - ((UINTN)Buffer) + 1) ||
        0 == Length) {
        return 0;
    }

    for (Checksum = 0, Count = 0; Count < Length; ++Count)
        Checksum = (UINT8)(Checksum + *(Buffer + Count));

    return Checksum;
}


UINT64
EFIAPI
FileSize(IN CONST EFI_FILE_HANDLE *FileHandle)
{
    UINT64 value;
    EFI_FILE_INFO *FileInfo;

    FileInfo = LibFileInfo(*FileHandle);
    if (NULL == FileInfo) {
        return 0;
    }

    value = FileInfo->FileSize;

    FreePool(FileInfo);

    return value;
}


VOID
EFIAPI
Shutdown(IN CONST EFI_STATUS Reason)
{
    EFI_STATUS Status = EFI_SUCCESS;

    Status = uefi_call_wrapper(gRT->ResetSystem, 4, EfiResetCold, Reason, 0, NULL);

    PANIC(L"System execution suspended, but failed to restart automatically. Please reboot manually.");
}


VOID
EFIAPI
PrintProgress(IN CONST UINT64 *CurrentValue,
              IN CONST UINT64 *OutOfValue,
              IN VOID *ExtraInfo OPTIONAL)
{
    UINT8 PercentDone, PercentByFive;
    double PercentRaw;
    
    if (NULL == CurrentValue || NULL == OutOfValue || 0 == *OutOfValue)
        return;

    PercentRaw = (double)(*CurrentValue) / (double)(*OutOfValue);
    PercentDone = MIN(100, (int)(PercentRaw * 100.0f));
    PercentByFive = MIN(20, (PercentDone / 5));

    Print(L"\r  %3d%% [", PercentDone);
    for (int i = 0; i < PercentByFive; ++i) Print(L"=");
    for (int i = 0; i < (20 - PercentByFive); ++i) Print(L" ");
    Print(L"] (%16x / %16x)   ", *CurrentValue, *OutOfValue);
}


/* This is annoying, but it's a per-thread hook function to provide
    real-time updates on progress. */
STATIC
VOID
SaveThreadProgress(const uint64_t *cur,
                   const uint64_t *out_of,
                   void *extra)
{
    if (NULL == extra || NULL == cur) return;

    *((uint64_t *)extra) = *cur;
}


STATIC
VOID
EFIAPI
work(VOID *ctx)
{
    DECRYPT_THREAD_CTX *Context = (DECRYPT_THREAD_CTX *)ctx;
    mftah_status_t Status = MFTAH_SUCCESS;

    Status = MFTAH_CRYPT_HOOK_DEFAULT(MFTAH,
                                     Context->WorkOrder,
                                     Context->Sha256Key,
                                     Context->InitializationVector,
                                     Context->Progress);
    
    if (MFTAH_ERROR(Status)) {
        PANIC(L"A failure was reported during the thread operation.");
    }

    /*
     * NOTE: The UEFI event system has proven unreliable and it's very hard
     *        to find documentation explaining why. At this point, the thread
     *        is finished and we don't need to wait for the event to wrap up
     *        the thread for us, so `FinishThread` is called manually for the
     *        sake of reliability while booting.
     */
    FinishThread(NULL, (VOID *)(Context->Thread));
}


mftah_status_t
SpawnDecryptionWorker(IN mftah_immutable_protocol_t MFTAH,
                      IN mftah_work_order_t *WorkOrder,
                      IN immutable_ref_t Sha256Key,
                      IN immutable_ref_t InitializationVector,
                      IN mftah_progress_t *ProgressMeta OPTIONAL)
{
    EFI_STATUS Status = EFI_SUCCESS;
    mftah_status_t MftahStatus = MFTAH_SUCCESS;
    DECRYPT_THREAD_CTX VOLATILE *NewThreadContext = NULL;
    mftah_progress_t ThreadProgress = {0};
    mftah_progress_t *ThreadProgressClone = NULL;
    mftah_work_order_t *WorkOrderClone = NULL;

    if (
        NULL == MFTAH
        || NULL == WorkOrder
        || NULL == Sha256Key
        || NULL == InitializationVector
    ) {
        return MFTAH_INVALID_PARAMETER;
    }

    /* The library should never return a thread index higher than what is possible. */
    if (WorkOrder->thread_index >= MFTAH_MAX_THREAD_COUNT) {
        EFI_DANGERLN(
            L"Invalid thread index of %u. MFTAH has a thread range of 1 to %u.",
            WorkOrder->thread_index,
            MFTAH_MAX_THREAD_COUNT
        );

        Status = EFI_ABORTED;
        PANIC(L"Invalid thread index.");
    }

    if (!WorkOrder->length) return MFTAH_SUCCESS;

    /* Set up the progress hook. */
    ThreadProgress.context = NULL;
    ThreadProgress.hook = (FALSE != WorkOrder->suppress_progress)
        ? NULL
        : (IsThreadingEnabled() ? SaveThreadProgress : PrintProgress);

    /* This will synchronously run the operation. Each progress message is tracked individually. */
    if (!IsThreadingEnabled()) {
        PRINTLN(L"\r\nDecrypting contiguous block at (%p) of (%llu) bytes.", WorkOrder->location, WorkOrder->length);

        MftahStatus = MFTAH_CRYPT_HOOK_DEFAULT(MFTAH,
                                             WorkOrder,
                                             Sha256Key,
                                             InitializationVector,
                                             &ThreadProgress);

        PRINT(L"   OK\r\n");
        return MftahStatus;
    }

    /* When threading, the work order has everything necessary for the decryption worker. */
    if (Threads[WorkOrder->thread_index].Started && !Threads[WorkOrder->thread_index].Finished) {
        return MFTAH_THREAD_BUSY;
    }

    NewThreadContext = (DECRYPT_THREAD_CTX *)AllocateZeroPool(sizeof(DECRYPT_THREAD_CTX));
    NewThreadContext->CurrentPlace = 0;
    NewThreadContext->Thread = &(Threads[WorkOrder->thread_index]);
    CopyMem(NewThreadContext->InitializationVector, (VOID *)InitializationVector, AES_BLOCKLEN);
    CopyMem(NewThreadContext->Sha256Key, (VOID *)Sha256Key, SIZE_OF_SHA_256_HASH);

    WorkOrderClone = (mftah_work_order_t *)AllocateZeroPool(sizeof(mftah_work_order_t));
    CopyMem(WorkOrderClone, WorkOrder, sizeof(mftah_work_order_t));
    NewThreadContext->WorkOrder = WorkOrderClone;

    ThreadProgress.context = (VOID *)&(NewThreadContext->CurrentPlace);

    ThreadProgressClone = (mftah_progress_t *)AllocateZeroPool(sizeof(mftah_progress_t));
    CopyMem(ThreadProgressClone, &ThreadProgress, sizeof(mftah_progress_t));
    NewThreadContext->Progress = ThreadProgressClone;

    DPRINTLN(
        L"Initializing decryption worker #%u (%p : 0x%08llx).",
        WorkOrderClone->thread_index,
        WorkOrderClone->location,
        WorkOrderClone->length
    );

    Status = CreateThread(work, (VOID *)NewThreadContext, (MFTAH_THREAD *)(NewThreadContext->Thread));
    if (EFI_ERROR(Status)) {
        PANIC(L"Unable to create a new thread instance.");
    }

    /*
     * DO NOT wait for the thread to start here. Let all threads be structured and then
     *   the BSP will move onto the "spin" function, where lingering, waiting threads can
     *   be started as other work finishes.
     */
    // Status = StartThread(Thread, FALSE);
    // if (EFI_ERROR(Status) && EFI_OUT_OF_RESOURCES != Status) {
    //     PANIC(L"Critical error while trying to start a decryption thread.");
    // }

    return MFTAH_SUCCESS;
}


void
UefiSpin(IN UINT64 *QueuedBytes)
{
    EFI_STATUS Status = EFI_SUCCESS;
    DECRYPT_THREAD_CTX *ThisThreadContext = NULL;
    BOOLEAN SuppressProgress = FALSE;
    BOOLEAN Completed = TRUE;
    UINT64 Progress = 0;
    UINT64 TotalProgress = *QueuedBytes;

    /* No need to spin and wait when threading is not supported. */
    if (!IsThreadingEnabled()) return;

    DPRINTLN(L"Initiating main thread spin and awaiting.");
    DPRINTLN(L"Got payload size to launder: %llu", TotalProgress);

    if (0 == TotalProgress) {
        PANIC(L"Total progress indicates the payload size is 0.");
    }

    /* Quickly check whether any of the threads indicate progress suppression. */
    for (int i = 0; i < MFTAH_MAX_THREAD_COUNT; ++i) {
        ThisThreadContext = (DECRYPT_THREAD_CTX *)(Threads[i].Context);
        if (NULL != ThisThreadContext && NULL != ThisThreadContext->WorkOrder) {
            SuppressProgress |= ThisThreadContext->WorkOrder->suppress_progress;
        }
    }

    do {
        Completed = TRUE;
        Progress = FALSE;
        for (int i = 0; i < MFTAH_MAX_THREAD_COUNT; ++i) {
            ThisThreadContext = (DECRYPT_THREAD_CTX *)(Threads[i].Context);

            if (NULL == ThisThreadContext || NULL == ThisThreadContext->WorkOrder) {
                /* If a thread isn't active or has no context pointer, skip it. */
                continue;
            }

            if (!Threads[i].Started && !Threads[i].Finished) {
                StartThread((MFTAH_THREAD *)&Threads[i], FALSE);   /* Keep trying to start waiting threads. */
            }

            Completed &= Threads[i].Finished;
            Progress += ThisThreadContext->CurrentPlace;
        }

        if (!SuppressProgress) {
            PrintProgress(&Progress, &TotalProgress, NULL);
        }

        /* 50 ms sleep. */
        uefi_call_wrapper(BS->Stall, 1, (50 * 1000));
    } while (!Completed && Progress < TotalProgress);

    DPRINTLN(L"All done!");

    /* Be sure the 100% message always gets out. */
    if (!SuppressProgress) {
        PrintProgress(&TotalProgress, &TotalProgress, NULL);
        PRINT(L"\n    ~~~ OK ~~~\n\n");
    }
}
