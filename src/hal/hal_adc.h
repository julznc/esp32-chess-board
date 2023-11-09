
#pragma once

#include <esp_adc/adc_oneshot.h>
#include "hal_adc_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void hal_adc_init(void);

int adc_batt_mv(void);


#ifdef __cplusplus
}
#endif
