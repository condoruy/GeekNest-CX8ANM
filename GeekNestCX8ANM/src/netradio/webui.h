#pragma once
#include <Arduino.h>
// HTTP setup and control page (port 80), shared by both applications. In
// access-point mode a DNS server answers every name so phones open the page
// as a captive portal.
enum webui_mode_t { WEBUI_NET, WEBUI_RF };
void webui_begin(bool captive_portal, webui_mode_t mode);
void webui_update(void);
// Status JSON of the receiver (rfweb.cpp).
String rf_status_json(void);
