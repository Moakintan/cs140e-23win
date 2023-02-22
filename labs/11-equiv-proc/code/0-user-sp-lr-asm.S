#include "rpi-asm.h"
    
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ get USER banked regs by switching to SYSTEM mode and back.
@ if we switched to USER, we would need to use a system call to 
@ get back.

@ get the USER sp by switching to SYS_MODE 
@ and back.
MK_FN(user_sp_get)
    cps #SYS_MODE
    prefetch_flush(r1);
    
    @ USER And SYSTEM share the same sp/lr/cpsr
    mov r0, sp

    cps #SUPER_MODE
    prefetch_flush(r1);

    bx lr

@ set the USER sp
MK_FN(user_sp_set)
    mrs r1, cpsr    // Save current CPSR
    cps #SYS_MODE
    prefetch_flush(r2);
    ldm r0, {sp}^   // Load the user sp from memory
    cps #SUPER_MODE
    msr cpsr_c, r1  // Restore saved CPSR
    @ asm_todo(implement this using a macro to wrap things up);
    bx lr

@ get the USER lr 
MK_FN(user_lr_get)
    mrs r1, cpsr    // Save current CPSR
    cps #SYS_MODE
    prefetch_flush(r2);
    mov r0, lr      // Move the user lr into r0
    cps #SUPER_MODE
    msr cpsr_c, r1  // Restore saved CPSR
    @ asm_todo(implement this using a macro to wrap things up);
    bx lr

@ set USER lr 
MK_FN(user_lr_set)
    mrs r1, cpsr    // Save current CPSR
    cps #SYS_MODE
    prefetch_flush(r2);
    stm r0, {lr}^   // Store the user lr to memory
    cps #SUPER_MODE
    msr cpsr_c, r1  // Restore saved CPSR
    @ asm_todo(implement this using a macro to wrap things up);
    bx lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ get USER banked regs by using ldm/stm instructions with the ^
@ modifier.

@ address to a uint32_t word is passed in r0
MK_FN(mem_user_sp_get)
    stm r0, {sp}^
    bx lr

@ address to a uint32_t word is passed in r0
MK_FN(mem_user_sp_set)
    ldm r0, {sp}^ 
    asm_todo("implement this");
    bx lr

@ address to a uint32_t word is passed in r0
MK_FN(mem_user_lr_get)
    stm r0, {sp}^
    @ asm_todo(implement this);
    bx lr

@ address to a uint32_t word is passed in r0
MK_FN(mem_user_lr_set)
    ldm r0, {lr}^  
    @ asm_todo(implement this);
    bx lr

@ address to an array with two uint32_t words 
@ is passed in r0
MK_FN(mem_user_sp_lr_get)
    stm r0, {sp, lr}^ 
    @ asm_todo(implement this);
    bx lr

@ address to an array with two uint32_t words 
@ is passed in r0
MK_FN(mem_user_sp_lr_set)
    ldm r0, {sp, lr}^ 
    @ asm_todo(implement this);
    bx lr
