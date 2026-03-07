/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"

#include "core/lv_obj.h"
#include "core/lv_obj_style_gen.h"
#include "display/lv_display.h"
#include "esp_log.h"
#include "misc/lv_color.h"
#include "misc/lv_style_gen.h"
#include "misc/lv_text.h"

#define TAG "lvgl_demo_ui"

static lv_style_t s_style;

void example_lvgl_ui(lv_display_t *disp)
{
    ESP_LOGD(TAG, "Running LVGL demo UI");

    lv_style_set_bg_color(&s_style, lv_color_black());
    lv_style_set_text_color(&s_style, lv_color_white());
    lv_style_set_text_font(&s_style, &lv_font_unscii_8);
    lv_style_set_text_align(&s_style, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_clean(scr);
    lv_obj_add_style(scr, &s_style, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);

    lv_label_set_text(label, "Loaded OK.");
    lv_obj_center(label);
}
