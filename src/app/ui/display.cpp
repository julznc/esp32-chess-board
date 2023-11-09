
#include <atomic>
#include "globals.h"
#include "display.h"


namespace ui::display
{


// to do: transfer this routine to HAL
static bool oled_i2c_write(const uint8_t *buffer, size_t buffer_len, bool stop,
                            const uint8_t *prefix_buffer, size_t prefix_len)
{
    i2c_cmd_handle_t cmd = NULL;
    esp_err_t        err = ESP_FAIL;
    uint8_t         cmd_buff[I2C_LINK_RECOMMENDED_SIZE(1)] = { 0 };

    if (NULL == (cmd = i2c_cmd_link_create_static(cmd_buff, I2C_LINK_RECOMMENDED_SIZE(1))))
    {
        //LOGW("create cmd link failed (err=%d)", err);
    }
    else if (ESP_OK != (err = i2c_master_start(cmd)))
    {
        //LOGW("start i2c failed (err=%d)", err);
    }
    else if (ESP_OK != (err = i2c_master_write_byte(cmd, (SCREEN_ADDRESS << 1) | I2C_MASTER_WRITE, true)))
    {
        //LOGW("write address failed (err=%d)", err);
    }
    else if ((NULL != prefix_buffer) && (0 != prefix_len) &&
             (ESP_OK != (err = i2c_master_write(cmd, prefix_buffer, prefix_len, true))))
    {
        //LOGW("write prefix failed (err=%d)", err);
    }
    else if ((NULL != buffer) && (0 != buffer_len) &&
             (ESP_OK != (err = i2c_master_write(cmd, buffer, buffer_len, true))))
    {
        //LOGW("write data failed (err=%d)", err);
    }
    else if ((true == stop) && (ESP_OK != (err = i2c_master_stop(cmd))))
    {
        //LOGW("stop i2c failed (err=%d)", err);
    }
    else if (ESP_OK != (err = i2c_master_cmd_begin(OLED_I2C_MASTER_PORT, cmd, 1000)))
    {
        LOGW("i2c command failed (err=%d)", err);
    }
    else
    {
        //LOGD("i2c write(%u) success", buffer_len);
    }

    i2c_cmd_link_delete_static(cmd);
    return (ESP_OK == err);
}

SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT, oled_i2c_write);
static SemaphoreHandle_t mtx             = NULL;
static std::atomic<bool> b_display_found;
static std::atomic<bool> b_batt_ok;
static uint8_t           u8_error_count  = 0;


bool init()
{
    //LOGD("%s", __PRETTY_FUNCTION__);
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        assert(NULL != mtx);

        hal_i2c_init();
    }

    b_display_found = false;
    b_batt_ok       = false;

    if (!oled.init())
    {
        LOGW("OLED not found");
        b_display_found = false;
      #if 1 // skip checking of battery level if testing with mcu-board only
        if (++u8_error_count >= 3) {
            b_batt_ok       = true;
            u8_error_count  = 0;
        }
      #endif
        delayms(5000UL);
    }
    else
    {
        oled.setTextColor(SH110X_WHITE, SH110X_BLACK);
        oled.cp437(true);

        oled.splash();
        oled.display();
        LOGD("OLED ok");
        b_display_found = true;
        u8_error_count  = 0;
    }

    return b_display_found;
}

bool lock()
{
    return b_display_found && (pdTRUE == xSemaphoreTake(mtx, portMAX_DELAY));
}

void unlock()
{
    xSemaphoreGive(mtx);
}

void showBattLevel(void)
{
    static uint32_t prev_batt   = 0;
    static bool     b_warned    = false;
    uint32_t        scaled      = adc_batt_mv(); // divided
    uint32_t        batt        = (scaled * (BATT_ADC_R1 + BATT_ADC_R2)) / BATT_ADC_R2;

    //LOGD("scaled=%lu mv=%lu", scaled, batt);

    if (prev_batt != batt)
    {
        float f_batt = (float)batt / 1000.0f; // mV to V

        DISPLAY_TEXT1(95, 0, "%.2fV", f_batt);
        b_batt_ok   = (batt >= BATT_LEVEL_MIN);
        prev_batt = batt;

        if (!b_batt_ok && !b_warned)
        {
            LOGW("low battery level");
            DISPLAY_TEXT1(10, 10, "LOW BATTERY LEVEL");
            b_warned = true;
        }
    }
}

bool battLevelOk(void)
{
    return b_batt_ok;
}

} // namespace ui::display
