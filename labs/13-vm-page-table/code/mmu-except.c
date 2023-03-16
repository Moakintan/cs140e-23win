#include "vm-ident.h"
#include "libc/bit-support.h"
#include "armv6-debug-impl.h"

volatile struct proc_state proc;

// this is called by reboot: we turn off mmu so that things work.
void reboot_callout(void) {
    if(mmu_is_enabled())
        mmu_disable();
}

void prefetch_abort_vector(unsigned lr) {
    //unimplemented();
}

void data_abort_vector(unsigned lr) {
    uint32_t dfsr = cp15_dfsr_get();
    uint32_t status = bits_get(dfsr, 0, 3);
    uint32_t far = cp15_far_get();

//    trace("Data abort fault occurred at lr: %x\n", lr);
//    trace("Domain: %x with the DFSR status: %b\n At address %x\n", dom, status, far);

    proc.fault_count++;

    if (status == 0b0101) {  // translation fault
        if (proc.fault_addr == far) {
//            trace("Address matched!\n");
            uint32_t new_addr = bits_clr(far, 0, 19);
            mmu_map_section(proc.pt, new_addr, new_addr, proc.dom_id);
        }
    } else if (status == 0b1101) { // permission fault
        domain_access_ctrl_set(0b11 << dom_id*2);
    } else {
        clean_reboot();
    }
    mmu_sync_pte_mods();
    //unimplemented();
}
