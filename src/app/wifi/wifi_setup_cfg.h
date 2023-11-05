
#pragma once

namespace wifi
{

// STA mode
#define MAX_FOUND_APS               (8)
#define SCAN_LIST_SIZE              (32)
#define MAX_CONNECT_RETRIES         (10)
#define CONNECT_TIMEOUT_MS          (15*1000UL)

// Soft-AP
const char *AP_SSID_PREFIX  = "esp32-chess";
const char *AP_PASSPHRASE   = "12345678";


} // namespace wifi
