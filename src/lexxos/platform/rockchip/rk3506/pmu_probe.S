/* Guarded Cortex-A7 PMU register snapshot, CPU0 Non-secure SVC mode.
 * Exception recovery is permitted ONLY for the six labelled instructions.
 * Never hide unrelated undefined-instruction exceptions.
 */
.syntax unified
.arm
.section .bss, "aw", %nobits
.balign 4
.global rk3506_pmu_probe_armed
rk3506_pmu_probe_armed:
    .word 0

.section .text, "ax", %progbits
.balign 4
.global rk3506_pmu_probe
.type rk3506_pmu_probe, %function
rk3506_pmu_probe:
    push {r4-r5, lr}
    mov r4, r0               /* caller's snapshot: five uint32_t registers */
    mrs r5, cpsr
    cpsid i                  /* avoid switching out during guarded probe */
    ldr r1, =rk3506_pmu_probe_armed
    mov r2, #1
    str r2, [r1]
    dsb sy
    isb sy
.global rk3506_pmu_probe_read
rk3506_pmu_probe_read:
    mrc p15, 0, r3, c9, c12, 0
    str r3, [r4, #0]         /* PMCR */
.global rk3506_pmu_probe_write
rk3506_pmu_probe_write:
    mcr p15, 0, r3, c9, c12, 0 /* unchanged PMCR; P/C self-clear on reads */
    isb sy
.global rk3506_pmu_probe_enabled
rk3506_pmu_probe_enabled:
    mrc p15, 0, r3, c9, c12, 1
    str r3, [r4, #4]         /* PMCNTENSET */
.global rk3506_pmu_probe_interrupts
rk3506_pmu_probe_interrupts:
    mrc p15, 0, r3, c9, c14, 1
    str r3, [r4, #8]         /* PMINTENSET */
.global rk3506_pmu_probe_overflow
rk3506_pmu_probe_overflow:
    mrc p15, 0, r3, c9, c12, 3
    str r3, [r4, #12]        /* PMOVSR */
.global rk3506_pmu_probe_selector
rk3506_pmu_probe_selector:
    mrc p15, 0, r3, c9, c12, 5
    str r3, [r4, #16]        /* PMSELR */
    mov r2, #0
    str r2, [r1]
    mov r0, #1
    b 1f
.global rk3506_pmu_probe_abort
rk3506_pmu_probe_abort:
    mov r2, #0
    str r2, [r1]
    mov r0, #0
1:
    tst r5, #0x80
    bne 2f
    cpsie i
2:
    pop {r4-r5, pc}
.size rk3506_pmu_probe, . - rk3506_pmu_probe
.ltorg
