#pragma once
#include <Arduino.h>
// Firmware updates from GitHub (flasheo/ota.json in condoruy/GeekNest-CX8ANM)
// or from a .bin uploaded through the web page. Two OTA app slots
// (min_spiffs partition table) let the radio keep running while it writes.

#define CX8_OTA_MANIFEST_URL "https://raw.githubusercontent.com/condoruy/GeekNest-CX8ANM/main/flasheo/ota.json"

struct updater_state_t {
  bool checking = false;
  bool checked = false;
  bool available = false;
  bool installing = false;
  int progress = -1;      // 0..100 while installing
  String latest;          // Version in the manifest
  String notes;
  String url, sha256;
  uint32_t size = 0;
  String error;
  uint32_t checked_at = 0;
};

extern updater_state_t updater;

// Starts a background check (needs WiFi and ~50 KB of free heap for TLS).
// Returns false when a check is already running or memory is short.
bool updater_check_async(void);
// Downloads and installs the version found by the last check; blocking.
// progress(percent) is called while writing. On success the caller restarts.
bool updater_install(void (*progress)(int percent));
// Pending install across a restart (RF mode hands the update to the internet mode).
void updater_set_pending(bool pending);
bool updater_take_pending(void);
// Compares "a.b.c" versions: >0 when a is newer than b.
int updater_compare(const String& a, const String& b);
String updater_json(void);

// Web upload of a .bin (WebServer upload callbacks).
void updater_upload_begin(void);
void updater_upload_write(const uint8_t* data, size_t size);
bool updater_upload_end(String* error);
