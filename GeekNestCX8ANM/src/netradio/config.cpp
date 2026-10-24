#include "config.h"
#include <Preferences.h>
extern "C" {
#include "boards/board.h"
}

net_config_t net_config;

namespace {
const char NAMESPACE[] = "cx8net";
// Default list: Uruguayan stations checked in October 2026. URLs change over
// time; edit them from the web page.
const char DEFAULT_STATIONS[] =
  // Uruguay
  "El Espectador 810|https://espectador-1.nty.uy/;\n"
  "Radio Carve 850|https://icecast23.innovanexo.com:9000/radiocarve850.mp3\n"
  "Sarandí 690|https://radiolatina.live:10977/sarandi\n"
  "En Perspectiva (Radiomundo 1170)|https://stream1.enperspectiva.net:8443/radio.mp4\n"
  "la diaria Radio|https://radiolatina.live/8156/stream\n"
  "Del Sol FM|https://delsol-1.nty.uy/;\n"
  "Océano FM|https://oceano-2.nty.uy/;\n"
  "FM Hit 90.3|https://hit-1.nty.uy/stream\n"
  // Noticias en español del exterior
  "Radio Mitre 790 (Argentina)|https://playerservices.streamtheworld.com/api/livestream-redirect/AM790_56AAC.aac\n"
  "Radio Cooperativa (Chile)|https://unlimited3-cl.dps.live/cooperativafm/mp3/icecast.audio\n"
  "Radio Bío-Bío (Chile)|https://unlimited3-cl.dps.live/biobiosantiago/mp3/icecast.audio\n"
  "ADN Radio (Chile)|https://playerservices.streamtheworld.com/api/livestream-redirect/ADNAAC.aac\n";

String stored_text;
}

int net_config_parse_stations(const String& text) {
  int count = 0, start = 0;
  const int length = text.length();
  int stored = 0; // NVS strings are limited to about 4000 bytes.
  while (start < length && count < NET_MAX_STATIONS) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = length;
    String line = text.substring(start, end);
    start = end + 1;
    line.trim();
    if (!line.length() || line[0] == '#') continue;
    const int bar = line.indexOf('|');
    String name, url;
    if (bar < 0) { url = line; name = line; }
    else { name = line.substring(0, bar); url = line.substring(bar + 1); }
    name.trim(); url.trim();
    if (!(url.startsWith("http://") || url.startsWith("https://"))) continue;
    if (url.length() > 290) continue;
    if (name.length() > 48) {
      name = name.substring(0, 48);
      // Do not leave half of a UTF-8 character at the end.
      while (name.length() && (uint8_t)name[name.length() - 1] >= 0x80) name.remove(name.length() - 1);
    }
    if (!name.length()) name = url;
    stored += name.length() + url.length() + 2;
    if (stored > 3800) break;
    net_config.stations[count].name = name;
    net_config.stations[count].url = url;
    ++count;
  }
  net_config.count = count;
  if (net_config.current >= count) net_config.current = 0;
  return count;
}

String net_config_stations_text(void) {
  String text;
  for (int i = 0; i < net_config.count; ++i)
    text += net_config.stations[i].name + "|" + net_config.stations[i].url + "\n";
  return text;
}

void net_config_reset_stations(void) { net_config_parse_stations(DEFAULT_STATIONS); }

void net_config_load(void) {
  Preferences p;
  p.begin(NAMESPACE, true);
  net_config.ssid = p.getString("ssid", "");
  net_config.password = p.getString("pass", "");
  stored_text = p.getString("st", "");
  net_config.current = p.getInt("cur", 0);
  net_config.volume = p.getUChar("vol", 10);
  net_config.utc_offset = p.getShort("tz", -180);
  net_config.brightness = p.getUChar("bri", 160);
  net_config.autoplay = p.getBool("auto", true);
  net_config.rf_wifi = p.getBool("rfwifi", true);
  net_config.auto_update_check = p.getBool("autoupd", true);
  net_config.encoder_inverted = p.getBool("encinv", true);
  net_config.favorites = p.getString("fav", "");
  p.end();
  board_encoder_set_inverted(net_config.encoder_inverted);
  if (net_config.volume > 21) net_config.volume = 21;
  if (net_config.brightness < 16) net_config.brightness = 16;
  if (!stored_text.length() || !net_config_parse_stations(stored_text)) net_config_reset_stations();
  else if (p.begin(NAMESPACE, false)) { // v1.1.3: add the new default stations once
    if (!p.getBool("def113", false)) {
      String merged = net_config_stations_text();
      const String defaults = DEFAULT_STATIONS;
      int start = 0;
      while (start < (int)defaults.length()) {
        int end = defaults.indexOf('\n', start); if (end < 0) end = defaults.length();
        const String line = defaults.substring(start, end); start = end + 1;
        const String url = line.substring(line.indexOf('|') + 1);
        if (url.length() && merged.indexOf(url) < 0) merged += line + "\n";
      }
      net_config_parse_stations(merged);
      p.putString("st", net_config_stations_text());
      p.putBool("def113", true);
    }
    p.end();
  }
  if (net_config.current < 0 || net_config.current >= net_config.count) net_config.current = 0;
}

void net_config_save_wifi(void) {
  Preferences p;
  p.begin(NAMESPACE, false);
  p.putString("ssid", net_config.ssid);
  p.putString("pass", net_config.password);
  p.end();
}

void net_config_save_stations(void) {
  Preferences p;
  p.begin(NAMESPACE, false);
  p.putString("st", net_config_stations_text());
  p.end();
}

void net_config_save_state(void) {
  Preferences p;
  p.begin(NAMESPACE, false);
  p.putInt("cur", net_config.current);
  p.putUChar("vol", net_config.volume);
  p.putShort("tz", net_config.utc_offset);
  p.putUChar("bri", net_config.brightness);
  p.putBool("auto", net_config.autoplay);
  p.putBool("rfwifi", net_config.rf_wifi);
  p.putBool("autoupd", net_config.auto_update_check);
  p.putBool("encinv", net_config.encoder_inverted);
  p.end();
}

void net_config_save_favorites(void) {
  Preferences p;
  p.begin(NAMESPACE, false);
  if (net_config.favorites.length() > 3800) net_config.favorites = net_config.favorites.substring(0, 3800);
  p.putString("fav", net_config.favorites);
  p.end();
}
