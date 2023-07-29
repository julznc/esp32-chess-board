#include <sntp.h>

#include "globals.h"

#if 0 // to do
#include "ui/display.h"
#include "web_server_cfg.h"
#include "web_server.h"
#else

#define DISPLAY_TEXT1(...)
#define DISPLAY_CLEAR_ROW(...)

#define WEB_SERVER_HTTP_PORT        (80)

#endif

#include "wifi_setup_cfg.h"
#include "wifi_setup.h"


namespace wifi
{


static const int stationCount = sizeof(stationList)/sizeof(stationList[0]);
static IPAddress ip;
static String hostname = stationList[0].ssid;
static int foundNetwork = -1;
static bool accesspoint = false;
static volatile bool ntp_connected = false;


static int scanNetworks(void)
{
  int bestStation = -1;
  int bestRSSI = -999;

  LOGD("Scanning local WiFi networks ...");
  DISPLAY_TEXT1(10, 120, "scanning Wi-Fi ...");
  int stationsFound = WiFi.scanNetworks();
  LOGI("%i networks found", stationsFound);
  DISPLAY_CLEAR_ROW(120, 8);

  for (int i = 0; i < stationsFound; ++i)
  {
    String thisSSID = WiFi.SSID(i);
    int thisRSSI = WiFi.RSSI(i);
    PRINTF("%2d: %s (%i)", i + 1, thisSSID.c_str(), thisRSSI);
    for (int j = 0; j < stationCount; j++)
    {
      if (strcmp(stationList[j].ssid, thisSSID.c_str()) == 0)
      {
        PRINTF("  -  known!");
        if (thisRSSI > bestRSSI)
        {
          bestStation = j;
          bestRSSI = thisRSSI;
        }
      }
    }
    PRINTF("\r\n");
  }

  return bestStation;
}

static void wifiConnect(void)
{
  foundNetwork = scanNetworks();
  if (foundNetwork < 0)
  {
    LOGW("No known networks found, entering AccessPoint fallback mode");
    accesspoint = true;
  }
  else
  {
    LOGD("Connecting to WiFi Network (%d) %s ...", foundNetwork + 1, stationList[foundNetwork].ssid);
    DISPLAY_TEXT1(2, 120, "%s connecting ...", stationList[foundNetwork].ssid);

    WiFi.disconnect();
    // Initiate network connection request (3rd argument, channel = 0 is 'auto')
    int status = WiFi.begin(stationList[foundNetwork].ssid, stationList[foundNetwork].password);
    //WiFi.setTxPower(WIFI_POWER_8_5dBm);
    LOGD("status = %d", status);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(false);

    // Wait to connect, or timeout
    unsigned long start = millis();
    while ((millis() - start <= (15 * 1000)) && (WL_CONNECTED != (status = WiFi.status()))) {
        PRINTF("%d", status);
        delay(500);
    }
    PRINTF("\r\n");
    DISPLAY_CLEAR_ROW(120, 8);
    if (status == WL_CONNECTED)
    {
      LOGD("Client connection succeeded");
      WiFi.setAutoReconnect(true);
      ip = WiFi.localIP();
      LOGI("IP address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      //DISPLAY_TEXT1(2, 120, "%s %s", stationList[foundNetwork].ssid, WiFi.localIP().toString().c_str());
      DISPLAY_TEXT1(2, 120, "%s - %d.%d", stationList[foundNetwork].ssid, ip[2], ip[3]); // hide "192.168" octet

      configTime(PH_GMT_OFFSET_SEC, PH_DAYLIGHT_OFFSET_SEC, NTP_SERVERS[0], NTP_SERVERS[1], NTP_SERVERS[2]);
#if 1
      if (!MDNS.begin(hostname.c_str())) {
        LOGW("Error setting up MDNS responder!");
        while(1) { delay(1000); }
      }
      LOGD("mDNS responder started");
      MDNS.addService("http", "tcp", WEB_SERVER_HTTP_PORT);
#endif
    }
    else
    {
      LOGE("Client connection Failed");
      WiFi.disconnect(true, true);   // resets the WiFi scan
      WiFi.setAutoReconnect(false);
      //WiFi.disconnect();
      DISPLAY_TEXT1(2, 120, "connect failed");
    }
  }

  if (accesspoint && (WiFi.status() != WL_CONNECTED))
  {
    LOGI("Setting up AccessPoint \"%s\" (pw \"%s\")", hostname.c_str(), stationList[0].password);
    WiFi.softAP(hostname.c_str(), stationList[0].password);
    ip = WiFi.softAPIP();
    LOGI("IP address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    DISPLAY_TEXT1(2, 120, "soft-AP %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  }
}

static void ntpCheck(void)
{
  if (!ntp_connected)
  {
    for (int i = 0; i < SNTP_MAX_SERVERS; i++)
    {
      if (sntp_getreachability(i))
      {
        ntp_connected = true;
        LOGI("sntp(%d) \"%s\"", i+1, sntp_getservername(i), sntp_getreachability(i));
        break;
      }
    }
  }
}

static void taskWiFi(void *parameter)
{
  wifiConnect();
#if 0
  web::serverSetup();
#endif
  WDT_WATCH(NULL);

  for (;;)
  {
#if 0
    web::serverHandle();
#endif
    if (accesspoint)
    {
      //
    }
    else
    {
      static bool warned = false;
      if (WiFi.status() == WL_CONNECTED)
      {
        if (warned) {
          LOGI("WiFi reconnected");
          warned = false;
        }
        ntpCheck();
      }
      else
      {
        if (!warned)
        {
          WiFi.disconnect();
          LOGW("WiFi disconnected, retrying");
          DISPLAY_TEXT1(2, 120, " disconnected ");
          warned = true;
        }
        ntp_connected = false;
        wifiConnect();
      }
    }
    WDT_FEED();
    delay(10);
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

    xTaskCreate(
        taskWiFi,         /* Task function. */
        "taskWiFi",       /* String with name of task. */
        8192,             /* Stack size in bytes. */
        NULL,             /* Parameter passed as input of the task */
        3,                /* Priority of the task. */
        NULL);            /* Task handle. */
}

bool is_ntp_connected(void)
{
  return ntp_connected;
}

bool get_host(String &name, IPAddress &addr)
{
  if (foundNetwork >= 0)
  {
    name = stationList[foundNetwork].ssid;
    addr = ip;
    return true;
  }
  return false;
}

} // namespace wifi
