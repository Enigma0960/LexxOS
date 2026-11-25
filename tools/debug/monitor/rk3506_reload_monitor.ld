/* Out-of-image emergency monitor. Main kernel runs below 0x027f0000.
 * GDB writes this monitor separately; NEVER include it in lexxos.elf.
 * 0x027f0200 holds the status word; leave 0x027f0000..027f0fff free.
 */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS {
    . = 0x027f0000;
    .text : { KEEP(*(.text._start)) *(.text .text.*) *(.rodata .rodata.*) }
    /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.attributes*) }
}
ASSERT(SIZEOF(.text) <= 0x200, "RK3506 reload monitor overlaps status word")
