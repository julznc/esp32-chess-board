#ifndef __WIFI_SETUP_CFG_H__
#define __WIFI_SETUP_CFG_H__


namespace wifi
{

// Soft-AP
const char *AP_SSID_PREFIX  = "esp32-chess";
const char *AP_PASSPHRASE   = "12345678";


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
