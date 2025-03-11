#include "core/input.h"



EFI_STATUS
EFIAPI
ReadChar16KeyboardInput(IN CONST CHAR16 *Prompt,
                        OUT CHAR16 *InputBuffer,
                        OUT UINT8 *ResultingBufferLength,
                        IN BOOLEAN IsSecret,
                        IN UINT8 MaxLength)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_INPUT_KEY InputKey;
    UINTN KeyEvent;
    UINT8 InputLength = 0;

    ERRCHECK_UEFI(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    SetMem(InputBuffer, sizeof(CHAR16) * (MaxLength + 1), 0);

    do {
        PRINT(L"\r"); PRINT(Prompt);
        for (UINT8 p = 0; p < InputLength; ++p) {
            if (TRUE == IsSecret) {
                PRINT(L"*");
            } else {
                PRINT(L"%c", InputBuffer[p]);
            }
        }
        for (UINT8 p = (InputLength + 1); p > 0; p--) {
            PRINT(L" ");
        }

        ERRCHECK_UEFI(BS->WaitForEvent, 3, 1, &(ST->ConIn->WaitForKey), &KeyEvent);

        Status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &InputKey);
        if (EFI_NOT_READY == Status) {
            /* No keystroke data is available. Move on. */
            continue;
        } else if (EFI_DEVICE_ERROR == Status) {
            /* Keyboard hardware issue detected. */
            PANIC(L"Keyboard hardware fault detected.");
        } else if (EFI_UNSUPPORTED == Status) {
            PANIC(L"Unable to read input from this keyboard.");
        } else if (EFI_ERROR(Status)) {
            return Status;
        }

        ERRCHECK_UEFI(ST->ConIn->Reset, 2, ST->ConIn, FALSE);

        if (L'\x0A' == InputKey.UnicodeChar || L'\x0D' == InputKey.UnicodeChar) {
            break;
        } else if (L'\x08' == InputKey.UnicodeChar) {
            InputBuffer[InputLength] = 0;
            if (InputLength > 0) {
                InputLength--;
            }
        } else if (L'\x20' <= InputKey.UnicodeChar
                    && InputKey.UnicodeChar <= L'\x7E'
                    && InputLength < MaxLength) {
            InputBuffer[InputLength] = InputKey.UnicodeChar;
            InputLength++;
        }
    } while (TRUE);

    InputBuffer[MaxLength] = '\0';
    *ResultingBufferLength = InputLength * sizeof(CHAR16);

    PRINTLN(L"");
    return Status;
}


EFI_STATUS
EFIAPI
Confirm(IN CONST CHAR16 *Prompt,
        IN CONST BOOLEAN IsDefaultConfirm)
{
    CHAR16 *FullPrompt = NULL;
    CHAR16 *ConfirmationIndicators = NULL;

    CHAR16 UserInput[2] = {0};
    UINT8 UserInputLength = 0;

    if (IsDefaultConfirm) {
        ConfirmationIndicators = L" [Y/n]  ";
    } else {
        ConfirmationIndicators = L" [y/N]  ";
    }

    FullPrompt = StrDuplicate(Prompt);
    StrCat(FullPrompt, ConfirmationIndicators);

    ReadChar16KeyboardInput(FullPrompt, UserInput, &UserInputLength, FALSE, 1);

    FreePool(FullPrompt);

    DPRINTLN(L"Read user input: '%s'", UserInput);

    if (L'Y' == UserInput[0] || L'y' == UserInput[0]) {
        DPRINTLN(L"Explicit 'Y' response.");
        return EFI_SUCCESS;
    } else if (L'N' == UserInput[0] || L'n' == UserInput[0]) {
        DPRINTLN(L"Explicit 'N' response.");
        return EFI_ABORTED;
    } else if (L'\0' == UserInput[0]) {
        DPRINTLN(L"Implicit response left to default.");
        return IsDefaultConfirm ? EFI_SUCCESS : EFI_ABORTED;
    } else {
        DPRINTLN(L"Implicit FALSE: non-empty and misunderstood character.");
        return EFI_ABORTED;
    }
}