/** The physical starting address of the kernel. 1 MiB is a safe bet. */
KERNEL_PADDR_ENTRY = 1M;

/** The size of the kernel stack. */
KERNEL_STACK_SIZE = 0x4000;

ENTRY (kmain)

SECTIONS
{
	. = KERNEL_PADDR_ENTRY;

	/DISCARD/ : { *(.comment*) }

	.text : ALIGN (4K)
	{
		KEEP(*(.multiboot*))
		*(.text*)
	}

	.rodata : ALIGN (4K)
	{
		*(.rodata*)
	}

	.data : ALIGN (4K)
	{
		*(.data*)
	}

	.bss : ALIGN (4K)
	{
		*(COMMON)
		*(.bss*)

    . = ALIGN (16);
    stack_bottom = .;
    . += KERNEL_STACK_SIZE;
    stack_top = .;
	}

	kernel_end = .;
}
