idf_component_register(SRCS "main.c" "handlers/wifi.c" "handlers/http.c" "handlers/ai.c" "handlers/config.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash esp_http_client esp_driver_uart esp_driver_gpio cjson esp-tls mbedtls
    PRIV_REQUIRES spi_flash)
