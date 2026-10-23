#pragma once
// CX8ANM: remote control of the tinyradio receiver from the web page.
// Every function runs on the Arduino loop (the web server is polled from
// receiver_update), so no locking is needed.
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char* band;          // Band name
  int band_index;
  uint32_t frequency_hz;
  uint32_t step_hz;
  const char* mode;          // "FM", "AM", "LSB", "USB"
  int mode_index;
  uint8_t rssi, snr;
  bool stereo, valid, seeking, seek_mode, tuning, failed, muted;
  int volume;                // 0..63
  const char* ps;            // RDS station name
  const char* text;          // RDS radio text
  const char* error;
  float battery_volts;
  int battery_percent;
  int32_t temperature_milli; // SHT30, valid when environment_valid
  uint32_t humidity_milli;
  bool environment_valid;
} rf_remote_status_t;

void rf_remote_status(rf_remote_status_t* out);
// Band names and availability, for the web page band buttons.
int rf_remote_band_count(void);
const char* rf_remote_band_name(int index, bool* available, uint32_t* min_hz, uint32_t* max_hz);

bool rf_remote_tune_hz(uint32_t hz);   // Picks the band that contains hz
void rf_remote_step(int delta);        // Tuning steps
void rf_remote_band(int index);
bool rf_remote_mode(int mode);         // 0 FM, 1 AM, 2 LSB, 3 USB
void rf_remote_step_size(int delta);   // Cycle the tuning step
void rf_remote_volume(int volume);     // 0..63
void rf_remote_mute(bool muted);
void rf_remote_seek(int direction);    // +1 up, -1 down, 0 stop
void rf_remote_notice(const char* text, uint32_t ms);
void rf_remote_set_clock(int64_t utc_seconds, int16_t utc_offset_minutes); // From NTP
void rf_remote_set_utc_offset(int16_t minutes);
void rf_remote_switch_to_net(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
