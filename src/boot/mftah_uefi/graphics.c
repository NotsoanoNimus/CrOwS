#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"

#include "core/graphics.h"


static
EFI_STATUS
EFIAPI
DetectGraphicsSupport(EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsProtocol)
{
    EFI_STATUS Status;

    /* First, get the graphics protocol, if one exists. */
    ERRCHECK(
        uefi_call_wrapper(
            BS->LocateProtocol,
            3,
            &gEfiGraphicsOutputProtocolGuid,
            NULL,
            (VOID **)&GraphicsProtocol
        )
    );

    if (NULL == GraphicsProtocol) {
        return EFI_NOT_FOUND;
    }

    /* Then, select the video mode with the highest combined resolution,
          with a maximum X*Y limit at 1080p. */

    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
EntryGraphics()
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsProtocol;

    /* Set up the basic screen. */
    ST->ConOut->SetAttribute(ST->ConOut, MFTAH_COLOR_DEFAULT);
    ST->ConOut->EnableCursor(ST->ConOut, FALSE);
    ST->ConOut->ClearScreen(ST->ConOut);
    ST->ConOut->SetCursorPosition(ST->ConOut, 0, 0);

    /* Without valid GOP support, just output some ASCII art and be done. */
#ifdef MFTAH_FULL_GRAPHICS
    if (EFI_SUCCESS != DetectGraphicsSupport(GraphicsProtocol)) {
#endif
        ERRCHECK_UEFI(ST->ConOut->SetAttribute, 2, ST->ConOut, MFTAH_COLOR_ASCII_ART);
        PRINT(MftahAsciiArt);
        ERRCHECK_UEFI(ST->ConOut->SetAttribute, 2, ST->ConOut, MFTAH_COLOR_DEFAULT);

        return EFI_SUCCESS;
#ifdef MFTAH_FULL_GRAPHICS
    }

    /* Otherwise, we can draw some kewl graphics since the preferred video mode is set,. */

    return EFI_SUCCESS;
#endif
}



#pragma clang diagnostic pop
