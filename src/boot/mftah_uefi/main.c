#include "core/mftah_uefi.h"

#include "core/util.h"
#include "core/loader.h"
#include "core/input.h"
#include "core/wrappers.h"

#include "drivers/graphics.h"
#include "drivers/ramdisk.h"
#include "drivers/threading.h"


/* "Fixup" for GNU-EFI's print.c compilation module. idk */
extern UINTN _fltused = 0;


UINT8 *gRamdiskImage;
UINT64 gRamdiskImageLength;

EFI_HANDLE gImageHandle;
EFI_GUID gXmitVendorGuid = XMIT_XYZ_VENDOR_GUID;
PAYLOAD VOLATILE gOperatingPayload = {0};

mftah_protocol_t *MFTAH;

MFTAH_THREAD VOLATILE Threads[MFTAH_MAX_THREAD_COUNT];



STATIC VOID EFIAPI EnvironmentInitialize();

STATIC EFI_STATUS EFIAPI GetPassword(
    OUT UINT8 *Password,
    OUT UINT8 *PassLen OPTIONAL
);

STATIC EFI_STATUS EFIAPI CheckPassword(
    IN EFI_FILE_PROTOCOL *PayloadFileHandle,
    IN CONST UINT8 *Password,
    IN CONST UINT8 PasswordLength,
    OUT CHAR8 *PasswordActual,
    OUT UINT8 *PasswordLengthActual
);

STATIC EFI_STATUS EFIAPI LoadAndDecrypt(
    IN EFI_FILE_PROTOCOL *PayloadFileHandle,
    IN CONST UINT8 *Password,
    IN CONST UINT8 PasswordLength,
    OUT UINT8 *LoadedPayloadHash
);

STATIC EFI_STATUS EFIAPI SetEfiVarsHints(
    IN UINT8 **RamdiskMemoryLocation,
    IN UINT64 *RamdiskLength,
    IN UINT8 *LoadedLoaderHash,
    IN UINT8 *LoadedPayloadHash
);

STATIC EFI_STATUS EFIAPI WrapperRegisterRamdisk();
STATIC EFI_STATUS EFIAPI JumpToRamdisk();


EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle,
         EFI_SYSTEM_TABLE *SystemTable)
{
    /* Local variable initializations. */
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_INPUT_KEY Key = {0};

    UINT8 Password[(MFTAH_MAX_PW_LEN * sizeof(CHAR16)) + 1] = {0};
    UINT8 PasswordLength = 0;
    CHAR8 PasswordActual[MFTAH_MAX_PW_LEN + 1] = {0};
    UINT8 PasswordLengthActual = 0;

    UINT8 LoadedLoaderHash[SIZE_OF_SHA_256_HASH] = {0};
    UINT8 LoadedPayloadHash[SIZE_OF_SHA_256_HASH] = {0};

    UINT64 PayloadFileStartingPosition = 0;
    EFI_FILE_PROTOCOL *PayloadFileHandle = NULL;

    /* Initialize the loader. */
    InitializeLib(ImageHandle, SystemTable);
    gImageHandle = ImageHandle;

    /* Disable the UEFI watchdog timer. The code '0x1FFFF' is a dummy
        value and doesn't actually do anything. Not a magic number. */
    ERRCHECK_UEFI(BS->SetWatchdogTimer, 4, 0, 0x1FFFF, 0, NULL);

    /* Load all necessary application drivers. */
    EnvironmentInitialize();

    PRINTLN(L"Welcome to the MFTAH loader!");
    PRINTLN(L"    Select your payload image to decrypt to get started.\r\n");

Label__PayloadSelectionMenu:
    FreePool(gOperatingPayload.Name);
    gOperatingPayload.Name = NULL;

    /* Detect available MFTAH payloads and select a file handle to open. */
    SelectPayload(&PayloadFileHandle,
                  &PayloadFileStartingPosition,
                  LoadedLoaderHash);

    /* Unlock/Decrypt the selected payload and load it into memory. */
    do {
        Status = GetPassword(Password, &PasswordLength);

        if (EFI_MENU_GO_BACK == Status) {
            PayloadFileStartingPosition = 0;
            PayloadFileHandle = NULL;

            DPRINTLN(L"Requested to return to payload selection.");
            goto Label__PayloadSelectionMenu;
        } else if (EFI_ERROR(Status)) {
            PANIC(L"Something went wrong while getting the password.");
        }

        /* Check the password against the payload blob. */
        Status = CheckPassword(PayloadFileHandle,
                               Password,
                               PasswordLength,
                               PasswordActual,
                               &PasswordLengthActual);
        if (EFI_INVALID_PASSWORD == Status) {
            EFI_WARNINGLN(L"-- Invalid password. Try again.");
            continue;
        } else if (EFI_ERROR(Status)) {
            PANIC(L"Some other error occurred when loading the chosen payload.");
        }

        /* Since the password is confirmed, attempt to load and decrypt the ramdisk. */
        Status = LoadAndDecrypt(PayloadFileHandle,
                                PasswordActual,
                                PasswordLengthActual,
                                LoadedPayloadHash);
        if (EFI_ERROR(Status) || NULL == gRamdiskImage || 0 == gRamdiskImageLength) {
            PANIC(L"Failed to load the chosen payload.");
        }

        /* Clear the password out of system memory and jump out of the loop. */
        SetMem(Password, sizeof(Password), 0x00);
        SetMem(PasswordActual, sizeof(PasswordActual), 0x00);
        break;
    } while (TRUE);

    /* All done with this, it's not needed here anymore. */
    FreePool(gOperatingPayload.Name);
    gOperatingPayload.Name = NULL;

    DPRINT(L"\r\n\r\n");

    /* Hint to the loaded OS where the boot ramdisk is in physical memory and its size. */
    Status = SetEfiVarsHints(&gRamdiskImage,
                             &gRamdiskImageLength,
                             NULL,
                             NULL);
#if MFTAH_ENSURE_HINTS == 1
    if (EFI_ERROR(Status)) {
        PANIC(L"Could not set related EFI variables as hints about the loaded ramdisk.");
    }
#endif

    /* Register the ramdisk device. */
    Status = WrapperRegisterRamdisk();
    if (EFI_ERROR(Status)) {
        PANIC(L"Failed to register the loaded ramdisk as a filesystem.");
    }

    /* Transfer bootloader control to it. */
    Status = JumpToRamdisk();
    if (EFI_ERROR(Status)) {
        PANIC(L"Failed to transfer control to the loaded ramdisk.");
    }

    /************************/
    /* The following code should never execute. */

    /* Await key-press event on STDIN. */
    PRINTLN(L"\r\n");
    EFI_WARNINGLN(
        L"No bootable device was found.\r\n"
        L"    This is likely because the MFTAH canary file (%s)\r\n"
        L"    is missing from the ramdisk filesystem root.\r\n",
        CanaryFileName
    );
    PRINTLN(L"Press any key to exit...");
    
    ERRCHECK_UEFI(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    while ((Status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key)) == EFI_NOT_READY);

    /* All done. */
    // unload_ramdisk(ImageHandle);  // At some point, this needs to be done once the OS is loaded.
    uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
    return EFI_SUCCESS;
}



STATIC
VOID
EFIAPI
EnvironmentInitialize()
{
    EFI_STATUS Status = EFI_SUCCESS;
    mftah_status_t MftahStatus = MFTAH_SUCCESS;

    /* Initial graphics setup. */
    Status = EntryGraphics();
    if (EFI_ERROR(Status)) {
        PANIC(L"Failed to initialize the graphics driver!");
    }

    /* Initialize drivers. */
    Status = AcpiLoadDriver();
    if (EFI_ERROR(Status) || NULL == gAcpiTableProtocol) {
        PANIC(L"Failed to load the ACPI Table Protocol!");
    }

    Status = RDEntryPoint(gImageHandle, gST);
    if (EFI_ERROR(Status) && EFI_ALREADY_STARTED != Status) {
        PANIC(L"Failed to load the RamDisk driver!");
    }

    // Status = InitializeThreading();
    if (EFI_ERROR(Status)) {
        EFI_WARNINGLN(L"Cannot initialize multiprocessing.\r\nOperations may take significantly longer to complete.");
    }

    DPRINTLN(L"Loading and registering a new MFTAH protocol instance.");
    MFTAH = (mftah_protocol_t *)AllocateZeroPool(sizeof(mftah_protocol_t));
    MftahStatus = mftah_protocol_factory__create(MFTAH);
    if (MFTAH_ERROR(MftahStatus)) {
        PANIC(L"Failed to create a MFTAH protocol instance.");
    }

    mftah_registration_details_t *hooks_registration
        = (mftah_registration_details_t *)AllocateZeroPool(sizeof(mftah_registration_details_t));

    hooks_registration->calloc  = MftahUefi__wrapper__AllocateZeroPool;
    hooks_registration->malloc  = MftahUefi__wrapper__AllocatePool;
    hooks_registration->realloc = MftahUefi__wrapper__ReallocatePool;
    hooks_registration->free    = MftahUefi__wrapper__FreePool;
    hooks_registration->memcmp  = MftahUefi__wrapper__CompareMem;
    hooks_registration->memcpy  = MftahUefi__wrapper__CopyMem;
    hooks_registration->memset  = MftahUefi__wrapper__SetMem;
    hooks_registration->memmove = MftahUefi__wrapper__MemMove;
    hooks_registration->printf  = MftahUefi__wrapper__Print;

    MftahStatus = MFTAH->register_hooks(MFTAH, hooks_registration);
    FreePool(hooks_registration);

    DPRINTLN(L"-- All done!\r\n");
}


static
EFI_STATUS
EFIAPI
GetPassword(OUT UINT8 *Password,
            OUT UINT8 *PassLen OPTIONAL)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINT8  PasswordLength = 0;

    CHAR16 *PasswordPrompt = NULL;
    PasswordPrompt = AllocateZeroPool(256 + 1);
    SPrint(
        PasswordPrompt,
        256,
        gOperatingPayload.FromMultiSelect
            ? L"Enter the password for '%s' ('q' to reboot; 'b' to go back): "
            : L"Enter the password for '%s' ('q' to reboot): ",
        gOperatingPayload.Name
    );

    do {
        /* Reset loop parameters and variables. */
        PasswordLength = 0;
        ZeroMem(Password, (MFTAH_MAX_PW_LEN + 1) * sizeof(CHAR16));

        /* Prompt for a password to decrypt the payload, */
        Status = ReadChar16KeyboardInput(PasswordPrompt,
                                         Password,
                                         &PasswordLength,
                                         TRUE,
                                         MFTAH_MAX_PW_LEN);
        if (EFI_ERROR(Status)) {
            PANIC(L"Error occurred while processing the input password.");
        }

        /* Empty passwords always default to the hard-coded 'default' password. */
        if (0 == PasswordLength) {
            DPRINTLN(L"-- No password was entered.");
            continue;
        } else if (0 == StrnCmp(Password, L"q\0", 2 * sizeof(CHAR16))) {
            Shutdown(EFI_SUCCESS);
        } else if (TRUE == gOperatingPayload.FromMultiSelect && 0 == StrnCmp(Password, L"b\0", 2 * sizeof(CHAR16))) {
            FreePool(PasswordPrompt);
            return EFI_MENU_GO_BACK;
        } else {
            DPRINTLN(L"-- Got password %u bytes long: '%s'.", PasswordLength, Password);
            MEMDUMP(Password, PasswordLength);
        }
        
        if (NULL != PassLen) {
            *PassLen = PasswordLength;
        }

        FreePool(PasswordPrompt);
        return EFI_SUCCESS;
    } while (TRUE);
}


static
EFI_STATUS
EFIAPI
CheckPassword(IN EFI_FILE_PROTOCOL *PayloadFileHandle,
              IN CONST UINT8 *PasswordBuffer,
              IN CONST UINT8 PassLen,
              OUT CHAR8 *PasswordActual,
              OUT UINT8 *PasswordLengthActual)
{
    EFI_STATUS Status = EFI_SUCCESS;
    mftah_status_t MftahStatus = MFTAH_SUCCESS;
    mftah_payload_t *LoadedPayload = NULL;

    CHAR8 *PasswordBufferChar8 = NULL;
    UINT8 PassLenChar8 = 0;

    DPRINTLN(L"MFTAH header size: %u", mftah_payload_header__sizeof());
    CONST UINT64 InitialReadLength = mftah_payload_header__sizeof() + AES_BLOCKLEN;
    UINTN InitialReadLengthShadow = InitialReadLength;

    UINT64 ReadFileSize = 0;

    UINT64 DataLength = 0;
    UINT8 *DataBuffer = (UINT8 *)AllocateZeroPool(InitialReadLength);

    /* Read the file's size and do some sanity checks. */
    DPRINTLN(L"-- Getting payload attributes...");
    ReadFileSize = FileSize(&PayloadFileHandle);

    DPRINTLN(L"---- Apparent payload size: %u bytes", ReadFileSize);
    if (ReadFileSize < InitialReadLength) {
        EFI_WARNINGLN(L"The target payload is not  a valid length.");
        FreePool(DataBuffer);
        return EFI_ABORTED;
    } else if (0 != (ReadFileSize % AES_BLOCKLEN)) {
        EFI_WARNINGLN(L"The MFTAH payload size is not aligned to the AES block size.");
        FreePool(DataBuffer);
        return EFI_ABORTED;
    }

    /* Allocate a buffer at the full file size.
        Load the payload header and the first block to test the password. */
    DPRINTLN(L"-- Allocating initial password check pool.");
    DataBuffer = (UINT8 *)AllocatePool(InitialReadLength);

    DPRINTLN(L"-- Reading payload file headers (%u bytes).", InitialReadLength);
    Status = uefi_call_wrapper(
        PayloadFileHandle->Read,
        3,
        PayloadFileHandle,
        &InitialReadLengthShadow,
        DataBuffer
    );
    if (EFI_ERROR(Status)) {
        goto Label__CheckPassword__End;
    } else if (InitialReadLengthShadow != InitialReadLength) {
        Status = EFI_END_OF_FILE;
        goto Label__CheckPassword__End;
    }

    DPRINTLN(L"-- Setting payload file position back to 0.");
    Status = uefi_call_wrapper(
        PayloadFileHandle->SetPosition,
        2,
        PayloadFileHandle,
        0
    );
    if (EFI_ERROR(Status)) {
        goto Label__CheckPassword__End;
    }

    /* Now that the data is loaded into the buffer, create the payload object. */
    DPRINTLN(L"-- Creating a minimal MFTAH payload of %u bytes.", mftah_payload__sizeof());
    LoadedPayload = (mftah_payload_t *)AllocateZeroPool(mftah_payload__sizeof());

    DPRINTLN(L"-- Invoking MFTAH 'create_payload' method.");
    MftahStatus = MFTAH->create_payload(MFTAH,
                                      DataBuffer,
                                      InitialReadLength,
                                      LoadedPayload,
                                      NULL);
    if (MFTAH_ERROR(MftahStatus)) {
        EFI_WARNINGLN(L"Loading the MFTAH payload failed with code '%u'.", MftahStatus);
        Status = EFI_ABORTED;
        goto Label__CheckPassword__End;
    }

    /* Check the password. We assume the created payload is indeed an encrypted MFTAH file here.
        If it's not, then checks will just return garbage/failures. */
    DPRINTLN(L"-- Checking the password '%s'.", PasswordBuffer);

    DPRINTLN(L"---- Creating CHAR8 password buffer.");
    PassLenChar8 = PassLen / sizeof(CHAR16);
    PasswordBufferChar8 = (CHAR8 *)AllocateZeroPool(PassLenChar8 + 1);
    for (UINTN i = 0; i < PassLenChar8; ++i) {
        PasswordBufferChar8[i] = (CHAR8)PasswordBuffer[i * sizeof(CHAR16)];
    }

    MftahStatus = MFTAH->check_password(MFTAH,
                                      LoadedPayload,
                                      PasswordBufferChar8,
                                      PassLenChar8,
                                      MFTAH_CRYPT_HOOK_DEFAULT,
                                      NULL);
    switch (MftahStatus) {
        case MFTAH_SUCCESS:
            PRINTLN(L"Payload decrypted.");
            Status = EFI_SUCCESS;

            CopyMem(PasswordActual, PasswordBufferChar8, PassLenChar8);
            *PasswordLengthActual = PassLenChar8;

            break;
        case MFTAH_INVALID_PASSWORD:
            Status = EFI_INVALID_PASSWORD;
            break;
        default:
            Status = EFI_ABORTED;
            break;
    }

Label__CheckPassword__End:
    FreePool(PasswordBufferChar8);
    FreePool(DataBuffer);
    FreePool(LoadedPayload);

    return Status;
}


static
EFI_STATUS
EFIAPI
LoadAndDecrypt(IN EFI_FILE_PROTOCOL *PayloadFileHandle,
               IN CONST UINT8 *Password,
               IN CONST UINT8 PasswordLength,
               OUT UINT8 *LoadedPayloadHash)
{
    EFI_STATUS Status = EFI_SUCCESS;
    mftah_status_t MftahStatus = MFTAH_SUCCESS;

    UINT64 ReadFileSize = 0;
    UINT8 *ReadBuffer = NULL;
    mftah_payload_t *LoadedPayload = NULL;
    VOID *PayloadBufferBase = NULL;

    /* Read the file's size and do some sanity checks. */
    DPRINTLN(L"-- Getting payload attributes...");
    ReadFileSize = FileSize(&PayloadFileHandle);
    DPRINTLN(L"---- Apparent payload size: %d bytes", ReadFileSize);
    if (0 != (ReadFileSize % AES_BLOCKLEN)) {
        EFI_WARNINGLN(L"The MFTAH payload size is not aligned to the AES block size.");
        return EFI_ABORTED;
    }

    /* At this point, the file handle and password are valid. Load the whole encrypted ramdisk. */
    /* NOTE: This segment should be loaded as type 'Runtime Services Data' so the
        registered ramdisk and GRUB doesn't discard itself after ExitBootServices(). */
    DPRINTLN(L"-- Allocating buffer of %d bytes.", ReadFileSize);
    Status = uefi_call_wrapper(
        BS->AllocatePool,
        3,
        EfiReservedMemoryType,
        ReadFileSize,
        (VOID **)&ReadBuffer
    );
    if (EFI_OUT_OF_RESOURCES == Status) {
        PANIC(L"Not enough free memory available to allocate the ramdisk.");
    } else if (NULL == ReadBuffer) {
        PANIC(L"Ramdisk allocation returned a null pointer.");
    } else if (EFI_ERROR(Status)) {
        PANIC(L"An unknown error occurred while allocating the ramdisk.");
    }

    /* This operation probably doesn't need to be threaded, so it's not.
        Future optimizations might look into it. */
    PRINTLN(L"\r\n-- Reading payload into memory at '%p'...", ReadBuffer);
    DPRINTLN(L"---- Copying payload of size '0x%08llx' bytes into RAM at '%p'...", ReadFileSize, ReadBuffer);

    UINT64 FileChunkSize = MFTAH_RAMDISK_LOAD_BLOCK_SIZE;
    for (UINT64 i = 0; i < ReadFileSize; i += MFTAH_RAMDISK_LOAD_BLOCK_SIZE) {
        FileChunkSize = MFTAH_RAMDISK_LOAD_BLOCK_SIZE;

        Status = uefi_call_wrapper(
            PayloadFileHandle->Read,
            3,
            PayloadFileHandle,
            &FileChunkSize,
            (ReadBuffer + i)
        );

        /* Watch for errors reading the file handle. */
        if (EFI_ERROR(Status)) {
            FreePool(ReadBuffer);
            return Status;
        }

        /* Only print the progress every so often. */
        if (0 == (i % (1 << 24))) {
            PrintProgress(&i, &ReadFileSize, NULL);
        }
    }

    /* Guarantee this prints out a final 100%. */
    PrintProgress(&ReadFileSize, &ReadFileSize, NULL);
    PRINTLN(L"\r\n");

    /* Hash the loaded payload image for later. */
    PRINTLN(L"\r\n-- Generating a loaded payload hash.");
    MftahStatus = MFTAH->create_hash(MFTAH,
                                   ReadBuffer,
                                   ReadFileSize,
                                   LoadedPayloadHash,
                                   NULL);
    if (MFTAH_ERROR(MftahStatus)) {
        EFI_WARNINGLN(L"Failed to hash the loaded payload buffer...");
        EFI_WARNINGLN(L"    The '__MFTAH_PAYLOAD_HASH' EFI variable will not");
        EFI_WARNINGLN(L"    be available at OS runtime.");
    }

    /* Create the payload object. */
    DPRINTLN(L"-- Creating a full-size MFTAH payload object describing %u bytes.", ReadFileSize);
    LoadedPayload = (mftah_payload_t *)AllocateZeroPool(mftah_payload__sizeof());

    DPRINTLN(L"-- Creating a MFTAH payload.");
    MftahStatus = MFTAH->create_payload(MFTAH,
                                      ReadBuffer,
                                      ReadFileSize,
                                      LoadedPayload,
                                      NULL);
    if (MFTAH_ERROR(MftahStatus)) {
        EFI_WARNINGLN(L"Loading the MFTAH payload failed with code '%u'.", MftahStatus);
        FreePool(ReadBuffer);
        return EFI_ABORTED;
    }

    /* Decrypt the ramdisk. We assume the created payload is indeed an encrypted MFTAH file here.
        If it's not, then decryption will just return garbage or invalid responses and that's the
        user's fault/ordeal. */
    PRINTLN(L"-- Decrypting the ramdisk...");
    PRINTLN(L"---- This may take a few minutes.");
    PRINTLN(L"---- If you booted from external media, you may disconnect it now.");
    MftahStatus = MFTAH->decrypt(MFTAH,
                               LoadedPayload,
                               Password,
                               PasswordLength,
                               SpawnDecryptionWorker,
                               UefiSpin);
    if (MFTAH_ERROR(MftahStatus)) {
        EFI_WARNINGLN(L"Decrypting the MFTAH payload failed with code '%u'.", MftahStatus);
        FreePool(ReadBuffer);
        return EFI_ABORTED;
    }

    /* The 'length' field of the header is a certain offset into the base of the decrypted payload. */
    MftahStatus = MFTAH->get_buffer_base(MFTAH, LoadedPayload, &PayloadBufferBase);
    if (MFTAH_ERROR(MftahStatus) || NULL == PayloadBufferBase) {
        EFI_WARNINGLN(L"Getting the payload buffer base failed with code '%u'.", MftahStatus);
        FreePool(ReadBuffer);
        return EFI_ABORTED;
    }

    /* The actual ramdisk starts after the payload header (buffer base). */
    gRamdiskImage = (UINT8 *)PayloadBufferBase + mftah_payload_header__sizeof();
    /* The payload length is at a particular offset into the payload header. */
    gRamdiskImageLength = *((UINT64 *)((UINT8 *)PayloadBufferBase + 96));   /* TODO: No magic numbers pls & ty */

    return EFI_SUCCESS;
}


STATIC
EFI_STATUS
EFIAPI
SetEfiVarsHints(IN UINT8 **RamdiskMemoryLocation,
                IN UINT64 *RamdiskLength,
                IN UINT8 *LoadedLoaderHash,
                IN UINT8 *LoadedPayloadHash)
{
    EFI_STATUS Status = EFI_SUCCESS;

    PRINTLN(L"-- Setting memory address device hint '__MFTAH_RDBASE'.");
    ERRCHECK_UEFI(
        ST->RuntimeServices->SetVariable,
        5,
        L"__MFTAH_RDBASE",
        &gXmitVendorGuid,
        EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
        sizeof(VOID *),
        RamdiskMemoryLocation   /* passing (VOID **) here because we WANT a (VOID *) stored... */
    );

    PRINTLN(L"-- Setting ramdisk size hint '__MFTAH_RDSIZE'.");
    ERRCHECK_UEFI(
        ST->RuntimeServices->SetVariable,
        5,
        L"__MFTAH_RDSIZE",
        &gXmitVendorGuid,
        EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
        sizeof(UINT64),
        RamdiskLength
    );

    PRINTLN(L"-- Setting MFTAH loader hash '__MFTAH_LOADER_HASH`.");

    VOID *LoaderHashLocationInMemory = NULL;
    ERRCHECK_UEFI(
        BS->AllocatePool,
        3,
        EfiReservedMemoryType,
        SIZE_OF_SHA_256_HASH,
        &LoaderHashLocationInMemory
    );
    if (NULL == LoaderHashLocationInMemory) {
        return EFI_OUT_OF_RESOURCES;
    }

    CopyMem(LoaderHashLocationInMemory, LoadedLoaderHash, SIZE_OF_SHA_256_HASH);

    ERRCHECK_UEFI(
        ST->RuntimeServices->SetVariable,
        5,
        L"__MFTAH_LOADER_HASH",
        &gXmitVendorGuid,
        EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
        sizeof(VOID *),
        &LoaderHashLocationInMemory
    );

    PRINTLN(L"-- Setting selected payload hash '__MFTAH_PAYLOAD_HASH`.");

    VOID *PayloadHashLocationInMemory = NULL;
    ERRCHECK_UEFI(
        BS->AllocatePool,
        3,
        EfiReservedMemoryType,
        SIZE_OF_SHA_256_HASH,
        &PayloadHashLocationInMemory
    );
    if (NULL == PayloadHashLocationInMemory) {
        return EFI_OUT_OF_RESOURCES;
    }

    CopyMem(PayloadHashLocationInMemory, LoadedPayloadHash, SIZE_OF_SHA_256_HASH);

    ERRCHECK_UEFI(
        ST->RuntimeServices->SetVariable,
        5,
        L"__MFTAH_PAYLOAD_HASH",
        &gXmitVendorGuid,
        EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
        sizeof(VOID *),
        &PayloadHashLocationInMemory
    );

    return EFI_SUCCESS;
}


static
EFI_STATUS
EFIAPI
WrapperRegisterRamdisk()
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_RAM_DISK_PROTOCOL *RamdiskProtocol;
    EFI_DEVICE_PATH *RamdiskDevicePath;

    PRINTLN(L"Registering the loaded ramdisk as a filesystem...");

    DPRINTLN(L"-- Opening RamDisk protocol handle.");
    ERRCHECK_UEFI(
        BS->HandleProtocol,
        3,
        gImageHandle,
        &gEfiRamdiskGuid,
        (VOID **)&RamdiskProtocol
    );

    DPRINTLN(L"-- Using protocol to register.");
    ERRCHECK(
        RamdiskProtocol->Register((UINT64)gRamdiskImage,
                                  gRamdiskImageLength,
                                  &gEfiRamdiskVirtualDiskGuid,
                                  NULL,
                                  &RamdiskDevicePath)
    );

    DPRINTLN(L"\r-- Registered ramdisk at '%p' with size '%d'.", gRamdiskImage, gRamdiskImageLength);
    DPRINTLN(L"-- RamDisk Device Path Handle is at '%016x' (proto %016x).", *RamdiskDevicePath, RamdiskProtocol);

    CHAR16* DevPathAsStr = DevicePathToStr(RamdiskDevicePath);
    DPRINTLN(L"-- Device Path: '%s'", DevPathAsStr);
    FreePool(DevPathAsStr);

    DPRINTLN(L"-- All done!");
    return EFI_SUCCESS;
}


static
EFI_STATUS
EFIAPI
JumpToRamdisk()
{
    EFI_STATUS Status;
    EFI_HANDLE *handles, *block_handles;
    UINTN HandleCount;

    ERRCHECK_UEFI(
        BS->LocateHandleBuffer,
        5,
        ByProtocol,
        &gEfiBlockIoProtocolGuid,
        NULL,
        &HandleCount,
        &block_handles
    );

    DPRINTLN(L"There are '%d' Block I/O handles loaded...", HandleCount);
    for (UINTN i = 0; i < HandleCount; ++i) {
        DPRINTLN(L"---> Block I/O Handle: %p", block_handles[i]);
        DPRINTLN(L"-----> Media ID: %08x", ((EFI_BLOCK_IO *)(block_handles[i]))->Media->MediaId);
    }

    ERRCHECK_UEFI(
        BS->LocateHandleBuffer,
        5,
        ByProtocol,
        &gEfiDiskIoProtocolGuid,
        NULL,
        &HandleCount,
        &block_handles
    );

    DPRINTLN(L"There are '%d' Disk I/O handles loaded...", HandleCount);
    for (UINTN i = 0; i < HandleCount; ++i) {
        DPRINTLN(L"---> Disk I/O Handle: %p", block_handles[i]);
    }

    ERRCHECK_UEFI(
        BS->LocateHandleBuffer,
        5,
        ByProtocol,
        &gEfiSimpleFileSystemProtocolGuid,
        NULL,
        &HandleCount,
        &handles
    );

    DPRINTLN(L"Found '%d' filesystem handles.", HandleCount);

    for (UINTN i = 0; i < HandleCount; ++i) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
        EFI_FILE_PROTOCOL *root = NULL, *token = NULL;

        DPRINTLN(L"Get proto handle for SFS");
        ERRCHECK_UEFI(
            BS->HandleProtocol,
            3,
            handles[i],
            &gEfiSimpleFileSystemProtocolGuid,
            (VOID **)&fs
        );

        DPRINTLN(L"Open SFS volume");
        ERRCHECK_UEFI(
            fs->OpenVolume,
            2,
            fs,
            &root
        );

        Status = uefi_call_wrapper(
                root->Open,
                5,
                root,
                &token,
                (CHAR16 *)CanaryFileName,
                EFI_FILE_MODE_READ,
                EFI_FILE_READ_ONLY | EFI_FILE_ARCHIVE | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM
        );
        if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
            return Status;
        } else if (EFI_SUCCESS == Status) {
            DPRINTLN(L"MFTAH canary file found! Loading boot executable.");
            EFI_DEVICE_PATH *FullFilePath = FileDevicePath(*(handles + i), MFTAH_CHAINLOAD_TARGET_PATH);

            DPRINTLN(L"Device Path: %s", DevicePathToStr(FullFilePath));

            EFI_HANDLE loadedImage = NULL;
            ERRCHECK_UEFI(
                BS->LoadImage,
                6,
                FALSE,
                gImageHandle,
                FullFilePath,
                NULL,
                0,
                &loadedImage
            );

            EFI_LOADED_IMAGE_PROTOCOL *loadedImageProtocol = NULL;
            ERRCHECK_UEFI(
                BS->HandleProtocol,
                3,
                loadedImage,
                &gEfiLoadedImageProtocolGuid,
                (VOID **)&loadedImageProtocol
            );

            DPRINTLN(L"Boot image loaded!");
            MEMDUMP(loadedImageProtocol, sizeof(EFI_LOADED_IMAGE_PROTOCOL));
            DPRINT(L"\r\n\r\n");

            DPRINTLN(L"Booting...");
            ERRCHECK_UEFI(
                BS->StartImage,
                3,
                loadedImage,
                NULL,
                NULL
            );
        }

        uefi_call_wrapper(root->Close, 1, root);
    }

    return EFI_SUCCESS;
}
