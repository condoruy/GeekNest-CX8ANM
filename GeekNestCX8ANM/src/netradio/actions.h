#pragma once
#include <Arduino.h>
// Actions the web page can trigger; implemented in netradio.cpp and run on
// the Arduino loop (the web server is polled from there too).
void net_action_play(int index);
void net_action_stop(void);
void net_action_step(int delta);       // Next/previous station
void net_action_volume(int volume);    // 0..21
void net_action_mute(bool muted);
void net_action_brightness(int value); // 16..255
void net_action_timezone(int minutes);
void net_action_stations_changed(void);
void net_action_wifi_saved(void);      // Restart and connect with the new credentials
void net_action_switch_rf(void);
String net_status_json(void);
bool net_is_access_point(void);
