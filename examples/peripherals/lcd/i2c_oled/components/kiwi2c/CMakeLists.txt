set(KIWI2C_SRCS
    "kiwi2c/I2CFactory.cpp"
)

# Include the correct driver implemnetation depending on how the user configured the component in menuconfig
if(I2CDEV_USE_LEGACY_DRIVER)
    list(APPEND KIWI2C_SRCS "kiwi2c/I2CLegacy.cpp")
else()
    list(APPEND KIWI2C_SRCS "kiwi2c/I2C.cpp")
endif()

idf_component_register(
    SRCS ${KIWI2C_SRCS}
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_driver_i2c
)
