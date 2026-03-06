idf_component_register(SRCS "lvgl_demo_ui.c" "i2c_oled_example_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2c esp_lcd esp_timer)
