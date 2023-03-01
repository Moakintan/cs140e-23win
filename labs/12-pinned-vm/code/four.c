#include "rpi.h"
#include "vector-base.h"

#include "debug-fault.h"
#include "pinned-vm.h"
#include "mmu.h"
// copy -asm from previus lab
// prefetch abort , data abort 
void prefetch_abort(unsigned lr){
    if(was_brkpt_fault())
        brkpt_handler(lr,0);
    else 
        panic("should not get another fault\n");
}
// void data_abort
void data_abort_vector(unsigned lr) {
    // watchpt handler.
    if(was_brkpt_fault())
        panic("should only get debug faults!\n");
    if(!was_watchpt_fault())
        panic("should only get watchpoint faults!\n");

    assert(watchpt_handler);

    // instruction that caused watchpoint.
    uint32_t pc = watchpt_fault_pc();
    // addr that the load or store was on.
    void *addr = (void*)watchpt_fault_addr();
    watchpt_handler(addr, pc, 0); 
}
void notmain(void) {
    enum { OneMB = 1024*1024 };
    // Map the heap: for lab cksums must be at 0x100000.
    kmalloc_init_set_start((void*)MB, MB);
    assert(!mmu_is_enabled());

    enum { dom_kern = 1, dom_heap = 3 };          

    // Read-write the first MB.
    uint32_t user_addr = OneMB * 16;
    assert((user_addr>>12) % 16 == 0);
    uint32_t phys_addr1 = user_addr;
    PUT32(phys_addr1, 0xdeadbeef);

    // Set up the interrupt vector.
    extern uint32_t interrupt_vec[];
    vector_base_reset((void*)interrupt_vec);
    

    // Enable data aborts.
    enable_abort(data_abort_vector);

    // Make a new domain for heap.
    mmu_install_table(dom_heap, MB, MB+OneMB, MEM_RW);

    enum { dom_heap = 3 };
    mmu_set_domain_coarse(dom_heap, MEM_RW);

    // Map heap to domain heap.
    mmu_map_section(dom_heap, OneMB, OneMB, MEM_RW);

    // Load fault test.
    mmu_set_domain_access(dom_heap, 0);
    GET32(OneMB);
    trace("Load fault test passed.\n");
    // Store fault test.
    mmu_set_domain_access(dom_heap, 0);
    PUT32(OneMB, 0);
    trace("Store fault test passed.\n");
    // Jump fault test.
    mmu_set_domain_access(dom_heap, 0);
    void (*func_ptr)(void) = (void (*)(void)) (OneMB + 4);
    PUT32(OneMB, 0xe12fff1e);
    func_ptr();
    trace("Jump fault test passed.\n");

    // Invalid access fault tests.
    uint32_t *ptr = (uint32_t*) 0x0;
    GET32((uint32_t)ptr);
    PUT32((uint32_t)ptr, 0);
    asm volatile("mov lr, #0; bx lr");
}
// not main 
// {set up the itrupt vector 
// make a new dom_heap = 3, no need for user 
// kinda follow test-procmap.c

// uint32 heap_addr = OneMB * 16 }

