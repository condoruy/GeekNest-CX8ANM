#pragma once
#include <Arduino.h>
// Internet-radio settings and station list, in NVS namespace "cx8net".
constexpr int NET_MAX_STATIONS = 40;

struct net_station_t {
  String name;
  String url;
};

struct net_config_t {
  String ssid, password;
  net_station_t stations[NET_MAX_STATIONS];
  int count = 0;
  int current = 0;            // Index of the selected station
  uint8_t volume = 10;        // 0..21
  int16_t utc_offset = -180;  // Minutes; Uruguay is UTC-3
  uint8_t brightness = 160;
  bool autoplay = true;       // Resume the last station at boot
  bool rf_wifi = true;        // WiFi + web control while in receiver (RF) mode
  bool auto_update_check = true;
  String favorites;           // RF favorites, "Name|Hz|MODE" lines (parsed by the web page)
};

extern net_config_t net_config;

void net_config_load(void);
void net_config_save_wifi(void);
void net_config_save_stations(void);
void net_config_save_state(void);  // current, volume, brightness, offset, autoplay, rf_wifi
void net_config_save_favorites(void);
// "Name|URL" lines; returns the number of stations parsed (invalid lines skipped).
int net_config_parse_stations(const String& text);
String net_config_stations_text(void);
void net_config_reset_stations(void);
