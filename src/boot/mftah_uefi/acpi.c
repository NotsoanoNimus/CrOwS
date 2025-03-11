#include "drivers/acpi.h"



EFI_GUID gEfiAcpiTableProtocolGuid = EFI_ACPI_TABLE_PROTOCOL_GUID;

EFI_ACPI_TABLE_PROTOCOL *gAcpiTableProtocol = NULL;


/******************************************
 * Begin PUBLIC function implementations. *
 ******************************************/
EFI_STATUS
EFIAPI
AcpiLoadDriver()
{
    EFI_STATUS Status;

    Status = AcpiProtocolCheck();

    return (NULL == gAcpiTableProtocol)
        ? EFI_NOT_FOUND
        : EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
AcpiProtocolCheck()
{
    EFI_STATUS Status;

    /* Attempt to locate the EFI_ACPI_TABLE_PROTOCOL. */
    Status = uefi_call_wrapper(
        BS->LocateProtocol,
        3,
        &gEfiAcpiTableProtocolGuid,
        NULL,
        (VOID **)&gAcpiTableProtocol
    );

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return (NULL == gAcpiTableProtocol) ? EFI_NOT_FOUND : Status;
}