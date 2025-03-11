/**
 * Adapter methods for standard library functions used as hooks
 *   for the loaded MFTAH protocol.
 */



VOID *
MftahUefi__wrapper__AllocateZeroPool(__SIZE_TYPE__ Count,
                                    __SIZE_TYPE__ Length)
{
    return AllocateZeroPool(Count * Length);
}


VOID *
MftahUefi__wrapper__AllocatePool(__SIZE_TYPE__ Length)
{
    return AllocatePool(Length);
}


VOID
MftahUefi__wrapper__FreePool(VOID *Pointer)
{
    FreePool(Pointer);
}


VOID *
MftahUefi__wrapper__MemMove(VOID *Destination,
                           CONST VOID *Source,
                           __SIZE_TYPE__ Length)
{
    UINT8 SourceCopy = 0;

    /* Not the most performant operation but it works OK. */
    for (UINTN i = 0; i < Length; ++i) {
        SourceCopy = *((UINT8 *)Source++);
        *((UINT8 *)Destination++) = SourceCopy;
    }

    return Destination;
}


VOID *
MftahUefi__wrapper__SetMem(VOID *Destination,
                          INT32 Value,
                          __SIZE_TYPE__ Size)
{
    SetMem(Destination, Size, (UINT8)Value);

    return NULL;
}


VOID *
MftahUefi__wrapper__CopyMem(VOID *Destination,
                           CONST VOID *Source,
                           __SIZE_TYPE__ Size)
{
    CopyMem(Destination, (VOID *)Source, Size);

    return NULL;
}


INT32
MftahUefi__wrapper__CompareMem(CONST VOID *Left,
                              CONST VOID *Right,
                              __SIZE_TYPE__ Length)
{
    return CompareMem(Left, Right, Length);
}


VOID *
MftahUefi__wrapper__ReallocatePool(VOID *At,
                                  __SIZE_TYPE__ ToSize)
{
    /* UEFI isn't going to be bothered by simply READING (most) memory locations. */
    /* It's up to the caller to be aware that any INFLATION of a memory pool 'At' will,
        while preserving the previous data, also introduce GARBAGE DATA (not 0s) at its end. */
    VOID *NewPool = AllocatePool(ToSize);
    if (NULL == NewPool) {
        return NULL;
    }

    CopyMem(NewPool, At, ToSize);
    FreePool(At);

    return NewPool;
}


VOID
MftahUefi__wrapper__Print(mftah_log_level_t Level,
                         CONST CHAR *Format,
                         ...)
{
#ifndef EFI_DEBUG
    if (MFTAH_LEVEL_DEBUG == Level) return;
#endif

    /* For prints, we assume that all strings and other details will be safely 
        and dynamically pre-converted to wide characters. */
    va_list c;
    va_start(c, Format);
    VPrint(Format, c);
    va_end(c);
}
