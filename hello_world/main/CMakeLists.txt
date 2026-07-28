idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES spi_flash
                       PRIV_REQUIRES esp_driver_gpio
                       PRIV_REQUIRES esp_timer
                       INCLUDE_DIRS "")
