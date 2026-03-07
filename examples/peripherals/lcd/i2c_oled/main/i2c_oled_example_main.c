/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_panel_sh1106.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"

#include "hal/lcd_types.h"

#include "driver/i2c_master.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

static const char *TAG = "example";

// Within an enum to allow use in compile time arithmetic expressions for array
// sizing, etc.
enum
{
    EXAMPLE_SH1106_H_RES         = 128,
    EXAMPLE_SH1106_V_RES         = 64,
    EXAMPLE_SH1106_ROWS_PER_BYTE = 8
};

static const uint32_t EXAMPLE_SH1106_PAGE_HEIGHT = 8;

static const uint32_t EXAMPLE_LVGL_PIXELS_PER_BYTE = 8;

static const uint32_t EXAMPLE_I2C_BUS_PORT = 0;
static const uint32_t EXAMPLE_PIN_NUM_SDA  = 21;
static const uint32_t EXAMPLE_PIN_NUM_SCL  = 22;
static const uint32_t EXAMPLE_PIN_NUM_RST  = -1;
static const uint32_t EXAMPLE_I2C_HW_ADDR  = 0x3C;

static const uint8_t EXAMPLE_OLED_FRAME_WHITE = 0xFF;
static const uint8_t EXAMPLE_OLED_FRAME_BLACK = 0x00;

static const uint32_t EXAMPLE_I2C_PROBE_TIMEOUT_MS     = 50;
static const uint32_t EXAMPLE_BOOT_CHECK_STEP_DELAY_MS = 3000;

static const uint32_t EXAMPLE_LVGL_TASK_STACK_SIZE   = (4 * 1024);
static const uint32_t EXAMPLE_LVGL_TASK_PRIORITY     = 2;
static const uint32_t EXAMPLE_LVGL_PALETTE_SIZE      = 8;
static const uint32_t EXAMPLE_LVGL_TASK_MAX_DELAY_MS = 500;

/**
 * 1/Hz = s, so 1000/Hz = ms
 * i.e., if the tick source is called every 1 ms, then the frequency is 1000
 * Hz, and the min delay is 1ms.
 *
 * 1 / Hz * 1000 = 0 with integer math.
 * 1000 / Hz = round down to the nearest integer millisecond.
 */
static const uint32_t EXAMPLE_LVGL_TASK_MIN_DELAY_MS =
    (1000 / CONFIG_FREERTOS_HZ);

static uint8_t
    s_oled_buffer[(EXAMPLE_SH1106_V_RES / EXAMPLE_SH1106_ROWS_PER_BYTE) *
                  EXAMPLE_SH1106_H_RES];

/**
 * @brief Performs a self-test on the OLED display panel
 *
 * This function executes a self-test routine to verify the OLED display
 * is functioning correctly. It may include tests such as displaying patterns,
 * checking pixel responses, or other diagnostic operations.
 *
 * @param panel Handle to the LCD panel instance to be tested
 *
 * @return void
 *
 * @note This function is typically called during initialization or diagnostics
 *       to ensure the display hardware is working properly.
 */
static void example_oled_self_test(esp_lcd_panel_handle_t panel)
{

    ESP_LOGI(TAG, "Boot check 1/3 (panel): full black frame");
    memset(s_oled_buffer, EXAMPLE_OLED_FRAME_BLACK, sizeof(s_oled_buffer));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_SH1106_H_RES,
                                              EXAMPLE_SH1106_V_RES,
                                              s_oled_buffer));

    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));

    ESP_LOGI(TAG, "Boot check 2/3 (panel): full white frame");
    memset(s_oled_buffer, EXAMPLE_OLED_FRAME_WHITE, sizeof(s_oled_buffer));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_SH1106_H_RES,
                                              EXAMPLE_SH1106_V_RES,
                                              s_oled_buffer));

    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));

    ESP_LOGI(TAG, "Boot check 3/3 (panel): checkerboard frame");
    for (size_t i = 0; i < sizeof(s_oled_buffer); i++)
    {
        // 0xAA -> 0b10101010 - for odd numbered columns
        // 0x55 -> 0b01010101 - for even numbered columns
        s_oled_buffer[i] = (i & 1U) ? 0xAA : 0x55;
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_SH1106_H_RES,
                                              EXAMPLE_SH1106_V_RES,
                                              s_oled_buffer));

    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));

    ESP_LOGI(TAG, "Boot check complete (panel): full black frame");
    memset(s_oled_buffer, EXAMPLE_OLED_FRAME_BLACK, sizeof(s_oled_buffer));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_SH1106_H_RES,
                                              EXAMPLE_SH1106_V_RES,
                                              s_oled_buffer));
}

static _lock_t s_lvgl_api_lock;

static uint32_t example_tick_source(void)
{
    return esp_timer_get_time() / 1000;
}

/**
 * @brief LVGL flush-complete callback for the ESP LCD panel I/O driver.
 *
 * This callback is invoked when an asynchronous panel I/O transfer finishes.
 * It notifies LVGL that the current display flush operation has completed by
 * calling `lv_display_flush_ready()` on the display instance provided in
 * `user_ctx`.
 *
 * @param io_panel  Handle to the LCD panel I/O instance that triggered the
 * event. Unused by this implementation.
 * @param edata     Pointer to panel I/O event data for the completed transfer.
 *                  Unused by this implementation.
 * @param user_ctx  User-provided context expected to be a valid `lv_display_t
 * *`.
 *
 * @return `false` to indicate no higher-priority task wake-up is requested.
 */
static bool
example_notify_lvgl_panel_flush_complete(esp_lcd_panel_io_handle_t io_panel,
                                         esp_lcd_panel_io_event_data_t *edata,
                                         void *user_ctx)
{
    ESP_LOGD(TAG,
             "ESP LCD panel I/O event: flush complete... notifying LVGL...");
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void example_lvgl_boot_checks(lv_display_t *disp)
{
    lv_obj_t *scr   = NULL;
    lv_obj_t *label = NULL;

    _lock_acquire(&s_lvgl_api_lock);
    {
        scr = lv_display_get_screen_active(disp);
        lv_obj_clean(scr);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        label = lv_label_create(scr);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);

        ESP_LOGI(TAG, "Boot check (LVGL): dark background, light text");
        lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(label, "Light on dark");
        lv_obj_center(label);
    }
    _lock_release(&s_lvgl_api_lock);
#if 0
    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));

    _lock_acquire(&s_lvgl_api_lock);
    {
        ESP_LOGI(TAG, "Boot check (LVGL): light background, dark text");
        lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
        lv_label_set_text(label, "Dark on light");
        lv_obj_center(label);
    }
    _lock_release(&s_lvgl_api_lock);

    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));
#endif
}

static void example_flush_lvgl_to_panel(lv_display_t    *disp,
                                        const lv_area_t *area, uint8_t *px_map)
{
    ESP_LOGD(TAG, "Flushing LVGL buffer to panel");

    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

    // This is necessary because LVGL reserves 2 x 4 bytes in the buffer, as
    // these are assumed to be used as a palette. Skip the palette here More
    // information about the monochrome, please refer to
    // https://docs.lvgl.io/9.2/porting/display.html#monochrome-displays
    px_map += EXAMPLE_LVGL_PALETTE_SIZE;

    const int32_t x1 = area->x1;
    const int32_t x2 = area->x2;
    const int32_t y1 = area->y1;
    const int32_t y2 = area->y2;
    ESP_LOGD(TAG, "px_map area: x1=%d, y1=%d, x2=%d, y2=%d", x1, y1, x2, y2);

    const int32_t area_w = x2 - x1 + 1;
    const int32_t area_h = y2 - y1 + 1;
    ESP_LOGD(TAG, "px_map area width=%d, height=%d", area_w, area_h);

    /// The number of bytes per row in the px_map
    const int32_t px_map_bytes_per_row =
        (area_w + EXAMPLE_LVGL_PIXELS_PER_BYTE - 1) /
        EXAMPLE_LVGL_PIXELS_PER_BYTE;

    ESP_LOGD(TAG, "px_map row bytes: %d", px_map_bytes_per_row);

    // LVGL format
    // - row major
    // - 1 bit per pixel for monochrome display, 8 pixels packed in one byte
    //           MSB           LSB
    // bits       7 6 5 4 3 2 1 0
    // pixels     0 1 2 3 4 5 6 7
    //           Left         Right
    //
    // i.e., given the byte containing the pixel x, the bit corresponding to the
    // pixel is (1 << (7 - (x % 8))).
    //
    // therefore. the pmap is n_rows x n_cols / 8 in size, and the byte order is
    // big endian within each byte.

    // SH1106 uses pages of 8 pixels in height.
    // A page is 128 pixels wide.
    //
    // Page = y / 8
    // Column = x
    // Pixel within page = y % 8
    // i.e.,
    // sh1106_buffer[page * 128 + column]
    // pixel is (1 << (y % 8))
    //           MSB           LSB
    // bits       7 6 5 4 3 2 1 0
    // pixels     7 6 5 4 3 2 1 0
    //           Left         Right

    for (int32_t y = y1; y <= y2; ++y)
    {
        // Offset within the LVGL pixel map for the start of this row
        const uint32_t lvgl_px_map_y_offset = (y - y1) * px_map_bytes_per_row;

        // SH1106 page for the current row
        const uint32_t sh1106_page = y / EXAMPLE_SH1106_PAGE_HEIGHT;

        // SH1106 buffer offset for the start of the current page
        const uint32_t sh1106_page_offset = sh1106_page * EXAMPLE_SH1106_H_RES;

        // SH1106 bit mask for the current row within the page
        const uint32_t sh1106_row_mask = 1 << (y % EXAMPLE_SH1106_PAGE_HEIGHT);

        for (int32_t x = x1; x <= x2; ++x)
        {
            // Offset from the beginning of the current row within the LVGL
            // pixel map for the current column
            const uint32_t lvgl_px_map_x_offset =
                (x - x1) / EXAMPLE_LVGL_PIXELS_PER_BYTE;

            // Bit mask for the current pixel within the byte in the LVGL pixel
            // map
            const uint8_t lvgl_px_map_pixel_bit_mask =
                1 << ((EXAMPLE_LVGL_PIXELS_PER_BYTE - 1) -
                      (x % EXAMPLE_LVGL_PIXELS_PER_BYTE));

            // Byte in the LVGL pixel map containing the current pixel
            // row offset + column offset
            const uint8_t lvgl_px_map_byte_containing_pixel =
                px_map[lvgl_px_map_y_offset + lvgl_px_map_x_offset];

            // Determine if the current pixel is set (1) or not (0) in the LVGL
            // pixel map by applying the bit mask to the byte containing the
            // pixel.
            uint32_t px_pixel_is_set = !!(lvgl_px_map_byte_containing_pixel &
                                          lvgl_px_map_pixel_bit_mask);

            // SH1106 byte offset for the current pixel is the page offset + the
            // column (x coordinate)
            const uint32_t sh1106_byte_offset_for_pixel =
                sh1106_page_offset + x;

            // Remove conditional by computing both possibilities and using the
            // boolean as the index to choose
            const uint8_t reset_set_bit_in_byte[] = {
                s_oled_buffer[sh1106_byte_offset_for_pixel] &
                    ~sh1106_row_mask, // reset bit
                s_oled_buffer[sh1106_byte_offset_for_pixel] |
                    sh1106_row_mask}; // set bit

            s_oled_buffer[sh1106_byte_offset_for_pixel] =
                reset_set_bit_in_byte[px_pixel_is_set];
        }
    }

    /// TODO does this assume the entire buffer is updated?
    // i.e., what is the layout of color_data relative to (x_start, y_start) and
    // (x_end, y_end)? i.e., is color_data the entire frame buffer or just the
    // area to be drawn within the display?
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1,
                                              y2 + 1, s_oled_buffer));
}

/**
 * @brief LVGL event loop task function
 *
 * This function serves as the main event loop for LVGL (Light and Versatile
 * Graphics Library). It continuously processes LVGL tasks and handles GUI
 * updates by periodically calling lv_timer_handler(). This function is
 * typically run as a FreeRTOS task to manage the graphical user interface
 * rendering and event handling.
 *
 * @param arg Pointer to task arguments (typically NULL or task-specific
 * parameters)
 *
 * @note This function runs in an infinite loop and should be created as a
 * FreeRTOS task
 */
static void example_lvgl_event_loop(void *no_args)
{
    (void)no_args;

    ESP_LOGI(TAG,
             "Starting LVGL event loop task: max delay %u ms, min delay %u ms",
             EXAMPLE_LVGL_TASK_MAX_DELAY_MS, EXAMPLE_LVGL_TASK_MIN_DELAY_MS);

    uint32_t time_till_next_ms = 0;

    while (1)
    {
        _lock_acquire(&s_lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&s_lvgl_api_lock);

        // in case of triggering a task watchdog time out
        time_till_next_ms =
            MAX(time_till_next_ms, EXAMPLE_LVGL_TASK_MIN_DELAY_MS);

        // in case of lvgl display not ready yet
        time_till_next_ms =
            MIN(time_till_next_ms, EXAMPLE_LVGL_TASK_MAX_DELAY_MS);

        usleep(1000 * time_till_next_ms);
    }
}

extern void example_lvgl_ui(lv_display_t *disp);

void app_main(void)
{
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus    = NULL;
    i2c_master_bus_config_t bus_config = {
        .i2c_port          = EXAMPLE_I2C_BUS_PORT,
        .sda_io_num        = EXAMPLE_PIN_NUM_SDA,
        .scl_io_num        = EXAMPLE_PIN_NUM_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority     = 0,
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = true, .allow_pd = false}};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_LOGI(TAG, "Probing I2C device at address 0x%02X", EXAMPLE_I2C_HW_ADDR);
    esp_err_t probe_ret = i2c_master_probe(i2c_bus, EXAMPLE_I2C_HW_ADDR,
                                           EXAMPLE_I2C_PROBE_TIMEOUT_MS);
    if (probe_ret == ESP_OK)
    {
        ESP_LOGI(TAG, "I2C probe success at 0x%02X", EXAMPLE_I2C_HW_ADDR);
    }
    else
    {
        ESP_LOGW(TAG, "I2C probe failed at 0x%02X: %s", EXAMPLE_I2C_HW_ADDR,
                 esp_err_to_name(probe_ret));
    }

    ESP_LOGI(TAG, "Install SH1106 panel I/O I2C: (%dx%d)", EXAMPLE_SH1106_H_RES,
             EXAMPLE_SH1106_V_RES);
    esp_lcd_panel_io_handle_t     io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_SH1106_DEFAULT_IO_CONFIG;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SH1106 panel driver");
    esp_lcd_panel_handle_t     panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
        .rgb_ele_order =
            LCD_RGB_ELEMENT_ORDER_RGB, // Not used for monochrome panel, but set
                                       // to a default value to avoid potential
                                       // issues
        .data_endian =
            LCD_RGB_DATA_ENDIAN_LITTLE, // Not used for monochrome panel, but
                                        // set to a default value to avoid
                                        // potential issues
        .bits_per_pixel = SH1106_PIXELS_PER_BYTE /
                          8, // 1 bit per pixel for monochrome display
        .flags =
            {
                .reset_active_high =
                    0, // Most OLEDs are reset by pulling the reset pin low, so
                       // set reset_active_high to 0
            },
        .vendor_config = NULL};

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_sh1106(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Performing OLED self-test");
    example_oled_self_test(panel_handle);

    ESP_LOGI(TAG, "Initializing LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Install LVGL millisecond tick source");
    lv_tick_set_cb(&example_tick_source);

    ESP_LOGD(TAG, "Creating LVGL display");
    lv_display_t *display =
        lv_display_create(EXAMPLE_SH1106_H_RES, EXAMPLE_SH1106_V_RES);

    ESP_LOGD(TAG, "Setting LVGL display user data to the panel handle");
    lv_display_set_user_data(display, panel_handle);

    ESP_LOGD(TAG, "Setting LVGL color format for monochrome SH1106 display");
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);

    size_t draw_buffer_sz = EXAMPLE_SH1106_H_RES * EXAMPLE_SH1106_V_RES /
                                EXAMPLE_LVGL_PIXELS_PER_BYTE +
                            EXAMPLE_LVGL_PALETTE_SIZE;

    ESP_LOGD(TAG, "Allocating LVGL draw buffer of size %d bytes",
             draw_buffer_sz);
    void *buf = heap_caps_calloc(1, draw_buffer_sz,
                                 MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(buf);
    memset(buf, 0, draw_buffer_sz);

    ESP_LOGD(TAG, "Setting LVGL display buffers and render mode full");
    lv_display_set_buffers(display, buf, NULL, draw_buffer_sz,
                           LV_DISPLAY_RENDER_MODE_FULL);

    ESP_LOGI(TAG,
             "Register LVGL callback for flushing display buffer to the panel");
    lv_display_set_flush_cb(display, example_flush_lvgl_to_panel);

    ESP_LOGI(TAG, "Register ESP LCD panel for flush to panel complete");
    const esp_lcd_panel_io_callbacks_t esp_lcd_panel_callbacks = {
        .on_color_trans_done = example_notify_lvgl_panel_flush_complete,
    };
    esp_lcd_panel_io_register_event_callbacks(
        io_handle, &esp_lcd_panel_callbacks, display);

    ESP_LOGI(TAG, "Creating LVGL event loop task");
    xTaskCreate(example_lvgl_event_loop, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE,
                NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Performing LVGL boot checks");
    example_lvgl_boot_checks(display);

#if 0
    ESP_LOGI(TAG, "Display LVGL UI");
    _lock_acquire(&s_lvgl_api_lock);
    example_lvgl_ui(display);
    _lock_release(&s_lvgl_api_lock);
#endif
    ESP_LOGI(TAG, "Ending app_main");
}
