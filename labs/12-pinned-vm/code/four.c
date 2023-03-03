#include "rpi.h"
#include "vector-base.h"

#include "debug-fault.h"
#include "pinned-vm.h"
#include "mmu.h"
#include "armv6-debug-impl.h"
void hello() {
    printk("hello");
    while(1) {}
}
// copy -asm from previus lab
// prefetch abort , data abort 
void prefetch_abort_vector(unsigned pc){
    uint32_t dfsr = cp15_dfsr_get();
    uint32_t dom = bits_get(dfsr, 4, 7);
    uint32_t status = bits_get(dfsr, 0, 3);

    trace("Prefetch abort occurred at PC: %x\n", pc);
    trace("Domain: %x with the DFSR status: %x\n", dom, status);
    domain_access_ctrl_set((DOM_client << (1*2)) | (DOM_client << (dom*2)));
    return;
}
// void data_abort
void data_abort_vector(unsigned pc) {
    uint32_t dfsr = cp15_dfsr_get();
    uint32_t dom = bits_get(dfsr, 4, 7);
    uint32_t status = bits_get(dfsr, 0, 3);

    trace("Data abort fault occurred at PC: %x\n", pc);
    trace("Domain: %x with the DFSR status: %x\n", dom, status);
    domain_access_ctrl_set((DOM_client << (1*2)) | (DOM_client << (dom*2)));
    return;
}
void notmain(void) {
    enum { OneMB = 1024*1024};
    // map the heap: for lab cksums must be at 0x100000.

    kmalloc_init_set_start((void*)MB, MB);
    assert(!mmu_is_enabled());

    enum { dom_kern = 1,
           // domain for user = 2
           dom_user = 2,
           dom_heap = 3 // domain for the heap
    };

    // read write the first mb.
    uint32_t user_addr = OneMB * 16;
    assert((user_addr>>12) % 16 == 0);

    procmap_t p = procmap_default_mk(dom_kern);
    procmap_push(&p, pr_ent_mk(user_addr, MB, MEM_RW, dom_user));
//    procmap_push(&p, pr_ent_mk(heap, MB, MEM_RW, dom_heap));

    trace("about to enable\n");
    pin_mmu_on(&p);

//    lockdown_print_entries("about to turn on first time");
//    assert(mmu_is_enabled());
//    trace("MMU is on and working!\n");

    domain_access_ctrl_set((DOM_client << (dom_kern*2)) | (DOM_no_access << (dom_user*2)));
    uint32_t invalid = GET32(user_addr);

    domain_access_ctrl_set((DOM_client << (dom_kern*2)) | (DOM_no_access << (dom_user*2)));
    PUT32(user_addr, 0xe12fff1e);

    domain_access_ctrl_set((DOM_client << (dom_kern*2)) | (DOM_no_access << (dom_user*2)));
//    PUT32(user_addr, 0xe12fff1e);

    void (*fun_ptr)() = (void *)user_addr;
    (*fun_ptr)();
//    asm volatile("mov lr, %0": : "r" ((void *)user_addr));
//    asm volatile("bx lr");

    staff_mmu_disable();
    assert(!mmu_is_enabled());
    trace("MMU is off!\n");

    trace("SUCCESS!\n");


    // enum { OneMB = 1024*1024 };
    // // Map the heap: for lab cksums must be at 0x100000.
    // kmalloc_init_set_start((void*)MB, MB);
    // assert(!mmu_is_enabled());

    // enum { dom_kern = 1, dom_heap = 3 };          

    // // Read-write the first MB.
    // uint32_t user_addr = OneMB * 16;
    // assert((user_addr>>12) % 16 == 0);
    // uint32_t phys_addr1 = user_addr;
    // PUT32(phys_addr1, 0xdeadbeef);

    // // Set up the interrupt vector.
    // extern uint32_t interrupt_vec[];
    // vector_base_reset((void*)interrupt_vec);
    

    // // Enable data aborts.
    // enable_abort(data_abort_vector);

    // // Make a new domain for heap.
    // mmu_install_table(dom_heap, MB, MB+OneMB, MEM_RW);

    // enum { dom_heap = 3 };
    // mmu_set_domain_coarse(dom_heap, MEM_RW);

    // // Map heap to domain heap.
    // mmu_map_section(dom_heap, OneMB, OneMB, MEM_RW);

    // // Load fault test.
    // mmu_set_domain_access(dom_heap, 0);
    // GET32(OneMB);
    // trace("Load fault test passed.\n");
    // // Store fault test.
    // mmu_set_domain_access(dom_heap, 0);
    // PUT32(OneMB, 0);
    // trace("Store fault test passed.\n");
    // // Jump fault test.
    // mmu_set_domain_access(dom_heap, 0);
    // void (*func_ptr)(void) = (void (*)(void)) (OneMB + 4);
    // PUT32(OneMB, 0xe12fff1e);
    // func_ptr();
    // trace("Jump fault test passed.\n");

    // // Invalid access fault tests.
    // uint32_t *ptr = (uint32_t*) 0x0;
    // GET32((uint32_t)ptr);
    // PUT32((uint32_t)ptr, 0);
    // asm volatile("mov lr, #0; bx lr");
}
// not main 
// {set up the itrupt vector 
// make a new dom_heap = 3, no need for user 
// kinda follow test-procmap.c

// uint32 heap_addr = OneMB * 16 }

