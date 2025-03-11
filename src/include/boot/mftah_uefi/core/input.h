#ifndef MFTAH_INPUT_H
#define MFTAH_INPUT_H

#include "core/mftah_uefi.h"



/**
 * A generic method call to read a newline-terminated keyboard input line.
 * 
 * @param[in]  Prompt  A message to display when collecting the user's input.
 * @param[out] InputBuffer  A pointer to a CHAR16 array or buffer which can store up to MaxLength characters.
 * @param[out] ResultingBufferLength  A point to a UINT8 whose value is changed to the returned length of the entered string.
 * @param[in]  IsSecret  Whether the user's input should be kept hidden or not. This is usually set to TRUE for password inputs.
 * @param[in]  MaxLength  The maximum length of the accepted input string. The InputBuffer param should be capable of accepting this many characters.
 * 
 * @returns Whether the operation encountered an irrecoverable error.
 */
EFI_STATUS
EFIAPI
ReadChar16KeyboardInput(
    IN CONST CHAR16 *Prompt,
    OUT CHAR16 *InputBuffer,
    OUT UINT8 *ResultingBufferLength,
    IN BOOLEAN IsSecret,
    IN UINT8 MaxLength
);


/**
 * Confirm or deny a request to perform an action with a binary yes/no choice.
 * 
 * @param[in]  Prompt  The prompt to issue to the user. DO NOT include the "[Y/n]" markings here, as this method will append them.
 * @param[in]  IsDefaultConfirm  Whether the default action on pressing the ENTER key is to issue confirmation.
 * 
 * @retval  EFI_SUCCESS  The confirmation has been accepted by the user.
 * @retval  EFI_ERROR(any_type)  The confirmation has been denied by the user.
 */
EFI_STATUS
EFIAPI
Confirm(
    IN CONST CHAR16 *Prompt,
    IN CONST BOOLEAN IsDefaultConfirm
);



#endif   /* MFTAH_INPUT_H */
