#include "rpi.h"
#include "vector-base.h"

#include "debug-fault.h"

// currently only handle a single breakpoint.
static bfault_handler_t brkpt_handler = 0;
// currently only handle a single watchpoint
static wfault_handler_t watchpt_handler = 0;

// if we have a breakpoint fault, call the breakpoint
// handler.
void prefetch_abort_vector(unsigned lr) {
    // lr needs to get adjusted for watchpoint fault?
    // we should be able to return lr?
    if(was_brkpt_fault())
        brkpt_handler(lr,0);
    else 
        panic("should not get another fault\n");
}

// if we have a dataabort fault, call the watchpoint
// handler.
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

// setup handlers: enable cp14
void debug_fault_init(void) {
    extern uint32_t interrupt_vec[];
    vector_base_reset((void*)interrupt_vec);

    cp14_enable();

    //unimplemented();

    // just started, should not be enabled.
    assert(!cp14_bcr0_is_enabled());
    assert(!cp14_bcr0_is_enabled());
}

// set a breakpoint on <addr>: call <h> when triggers.
void brkpt_set0(uint32_t addr, bfault_handler_t h) {
     uint32_t b = cp14_bcr0_get();
    b = bit_clr(b, 0); // disable the breakpoint temporarily
    cp14_bcr0_set(b);

    cp14_bvr0_set((uint32_t)addr);

    b = cp14_bcr0_get();
    b = bits_clr(b, 20, 22);
    b = bits_set(b, 14, 15, 0b00);
    b = bits_set(b, 5, 8, 0b1111);
    b = bits_set(b, 1, 2, 0b11);
    b = bit_set(b, 0);
    cp14_bcr0_set(b);
    prefetch_flush();
    //unimplemented();

    assert(cp14_bcr0_is_enabled());
    brkpt_handler = h;
}

// set a watchpoint on <addr>: call handler <h> when triggers.
void watchpt_set0(uint32_t addr, wfault_handler_t h) {
    cp14_wcr0_disable();
    prefetch_flush();
    assert(!cp14_wcr0_is_enabled());
    cp14_wvr0_set(addr);
    prefetch_flush();

    uint32_t b = cp14_wcr0_get();
    b = bit_clr(b, 20);
    b = bits_set(b, 14, 15, 0b00);
    b = bits_set(b, 0, 8, 0x1FF);
    //transformations to b
    //cp14_wvr0_set(bit_set(cp14_wvr0_get(), 0));
   
   
    //unimplemented();
    cp14_wcr0_set(b);
    prefetch_flush();
    //unimplemented();


    assert(cp14_wcr0_is_enabled());
    watchpt_handler = h;
}
