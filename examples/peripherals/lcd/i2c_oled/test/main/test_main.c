#include "unity.h"

#include <sdkconfig.h>

void app_main(void) {
#if CONFIG_ESP_SYSTEM_GDBSTUB_RUNTIME
    unity_run_all_tests();
#else
    unity_run_menu();
#endif
}
