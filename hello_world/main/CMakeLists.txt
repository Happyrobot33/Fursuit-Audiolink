idf_component_register(SRCS "audiolink_data.pb.c" "hello_world_main.c"                             "audiolink_data.pb.c"                       PRIV_REQUIRES spi_flash
                       PRIV_REQUIRES esp_driver_gpio
                       PRIV_REQUIRES esp_wifi
                       PRIV_REQUIRES nvs_flash
                       PRIV_REQUIRES esp_event
                       PRIV_REQUIRES esp_netif
                       INCLUDE_DIRS "")
