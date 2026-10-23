#pragma once
#include <stdint.h>
#include <stddef.h>
// Streaming player running in its own FreeRTOS task. All calls are
// asynchronous and safe from the Arduino loop; the task owns the Audio object.
enum player_state_t { PLAYER_IDLE, PLAYER_CONNECTING, PLAYER_PLAYING, PLAYER_ERROR };

struct player_status_t {
  player_state_t state;
  uint32_t generation;     // Increments whenever station, title or state change.
  uint32_t bitrate;        // bits per second, 0 when unknown
  char codec[8];
  char icy_name[64];       // Station name announced by the stream
  char title[160];         // Current stream title (UTF-8)
  char error[48];
};

// Pins: BCLK, LRCK, DOUT (vendor yoRadio mapping on the V5A).
void player_begin(int bclk, int lrck, int dout);
void player_play(const char* url);
void player_stop(void);
void player_set_volume(uint8_t volume); // 0..21
void player_get_status(player_status_t* out);
uint32_t player_generation(void);
// Converts ISO-8859-1 text to UTF-8 when it is not valid UTF-8 already.
void player_normalize_text(const char* in, char* out, size_t size);
