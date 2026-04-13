idf_component_register(
    SRCS 
        "kiwi/I2C.cpp"
        "kiwi/I2CFactory.cpp"
        "kiwi/personal/sht41/sht41.c" 
        "kiwi/personal/sht41/SHT41.cpp"
        "kiwi/personal/ads1115/ads1115.c"
        "kiwi/personal/ads1115/ADS1115.cpp"
        "lvgl_demo_ui.c" 
        "i2c_oled_example_main.cpp"
        "watchdog_util.c"
        "sensor_data.c"
    REQUIRES 
        esp_driver_i2c 
        esp_lcd 
        esp_timer
)

