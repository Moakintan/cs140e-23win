#include "rpi.h"
#include "pi-sd.h"
#include "mbr.h"

mbr_t *mbr_read() {
  // Be sure to call pi_sd_init() before calling this function!
  mbr_t *mbr = pi_sec_read(0, 1);  // mbr occupies one sector at LBA

  mbr_check(mbr);

  return mbr;
}
