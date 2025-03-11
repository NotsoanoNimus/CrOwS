#ifndef MFTAH_GRAPHICS_H
#define MFTAH_GRAPHICS_H

#include "core/mftah_uefi.h"



/**
 * Initializes an appropriate graphics mode for the application, if configured.
 * Defaults to an ASCII-only (console) mode if MFTAH_FULL_GRAPHICS is not defined.
 * 
 * @returns Whether initialization succeeded or encountered a fatal error.
 */
EFI_STATUS
EFIAPI
EntryGraphics();


/* Beautiful (and totally not stolen) ASCII art. */
STATIC CONST
CHAR16 *
MftahAsciiArt =
    L"\r\n"
    L"\r\n"
    L"        M. F. T. A. H.                     ████████\r\n"
    L"                                        ███        ████\r\n"
    L"     ███████████████████████████████████               ██\r\n"
    L"    ██                                                   █\r\n"
    L"   ██              █                                      █\r\n"
    L"  ██               █  █ █   █                              █\r\n"
    L"   ██              █                               ███     █\r\n"
    L"     █████████████ █   █   ███   ███               ███     █\r\n"
    L"            █████  █   █  █   █ █   █                      █\r\n"
    L"         █████     ████████████████████                  ██\r\n"
    L"       ██                              ███             ██\r\n"
    L"        ██                   ████         ████      ███\r\n"
    L"          ███████████████████                 ██████\r\n"
    L"\r\n"
    L"\r\n"
;


#endif   /* MFTAH_GRAPHICS_H */
