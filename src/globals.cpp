
#include "globals.h"


void global_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if ((ESP_ERR_NVS_NO_FREE_PAGES == ret) ||
        (ESP_ERR_NVS_NEW_VERSION_FOUND == ret))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    const esp_app_desc_t *app_desc = esp_app_get_description();

    LOGI("--- %s (%s) ---", K_APP_NAME, app_desc->project_name);
    LOGI("--- ver: %s (%s)---", K_APP_VERSION, app_desc->version);

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

    const esp_partition_t *partition = esp_ota_get_running_partition();
    LOGI("--- run: %s ---", partition->label);

    esp_ota_img_states_t ota_state;
    if (ESP_OK == esp_ota_get_state_partition(partition, &ota_state)) {
        // Mark current app as valid
        if (ESP_OTA_IMG_PENDING_VERIFY == ota_state) {
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

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

    LOGW("%s reset", rst_reason);

    hal_gpio_init();
}
