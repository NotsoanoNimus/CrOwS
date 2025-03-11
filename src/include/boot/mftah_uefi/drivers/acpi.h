#ifndef MFTAH_ACPI_H
#define MFTAH_ACPI_H


#include "gnu-efi/inc/efi.h"
#include "gnu-efi/inc/efilib.h"


#define EFI_ACPI_TABLE_PROTOCOL_GUID \
    { 0xffe06bdd, 0x6107, 0x46a6, { 0x7b, 0xb2, 0x5a, 0x9c, 0x7e, 0xc5, 0x27, 0x5c } }

#define EFI_ACPI_NFIT_SIGNATURE   EFI_SIGNATURE_32('N', 'F', 'I', 'T')

#define EFI_ACPI_6_4_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE_TYPE 0
#define EFI_ACPI_6_4_NFIT_REGION_MAPPING_STRUCTURE_TYPE 1
#define EFI_ACPI_6_4_NVDIMM_FIRMWARE_INTERFACE_TABLE_REVISION 1


typedef UINT32 EFI_ACPI_TABLE_HEADER;
typedef UINT32 EFI_ACPI_TABLE_VERSION;
typedef VOID * EFI_ACPI_HANDLE;
typedef UINT32 EFI_ACPI_DATA_TYPE;


/* ACPI Common Description Table Header */
typedef
struct _EFI_ACPI_DESCRIPTION_HEADER {
    UINT32   Signature;
    UINT32   Length;
    UINT8    Revision;
    UINT8    Checksum;
    UINT8    OemId[6];
    UINT64   OemTableId;
    UINT32   OemRevision;
    UINT32   CreatorId;
    UINT32   CreatorRevision;
} __attribute__((packed)) EFI_ACPI_DESCRIPTION_HEADER;
/*************************************/


/* ACPI Table Protocol Declarations & Types */
typedef
struct S_EFI_ACPI_TABLE_PROTOCOL
EFI_ACPI_TABLE_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_ACPI_TABLE_INSTALL_ACPI_TABLE)(
    IN   EFI_ACPI_TABLE_PROTOCOL       *This,
    IN   VOID                          *AcpiTableBuffer,
    IN   UINTN                         AcpiTableBufferSize,
    OUT  UINTN                         *TableKey
);

typedef
EFI_STATUS
(EFIAPI *EFI_ACPI_TABLE_UNINSTALL_ACPI_TABLE)(
    IN  EFI_ACPI_TABLE_PROTOCOL       *This,
    IN  UINTN                         TableKey
);

struct S_EFI_ACPI_TABLE_PROTOCOL {
    EFI_ACPI_TABLE_INSTALL_ACPI_TABLE      InstallAcpiTable;
    EFI_ACPI_TABLE_UNINSTALL_ACPI_TABLE    UninstallAcpiTable;
};
/*************************************/


/*** NFIT Declarations & Types ***/
typedef
struct _EFI_ACPI_6_4_NVDIMM_FIRMWARE_INTERFACE_TABLE {
    EFI_ACPI_DESCRIPTION_HEADER Header;
    UINT32 Reserved;
} __attribute__((packed)) EFI_ACPI_6_4_NVDIMM_FIRMWARE_INTERFACE_TABLE;

typedef
struct _EFI_ACPI_6_4_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE {
    UINT16   Type;
    UINT16   Length;
    UINT16   SPARangeStructureIndex;
    UINT16   Flags;
    UINT32   Reserved_8;
    UINT32   ProximityDomain;
    EFI_GUID AddressRangeTypeGUID;
    UINT64   SystemPhysicalAddressRangeBase;
    UINT64   SystemPhysicalAddressRangeLength;
    UINT64   AddressRangeMemoryMappingAttribute;
} __attribute__((packed)) EFI_ACPI_6_4_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE;
/*************************************/


/* ACPI Table Protocol implementation. */
extern EFI_GUID gEfiAcpiTableProtocolGuid;
extern EFI_ACPI_TABLE_PROTOCOL *gAcpiTableProtocol;


/**
 * Load the custom implementation of the ACPI Table protocol if
 *  the firmware does not provide one.
 * 
 * @retval EFI_SUCCESS    The ACPI driver was successfully initialized.
 * @retval EFI_NOT_FOUND  The ACPI Table protocol driver could not
 *                         be loaded for some reason.
 */
EFI_STATUS
EFIAPI
AcpiLoadDriver();


/**
 * Check for ACPI Table protocol support from the firmware.
 * 
 * @retval EFI_SUCCESS     The ACPI Table protocol was found and placed
 *                          in the global 'gAcpiTableProtocol' handle.
 * @retval EFI_NOT_FOUND   The protocol is not supported by the firmware.
 * 
 */
EFI_STATUS
EFIAPI
AcpiProtocolCheck();



#endif   /* MFTAH_ACPI_H */
