idf_component_register(SRCS "main.cpp" "network.cpp" "protocol.cpp" "door_controller.cpp" "hardware.cpp" "node_link.cpp" "reporting.cpp"
    INCLUDE_DIRS "../include" "../../shared/protocol"
    REQUIRES esp_wifi esp_event esp_netif nvs_flash mqtt json esp_timer esp_psram spi_flash lwip mbedtls esp_driver_uart)
