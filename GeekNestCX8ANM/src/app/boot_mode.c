#include "boot_mode.h"
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>

static const char NAMESPACE[] = "cx8anm";
static const char KEY[] = "bootmode";

boot_mode_t boot_mode_get(void) {
  nvs_handle_t handle;
  uint8_t value = BOOT_MODE_RF;
  nvs_flash_init(); // Idempotent; Arduino has normally done it already.
  if (nvs_open(NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
    nvs_get_u8(handle, KEY, &value);
    nvs_close(handle);
  }
  return value == BOOT_MODE_NET ? BOOT_MODE_NET : BOOT_MODE_RF;
}
void boot_mode_set(boot_mode_t mode) {
  nvs_handle_t handle;
  if (nvs_open(NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
  nvs_set_u8(handle, KEY, (uint8_t)mode);
  nvs_commit(handle);
  nvs_close(handle);
}
void boot_mode_restart(void) {
  // V5A power latch (GPIO4): keep it high through the reset, otherwise a
  // battery-powered radio switches itself off instead of restarting.
  // take_power_latch() releases the hold on the next boot.
  gpio_hold_en(GPIO_NUM_4);
  vTaskDelay(pdMS_TO_TICKS(50));
  esp_restart();
}
void boot_mode_switch(boot_mode_t mode) {
  boot_mode_set(mode);
  boot_mode_restart();
}
