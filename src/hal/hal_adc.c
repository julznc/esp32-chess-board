
#include "hal_gpio.h"
#include "hal_adc.h"


static adc_oneshot_unit_handle_t    adc_handle = NULL;
static adc_cali_handle_t            calib_handle = NULL;

void hal_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_BATT,
        //.clk_src = ...,
        //.ulp_mode = ...
    };
    adc_oneshot_chan_cfg_t channel_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_BATT, &channel_cfg));

    adc_cali_line_fitting_config_t calib_cfg = {
        .unit_id = ADC_UNIT_BATT,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&calib_cfg, &calib_handle));
}


int adc_batt_mv(void)
{
    int adc_raw = 0;
    int voltage = 0;

    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_BATT, &adc_raw));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(calib_handle, adc_raw, &voltage));

    return voltage;
}
