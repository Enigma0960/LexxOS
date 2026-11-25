ENTRY(_start)

/*
 * LexxOS flat RV32I images keep text and writable data in one guest
 * address space. These PHDR flags describe the ELF build artifact only:
 * the LexxOS VM does not enforce guest memory page permissions.
 */
PHDRS
{
    text PT_LOAD FLAGS(5); /* PF_R | PF_X */
    data PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS
{
    . = 0x00000000;

    .text : ALIGN(4)
    {
        KEEP(*(.text._start))
        KEEP(*(.text.start))
        KEEP(*(.text.start.*))
        *(.text .text.*)
        *(.rodata .rodata.*)
    } :text

    .data : ALIGN(4)
    {
        *(.sdata .sdata.*)
        *(.data .data.*)
    } :data

    .bss (NOLOAD) : ALIGN(4)
    {
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
    } :data

    . = ALIGN(4);
    PROVIDE(__lexx_image_end = .);
}
