
#include "hal_gpio.h"
#include "hal_i2c.h"


void hal_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master.clk_speed = OLED_I2C_CLK_SPEED,
        .clk_flags = 0
    };

    ESP_ERROR_CHECK( i2c_param_config(OLED_I2C_MASTER_PORT, &conf) );
    ESP_ERROR_CHECK( i2c_driver_install(OLED_I2C_MASTER_PORT, conf.mode, 0, 0, 0) );
}
