#ifndef MFTAH_WRAPPERS_H
#define MFTAH_WRAPPERS_H

#include "../core/mftah_uefi.h"



VOID *
MftahUefi__wrapper__AllocateZeroPool(
    __SIZE_TYPE__ Count,
    __SIZE_TYPE__ Length
);

VOID *
MftahUefi__wrapper__AllocatePool(
    __SIZE_TYPE__ Length
);

VOID
MftahUefi__wrapper__FreePool(
    VOID *Pointer
);

VOID *
MftahUefi__wrapper__MemMove(
    VOID *Destination,
    CONST VOID *Source,
    __SIZE_TYPE__ Length)
;

VOID *
MftahUefi__wrapper__SetMem(
    VOID *Destination,
    INT32 Value,
    __SIZE_TYPE__ Size
);

VOID *
MftahUefi__wrapper__CopyMem(
    VOID *Destination,
    CONST VOID *Source,
    __SIZE_TYPE__ Size
);

INT32
MftahUefi__wrapper__CompareMem(
    CONST VOID *Left,
    CONST VOID *Right,
    __SIZE_TYPE__ Length
);

VOID *
MftahUefi__wrapper__ReallocatePool(
    VOID *At,
    __SIZE_TYPE__ ToSize
);

VOID
MftahUefi__wrapper__Print(
    mftah_log_level_t Level,
    CONST CHAR *Format,
    ...
);



#endif   /* MFTAH_WRAPPERS_H */
