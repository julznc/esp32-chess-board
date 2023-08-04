
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

#include <Wire.h>
#include "globals.h"


void global_init(void)
{
    HWSERIAL.begin(115200);
    HWSERIAL.setDebugOutput(true);

    // disable brownout detector
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // watchdog
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true); // enable panic so ESP32 restarts

    // I/O init
    pinMode(LED_BUILTIN, OUTPUT);
    Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);

    const char *rst_reason = "UNKNOWN";
    switch (esp_reset_reason())
    {
      #define RST_CASE(cause) case ESP_RST_ ## cause: rst_reason = #cause; break
        RST_CASE(POWERON);
        RST_CASE(EXT);
        RST_CASE(SW);
        RST_CASE(PANIC);
        RST_CASE(INT_WDT);
        RST_CASE(TASK_WDT);
        RST_CASE(WDT);
        RST_CASE(DEEPSLEEP);
        RST_CASE(BROWNOUT);
        RST_CASE(SDIO);
        default: break; // unknown
    }

    PRINTF("\r\n");
    LOGI("--- %s ---", K_APP_NAME);
    LOGI("--- ver: %s ---", K_APP_VERSION);

    uint8_t au8_base_mac[8];
    if (ESP_OK == esp_efuse_mac_get_default(au8_base_mac))
    {
        LOGI("--- mac: %02x:%02x:%02x:%02x:%02x:%02x ---",
            au8_base_mac[0], au8_base_mac[1], au8_base_mac[2], au8_base_mac[3], au8_base_mac[4], au8_base_mac[5]);
    }
    else
    {
        LOGW("--- mac: (unknown) ---");
    }

    LOGW("--- rst: %s ---\r\n", rst_reason);
}

void dump_bytes(const void *pv, size_t sz)
{
    const uint8_t *pb;
    pb = (const uint8_t *)pv;
    while (sz--) {
        PRINTF("%02x ", *pb++);
    }
    PRINTF("\r\n");
}
