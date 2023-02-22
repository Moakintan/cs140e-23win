#include "rpi.h"
#include "asm-helpers.h"
#include "cpsr-util.h"
#include "breakpoint.h"
#include "vector-base.h"

// Define the two steps to execute
void step1(void) {
  trace("Step 1\n");
}

void step2(void) {
  trace("Step 2\n");
}

// Implement the single-step handler
void simple_single_step(uint32_t pc) {
  if(!brkpt_fault_p())
    panic("pc=%x: is not a breakpoint fault??\n", pc);

  uint32_t spsr = spsr_get();
  if(mode_get(spsr) != USER_MODE)
    panic("pc=%x: is not at user level: <%s>?\n", pc, mode_str(spsr));

  trace("TRACE:simple_single_step:single step pc=%x\n", pc);

  // Clear the breakpoint and return to normal execution
  brkpt_mismatch_stop();
  cpsr_set(cpsr_get() & ~(1 << 6));
}

void notmain(void) {
  // Set up the vector base to handle the single-step exception
  extern uint32_t simple_single_except[];
  vector_base_set(simple_single_except);

  // Enable mismatch detection
  brkpt_mismatch_start();

  // Set a breakpoint at the first step
  brkpt_mismatch_set((uint32_t) step1);

  // Execute the first step
  step1();

  // Set a breakpoint at the second step
  brkpt_mismatch_set((uint32_t) step2);

  // Execute the second step
  step2();
}