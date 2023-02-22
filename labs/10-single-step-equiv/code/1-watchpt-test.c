// handle a store to address 0 (null)
#include "rpi.h"
#include "vector-base.h"
#include "armv6-debug-impl.h"

static int load_fault_n, store_fault_n;

// change to passing in the saved registers.
void data_abort_vector(unsigned lr) {
    if(was_brkpt_fault())
        panic("should only get debug faults!\n");

    if(!was_watchpt_fault())
        panic("should only get watchpoint faults!\n");

    // instruction that caused watchpoint.
    uint32_t pc = watchpt_fault_pc();

    if(datafault_from_ld()) {
        trace("load fault at pc=%x\n", pc);
        assert(pc == (uint32_t)GET32);
        load_fault_n++;
    } else {
        trace("store fault at pc=%x\n", pc);
        assert(pc == (uint32_t)PUT32);
        store_fault_n++;
    }

    cp14_wcr0_disable();
}

void notmain(void) {

    extern uint32_t interrupt_vec[];
    vector_base_set((void*)interrupt_vec);

    // Install the exception handlers

    // Enable the debug coprocessor
    // 1. install exception handlers using vector base.
    //      must have an entry for data-aborts that has
    //      a valid trampoline to call <data_abort_vector>
    //unimplemented();
    //vector_install(data_abort_vector);
    // 2. enable the debug coprocessor.
    cp14_enable();

    /* 
     * see: 
     *   - 13-47: how to set a simple watchpoint.
     *   - 13-17 for how to set bits in the <wcr0>
     */
    enum { null = 0 };
    cp14_wcr0_disable();
    prefetch_flush();
   // cp14_wcr0_set(cp14_status_get())
    // just started, should not be enabled.
    assert(!cp14_wcr0_is_enabled());

    cp14_wvr0_set(null);
    prefetch_flush();
    // setup watchpoint 0.  needs two registers.
    //  - see 13-17 for how to set bits in the <wcr0>

    //cp14_wcr0_set(bit_set(cp14_wcr0_get(), 0));
    //cp14_wcr0_set(bit_set(cp14_wcr0_get(), 0));
    //uint32_t b = 0xFF;
    uint32_t b = cp14_wcr0_get();
    b = bit_clr(b, 20);
    b = bits_set(b, 14, 15, 0b00);
    b = bits_set(b, 0, 8, 0x1FF);
    //transformations to b
    //cp14_wvr0_set(bit_set(cp14_wvr0_get(), 0));
   
   
    //unimplemented();
    cp14_wcr0_set(b);
    prefetch_flush();

    assert(cp14_wcr0_is_enabled());
    trace("set watchpoint for addr %p\n", null);

    trace("should see a store fault!\n");
    PUT32(null,0);

    if(!store_fault_n)
        panic("did not see a store fault\n");

    assert(!cp14_wcr0_is_enabled());
    //cp14_wcr0_enable();
    cp14_wcr0_set(b);
    //prefetch_flush();
    assert(cp14_wcr0_is_enabled());

    trace("should see a load fault!\n");
    GET32(null);
    if(!load_fault_n)
        panic("did not see a load fault\n");
    
    trace("SUCCESS\n");
}
