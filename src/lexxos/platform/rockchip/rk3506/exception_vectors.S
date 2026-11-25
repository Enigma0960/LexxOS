/* ARMv7-A exception vectors for RK3506G2, one CPU, ARM state.
 * Installed in VBAR. All other cores stay parked by the boot firmware.
 * IRQ state: SPSR_irq and interrupted LR are preserved; AAPCS stack aligned.
 */
.syntax unified
.arm
.section .text.vectors, "ax", %progbits
.balign 32
.global rk3506_exception_vectors
.type rk3506_exception_vectors, %object
rk3506_exception_vectors:
    b rk3506_unhandled_reset
    b rk3506_undefined
#if defined(LEXXOS_RK3506_FREERTOS)
    b FreeRTOS_SWI_Handler
#else
    b rk3506_svc
#endif
    b rk3506_prefetch_abort
    b rk3506_data_abort
    b rk3506_reserved
#if defined(LEXXOS_RK3506_FREERTOS)
    b FreeRTOS_IRQ_Handler
#else
    b rk3506_irq
#endif
    b rk3506_fiq
.size rk3506_exception_vectors, . - rk3506_exception_vectors

/* Report exception type and approximate faulting PC to UART, then halt.
 * 0=reset, 1=undefined, 2=svc, 3=prefetch abort, 4=data abort,
 * 5=reserved, 7=FIQ. IRQ is handled separately.
 */
.macro FAULT_HANDLER label, number, correction
\label:
    mov r0, #\number
    sub r1, lr, #\correction
    mrs r2, spsr
    b rk3506_exception_fatal
.endm

rk3506_unhandled_reset:
    cpsid if
1:  b 1b
// Only explicitly armed, exact CP15 snapshot instructions may recover.
// All other undefined instructions retain their original fatal behavior.
rk3506_undefined:
#if defined(LEXXOS_RK3506_PMU_PROFILING)
    stmdb sp!, {r0-r3, lr}
    ldr r0, =rk3506_pmu_probe_armed
    ldr r0, [r0]
    cmp r0, #1
    bne 1f
    sub r0, lr, #4
    ldr r1, =rk3506_pmu_probe_read
    cmp r0, r1
    beq 2f
    ldr r1, =rk3506_pmu_probe_write
    cmp r0, r1
    beq 2f
    ldr r1, =rk3506_pmu_probe_enabled
    cmp r0, r1
    beq 2f
    ldr r1, =rk3506_pmu_probe_interrupts
    cmp r0, r1
    beq 2f
    ldr r1, =rk3506_pmu_probe_overflow
    cmp r0, r1
    beq 2f
    ldr r1, =rk3506_pmu_probe_selector
    cmp r0, r1
    bne 1f
2:
    ldr r0, =rk3506_pmu_probe_abort
    str r0, [sp, #16]    /* saved LR_und; return with CPSR from SPSR_und */
    ldmia sp!, {r0-r3, lr}
    movs pc, lr
1:
    ldmia sp!, {r0-r3, lr}
#endif
    mov r0, #1
    sub r1, lr, #4
    mrs r2, spsr
    b rk3506_exception_fatal

#if !defined(LEXXOS_RK3506_FREERTOS)
FAULT_HANDLER rk3506_svc,             2, 4
#endif
FAULT_HANDLER rk3506_prefetch_abort,  3, 4
FAULT_HANDLER rk3506_data_abort,      4, 8
FAULT_HANDLER rk3506_reserved,        5, 4
FAULT_HANDLER rk3506_fiq,             7, 4

#if !defined(LEXXOS_RK3506_FREERTOS)
.type rk3506_irq, %function
rk3506_irq:
    /* LR_irq is interrupted PC+4; correct it before saving. */
    sub lr, lr, #4
    stmdb sp!, {r0-r3, r12, lr}
    mrs r0, spsr
    stmdb sp!, {r0, r1}      /* 8 words saved: keep 8-byte alignment */
    bl rk3506_irq_dispatch
    ldmia sp!, {r0, r1}
    msr spsr_cxsf, r0
    ldmia sp!, {r0-r3, r12, lr}
    movs pc, lr             /* restore CPSR from SPSR_irq */
.size rk3506_irq, . - rk3506_irq
#endif

.ltorg
