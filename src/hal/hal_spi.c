
#include <string.h>

#include "hal_gpio.h"
#include "hal_spi.h"


static spi_device_handle_t fspi;


void hal_fspi_init(void)
{
    esp_err_t ret;
    spi_bus_config_t buscfg = {
        .miso_io_num     = PIN_RFID_MISO,
        .mosi_io_num     = PIN_RFID_MOSI,
        .sclk_io_num     = PIN_RFID_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = RFID_SPI_MAX_TRANSFER_SZ
    };

    spi_device_interface_config_t devcfg = {
        //.duty_cycle_pos  = 128,
        .clock_speed_hz  = RFID_SPI_CLOCK_SPEED_HZ,
        //.input_delay_ns  = 8,
        .mode            = RFID_SPI_MODE,
        .spics_io_num    = PIN_RFID_CS,
        .queue_size      = 1,
        .flags           = SPI_DEVICE_POSITIVE_CS, // inverted CS pin logic
    };

#if 0 // https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/api-reference/peripherals/spi_slave.html#restrictions-and-known-issues
    ret = spi_bus_initialize(FSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
#else
    ret = spi_bus_initialize(FSPI_HOST, &buscfg, SPI_DMA_DISABLED);
#endif
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(FSPI_HOST, &devcfg, &fspi);
    ESP_ERROR_CHECK(ret);
}

bool fspi_transfer(const uint8_t *pui8_tx_buf, uint8_t *pui8_rx_buf, uint16_t ui16_size)
{
    spi_transaction_t   t;
    esp_err_t           ret;

    memset(&t, 0, sizeof(t));
    t.length    = ui16_size * 8; // number of bits
    t.tx_buffer = pui8_tx_buf;
    t.rx_buffer = pui8_rx_buf;

#if 1 // https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/api-reference/peripherals/spi_master.html#driver-usage
    ret = spi_device_polling_transmit(fspi, &t);
#else
    ret = spi_device_transmit(fspi, &t);
#endif

    return (ESP_OK == ret);
}
