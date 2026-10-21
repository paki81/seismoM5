#ifndef _web_H_
#define _web_H_

#include <ESPAsyncWebServer.h>

extern bool ap_mode;
extern float live_sta, live_lta; // Last STA / LTA values, for display only

void initWeb(AsyncWebServer& server);
void webLoop(); // Pushes live status to the browser
String apSsid();

#endif
