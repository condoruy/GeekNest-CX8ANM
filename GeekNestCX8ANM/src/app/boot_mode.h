#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// CX8ANM: which application the next boot starts. Kept in its own NVS
// namespace so the tinyradio receiver settings format is untouched.
//
// The Bluetooth modes live in a separate firmware image stored in the other
// OTA slot (the Bluetooth stack does not fit next to WiFi in one image).
// BOOT_MODE_BT boots that image; from it, RF and NET boot the main image again.
typedef enum { BOOT_MODE_RF = 0, BOOT_MODE_NET = 1, BOOT_MODE_BT = 2 } boot_mode_t;
typedef enum { BT_SUB_SPEAKER = 0, BT_SUB_TRANSMITTER = 1 } bt_sub_t;

boot_mode_t boot_mode_get(void);
void boot_mode_set(boot_mode_t mode);
// Stores the mode and restarts the chip (the power latch stays held). From the
// main image, BOOT_MODE_BT requires boot_mode_bt_installed().
void boot_mode_switch(boot_mode_t mode) __attribute__((noreturn));
// Restarts the chip keeping the board powered (use instead of esp_restart).
void boot_mode_restart(void) __attribute__((noreturn));
// Before powering off: the Bluetooth image makes the next power-on start the main receiver.
void boot_mode_power_off_prepare(void);

// Bluetooth image bookkeeping (main image).
bool boot_mode_bt_installed(void);            // The other slot holds the image recorded by bt_mark
void boot_mode_bt_mark(const char* version);  // Records the image now in the other slot
void boot_mode_bt_clear(void);                // The other slot was overwritten (main update)
void boot_mode_bt_version(char* out, int size);
bt_sub_t boot_mode_bt_sub(void);
void boot_mode_set_bt_sub(bt_sub_t sub);
// Pending "install Bluetooth then switch to it" request, handed from RF to NET mode.
void boot_mode_set_bt_install(bool pending, bt_sub_t sub);
bool boot_mode_take_bt_install(bt_sub_t* sub);
#ifdef __cplusplus
}
#endif
