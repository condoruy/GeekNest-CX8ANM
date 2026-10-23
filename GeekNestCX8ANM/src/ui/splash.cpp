#include "splash.h"
#include "boards/display.hpp"

namespace {
constexpr uint16_t BG = 0x0861;      // #0B0F14-ish
constexpr uint16_t ACCENT = 0xFD20;  // amber
constexpr uint16_t TEAL = 0x2D7F;    // cyan/teal
constexpr uint16_t TEXT = 0xE73C;
constexpr uint16_t DIM = 0x8C71;

// A simple condor silhouette: two swept wings and a head, drawn with triangles.
void condor(lgfx::LGFX_Device& g, int cx, int cy, uint16_t color) {
  g.fillTriangle(cx, cy, cx - 70, cy - 22, cx - 18, cy + 6, color);
  g.fillTriangle(cx - 70, cy - 22, cx - 92, cy - 10, cx - 40, cy + 2, color);
  g.fillTriangle(cx, cy, cx + 70, cy - 22, cx + 18, cy + 6, color);
  g.fillTriangle(cx + 70, cy - 22, cx + 92, cy - 10, cx + 40, cy + 2, color);
  g.fillCircle(cx, cy - 6, 7, color);
  g.fillTriangle(cx - 6, cy + 2, cx + 6, cy + 2, cx, cy + 16, color);
  g.fillCircle(cx, cy - 7, 2, BG);
}
}

extern "C" void cx8_splash(const char* mode_line, const char* hint) {
  auto& g = board_display_device();
  g.startWrite();
  g.fillScreen(BG);
  condor(g, 160, 52, ACCENT);
  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(TEXT, BG);
  g.setFont(&fonts::FreeSansBold24pt7b);
  g.drawString(CX8_FIRMWARE_NAME, 160, 112);
  g.fillRect(70, 140, 180, 3, ACCENT);
  g.setFont(&fonts::FreeSansBold12pt7b);
  g.setTextColor(TEAL, BG);
  g.drawString(mode_line ? mode_line : "", 160, 164);
  g.setFont(&fonts::FreeSans9pt7b);
  g.setTextColor(TEXT, BG);
  g.drawString(hint ? hint : "", 160, 194);
  g.setTextColor(DIM, BG);
  g.drawString("GeekNest V5A - fw " CX8_FIRMWARE_VERSION " - 73!", 160, 220);
  g.endWrite();
}

extern "C" void cx8_message(const char* title, const char* detail) {
  auto& g = board_display_device();
  g.startWrite();
  g.fillScreen(BG);
  g.setTextDatum(lgfx::middle_center);
  g.setFont(&fonts::FreeSansBold12pt7b);
  g.setTextColor(ACCENT, BG);
  g.drawString(title ? title : "", 160, 104);
  g.setFont(&fonts::FreeSans9pt7b);
  g.setTextColor(TEXT, BG);
  g.drawString(detail ? detail : "", 160, 140);
  g.endWrite();
}
