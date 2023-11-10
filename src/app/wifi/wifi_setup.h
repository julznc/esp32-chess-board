
#pragma once


namespace wifi
{

bool init();
void loop();
bool connected();

bool set_credentials(const char *ssid, const char *passwd);
bool get_credentials(const char **ssid, const char **passwd);

} // namespace wifi
