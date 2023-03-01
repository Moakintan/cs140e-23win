
#include "rpi-asm.h"

@ this uses a relative branch <b> --- we can do this since we don't
@ have to copy the vector anywhere.
.align 5
.globl interrupt_vec
interrupt_vec:
  ldr pc, =unhandled_reset
  ldr pc, =unhandled_undefined_instruction
  ldr pc, =unhandled_swi
  @ ldr pc, =prefetch_abort
  b prefetch_abort
  ldr pc, =data_abort
  ldr pc, =interrupt_asm

@ only handler that should run since we only enable general interrupts
interrupt_asm_trampoline:
  TRAMPOLINE_FULL_REGS(do_syscall)
  mov sp, #INT_STACK_ADDR   
  sub   lr, lr, #4
  push  {r0-r12,lr}         @ XXX: pushing too many registers: only need caller
  mov   r0, lr              @ Pass old pc
  bl    interrupt_vector    @ C function
  pop   {r0-r12,lr}         @ pop integer registers
  movs    pc, lr        @ moves the link register into the pc and implicitly
                        @ loads the PC with the result, then copies the
                        @ SPSR to the CPSR.

prefetch_abort:
  mov sp, #INT_STACK_ADDR   
  sub   lr, lr, #4
  push  {r0-r12,lr}         @ XXX: pushing too many registers: only need caller
  mov   r0, lr              @ Pass old pc
  bl    prefetch_abort_vector    @ C function
  pop   {r0-r12,lr}         @ pop integer registers
  movs    pc, lr        @ moves the link register into the pc and implicitly
                        @ loads the PC with the result, then copies the
                        @ SPSR to the CPSR.

data_abort:
  mov sp, #INT_STACK_ADDR   
  sub   lr, lr, #8
  push  {r0-r12,lr}         @ XXX: pushing too many registers: only need caller
  mov   r0, lr              @ Pass old pc
  bl    data_abort_vector    @ C function
  pop   {r0-r12,lr}         @ pop integer registers
  movs    pc, lr        @ moves the link register into the pc and implicitly
                        @ loads the PC with the result, then copies the
                        @ SPSR to the CPSR.
