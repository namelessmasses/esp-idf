/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"

#include "esp_log.h"

#define TAG "lvgl_demo_ui"

void example_lvgl_ui(lv_display_t *disp) {
    ESP_LOGD(TAG, "Running LVGL demo UI");
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Loaded OK.");
    lv_obj_center(label);
}
