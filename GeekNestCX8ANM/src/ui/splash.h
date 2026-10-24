#pragma once
#include <stdint.h>
#define CX8_FIRMWARE_NAME "CX8ANM"
#define CX8_FIRMWARE_VERSION "1.1.2"
#ifdef __cplusplus
extern "C" {
#endif
// Boot screen with the CX8ANM identity. Draws directly on the panel; call
// after board_display_begin() and with the backlight on. mode_line names the
// application that is starting ("Receptor RF", "Radio Internet"); hint is a
// short control reminder shown below it.
void cx8_splash(const char* mode_line, const char* hint);
// Full-screen one-line message (used just before switching modes).
void cx8_message(const char* title, const char* detail);
#ifdef __cplusplus
}
#endif
