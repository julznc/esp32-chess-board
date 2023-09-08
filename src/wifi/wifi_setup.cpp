#include <atomic>
#include <Preferences.h>
#include <sntp.h>
#include <esp_wifi.h>

#include "globals.h"
#include "ui/display.h"

#include "web_server_cfg.h"
#include "web_server.h"

#include "wifi_setup_cfg.h"
#include "wifi_setup.h"


#define SHOW_STATUS(msg, ...)       DISPLAY_CLEAR_ROW(10, 8); \
                                    DISPLAY_TEXT1(0, 10, msg, ## __VA_ARGS__)

#define MAX_FOUND_STATIONS          (16)
#define MAX_CONNECT_RETRIES         (8)
#define MS_CONNECT_TIMEOUT          (15 * 1000UL)


namespace wifi
{

static String               hostname = AP_SSID_PREFIX; // soft-ap name
static String               host_ssid;
static IPAddress            ip;
static Preferences          configs;
static std::atomic<bool>    b_ntp_connected;
static volatile uint32_t    ms_soft_ap;     // AP fallback start timestamp

static enum {
    WIFI_STATE_INIT,
    WIFI_STATE_SCAN,
    WIFI_STATE_CONNECT,
    WIFI_STATE_AP,      // soft-AP allback
    WIFI_STATE_LOOP,
    WIFI_STATE_IDLE
} e_state;

static struct {
    struct {
        uint8_t     u8_idx;         // scan index, both for ssid & bssid
        char        passphrase[64]; // password
    } as_ap[MAX_FOUND_STATIONS];    // AP records
    uint8_t         u8_connect;     // connect index
    uint8_t         u8_retry;       // connect retry counter
    uint8_t         u8_total;       // total stations found
    uint32_t        ms_connected;   // timestamp last connected to internet
} s_found_stations;



static void init()
{
    static bool     b_init = false;
    wifi_config_t   conf;

    if (!b_init)
    {
        memset(&s_found_stations, 0, sizeof(s_found_stations));

        b_init = true;
        WiFi.persistent(false);
        WiFi.setAutoReconnect(false);

      #if 0
        esp_wifi_connect();
        esp_wifi_restore();
      #endif

        (void)WiFi.begin(); // ignore initial errors (e.g. no ssid)
    }
    else
    {
        WiFi.mode(WIFI_STA);
    }

    memset(&conf, 0, sizeof(conf));
    if (ESP_OK != esp_wifi_set_config(WIFI_IF_STA, &conf))
    {
        LOGW("failed to clear wifi sta config");
    }

    WiFi.disconnect();
    WiFi.setSleep(false);

    b_ntp_connected = false;
}

static inline bool scan()
{
    String cfg_ssid;
    String cfg_passwd;
    wifi::get_credentials(cfg_ssid, cfg_passwd);

    if (cfg_ssid.isEmpty() || cfg_passwd.isEmpty())
    {
        LOGW("no configured wifi credentials");
        SHOW_STATUS("No Wi-Fi config");
        delay(3000);
        return false;
    }

    LOGD("Scanning local WiFi networks ...");
    SHOW_STATUS("scanning Wi-Fi...");
    int stationsFound = WiFi.scanNetworks();
    LOGD("%i networks found", stationsFound);

    memset(&s_found_stations.as_ap, 0, sizeof(s_found_stations.as_ap));
    s_found_stations.u8_connect = 0;
    s_found_stations.u8_total   = 0;

    for (int i = 0; i < stationsFound; ++i)
    {
        String  bssid = WiFi.BSSIDstr(i);
        String  ssid  = WiFi.SSID(i);
        int     rssi  = WiFi.RSSI(i);
        PRINTF("%2d. %s %s (%idBm)", i + 1, bssid.c_str(), ssid.c_str(), rssi);

        if (cfg_ssid.length() && cfg_passwd.length() && (ssid == cfg_ssid))
        {
            PRINTF(" - known!");
            // store idx and pw
            s_found_stations.as_ap[s_found_stations.u8_total].u8_idx = i; // store
            strncpy(s_found_stations.as_ap[s_found_stations.u8_total].passphrase, cfg_passwd.c_str(), 64 - 1);
            // increment
            s_found_stations.u8_total++;
        }

        PRINTF("\r\n");

        if (s_found_stations.u8_total >= MAX_FOUND_STATIONS) {
            break;
        }
    }

    return s_found_stations.u8_total > 0 ? true : false;
}

static inline void connect()
{
    //LOGD("connect %u/%u retry %u/%u", s_found_stations.u8_connect, s_found_stations.u8_total, s_found_stations.u8_retry + 1, MAX_CONNECT_RETRIES);
    if (s_found_stations.u8_connect >= s_found_stations.u8_total)
    {
        if (++s_found_stations.u8_retry >= MAX_CONNECT_RETRIES)
        {
            // all attempts failed
            LOGW("max connect retries");
          #if 0 // toggle radio
            WiFi.mode(WIFI_OFF);
          #endif
            s_found_stations.u8_retry = 0;
        }
        s_found_stations.u8_connect = 0;
        e_state = WIFI_STATE_INIT; // try rescan again
    }
    else
    {
        uint8_t         u8_idx     = s_found_stations.as_ap[s_found_stations.u8_connect].u8_idx;
        String          ssid       = WiFi.SSID(u8_idx);
        String          mac        = WiFi.BSSIDstr(u8_idx);
        const char     *passphrase = s_found_stations.as_ap[s_found_stations.u8_connect].passphrase;
        const char     *ssid_str   = ssid.c_str();
        const uint8_t  *bssid      = WiFi.BSSID(u8_idx);

        LOGD("connecting to AP #%d %s %s-%s (%u/%u)...", u8_idx + 1, mac.c_str(), ssid_str,
            passphrase, s_found_stations.u8_retry + 1, MAX_CONNECT_RETRIES);
        SHOW_STATUS("%s ...", ssid.c_str());

#if 0 // test to fail
        if (bssid && (0x43 == bssid[5])) {
            LOGW("expected to fail");
            passphrase = "wrongpass";
        }
#endif

        int status = WiFi.begin(ssid_str, passphrase, 0, bssid);
        LOGD("status=%d", status);

        // Wait to connect, or timeout
        unsigned long start = millis();
        while (millis() - start <= MS_CONNECT_TIMEOUT) {
            status = WiFi.status();
            PRINTF("%d", status);
            if ((WL_CONNECTED == status) || (WL_CONNECT_FAILED == status)) {
                break;
            }
            delay(500);
        }
        PRINTF("\r\n");

        if (WL_CONNECTED == status)
        {
            ip = WiFi.localIP();
            LOGI("connected to %s %s %d.%d.%d.%d", WiFi.BSSIDstr().c_str(),
                WiFi.SSID().c_str(), ip[0], ip[1], ip[2], ip[3]);
            SHOW_STATUS("%s %d.%d", ssid, ip[2], ip[3]); // hide "192.168" octet

            // set NTP
            configTime(PH_GMT_OFFSET_SEC, PH_DAYLIGHT_OFFSET_SEC, NTP_SERVERS[0], NTP_SERVERS[1], NTP_SERVERS[2]);

            s_found_stations.u8_retry = 0;
            host_ssid   = ssid;
            ms_soft_ap  = 0;
            e_state     = WIFI_STATE_LOOP;
        }
        else
        {
            // try next
            s_found_stations.u8_connect++;
            if (WL_CONNECT_FAILED == status) {
                LOGW("failed"); // e.g. AP was turned off, so just wait. before reconnecting
                delay(3000);
            }
        }
    }
}

static inline void soft_ap()
{
    LOGI("setting up AccessPoint \"%s\" (pw \"%s\")", hostname.c_str(), AP_PASSPHRASE);
    if (!WiFi.mode(WIFI_AP))
    {
        LOGW("set soft-ap mode failed");
        e_state = WIFI_STATE_INIT;
    }
    else if (!WiFi.softAP(hostname.c_str(), AP_PASSPHRASE))
    {
        LOGW("config soft-ap failed");
        e_state = WIFI_STATE_INIT;
    }
    else
    {
        ip = WiFi.softAPIP();
        LOGI("IP address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        // "esp32-chess-xx-xx-xx-xx-xx-xx"
        SHOW_STATUS("%.8s %d.%d.%d.%d", hostname.c_str() + 21, ip[0], ip[1], ip[2], ip[3]);

        ms_soft_ap  = millis();
        e_state     = WIFI_STATE_LOOP;
    }
}

static inline void ntp_check(void)
{
    if (!b_ntp_connected)
    {
        for (int i = 0; i < SNTP_MAX_SERVERS; i++)
        {
            if (sntp_getreachability(i))
            {
                b_ntp_connected = true;
                LOGI("sntp(%d) \"%s\"", i+1, sntp_getservername(i));
                s_found_stations.ms_connected = millis();
#if 0
                delay(2000UL);
                set_rtc_datetime(get_datetime()); // try update rtc
#endif
                break;
            }
        }
    }
}

static inline void loop()
{
    if (ms_soft_ap)
    {
        if (millis() - ms_soft_ap > (5 * 60 * 1000))
        {
            ms_soft_ap = 0;
            WiFi.softAPdisconnect();
            LOGD("soft-ap disconnet. will retry scanning...");
            e_state = WIFI_STATE_INIT;
        }
    }
    else
    {
        static bool warned = false;
        if (WL_CONNECTED == WiFi.status())
        {
            if (warned) {
                LOGI("WiFi reconnected");
                warned = false;
            }
            ntp_check();
        }
        else
        {
            if (!warned) {
                LOGW("WiFi disconnected, retrying");
                SHOW_STATUS("disconnected");
                warned = true;
            }
            b_ntp_connected = false;
            e_state = WIFI_STATE_INIT;
        }
    }
}

static void taskWiFi(void *)
{
    bool b_server_running = false;
    b_ntp_connected = false;

    web::serverSetup();

    WDT_WATCH(NULL);
    delay(2500UL);

    for (;;)
    {
        WDT_FEED();

        switch (e_state)
        {
        case WIFI_STATE_INIT:
            if (b_server_running)
            {
                web::serverStop();
                b_server_running = false;
            }
            init();
            e_state = WIFI_STATE_SCAN;
            if ((0 != s_found_stations.ms_connected) && (millis() - s_found_stations.ms_connected < (5 * 60 * 1000UL)))
            {
                e_state = WIFI_STATE_CONNECT; // try reconnect only, if got recent connection
            }
            break;

        case WIFI_STATE_SCAN:
            if (scan())
            {
                e_state = WIFI_STATE_CONNECT;
            }
            else if (0 != s_found_stations.ms_connected)
            {
                // rescan if has connected previously
            }
            else
            {
                e_state = WIFI_STATE_AP; // fallback to soft-ap
            }
            break;

        case WIFI_STATE_CONNECT:
            connect();
            break;

        case WIFI_STATE_AP:
            soft_ap();
            break;

        case WIFI_STATE_LOOP:
            loop();
            if (!b_server_running)
            {
                web::serverBegin();
                b_server_running = true;
            }
            web::serverHandle();
            break;

        default:
            e_state = WIFI_STATE_INIT;
            break;
        }

        delay(5);
    }
}


void setup(void)
{
    String mac = WiFi.macAddress();
    mac.toLowerCase();
    mac.replace(':', '-');

    hostname += "-";
    hostname += mac;
    LOGI("Wi-Fi hostname: %s", hostname.c_str());

    e_state = WIFI_STATE_INIT;

    if (!configs.begin("wifi-cfg", false))
    {
        LOGE("load configs failed");
    }
    else
    {
        xTaskCreate(
            taskWiFi,       /* Task function. */
            "taskWiFi",     /* String with name of task. */
            16*1024,        /* Stack size in bytes. */
            NULL,           /* Parameter passed as input of the task */
            3,              /* Priority of the task. */
            NULL);          /* Task handle. */
    }
}

bool set_credentials(const char *ssid, const char *passwd)
{
    if (!configs.putString("ssid", ssid) ||
        !configs.putString("passwd", passwd))
    {
        return false;
    }
    return true;
}

bool get_credentials(String &ssid, String &passwd)
{
    ssid = configs.getString("ssid");
    passwd = configs.getString("passwd");
    return (ssid.length() > 0) && (passwd.length() > 0);
}

void disconnect()
{
    WiFi.disconnect();
}

bool connected(void)
{
    return (0 != s_found_stations.ms_connected) && (WL_CONNECTED == WiFi.status());
}

bool is_ntp_connected(void)
{
    return b_ntp_connected;
}

bool get_host(String &name, IPAddress &addr)
{
    if (0 != connected())
    {
        name = host_ssid;
        addr = ip;
        return true;
    }
    return false;
}

} // namespace wifi
