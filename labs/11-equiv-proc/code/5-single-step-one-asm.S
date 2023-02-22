#include "regs-save-restore.h"

@ save all the registers in ascending order.
swi_trampoline:
    mov sp, #INT_STACK_ADDR
    push {r1-r12,r14}
    bl do_syscall
    pop {r1-r12,r14}
    movs pc, lr
    TRAMPOLINE_FULL_REGS(do_syscall)

single_step_trampoline:
    mov sp, #INT_STACK_ADDR
    sub   lr, lr, #4
    mov sp, #INT_STACK_ADDR
    sub lr, lr, #4
    push {r0-r12, lr}
    mov r0, lr
    bl simple_single_step
    pop {r0-r12, lr}
    bx lr
    TRAMPOLINE_FULL_REGS(single_step_full)

@ only handle single step.
.align 5; 
.globl test5_full_single
test5_full_single:
    b unhandled_reset
    b unhandled_undefined_instruction
    b swi_trampoline
    b single_step_trampoline
    b unhandled_data_abort
    b unhandled_interrupt
    b unhandled_fiq


