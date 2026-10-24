#include "player.h"
#include <Arduino.h>
#include <Audio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

namespace {
enum command_type_t : uint8_t { CMD_PLAY, CMD_STOP, CMD_VOLUME };
struct command_t {
  command_type_t type;
  uint8_t value;
  char url[300];
};

QueueHandle_t queue = nullptr;
Audio* audio = nullptr;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
player_status_t status = {};
int pins[3] = {25, 27, 26};
uint32_t connect_started = 0;
char current_url[300] = "";
volatile bool stream_ready = false; // Set from audio_info once data flows

void copy_text(char* dst, size_t size, const char* src) {
  if (!size) return;
  strncpy(dst, src ? src : "", size - 1);
  dst[size - 1] = 0;
}
void set_state(player_state_t state, const char* error = nullptr) {
  portENTER_CRITICAL(&lock);
  status.state = state;
  copy_text(status.error, sizeof status.error, error);
  ++status.generation;
  portEXIT_CRITICAL(&lock);
}
void reset_metadata(void) {
  portENTER_CRITICAL(&lock);
  status.icy_name[0] = status.title[0] = status.error[0] = status.codec[0] = 0;
  status.bitrate = 0;
  ++status.generation;
  portEXIT_CRITICAL(&lock);
}

void handle(const command_t& c) {
  switch (c.type) {
    case CMD_VOLUME: audio->setVolume(c.value > 21 ? 21 : c.value); break;
    case CMD_STOP:
      audio->stopSong();
      current_url[0] = 0;
      reset_metadata();
      set_state(PLAYER_IDLE);
      break;
    case CMD_PLAY:
      audio->stopSong();
      reset_metadata();
      copy_text(current_url, sizeof current_url, c.url);
      stream_ready = false;
      set_state(PLAYER_CONNECTING);
      connect_started = millis();
      if (!audio->connecttohost(current_url)) set_state(PLAYER_ERROR, "No se pudo conectar");
      break;
  }
}

void watch(void) {
  const player_state_t state = status.state;
  if (state == PLAYER_CONNECTING) {
    if (audio->isRunning() && (stream_ready || audio->getBitRate() > 0)) {
      portENTER_CRITICAL(&lock);
      status.bitrate = audio->getBitRate();
      copy_text(status.codec, sizeof status.codec, audio->getCodecname());
      portEXIT_CRITICAL(&lock);
      set_state(PLAYER_PLAYING);
    } else if (millis() - connect_started > 20000) {
      audio->stopSong();
      set_state(PLAYER_ERROR, "Sin respuesta del servidor");
    }
  } else if (state == PLAYER_PLAYING) {
    if (!audio->isRunning()) set_state(PLAYER_ERROR, "Transmision cortada");
    else {
      const uint32_t br = audio->getBitRate();
      if (br && br != status.bitrate) {
        portENTER_CRITICAL(&lock); status.bitrate = br; ++status.generation; portEXIT_CRITICAL(&lock);
      }
    }
  }
}

void task(void*) {
  audio = new Audio(false, 3, I2S_NUM_0);
  audio->setBufsize(16 * 1024, -1); // No PSRAM on this build: keep the stream buffer in internal RAM.
  audio->setPinout(pins[0], pins[1], pins[2]);
  audio->setConnectionTimeout(5000, 8000);
  audio->setVolume(10);
  command_t c;
  uint32_t last_watch = 0;
  for (;;) {
    while (xQueueReceive(queue, &c, 0) == pdTRUE) handle(c);
    audio->loop();
    if (millis() - last_watch >= 250) { last_watch = millis(); watch(); }
    vTaskDelay(2);
  }
}

void send(const command_t& c) {
  if (queue) xQueueSend(queue, &c, pdMS_TO_TICKS(100));
}
} // namespace

void player_normalize_text(const char* in, char* out, size_t size) {
  if (!size) return;
  // Valid UTF-8? Then copy as is.
  bool valid = true;
  for (const unsigned char* p = (const unsigned char*)in; *p && valid;) {
    int n = *p < 0x80 ? 0 : (*p >> 5) == 6 ? 1 : (*p >> 4) == 14 ? 2 : (*p >> 3) == 30 ? 3 : -1;
    if (n < 0) { valid = false; break; }
    ++p;
    for (int i = 0; i < n; ++i, ++p) if ((*p & 0xC0) != 0x80) { valid = false; break; }
  }
  size_t o = 0;
  for (const unsigned char* p = (const unsigned char*)in; *p && o + 1 < size; ++p) {
    if (valid || *p < 0x80) out[o++] = (char)*p;
    else {                       // ISO-8859-1 -> UTF-8
      if (o + 2 >= size) break;
      out[o++] = (char)(0xC0 | (*p >> 6));
      out[o++] = (char)(0x80 | (*p & 0x3F));
    }
  }
  out[o] = 0;
}

// Callbacks from the Audio library (run in the player task).
void audio_showstation(const char* info) {
  char text[64];
  player_normalize_text(info, text, sizeof text);
  portENTER_CRITICAL(&lock);
  copy_text(status.icy_name, sizeof status.icy_name, text);
  ++status.generation;
  portEXIT_CRITICAL(&lock);
}
void audio_showstreamtitle(const char* info) {
  char text[160];
  player_normalize_text(info, text, sizeof text);
  portENTER_CRITICAL(&lock);
  copy_text(status.title, sizeof status.title, text);
  ++status.generation;
  portEXIT_CRITICAL(&lock);
}
void audio_info(const char* info) {
  if (strstr(info, "stream ready") || strstr(info, "syncword found")) stream_ready = true;
  // A web page instead of an audio stream (e.g. the station's site) or an HLS playlist.
  if (strstr(info, "text/html")) set_state(PLAYER_ERROR, "Es una pagina web, no un stream");
  else if (strstr(info, "mpegurl") || strstr(info, ".m3u8")) set_state(PLAYER_ERROR, "Formato HLS no soportado");
  Serial.printf("audio: %s\n", info);
}
void audio_eof_stream(const char*) { set_state(PLAYER_ERROR, "Fin de la transmision"); }

void player_begin(int bclk, int lrck, int dout) {
  pins[0] = bclk; pins[1] = lrck; pins[2] = dout;
  queue = xQueueCreate(4, sizeof(command_t));
  // Core 0 (with WiFi): the Arduino loop on core 1 keeps the knob, keys, screen and web responsive.
  xTaskCreatePinnedToCore(task, "cx8audio", 12288, nullptr, 2, nullptr, 0);
}
void player_play(const char* url) {
  command_t c = {CMD_PLAY, 0, {0}};
  copy_text(c.url, sizeof c.url, url);
  send(c);
}
void player_stop(void) { command_t c = {CMD_STOP, 0, {0}}; send(c); }
void player_set_volume(uint8_t volume) { command_t c = {CMD_VOLUME, volume, {0}}; send(c); }
void player_get_status(player_status_t* out) {
  portENTER_CRITICAL(&lock);
  *out = status;
  portEXIT_CRITICAL(&lock);
}
uint32_t player_generation(void) { return status.generation; }
