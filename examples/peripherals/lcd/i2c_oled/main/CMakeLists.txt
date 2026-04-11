idf_component_register(
    SRCS 
        "kiwi/I2C.cpp"
        "kiwi/I2CLegacy.cpp"
        "kiwi/personal/sht41/sht41.c" 
        "lvgl_demo_ui.c" 
        "i2c_oled_example_main.cpp"
    REQUIRES 
        esp_driver_i2c 
        esp_lcd 
        esp_timer
)

