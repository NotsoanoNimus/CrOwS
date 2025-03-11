#include "drivers/threading.h"
#include "core/mftah_uefi.h"


/* Mutex to carefully synchronize certain thread operations. */
STATIC BOOLEAN ThreadMutex = FALSE;


/* Maintain an open handle to a loaded MP service protocol from the DXE. */
STATIC
EFI_MP_SERVICES_PROTOCOL *
mEfiMpServicesProtocol = NULL;

/* Keep a local structure present for all threading operations. */
STATIC
MFTAH_SYSTEM_MP_CTX VOLATILE
mSystemMultiprocessingContext = {0};

/* Bring the GUID definition into a reference-able variable. */
STATIC
EFI_GUID
mEfiMpServicesProtocolGuid = EFI_MP_SERVICES_PROTOCOL_GUID;


/* Internal method to refresh MP states for mSystemMultiprocessingContext. */
STATIC
EFI_STATUS
EFIAPI
RefreshMPs()
{
    MUTEX_SYNC(ThreadMutex);
    MUTEX_LOCK(ThreadMutex);

    DPRINTLN(L"=-=-= Refreshing known MPs state. =-=-=");

    EFI_STATUS Status = EFI_SUCCESS;
    EFI_PROCESSOR_INFORMATION CurrentProcessorInfo = {0};
    MFTAH_SYSTEM_MP *OldMPsList = NULL;
    
    MFTAH_SYSTEM_MP *ListOfSystemMPs = (MFTAH_SYSTEM_MP *)
        AllocateZeroPool(sizeof(MFTAH_SYSTEM_MP) * mSystemMultiprocessingContext.MpCount);
    if (NULL == ListOfSystemMPs) {
        MUTEX_UNLOCK(ThreadMutex);
        return EFI_OUT_OF_RESOURCES;
    }

    for (UINTN i = 0; i < mSystemMultiprocessingContext.MpCount; ++i) {
        Status = uefi_call_wrapper(
            mEfiMpServicesProtocol->GetProcessorInfo, 3,
            mEfiMpServicesProtocol,
            i,
            &CurrentProcessorInfo
        );
        if (EFI_ERROR(Status)) {
            MUTEX_UNLOCK(ThreadMutex);
            return EFI_ABORTED;
        }

        ListOfSystemMPs[i].IsBSP = !!(CurrentProcessorInfo.StatusFlag & PROCESSOR_AS_BSP_BIT);
        ListOfSystemMPs[i].IsEnabled = !!(CurrentProcessorInfo.StatusFlag & PROCESSOR_ENABLED_BIT);
        ListOfSystemMPs[i].IsHealthy = !!(CurrentProcessorInfo.StatusFlag & PROCESSOR_HEALTH_STATUS_BIT);
        ListOfSystemMPs[i].IsWorking = mSystemMultiprocessingContext.MpList[i].IsWorking;
        ListOfSystemMPs[i].ProcessorNumber = i;
    }

    OldMPsList = mSystemMultiprocessingContext.MpList;
    mSystemMultiprocessingContext.MpList = ListOfSystemMPs;
    FreePool(OldMPsList);

    MUTEX_UNLOCK(ThreadMutex);
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
InitializeThreading()
{
    EFI_STATUS Status;

    UINTN NumberOfProcessors = 0;
    UINTN NumberOfEnabledProcessors = 0;
    UINTN BspProcessorNumber = 0;

    MFTAH_SYSTEM_MP *ListOfSystemMPs = NULL;
    EFI_PROCESSOR_INFORMATION CurrentProcessorInfo = {0};

    UINT32 HealthyStatus = (0x0 | PROCESSOR_HEALTH_STATUS_BIT);

    /* First, get the protocol to determine if MP is supported. */
    Status = uefi_call_wrapper(
        BS->LocateProtocol,
        3,
        &mEfiMpServicesProtocolGuid,
        NULL,
        (VOID **)&mEfiMpServicesProtocol
    );
    if (EFI_ERROR(Status)) {
        EFI_WARNINGLN(L"Fetching MP protocol returned code '%d'.", Status);
        mEfiMpServicesProtocol = NULL;
        return Status;
    }

    DPRINTLN(L"Getting system MP count.");
    ERRCHECK_UEFI(
        mEfiMpServicesProtocol->GetNumberOfProcessors, 3,
        mEfiMpServicesProtocol,
        &NumberOfProcessors,
        &NumberOfEnabledProcessors
    );
    DPRINTLN(
        L"-- Found '%d' multiprocessor objects ('%d' enabled) to utilize.",
        NumberOfProcessors,
        NumberOfEnabledProcessors
    );

    DPRINTLN(L"Getting BSP MP number.");
    ERRCHECK_UEFI(
        mEfiMpServicesProtocol->WhoAmI, 2,
        mEfiMpServicesProtocol,
        &BspProcessorNumber
    );
    DPRINTLN(L"--- Found BSP processor number: '%d'", BspProcessorNumber);

    ListOfSystemMPs = (MFTAH_SYSTEM_MP *)
        AllocateZeroPool(sizeof(MFTAH_SYSTEM_MP) * NumberOfProcessors);
    if (NULL == ListOfSystemMPs) {
        return EFI_OUT_OF_RESOURCES;
    }

    /* Iterate each processor information block and extract useful information from each. */
    DPRINTLN(L"Extracting processor information from each MP.");
    for (UINTN i = 0; i < NumberOfProcessors; ++i) {
        DPRINTLN(L"-- Read logical processor #%d", i);
        Status = uefi_call_wrapper(
            mEfiMpServicesProtocol->GetProcessorInfo, 3,
            mEfiMpServicesProtocol,
            i,
            &CurrentProcessorInfo
        );

        switch (Status) {
            case EFI_SUCCESS:
                break;
            case EFI_NOT_FOUND:
                EFI_WARNINGLN(L"Processor handle #%d does not exist on this system. Skipping.", i);
                NumberOfProcessors--;
                continue;
            case EFI_DEVICE_ERROR:
                EFI_DANGERLN(L"The calling processor attempting to enumerate system MPs in an AP. This is illegal!");
                break;
            case EFI_INVALID_PARAMETER:
                EFI_DANGERLN(L"A buffer with insufficient size was provided during system MP enumeration.");
                break;
            default:
                EFI_DANGERLN(L"An unknown error occurred while enumerating system MPs: '%d'", Status);
                break;
        }
        if (EFI_ERROR(Status)) {
            EFI_DANGERLN(L"-- Multiprocessing support disabled.");

            FreePool(ListOfSystemMPs);
            return Status;
        }

        if (!(CurrentProcessorInfo.StatusFlag & PROCESSOR_AS_BSP_BIT)) {
            Status = uefi_call_wrapper(
                mEfiMpServicesProtocol->EnableDisableAP, 4,
                mEfiMpServicesProtocol,
                i,
                TRUE,
                &HealthyStatus
            );

            switch (Status) {
                case EFI_SUCCESS:
                    DPRINTLN(L"-- Enabled MP #%d", i);
                    break;
                case EFI_UNSUPPORTED:
                    EFI_DANGERLN(L"Failed to enable MP #%d.", i);
                    break;
                case EFI_DEVICE_ERROR:
                    EFI_WARNINGLN(L"Cannot enable an MP from an AP.");
                    break;
                case EFI_NOT_FOUND:
                    EFI_WARNINGLN(L"Processor handle #%d does not exist on this system. Not enabling.", i);
                    break;
                case EFI_INVALID_PARAMETER:
                    EFI_WARNINGLN(L"The BSP processor is already enabled.");
                    break;
                default: break;
            }
        }

        DPRINTLN(
            L"---- MP Info: ID 0x%x (P%u:C%u:T%u) %s / %s / %s",
            CurrentProcessorInfo.ProcessorId,
            CurrentProcessorInfo.Location.Package,
            CurrentProcessorInfo.Location.Core,
            CurrentProcessorInfo.Location.Thread,
            (CurrentProcessorInfo.StatusFlag & PROCESSOR_AS_BSP_BIT)
                ? L"BSP" : L"AP",
            (CurrentProcessorInfo.StatusFlag & PROCESSOR_ENABLED_BIT)
                ? L"Enabled" : L"Disabled",
            (CurrentProcessorInfo.StatusFlag & PROCESSOR_HEALTH_STATUS_BIT)
                ? L"Healthy" : L"Unhealthy"
        );

        ListOfSystemMPs[i].IsBSP = !!(CurrentProcessorInfo.StatusFlag & PROCESSOR_AS_BSP_BIT);
        ListOfSystemMPs[i].IsEnabled = !!(CurrentProcessorInfo.StatusFlag & PROCESSOR_ENABLED_BIT);
        ListOfSystemMPs[i].IsHealthy = !!(CurrentProcessorInfo.StatusFlag & PROCESSOR_HEALTH_STATUS_BIT);
        ListOfSystemMPs[i].IsWorking = FALSE;
        ListOfSystemMPs[i].ProcessorNumber = i;
    }

    DPRINTLN(
        L"-- Got %u processors, BSP @%u, and a list at %p.",
        NumberOfProcessors,
        BspProcessorNumber,
        ListOfSystemMPs
    );
    MEMDUMP(ListOfSystemMPs, sizeof(MFTAH_SYSTEM_MP) * NumberOfProcessors);

    /* Populate the static thread tracker variable. */
    mSystemMultiprocessingContext.MpCount = NumberOfProcessors;
    mSystemMultiprocessingContext.BspProcessorNumber = BspProcessorNumber;
    mSystemMultiprocessingContext.MpList = ListOfSystemMPs;

    return EFI_SUCCESS;
}


BOOLEAN
EFIAPI
IsThreadingEnabled()
{
    return NULL != mEfiMpServicesProtocol || mSystemMultiprocessingContext.MpCount >= 1;
}


UINTN
EFIAPI
GetThreadLimit()
{
    /* Need to subtract the BSP, which never runs threads. */
    return IsThreadingEnabled()
        ? (mSystemMultiprocessingContext.MpCount - 1)
        : 0;
}


EFI_STATUS
EFIAPI
CreateThread(IN EFI_AP_PROCEDURE Method,
             IN VOID             *Context,
             IN OUT MFTAH_THREAD  *NewThread)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (
        NULL == Method
        || NULL == Context
        || NULL == NewThread
    ) {
        return EFI_INVALID_PARAMETER;
    }

    NewThread->Method = Method;
    NewThread->Context = Context;
    NewThread->Finished = FALSE;
    NewThread->Started = FALSE;

    Status = uefi_call_wrapper(
        BS->CreateEvent,
        5,
        (EVT_TIMER | EVT_NOTIFY_SIGNAL),
        TPL_NOTIFY,
        /* Note: sometimes this isn't called, so it's manually called at the end of `work`. */
        (EFI_EVENT_NOTIFY)FinishThread,
        (VOID *)NewThread,
        (EFI_EVENT *)&(NewThread->CompletionEvent)
    );
    if (EFI_ERROR(Status)) {
        PANIC(L"Unable to register thread completion event.");
    }

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
StartThread(IN MFTAH_THREAD *Thread,
            IN BOOLEAN      Wait)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (Thread->Started) {
        DPRINTLN(L"StartThread: Already started.");
        return EFI_ALREADY_STARTED;
    } else if (Thread->Finished) {
        DPRINTLN(L"StartThread: Already finished.");
        return EFI_INVALID_PARAMETER;
    }

    do {
        RefreshMPs();

        for (UINTN i = 0; i < mSystemMultiprocessingContext.MpCount; ++i) {
            if (
                mSystemMultiprocessingContext.MpList[i].IsBSP
                || mSystemMultiprocessingContext.MpList[i].IsWorking
                // || !mSystemMultiprocessingContext.MpList[i].IsHealthy
            ) {
                DPRINTLN(L"StartThread: MP #%u is not available.", i);
                uefi_call_wrapper(BS->Stall, 1, 5*1000);   /* 5ms delay */
                continue;
            }

            /* Kick off the operation and mark the AP as working. */
            DPRINTLN(L"StartThread: MP #%u is available. Assigning thread task.", i);
            ERRCHECK_UEFI(
                mEfiMpServicesProtocol->StartupThisAP,
                7,
                mEfiMpServicesProtocol,
                Thread->Method,
                i,
                Thread->CompletionEvent,
                0,
                (VOID *)Thread->Context,
                NULL
            );

            MUTEX_SYNC(ThreadMutex);
            MUTEX_LOCK(ThreadMutex);

            mSystemMultiprocessingContext.MpList[i].IsWorking = TRUE;

            MUTEX_UNLOCK(ThreadMutex);

            Thread->Started = TRUE;
            Thread->AssignedProcessorNumber = i;
            break;
        }
    } while (!Thread->Started && Wait);

    if (!Thread->Started) {
        return EFI_OUT_OF_RESOURCES;
    }

    return EFI_SUCCESS;
}


VOID
EFIAPI
FinishThread(IN EFI_EVENT EventSource,
             IN VOID *Thread)
{
    UINTN ProcNumber = 0;

    if (NULL == Thread) {
        EFI_WARNINGLN(L"FinishThread: The thread to close is NULL.");
        return;
    } else if (TRUE == ((MFTAH_THREAD *)Thread)->Finished) {
        DPRINTLN(L"FinishThread: The thread is already finished. Moving on...");
        return;
    }

    ProcNumber = ((MFTAH_THREAD *)Thread)->AssignedProcessorNumber;

    MUTEX_SYNC(ThreadMutex);
    MUTEX_LOCK(ThreadMutex);

    mSystemMultiprocessingContext.MpList[ProcNumber].IsWorking = FALSE;

    MUTEX_UNLOCK(ThreadMutex);

    ((MFTAH_THREAD *)Thread)->Finished = TRUE;
    DPRINTLN(L"Finished thread on MP #%d.", ProcNumber);

    RefreshMPs();
}


VOID
EFIAPI
JoinThread(IN MFTAH_THREAD *Thread)
{
    while (Thread->Started && !Thread->Finished);
    RefreshMPs();
}


VOID
EFIAPI
DestroyThread(IN MFTAH_THREAD *Thread)
{
    while (Thread->Started && !Thread->Finished);
    FreePool(Thread);
    RefreshMPs();
}
