/* Emergency CPU0 quiescence helper loaded by GDB to 0x027f0000.
 * Executes OUTSIDE the LexxOS image that GDB replaces. No stack,
 * runtime, MMU, scheduler, heap, or existing exception vectors required.
 * GDB must first halt CPU0 and set CPSR to ARM SVC, IRQ/FIQ masked.
 * This is NOT a generic SoC reset: unknown bus masters are out of scope.
 */
.syntax unified
.cpu cortex-a7
.arm
.section .text._start, "ax", %progbits
.global _start
.global rk3506_reload_monitor_stop
.type _start, %function
_start:
    cpsid if

    /* SCTLR.M/C must be OFF: flat physical DDR and uncached data only. */
    mrc p15, 0, r0, c1, c0, 0
    tst r0, #5
    bne fail_cache

    /* DMAC0 manager and all eight channels must be stopped. */
    ldr r0, =0xFF000000
    ldr r1, [r0]
    tst r1, #15
    bne fail_busy
    mov r1, #0
1:
    add r2, r0, #0x100
    add r2, r2, r1, lsl #3
    ldr r3, [r2]
    tst r3, #15
    bne fail_busy
    add r1, r1, #1
    cmp r1, #8
    blo 1b
    ldr r1, [r0, #0x30]
    tst r1, #1
    bne fail_fault
    ldr r1, [r0, #0x34]
    tst r1, #0xff
    bne fail_fault

    /* No DMA writes are in progress according to DMAC0. */
    /* Disable local virtual timer, counter event stream and owned IRQs. */
    mov r0, #0
    mcr p15, 0, r0, c14, c3, 1
    mrc p15, 0, r1, c14, c1, 0
    bic r1, r1, #4
    mcr p15, 0, r1, c14, c1, 0

    ldr r0, =0xFF581180      /* GICD_ICENABLER0, PPI27 */
    ldr r1, =0x08000000
    str r1, [r0]
    ldr r0, =0xFF581280      /* GICD_ICPENDR0, PPI27 */
    str r1, [r0]
    ldr r0, =0xFF581184      /* GICD_ICENABLER1, DMA IRQ55/56 */
    ldr r1, =0x01800000
    str r1, [r0]

    /* I-cache and branch prediction cannot retain overwritten code. */
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #0x1000
    dsb sy
    mcr p15, 0, r0, c1, c0, 0
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0    /* ICIALLU */
    mcr p15, 0, r0, c7, c5, 6    /* BPIALL */
    dsb sy
    isb sy

    ldr r0, =0x52414D4F          /* RAMO: ready for DDR reload */
    b report
fail_cache:
    ldr r0, =0x43414348          /* CACH: MMU or D-cache active */
    b report
fail_busy:
    ldr r0, =0x444D4142          /* DMAB: PL330 busy */
    b report
fail_fault:
    ldr r0, =0x444D4146          /* DMAF: PL330 fault */
report:
    ldr r1, =0x027F0200         /* Status in reserved DDR scratch */
    str r0, [r1]
    dsb sy
rk3506_reload_monitor_stop:
    /* GDB hardware-breakpoint at this symbol, before RAM overwrite. */
    b rk3506_reload_monitor_stop
.size _start, . - _start
.ltorg
