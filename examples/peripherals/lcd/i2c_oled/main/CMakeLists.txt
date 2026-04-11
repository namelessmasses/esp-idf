idf_component_register(SRCS "kiwi/personal/sht41/sht41.c" "lvgl_demo_ui.c" "i2c_oled_example_main.c"
                       REQUIRES esp_driver_i2c esp_lcd esp_timer)

