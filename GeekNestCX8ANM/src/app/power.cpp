#include "power.h"
#include <Arduino.h>
#include "boards/display.hpp"
extern "C" {
#include "boards/board.h"
}

namespace {
enum screen_state_t { SCREEN_ON, SCREEN_DIM, SCREEN_OFF };
screen_state_t screen = SCREEN_ON;
uint16_t dim_after_s = 30, off_after_s = 120;
uint8_t user_brightness = 128;
uint32_t last_input = 0;
uint32_t sleep_deadline = 0;   // millis(); 0 = no timer
bool low_warned = false;
uint32_t low_since = 0, critical_since = 0;
bool started = false;

// 18650 thresholds measured at the board's battery divider (under load).
constexpr float LOW_VOLTS = 3.30f, CRITICAL_VOLTS = 3.10f;
constexpr uint32_t LOW_HOLD_MS = 20000, CRITICAL_HOLD_MS = 15000;

uint8_t dim_level() {
  const int level = user_brightness / 6;
  return (uint8_t)(level < 6 ? 6 : level);
}
void apply(screen_state_t next) {
  if (next == screen) return;
  auto& panel = board_display_device();
  if (screen == SCREEN_OFF) { panel.wakeup(); delay(5); }
  if (next == SCREEN_OFF) {
    board_display_set_brightness(0);
    panel.sleep();                // Panel sleep-in: a few mA less than a dark backlight alone
  } else {
    board_display_set_brightness(next == SCREEN_DIM ? dim_level() : user_brightness);
  }
  screen = next;
}
} // namespace

void power_configure(uint16_t dim_s, uint16_t off_s) {
  dim_after_s = dim_s;
  off_after_s = off_s;
  last_input = millis();
}
void power_begin(uint8_t brightness) {
  user_brightness = brightness;
  screen = SCREEN_ON;
  last_input = millis();
  started = true;
}
void power_set_brightness(uint8_t brightness) {
  user_brightness = brightness;
  last_input = millis();
  if (screen == SCREEN_ON) board_display_set_brightness(brightness);
  else apply(SCREEN_ON);
}
bool power_wake(void) {
  last_input = millis();
  if (!started || screen == SCREEN_ON) return false;
  const bool was_off = screen == SCREEN_OFF;
  apply(SCREEN_ON);
  return was_off;
}
bool power_screen_on(void) { return screen != SCREEN_OFF; }

power_event_t power_update(uint32_t now, float volts, bool plausible) {
  power_event_t event = POWER_EVENT_NONE;
  if (started) {
    const uint32_t idle = now - last_input;
    screen_state_t want = SCREEN_ON;
    if (off_after_s && idle >= off_after_s * 1000UL) want = SCREEN_OFF;
    else if (dim_after_s && idle >= dim_after_s * 1000UL) want = SCREEN_DIM;
    // Only step down automatically; waking is done by power_wake().
    if (want > screen) apply(want);
  }
  if (sleep_deadline && (int32_t)(now - sleep_deadline) >= 0) {
    sleep_deadline = 0;
    return POWER_EVENT_SLEEP_TIMER;
  }
  if (plausible && volts > 2.5f) {
    if (volts < CRITICAL_VOLTS) {
      if (!critical_since) critical_since = now;
      else if (now - critical_since >= CRITICAL_HOLD_MS) return POWER_EVENT_SHUTDOWN;
    } else critical_since = 0;
    if (volts < LOW_VOLTS) {
      if (!low_since) low_since = now;
      else if (!low_warned && now - low_since >= LOW_HOLD_MS) { low_warned = true; event = POWER_EVENT_LOW_BATTERY; }
    } else if (volts > LOW_VOLTS + 0.15f) { low_since = 0; low_warned = false; } // Recharged
  }
  return event;
}

void power_set_sleep_timer(uint16_t minutes) {
  sleep_deadline = minutes ? millis() + minutes * 60000UL : 0;
  if (sleep_deadline == 0 && minutes) sleep_deadline = 1;
}
uint16_t power_sleep_minutes_left(void) {
  if (!sleep_deadline) return 0;
  const int32_t left = (int32_t)(sleep_deadline - millis());
  return left <= 0 ? 0 : (uint16_t)((left + 59999) / 60000);
}
uint16_t power_dim_seconds(void) { return dim_after_s; }
uint16_t power_off_seconds(void) { return off_after_s; }
