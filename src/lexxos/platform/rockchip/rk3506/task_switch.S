/* Cooperative Cortex-A7 scheduler switch, tasks execute in SVC mode.
 * r4-r11 are callee-saved, LR is the resume PC; r12 is saved to maintain
 * the 8-byte AAPCS stack alignment across the C++ scheduler selection call.
 * IRQs remain enabled: IRQ has its own banked stack and does not switch tasks.
 */
.syntax unified
.arm
.section .text.rk3506_task_switch, "ax", %progbits
.global rk3506_task_switch
.type rk3506_task_switch, %function
rk3506_task_switch:
    stmdb sp!, {r4-r11, r12, lr}
    mov r0, sp
    bl rk3506_scheduler_select_stack
    mov sp, r0
    ldmia sp!, {r4-r11, r12, lr}
    bx lr
.size rk3506_task_switch, . - rk3506_task_switch
