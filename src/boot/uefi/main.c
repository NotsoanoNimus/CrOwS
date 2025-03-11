/**
 * @author Zack Puhl <zack@crows.dev>
 * @date   December 2024
 * 
 * Primary UEFI loader used to securely boot the CrOwS operating system. It should
 *  be EXPLICITLY NOTED that MFTAH-UEFI is the REQUIRED bootstrap loader for this
 *  image, and that the chainloaded image should really be MFTAH-encapsulated if you
 *  care at all about security (which since you're here I hope you do).
 * 
 * Against perhaps better judgement, I've decided to use the TCG/TPM protocols
 *  in order to secure the boot process. This works through a TOTP-based "handshake"
 *  from the MFTAH-UEFI loader to this shim, then back to another MFTAH-UEFI loader
 *  embedded within the boot filesystem that can bootstrap the CrOwS kernel.
 * 
 * To make TOTP work automatically and safely, the key from which codes are derived
 *  is generated at OS compile-time and exists only within the ramdisk tmpfs (until
 *  the kernel securely wipes it shortly hereafter).
 * 
 * The CrOwS kernel image is signed with an asymmetric key at compile-time, whose
 *  public-key component is also contained within the ramdisk FAT filesystem.
 */

#include "../../../include/boot/uefi/uefi.h"



EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle,
         EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status = EFI_SUCCESS;

    /* Initialize the loader. */
    InitializeLib(ImageHandle, SystemTable);

    /* For now just print some dummy text and vibe for like 60 seconds.
        This is until there is confirmation that the first-stage MFTAH
        loader will successfully chain-boot this executable. */
    Print(L"Chainload success!\n\n");
    BS->Stall(60000000);

    EFI_LOADED_IMAGE_PROTOCOL *LIP = NULL;
    Status = BS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, &LIP);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get the LIP handle.\n");
        BS->Stall(10000000);

        Print(L"Resetting...");
        RT->ResetSystem(EfiResetCold, Status, 0, NULL);
        while (TRUE);   /* should not be reached, but hang just in case */
    }

    LIP->

    return EFI_SUCCESS;
}
