#ifndef __WIFI_SETUP_H__
#define __WIFI_SETUP_H__

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

namespace wifi
{

void setup(void);
bool is_ntp_connected(void);
bool get_host(String &name, IPAddress &addr);

} // namespace wifi

#endif // __WIFI_SETUP_H__
