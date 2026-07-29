idf_component_register(SRCS "audiolink_data.pb.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_netif esp_timer esp_driver_uart)
