#pragma once
// CX8ANM battery saving, shared by the receiver (C) and the internet radio (C++):
// screen dimming and switch-off, sleep timer and low-battery protection.
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  POWER_EVENT_NONE = 0,
  POWER_EVENT_LOW_BATTERY,  // Show a warning (once)
  POWER_EVENT_SHUTDOWN,     // Battery critically low: save and power off now
  POWER_EVENT_SLEEP_TIMER,  // Sleep timer expired: save and power off
  POWER_EVENT_SCREEN_ON     // Screen came back: redraw everything
} power_event_t;

// dim_s / off_s: seconds without input before dimming / switching the screen off (0 = never).
void power_configure(uint16_t dim_s, uint16_t off_s);
void power_begin(uint8_t brightness);
void power_set_brightness(uint8_t brightness); // User brightness (also wakes the screen)
// Call on every user input. Returns true when the screen was off: the input
// only wakes it and must be ignored.
bool power_wake(void);
bool power_screen_on(void);                    // False while the screen is off: skip drawing
power_event_t power_update(uint32_t now, float battery_volts, bool battery_plausible);
void power_set_sleep_timer(uint16_t minutes);  // 0 cancels
uint16_t power_sleep_minutes_left(void);       // 0 when not set
uint16_t power_dim_seconds(void);
uint16_t power_off_seconds(void);

#ifdef __cplusplus
}
#endif
