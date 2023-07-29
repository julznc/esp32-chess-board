#ifndef __WIFI_SETUP_CFG_H__
#define __WIFI_SETUP_CFG_H__


namespace wifi
{

typedef struct {
  const char ssid[64];
  const char password[64];
} station_t;


station_t stationList[] =
{
  {"esp32-chess",   "12345678"}, // as soft-AP mode
  {"TPU4G_R3HF",    "65782324"},
  {"TP-Link_6388",  "69078880"},
  {"marius",        "j03m05c15"}
};


const char *NTP_SERVERS[] =
{
    "pool.ntp.org",
    "0.asia.pool.ntp.org",
    "ph.pool.ntp.org"
};

#define PH_GMT_OFFSET_SEC           (8 * 60 * 60)
#define PH_DAYLIGHT_OFFSET_SEC      (00)

} // namespace wifi

#endif // __WIFI_SETUP_CFG_H__
