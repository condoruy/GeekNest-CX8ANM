#pragma once
#include <Arduino.h>
#include "player.h"

// What the internet-radio screen shows. netradio.cpp fills it each frame;
// screen.cpp redraws only the regions whose content changed.
enum net_overlay_t { OVERLAY_NONE, OVERLAY_BROWSE, OVERLAY_INFO };

struct net_view_t {
  // Header
  String clock;          // "HH:MM" or ""
  int wifi_bars;         // -1 = no WiFi, 0..4
  int battery_percent;   // -1 unknown
  float battery_volts;
  // Station
  int index, count;
  String station;        // Name from the list
  player_status_t player;
  bool muted;
  uint8_t volume;        // 0..21
  // Footer
  String footer;
  String notice;         // Transient message, or ""
  // Overlays
  net_overlay_t overlay;
  int browse_index;
  const String* names;   // count entries, for the browse list
  String info_lines[7];
};

void screen_begin(uint8_t brightness);
void screen_set_brightness(uint8_t brightness);
void screen_invalidate(void);          // Force a full redraw
void screen_draw(const net_view_t& view);
// Setup screen shown while the access point is active.
void screen_setup(const char* ap_ssid, const char* ap_password, const char* ip, const char* reason);
// Connecting progress screen.
void screen_connecting(const char* ssid, int seconds);
// Full-screen progress bar (firmware update).
void screen_progress(const char* title, int percent);
