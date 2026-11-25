/* Lyra M DDR begins at 0x0 and spans 128 MiB. The CPU0 LexxOS image is
 * linked in BOOTRAM (0x02000000..0x027fffff); managed-MMU mode reserves the
 * last MiB for non-cacheable DMA and the GDB monitor. All other DDR regions
 * remain firmware-owned or unallocated until explicit reservation handling.
 */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    BOOTRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 0x00800000
    /* CPU0-only, Normal non-cacheable: never included in the FIT payload. */
    DMARAM (rw) : ORIGIN = 0x02700000, LENGTH = 0x000F0000
}

/* ELF metadata only: optional MMU identity sections still map all DDR RWX.
 * PT_LOAD flags do not create memory protection; they avoid an RWX LOAD. */
PHDRS {
    code PT_LOAD FLAGS(5); /* R|X */
    data PT_LOAD FLAGS(6); /* R|W */
}

SECTIONS {
    . = ORIGIN(BOOTRAM);
    .text : ALIGN(4) {
        KEEP(*(.text._start))
        . = ALIGN(32);
        KEEP(*(.text.vectors))
        *(.text .text.*)
    } > BOOTRAM :code
    .rodata : ALIGN(4) {
        *(.rodata .rodata.*)
    } > BOOTRAM :code
    /* libgcc ARM EABI unwind helper looks up these boundaries, even when
     * the application is compiled with -fno-exceptions. Preserve the real
     * tables instead of discarding .ARM.exidx/.ARM.extab. */
    .ARM.extab : ALIGN(4) {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > BOOTRAM :code
    .ARM.exidx : ALIGN(4) {
        __exidx_start = .;
        KEEP(*(.ARM.exidx* .gnu.linkonce.armexidx.*))
        __exidx_end = .;
    } > BOOTRAM :code
    /* Managed MMU uses RX (APX=1) pages up to this 4 KiB boundary. */
    . = ALIGN(4096);
    __rx_end__ = .;
    .preinit_array : ALIGN(4096) {
        __preinit_array_start = .;
        KEEP(*(.preinit_array .preinit_array.*))
        __preinit_array_end = .;
    } > BOOTRAM :code
    .init_array : ALIGN(4096) {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } > BOOTRAM :code
    .data : ALIGN(4096) {
        *(.data .data.*)
    } > BOOTRAM :data
    .bss (NOLOAD) : ALIGN(16) {
        __bss_start__ = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(16);
        __bss_end__ = .;
    } > BOOTRAM :data
    .stack (NOLOAD) : ALIGN(16) {
        __stack_bottom__ = .;
        . += 0x4000;
        __stack_top__ = .;
        . = ALIGN(16);
        __irq_stack_bottom__ = .;
        . += 0x1000;
        __irq_stack_top__ = .;
        . = ALIGN(16);
        __abt_stack_bottom__ = .;
        . += 0x400;
        __abt_stack_top__ = .;
        . = ALIGN(16);
        __und_stack_bottom__ = .;
        . += 0x400;
        __und_stack_top__ = .;
        . = ALIGN(16);
        __fiq_stack_bottom__ = .;
        . += 0x400;
        __fiq_stack_top__ = .;
    } > BOOTRAM :data
    __image_end__ = .;
    /* A non-cacheable pool outside the contiguous loaded image. */
    .dma (NOLOAD) : ALIGN(4096) {
        __dma_start__ = .;
        KEEP(*(.dma .dma.*))
        . = ALIGN(4096);
        __dma_end__ = .;
    } > DMARAM :NONE
    /DISCARD/ : { *(.comment) *(.note*) }
}
ASSERT(__image_end__ <= ORIGIN(BOOTRAM) + LENGTH(BOOTRAM), "RK3506 bootstrap exceeds RAM reservation")
/* Reserved for the out-of-image GDB reload monitor at 0x027f0000.
 * The monitor is injected by the host, not linked into lexxos.elf.
 */
ASSERT(__image_end__ <= 0x027f0000, "RK3506 ELF overlaps GDB debug monitor reserve")

/* Managed-MMU mode reserves the final MiB of BOOTRAM for DMA/monitor. */
ASSERT(__image_end__ <= ORIGIN(DMARAM), "RK3506 kernel overlaps DMA reservation")
ASSERT(__dma_end__ <= ORIGIN(DMARAM) + LENGTH(DMARAM), "RK3506 DMA pool too large")
