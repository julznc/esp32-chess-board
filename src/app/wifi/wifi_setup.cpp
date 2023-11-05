
#include <esp_wifi.h>

#include "globals.h"

#include "web/web_server.h"
#include "wifi_setup_cfg.h"
#include "wifi_setup.h"


namespace wifi
{

static struct {
    nvs_handle_t    nvs;
    char            ac_ssid[64];
    char            ac_passwd[64];
} s_configs;

static enum {
    WIFI_STATE_INIT,
    WIFI_STATE_SCAN,
    WIFI_STATE_CONNECT,
    WIFI_STATE_AP,      // soft-AP allback
    WIFI_STATE_CHECK    // loop
} e_state;

static wifi_ap_record_t     as_scanned_aps[SCAN_LIST_SIZE];
static EventGroupHandle_t   s_wifi_event_group;
static esp_netif_t         *sta_netif = NULL;
static esp_netif_t         *ap_netif = NULL;


#define BSSIDSTR            "%02x:%02x:%02x:%02x:%02x:%02x"
#define BSSID2STR(buf6)     buf6[0], buf6[1], buf6[2], buf6[3], buf6[4], buf6[5]

#define WIFI_CONNECTED_BIT  BIT0    // we are connected to the AP with an IP
#define WIFI_FAIL_BIT       BIT1    // we failed to connect after the maximum amount of retries


static struct {
    struct {
        uint8_t     u8_idx;         // scan index, both for ssid & bssid
        const char  *password;      // passphrase
    } as_ap[MAX_FOUND_APS];         // AP records
    uint8_t         u8_connect;     // connect index
    uint8_t         u8_retry;       // connect retry counter
    uint8_t         u8_total;       // total stations found
    bool            b_connected;    // connected to AP (and w/ IP)
} s_found_aps;
static uint32_t     ms_soft_ap;     // AP fallback start timestamp
static char         ac_host_info[32];   // sta host name & ip

static void start(); // init
static void scan();
static void connect();
static void soft_ap();
static void check();


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    static uint8_t u8_retry_connect = 0;
    //LOGD("%s (id=%ld)", event_base, event_id);

    switch (event_id)
    {
    case WIFI_EVENT_SCAN_DONE:
        LOGD("scan done");
        break;

    case WIFI_EVENT_STA_START:
        LOGD("STA start");
        u8_retry_connect = 0;
        break;

    case WIFI_EVENT_STA_STOP:
        LOGD("STA stop");
        break;

    case WIFI_EVENT_STA_CONNECTED:
        LOGD("STA connected");
        u8_retry_connect = 0;
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        if (++u8_retry_connect > MAX_CONNECT_RETRIES) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            LOGW("connect to the AP failed");
        } else {
            LOGW("retry to connect to the AP (%u/%u)", u8_retry_connect, MAX_CONNECT_RETRIES);
            esp_wifi_connect();
        }
        break;

    case WIFI_EVENT_AP_START:
        LOGD("AP start");
        break;

    case WIFI_EVENT_AP_STOP:
        LOGD("AP stop");
        break;

    case WIFI_EVENT_AP_STACONNECTED:
        LOGD("AP connected");
        break;

    default:
        LOGW("not handled %s (%ld)", event_base, event_id);
        break;
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    wifi_ap_record_t ap_info;
    ip_event_got_ip_t *got_ip = (ip_event_got_ip_t*)event_data;

    LOGD("%s (id=%ld)", event_base, event_id);

    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        esp_wifi_sta_get_ap_info(&ap_info);
        LOGI("IP addr: " IPSTR, IP2STR(&got_ip->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        snprintf(ac_host_info, sizeof(ac_host_info) - 1,
                "%.16s - .%d.%d", ap_info.ssid,
                esp_ip4_addr3_16(&got_ip->ip_info.ip), // hide "192.168" octets
                esp_ip4_addr4_16(&got_ip->ip_info.ip));
        break;
    case IP_EVENT_STA_LOST_IP: // see NETIF_IP_LOST_TIMER_INTERVAL
        LOGW("lost ip");
        s_found_aps.b_connected = false;
        break;
    default:
        LOGW("not handled %s (%ld)", event_base, event_id);
        break;
    }
}


bool init()
{
    //LOGD("%s()", __func__);

    wifi_init_config_t  init_cfg    = WIFI_INIT_CONFIG_DEFAULT();
    bool                b_status    = false;

    esp_event_handler_instance_t    evt_wifi;
    esp_event_handler_instance_t    evt_ip;

    memset(&s_configs, 0, sizeof(s_configs));
    memset(&s_found_aps, 0, sizeof(s_found_aps));
    memset(ac_host_info, 0, sizeof(ac_host_info));

    e_state = WIFI_STATE_INIT;

    if (NULL == (s_wifi_event_group = xEventGroupCreate()))
    {
        LOGW("xEventGroupCreate() failed");
    }
    else if (ESP_OK != esp_netif_init())
    {
        LOGW("netif_init() failed");
    }
    else if (ESP_OK != esp_event_loop_create_default())
    {
        LOGW("event_loop_create_default() failed");
    }
    else if (NULL == (sta_netif = esp_netif_create_default_wifi_sta()))
    {
        LOGW("netif_create_default_wifi_sta() failed");
    }
    else if (NULL == (ap_netif = esp_netif_create_default_wifi_ap()))
    {
        LOGW("netif_create_default_wifi_sta() failed");
    }
    else if (ESP_OK != esp_wifi_init(&init_cfg))
    {
        LOGW("wifi_init() failed");
    }
    else if (ESP_OK != esp_wifi_set_storage(WIFI_STORAGE_RAM)) // disable persistent
    {
        LOGW("wifi_set_storage(ram) failed");
    }
    else if (ESP_OK != esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler, NULL,
                                                            &evt_wifi))
    {
        LOGW("register wifi event handler failed");
    }
    else if (ESP_OK != esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                                            &ip_event_handler, NULL,
                                                            &evt_ip))
    {
        LOGW("register ip event handler failed");
    }
    else if(ESP_OK != nvs_open("wifi-cfg", NVS_READWRITE, &s_configs.nvs))
    {
        LOGW("nvs_open failed");
    }
    else if (false == web::server::start())
    {
        LOGW("web_server failed");
    }
    else
    {
        LOGD("%s() ok", __func__);
        b_status = true;
    }

#if 0 // set initial wifi credentials
    set_credentials("myssid", "mypasswd");
    delayms(1000);
#endif

    return b_status;
}

void loop()
{
    switch (e_state)
    {
    case WIFI_STATE_INIT:
        start();
        break;

    case WIFI_STATE_SCAN:
        scan();
        break;

    case WIFI_STATE_CONNECT:
        connect();
        break;

    case WIFI_STATE_AP:
        soft_ap();
        break;

    case WIFI_STATE_CHECK:
        check();
        break;

    default:
        e_state = WIFI_STATE_INIT;
        break;
    }
}

bool set_credentials(const char *ssid, const char *passwd)
{
    esp_err_t   err;
    bool        b_status = false;

    if ((NULL == ssid) && (NULL == passwd))
    {
        //
    }
    else if (ESP_OK != (err = nvs_set_str(s_configs.nvs, "ssid", ssid)))
    {
        LOGW("nvs_set_str() = %d", err);
    }
    else if (ESP_OK != (err = nvs_set_str(s_configs.nvs, "passwd", passwd)))
    {
        LOGW("nvs_set_str() = %d", err);
    }
    else if (ESP_OK != (err = nvs_commit(s_configs.nvs)))
    {
        LOGW("nvs_set_str() = %d", err);
    }
    else
    {
        b_status = true;
    }

    return b_status;
}

bool get_credentials(const char **ssid, const char **passwd)
{
    esp_err_t   err;
    size_t      ssid_len = sizeof(s_configs.ac_ssid);
    size_t      passwd_len = sizeof(s_configs.ac_ssid);
    bool        b_status = false;

    if (ESP_OK != (err = nvs_get_str(s_configs.nvs, "ssid", s_configs.ac_ssid, &ssid_len)))
    {
        LOGW("nvs_get_str() = %d", err);
    }
    else if (ESP_OK != (err = nvs_get_str(s_configs.nvs, "passwd", s_configs.ac_passwd, &passwd_len)))
    {
        LOGW("nvs_get_str() = %d", err);
    }
    else
    {
        if ((NULL != ssid) && (NULL != passwd))
        {
            *ssid = s_configs.ac_ssid;
            *passwd = s_configs.ac_passwd;
        }

        //LOGD("cfg: \"%s\" - \"%s\"", s_configs.ac_ssid, s_configs.ac_passwd);
        b_status = s_configs.ac_ssid[0] && s_configs.ac_passwd[0];
    }

    return b_status;
}

static void start()
{
    if (ESP_OK != esp_wifi_set_mode(WIFI_MODE_STA))
    {
        LOGW("wifi_set_mode(STA) failed");
    }
    else if (ESP_OK != esp_wifi_start())
    {
        LOGW("wifi_start() failed");
    }
    else
    {
        e_state = WIFI_STATE_SCAN;
    }
}

static void scan()
{
    uint16_t ap_count = SCAN_LIST_SIZE;

    memset(as_scanned_aps, 0, sizeof(as_scanned_aps));
    s_found_aps.u8_connect = 0;
    s_found_aps.u8_total   = 0;

    (void)esp_wifi_clear_ap_list();
    if (false == get_credentials(NULL, NULL))
    {
        LOGW("no wi-fi credentials yet");
        e_state = WIFI_STATE_AP;
    }
    else if (ESP_OK != esp_wifi_scan_start(NULL, true))
    {
        LOGW("wifi_scan_start() failed");
        (void)esp_wifi_clear_ap_list();
    }
    else if (ESP_OK != esp_wifi_scan_get_ap_records(&ap_count, as_scanned_aps))
    {
        LOGW("wifi_scan_get_ap_records() failed");
    }
    else
    {
        //LOGD("ap count=%u", ap_count);
        for (uint16_t i = 0; (i < SCAN_LIST_SIZE) && (i < ap_count); i++)
        {
            wifi_ap_record_t *ps_ap  = &as_scanned_aps[i];

            PRINTF("%2d. " BSSIDSTR " - %s (%idBm)", i + 1, BSSID2STR(ps_ap->bssid), ps_ap->ssid, ps_ap->rssi);

            if (0 == strcmp(s_configs.ac_ssid, (const char *)as_scanned_aps[i].ssid))
            {
                PRINTF(" - known!");
                // store idx and pw
                s_found_aps.as_ap[s_found_aps.u8_total].u8_idx   = i; // store
                s_found_aps.as_ap[s_found_aps.u8_total].password = s_configs.ac_passwd;
                // increment
                s_found_aps.u8_total++;
                break;
            }

            if (s_found_aps.u8_total >= MAX_FOUND_APS) {
                break;
            }

            PRINTF("\r\n");
        }

        if (s_found_aps.u8_total)
        {
            e_state = WIFI_STATE_CONNECT;
        }
        else
        {
            e_state = WIFI_STATE_AP;
        }
    }
}

static void connect()
{
    wifi_config_t       wifi_cfg;
    wifi_sta_config_t  *ps_sta = &wifi_cfg.sta;
    wifi_ap_record_t   *ps_ap  = NULL;

    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    (void)esp_wifi_stop();

    if (s_found_aps.u8_connect >= s_found_aps.u8_total)
    {
        if (++s_found_aps.u8_retry >= MAX_CONNECT_RETRIES)
        {
            // all attempts failed
            LOGE("max connect retries");
            s_found_aps.u8_retry = 0;
            e_state = WIFI_STATE_AP; // fallback to soft-ap
        }
        else
        {
            LOGW("retry scan APs (%u/%u)", s_found_aps.u8_retry, MAX_CONNECT_RETRIES);
            e_state = WIFI_STATE_INIT; // try rescan again
        }
    }
    else
    {
        uint8_t     u8_idx   = s_found_aps.as_ap[s_found_aps.u8_connect].u8_idx;
        const char *password = s_found_aps.as_ap[s_found_aps.u8_connect].password;

        ps_ap = &as_scanned_aps[u8_idx];
        memcpy(ps_sta->ssid, ps_ap->ssid, sizeof(ps_sta->ssid));
        memcpy(ps_sta->bssid, ps_ap->bssid, sizeof(ps_sta->ssid));
        memcpy(ps_sta->password, password, sizeof(ps_sta->password));
        ps_sta->bssid_set = true;

#if 0 // test to fail
        if (0x43 == ps_ap->bssid[5]) {
            LOGW("expected to fail");
            memcpy(ps_sta->password, "wrongpass", 10);
        }
#endif

        if (ESP_OK != esp_wifi_set_mode(WIFI_MODE_STA))
        {
            LOGW("wifi_set_mode(STA) failed");
        }
        else if (ESP_OK != esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg))
        {
            LOGW("wifi_set_config(STA) failed");
        }
        else if (ESP_OK != esp_wifi_set_ps(WIFI_PS_NONE)) // disable power save
        {
            LOGW("wifi_set_ps(0) failed");
        }
        else if (ESP_OK != esp_wifi_start())
        {
            LOGW("wifi_start() failed");
        }
        else if (ESP_OK != esp_wifi_connect())
        {
            LOGW("wifi_connect() failed");
        }
        else
        {
            LOGD("connecting to AP #%d " BSSIDSTR " (%s:%s) ...", u8_idx + 1, BSSID2STR(ps_sta->bssid), ps_sta->ssid, ps_sta->password);
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                pdFALSE, pdFALSE, CONNECT_TIMEOUT_MS);

            if (bits & WIFI_CONNECTED_BIT)
            {
                s_found_aps.b_connected = true;
                s_found_aps.u8_retry    = 0;
                ms_soft_ap              = 0;
                e_state = WIFI_STATE_CHECK;
            }
            else // failed or timed out
            {
                LOGW("Failed to connect");
                // try next AP
                s_found_aps.u8_connect++;
            }
        }
    }
}

static void soft_ap()
{
    esp_netif_ip_info_t ip_info;
    wifi_config_t       wifi_cfg;
    wifi_ap_config_t   *ps_ap = &wifi_cfg.ap;
    uint8_t             mac[6];

    (void)esp_wifi_get_mac(WIFI_IF_AP, mac);

    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    (void)esp_wifi_stop();

    snprintf((char *)ps_ap->ssid, sizeof(ps_ap->ssid) - 1,
        "%s-%02x%02x%02x%02x%02x%02x", AP_SSID_PREFIX, BSSID2STR(mac));
    strncpy((char *)ps_ap->password, AP_PASSPHRASE, sizeof(ps_ap->password) - 1);
    ps_ap->ssid_len         = strlen((char *)ps_ap->ssid);
    ps_ap->channel          = 1;
    ps_ap->max_connection   = 4;
    ps_ap->authmode         = WIFI_AUTH_WPA2_PSK;
    ps_ap->pmf_cfg.required = true;

    e_state = WIFI_STATE_INIT; // fallback

    if (ESP_OK != esp_wifi_set_mode(WIFI_MODE_AP))
    {
        LOGW("wifi_set_mode(AP) failed");
    }
    else if (ESP_OK != esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg))
    {
        LOGW("wifi_set_config(AP) failed");
    }
    else if (ESP_OK != esp_wifi_start())
    {
        LOGW("wifi_start() failed");
    }
    else
    {
        LOGI("soft AP: %s", ps_ap->ssid);

        if (ESP_OK == esp_netif_get_ip_info(ap_netif, &ip_info))
        {
            LOGI("IP aadr: " IPSTR, IP2STR(&ip_info.ip));
        }
        else
        {
            LOGW("failed to get IP info");
        }

        ms_soft_ap  = millis();
        e_state     = WIFI_STATE_CHECK;
    }
}

static void check()
{
    if (ms_soft_ap)
    {
        if (millis() - ms_soft_ap > (5 * 60 * 1000))
        {
            ms_soft_ap = 0;
            (void)esp_wifi_stop();
            LOGD("soft-ap disconnet. will retry scanning...");
            e_state = WIFI_STATE_INIT;
        }
    }
    else
    {
        static bool warned = false;
        if (true == s_found_aps.b_connected)
        {
            if (warned) {
                LOGI("WiFi reconnected");
                warned = false;
            }
        }
        else
        {
            if (!warned) {
                LOGW("WiFi disconnected, retrying");
                warned = true;
            }
            (void)esp_wifi_stop();
            e_state = WIFI_STATE_INIT;
        }
    }
}

} // namespace wifi
