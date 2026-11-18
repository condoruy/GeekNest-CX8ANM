#ifndef CX8_BT_FIRMWARE // Main image only (the Bluetooth image has no WiFi)
#include "updater.h"
#include "certs.h"
#include "ui/splash.h"
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <nvs.h>
#include <esp_ota_ops.h>
extern "C" {
#include "app/boot_mode.h"
}

updater_state_t updater;

namespace {
String json_string(const String& json, const char* key) {
  const String pattern = String("\"") + key + "\"";
  int i = json.indexOf(pattern);
  if (i < 0) return "";
  i = json.indexOf(':', i + pattern.length());
  if (i < 0) return "";
  while (++i < (int)json.length() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')) {}
  if (i >= (int)json.length()) return "";
  if (json[i] != '"') { // number
    int end = i;
    while (end < (int)json.length() && (isDigit(json[end]) || json[end] == '.')) ++end;
    return json.substring(i, end);
  }
  String out;
  for (++i; i < (int)json.length() && json[i] != '"'; ++i) {
    if (json[i] == '\\' && i + 1 < (int)json.length()) {
      const char n = json[++i];
      out += n == 'n' ? '\n' : n == 't' ? ' ' : n;
    } else out += json[i];
  }
  return out;
}

String json_escape(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}

void check_task(void*) {
  WiFiClientSecure client;
  client.setCACert(CX8_GITHUB_ROOT_CAS);
  client.setTimeout(15);
  HTTPClient http;
  String error;
  if (!http.begin(client, CX8_OTA_MANIFEST_URL)) error = "No se pudo abrir la URL de GitHub";
  else {
    http.setTimeout(15000);
    const int code = http.GET();
    if (code != 200) error = code < 0 ? String("Sin conexion con GitHub (") + http.errorToString(code) + ")"
                                       : String("GitHub respondio ") + code;
    else {
      const String body = http.getString();
      const String version = json_string(body, "version");
      if (!version.length()) error = "ota.json sin version";
      else {
        updater.latest = version;
        updater.notes = json_string(body, "notes");
        updater.url = json_string(body, "url");
        updater.sha256 = json_string(body, "sha256");
        updater.size = (uint32_t)json_string(body, "size").toInt();
        updater.bt_url = json_string(body, "bt_url");
        updater.bt_sha256 = json_string(body, "bt_sha256");
        updater.bt_size = (uint32_t)json_string(body, "bt_size").toInt();
        updater.available = updater_compare(version, CX8_FIRMWARE_VERSION) > 0 && updater.url.length() && updater.size;
      }
    }
    http.end();
  }
  updater.error = error;
  updater.checked = error.length() == 0;
  updater.checked_at = millis();
  updater.checking = false;
  Serial.printf("CX8ANM: update check %s latest=%s available=%d\n", error.length() ? error.c_str() : "ok",
                updater.latest.c_str(), updater.available);
  vTaskDelete(nullptr);
}

// Upload state
bool upload_ok = false;
String upload_error;
} // namespace

int updater_compare(const String& a, const String& b) {
  int ia = 0, ib = 0;
  for (int part = 0; part < 3; ++part) {
    const long va = a.substring(ia).toInt(), vb = b.substring(ib).toInt();
    if (va != vb) return va > vb ? 1 : -1;
    ia = a.indexOf('.', ia); ib = b.indexOf('.', ib);
    if (ia < 0 && ib < 0) return 0;
    ia = ia < 0 ? a.length() : ia + 1;
    ib = ib < 0 ? b.length() : ib + 1;
  }
  return 0;
}

bool updater_check_async(void) {
  if (updater.checking || updater.installing) return false;
  if (WiFi.status() != WL_CONNECTED) { updater.error = "Sin WiFi"; return false; }
  if (ESP.getMaxAllocHeap() < 40000 || ESP.getFreeHeap() < 60000) {
    updater.error = "Poca memoria libre: busca la actualizacion desde el modo Radio Internet";
    return false;
  }
  updater.checking = true;
  updater.error = "";
  if (xTaskCreatePinnedToCore(check_task, "cx8ota", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    updater.checking = false;
    updater.error = "No se pudo iniciar la busqueda";
    return false;
  }
  return true;
}

bool updater_install(void (*progress)(int percent)) {
  if (!updater.available) { updater.error = "No hay version nueva"; return false; }
  updater.installing = true;
  updater.progress = 0;
  updater.error = "";
  WiFiClientSecure client;
  client.setCACert(CX8_GITHUB_ROOT_CAS);
  client.setTimeout(20);
  HTTPClient http;
  bool ok = false;
  if (!http.begin(client, updater.url)) updater.error = "URL de descarga invalida";
  else {
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    const int code = http.GET();
    const int length = http.getSize();
    if (code != 200) updater.error = String("Descarga fallo: ") + (code < 0 ? http.errorToString(code) : String(code));
    else if (length > 0 && (uint32_t)length != updater.size) updater.error = "Tamano distinto al anunciado";
    else if (!Update.begin(updater.size, U_FLASH)) updater.error = String("Sin espacio para actualizar: ") + Update.errorString();
    else {
      mbedtls_sha256_context sha;
      mbedtls_sha256_init(&sha);
      mbedtls_sha256_starts_ret(&sha, 0);
      WiFiClient* stream = http.getStreamPtr();
      constexpr size_t BUFFER_SIZE = 4096;
      uint8_t* buffer = (uint8_t*)malloc(BUFFER_SIZE);
      if (!buffer) updater.error = "Sin memoria para descargar";
      uint32_t written = 0, last_data = millis();
      int shown = -1;
      while (buffer && written < updater.size && http.connected()) {
        const size_t available = stream->available();
        if (!available) {
          if (millis() - last_data > 20000) break;
          delay(2);
          continue;
        }
        const size_t want = min(BUFFER_SIZE, (size_t)(updater.size - written));
        const int got = stream->readBytes(buffer, min(available, want));
        if (got <= 0) continue;
        last_data = millis();
        mbedtls_sha256_update_ret(&sha, buffer, got);
        if (Update.write(buffer, got) != (size_t)got) { updater.error = String("Error al grabar: ") + Update.errorString(); break; }
        written += got;
        updater.progress = (int)((uint64_t)written * 100 / updater.size);
        if (progress && updater.progress != shown) { shown = updater.progress; progress(shown); }
      }
      free(buffer);
      uint8_t digest[32];
      mbedtls_sha256_finish_ret(&sha, digest);
      mbedtls_sha256_free(&sha);
      char hex[65];
      for (int i = 0; i < 32; ++i) sprintf(hex + i * 2, "%02x", digest[i]);
      if (!updater.error.length() && written != updater.size) updater.error = "Descarga incompleta";
      if (!updater.error.length() && updater.sha256.length() && !updater.sha256.equalsIgnoreCase(hex))
        updater.error = "El archivo descargado no coincide (SHA-256)";
      if (updater.error.length()) Update.abort();
      else if (!Update.end(true)) updater.error = String("No se pudo activar: ") + Update.errorString();
      else { ok = true; boot_mode_bt_clear(); } // The new main image replaced the Bluetooth one
    }
    http.end();
  }
  updater.installing = false;
  if (!ok) updater.progress = -1;
  Serial.printf("CX8ANM: install %s\n", ok ? "ok" : updater.error.c_str());
  return ok;
}

void updater_set_pending(bool pending) {
  nvs_handle_t h;
  if (nvs_open("cx8anm", NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_u8(h, "otapend", pending ? 1 : 0);
  nvs_commit(h);
  nvs_close(h);
}
bool updater_take_pending(void) {
  nvs_handle_t h;
  uint8_t value = 0;
  if (nvs_open("cx8anm", NVS_READWRITE, &h) != ESP_OK) return false;
  nvs_get_u8(h, "otapend", &value);
  if (value) { nvs_set_u8(h, "otapend", 0); nvs_commit(h); }
  nvs_close(h);
  return value != 0;
}

String updater_json(void) {
  String j = "{";
  j += "\"current\":\"" CX8_FIRMWARE_VERSION "\",";
  j += "\"latest\":\"" + json_escape(updater.latest) + "\",";
  j += "\"available\":" + String(updater.available ? "true" : "false") + ",";
  j += "\"checking\":" + String(updater.checking ? "true" : "false") + ",";
  j += "\"checked\":" + String(updater.checked ? "true" : "false") + ",";
  j += "\"installing\":" + String(updater.installing ? "true" : "false") + ",";
  j += "\"progress\":" + String(updater.progress) + ",";
  j += "\"notes\":\"" + json_escape(updater.notes) + "\",";
  char btver[24];
  boot_mode_bt_version(btver, sizeof btver);
  j += "\"bt_installed\":" + String(boot_mode_bt_installed() ? "true" : "false") + ",\"bt_ver\":\"" + json_escape(btver) + "\",";
  j += "\"bt_sub\":" + String((int)boot_mode_bt_sub()) + ",";
  j += "\"error\":\"" + json_escape(updater.error) + "\"";
  j += "}";
  return j;
}

void updater_upload_begin(void) {
  upload_ok = false;
  upload_error = "";
  updater.installing = true;
  updater.progress = 0;
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) upload_error = String("No se pudo empezar: ") + Update.errorString();
}
void updater_upload_write(const uint8_t* data, size_t size) {
  if (upload_error.length()) return;
  if (Update.write((uint8_t*)data, size) != size) upload_error = String("Error al grabar: ") + Update.errorString();
}
bool updater_upload_end(String* error) {
  updater.installing = false;
  if (!upload_error.length() && !Update.end(true)) upload_error = String("Archivo invalido: ") + Update.errorString();
  if (upload_error.length()) Update.abort();
  upload_ok = !upload_error.length();
  if (upload_ok) boot_mode_bt_clear();
  updater.progress = upload_ok ? 100 : -1;
  if (error) *error = upload_error;
  return upload_ok;
}

#endif // CX8_BT_FIRMWARE

#ifndef CX8_BT_FIRMWARE
namespace {
esp_ota_handle_t bt_handle = 0;
const esp_partition_t* bt_part = nullptr;
String bt_upload_error;
uint32_t bt_written = 0;
}

bool updater_install_bt(void (*progress)(int percent)) {
  if (!updater.bt_url.length() || !updater.bt_size) { updater.error = "No hay imagen Bluetooth publicada"; return false; }
  bt_part = esp_ota_get_next_update_partition(nullptr);
  if (!bt_part || updater.bt_size > bt_part->size) { updater.error = "Sin espacio para el modo Bluetooth"; return false; }
  updater.installing = true;
  updater.progress = 0;
  updater.error = "";
  boot_mode_bt_clear();
  WiFiClientSecure client;
  client.setCACert(CX8_GITHUB_ROOT_CAS);
  client.setTimeout(20);
  HTTPClient http;
  bool ok = false;
  if (!http.begin(client, updater.bt_url)) updater.error = "URL de descarga invalida";
  else {
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    const int code = http.GET();
    const int length = http.getSize();
    if (code != 200) updater.error = String("Descarga fallo: ") + (code < 0 ? http.errorToString(code) : String(code));
    else if (length > 0 && (uint32_t)length != updater.bt_size) updater.error = "Tamano distinto al anunciado";
    else if (esp_ota_begin(bt_part, updater.bt_size, &bt_handle) != ESP_OK) updater.error = "No se pudo preparar la memoria";
    else {
      mbedtls_sha256_context sha;
      mbedtls_sha256_init(&sha);
      mbedtls_sha256_starts_ret(&sha, 0);
      WiFiClient* stream = http.getStreamPtr();
      constexpr size_t BUFFER_SIZE = 4096;
      uint8_t* buffer = (uint8_t*)malloc(BUFFER_SIZE);
      if (!buffer) updater.error = "Sin memoria para descargar";
      uint32_t written = 0, last_data = millis();
      int shown = -1;
      while (buffer && written < updater.bt_size && http.connected()) {
        const size_t available = stream->available();
        if (!available) { if (millis() - last_data > 20000) break; delay(2); continue; }
        const int got = stream->readBytes(buffer, min(available, min(BUFFER_SIZE, (size_t)(updater.bt_size - written))));
        if (got <= 0) continue;
        last_data = millis();
        mbedtls_sha256_update_ret(&sha, buffer, got);
        if (esp_ota_write(bt_handle, buffer, got) != ESP_OK) { updater.error = "Error al grabar"; break; }
        written += got;
        updater.progress = (int)((uint64_t)written * 100 / updater.bt_size);
        if (progress && updater.progress != shown) { shown = updater.progress; progress(shown); }
      }
      free(buffer);
      uint8_t digest[32];
      mbedtls_sha256_finish_ret(&sha, digest);
      mbedtls_sha256_free(&sha);
      char hex[65];
      for (int i = 0; i < 32; ++i) sprintf(hex + i * 2, "%02x", digest[i]);
      if (!updater.error.length() && written != updater.bt_size) updater.error = "Descarga incompleta";
      if (!updater.error.length() && updater.bt_sha256.length() && !updater.bt_sha256.equalsIgnoreCase(hex))
        updater.error = "El archivo descargado no coincide (SHA-256)";
      if (updater.error.length()) esp_ota_abort(bt_handle);
      else if (esp_ota_end(bt_handle) != ESP_OK) updater.error = "Imagen Bluetooth invalida";
      else { ok = true; boot_mode_bt_mark(updater.latest.c_str()); }
    }
    http.end();
  }
  updater.installing = false;
  updater.progress = ok ? 100 : -1;
  Serial.printf("CX8ANM: Bluetooth install %s\n", ok ? "ok" : updater.error.c_str());
  return ok;
}

void updater_bt_upload_begin(void) {
  bt_upload_error = "";
  bt_written = 0;
  updater.installing = true;
  boot_mode_bt_clear();
  bt_part = esp_ota_get_next_update_partition(nullptr);
  if (!bt_part || esp_ota_begin(bt_part, OTA_SIZE_UNKNOWN, &bt_handle) != ESP_OK) bt_upload_error = "No se pudo preparar la memoria";
}
void updater_bt_upload_write(const uint8_t* data, size_t size) {
  if (bt_upload_error.length()) return;
  if (esp_ota_write(bt_handle, data, size) != ESP_OK) bt_upload_error = "Error al grabar";
  bt_written += size;
}
bool updater_bt_upload_end(String* error) {
  updater.installing = false;
  if (!bt_upload_error.length() && esp_ota_end(bt_handle) != ESP_OK) bt_upload_error = "Archivo invalido";
  else if (bt_upload_error.length()) esp_ota_abort(bt_handle);
  if (!bt_upload_error.length()) boot_mode_bt_mark("manual");
  if (error) *error = bt_upload_error;
  return !bt_upload_error.length();
}
#endif
