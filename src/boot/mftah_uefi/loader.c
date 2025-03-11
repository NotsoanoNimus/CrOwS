#include "core/loader.h"
#include "core/util.h"
#include "core/input.h"


EFI_STATUS
EFIAPI
DiscoverPayloads(IN EFI_HANDLE BaseImageHandle,
                 OUT EFI_FILE_PROTOCOL ***LoadedPayloadHandles,
                 OUT UINTN *LoadedHandlesCount,
                 OUT UINT8 *LoadedLoaderHash OPTIONAL)
{
    EFI_STATUS Status = EFI_SUCCESS;
    mftah_status_t MftahStatus = MFTAH_SUCCESS;
    
    EFI_LOADED_IMAGE *LoadedImage;
    
    EFI_FILE_IO_INTERFACE *ImageIo;
    EFI_FILE_PROTOCOL *VolumeHandle, *PayloadHandle;
    EFI_FILE_INFO *FileInfo;
    
    UINTN HandleCount, FileInfoSize;
    
    EFI_FILE_PROTOCOL *LoaderHandle;
    UINT8 *LoaderBuffer;
    UINT64 LoaderFileLength;
    UINT64 LoaderChunkSize = 1 << 12;   /* 4 KiB */

    PRINTLN(L"Detecting payloads on the boot filesystem...");

    if (NULL == LoadedPayloadHandles)
        return EFI_INVALID_PARAMETER;

    DPRINTLN(L"-- Fetching UEFI Loaded Image Protocol.");
    ERRCHECK_UEFI(
        BS->HandleProtocol, 3,
        BaseImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&LoadedImage
    );

    DPRINTLN(L"-- Getting volume handle via UEFI Simple Filesystem Protocol.");
    ERRCHECK_UEFI(
        BS->HandleProtocol, 3,
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&ImageIo
    );

    DPRINTLN(L"-- Opening root volume device handle.");
    ERRCHECK_UEFI(
        ImageIo->OpenVolume, 2,
        ImageIo,
        &VolumeHandle
    );

    /* Save this for later in case we need to reload a new payload file handle instance. */
    gOperatingPayload.VolumeHandle = VolumeHandle;

    *LoadedPayloadHandles = (EFI_FILE_PROTOCOL **)
        AllocatePool(sizeof(EFI_FILE_PROTOCOL *) * MFTAH_MAX_PAYLOADS);
    if (NULL == *LoadedPayloadHandles)
        return EFI_OUT_OF_RESOURCES;

    FileInfoSize = 1 << 12;   /* 4 KiB */
    FileInfo = (EFI_FILE_INFO *)AllocatePool(FileInfoSize);
    if (NULL == FileInfo)
        return EFI_OUT_OF_RESOURCES;

    /* Quickly create the hash of the loader, since the root filesystem is opened. */
    DPRINTLN(L"-- Attempting to hash the MFTAH loader EXE.");
    Status = uefi_call_wrapper(
        VolumeHandle->Open, 5,
        VolumeHandle,
        &LoaderHandle,
        MFTAH_LOADER_EXE_PATH,
        EFI_FILE_MODE_READ,
        EFI_FILE_READ_ONLY | EFI_FILE_ARCHIVE | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM
    );
    if (EFI_ERROR(Status)) {
        EFI_WARNINGLN(L"Failed to find the MFTAH application by path...");
        EFI_WARNINGLN(L"    The EFI variable '__MFTAH_LOADER_HASH'");
        EFI_WARNINGLN(L"    will not be available at OS runtime.");
    } else {
        LoaderFileLength = FileSize(&LoaderHandle);
        LoaderBuffer = (UINT8 *)AllocateZeroPool(LoaderFileLength);

        DPRINTLN(L"Opened loader file at (%llu) bytes. Readying hash.", LoaderFileLength);

        if (0 == LoaderFileLength || NULL == LoaderBuffer) {
            Status = EFI_OUT_OF_RESOURCES;
        } else {
            Status = uefi_call_wrapper(
                LoaderHandle->SetPosition,
                2,
                LoaderHandle,
                0
            );

            for (UINTN i = 0; LoaderChunkSize > 0; ++i) {
                Status = uefi_call_wrapper(
                    LoaderHandle->Read,
                    3,
                    LoaderHandle,
                    &LoaderChunkSize,
                    (LoaderBuffer + (i * (1 << 12)))
                );

                if (EFI_ERROR(Status)) break;
            }

            if (!EFI_ERROR(Status)) {
                MftahStatus = MFTAH->create_hash(MFTAH,
                                               LoaderBuffer,
                                               LoaderFileLength,
                                               LoadedLoaderHash,
                                               NULL);
            }
        }
        if (MFTAH_ERROR(MftahStatus) || EFI_ERROR(Status)) {
            EFI_WARNINGLN(L"Failed to hash the MFTAH binary...");
            EFI_WARNINGLN(L"    The EFI variable '__MFTAH_LOADER_HASH'");
            EFI_WARNINGLN(L"    will not be available at OS runtime.");
        }
    }

    /*
     * For sanity's sake, we only bother to check the root directory of the device
     *   in order to find a list of valid payload files.
     * 
     * Reading the root volume handle returns 0 when all files have been checked.
     */

    DPRINTLN(L"-- Enumerating device file handles, up to a maximum of %d.", MFTAH_MAX_PAYLOADS);
    for (HandleCount = 0; HandleCount < MFTAH_MAX_PAYLOADS; ) {
        DPRINTLN(L"---- Searching for next file... ");
        Status = uefi_call_wrapper(
            VolumeHandle->Read, 3,
            VolumeHandle,
            &FileInfoSize,
            FileInfo
        );

        if (EFI_BUFFER_TOO_SMALL == Status) {
            DPRINTLN(L"[readjusting buffer length, please hold]");
            
            FileInfoSize <<= 1;
            FreePool(FileInfo);
            FileInfo = (EFI_FILE_INFO *)AllocatePool(FileInfoSize);

            continue;
        } else if (0 == FileInfoSize) {
            DPRINTLN(L" (end of listing)");
            break;
        } else if (EFI_ERROR(Status)) {
            return Status;
        }

        DPRINTLN(L"'%s' (%lu)", FileInfo->FileName, FileInfo->FileSize);

        /* The filename must be greater than the suffix pattern to check against. */
        if (StrSize(FileInfo->FileName) > StrSize(BootPayloadExtensionPattern)) {
            CHAR16 *slice = FileInfo->FileName
                + StrLen(FileInfo->FileName)
                - StrLen(BootPayloadExtensionPattern);
            StrUpr(slice);

            if (0 == StrCmp(BootPayloadExtensionPattern, slice)) {
                DPRINTLN(L"------ Valid payload file! ");
                ERRCHECK_UEFI(
                    VolumeHandle->Open, 5,
                    VolumeHandle,
                    &PayloadHandle,
                    FileInfo->FileName,
                    EFI_FILE_MODE_READ,
                    EFI_FILE_READ_ONLY | EFI_FILE_ARCHIVE | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM
                );

                CopyMem(&((*LoadedPayloadHandles)[HandleCount]),
                        &PayloadHandle,
                        sizeof(EFI_FILE_PROTOCOL *));

                PRINTLN(L"-- Discovered '%s'.", FileInfo->FileName);

                ++HandleCount;
                DPRINTLN(L"(open,success)");
            }
        }

        ZeroMem(FileInfo, FileInfoSize);
        FileInfoSize = 1 << 12;
    }

    DPRINTLN(L"-- All done!");
    PRINTLN(L"");

    *LoadedHandlesCount = HandleCount;
    FreePool(FileInfo);

    if (EFI_ERROR(Status)) return Status;

    /* Return a code based on the amount of discovered handles. */
    switch (HandleCount) {
        case 0: return EFI_NO_PAYLOAD_FOUND;
        case 1: return EFI_SINGLE_PAYLOAD_FOUND;
        default: return EFI_MULTI_PAYLOAD_FOUND;
    }
}


VOID
EFIAPI
SelectPayload(IN OUT EFI_FILE_PROTOCOL **PayloadFileHandle,
              OUT UINT64 *PayloadFileStartingPosition,
              OUT UINT8 *LoadedLoaderHash OPTIONAL)
{
    EFI_STATUS Status = EFI_SUCCESS;
    
    EFI_FILE_PROTOCOL **PayloadHandles = NULL;
    UINTN PayloadHandlesCount = 0;
    EFI_FILE_INFO *CurrentFileHandleInfo = NULL;
    CHAR16 *CurrentFileNameShadow = NULL;

    EFI_FILE_PROTOCOL *PartialFileMatch = NULL;
    UINTN PartialMatchesCount = 0;

    UINT8 SelectedPayloadName[MFTAH_MAX_FILENAME_LENGTH+1] = {0};
    UINTN SelectedPayloadNameLength = 0;
    BOOLEAN IsValidSelection = FALSE;

    Status = DiscoverPayloads(gImageHandle,
                              &PayloadHandles,
                              &PayloadHandlesCount,
                              LoadedLoaderHash);
    switch (Status) {
        case EFI_NO_PAYLOAD_FOUND:
            PANIC(L"No compatible MFTAH payload was found on the boot filesystem/partition.");
        case EFI_MULTI_PAYLOAD_FOUND:
            PRINTLN(L"\r\nMultiple payloads were discovered. Choose one to load by typing its name.");
            PRINTLN(L"   You can also type the first few unique characters of a payload to select it.\r\n");
            
            while (!IsValidSelection) {
                /* Reset between selections. */
                PartialFileMatch = NULL;
                PartialMatchesCount = 0;

                Status = ReadChar16KeyboardInput(L"Which payload? ('q' to quit and reboot): ",
                                                 SelectedPayloadName,
                                                 &SelectedPayloadNameLength,
                                                 FALSE,
                                                 MFTAH_MAX_FILENAME_LENGTH);
                if (EFI_ERROR(Status)) {
                    PANIC(L"There was a problem parsing which payload file was selected.");
                }
                if (0 == SelectedPayloadNameLength) {
                    continue;
                }

                if (0 == StrnCmp(SelectedPayloadName, L"q\0", 2 * sizeof(CHAR16))) {
                    Shutdown(EFI_SUCCESS);
                }

                /* Convert the selected payload's name to uppercase for case-insensitive comparisons. */
                StrUpr(SelectedPayloadName);
                
                for (UINTN i = 0; i < PayloadHandlesCount; ++i) {
                    CurrentFileHandleInfo = LibFileInfo(PayloadHandles[i]);
                    if (NULL == CurrentFileHandleInfo) {
                        PRINTLN(L"\r\nSystem appears to be out of resources. This means your selection might not work.");
                        continue;
                    }

                    /* Preserve the original filename. */
                    CurrentFileNameShadow = StrDuplicate(CurrentFileHandleInfo->FileName);

                    /* Convert the target filename to uppercase for case-insensitive comparisons. */
                    StrUpr(CurrentFileHandleInfo->FileName);

                    DPRINTLN(L"Selected '%s' vs Check '%s'", SelectedPayloadName, CurrentFileHandleInfo->FileName);
                    
                    /* First, check the whole filename. */
                    if (0 != StrnCmp(SelectedPayloadName, CurrentFileHandleInfo->FileName, MFTAH_MAX_FILENAME_LENGTH)) {
                        /* If it's not a full match, but it is partial, point to it and add to the partials counter. */
                        if (0 == StrnCmp(SelectedPayloadName, CurrentFileHandleInfo->FileName, SelectedPayloadNameLength)) {
                            PartialFileMatch = PayloadHandles[i];
                            PartialMatchesCount++;

                            if (1 == PartialMatchesCount) {
                                PRINTLN(L"    Found a partial match with '%s'.\r\n", CurrentFileNameShadow);
                                gOperatingPayload.Name = StrDuplicate(CurrentFileHandleInfo->FileName);
                            } else if (2 == PartialMatchesCount) {
                                PRINTLN(L"    Found another partial match with '%s'. Failed to determine which one to load.\r\n", CurrentFileNameShadow);
                                
                                FreePool(gOperatingPayload.Name);
                                gOperatingPayload.Name = NULL;

                                FreePool(CurrentFileHandleInfo);
                                FreePool(CurrentFileNameShadow);
                                break;
                            }
                        }

                        FreePool(CurrentFileHandleInfo);
                        FreePool(CurrentFileNameShadow);
                        continue;
                    }
                    
                    IsValidSelection = TRUE;
                    *PayloadFileHandle = PayloadHandles[i];

                    PRINTLN(L"++++ Selected payload '%s'...\r\n", CurrentFileNameShadow);

                    FreePool(CurrentFileHandleInfo);
                    FreePool(CurrentFileNameShadow);
                    break;
                }

                /* If and only if a SINGLE partial match was found, use it. */
                if (1 == PartialMatchesCount && NULL != PartialFileMatch) {
                    *PayloadFileHandle = PartialFileMatch;
                    IsValidSelection = TRUE;
                }

                if (!IsValidSelection) {
                    PRINTLN(L"    The selected payload name was not found. Try again.\r\n");
                } else {
                    PRINTLN(L"");
                }
            }

            gOperatingPayload.FromMultiSelect = TRUE;
            break;
        case EFI_SINGLE_PAYLOAD_FOUND:
            DPRINTLN(L"Got a single payload option. Easy choice.");

            CurrentFileHandleInfo = LibFileInfo(PayloadHandles[0]);
            gOperatingPayload.Name = StrDuplicate(CurrentFileHandleInfo->FileName);
            FreePool(CurrentFileHandleInfo);

            *PayloadFileHandle = PayloadHandles[0];
            PRINTLN(L"Loading payload '%s'...", gOperatingPayload.Name);

            gOperatingPayload.FromMultiSelect = FALSE;
            break;
        default:   /* Even an EFI_SUCCESS should not be returned. */
            PANIC(L"Failure while gathering a list of MFTAH payloads!");
    }

    Status = uefi_call_wrapper(
        (*PayloadFileHandle)->GetPosition, 2,
        (*PayloadFileHandle),
        PayloadFileStartingPosition
    );
    if (EFI_ERROR(Status)) {
        PANIC(L"Could not get payload handle position.");
    }

    PRINTLN(L"");
}
