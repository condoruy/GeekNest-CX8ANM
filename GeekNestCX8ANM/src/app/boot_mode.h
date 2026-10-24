#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// CX8ANM: which application the next boot starts. Kept in its own NVS
// namespace so the tinyradio receiver settings format is untouched.
typedef enum { BOOT_MODE_RF = 0, BOOT_MODE_NET = 1 } boot_mode_t;

boot_mode_t boot_mode_get(void);
void boot_mode_set(boot_mode_t mode);
// Stores the mode and restarts the chip (the power latch stays held).
void boot_mode_switch(boot_mode_t mode) __attribute__((noreturn));
// Restarts the chip keeping the board powered (use instead of esp_restart).
void boot_mode_restart(void) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif
