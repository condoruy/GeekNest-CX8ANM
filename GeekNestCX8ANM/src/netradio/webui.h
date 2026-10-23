#pragma once
// HTTP setup and control page (port 80). In access-point mode a DNS server
// answers every name so phones open the page as a captive portal.
void webui_begin(bool captive_portal);
void webui_update(void);
