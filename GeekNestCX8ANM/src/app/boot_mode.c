#include "boot_mode.h"
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <string.h>

static const char NAMESPACE[] = "cx8anm";
static const char KEY[] = "bootmode";

static bool open_rw(nvs_handle_t* h) { return nvs_open(NAMESPACE, NVS_READWRITE, h) == ESP_OK; }

boot_mode_t boot_mode_get(void) {
  nvs_handle_t handle;
  uint8_t value = BOOT_MODE_RF;
  nvs_flash_init(); // Idempotent; Arduino has normally done it already.
  if (nvs_open(NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
    nvs_get_u8(handle, KEY, &value);
    nvs_close(handle);
  }
  return value == BOOT_MODE_NET ? BOOT_MODE_NET : BOOT_MODE_RF; // BT is never booted by mode, only by slot
}
void boot_mode_set(boot_mode_t mode) {
  nvs_handle_t handle;
  if (!open_rw(&handle)) return;
  nvs_set_u8(handle, KEY, (uint8_t)(mode == BOOT_MODE_BT ? BOOT_MODE_RF : mode));
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

static const esp_partition_t* other_slot(void) { return esp_ota_get_next_update_partition(NULL); }

#ifdef CX8_BT_FIRMWARE
// Bluetooth image: every mode change goes back to the main image.
static const esp_partition_t* main_slot(void) {
  nvs_handle_t h;
  char label[17] = "";
  size_t len = sizeof label;
  if (nvs_open(NAMESPACE, NVS_READONLY, &h) == ESP_OK) { nvs_get_str(h, "mainpart", label, &len); nvs_close(h); }
  const esp_partition_t* p = label[0] ? esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label) : NULL;
  return p ? p : other_slot();
}
void boot_mode_power_off_prepare(void) {
  boot_mode_set(BOOT_MODE_RF);
  const esp_partition_t* p = main_slot();
  if (p) esp_ota_set_boot_partition(p);
}
void boot_mode_switch(boot_mode_t mode) {
  if (mode != BOOT_MODE_BT) {
    boot_mode_set(mode);
    const esp_partition_t* p = main_slot();
    if (p) esp_ota_set_boot_partition(p);
  }
  boot_mode_restart();
}
#else
void boot_mode_power_off_prepare(void) {}
void boot_mode_switch(boot_mode_t mode) {
  if (mode == BOOT_MODE_BT) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* bt = other_slot();
    nvs_handle_t h;
    if (running && bt && open_rw(&h)) { nvs_set_str(h, "mainpart", running->label); nvs_commit(h); nvs_close(h); }
    if (!bt || esp_ota_set_boot_partition(bt) != ESP_OK) mode = BOOT_MODE_RF;
  }
  if (mode != BOOT_MODE_BT) boot_mode_set(mode);
  boot_mode_restart();
}
#endif

bool boot_mode_bt_installed(void) {
  nvs_handle_t h;
  uint8_t stored[32], actual[32];
  size_t len = sizeof stored;
  bool known = false;
  if (nvs_open(NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
    known = nvs_get_blob(h, "btsha", stored, &len) == ESP_OK && len == sizeof stored;
    nvs_close(h);
  }
  const esp_partition_t* bt = other_slot();
  return known && bt && esp_partition_get_sha256(bt, actual) == ESP_OK && memcmp(stored, actual, sizeof actual) == 0;
}
void boot_mode_bt_mark(const char* version) {
  uint8_t sha[32];
  const esp_partition_t* bt = other_slot();
  nvs_handle_t h;
  if (!bt || esp_partition_get_sha256(bt, sha) != ESP_OK || !open_rw(&h)) return;
  nvs_set_blob(h, "btsha", sha, sizeof sha);
  nvs_set_str(h, "btver", version ? version : "");
  nvs_commit(h);
  nvs_close(h);
}
void boot_mode_bt_clear(void) {
  nvs_handle_t h;
  if (!open_rw(&h)) return;
  nvs_erase_key(h, "btsha");
  nvs_erase_key(h, "btver");
  nvs_commit(h);
  nvs_close(h);
}
void boot_mode_bt_version(char* out, int size) {
  nvs_handle_t h;
  size_t len = (size_t)size;
  out[0] = 0;
  if (nvs_open(NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
    if (nvs_get_str(h, "btver", out, &len) != ESP_OK) out[0] = 0;
    nvs_close(h);
  }
}
bt_sub_t boot_mode_bt_sub(void) {
  nvs_handle_t h;
  uint8_t v = BT_SUB_SPEAKER;
  if (nvs_open(NAMESPACE, NVS_READONLY, &h) == ESP_OK) { nvs_get_u8(h, "btsub", &v); nvs_close(h); }
  return v == BT_SUB_TRANSMITTER ? BT_SUB_TRANSMITTER : BT_SUB_SPEAKER;
}
void boot_mode_set_bt_sub(bt_sub_t sub) {
  nvs_handle_t h;
  if (!open_rw(&h)) return;
  nvs_set_u8(h, "btsub", (uint8_t)sub);
  nvs_commit(h);
  nvs_close(h);
}
void boot_mode_set_bt_install(bool pending, bt_sub_t sub) {
  nvs_handle_t h;
  if (!open_rw(&h)) return;
  nvs_set_u8(h, "btinst", pending ? (uint8_t)(1 + sub) : 0);
  nvs_commit(h);
  nvs_close(h);
}
bool boot_mode_take_bt_install(bt_sub_t* sub) {
  nvs_handle_t h;
  uint8_t v = 0;
  if (!open_rw(&h)) return false;
  nvs_get_u8(h, "btinst", &v);
  if (v) { nvs_set_u8(h, "btinst", 0); nvs_commit(h); }
  nvs_close(h);
  if (v && sub) *sub = v == 2 ? BT_SUB_TRANSMITTER : BT_SUB_SPEAKER;
  return v != 0;
}
