#include "watchdog_util.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <sdkconfig.h>

static const char *TAG = "watchdog_reset";

static const uint32_t k_WATCHDOG_YIELD_TIME_MS = 10;

const char *watchdog_current_task_name(TaskHandle_t task_handle) {
    const char *task_name = pcTaskGetName(task_handle);
    return (task_name != NULL) ? task_name : "<unnamed>";
}

esp_err_t watchdog_subscribe_current_task_if_needed(TaskHandle_t task_handle,
                                                    const char  *task_name) {
#if defined(CONFIG_ESP_TASK_WDT_EN)
    esp_err_t tmp_status_ret = esp_task_wdt_status(NULL);
    if (tmp_status_ret == ESP_OK) {
        return ESP_OK;
    }

    if (tmp_status_ret == ESP_ERR_NOT_FOUND) {
        esp_err_t tmp_add_ret = esp_task_wdt_add(NULL);

        if (tmp_add_ret == ESP_OK) {
            ESP_LOGI(TAG,
                     "TWDT subscribed task=%s handle=%p core=%d",
                     task_name,
                     task_handle,
                     xPortGetCoreID());
            return ESP_OK;
        }

        if (tmp_add_ret == ESP_ERR_INVALID_ARG) {
            ESP_LOGD(TAG,
                     "TWDT already subscribed task=%s handle=%p core=%d",
                     task_name,
                     task_handle,
                     xPortGetCoreID());
            return ESP_OK;
        }

        return tmp_add_ret;
    }

    return tmp_status_ret;
#else
    return ESP_OK;
#endif
}

void watchdog_reset(void) {
#if defined(CONFIG_ESP_TASK_WDT_EN)
    TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();
    const char  *task_name   = watchdog_current_task_name(task_handle);

    esp_err_t tmp_subscribe_ret =
        watchdog_subscribe_current_task_if_needed(task_handle, task_name);

    static bool s_logged_uninitialized_once = false;
    if (tmp_subscribe_ret == ESP_ERR_INVALID_STATE) {
        if (!s_logged_uninitialized_once) {
            s_logged_uninitialized_once = true;
            ESP_LOGW(TAG,
                     "Task Watchdog is not initialized yet; "
                     "task=%s handle=%p core=%d",
                     task_name,
                     task_handle,
                     xPortGetCoreID());
        }
        return;
    }

    if (tmp_subscribe_ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "Task Watchdog subscribe failed for task=%s handle=%p "
                 "core=%d: %s",
                 task_name,
                 task_handle,
                 xPortGetCoreID(),
                 esp_err_to_name(tmp_subscribe_ret));
        return;
    }

    esp_err_t tmp_reset_ret = esp_task_wdt_reset();
    if (tmp_reset_ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "Task Watchdog reset failed for task=%s handle=%p "
                 "core=%d: %s",
                 task_name,
                 task_handle,
                 xPortGetCoreID(),
                 esp_err_to_name(tmp_reset_ret));
        return;
    }

    ESP_LOGV(TAG,
             "Task Watchdog reset task=%s handle=%p core=%d",
             task_name,
             task_handle,
             xPortGetCoreID());
#endif
}

void yield_for_watchdog() {
    watchdog_reset();
    vTaskDelay(pdMS_TO_TICKS(k_WATCHDOG_YIELD_TIME_MS));
}
