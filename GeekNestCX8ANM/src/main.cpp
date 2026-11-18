// CX8ANM firmware for the GeekNest V5A.
// Main image: tinyradio receiver + internet radio; the boot mode (NVS
// "cx8anm/bootmode") decides which runs. Bluetooth image (CX8_BT_FIRMWARE):
// speaker and transmitter, stored in the other OTA slot.
// Arduino declares setup() and loop() with C++ linkage; everything else is C.
extern "C" {
#include "app/receiver.h"
#include "app/boot_mode.h"
}
#ifdef CX8_BT_FIRMWARE
#include "btapp/btapp.h"
void setup() { btapp_begin(); }
void loop() { btapp_update(); }
#else
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
#endif
