idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES i2c_config oled_printf oled_setup mqtt nvs_flash esp_netif esp_event esp_driver_gpio protocol_examples_common)