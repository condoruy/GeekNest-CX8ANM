// CX8ANM firmware for the GeekNest V5A: tinyradio receiver + internet radio.
// The boot mode (NVS "cx8anm/bootmode") decides which application runs; each
// mode owns the hardware alone, so switching modes restarts the chip.
// Arduino declares setup() and loop() with C++ linkage; everything else is C.
extern "C" {
#include "app/receiver.h"
#include "app/boot_mode.h"
}
#include "netradio/netradio.h"

static boot_mode_t mode = BOOT_MODE_RF;

void setup() {
  mode = boot_mode_get();
  if (mode == BOOT_MODE_NET) netradio_begin();
  else receiver_begin();
}
void loop() {
  if (mode == BOOT_MODE_NET) netradio_update();
  else receiver_update();
}
