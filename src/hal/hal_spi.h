
#pragma once

#include <driver/spi_master.h>

#include "hal_spi_cfg.h"


#ifdef __cplusplus
extern "C" {
#endif

void hal_fspi_init(void);

bool fspi_transfer(const uint8_t *pui8_tx_buf, uint8_t *pui8_rx_buf, uint16_t ui16_size);


#ifdef __cplusplus
}
#endif
