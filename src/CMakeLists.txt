
idf_component_register(
    SRCS
        "main.cpp"
        "globals.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        app_update
        driver
        esp_partition
        esp_psram
        nvs_flash
        spi_flash
)
