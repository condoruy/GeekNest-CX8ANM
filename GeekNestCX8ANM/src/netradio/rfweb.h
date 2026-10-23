#pragma once
// CX8ANM: WiFi and web control while the tinyradio receiver runs (RF mode).
// Connects with the WiFi saved in internet-radio mode; never opens an access
// point here. Disabled with the "WiFi en modo RF" setting of the web page.
#ifdef __cplusplus
extern "C" {
#endif
void rfweb_begin(void);
void rfweb_update(void);
#ifdef __cplusplus
}
#endif
