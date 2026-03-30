/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "lvgl.h"

#include "ads1115.h"
#include "core/lv_obj.h"
#include "core/lv_obj_style.h"
#include "core/lv_obj_style_gen.h"
#include "display/lv_display.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_sh1106.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "esp_timer.h"
#include "font/lv_font.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "layouts/grid/lv_grid.h"
#include "layouts/lv_layout.h"
#include "lv_api_map_v8.h"
#include "misc/lv_area.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "misc/lv_style_gen.h"
#include "misc/lv_text.h"
#include "misc/lv_timer.h"
#include "sht41.h"
#include "widgets/scale/lv_scale.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

static const char *TAG = "example";

// Within an enum to allow use in compile time arithmetic expressions for array
// sizing, etc.
enum
{
    EXAMPLE_SH1106_H_RES         = SH1106_WIDTH,
    EXAMPLE_SH1106_V_RES         = SH1106_HEIGHT,
    EXAMPLE_SH1106_ROWS_PER_BYTE = 8,
    EXAMPLE_LVGL_PIXELS_PER_BYTE = 8,
    EXAMPLE_LVGL_PALETTE_SIZE    = 8
};

static const uint32_t EXAMPLE_SH1106_PAGE_HEIGHT = SH1106_PIXELS_PER_BYTE;

static const uint32_t EXAMPLE_I2C_BUS_PORT = 0;
static const uint32_t EXAMPLE_PIN_NUM_SDA  = 21;
static const uint32_t EXAMPLE_PIN_NUM_SCL  = 22;
static const uint32_t EXAMPLE_PIN_NUM_RST  = -1;

static const uint8_t EXAMPLE_OLED_FRAME_WHITE = 0xFF;
static const uint8_t EXAMPLE_OLED_FRAME_BLACK = 0x00;

static const uint32_t EXAMPLE_BOOT_CHECK_STEP_DELAY_MS = 500;

static const uint32_t EXAMPLE_LVGL_TASK_STACK_SIZE   = (4 * 1024);
static const uint32_t EXAMPLE_LVGL_TASK_PRIORITY     = 2;
static const uint32_t EXAMPLE_LVGL_TASK_MAX_DELAY_MS = 500;

static const uint32_t sc_poll_sensors_period_ms = 500;

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
    s_SH1106_framebuffer[(EXAMPLE_SH1106_V_RES / EXAMPLE_SH1106_ROWS_PER_BYTE) *
                         EXAMPLE_SH1106_H_RES];

static uint8_t s_LVGL_framebuffer[EXAMPLE_SH1106_H_RES * EXAMPLE_SH1106_V_RES /
                                      EXAMPLE_LVGL_PIXELS_PER_BYTE +
                                  EXAMPLE_LVGL_PALETTE_SIZE];

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
    ESP_LOGI(TAG, "Starting OLED self-test routine...default state");
    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));

    ESP_LOGI(TAG, "Boot check 1/3 (panel): full black frame");
    memset(s_SH1106_framebuffer, EXAMPLE_OLED_FRAME_BLACK,
           sizeof(s_SH1106_framebuffer));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_SH1106_H_RES,
                                              EXAMPLE_SH1106_V_RES,
                                              s_SH1106_framebuffer));

    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));

    ESP_LOGI(TAG, "Boot check 2/3 (panel): full white frame");
    memset(s_SH1106_framebuffer, EXAMPLE_OLED_FRAME_WHITE,
           sizeof(s_SH1106_framebuffer));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_SH1106_H_RES,
                                              EXAMPLE_SH1106_V_RES,
                                              s_SH1106_framebuffer));

    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));

    ESP_LOGI(TAG, "Boot check 3/3 (panel): checkerboard frame");
    for (size_t i = 0; i < sizeof(s_SH1106_framebuffer); i++)
    {
        // 0xAA -> 0b10101010 - for odd numbered columns
        // 0x55 -> 0b01010101 - for even numbered columns
        s_SH1106_framebuffer[i] = (i & 1U) ? 0xAA : 0x55;
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_SH1106_H_RES,
                                              EXAMPLE_SH1106_V_RES,
                                              s_SH1106_framebuffer));

    vTaskDelay(pdMS_TO_TICKS(EXAMPLE_BOOT_CHECK_STEP_DELAY_MS));

    ESP_LOGI(TAG, "Boot check complete (panel): full black frame");
    memset(s_SH1106_framebuffer, EXAMPLE_OLED_FRAME_BLACK,
           sizeof(s_SH1106_framebuffer));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, EXAMPLE_SH1106_H_RES,
                                              EXAMPLE_SH1106_V_RES,
                                              s_SH1106_framebuffer));
}

static _lock_t s_lvgl_api_lock;

static atomic_flag s_flush_pending = ATOMIC_FLAG_INIT;

static uint32_t example_lvgl_tick_get_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
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
    if (atomic_flag_test_and_set(&s_flush_pending))
    {
        return false;
    }

    ESP_LOGD(TAG,
             "ESP LCD panel I/O event: flush complete... notifying LVGL...");
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void example_flush_lvgl_to_panel(lv_display_t    *disp,
                                        const lv_area_t *area, uint8_t *px_map)
{
    ESP_LOGD(TAG, "Flushing LVGL buffer to panel: %p", px_map);

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
                s_SH1106_framebuffer[sh1106_byte_offset_for_pixel] &
                    ~sh1106_row_mask, // reset bit
                s_SH1106_framebuffer[sh1106_byte_offset_for_pixel] |
                    sh1106_row_mask}; // set bit

            s_SH1106_framebuffer[sh1106_byte_offset_for_pixel] =
                reset_set_bit_in_byte[px_pixel_is_set];
        }
    }

    ESP_LOGD(TAG, "s_LVGL_framebuffer -> s_SH1106_framebuffer done, flushing "
                  "to panel...");

    /// TODO does this assume the entire buffer is updated?
    // i.e., what is the layout of color_data relative to (x_start, y_start) and
    // (x_end, y_end)? i.e., is color_data the entire frame buffer or just the
    // area to be drawn within the display?
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1,
                                              y2 + 1, s_SH1106_framebuffer));

    atomic_flag_clear(&s_flush_pending);
    ESP_LOGD(TAG, "Flush command issued to panel");
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

        vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
    }
}

static struct
{
    ads1115_register_t  ads1115_config;
    ads1115_register_t  ads1115_reading;
    float               voltage_data[2];
    sht41_data_t        temp_humid_data[2];
    atomic_int_fast32_t index;
} s_sensor_data = {
    .ads1115_config =
        {.address.P  = ADS1115_REG_CONFIG,
         .reg.config = {.OS   = ads1115_config_OS_WRITE_START_SINGLE_CONVERSION,
                        .MUX  = ads1115_config_MUX_AIN0_AIN3,
                        .PGA  = ads1115_config_PGA_4_096V,
                        .MODE = ads1115_config_MODE_SINGLE_SHOT,
                        .DR   = ads1115_config_DR_128SPS,
                        .COMP_MODE = ads1115_config_COMP_MODE_DEFAULT,
                        .COMP_POL  = ads1115_config_COMP_POL_DEFAULT,
                        .COMP_LAT  = ads1115_config_COMP_LAT_DEFAULT,
                        .COMP_QUE  = ads1115_config_COMP_QUE_DEFAULT}},
    .ads1115_reading = {.address.P      = ADS1115_REG_CONVERSION,
                        .reg.conversion = {0}},
    .voltage_data    = {0.f},
    .temp_humid_data = {{0}},
    .index           = -1};

typedef struct
{
    i2c_master_dev_handle_t sht41_handle;
    i2c_master_dev_handle_t ads1115_handle;
} poll_sensors_arg_t;

static float const R1            = 0.f;  // 983.f;
static float const R2            = 1.0f; // 323.f;
static float const VOLTAGE_SCALE = (R1 + R2) / R2;

static poll_sensors_arg_t s_poll_sensors_arg = {.sht41_handle   = 0,
                                                .ads1115_handle = 0};

static void poll_sensors(void *arg)
{
    poll_sensors_arg_t *poll_arg = (poll_sensors_arg_t *)arg;

    int32_t data_index = atomic_load(&s_sensor_data.index);
    ++data_index;
    data_index &= 1; // toggle between 0 and 1

    ESP_ERROR_CHECK(
        sht41_get_reading(poll_arg->sht41_handle, CMD_READ_LOW_PRECISION,
                          &s_sensor_data.temp_humid_data[data_index], 1000));

    s_sensor_data.ads1115_reading.reg.raw   = 0;
    s_sensor_data.ads1115_reading.address.P = ADS1115_REG_CONVERSION;
    ESP_ERROR_CHECK(ads1115_read_register(poll_arg->ads1115_handle,
                                          &s_sensor_data.ads1115_reading));
    ads1115_log_register(ESP_LOG_DEBUG, &s_sensor_data.ads1115_reading);

    s_sensor_data.voltage_data[data_index] =
        ads1115_get_voltage(s_sensor_data.ads1115_config.reg.config.PGA,
                            &s_sensor_data.ads1115_reading.reg.conversion) *
        VOLTAGE_SCALE;

    atomic_store(&s_sensor_data.index, data_index);

    sht41_print_data(&s_sensor_data.temp_humid_data[data_index]);
}

static struct
{
    const int32_t layout_column_dsc[3];
    const int32_t layout_row_dsc[3];
    lv_style_t    style;
    lv_obj_t     *temp;
    lv_obj_t     *rh;
    lv_obj_t     *voltage;
    lv_obj_t     *power;
} s_ui = {.layout_column_dsc = {64, 64, LV_GRID_TEMPLATE_LAST},
          .layout_row_dsc    = {32, 32, LV_GRID_TEMPLATE_LAST},
          .style             = {0},
          .temp              = NULL,
          .rh                = NULL,
          .voltage           = NULL,
          .power             = NULL};

static void ui_initialize(lv_display_t *disp)
{
    lv_style_init(&s_ui.style);
    lv_style_set_bg_color(&s_ui.style, lv_color_black());
    lv_style_set_text_color(&s_ui.style, lv_color_white());
    lv_style_set_text_font(&s_ui.style, &lv_font_unscii_8);
    lv_style_set_text_align(&s_ui.style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_column(&s_ui.style, 2);

    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_clean(scr);
    lv_obj_add_style(scr, &s_ui.style, LV_PART_MAIN);

    lv_obj_t *label1 = lv_label_create(scr);
    lv_label_set_text(label1, LV_SYMBOL_OK);
    lv_obj_add_style(label1, &s_ui.style, LV_PART_MAIN);

    vTaskDelay(pdMS_TO_TICKS(3000));
    lv_obj_clean(scr);
    lv_obj_add_style(scr, &s_ui.style, LV_PART_MAIN);

    lv_obj_set_grid_dsc_array(scr, s_ui.layout_column_dsc, s_ui.layout_row_dsc);
    lv_obj_add_style(scr, &s_ui.style, LV_PART_MAIN);

    s_ui.temp = lv_label_create(scr);
    lv_obj_add_style(s_ui.temp, &s_ui.style, LV_PART_MAIN);
    lv_label_set_text(s_ui.temp, "--.--C");
    lv_obj_set_grid_cell(s_ui.temp, LV_GRID_ALIGN_START, 0, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

    s_ui.rh = lv_label_create(scr);
    lv_obj_add_style(s_ui.rh, &s_ui.style, LV_PART_MAIN);
    lv_label_set_text(s_ui.rh, "--.--%");
    lv_obj_set_grid_cell(s_ui.rh, LV_GRID_ALIGN_START, 1, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

#if VOLTAGE_SCALE
    s_ui.voltage = lv_scale_create(scr);
    lv_obj_add_style(s_ui.voltage, &s_ui.style, LV_PART_MAIN);
    lv_scale_set_label_show(s_ui.voltage, false);
    lv_scale_set_total_tick_count(s_ui.voltage, 41);
    lv_scale_set_major_tick_every(s_ui.voltage, 10);
    lv_obj_set_style_length(s_ui.voltage, 2, LV_PART_ITEMS);
    lv_obj_set_style_length(s_ui.voltage, 4, LV_PART_INDICATOR);
    lv_scale_set_range(s_ui.voltage, 10, 14);
    lv_scale_set_angle_range(s_ui.voltage, 270);
    lv_scale_set_rotation(s_ui.voltage, 135);

    lv_obj_t *voltage_needle = lv_line_create(s_ui.voltage);
    lv_obj_set_style_line_width(voltage_needle, 2, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(voltage_needle, true, LV_PART_MAIN);
    lv_scale_set_line_needle_value(s_ui.voltage, voltage_needle, -5, 13);
#else
    s_ui.voltage = lv_label_create(scr);
    lv_obj_add_style(s_ui.voltage, &s_ui.style, LV_PART_MAIN);
    lv_label_set_text(s_ui.voltage, "--.-- V");

#endif
    lv_obj_set_grid_cell(s_ui.voltage, LV_GRID_ALIGN_CENTER, 0, 1,
                         LV_GRID_ALIGN_CENTER, 1, 1);

    s_ui.power = lv_label_create(scr);
    lv_obj_add_style(s_ui.power, &s_ui.style, LV_PART_MAIN);
    lv_label_set_text(s_ui.power, "---- W");
    lv_obj_set_grid_cell(s_ui.power, LV_GRID_ALIGN_START, 1, 1,
                         LV_GRID_ALIGN_CENTER, 1, 1);
}

static void ui_update(lv_timer_t *timer)
{
    (void)timer;

    int32_t data_index = atomic_load(&s_sensor_data.index);

    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.2fC\n%.2fF",
             s_sensor_data.temp_humid_data[data_index].temperature_celcius,
             s_sensor_data.temp_humid_data[data_index].temperature_fahrenheit);

    char rh_str[16];
    snprintf(rh_str, sizeof(rh_str), "Rel.Hu.\n%.2f%%",
             s_sensor_data.temp_humid_data[data_index].relative_humidity);

    lv_label_set_text(s_ui.temp, temp_str);
    lv_label_set_text(s_ui.rh, rh_str);

    char voltage_str[8] = {0};
    snprintf(voltage_str, sizeof(voltage_str), "%2.2fV",
             s_sensor_data.voltage_data[data_index]);

    lv_label_set_text(s_ui.voltage, voltage_str);
}

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

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

    ESP_LOGI(TAG, "Adding SHT41 to the I2C bus");
    ESP_ERROR_CHECK(sht41_bus_add_device(i2c_bus, SHT41_SENSOR_ADDR,
                                         &s_poll_sensors_arg.sht41_handle));

    ESP_LOGI(TAG, "Adding ADS1115 to the I2C bus");
    ESP_ERROR_CHECK(ads1115_bus_add_device(i2c_bus, ADS1115_SENSOR_ADDR,
                                           &s_poll_sensors_arg.ads1115_handle));

#if 0
    ESP_LOGI(TAG, "ADS1115 reading default config register");
    ads1115_register_t read_config = {.address.P      = ADS1115_REG_CONFIG,
                                      .reg.config.raw = 0};
    ESP_ERROR_CHECK(
        ads1115_read_register(s_poll_sensors_arg.ads1115_handle, &read_config));
    ads1115_log_register(ESP_LOG_INFO, &read_config);
#endif

    ESP_LOGI(TAG, "Configuring ADS1115 - writing config register");
    ads1115_log_register(ESP_LOG_INFO, &s_sensor_data.ads1115_config);
    ESP_ERROR_CHECK(ads1115_write_register(s_poll_sensors_arg.ads1115_handle,
                                           &s_sensor_data.ads1115_config));

#if 0
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Reading back ADS1115 config register after writing config");
    read_config.address.P      = ADS1115_REG_CONFIG;
    read_config.reg.config.raw = 0;
    ESP_ERROR_CHECK(
        ads1115_read_register(s_poll_sensors_arg.ads1115_handle, &read_config));
    ads1115_log_register(ESP_LOG_INFO, &read_config);
    assert(read_config.reg.config.MODE ==
           s_sensor_data.ads1115_config.reg.config.MODE);
    assert(read_config.reg.config.MUX ==
           s_sensor_data.ads1115_config.reg.config.MUX);
    assert(read_config.reg.config.PGA ==
           s_sensor_data.ads1115_config.reg.config.PGA);
    assert(read_config.reg.config.DR ==
           s_sensor_data.ads1115_config.reg.config.DR);
    assert(read_config.reg.config.COMP_MODE ==
           s_sensor_data.ads1115_config.reg.config.COMP_MODE);
    assert(read_config.reg.config.COMP_POL ==
           s_sensor_data.ads1115_config.reg.config.COMP_POL);
    assert(read_config.reg.config.COMP_LAT ==
           s_sensor_data.ads1115_config.reg.config.COMP_LAT);
    assert(read_config.reg.config.COMP_QUE ==
           s_sensor_data.ads1115_config.reg.config.COMP_QUE);
#endif

    ESP_LOGI(TAG, "Creating timer to poll sensors every %u ms",
             sc_poll_sensors_period_ms);
    esp_timer_create_args_t poll_sensors_timer_args = {
        .callback              = poll_sensors,
        .arg                   = &s_poll_sensors_arg,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "poll_sensors_timer",
        .skip_unhandled_events = true};
    esp_timer_handle_t poll_sensors_timer;
    ESP_ERROR_CHECK(
        esp_timer_create(&poll_sensors_timer_args, &poll_sensors_timer));

    ESP_LOGI(TAG, "Starting timer to poll sensors every %u ms",
             sc_poll_sensors_period_ms);
    ESP_ERROR_CHECK(esp_timer_start_periodic(
        poll_sensors_timer, sc_poll_sensors_period_ms * 1000ULL));

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
        .bits_per_pixel = 1, // 1 bit per pixel for monochrome display
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
    // ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Performing OLED self-test");
    example_oled_self_test(panel_handle);

    ESP_LOGI(TAG, "Clearing LVGL framebuffer %p", s_LVGL_framebuffer);
    memset(s_LVGL_framebuffer, 0, sizeof(s_LVGL_framebuffer));

    ESP_LOGI(TAG, "Initializing LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Configuring LVGL tick callback");
    lv_tick_set_cb(example_lvgl_tick_get_cb);

    ESP_LOGI(TAG, "Creating LVGL display");
    lv_display_t *display =
        lv_display_create(EXAMPLE_SH1106_H_RES, EXAMPLE_SH1106_V_RES);

    ESP_LOGI(TAG, "Setting LVGL display user data to the panel handle");
    lv_display_set_user_data(display, panel_handle);

    ESP_LOGI(TAG, "Setting LVGL display color format to 1-bit indexed");
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);

    ESP_LOGI(TAG, "Setting LVGL display buffers and render mode full");
    lv_display_set_buffers(display, s_LVGL_framebuffer, NULL,
                           sizeof(s_LVGL_framebuffer),
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

    // After this point access to the LVGL API must be protected by the
    // s_lvgl_api_lock mutex, as the LVGL event loop is running in a separate
    // task and may call back into user code (e.g.,
    // example_notify_lvgl_panel_flush_complete) that also needs to call LVGL
    // API functions.

    ESP_LOGI(TAG, "Initializing LVGL UI");
    ui_initialize(display);

    ESP_LOGI(TAG, "Creating LVGL event loop task");
    xTaskCreate(example_lvgl_event_loop, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE,
                NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    lv_timer_create(ui_update, 100, NULL);

    ESP_LOGI(TAG, "Ending app_main");
}
