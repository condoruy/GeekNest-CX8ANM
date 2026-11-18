#ifndef CX8_BT_FIRMWARE // Main image only (the Bluetooth image has no WiFi)
// CX8ANM internet radio application for the GeekNest V5A.
#include "netradio.h"
#include "actions.h"
#include "config.h"
#include "player.h"
#include "screen.h"
#include "webui.h"
#include "updater.h"
#include "ui/splash.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <nvs.h>
#include <sys/time.h>
#include <time.h>
#include <esp_sntp.h>
extern "C" {
#include "app/boot_mode.h"
#include "app/power.h"
#include "boards/board.h"
}

namespace {
// Vendor yoRadio mapping for the V5A I2S DAC.
constexpr int I2S_BCLK = 25, I2S_LRCK = 27, I2S_DOUT = 26;
constexpr char AP_SSID[] = "CX8ANM-Radio";
constexpr char AP_PASSWORD[] = "cx8anm73";
constexpr char HOSTNAME[] = "cx8anm";
constexpr uint32_t LONG_PRESS_MS = 650, POWER_OFF_MS = 3000, BROWSE_TIMEOUT_MS = 8000;
constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;

enum net_phase_t { PHASE_CONNECTING, PHASE_ONLINE, PHASE_ACCESS_POINT };
net_phase_t phase = PHASE_CONNECTING;
uint32_t connect_started = 0, last_connect_draw = 0;

bool want_playing = false;   // The user wants sound (retry on errors)
bool muted = false;
uint32_t retry_at = 0, retry_delay = 5000;
net_overlay_t overlay = OVERLAY_NONE;
int browse_index = 0;
uint32_t overlay_touched = 0;
String notice;
uint32_t notice_until = 0;
bool state_dirty = false;
uint32_t state_changed = 0;
hal_battery_reading_t battery = {};
uint32_t last_battery = 0, last_draw = 0;
bool clock_stored = false;
volatile bool ntp_synced = false;
void on_time_sync(struct timeval*) { ntp_synced = true; }
String names[NET_MAX_STATIONS];
// Updates
bool install_requested = false, install_pending = false, update_announced = false;
bool resume_after_check = false; uint32_t check_at = 0;
int bt_install_sub = -1;   // >= 0: install the Bluetooth image and switch to it
uint32_t online_at = 0, last_update_check = 0;

// Encoder push and side keys
bool previous_pressed = false, held = false, power_pending = false, long_pending = false;
uint32_t pressed_at = 0;
int key_a_repeats = 0;

void show_notice(const String& text, uint32_t ms = 2500) { notice = text; notice_until = millis() + ms; }
void touch_state() { state_dirty = true; state_changed = millis(); }
void refresh_names() { for (int i = 0; i < net_config.count; ++i) names[i] = net_config.stations[i].name; }
void apply_volume() { player_set_volume(muted ? 0 : net_config.volume); }

void play_current() {
  if (!net_config.count || phase != PHASE_ONLINE) return;
  if (net_config.current >= net_config.count) net_config.current = 0;
  want_playing = true;
  retry_delay = 5000; retry_at = 0;
  player_play(net_config.stations[net_config.current].url.c_str());
  touch_state();
}

// --- Time ---------------------------------------------------------------
void apply_timezone() {
  const int m = net_config.utc_offset; // local = UTC + m
  char tz[24];
  snprintf(tz, sizeof tz, "CX%c%d:%02d", m <= 0 ? '+' : '-', abs(m) / 60, abs(m) % 60);
  setenv("TZ", tz, 1);
  tzset();
}
void time_from_rtc() {
  int64_t utc;
  if (!board_rtc_read(&utc)) return;
  timeval tv = {(time_t)utc, 0};
  settimeofday(&tv, nullptr);
}
// Shares the NTP time with the RF receiver: UTC in the RTC, offset in its NVS.
void store_offset_for_receiver() {
  nvs_handle_t h;
  if (nvs_open(BOARD_STORAGE_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
    const int16_t m = net_config.utc_offset;
    const uint8_t bytes[2] = {(uint8_t)m, (uint8_t)((uint16_t)m >> 8)};
    nvs_set_blob(h, "utcoffset", bytes, sizeof bytes);
    nvs_commit(h);
    nvs_close(h);
  }
}
void store_clock_for_receiver() {
  const time_t now = time(nullptr);
  if (now < 1700000000) return;
  board_rtc_write((int64_t)now);
  store_offset_for_receiver();
  clock_stored = true;
  Serial.println("CX8ANM: RTC set from NTP");
}

// --- WiFi -----------------------------------------------------------------
void start_access_point(const char* reason) {
  phase = PHASE_ACCESS_POINT;
  // Stop station-mode retries: channel hopping would drop the phone from the AP.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(100);
  webui_begin(true, WEBUI_NET);
  screen_setup(AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str(), reason);
  Serial.printf("CX8ANM: access point %s at %s (%s)\n", AP_SSID, WiFi.softAPIP().toString().c_str(), reason);
}
void go_online() {
  phase = PHASE_ONLINE;
  WiFi.setSleep(false); // Modem sleep causes audio drop-outs.
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  sntp_set_time_sync_notification_cb(on_time_sync);
  configTime(0, 0, "pool.ntp.org", "time.google.com", "uy.pool.ntp.org");
  apply_timezone(); // configTime() sets TZ to UTC; restore the local zone
  webui_begin(false, WEBUI_NET);
  screen_invalidate();
  Serial.printf("CX8ANM: online %s IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  online_at = millis();
  bt_sub_t pending_sub;
  if (boot_mode_take_bt_install(&pending_sub)) {
    bt_install_sub = (int)pending_sub;
    show_notice("Preparando el modo Bluetooth...", 20000);
    if (!updater_check_async()) { bt_install_sub = -1; show_notice(updater.error, 6000); }
    online_at = millis();
    return; // No autoplay: the radio is about to switch
  }
  install_pending = updater_take_pending(); // Requested from the RF web page
  if (install_pending && !updater_check_async()) {
    install_pending = false;
    show_notice(updater.error, 6000);
  }
  if (install_pending) show_notice("Buscando la actualización en GitHub...", 20000);
  else if (net_config.auto_update_check && updater_check_async()) {
    last_update_check = millis();            // Check first, while memory is free; then play
    resume_after_check = net_config.autoplay;
  } else if (net_config.autoplay) play_current();
}

int wifi_bars() {
  if (phase == PHASE_ACCESS_POINT || WiFi.status() != WL_CONNECTED) return -1;
  const int r = WiFi.RSSI();
  return r > -55 ? 4 : r > -65 ? 3 : r > -75 ? 2 : r > -85 ? 1 : 0;
}

// --- Mode changes -----------------------------------------------------------
void save_now() { net_config_save_state(); state_dirty = false; }
void shutdown_audio() {
  player_stop();
  delay(250);
}
[[noreturn]] void to_rf() {
  save_now();
  shutdown_audio();
  cx8_message("Receptor RF", "Reiniciando en modo SI4732...");
  WiFi.disconnect(true);
  boot_mode_switch(BOOT_MODE_RF);
}
[[noreturn]] void power_off() {
  save_now();
  shutdown_audio();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("CX8ANM: power off");
  board_power_off();
}

void progress_screen(int percent) { screen_progress("Actualizando firmware", percent); }
[[noreturn]] void install_update() {
  save_now();
  shutdown_audio();
  progress_screen(0);
  if (updater_install(progress_screen)) {
    cx8_message("Firmware actualizado", ("Version " + updater.latest + " - reiniciando...").c_str());
    delay(1500);
    boot_mode_restart();
  }
  cx8_message("No se pudo actualizar", updater.error.c_str());
  delay(4000);
  boot_mode_restart(); // Back to a clean state with the current firmware
}

void bt_progress_screen(int percent) { screen_progress("Instalando modo Bluetooth", percent); }
[[noreturn]] void go_bluetooth(int sub) {
  save_now();
  shutdown_audio();
  boot_mode_set_bt_sub(sub == 1 ? BT_SUB_TRANSMITTER : BT_SUB_SPEAKER);
  cx8_message(sub == 1 ? "Transmisor Bluetooth" : "Parlante Bluetooth", "Reiniciando en modo Bluetooth...");
  WiFi.disconnect(true);
  boot_mode_switch(BOOT_MODE_BT);
}
void install_bt_and_go(int sub) {
  save_now();
  shutdown_audio();
  bt_progress_screen(0);
  if (updater_install_bt(bt_progress_screen) && boot_mode_bt_installed()) go_bluetooth(sub);
  cx8_message("No se pudo instalar Bluetooth", updater.error.c_str());
  delay(4000);
  boot_mode_restart();
}

// --- Input ------------------------------------------------------------------
void open_browse() {
  overlay = OVERLAY_BROWSE;
  browse_index = net_config.current;
  overlay_touched = millis();
}
void choose_browsed() {
  overlay = OVERLAY_NONE;
  net_config.current = browse_index;
  touch_state();
  play_current();
}
void short_press() {
  if (phase == PHASE_ACCESS_POINT) {
    if (net_config.ssid.length()) { cx8_message("Reintentando WiFi", net_config.ssid.c_str()); delay(400); boot_mode_restart(); }
    return;
  }
  if (phase != PHASE_ONLINE) return;
  if (overlay == OVERLAY_BROWSE) choose_browsed();
  else if (overlay == OVERLAY_INFO) overlay = OVERLAY_NONE;
  else if (net_config.count) open_browse();
}
void toggle_mute() {
  muted = !muted;
  apply_volume();
  show_notice(muted ? "Sonido silenciado" : "Sonido activado", 1500);
}
void button_input(uint32_t now) {
  board_button_update(now);
  const bool pressed = board_button_pressed();
  if (pressed && !previous_pressed) {
    pressed_at = now; held = long_pending = power_pending = false;
    if (power_wake()) { held = true; screen_invalidate(); } // First press only wakes the screen
  }
  if (pressed && !held) {
    if (now - pressed_at >= POWER_OFF_MS) {
      if (!power_pending) { power_pending = true; show_notice("Soltá la rueda para apagar", 10000); }
    } else if (now - pressed_at >= LONG_PRESS_MS) long_pending = true;
  }
  if (!pressed && previous_pressed && !held) {
    if (power_pending) power_off();
    else if (long_pending) toggle_mute();
    else short_press();
  }
  if (!pressed) long_pending = power_pending = false;
  previous_pressed = pressed;
}
void key_input(uint32_t now) {
  enum { SWITCH_REPEATS = 9 };
  uint8_t repeats;
  const uint8_t pressed = board_keys_read(now, &repeats);
  if (pressed && power_wake()) { screen_invalidate(); return; } // First key only wakes the screen
  if (repeats) power_wake();
  if (pressed & BOARD_KEY_A) {
    key_a_repeats = 0;
    overlay = overlay == OVERLAY_NONE ? OVERLAY_INFO : OVERLAY_NONE;
  }
  const bool both = (repeats & BOARD_KEY_B) && (repeats & BOARD_KEY_C);
  if (both) {
    if (++key_a_repeats == 3) show_notice("Seguí apretando B y C para el modo Bluetooth", 2000);
    if (key_a_repeats >= SWITCH_REPEATS) { key_a_repeats = 0; net_action_bluetooth((int)boot_mode_bt_sub()); }
    return;
  }
  if (pressed & (BOARD_KEY_B | BOARD_KEY_C)) key_a_repeats = 0;
  if (repeats & BOARD_KEY_A) {
    if (++key_a_repeats == 3) show_notice("Seguí apretando A para pasar al receptor RF", 2000);
    if (key_a_repeats >= SWITCH_REPEATS) to_rf();
  }
  const uint8_t vol = (pressed | repeats) & (BOARD_KEY_B | BOARD_KEY_C);
  if (vol && overlay == OVERLAY_BROWSE && net_config.count) { // B/C move through the list
    browse_index = ((browse_index + (vol & BOARD_KEY_B ? -1 : 1)) % net_config.count + net_config.count) % net_config.count;
    overlay_touched = millis();
  } else if (vol) net_action_volume(net_config.volume + (vol & BOARD_KEY_B ? 1 : -1));
}
void encoder_input() {
  const int delta = board_encoder_read();
  if (delta && power_wake()) { screen_invalidate(); return; }
  if (!delta || !net_config.count) return;
  if (overlay != OVERLAY_BROWSE) { open_browse(); }
  browse_index = ((browse_index + delta) % net_config.count + net_config.count) % net_config.count;
  overlay_touched = millis();
}
void serial_input() {
  while (Serial.available()) {
    const int c = Serial.read();
    if (c == 'R') to_rf();
    else if (c == '+') net_action_volume(net_config.volume + 1);
    else if (c == '-') net_action_volume(net_config.volume - 1);
    else if (c == 'n') net_action_step(1);
    else if (c == 'p') net_action_step(-1);
    else if (c == 's') net_action_stop();
    else if (c == '?') Serial.println(net_status_json());
  }
}

// --- Drawing ----------------------------------------------------------------
String json_escape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}
String clock_text() {
  const time_t now = time(nullptr);
  if (now < 1700000000) return "";
  tm t;
  localtime_r(&now, &t);
  char text[8];
  snprintf(text, sizeof text, "%02d:%02d", t.tm_hour, t.tm_min);
  return text;
}
void draw(uint32_t now) {
  net_view_t v;
  v.clock = clock_text();
  v.wifi_bars = wifi_bars();
  v.battery_percent = battery.plausible ? battery.estimated_percent : -1;
  v.battery_volts = battery.volts;
  v.index = net_config.current;
  v.count = net_config.count;
  v.station = net_config.count ? net_config.stations[net_config.current].name : String("");
  player_get_status(&v.player);
  v.muted = muted;
  v.volume = net_config.volume;
  if (WiFi.status() == WL_CONNECTED)
    v.footer = "http://" + WiFi.localIP().toString() + "   o   http://cx8anm.local";
  else v.footer = "Sin WiFi - reconectando...";
  if (notice.length() && now > notice_until) notice = "";
  v.notice = notice;
  v.overlay = overlay;
  v.browse_index = browse_index;
  v.names = names;
  if (overlay == OVERLAY_INFO) {
    v.info_lines[0] = "Red: " + WiFi.SSID() + "  (" + String(WiFi.RSSI()) + " dBm)";
    v.info_lines[1] = "IP: " + WiFi.localIP().toString() + "   cx8anm.local";
    v.info_lines[2] = "Batería: " + String(battery.volts, 2) + " V";
    v.info_lines[3] = "Memoria libre: " + String(ESP.getFreeHeap() / 1024) + " KB (min " + String(ESP.getMinFreeHeap() / 1024) + ")";
    v.info_lines[4] = "Rueda: elegir emisora - pulsar: escuchar";
    v.info_lines[5] = "B/C: volumen - rueda larga: mudo - 3 s: apagar";
    v.info_lines[6] = "A 2 seg: receptor RF - firmware " CX8_FIRMWARE_VERSION;
  }
  screen_draw(v);
}
} // namespace

// --- Actions (web page and serial) -----------------------------------------------
void net_action_play(int index) {
  if (index < 0 || index >= net_config.count) return;
  net_config.current = index;
  overlay = OVERLAY_NONE;
  play_current();
}
void net_action_stop(void) { want_playing = false; player_stop(); }
void net_action_step(int delta) {
  if (!net_config.count) return;
  net_action_play(((net_config.current + delta) % net_config.count + net_config.count) % net_config.count);
}
void net_action_volume(int volume) {
  net_config.volume = (uint8_t)constrain(volume, 0, 21);
  if (muted && volume > 0) muted = false;
  apply_volume();
  touch_state();
}
void net_action_mute(bool value) { if (value != muted) toggle_mute(); }
void net_action_brightness(int value) {
  net_config.brightness = (uint8_t)constrain(value, 16, 255);
  power_set_brightness(net_config.brightness);
  touch_state();
}
void net_action_timezone(int minutes) {
  if (minutes < -720 || minutes > 840 || minutes % 30) return;
  net_config.utc_offset = (int16_t)minutes;
  apply_timezone();
  store_offset_for_receiver(); // The RF receiver shows the same local time
  touch_state();
}
void net_action_stations_changed(void) {
  refresh_names();
  if (net_config.current >= net_config.count) net_config.current = 0;
  touch_state();
  show_notice("Lista de emisoras actualizada", 2000);
}
void net_action_wifi_saved(void) {
  save_now();
  shutdown_audio();
  cx8_message("WiFi guardado", "Reiniciando para conectar...");
  delay(500);
  boot_mode_restart();
}
void net_action_switch_rf(void) { to_rf(); }
void net_action_update_start(void) { install_requested = true; }
void net_action_bluetooth(int sub) {
  if (boot_mode_bt_installed()) go_bluetooth(sub);
  if (phase != PHASE_ONLINE) { show_notice("Sin WiFi: no se puede descargar el modo Bluetooth", 5000); return; }
  // Download needs RAM for HTTPS: stop the stream, check the manifest, install.
  want_playing = false;
  player_stop();
  bt_install_sub = sub;
  updater.checked_at = 0;
  show_notice("Descargando el modo Bluetooth...", 20000);
  check_at = millis() + 1500;
  resume_after_check = false;
}
void net_action_update_check(void) {
  if (updater.checking || check_at) return;
  if (ESP.getMaxAllocHeap() >= 40000 && ESP.getFreeHeap() >= 60000 && updater_check_async()) return;
  // Not enough RAM next to the stream: pause it, check, resume afterwards.
  resume_after_check = want_playing;
  want_playing = false;
  player_stop();
  show_notice("Buscando actualización...", 4000);
  check_at = millis() + 1500;
}
bool net_is_access_point(void) { return phase == PHASE_ACCESS_POINT; }
String net_status_json(void) {
  player_status_t p;
  player_get_status(&p);
  static const char* states[] = {"IDLE", "CONNECTING", "PLAYING", "ERROR"};
  String j = "{";
  j += "\"mode\":\"net\",\"ver\":\"" CX8_FIRMWARE_VERSION "\",";
  j += "\"ap\":" + String(phase == PHASE_ACCESS_POINT ? "true" : "false") + ",";
  j += "\"ssid\":\"" + json_escape(WiFi.SSID()) + "\",";
  j += "\"ip\":\"" + (phase == PHASE_ACCESS_POINT ? WiFi.softAPIP() : WiFi.localIP()).toString() + "\",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"idx\":" + String(net_config.current) + ",\"count\":" + String(net_config.count) + ",";
  j += "\"name\":\"" + json_escape(net_config.count ? net_config.stations[net_config.current].name : String("")) + "\",";
  j += "\"title\":\"" + json_escape(p.title) + "\",\"icy\":\"" + json_escape(p.icy_name) + "\",";
  j += "\"state\":\"" + String(states[p.state]) + "\",\"err\":\"" + json_escape(p.error) + "\",";
  j += "\"codec\":\"" + json_escape(p.codec) + "\",\"br\":" + String(p.bitrate) + ",";
  j += "\"vol\":" + String(net_config.volume) + ",\"mute\":" + String(muted ? "true" : "false") + ",";
  j += "\"bri\":" + String(net_config.brightness) + ",\"tz\":" + String(net_config.utc_offset) + ",";
  j += "\"rfwifi\":" + String(net_config.rf_wifi ? "true" : "false") + ",\"autoupd\":" + String(net_config.auto_update_check ? "true" : "false") + ",\"encinv\":" + String(net_config.encoder_inverted ? "true" : "false") + ",";
  j += "\"bat\":" + String(battery.volts, 2) + ",\"heap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"dim\":" + String(power_dim_seconds()) + ",\"off\":" + String(power_off_seconds()) + ",\"sleep\":" + String(power_sleep_minutes_left()) + ",\"rfwoff\":" + String(net_config.rf_wifi_off_min) + ",";
  j += "\"update\":" + updater_json();
  j += "}";
  return j;
}

// --- Application --------------------------------------------------------------
void netradio_begin(void) {
  Serial.begin(115200);
  board_begin_net();
  net_config_load();
  refresh_names();
  apply_timezone();
  time_from_rtc();
  const bool lcd = board_display_begin();
  if (lcd) {
    screen_begin(net_config.brightness);
    power_begin(net_config.brightness);
    cx8_splash("Radio Internet", "Tecla A 2 seg: Receptor RF");
  }
  battery = board_battery_read();
  player_begin(I2S_BCLK, I2S_LRCK, I2S_DOUT);
  apply_volume();
  delay(1200);
  // A key still held from power-on must be released before it acts.
  previous_pressed = held = board_button_pressed();
  WiFi.persistent(false);
  WiFi.setHostname(HOSTNAME);
  if (!net_config.ssid.length()) {
    start_access_point("Todavía no hay una red WiFi configurada");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(net_config.ssid.c_str(), net_config.password.c_str());
  connect_started = millis();
  phase = PHASE_CONNECTING;
}

void netradio_update(void) {
  const uint32_t now = millis();
  button_input(now);
  key_input(now);
  serial_input();

  if (phase != PHASE_ONLINE) board_encoder_read(); // Discard turns until there is something to browse.
  if (phase == PHASE_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) go_online();
    else if (now - connect_started > CONNECT_TIMEOUT_MS) {
      const String reason = "No se pudo conectar a " + net_config.ssid;
      start_access_point(reason.c_str());
    } else if (now - last_connect_draw >= 1000) {
      last_connect_draw = now;
      screen_connecting(net_config.ssid.c_str(), (now - connect_started) / 1000);
    }
    delay(5);
    return;
  }
  webui_update();
  if (phase == PHASE_ACCESS_POINT) { delay(2); return; }

  encoder_input();
  if (overlay == OVERLAY_BROWSE && now - overlay_touched > BROWSE_TIMEOUT_MS) overlay = OVERLAY_NONE;

  // Keep the stream alive: retry with back-off after errors or WiFi loss.
  player_status_t p;
  player_get_status(&p);
  if (want_playing && p.state == PLAYER_ERROR && WiFi.status() == WL_CONNECTED) {
    if (!retry_at) retry_at = now + retry_delay;
    else if ((int32_t)(now - retry_at) >= 0) {
      Serial.println("CX8ANM: retrying stream");
      const uint32_t next = retry_delay * 2 > 60000 ? 60000 : retry_delay * 2;
      play_current();
      retry_delay = next;
    }
  }
  if (p.state == PLAYER_PLAYING) { retry_at = 0; retry_delay = 5000; }

  // Bluetooth image: install (after the manifest check) and switch.
  if (bt_install_sub >= 0 && !updater.checking && updater.checked_at) {
    const int sub = bt_install_sub;
    bt_install_sub = -1;
    if (updater.bt_url.length()) install_bt_and_go(sub);
    show_notice(updater.error.length() ? updater.error : String("No hay modo Bluetooth publicado"), 6000);
  }
  // Updates: requested from the web page, handed over from RF mode, or periodic checks.
  if (install_requested) install_update();
  if (install_pending && !updater.checking && updater.checked_at) {
    install_pending = false;
    if (updater.available) install_update();
    show_notice(updater.error.length() ? updater.error : String("Ya tenés la última versión"), 5000);
    if (net_config.autoplay) play_current();
  }
  // A check that needs the stream paused (HTTPS needs RAM), then resume.
  if (check_at && (int32_t)(now - check_at) >= 0) {
    check_at = 0;
    if (!updater_check_async()) {
      show_notice(updater.error, 5000);
      bt_install_sub = -1;
      if (resume_after_check) { resume_after_check = false; play_current(); }
    }
  }
  if (resume_after_check && !updater.checking && !check_at && !install_requested) {
    resume_after_check = false;
    play_current();
  }
  if (net_config.auto_update_check && !updater.checking && !install_pending && !resume_after_check &&
      ESP.getMaxAllocHeap() >= 40000 &&
      ((!last_update_check && now - online_at > 30000) || (last_update_check && now - last_update_check > 12UL * 3600 * 1000))) {
    last_update_check = now;
    updater_check_async();
  }
  if (updater.available && !update_announced) {
    update_announced = true;
    show_notice("Nueva versión " + updater.latest + " disponible: actualizá desde la página web", 10000);
  }
  if (!clock_stored && ntp_synced) store_clock_for_receiver();
  if (state_dirty && now - state_changed > 3000) save_now();
  if (now - last_battery >= 5000) { last_battery = now; battery = board_battery_read(); }
  // Battery saving: CPU at 80 MHz and the I2S amplifier off whenever nothing plays.
  static int cpu_mhz = 0;
  const int want_mhz = want_playing ? 240 : 80;
  if (want_mhz != cpu_mhz) { setCpuFrequencyMhz(want_mhz); cpu_mhz = want_mhz; }
  static int amp = -1;
  const int want_amp = want_playing && !muted && net_config.volume > 0;
  if (want_amp != amp) { digitalWrite(2, want_amp ? HIGH : LOW); amp = want_amp; }
  switch (power_update(now, battery.volts, battery.plausible)) {
    case POWER_EVENT_LOW_BATTERY: power_wake(); screen_invalidate(); show_notice("Batería baja: cargá la radio pronto", 8000); break;
    case POWER_EVENT_SHUTDOWN: Serial.println("CX8ANM: battery critical"); power_off();
    case POWER_EVENT_SLEEP_TIMER: Serial.println("CX8ANM: sleep timer"); power_off();
    default: break;
  }
  static uint32_t last_heap_log = 0;
  if (now - last_heap_log >= 10000) {
    last_heap_log = now;
    Serial.printf("CX8ANM: heap libre=%u minimo=%u bloque=%u wifi=%d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  ESP.getMaxAllocHeap(), WiFi.status() == WL_CONNECTED);
  }
  if (now - last_draw >= 100 && power_screen_on()) { last_draw = now; draw(now); }
  delay(2);
}

#endif // CX8_BT_FIRMWARE
