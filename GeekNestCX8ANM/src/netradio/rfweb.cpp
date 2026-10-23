#include "rfweb.h"
#include "config.h"
#include "updater.h"
#include "webui.h"
#include "ui/splash.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>
extern "C" {
#include "app/rf_remote.h"
#include "boards/board.h"
}

namespace {
enum phase_t { RF_WEB_OFF, RF_WEB_CONNECTING, RF_WEB_ONLINE };
phase_t phase = RF_WEB_OFF;
uint32_t started = 0, last_attempt = 0;
volatile bool ntp_synced = false;
bool clock_set = false, web_started = false;
void on_time_sync(struct timeval*) { ntp_synced = true; }

String json_escape(const char* text) {
  String out;
  for (const char* p = text ? text : ""; *p; ++p) {
    if (*p == '"' || *p == '\\') { out += '\\'; out += *p; }
    else if ((uint8_t)*p < 0x20) out += ' ';
    else out += *p;
  }
  return out;
}
} // namespace

String rf_status_json(void) {
  rf_remote_status_t s;
  rf_remote_status(&s);
  String j = "{\"mode\":\"rf\",\"ver\":\"" CX8_FIRMWARE_VERSION "\",";
  j += "\"ssid\":\"" + json_escape(WiFi.SSID().c_str()) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\",";
  j += "\"rssi_wifi\":" + String(WiFi.RSSI()) + ",";
  j += "\"band\":\"" + json_escape(s.band) + "\",\"band_index\":" + String(s.band_index) + ",";
  j += "\"hz\":" + String(s.frequency_hz) + ",\"step\":" + String(s.step_hz) + ",";
  j += "\"rmode\":\"" + json_escape(s.mode) + "\",\"rmode_index\":" + String(s.mode_index) + ",";
  j += "\"rssi\":" + String(s.rssi) + ",\"snr\":" + String(s.snr) + ",";
  j += "\"stereo\":" + String(s.stereo ? "true" : "false") + ",\"valid\":" + String(s.valid ? "true" : "false") + ",";
  j += "\"seeking\":" + String(s.seeking ? "true" : "false") + ",\"tuning\":" + String(s.tuning ? "true" : "false") + ",";
  j += "\"failed\":" + String(s.failed ? "true" : "false") + ",\"err\":\"" + json_escape(s.error) + "\",";
  j += "\"vol\":" + String(s.volume) + ",\"mute\":" + String(s.muted ? "true" : "false") + ",";
  j += "\"ps\":\"" + json_escape(s.ps) + "\",\"rt\":\"" + json_escape(s.text) + "\",";
  j += "\"bat\":" + String(s.battery_volts, 2) + ",\"batp\":" + String(s.battery_percent) + ",";
  if (s.environment_valid)
    j += "\"temp\":" + String(s.temperature_milli / 1000.0f, 1) + ",\"hum\":" + String(s.humidity_milli / 1000.0f, 1) + ",";
  j += "\"tz\":" + String(net_config.utc_offset) + ",\"rfwifi\":" + String(net_config.rf_wifi ? "true" : "false") + ",";
  j += "\"autoupd\":" + String(net_config.auto_update_check ? "true" : "false") + ",";
  j += "\"heap\":" + String(ESP.getFreeHeap()) + ",\"bands\":[";
  for (int i = 0; i < rf_remote_band_count(); ++i) {
    bool available; uint32_t lo, hi;
    const char* name = rf_remote_band_name(i, &available, &lo, &hi);
    if (i) j += ",";
    j += "{\"n\":\"" + json_escape(name) + "\",\"a\":" + (available ? "true" : "false") + ",\"lo\":" + String(lo) + ",\"hi\":" + String(hi) + "}";
  }
  j += "],\"update\":" + updater_json() + "}";
  return j;
}

void rfweb_begin(void) {
  net_config_load();
  if (!net_config.rf_wifi || !net_config.ssid.length()) {
    Serial.println("CX8ANM: RF web off (disabled or no WiFi saved)");
    return;
  }
  // The receiver's frame buffers are already allocated; WiFi needs roughly 60 KB more.
  if (ESP.getFreeHeap() < 70000) {
    Serial.printf("CX8ANM: RF web off, free heap %u\n", ESP.getFreeHeap());
    rf_remote_notice("WiFi off: low memory", 6000);
    return;
  }
  WiFi.persistent(false);
  WiFi.setHostname("cx8anm");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(net_config.ssid.c_str(), net_config.password.c_str());
  phase = RF_WEB_CONNECTING;
  started = last_attempt = millis();
}

void rfweb_update(void) {
  if (phase == RF_WEB_OFF) return;
  const uint32_t now = millis();
  if (phase == RF_WEB_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      phase = RF_WEB_ONLINE;
      WiFi.setSleep(true); // Modem sleep: less 2.4 GHz activity while receiving.
      if (MDNS.begin("cx8anm")) MDNS.addService("http", "tcp", 80);
      sntp_set_time_sync_notification_cb(on_time_sync);
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      if (!web_started) { webui_begin(false, WEBUI_RF); web_started = true; }
      const String text = "WiFi " + WiFi.localIP().toString() + "  cx8anm.local";
      rf_remote_notice(text.c_str(), 10000);
      Serial.printf("CX8ANM: RF web at %s, heap %u\n", WiFi.localIP().toString().c_str(), ESP.getFreeHeap());
    } else if (now - last_attempt > 60000) { // Keep trying quietly once a minute.
      last_attempt = now;
      WiFi.disconnect();
      WiFi.begin(net_config.ssid.c_str(), net_config.password.c_str());
    }
    return;
  }
  if (ntp_synced && !clock_set) {
    clock_set = true;
    rf_remote_set_clock((int64_t)time(nullptr), net_config.utc_offset);
  }
  webui_update();
}
