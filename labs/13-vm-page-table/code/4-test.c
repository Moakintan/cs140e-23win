/* engler, 140e: sorta cleaned up test code from 14-vm.  */
#include "rpi.h"
#include "rpi-constants.h"
#include "rpi-interrupts.h"
#include "vm-ident.h"
#include "bit-support.h"
#include "mmu.h"
#include "vector-base.h"


#if 0
// for today, just crash and burn if we get a fault.
void data_abort_vector(unsigned lr) {
    unsigned fault_addr;
    // b4-44
    asm volatile("MRC p15, 0, %0, c6, c0, 0" : "=r" (fault_addr));
    staff_mmu_disable();
    panic("data_abort fault at %x\n", fault_addr);
}

// shouldn't happen: need to fix libpi so we don't have to do this.
void interrupt_vector(unsigned lr) {
    staff_mmu_disable();
    panic("impossible\n");
}
#endif

static void fld_set_base_addr(fld_t *f, unsigned addr) {
    // 20 b/c we have 1MB sections.
    demand(mod_pow2(addr,20), addr is not aligned!);

    // make sure if you read it back, it's what you set it to.
    f->sec_base_addr = addr >> 20;

    // if the previous code worked, this should always succeed.
    assert((f->sec_base_addr << 20) == addr);
}

void mmu_map_section_ng(fld_t *pt, uint32_t pa) {
  uint32_t va = 0x200000;
  fld_t *pte = pt + bits_get(va, 20, 31);
  pte->tag = 0b10;
  pte->nG = 0b1;
  pte->AP = 0b11;
  pte->APX = 0b0;
  pte->domain = dom_id;
  pte->TEX = 0b000;
  pte->XN = 0b0;
  fld_set_base_addr(pte, pa);
}

/*
 * an ugly, trivial test run.
 *  1. setup the virtual memory sections.
 *  2. enable the mmu.
 *  3. do some simple tests.
 *  4. turn the mmu off.
 */ 
void vm_test(void) {
    mmu_init();
    assert(!mmu_is_enabled());

    // ugly hack: we start by allocating a single page table at a known address.
    fld_t *pt1 = mmu_pt_alloc(4096), *pt2 = mmu_pt_alloc(4096);

    // 2. setup mappings

    // map the first MB: shouldn't need more memory than this.
    mmu_map_section(pt1, 0x0, 0x0, dom_id);
    mmu_map_section(pt2, 0x0, 0x0, dom_id);

    // map the heap/page table: for lab cksums must be at 0x100000.
    mmu_map_section(pt1, 0x100000,  0x100000, dom_id);
    mmu_map_section(pt2, 0x100000,  0x100000, dom_id);


    // map the GPIO: make sure these are not cached and not writeback.
    // [how to check this in general?]
    mmu_map_sections(pt1, 0x20000000, 0x20000000, 3, dom_id);
    mmu_map_sections(pt2, 0x20000000, 0x20000000, 3, dom_id);


    // setup user stack (note: grows down!)
    pt1->TEX = 0b00;
    pt2->TEX = 0b00;
    mmu_map_section(pt1, STACK_ADDR - OneMB, STACK_ADDR - OneMB, dom_id);
    mmu_map_section(pt2, STACK_ADDR - OneMB, STACK_ADDR - OneMB, dom_id);


    // setup interrupt stack (note: grows down!)
    // if we don't do this, then the first exception = bad infinite loop
    mmu_map_section(pt1, INT_STACK_ADDR - OneMB, INT_STACK_ADDR - OneMB, dom_id);
    mmu_map_section(pt2, INT_STACK_ADDR - OneMB, INT_STACK_ADDR - OneMB, dom_id);

    mmu_map_section_ng(pt1, 0x200000);
    mmu_map_section_ng(pt2, 0x300000);


    // 3. install fault handler to catch if we make mistake.
    extern uint32_t default_vec_ints[];
    vector_base_set(default_vec_ints);

    // 4. start the context switch:

    // set up permissions for the one domain we use rn.
    domain_access_ctrl_set(0b01 << dom_id*2);


    set_procid_ttbr0(71, 1, pt1);
    mmu_enable();

    uint32_t ngp = 0x200000, gp = 0x100000;
    PUT32(ngp, 0xdeadbeef);
    PUT32(gp, 1);

    uint32_t gp1 = GET32(gp), ngp1 = GET32(ngp);
    trace("%x %x\n", gp1, ngp1);

    set_procid_ttbr0(72, 2, pt2);

    uint32_t gp2 = GET32(gp), ngp2 = GET32(ngp);
    trace("%x %x\n", gp2, ngp2);

    printk("******** success ************\n");
}

void notmain() {
    kmalloc_init_set_start((void*)OneMB, OneMB);
    printk("implement one at a time.\n");
    check_vm_structs();
    vm_test();
}