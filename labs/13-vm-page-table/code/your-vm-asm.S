@ Must carefully read B2 of the armv6 manual.  You cannot rely on "well it
@ worked on my test case": mistakes cause stale data, which may or may not
@ show up in your test case (likely not) despite being wildly broken.
@
@ Note: Rd is allegedly a read-only parameter to these instructions, but
@ the manual says SBZ (should be zero).  I think (and the linux source seems
@ to think) that this means we have to explicitly set it to 0.  Not setting
@ Rd=0 is an easy mistake to make.  Perhaps is worth building in (Linux does
@ not, but) --- the cost of an extra mov is negligible given our other 
@ overheads.
@
@ Alot of people put these instructions in inline assembly, wrapped up by 
@ function calls (e.g., cs107e's MMU code).  This practice is dangerous. 
@ For example, if you are flushing caches, you cannot be sure that the 
@ function return, etc does not then reload them, esp w.r.t. the BTB.  I 
@ think the only safe approach is to put directly in assembly so you are 
@ guaranteed no branch, indirect jump, load/store happens in between.
@
@ A lot of MMU code out there appears pretty broken b/c they don't do enough
@ flushing operations and/or rely on gcc code generation to not do the above.

#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ armv6-coprocessor-asm.h
@   - has many macros for the privileged instructions.
@   - many page numbers 
@
@ make sure you look at B2 as well as 

@ just read the cache type: use C code to figure out if it is unified
@ b6-13, b6-14
MK_FN(get_cache_type)
    asm_todo("implement this")
    bx lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 1:  write to the domain register.
@    easy sequencing rule: look at 2.7.6
@
@ depending on your code, you might name this
@ something else (or inline into the c code):
@  void domain_access_ctrl_set(uint32_t dom_reg);
@ move the value 0 into r1, read domain control regsiter, do a prefetch flush (no DSB needed)
MK_FN(cp15_domain_ctrl_rd)
    mov r1, #0 @ moves the value 0 into the r0 register
    DOMAIN_CTRL_RD(r0)  @ need to read dom ctrl/ need to read control reg1
    PREFETCH_FLUSH(r1)  @ according to section B2.7.6: any change to cp15 needs prefectchflush, no DSB needed
    bx lr


@ write input param into RD, do a prefectch flush (no DSB needed)
MK_FN(cp15_domain_ctrl_wr)
    DOMAIN_CTRL_WR(r0) @ arm1176 3-63 write to Domain Control reg / need to write input param into RD
    @ any change to CP14/CP15 need prefetch flush to be VISIBLE - see b2-24
    mov r0, #0  
    PREFETCH_FLUSH(r0)
    bx lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 2: 
@ void mmu_reset(void)
@   - called on boot-up.
@   - clear the dcache, icache, tlbs: make sure you obey 
@     any rules from B2


@ inavlidate dcache and icache (do not need to DSB because we are not cleaning the cache,
@ invalidate both iTLB and dTLB
@ invalidate BTB, need this b/c the code changed and so the branch predictor contents could be stale
@ ensure invalidations complete and are visible
@ do a prefecth flush to force completion on the BTB flush
MK_FN(mmu_reset)
    @ first clear dcache and icache )
    @ see page B2-22, data cache invalidate must come before instruc cache
    @ DO NOT clean data cache as it might potentially write garbage back to memory
    mov r0, #0
    INV_ALL_CACHES(r0) @ invalidate dcache and icache, do not need to DSB because we are not cleaning the cache
    INV_TLB(r0)   @ invalidate both iTLB and dTLB b2-23
    FLUSH_BTB(r0) @ invalidate BTB, need this b/c the code changed and so the branch predictor contents could be stale
    DSB(r0) @ ensure completion of the ICache invalidation: b2-23
    PREFETCH_FLUSH(r0) @ b2-23 - force completion on BTB flush, need to flush after a BTB flush, make TLB invalidation visible
    bx lr



@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 3
@ void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
@
@ given the control register value <c>, write it 
@ and do any needed sync
@
@ this is a pretty tricky routine.  make sure you handle the 
@ icache and dcache.   when the mmu is off, these mappings will
@ change.  also, dcache does not work with MMU off.
@
@ make sure you look at arm1176, 6-9.
@
MK_FN(mmu_disable_set_asm)

    @ note: this piece of code is mapped into the sample place in both
    @ MMU-enabled and MMU-disabled so BTB and Prefetch entries work identically
    @ to both MMU-enable and disable.   Without this requirement I believe is
    @ very hard to get right (perhaps impossible?) since we cannot flush
    @ the BTB and prefetch and switch MMU on/off atomically.  thus, would
    @ flush, but instructions could get fetched before we could disable,
    @ defeating the purpose.

    @ first, clean data cache, as this will be disabled in this op.
    @ again: CLEAN, not invalidate. We want to preserve whatever updates
    @ were made to memory prior to mmu being disabled
    mov r2, #0
    CLEAN_INV_DCACHE(r2) @ says that data cache must be disabled before
    INV_ALL_CACHES(r2)
    @INV_TLB(r2) @ invalidate TLB contents - readme states that we must invalidate TLBs aswell
    @ FLUSH_BTB(r2) we don't need to do this here cause its assembly. We can just do this at the end
    DSB(r2) @ ensure data cache operation completed - b2-21
    PREFETCH_FLUSH(r2) @ ensure instruction operations visible - b2-21
    @ 
    @ clear bit 2 (C) of control register c1 - b3-13, updating the control register
    @ also clear bit 2 (M) to disable MMU - b3-12
    mov r1, r0 @ copy control register value to r1
    and r1, r1, #1 @ turn off bits 0 and 2 - see above
                   @ it should be okay to turn off both of these at once: 6-9
                   @ we also want to disable iCache
    mvn r2,  #0x1000 @ negate 12th bit to create mask
    and r1, r1, r2 @ AND r1 with masked bit 12

    @ write value back into control register
    CONTROL_REG1_WR(r1)
    mov r2, #0
    PREFETCH_FLUSH(r2) @ perform prefetch flush after writing to coproc - b2-24
    INV_ALL_CACHES(r2) @ invalidate instruction cache
    FLUSH_BTB(r2) @ Since we are disabling the MMU here, we must invalidate BTB and PF
    DSB(r2)
    PREFETCH_FLUSH(r2) @ perform prefetch flush after writing to coproc - b2-24

    bx lr

@ void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
@  called by <mmu.c:mmu_enable()>
@
@ make sure you look at arm1176, 6-9.
@
MK_FN(mmu_enable_set_asm)
    @ invalidate cache before enabling MMU
    CONTROL_REG1_RD(r3)
    bic r3, r3, #(1<<12) @bit clear
    INV_ALL_CACHES(r1)
    CONTROL_REG1_WR(r3)

    mov r1, #0
    PREFETCH_FLUSH(r1)

    INV_ALL_CACHES(r1)
    INV_TLB(r1) @ must invalidate contents if invalid and re-enabled: 6-9
    DSB(r1) @ ensure data cache operation completed - b2-21
    PREFETCH_FLUSH(r1) @ ensure instruction operations visible - b2-21

    @ write enable bit to CP15 control register 1: 6-9
    CONTROL_REG1_WR(r0)

	@ enforce barrier before re-writing to control_reg1
	FLUSH_BTB(r1)
	DSB(r1)
	PREFETCH_FLUSH(r1)
    
    bx lr


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 3.5
@ void mmu_sync_pte_mods();
@     sync caches and wait for completion/visibility after updating
@     PTEs.
@
MK_FN(mmu_sync_pte_mods)
  @ see instruction sequence on pg b2-23
  mov r0, #0
  CLEAN_INV_DCACHE(r0)
  @ INV_ICACHE(r0)
  DSB(r0) @ ensure data cleaned from D cache is visible
  INV_TLB(r0) 
  FLUSH_BTB(r0)
  DSB(r0) @ ensure completion of invalidate BTB
  @ mov r0, #0
  PREFETCH_FLUSH(r0)
  bx lr

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ part 4
@
@ void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);
@     sequence from b2-25 + rules from 3-129
@
@ this is the fanciest one.
MK_FN(cp15_set_procid_ttbr0)
    @ first, set ASID to 0: see b2-25

    mov r2, #0
    DSB(r2)
    ASID_SET(r2) @ write 0 to ASID
    mov r2, #0
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2) @ ASID reset requires PF

    @ b4-41: write 0 to lower 5 bits to be safe
    mvn r2, #0x1f
    and r1, r2
    @ write to TTBR0
    TTBR0_SET(r1)
    @ write 0 to TTBR1 and TTBRBCR

    mov r2, #0
    TTBR1_SET(r2)
    TTBR_BASE_CTRL_WR(r2)  @ Set ttbrd to 0

    FLUSH_BTB(r2) @ must flush BTB after updating TTBR: b2-24 section 2.7.5
    DSB(r2) @ensure BTB invalidated
    PREFETCH_FLUSH(r2)
    @ then update ASID/ProcID
    ASID_SET(r0)
    FLUSH_BTB(r2) @ need to invalidate BTB after changing ASID
    DSB(r2) @ensure BTB invalidated
    PREFETCH_FLUSH(r2) @ ASID reset requires PF

    bx lr


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ you need this for pinned-vm.c
@
@ this just turns the MMU on for the first time.
@ r0 = asid
@ r1 = pointer to empty page table
@
@ is a combination/simplification of mmu_init() and 
@ mmu_enable_set_asm().
@
@ also could just call the above from C code! :)
MK_FN(mmu_on_first_time)
    @ asm_todo("implement this for pinned vm")
    @ cp15_set_procid_ttbr0
    bx lr


MK_FN(cp15_ctrl_reg1_wr)
    mov r1, #0
    CONTROL_REG1_WR(r0)  @ need to write input param into Rd
    PREFETCH_FLUSH(r1)  @ according to section B2.7.6: any change to cp15 needs prefectchflush, no DSB needed
    bx lr

MK_FN(cp15_ctrl_reg1_rd)
    mov r1, #0
    CONTROL_REG1_RD(r0)  @ need to read control reg1
    PREFETCH_FLUSH(r1)  @ according to section B2.7.6: any change to cp15 needs prefectchflush, no DSB needed
    bx lr
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ assorted other ASM helpers below:
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@



@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@
@ mmu_ttbr0_get()
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
MK_FN(mmu_ttbr0_get)
  TTBR0_GET(r0)
  bx lr

