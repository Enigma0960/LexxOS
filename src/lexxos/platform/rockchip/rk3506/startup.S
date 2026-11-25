/* Entered by U-Boot bootm using the 32-bit ARM Linux boot protocol.
 * Precondition: ARM privileged mode, DDR initialized, MMU and D-cache off,
 * IRQ/FIQ masked, physical load and entry address 0x02000000.
 * U-Boot owns DDR initialization. We deliberately do NOT touch PLL/DDR here.
 */
.syntax unified
.arm
.section .text._start, "ax", %progbits
.global _start
.type _start, %function
_start:
#if defined(LEXXOS_RK3506_MMU_MANAGED)
    /* U-Boot's ARM bootm protocol supplies the DTB physical address in r2.
     * Preserve it in a callee-saved register before startup reuses r2. */
    mov r8, r2
#endif
    cpsid if

    /* Bootstrap stacks for each ARM exception mode. Do not touch the secure
       monitor's state or bring up CPU1/CPU2 at this stage. */
    cps #0x12                 /* IRQ */
    ldr sp, =__irq_stack_top__
    cps #0x17                 /* abort */
    ldr sp, =__abt_stack_top__
    cps #0x1b                 /* undefined */
    ldr sp, =__und_stack_top__
    cps #0x11                 /* FIQ */
    ldr sp, =__fiq_stack_top__
    cps #0x13                 /* SVC */

    ldr sp, =__stack_top__

    /* The image is loaded directly at its linked VMA, so .data is already
       initialized. Only .bss needs clearing before entering C++. */
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
1:
    cmp r0, r1
    strlo r2, [r0], #4
    blo 1b

#if defined(LEXXOS_RK3506_MMU_MANAGED)
    /* The FDT copy is validated while DDR is still identity-addressable.
     * A missing/invalid DTB disables external allocations, not boot. */
    ldr r0, =rk3506_boot_fdt_address
    str r8, [r0]
    bl rk3506_fdt_capture
#endif

    /* U-Boot leaves SCTLR policy inherited by bare-metal LexxOS.
     * Keep high vectors disabled so VBAR works. Cortex-A7's ARMv7 normal
     * RAM supports unaligned byte/halfword/word accesses when SCTLR.A=0;
     * an inherited A=1 otherwise faults on optimized C++ string copies.
     * Device MMIO stays naturally aligned (32-bit registers).
     */
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #0x2000
    bic r0, r0, #0x2       /* SCTLR.A: disable strict alignment faults */

    /* Explicitly select the I-cache policy instead of inheriting it.
     * Keep SCTLR.M and SCTLR.C unchanged (both must be OFF).
     * DMA buffers remain uncached. The SWD RAM-reload preparation
     * already disables and invalidates the I-cache before upload.
     */
    bic r0, r0, #0x1000   /* SCTLR.I: off by default */
#if defined(LEXXOS_RK3506_ICACHE)
    mov r1, #0
    mcr p15, 0, r1, c7, c5, 0  /* Invalidate I-cache */
    mcr p15, 0, r1, c7, c5, 6  /* Invalidate branch predictor */
    dsb sy
    orr r0, r0, #0x1000       /* SCTLR.I: enable */
#endif

    mcr p15, 0, r0, c1, c0, 0
    isb sy

    /* Move vector base to our code instead of firmware-owned vectors. */
    ldr r0, =rk3506_exception_vectors
    mcr p15, 0, r0, c12, c0, 0
    dsb sy
    isb sy

#if defined(LEXXOS_RK3506_MMU_NORMAL_NC) || defined(LEXXOS_RK3506_MMU_MANAGED)
    /* Both legacy and managed modes install identity mappings before the
     * constructors. Managed mode also enables D-cache and protects pages.
     */
    bl rk3506_mmu_identity_normal_nc_initialize
        cmp r0, #0
        bne .Lmmu_ready
    .Lmmu_failed:
        /* U-Boot should hand us MMU and D-cache disabled. Never continue
         * into DMA-enabled drivers when this precondition is violated.
         */
        wfi
        b .Lmmu_failed
    .Lmmu_ready:
#endif

    /* Run freestanding C++ global/static constructors before main. */
    /* On GDB RAM re-entry .bss was cleared, but IOC still routes UART0
     * pins to SWD. Recreate the UART1 software state BEFORE any output.
     * Cold SD boot simply preserves the firmware-provided UART0.
     */
    bl rk3506_debug_bootstrap_console
    bl rk3506_cpp_initialize
    bl rk3506_early_main
2:
    b 2b
.size _start, . - _start
.ltorg
