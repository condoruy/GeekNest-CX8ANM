#ifdef CX8_BT_FIRMWARE
#include "btapp.h"
#include <Arduino.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include "BluetoothA2DPSink.h"
#include "BluetoothA2DPSource.h"
#include "boards/display.hpp"
#include "netradio/fonts_cx8.h"
#include "ui/splash.h"
extern "C" {
#include "app/boot_mode.h"
#include "app/power.h"
#include "app/receiver.h"
#include "app/rf_remote.h"
#include "boards/board.h"
}

// The A2DP library uses the core's btStart(), which links esp32-hal-bt.c and its
// btInUse() == true, so Arduino keeps the Bluetooth controller memory at boot.

namespace {
constexpr char BT_NAME[] = "CX8ANM-Radio";
constexpr int I2S_BCLK = 25, I2S_LRCK = 27, I2S_DOUT = 26, PIN_DAC_ENABLE = 2;
constexpr uint32_t LONG_PRESS_MS = 650, POWER_OFF_MS = 3000;

bt_sub_t sub = BT_SUB_SPEAKER;

// Settings shared with the main image (NVS "cx8net").
void load_shared_settings() {
  Preferences p;
  p.begin("cx8net", true);
  board_encoder_set_inverted(p.getBool("encinv", true));
  power_configure(p.getUShort("dim", 30), p.getUShort("off", 120));
  p.end();
}

// =====================================================================
// Parlante Bluetooth (A2DP sink)
BluetoothA2DPSink* sink = nullptr;
volatile bool connected = false, playing = false, meta_dirty = true;
char peer[48] = "", title[96] = "", artist[64] = "";
portMUX_TYPE meta_lock = portMUX_INITIALIZER_UNLOCKED;
int volume = 80;                 // 0..127
bool muted = false;
hal_battery_reading_t battery = {};
uint32_t last_battery = 0, last_draw = 0;
bool prev_pressed = false, held = false, long_pending = false, power_pending = false;
uint32_t pressed_at = 0;
int a_repeats = 0;
String notice;
uint32_t notice_until = 0;

constexpr uint16_t BG = 0x0861, PANEL = 0x10C3, BORDER = 0x2945, TEXT = 0xE73C, DIM = 0x8C71,
                   ACCENT = 0xFD20, BLUE = 0x2D7F, OK = 0x3666, BAD = 0xF208;

void copy_text(char* dst, size_t size, const uint8_t* src) {
  portENTER_CRITICAL(&meta_lock);
  strncpy(dst, src ? (const char*)src : "", size - 1);
  dst[size - 1] = 0;
  meta_dirty = true;
  portEXIT_CRITICAL(&meta_lock);
}
void on_metadata(uint8_t id, const uint8_t* text) {
  if (id == ESP_AVRC_MD_ATTR_TITLE) copy_text(title, sizeof title, text);
  else if (id == ESP_AVRC_MD_ATTR_ARTIST) copy_text(artist, sizeof artist, text);
}
void on_connection(esp_a2d_connection_state_t state, void*) {
  connected = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
  if (!connected) { playing = false; title[0] = artist[0] = 0; }
  else if (sink) { const char* n = sink->get_peer_name(); copy_text(peer, sizeof peer, (const uint8_t*)(n ? n : "")); }
  meta_dirty = true;
}
void on_audio_state(esp_a2d_audio_state_t state, void*) {
  playing = state == ESP_A2D_AUDIO_STATE_STARTED;
  meta_dirty = true;
}
void on_remote_volume(int v) { volume = v; meta_dirty = true; }

void show_notice(const String& text, uint32_t ms = 2000) { notice = text; notice_until = millis() + ms; meta_dirty = true; }

String fit(lgfx::LGFX_Device& g, const String& text, int width) {
  if (g.textWidth(text) <= width) return text;
  String s = text;
  while (s.length()) {
    int cut = s.length() - 1;
    while (cut > 0 && ((uint8_t)s[cut] & 0xC0) == 0x80) --cut;
    s = s.substring(0, cut);
    if (g.textWidth(s + "...") <= width) return s + "...";
  }
  return "...";
}

void bt_icon(lgfx::LGFX_Device& g, int x, int y, uint16_t c) {
  // Stylised Bluetooth rune, 20 x 34 px
  g.drawLine(x, y + 8, x + 18, y + 24, c);     g.drawLine(x + 1, y + 8, x + 19, y + 24, c);
  g.drawLine(x, y + 26, x + 18, y + 10, c);    g.drawLine(x + 1, y + 26, x + 19, y + 10, c);
  g.drawLine(x + 9, y, x + 9, y + 34, c);      g.drawLine(x + 10, y, x + 10, y + 34, c);
  g.drawLine(x + 9, y, x + 18, y + 9, c);      g.drawLine(x + 10, y, x + 19, y + 9, c);
  g.drawLine(x + 9, y + 34, x + 18, y + 25, c); g.drawLine(x + 10, y + 34, x + 19, y + 25, c);
}

void draw_speaker() {
  auto& g = board_display_device();
  char p[48], t[96], a[64];
  portENTER_CRITICAL(&meta_lock);
  memcpy(p, peer, sizeof p); memcpy(t, title, sizeof t); memcpy(a, artist, sizeof a);
  meta_dirty = false;
  portEXIT_CRITICAL(&meta_lock);
  g.startWrite();
  g.fillScreen(BG);
  g.fillRect(0, 0, 320, 26, PANEL);
  g.drawFastHLine(0, 26, 320, BORDER);
  g.setTextDatum(lgfx::middle_left);
  g.setFont(&fonts::FreeSansBold9pt7b);
  g.setTextColor(ACCENT, PANEL);
  g.drawString("CX8ANM", 8, 13);
  g.setFont(&cx8_sans12);
  g.setTextColor(DIM, PANEL);
  g.drawString("PARLANTE BLUETOOTH", 88, 13);
  if (battery.plausible) {
    g.setTextDatum(lgfx::middle_right);
    g.setTextColor(battery.estimated_percent > 15 ? TEXT : BAD, PANEL);
    g.drawString(String(battery.estimated_percent) + "%", 312, 13);
  }
  bt_icon(g, 16, 40, connected ? BLUE : DIM);
  g.setTextDatum(lgfx::top_left);
  g.setFont(&cx8_sans24b);
  g.setTextColor(TEXT, BG);
  g.drawString(connected ? fit(g, String(p[0] ? p : "Conectado"), 260) : String("Esperando..."), 50, 40);
  g.setFont(&cx8_sans12);
  g.setTextColor(connected ? (playing ? OK : DIM) : DIM, BG);
  g.drawString(connected ? (playing ? "Reproduciendo" : "En pausa") : String("Buscá \"") + BT_NAME + "\" en el celular", 50, 66);
  g.fillRoundRect(8, 90, 304, 82, 6, PANEL);
  g.drawRoundRect(8, 90, 304, 82, 6, BORDER);
  g.setFont(&cx8_sans16b);
  g.setTextColor(TEXT, PANEL);
  g.drawString(fit(g, String(t[0] ? t : (connected ? "-" : "Sin conexión")), 284), 18, 102);
  g.setFont(&cx8_sans16);
  g.setTextColor(DIM, PANEL);
  g.drawString(fit(g, String(a), 284), 18, 128);
  if (notice.length() && millis() < notice_until) {
    g.setTextColor(ACCENT, PANEL);
    g.drawString(fit(g, notice, 284), 18, 150);
  }
  g.setFont(&cx8_sans12);
  g.setTextColor(DIM, BG);
  g.drawString("VOL", 10, 184);
  const int level = muted ? 0 : volume * 21 / 127;
  for (int i = 0; i < 21; ++i)
    g.fillRect(40 + i * 12, 182 + (20 - i) / 4, 10, 14 - (20 - i) / 4, i < level ? (i < 14 ? BLUE : ACCENT) : BORDER);
  g.setTextDatum(lgfx::top_right);
  g.setTextColor(muted ? BAD : TEXT, BG);
  g.drawString(muted ? "MUDO" : String(volume * 100 / 127) + "%", 312, 184);
  g.fillRect(0, 210, 320, 30, PANEL);
  g.drawFastHLine(0, 210, 320, BORDER);
  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(DIM, PANEL);
  g.drawString("Rueda: volumen/pausa  B/C: tema  A: transmisor  A 2s: RF", 160, 225);
  g.endWrite();
}

void apply_volume() { if (sink) sink->set_volume(muted ? 0 : volume); meta_dirty = true; }

[[noreturn]] void speaker_power_off() {
  if (sink) sink->end(false);
  boot_mode_power_off_prepare();
  digitalWrite(PIN_DAC_ENABLE, LOW);
  board_power_off();
  for (;;) {}
}

void speaker_begin() {
  board_begin_net();             // Power latch, inputs, battery; XL9555 released, SI4732 idle
  pinMode(PIN_DAC_ENABLE, OUTPUT);
  digitalWrite(PIN_DAC_ENABLE, LOW);
  load_shared_settings();
  Preferences p;
  p.begin("cx8bt", true);
  volume = p.getUChar("vol", 80);
  p.end();
  if (board_display_begin()) {
    board_display_set_brightness(160);
    cx8_splash("Parlante Bluetooth", "Busca CX8ANM-Radio en el celular");
    power_begin(160);
  }
  battery = board_battery_read();
  sink = new BluetoothA2DPSink();
  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_BCLK; pins.ws_io_num = I2S_LRCK; pins.data_out_num = I2S_DOUT; pins.data_in_num = I2S_PIN_NO_CHANGE;
  sink->set_pin_config(pins);
  sink->set_avrc_metadata_callback(on_metadata);
  sink->set_on_connection_state_changed(on_connection);
  sink->set_on_audio_state_changed(on_audio_state);
  sink->set_avrc_rn_volumechange(on_remote_volume);
  sink->set_auto_reconnect(true);
  sink->start(BT_NAME);
  sink->set_volume(volume);
  delay(900);
  prev_pressed = held = board_button_pressed();
  draw_speaker();
}

void save_volume() {
  Preferences p;
  p.begin("cx8bt", false);
  p.putUChar("vol", (uint8_t)volume);
  p.end();
}

void speaker_update() {
  const uint32_t now = millis();
  // Knob: volume
  const int delta = board_encoder_read();
  if (delta && !power_wake()) { volume = constrain(volume + delta * 4, 0, 127); muted = false; apply_volume(); }
  else if (delta) meta_dirty = true;
  // Keys
  uint8_t repeats;
  const uint8_t pressed = board_keys_read(now, &repeats);
  if (pressed && power_wake()) { meta_dirty = true; }
  else {
    const bool both = (repeats & BOARD_KEY_B) && (repeats & BOARD_KEY_C);
    if (pressed & BOARD_KEY_A) a_repeats = 0;
    if ((repeats & BOARD_KEY_A) || both) {
      ++a_repeats;
      if (a_repeats == 3) show_notice(both ? "Mantener: Radio Internet" : "Mantener: volver al receptor RF");
      if (a_repeats >= 9) {
        save_volume();
        if (sink) sink->end(false);
        digitalWrite(PIN_DAC_ENABLE, LOW);
        cx8_message(both ? "Radio Internet" : "Receptor RF", "Volviendo al firmware principal...");
        boot_mode_switch(both ? BOOT_MODE_NET : BOOT_MODE_RF);
      }
    }
    // A short (no auto-repeat within 0.8 s): switch to the transmitter.
    static uint32_t a_down_at = 0;
    if (pressed & BOARD_KEY_A) a_down_at = now;
    if (repeats & BOARD_KEY_A) a_down_at = 0;
    if (a_down_at && now - a_down_at > 800) {
      a_down_at = 0;
      // A short: switch to the transmitter
      save_volume();
      if (sink) sink->end(false);
      digitalWrite(PIN_DAC_ENABLE, LOW);
      boot_mode_set_bt_sub(BT_SUB_TRANSMITTER);
      cx8_message("Transmisor Bluetooth", "Reiniciando...");
      boot_mode_restart();
    }
    if (!both && (pressed & BOARD_KEY_B) && sink && connected) { sink->previous(); show_notice("Tema anterior"); }
    if (!both && (pressed & BOARD_KEY_C) && sink && connected) { sink->next(); show_notice("Tema siguiente"); }
  }
  // Encoder push: short play/pause, long mute, 3 s power off
  board_button_update(now);
  const bool p = board_button_pressed();
  if (p && !prev_pressed) {
    pressed_at = now; held = long_pending = power_pending = false;
    if (power_wake()) { held = true; meta_dirty = true; }
  }
  if (p && !held) {
    if (now - pressed_at >= POWER_OFF_MS) { if (!power_pending) { power_pending = true; show_notice("Soltá la rueda para apagar", 10000); } }
    else if (now - pressed_at >= LONG_PRESS_MS) long_pending = true;
  }
  if (!p && prev_pressed && !held) {
    if (power_pending) { save_volume(); speaker_power_off(); }
    else if (long_pending) { muted = !muted; apply_volume(); show_notice(muted ? "Silenciado" : "Sonido activado"); }
    else if (sink && connected) { if (playing) sink->pause(); else sink->play(); }
  }
  if (!p) long_pending = power_pending = false;
  prev_pressed = p;

  // DAC/amplifier only while music plays
  static int dac = -1;
  const int want = connected && playing && !muted && volume > 0;
  if (want != dac) { digitalWrite(PIN_DAC_ENABLE, want ? HIGH : LOW); dac = want; }

  if (now - last_battery >= 5000) { last_battery = now; battery = board_battery_read(); meta_dirty = true; }
  switch (power_update(now, battery.volts, battery.plausible)) {
    case POWER_EVENT_LOW_BATTERY: power_wake(); show_notice("Batería baja: cargá la radio", 8000); break;
    case POWER_EVENT_SHUTDOWN:
    case POWER_EVENT_SLEEP_TIMER: save_volume(); speaker_power_off();
    default: break;
  }
  if (notice.length() && now >= notice_until) { notice = ""; meta_dirty = true; }
  static uint32_t last_saved_change = 0; static int saved_volume = -1;
  if (volume != saved_volume) { if (!last_saved_change) last_saved_change = now; else if (now - last_saved_change > 5000) { save_volume(); saved_volume = volume; last_saved_change = 0; } }
  if (meta_dirty && now - last_draw >= 150 && power_screen_on()) { last_draw = now; draw_speaker(); }
  delay(5);
}

// =====================================================================
// Transmisor Bluetooth (A2DP source fed by the receiver's analog audio)
BluetoothA2DPSource* source = nullptr;
volatile bool tx_connected = false;
char tx_peer[48] = "";
volatile bool tx_event = false;
uint32_t phase = 0;            // Resampler position, Q16 fractional
int16_t prev_sample = 0, cur_sample = 0;
constexpr uint32_t STEP = (uint32_t)((16000.0 / 44100.0) * 65536.0); // input per output sample

int32_t get_audio(Frame* frames, int32_t count) {
  // Keep latency bounded: if more than ~180 ms is buffered, drop the excess.
  const size_t available = board_audio_stream_available();
  if (available > 2900) board_audio_stream_skip(available - 1600);
  for (int32_t i = 0; i < count; ++i) {
    phase += STEP;
    while (phase >= 65536) {
      phase -= 65536;
      prev_sample = cur_sample;
      int16_t s;
      if (board_audio_stream_read(&s, 1) == 1) cur_sample = s;
      else cur_sample = (int16_t)(cur_sample * 7 / 8); // Underrun: fade to silence
    }
    const int32_t v = prev_sample + (((int32_t)(cur_sample - prev_sample) * (int32_t)phase) >> 16);
    frames[i].channel1 = frames[i].channel2 = (int16_t)v;
  }
  return count;
}
bool accept_device(const char* name, esp_bd_addr_t, int rssi) {
  // Connect to the first audio device that answers (headphones or speaker in pairing mode).
  Serial.printf("CX8ANM BT: found '%s' rssi %d\n", name ? name : "", rssi);
  if (!name || !name[0]) return false;
  strncpy(tx_peer, name, sizeof tx_peer - 1);
  tx_event = true;
  return true;
}
void on_tx_connection(esp_a2d_connection_state_t state, void*) {
  tx_connected = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
  tx_event = true;
}

void transmitter_begin() {
  load_shared_settings();
  // Bluetooth first: its controller and stack need large blocks of RAM; the
  // receiver then draws without a frame buffer if one no longer fits.
  source = new BluetoothA2DPSource();
  source->set_local_name(BT_NAME);
  source->set_ssid_callback(accept_device);
  source->set_on_connection_state_changed(on_tx_connection);
  source->set_auto_reconnect(true);
  source->start(get_audio);
  receiver_begin();
  board_speaker_lock_off(true);  // Audio to the Bluetooth device only
  board_audio_stream_begin();
  rf_remote_notice("BT: buscando auriculares/parlante...", 8000);
}
void transmitter_update() {
  receiver_update();
  if (tx_event) {
    tx_event = false;
    char text[64];
    if (tx_connected) snprintf(text, sizeof text, "BT conectado: %s", tx_peer[0] ? tx_peer : "dispositivo");
    else snprintf(text, sizeof text, "BT: buscando auriculares/parlante...");
    rf_remote_notice(text, tx_connected ? 6000 : 8000);
  }
}
} // namespace

void btapp_begin(void) {
  // Take the battery latch first: starting Bluetooth takes about a second.
  gpio_hold_dis(GPIO_NUM_4);
  digitalWrite(4, HIGH);
  pinMode(4, OUTPUT);
  Serial.begin(115200);
  setCpuFrequencyMhz(160);
  sub = boot_mode_bt_sub();
  Serial.printf("CX8ANM BT image %s, mode %s, heap %u\n", CX8_FIRMWARE_VERSION,
                sub == BT_SUB_TRANSMITTER ? "transmitter" : "speaker", ESP.getFreeHeap());
  if (sub == BT_SUB_TRANSMITTER) transmitter_begin();
  else speaker_begin();
}
void btapp_update(void) {
  if (sub == BT_SUB_TRANSMITTER) transmitter_update();
  else speaker_update();
}
#endif // CX8_BT_FIRMWARE
