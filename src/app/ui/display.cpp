
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
        LOGD("i2c write(%u) success", buffer_len);
    }

    i2c_cmd_link_delete_static(cmd);
    return (ESP_OK == err);
}

SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT, oled_i2c_write);
static SemaphoreHandle_t mtx             = NULL;


bool init()
{
    //LOGD("%s", __PRETTY_FUNCTION__);
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        assert(NULL != mtx);

        hal_i2c_init();
    }

    oled.init();

    return true;
}

} // namespace ui::display
