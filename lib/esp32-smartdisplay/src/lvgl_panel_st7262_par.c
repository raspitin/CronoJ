#ifdef DISPLAY_ST7262_PAR

#include <esp32_smartdisplay.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_panel_ops.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t vsync_semaphore = NULL;
static lv_display_render_mode_t active_render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;

/* Runtime knobs:
 * - SMARTDISPLAY_AVOID_TEAR: synchronize flush start to VSYNC
 * - SMARTDISPLAY_LVGL_BUFFER_NUM: 1 or 2 draw buffers
 * - SMARTDISPLAY_FULL_REFRESH: 1 = full frame draw buffer if memory allows
 */
#ifndef SMARTDISPLAY_AVOID_TEAR
#define SMARTDISPLAY_AVOID_TEAR 1
#endif

#ifndef SMARTDISPLAY_LVGL_BUFFER_NUM
#define SMARTDISPLAY_LVGL_BUFFER_NUM 2
#endif

#ifndef SMARTDISPLAY_FULL_REFRESH
#define SMARTDISPLAY_FULL_REFRESH 1
#endif

#define ST7262_PARTIAL_BUFFER_ROWS 40
#define ST7262_VSYNC_WAIT_MS 35

static void *alloc_display_buffer(size_t size, bool prefer_internal)
{
    void *buf = NULL;
    if (prefer_internal) {
        buf = heap_caps_aligned_alloc(64, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (buf) return buf;
    }

    buf = heap_caps_aligned_alloc(64, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf) return buf;

    buf = heap_caps_aligned_alloc(64, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (buf) return buf;

    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static bool IRAM_ATTR on_vsync_event(esp_lcd_panel_handle_t panel,
                                     esp_lcd_rgb_panel_event_data_t *edata,
                                     void *user_ctx)
{
    (void)panel;
    (void)edata;
    (void)user_ctx;
    BaseType_t high_task_awoken = pdFALSE;
    if (vsync_semaphore != NULL) {
        xSemaphoreGiveFromISR(vsync_semaphore, &high_task_awoken);
    }
    return (high_task_awoken == pdTRUE);
}

static void direct_io_lv_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)display->user_data;
    const bool is_partial = (active_render_mode == LV_DISPLAY_RENDER_MODE_PARTIAL);
    const bool is_last = lv_display_flush_is_last(display);

    if (!is_partial && !is_last) {
        /* In FULL mode wait for the last LVGL flush and transfer exactly once. */
        lv_display_flush_ready(display);
        return;
    }

#if SMARTDISPLAY_AVOID_TEAR
    if (vsync_semaphore != NULL) {
        while (xSemaphoreTake(vsync_semaphore, 0) == pdTRUE) {}
        /* Align the transfer to the beginning of a frame to reduce visible tearing. */
        (void)xSemaphoreTake(vsync_semaphore, pdMS_TO_TICKS(ST7262_VSYNC_WAIT_MS));
    }
#endif

    if (is_partial) {
        esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    } else {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, px_map);
    }

    lv_display_flush_ready(display);
}

lv_display_t *lvgl_lcd_init()
{
    log_i("Init ST7262 RGB (full_refresh=%d, buffers=%d)", SMARTDISPLAY_FULL_REFRESH, SMARTDISPLAY_LVGL_BUFFER_NUM);
    log_i("ST7262 timing flags: pclk_active_neg=%d pclk_idle_high=%d",
          (int)ST7262_PANEL_CONFIG_TIMINGS_FLAGS_PCLK_ACTIVE_NEG,
          (int)ST7262_PANEL_CONFIG_TIMINGS_FLAGS_PCLK_IDLE_HIGH);

    lv_display_t *display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    vsync_semaphore = xSemaphoreCreateBinary();
    if (vsync_semaphore == NULL) {
        log_e("Failed to create VSYNC semaphore");
    }

    uint32_t buffer_rows = ST7262_PARTIAL_BUFFER_ROWS;
    void *drawBuffer1 = NULL;
    void *drawBuffer2 = NULL;
    uint32_t buffer_size = 0;

    const uint32_t full_buffer_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t);

#if SMARTDISPLAY_FULL_REFRESH
    drawBuffer1 = alloc_display_buffer(full_buffer_size, false);
    if (drawBuffer1 != NULL) {
        buffer_size = full_buffer_size;
        active_render_mode = LV_DISPLAY_RENDER_MODE_FULL;
    } else {
        log_w("Full frame buffer allocation failed, fallback to PARTIAL mode");
    }
#endif

    if (drawBuffer1 == NULL) {
        active_render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
        while (buffer_rows >= 20 && drawBuffer1 == NULL) {
            buffer_size = DISPLAY_WIDTH * buffer_rows * sizeof(lv_color_t);
            /* Prefer internal RAM in partial mode to reduce PSRAM/LCD bus contention. */
            drawBuffer1 = alloc_display_buffer(buffer_size, true);
            if (drawBuffer1 == NULL) {
                buffer_rows /= 2;
            }
        }
    }

    if (drawBuffer1 == NULL) {
        log_e("Display buffer allocation failed");
        return display;
    }

#if SMARTDISPLAY_LVGL_BUFFER_NUM >= 2
    drawBuffer2 = alloc_display_buffer(buffer_size, active_render_mode == LV_DISPLAY_RENDER_MODE_PARTIAL);
    if (drawBuffer2 == NULL) {
        log_w("Second LVGL buffer not available, using single buffer");
    }
#endif

    lv_display_set_buffers(display, drawBuffer1, drawBuffer2, buffer_size, active_render_mode);
    if (active_render_mode == LV_DISPLAY_RENDER_MODE_PARTIAL) {
        log_i("LVGL PARTIAL buffers: rows=%u size=%uKB second=%s", buffer_rows, buffer_size / 1024, drawBuffer2 ? "yes" : "no");
    } else {
        log_i("LVGL FULL buffers: size=%uKB second=%s", buffer_size / 1024, drawBuffer2 ? "yes" : "no");
    }

    const esp_lcd_rgb_panel_config_t rgb_panel_config = {
        .clk_src = ST7262_PANEL_CONFIG_CLK_SRC,
        .timings = {
            .pclk_hz = ST7262_PANEL_CONFIG_TIMINGS_PCLK_HZ,
            .h_res = ST7262_PANEL_CONFIG_TIMINGS_H_RES,
            .v_res = ST7262_PANEL_CONFIG_TIMINGS_V_RES,
            .hsync_pulse_width = ST7262_PANEL_CONFIG_TIMINGS_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = ST7262_PANEL_CONFIG_TIMINGS_HSYNC_BACK_PORCH,
            .hsync_front_porch = ST7262_PANEL_CONFIG_TIMINGS_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = ST7262_PANEL_CONFIG_TIMINGS_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = ST7262_PANEL_CONFIG_TIMINGS_VSYNC_BACK_PORCH,
            .vsync_front_porch = ST7262_PANEL_CONFIG_TIMINGS_VSYNC_FRONT_PORCH,
            .flags = {
                .hsync_idle_low = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_HSYNC_IDLE_LOW,
                .vsync_idle_low = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_VSYNC_IDLE_LOW,
                .de_idle_high = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_DE_IDLE_HIGH,
                .pclk_active_neg = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_PCLK_ACTIVE_NEG,
                .pclk_idle_high = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_PCLK_IDLE_HIGH,
            }
        },
        .data_width = ST7262_PANEL_CONFIG_DATA_WIDTH,
        .sram_trans_align = ST7262_PANEL_CONFIG_SRAM_TRANS_ALIGN,
        .psram_trans_align = ST7262_PANEL_CONFIG_PSRAM_TRANS_ALIGN,
        .hsync_gpio_num = ST7262_PANEL_CONFIG_HSYNC,
        .vsync_gpio_num = ST7262_PANEL_CONFIG_VSYNC,
        .de_gpio_num = ST7262_PANEL_CONFIG_DE,
        .pclk_gpio_num = ST7262_PANEL_CONFIG_PCLK,
        .data_gpio_nums = {
            ST7262_PANEL_CONFIG_DATA_R0, ST7262_PANEL_CONFIG_DATA_R1, ST7262_PANEL_CONFIG_DATA_R2, ST7262_PANEL_CONFIG_DATA_R3, ST7262_PANEL_CONFIG_DATA_R4,
            ST7262_PANEL_CONFIG_DATA_G0, ST7262_PANEL_CONFIG_DATA_G1, ST7262_PANEL_CONFIG_DATA_G2, ST7262_PANEL_CONFIG_DATA_G3, ST7262_PANEL_CONFIG_DATA_G4, ST7262_PANEL_CONFIG_DATA_G5,
            ST7262_PANEL_CONFIG_DATA_B0, ST7262_PANEL_CONFIG_DATA_B1, ST7262_PANEL_CONFIG_DATA_B2, ST7262_PANEL_CONFIG_DATA_B3, ST7262_PANEL_CONFIG_DATA_B4
        },
        .disp_gpio_num = ST7262_PANEL_CONFIG_DISP,
        .on_frame_trans_done = on_vsync_event,
        .user_ctx = display,
        .flags = {
            .disp_active_low = ST7262_PANEL_CONFIG_FLAGS_DISP_ACTIVE_LOW,
            .relax_on_idle = ST7262_PANEL_CONFIG_FLAGS_RELAX_ON_IDLE,
            .fb_in_psram = ST7262_PANEL_CONFIG_FLAGS_FB_IN_PSRAM,
        }
    };

    esp_lcd_panel_handle_t panel_handle;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    display->user_data = panel_handle;
    display->flush_cb = direct_io_lv_flush;

    const uint32_t h_total = ST7262_PANEL_CONFIG_TIMINGS_H_RES +
                             ST7262_PANEL_CONFIG_TIMINGS_HSYNC_PULSE_WIDTH +
                             ST7262_PANEL_CONFIG_TIMINGS_HSYNC_BACK_PORCH +
                             ST7262_PANEL_CONFIG_TIMINGS_HSYNC_FRONT_PORCH;
    const uint32_t v_total = ST7262_PANEL_CONFIG_TIMINGS_V_RES +
                             ST7262_PANEL_CONFIG_TIMINGS_VSYNC_PULSE_WIDTH +
                             ST7262_PANEL_CONFIG_TIMINGS_VSYNC_BACK_PORCH +
                             ST7262_PANEL_CONFIG_TIMINGS_VSYNC_FRONT_PORCH;
    if (h_total > 0 && v_total > 0) {
        const uint32_t fps = ST7262_PANEL_CONFIG_TIMINGS_PCLK_HZ / (h_total * v_total);
        log_i("RGB timing: pclk=%uHz fps~%u", (uint32_t)ST7262_PANEL_CONFIG_TIMINGS_PCLK_HZ, fps);
    }
    log_i("ST7262 display initialized");
    return display;
}

#endif
