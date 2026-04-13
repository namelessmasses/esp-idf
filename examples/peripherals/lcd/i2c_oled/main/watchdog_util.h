#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const char *watchdog_current_task_name(TaskHandle_t task_handle);

void watchdog_reset(void);

void yield_for_watchdog();
