# Load this file with the *new* ELF after running `debug ram prepare` on UART1:
# arm-none-eabi-gdb -x tools/debug/gdb/rk3506-ram.gdb build/picocalc-lyra-debug/lexxos.elf
# ATTENTION: a GDB load overwrites the old kernel, and can fail on hardware.
# Merely sourcing this file DOES NOT overwrite RAM or change execution.
set pagination off
set architecture arm

target extended-remote 127.0.0.1:3333

define lexxos_ram_reload
    # Precondition: the currently running SD/DDR firmware was upgraded once
    # to support `debug ram prepare` and is parked outside any IRQ handler.
    monitor halt
    # Drop breakpoints whose addresses belong to the old image first.
    delete breakpoints
    load
    # The halted Cortex-A7 may have a banked exception-mode CPSR; initialize
    # a deterministic privileged ARM SVC context, both IRQ and FIQ masked.
    set $cpsr = ($cpsr & 0xffffff00) | 0xd3
    set $pc = 0x02000000
    hbreak rk3506_early_main
    echo RAM image loaded; inspect PC and use `continue` to start new LexxOS.\n
end

document lexxos_ram_reload
    DANGEROUS: replace the currently executing RK3506 kernel in DDR.
    First execute `debug swd on`, then `debug ram prepare` via UART1.
    Must NOT be called from an arbitrary running task or IRQ handler.
    Does not change the SD card or hardware-reset RK3506.
end

echo Connected. After `debug ram prepare`, invoke `lexxos_ram_reload` explicitly.\n
