#include "screen.h"
#include "boards/display.hpp"
#include "ui/splash.h"
#include "fonts_cx8.h"
extern "C" {
#include "boards/board.h"
}

namespace {
// Palette (RGB565)
constexpr uint16_t BG = 0x0861;
constexpr uint16_t PANEL = 0x10C3;
constexpr uint16_t BORDER = 0x2945;
constexpr uint16_t TEXT = 0xE73C;
constexpr uint16_t DIM = 0x8C71;
constexpr uint16_t ACCENT = 0xFD20;   // amber
constexpr uint16_t TEAL = 0x2D7F;
constexpr uint16_t OK = 0x3666;       // green
constexpr uint16_t WARN = 0xFE60;     // yellow
constexpr uint16_t BAD = 0xF208;      // red

constexpr int W = 320;
const lgfx::IFont* F12 = &cx8_sans12;
const lgfx::IFont* F16 = &cx8_sans16;
const lgfx::IFont* F16B = &cx8_sans16b;
const lgfx::IFont* F24B = &cx8_sans24b;

lgfx::LGFX_Device& g() { return board_display_device(); }

String last_header, last_station, last_title, last_volume, last_footer, last_overlay;
bool full = true;

// Shortens text to fit width, adding "..." (UTF-8 safe).
String fit(const String& text, int width) {
  if (g().textWidth(text) <= width) return text;
  String s = text;
  while (s.length()) {
    int cut = s.length() - 1;
    while (cut > 0 && ((uint8_t)s[cut] & 0xC0) == 0x80) --cut;
    s = s.substring(0, cut);
    if (g().textWidth(s + "...") <= width) return s + "...";
  }
  return "...";
}

// Word wrap into up to max_lines lines that fit width.
int wrap(const String& text, int width, String* lines, int max_lines) {
  int count = 0;
  String rest = text;
  rest.trim();
  while (rest.length() && count < max_lines) {
    if (g().textWidth(rest) <= width) { lines[count++] = rest; break; }
    int best = -1;
    for (int i = 0; i < (int)rest.length(); ++i) {
      if (rest[i] != ' ') continue;
      if (g().textWidth(rest.substring(0, i)) <= width) best = i; else break;
    }
    if (count == max_lines - 1 || best <= 0) {
      lines[count++] = fit(rest, width);
      break;
    }
    lines[count++] = rest.substring(0, best);
    rest = rest.substring(best + 1);
    rest.trim();
  }
  return count;
}

void wifi_icon(int x, int y, int bars) {
  for (int i = 0; i < 4; ++i) {
    const int h = 3 + i * 3;
    const uint16_t c = bars < 0 ? BORDER : (i < bars ? TEAL : BORDER);
    g().fillRect(x + i * 5, y + 12 - h, 3, h, c);
  }
  if (bars < 0) { g().drawLine(x, y, x + 18, y + 12, BAD); }
}

void battery_icon(int x, int y, int percent) {
  g().drawRect(x, y, 22, 11, DIM);
  g().fillRect(x + 22, y + 3, 2, 5, DIM);
  if (percent >= 0) {
    const uint16_t c = percent > 40 ? OK : percent > 15 ? WARN : BAD;
    g().fillRect(x + 2, y + 2, (18 * percent) / 100, 7, c);
  }
}

void draw_header(const net_view_t& v) {
  const String sig = v.clock + "|" + v.wifi_bars + "|" + v.battery_percent;
  if (!full && sig == last_header) return;
  last_header = sig;
  g().fillRect(0, 0, W, 26, PANEL);
  g().drawFastHLine(0, 26, W, BORDER);
  g().setTextDatum(lgfx::middle_left);
  g().setFont(&fonts::FreeSansBold9pt7b);
  g().setTextColor(ACCENT, PANEL);
  g().drawString("CX8ANM", 8, 13);
  g().setFont(F12);
  g().setTextColor(DIM, PANEL);
  g().drawString("RADIO INTERNET", 88, 13);
  g().setFont(F16);
  g().setTextColor(TEXT, PANEL);
  g().setTextDatum(lgfx::middle_right);
  g().drawString(v.clock.length() ? v.clock : "--:--", 252, 14);
  wifi_icon(258, 7, v.wifi_bars);
  battery_icon(286, 8, v.battery_percent);
}

const char* state_text(const player_status_t& p, uint16_t* color) {
  switch (p.state) {
    case PLAYER_CONNECTING: *color = WARN; return "Conectando...";
    case PLAYER_PLAYING: *color = OK; return "En el aire";
    case PLAYER_ERROR: *color = BAD; return p.error[0] ? p.error : "Error";
    default: *color = DIM; return "Detenido";
  }
}

void draw_station(const net_view_t& v) {
  uint16_t color;
  const char* state = state_text(v.player, &color);
  String detail;
  if (v.player.state == PLAYER_PLAYING) {
    detail = String(v.player.codec);
    if (v.player.bitrate) detail += "  " + String(v.player.bitrate / 1000) + " kbps";
  }
  const String sig = String(v.index) + "|" + v.count + "|" + v.station + "|" + state + "|" + detail + "|" + v.player.icy_name;
  if (!full && sig == last_station) return;
  last_station = sig;
  g().fillRect(0, 27, W, 76, BG);
  g().setTextDatum(lgfx::top_left);
  g().setFont(F12);
  g().setTextColor(DIM, BG);
  char idx[24];
  snprintf(idx, sizeof idx, "EMISORA %02d/%02d", v.count ? v.index + 1 : 0, v.count);
  g().drawString(idx, 10, 33);
  if (v.player.icy_name[0] && strcmp(v.player.icy_name, v.station.c_str())) {
    g().setTextDatum(lgfx::top_right);
    g().drawString(fit(String(v.player.icy_name), 190), W - 10, 33);
    g().setTextDatum(lgfx::top_left);
  }
  g().setFont(F24B);
  g().setTextColor(TEXT, BG);
  g().drawString(fit(v.station.length() ? v.station : String("Sin emisoras"), W - 20), 10, 50);
  g().fillCircle(15, 89, 4, color);
  g().setFont(F16);
  g().setTextColor(color, BG);
  g().drawString(state, 26, 81);
  if (detail.length()) {
    g().setTextColor(TEAL, BG);
    g().setTextDatum(lgfx::top_right);
    g().drawString(detail, W - 10, 81);
    g().setTextDatum(lgfx::top_left);
  }
}

void draw_title(const net_view_t& v) {
  const String sig = String(v.player.title) + "|" + v.notice;
  if (!full && sig == last_title) return;
  last_title = sig;
  g().fillRect(0, 103, W, 78, BG);
  g().fillRoundRect(8, 106, W - 16, 72, 6, PANEL);
  g().drawRoundRect(8, 106, W - 16, 72, 6, BORDER);
  g().setTextDatum(lgfx::top_left);
  g().setFont(F12);
  if (v.notice.length()) {
    g().setTextColor(ACCENT, PANEL);
    g().drawString("AVISO", 18, 112);
  } else {
    g().setTextColor(DIM, PANEL);
    g().drawString("SONANDO", 18, 112);
  }
  g().setFont(F16);
  g().setTextColor(v.notice.length() ? ACCENT : TEXT, PANEL);
  String lines[3];
  const String text = v.notice.length() ? v.notice : (v.player.title[0] ? String(v.player.title) : String("-"));
  const int n = wrap(text, W - 36, lines, 3);
  for (int i = 0; i < n; ++i) g().drawString(lines[i], 18, 126 + i * 17);
}

void draw_volume(const net_view_t& v) {
  const String sig = String(v.volume) + "|" + v.muted;
  if (!full && sig == last_volume) return;
  last_volume = sig;
  g().fillRect(0, 181, W, 28, BG);
  g().setTextDatum(lgfx::middle_left);
  g().setFont(F12);
  g().setTextColor(DIM, BG);
  g().drawString("VOL", 10, 195);
  const int x0 = 40, seg = 10;
  for (int i = 0; i < 21; ++i) {
    const uint16_t c = v.muted ? BORDER : (i < v.volume ? (i < 14 ? TEAL : ACCENT) : BORDER);
    g().fillRect(x0 + i * (seg + 2), 188 + (20 - i) / 4, seg, 14 - (20 - i) / 4, c);
  }
  g().setFont(F16);
  g().setTextDatum(lgfx::middle_right);
  g().setTextColor(v.muted ? BAD : TEXT, BG);
  g().drawString(v.muted ? "MUDO" : String(v.volume), W - 8, 195);
}

void draw_footer(const net_view_t& v) {
  if (!full && v.footer == last_footer) return;
  last_footer = v.footer;
  g().fillRect(0, 210, W, 30, PANEL);
  g().drawFastHLine(0, 210, W, BORDER);
  g().setTextDatum(lgfx::middle_center);
  g().setFont(F12);
  g().setTextColor(DIM, PANEL);
  g().drawString(fit(v.footer, W - 12), W / 2, 225);
}

void draw_browse(const net_view_t& v) {
  String sig = "B" + String(v.browse_index) + "|" + v.count + "|" + v.index;
  if (!full && sig == last_overlay) return;
  last_overlay = sig;
  g().fillRect(0, 27, W, 183, BG);
  g().setTextDatum(lgfx::top_left);
  g().setFont(F12);
  g().setTextColor(ACCENT, BG);
  g().drawString("ELEGIR EMISORA  -  pulsar para escuchar", 10, 32);
  constexpr int rows = 5, top = 50, h = 31;
  int first = v.browse_index - rows / 2;
  if (first > v.count - rows) first = v.count - rows;
  if (first < 0) first = 0;
  g().setFont(F16);
  for (int r = 0; r < rows && first + r < v.count; ++r) {
    const int i = first + r, y = top + r * h;
    const bool sel = i == v.browse_index;
    if (sel) g().fillRoundRect(6, y, W - 12, h - 3, 5, ACCENT);
    g().setTextColor(sel ? BG : (i == v.index ? TEAL : TEXT), sel ? ACCENT : BG);
    char num[6];
    snprintf(num, sizeof num, "%02d", i + 1);
    g().drawString(num, 14, y + 6);
    g().drawString(fit(v.names[i], W - 70), 44, y + 6);
    if (i == v.index && !sel) g().fillCircle(W - 18, y + 14, 4, TEAL);
  }
}

void draw_info(const net_view_t& v) {
  String sig = "I";
  for (const auto& l : v.info_lines) sig += l + "|";
  if (!full && sig == last_overlay) return;
  last_overlay = sig;
  g().fillRect(0, 27, W, 183, BG);
  g().setTextDatum(lgfx::top_left);
  g().setFont(F12);
  g().setTextColor(ACCENT, BG);
  g().drawString("INFORMACIÓN  -  tecla A para volver", 10, 32);
  g().setFont(F16);
  for (int i = 0; i < 7; ++i) {
    g().setTextColor(i < 4 ? TEXT : DIM, BG);
    g().drawString(fit(v.info_lines[i], W - 20), 10, 52 + i * 22);
  }
}
} // namespace

void screen_begin(uint8_t brightness) {
  g().fillScreen(BG);
  board_display_set_brightness(brightness);
  full = true;
}
void screen_set_brightness(uint8_t brightness) { board_display_set_brightness(brightness); }
void screen_invalidate(void) { full = true; last_overlay = ""; }

void screen_draw(const net_view_t& v) {
  static net_overlay_t shown = OVERLAY_NONE;
  if (v.overlay != shown) { shown = v.overlay; full = true; last_overlay = ""; }
  g().startWrite();
  draw_header(v);
  if (v.overlay == OVERLAY_BROWSE) draw_browse(v);
  else if (v.overlay == OVERLAY_INFO) draw_info(v);
  else { draw_station(v); draw_title(v); draw_volume(v); }
  draw_footer(v);
  g().endWrite();
  full = false;
}

void screen_setup(const char* ap_ssid, const char* ap_password, const char* ip, const char* reason) {
  auto& d = g();
  d.startWrite();
  d.fillScreen(BG);
  d.fillRect(0, 0, W, 26, PANEL);
  d.setTextDatum(lgfx::middle_left);
  d.setFont(&fonts::FreeSansBold9pt7b);
  d.setTextColor(ACCENT, PANEL);
  d.drawString("CX8ANM", 8, 13);
  d.setFont(F12);
  d.setTextColor(DIM, PANEL);
  d.drawString("CONFIGURACIÓN WiFi", 88, 13);
  d.setTextDatum(lgfx::top_left);
  d.setFont(F16);
  d.setTextColor(WARN, BG);
  d.drawString(fit(String(reason ? reason : ""), W - 20), 10, 34);
  d.setTextColor(TEXT, BG);
  d.drawString("1. En el celular, conectate a la red:", 10, 60);
  d.setFont(F24B);
  d.setTextColor(ACCENT, BG);
  d.drawString(ap_ssid, 24, 80);
  d.setFont(F16);
  d.setTextColor(TEXT, BG);
  d.drawString(String("   clave: ") + ap_password, 10, 108);
  d.drawString("2. Abrí en el navegador:", 10, 134);
  d.setFont(F24B);
  d.setTextColor(TEAL, BG);
  d.drawString(String("http://") + ip, 24, 154);
  d.setFont(F12);
  d.setTextColor(DIM, BG);
  d.drawString("3. Elegí tu WiFi y guardá. La radio se reinicia.", 10, 188);
  d.drawString("Pulsar rueda: reintentar   -   rueda 3 seg: apagar", 10, 206);
  d.drawString("Tecla A (2 seg): volver al receptor RF", 10, 222);
  d.endWrite();
  full = true;
}

void screen_connecting(const char* ssid, int seconds) {
  auto& d = g();
  d.startWrite();
  d.fillRect(0, 150, W, 90, BG);
  d.setTextDatum(lgfx::middle_center);
  d.setFont(F16);
  d.setTextColor(TEXT, BG);
  d.drawString(fit(String("Conectando a ") + ssid, W - 20), W / 2, 165);
  String dots;
  for (int i = 0; i < (seconds % 6) + 1; ++i) dots += " .";
  d.setTextColor(TEAL, BG);
  d.fillRect(0, 180, W, 24, BG);
  d.drawString(String(seconds) + " s" + dots, W / 2, 190);
  d.endWrite();
  full = true;
}

void screen_progress(const char* title, int percent) {
  auto& d = g();
  static bool drawn = false;
  d.startWrite();
  if (percent <= 0 || !drawn) {
    d.fillScreen(BG);
    d.setTextDatum(lgfx::middle_center);
    d.setFont(F24B);
    d.setTextColor(ACCENT, BG);
    d.drawString(title, W / 2, 80);
    d.setFont(F16);
    d.setTextColor(TEXT, BG);
    d.drawString("No apagues la radio", W / 2, 112);
    d.drawRoundRect(30, 140, W - 60, 22, 6, BORDER);
    drawn = true;
  }
  const int p = percent < 0 ? 0 : percent > 100 ? 100 : percent;
  d.fillRoundRect(33, 143, (W - 66) * p / 100, 16, 4, ACCENT);
  d.fillRect(0, 172, W, 26, BG);
  d.setTextDatum(lgfx::middle_center);
  d.setFont(F16);
  d.setTextColor(TEXT, BG);
  d.drawString(String(p) + " %", W / 2, 184);
  d.endWrite();
  full = true;
}
