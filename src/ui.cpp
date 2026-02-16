#include <lvgl.h>
#include "ui.h"
#include "network_manager.h" 
#include "config_manager.h"
#include <WiFi.h>
#include <time.h>
#include "config.h" 
#include <LittleFS.h>     
#include <ArduinoJson.h>  
#include <cstring>
#include <cstdio>

#include "thermostat.h"
extern Thermostat thermo;

// DICHIARAZIONE IMMAGINI
LV_IMG_DECLARE(logo_splash);
LV_IMG_DECLARE(qr_code); 
LV_IMG_DECLARE(wx_01);
LV_IMG_DECLARE(wx_02);
LV_IMG_DECLARE(wx_03);
LV_IMG_DECLARE(wx_04);
LV_IMG_DECLARE(wx_09);
LV_IMG_DECLARE(wx_10);
LV_IMG_DECLARE(wx_11);
LV_IMG_DECLARE(wx_13);
LV_IMG_DECLARE(wx_50);

// --- OGGETTI UI ---
lv_obj_t *scr_splash = NULL;
lv_obj_t *lbl_splash_status = NULL;

lv_obj_t *scr_main = NULL;
lv_obj_t *scr_program = NULL;
lv_obj_t *scr_setup = NULL;
lv_obj_t *scr_impegni = NULL;
static lv_obj_t *scr_ota = NULL;
static lv_obj_t *lbl_ota_progress = NULL;
static lv_obj_t *scr_before_ota = NULL;
static uint8_t ota_ui_last_pct = 255;

// Widget Home
lv_obj_t *ui_lbl_hour = NULL;
lv_obj_t *ui_lbl_min = NULL;
lv_obj_t *ui_lbl_dots = NULL;

lv_obj_t *ui_lbl_temp_val = NULL;
lv_obj_t *ui_lbl_hum_val = NULL;
lv_obj_t *ui_lbl_press_val = NULL; 

lv_obj_t *lbl_date = NULL;         
lv_obj_t *lbl_weather_today_val = NULL;
lv_obj_t *lbl_weather_tmrw_val = NULL;
lv_obj_t *img_weather_today = NULL;
lv_obj_t *img_weather_tmrw = NULL;

// Setup
lv_obj_t *lbl_setup_ssid = NULL;
lv_obj_t *lbl_setup_ip = NULL;
lv_obj_t *lbl_setup_gw = NULL;

// Boost
lv_obj_t *btn_boost = NULL;          
lv_obj_t *lbl_boost_status = NULL;   
static int boost_minutes_selection = 30;
static lv_obj_t *lbl_popup_minutes = NULL;

// Programmazione
lv_obj_t *tv_days = NULL;      
lv_obj_t *timelines[7]; 
lv_obj_t *lbl_intervals[7]; 
lv_obj_t *lbl_drag_info[7];
static lv_obj_t * checkboxes[7]; 
static int source_day_for_copy = 0; 
static lv_obj_t *interval_editor_win = NULL;
static lv_obj_t *interval_start_roller = NULL;
static lv_obj_t *interval_end_roller = NULL;
static lv_obj_t *interval_mode_on_btn = NULL;
static lv_obj_t *interval_mode_off_btn = NULL;
static bool interval_mode_set_on = true;
static char interval_time_options[320];
static bool interval_time_options_ready = false;

// Impegni
lv_obj_t *list_impegni = NULL; 

#define TIMELINE_PAD_X 25 
static int prev_drag_slot_idx = -1;   
static int drag_day_idx = -1;
static bool drag_changed = false;

// ============================================================================
//  PROTOTIPI LOCALI (FONDAMENTALI PER EVITARE ERRORI)
// ============================================================================
void load_impegni_to_ui();
void refresh_intervals_display(int ui_idx);
void get_time_string_from_slot(int slot, char* buf);
void create_home_button(lv_obj_t *parent);
void update_main_info_label(bool force);
void create_boost_popup();
void show_relay_error_popup(); // Dichiarata anche in header, ma utile qui
void create_interval_editor_popup(int ui_idx);

// ============================================================================
//  FUNZIONI UTILITY & CALLBACKS
// ============================================================================

static void set_label_text_if_changed(lv_obj_t *label, const char *text) {
    if (!label || !text) return;
    const char *current = lv_label_get_text(label);
    if (current && strcmp(current, text) == 0) return;
    lv_label_set_text(label, text);
}

static const lv_image_dsc_t* map_weather_icon(const String &iconCode) {
    if (iconCode.length() < 2) return &wx_03;
    const char c0 = iconCode[0];
    const char c1 = iconCode[1];

    if (c0 == '0' && c1 == '1') return &wx_01;
    if (c0 == '0' && c1 == '2') return &wx_02;
    if (c0 == '0' && c1 == '3') return &wx_03;
    if (c0 == '0' && c1 == '4') return &wx_04;
    if (c0 == '0' && c1 == '9') return &wx_09;
    if (c0 == '1' && c1 == '0') return &wx_10;
    if (c0 == '1' && c1 == '1') return &wx_11;
    if (c0 == '1' && c1 == '3') return &wx_13;
    if (c0 == '5' && c1 == '0') return &wx_50;
    return &wx_03;
}

static void nav_event_cb(lv_event_t * e) {
    lv_obj_t * target_screen = (lv_obj_t *)lv_event_get_user_data(e);
    if(target_screen) {
        if(target_screen == scr_impegni) {
            load_impegni_to_ui();
        }
        if(target_screen == scr_program && tv_days) {
            for (int i = 0; i < 7; i++) {
                refresh_intervals_display(i);
                if (timelines[i]) lv_obj_invalidate(timelines[i]);
            }
        }
        lv_screen_load(target_screen);
    }
}

void create_home_button(lv_obj_t *parent) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 60, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_HOME); 
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, nav_event_cb, LV_EVENT_CLICKED, scr_main);
}

static void error_popup_close_cb(lv_event_t * e) {
    lv_obj_t * win = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_delete(win);
}

void show_relay_error_popup() {
    lv_obj_t * win = lv_obj_create(lv_scr_act());
    lv_obj_set_size(win, 450, 280);
    lv_obj_center(win);
    lv_obj_set_style_bg_color(win, lv_color_hex(0x440000), 0); 
    lv_obj_set_style_border_color(win, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(win, 3, 0);
    lv_obj_set_flex_flow(win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(win, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * icon = lv_label_create(win);
    lv_label_set_text(icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFD700), 0);

    lv_obj_t * lbl = lv_label_create(win);
    lv_label_set_text(lbl, "ERRORE COMUNICAZIONE\n\nRelè non raggiungibile.\nControllare alimentazione\no riavviarlo manualmente.");
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(lbl, 400);

    lv_obj_t * btn = lv_button_create(win);
    lv_obj_set_size(btn, 150, 50);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn, error_popup_close_cb, LV_EVENT_CLICKED, win);
    
    lv_obj_t * l_btn = lv_label_create(btn);
    lv_label_set_text(l_btn, "ESCI");
    lv_obj_set_style_text_color(l_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l_btn);
}

void get_time_string_from_slot(int slot, char* buf) {
    if(slot > 48) slot = 48;
    int h = slot / 2;
    int m = (slot % 2) * 30;
    sprintf(buf, "%02d:%02d", h, m);
}

// ============================================================================
//  SPLASH SCREEN
// ============================================================================
void ui_show_splash() {
    if(scr_splash) return;
    scr_splash = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_splash, lv_color_hex(0x000000), 0); 

    lv_obj_t * img_logo = lv_image_create(scr_splash);
    lv_image_set_src(img_logo, &logo_splash);
    lv_obj_align(img_logo, LV_ALIGN_LEFT_MID, 100, -40); 

    lv_obj_t * lbl_title = lv_label_create(scr_splash);
    lv_label_set_text(lbl_title, "Cronotermostato\nSmart V1.0");
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0); 
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(lbl_title, img_logo, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t * img_qr = lv_image_create(scr_splash);
    lv_image_set_src(img_qr, &qr_code);
    lv_image_set_scale(img_qr, 128); 
    lv_obj_align(img_qr, LV_ALIGN_RIGHT_MID, -100, -90);
    lv_obj_set_style_border_color(img_qr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(img_qr, 4, 0);
    lv_obj_set_style_radius(img_qr, 8, 0);

    lv_obj_t * lbl_instr = lv_label_create(scr_splash);
    if (strlen(WIFI_SETUP_AP_PASSWORD) >= 8) {
        lv_label_set_text_fmt(
            lbl_instr,
            "1. Connettiti a: %s\n   Password: %s\n\n2. Vai su http://192.168.4.1",
            WIFI_SETUP_AP_SSID,
            WIFI_SETUP_AP_PASSWORD
        );
    } else {
        lv_label_set_text_fmt(
            lbl_instr,
            "1. Connettiti a: %s\n   Rete aperta (senza password)\n\n2. Vai su http://192.168.4.1",
            WIFI_SETUP_AP_SSID
        );
    }
    lv_obj_set_style_text_align(lbl_instr, LV_TEXT_ALIGN_LEFT, 0); 
    lv_obj_set_style_text_color(lbl_instr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(lbl_instr, img_qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);

    lbl_splash_status = lv_label_create(scr_splash);
    lv_label_set_text(lbl_splash_status, "Avvio...");
    lv_obj_set_style_text_color(lbl_splash_status, lv_color_hex(0xE67E22), 0);
    lv_obj_align(lbl_splash_status, LV_ALIGN_BOTTOM_MID, 0, -40);

    lv_screen_load(scr_splash); 
}

void ui_splash_config_mode() {
    if (lbl_splash_status) {
        lv_label_set_text(lbl_splash_status, "Modalita' Access Point Attiva");
        lv_timer_handler(); 
    }
}

void ui_show_ota_screen(uint8_t progress_pct) {
    if (!scr_ota) {
        scr_ota = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(scr_ota, lv_color_hex(0x050505), 0);
        lv_obj_set_style_bg_grad_color(scr_ota, lv_color_hex(0x001030), 0);
        lv_obj_set_style_bg_grad_dir(scr_ota, LV_GRAD_DIR_VER, 0);

        lv_obj_t *lbl_title = lv_label_create(scr_ota);
        lv_label_set_text(lbl_title, "Aggiornamento OTA");
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, -40);

        lbl_ota_progress = lv_label_create(scr_ota);
        lv_label_set_text(lbl_ota_progress, "In corso... 0%");
        lv_obj_set_style_text_font(lbl_ota_progress, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_ota_progress, lv_color_hex(0xFFD700), 0);
        lv_obj_align(lbl_ota_progress, LV_ALIGN_CENTER, 0, 24);
    }

    if (lbl_ota_progress && progress_pct != ota_ui_last_pct) {
        lv_label_set_text_fmt(lbl_ota_progress, "In corso... %u%%", (unsigned)progress_pct);
        ota_ui_last_pct = progress_pct;
    }

    if (lv_scr_act() != scr_ota) {
        scr_before_ota = lv_scr_act();
        lv_screen_load(scr_ota);
    }
}

void ui_hide_ota_screen() {
    if (!scr_ota || lv_scr_act() != scr_ota) return;
    ota_ui_last_pct = 255;
    if (scr_before_ota) lv_screen_load(scr_before_ota);
    else if (scr_main) lv_screen_load(scr_main);
}

// ============================================================================
//  MAIN SCREEN (Home)
// ============================================================================
static void boost_plus_cb(lv_event_t * e) {
    boost_minutes_selection += 30;
    if(boost_minutes_selection > 480) boost_minutes_selection = 480;
    lv_label_set_text_fmt(lbl_popup_minutes, "%d min", boost_minutes_selection);
}

static void boost_minus_cb(lv_event_t * e) {
    boost_minutes_selection -= 30;
    if(boost_minutes_selection < 30) boost_minutes_selection = 30;
    lv_label_set_text_fmt(lbl_popup_minutes, "%d min", boost_minutes_selection);
}

static void boost_start_cb(lv_event_t * e) {
    lv_obj_t * win = (lv_obj_t *)lv_event_get_user_data(e);
    bool ok = thermo.startBoost(boost_minutes_selection);
    lv_obj_delete(win);
    update_ui(); 
    if (!ok) show_relay_error_popup();
}

static void boost_cancel_cb(lv_event_t * e) {
    lv_obj_t * win = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_delete(win);
}

void create_boost_popup() {
    boost_minutes_selection = 30; 
    lv_obj_t * win = lv_obj_create(lv_scr_act());
    const int popup_w = 420;
    const int popup_h = 300;
    const int menu_col_w = 150;
    const int screen_w = lv_obj_get_width(lv_scr_act());
    const int screen_h = lv_obj_get_height(lv_scr_act());
    const int left_area_w = screen_w - menu_col_w;
    int popup_x = (left_area_w - popup_w) / 2;
    if (popup_x < 10) popup_x = 10;
    int popup_y = (screen_h - popup_h) / 2;
    if (popup_y < 10) popup_y = 10;

    lv_obj_set_size(win, popup_w, popup_h);
    lv_obj_set_pos(win, popup_x, popup_y);
    lv_obj_remove_flag(win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(win, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(win, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(win, lv_color_hex(0xF1C40F), 0);
    lv_obj_set_style_border_width(win, 2, 0);
    lv_obj_set_flex_flow(win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(win, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(win, 14, 0);
    lv_obj_set_style_pad_all(win, 14, 0);

    lv_obj_t * title = lv_label_create(win);
    lv_label_set_text(title, "Accensione Temporizzata");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t * cont_sel = lv_obj_create(win);
    lv_obj_set_size(cont_sel, 300, 100);
    lv_obj_remove_flag(cont_sel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(cont_sel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(cont_sel, 0, 0);
    lv_obj_set_style_border_width(cont_sel, 0, 0);
    lv_obj_set_flex_flow(cont_sel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_sel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_m = lv_button_create(cont_sel);
    lv_obj_set_size(btn_m, 60, 60);
    lv_obj_add_event_cb(btn_m, boost_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_m = lv_label_create(btn_m);
    lv_label_set_text(lbl_m, "-");
    lv_obj_set_style_text_font(lbl_m, &lv_font_montserrat_36, 0);
    lv_obj_center(lbl_m);

    lbl_popup_minutes = lv_label_create(cont_sel);
    lv_obj_set_style_text_font(lbl_popup_minutes, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lbl_popup_minutes, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_popup_minutes, "30 min");

    lv_obj_t * btn_p = lv_button_create(cont_sel);
    lv_obj_set_size(btn_p, 60, 60);
    lv_obj_add_event_cb(btn_p, boost_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_p = lv_label_create(btn_p);
    lv_label_set_text(lbl_p, "+");
    lv_obj_set_style_text_font(lbl_p, &lv_font_montserrat_36, 0);
    lv_obj_center(lbl_p);

    lv_obj_t * cont_act = lv_obj_create(win);
    lv_obj_set_size(cont_act, 350, 70);
    lv_obj_remove_flag(cont_act, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(cont_act, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(cont_act, 0, 0);
    lv_obj_set_style_border_width(cont_act, 0, 0);
    lv_obj_set_flex_flow(cont_act, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_act, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_ok = lv_button_create(cont_act);
    lv_obj_set_size(btn_ok, 140, 50);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0xE67E22), 0); 
    lv_obj_add_event_cb(btn_ok, boost_start_cb, LV_EVENT_CLICKED, win);
    lv_obj_t * lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "AVVIA");
    lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_ok);

    lv_obj_t * btn_no = lv_button_create(cont_act);
    lv_obj_set_size(btn_no, 140, 50);
    lv_obj_set_style_bg_color(btn_no, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_no, boost_cancel_cb, LV_EVENT_CLICKED, win);
    lv_obj_t * lbl_no = lv_label_create(btn_no);
    lv_label_set_text(lbl_no, "ANNULLA");
    lv_obj_set_style_text_font(lbl_no, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_no);
}

static void btn_boost_click_cb(lv_event_t * e) {
    if (!thermo.verifyRelayAvailability(true)) {
        show_relay_error_popup();
        return;
    }
    if(thermo.isHeatingState()) {
        thermo.toggleOverride();
    } else {
        create_boost_popup(); 
    }
}

void build_scr_main() {
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(0x050505), 0);
    lv_obj_set_style_bg_grad_color(scr_main, lv_color_hex(0x001030), 0); 
    lv_obj_set_style_bg_grad_dir(scr_main, LV_GRAD_DIR_VER, 0);
    
    // --- COLONNA SINISTRA (DATI) ---
    lv_obj_t *col_left = lv_obj_create(scr_main); 
    lv_obj_set_size(col_left, 650, 480);
    lv_obj_set_style_bg_opa(col_left, 0, 0); 
    lv_obj_set_style_border_width(col_left, 0, 0);
    lv_obj_align(col_left, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_remove_flag(col_left, LV_OBJ_FLAG_SCROLLABLE);

    // RIGA 1: Giorno + data + ora
    ui_lbl_hour = lv_label_create(col_left);
    lv_obj_set_style_text_font(ui_lbl_hour, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(ui_lbl_hour, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(ui_lbl_hour, 620);
    lv_label_set_long_mode(ui_lbl_hour, LV_LABEL_LONG_CLIP);
    lv_obj_align(ui_lbl_hour, LV_ALIGN_TOP_LEFT, 20, 16);
    lv_label_set_text(ui_lbl_hour, "--- --/--- --:--");

    // RIGA 2: Clima interno compatto
    ui_lbl_temp_val = lv_label_create(col_left);
    lv_obj_set_style_text_font(ui_lbl_temp_val, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ui_lbl_temp_val, lv_color_hex(0xDCEBFF), 0);
    lv_obj_set_width(ui_lbl_temp_val, 620);
    lv_label_set_long_mode(ui_lbl_temp_val, LV_LABEL_LONG_CLIP);
    lv_obj_align(ui_lbl_temp_val, LV_ALIGN_TOP_LEFT, 20, 68);
    lv_label_set_text(ui_lbl_temp_val, "Clima Interno: T --.-C H --% P ---- hPa");

    // RIGA 3: Accensioni oggi
    lbl_date = lv_label_create(col_left);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(lbl_date, 620);
    lv_obj_set_height(lbl_date, 52);
    lv_obj_set_style_text_line_space(lbl_date, 2, 0);
    lv_label_set_long_mode(lbl_date, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_LEFT, 20, 106);
    lv_label_set_text(lbl_date, "Accensioni per oggi: nessuna");

    // Non usati nel nuovo layout home (mantenuti per compatibilita')
    ui_lbl_min = NULL;
    ui_lbl_dots = NULL;
    ui_lbl_hum_val = NULL;
    ui_lbl_press_val = NULL;

    // Sezione Meteo
    lv_obj_t *cont_weather_section = lv_obj_create(col_left); 
    lv_obj_set_size(cont_weather_section, 620, 150);
    lv_obj_set_style_bg_opa(cont_weather_section, 0, 0); 
    lv_obj_set_style_border_width(cont_weather_section, 0, 0);
    lv_obj_align(cont_weather_section, LV_ALIGN_TOP_LEFT, 20, 152);
    lv_obj_set_flex_flow(cont_weather_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont_weather_section, 10, 0); 
    lv_obj_remove_flag(cont_weather_section, LV_OBJ_FLAG_SCROLLABLE);

    // Oggi
    lv_obj_t *row_today = lv_obj_create(cont_weather_section);
    lv_obj_set_size(row_today, 620, 54);
    lv_obj_set_style_bg_opa(row_today, 0, 0); 
    lv_obj_set_style_border_width(row_today, 0, 0);
    lv_obj_set_flex_flow(row_today, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_today, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_today, 10, 0);
    lv_obj_remove_flag(row_today, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *lbl_today = lv_label_create(row_today);
    lv_obj_set_style_text_font(lbl_today, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_today, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(lbl_today, 90);
    lv_label_set_text(lbl_today, "Oggi:");

    img_weather_today = lv_image_create(row_today);
    lv_image_set_src(img_weather_today, &wx_03);

    lbl_weather_today_val = lv_label_create(row_today);
    lv_obj_set_style_text_font(lbl_weather_today_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_weather_today_val, lv_color_hex(0xFFD700), 0);
    lv_obj_set_width(lbl_weather_today_val, 480);
    lv_label_set_long_mode(lbl_weather_today_val, LV_LABEL_LONG_CLIP);
    lv_label_set_text(lbl_weather_today_val, "--°C");

    // Domani
    lv_obj_t *row_tmrw = lv_obj_create(cont_weather_section);
    lv_obj_set_size(row_tmrw, 620, 54);
    lv_obj_set_style_bg_opa(row_tmrw, 0, 0); 
    lv_obj_set_style_border_width(row_tmrw, 0, 0);
    lv_obj_set_flex_flow(row_tmrw, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_tmrw, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_tmrw, 10, 0);
    lv_obj_remove_flag(row_tmrw, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_tmrw = lv_label_create(row_tmrw);
    lv_obj_set_style_text_font(lbl_tmrw, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_tmrw, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(lbl_tmrw, 90); 
    lv_label_set_text(lbl_tmrw, "Domani:");

    img_weather_tmrw = lv_image_create(row_tmrw);
    lv_image_set_src(img_weather_tmrw, &wx_03);

    lbl_weather_tmrw_val = lv_label_create(row_tmrw);
    lv_obj_set_style_text_font(lbl_weather_tmrw_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_weather_tmrw_val, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_width(lbl_weather_tmrw_val, 480);
    lv_label_set_long_mode(lbl_weather_tmrw_val, LV_LABEL_LONG_CLIP);
    lv_label_set_text(lbl_weather_tmrw_val, "--°C");

    // Tasto Boost
    lv_obj_t *bot_section = lv_obj_create(col_left); 
    lv_obj_set_size(bot_section, 630, 100); 
    lv_obj_set_style_bg_opa(bot_section, 0, 0); 
    lv_obj_set_style_border_width(bot_section, 0, 0);
    lv_obj_align(bot_section, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_remove_flag(bot_section, LV_OBJ_FLAG_SCROLLABLE);
    
    btn_boost = lv_button_create(bot_section);
    lv_obj_set_size(btn_boost, 250, 70);
    lv_obj_center(btn_boost);
    lv_obj_add_event_cb(btn_boost, btn_boost_click_cb, LV_EVENT_CLICKED, NULL); 
    lv_obj_set_style_bg_color(btn_boost, lv_color_hex(0x3498DB), 0); 
    
    lbl_boost_status = lv_label_create(btn_boost);
    lv_obj_set_style_text_font(lbl_boost_status, &lv_font_montserrat_24, 0);
    lv_obj_set_width(lbl_boost_status, 230);
    lv_obj_set_style_text_align(lbl_boost_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl_boost_status, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl_boost_status, "Brr che freddo!!!"); 
    lv_obj_center(lbl_boost_status);

    // COLONNA DESTRA (MENU)
    lv_obj_t *col_right = lv_obj_create(scr_main); lv_obj_set_size(col_right, 150, 480);
    lv_obj_set_style_bg_color(col_right, lv_color_hex(0x151515), 0);
    lv_obj_align(col_right, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_flex_flow(col_right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col_right, 30, 0);

    auto create_menu_btn = [&](const char* text, const char* symbol, lv_event_cb_t cb, lv_obj_t* user_data, uint32_t color) {
        lv_obj_t *btn = lv_button_create(col_right);
        lv_obj_set_size(btn, 120, 100);
        lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *lbl_sym = lv_label_create(btn); lv_label_set_text(lbl_sym, symbol); lv_obj_set_style_text_font(lbl_sym, &lv_font_montserrat_24, 0);
        lv_obj_t *lbl_txt = lv_label_create(btn); lv_label_set_text(lbl_txt, text);
    };

    create_menu_btn("PROGRAM", LV_SYMBOL_LIST, nav_event_cb, scr_program, 0xE67E22);
    create_menu_btn("SETUP", LV_SYMBOL_SETTINGS, nav_event_cb, scr_setup, 0x7F8C8D);
    create_menu_btn("IMPEGNI", LV_SYMBOL_LIST, nav_event_cb, scr_impegni, 0x27AE60);
    
    update_main_info_label(true);
}

// ============================================================================
//  ALTRE SCHERMATE (Program, Setup, Impegni)
// ============================================================================

void load_impegni_to_ui() {
    lv_obj_clean(list_impegni);
    if (!LittleFS.exists("/impegni.json")) {
        lv_obj_t * lbl = lv_label_create(list_impegni); lv_label_set_text(lbl, "Nessun impegno salvato.");
        return;
    }
    File file = LittleFS.open("/impegni.json", "r");
    if (!file) return;
    DynamicJsonDocument doc(8192); DeserializationError error = deserializeJson(doc, file); file.close();
    if (error) { lv_obj_t * lbl = lv_label_create(list_impegni); lv_label_set_text(lbl, "Errore dati."); return; }
    
    JsonArray arr = doc.as<JsonArray>();
    for (JsonVariant v : arr) {
        String imp = v.as<String>();
        lv_obj_t * btn = lv_list_add_btn(list_impegni, LV_SYMBOL_BULLET, imp.c_str());
    }
}

void refresh_intervals_display(int ui_idx) {
    if(ui_idx < 0 || ui_idx >= 7) return;
    if(!lbl_intervals[ui_idx]) return;
    int config_idx = ui_idx;
    uint64_t slots = configManager.data.weekSchedule[config_idx].timeSlots;
    String text = "";
    int count = 0; int i = 0;
    while(i < 48 && count < 3) {
        if ((slots >> i) & 1ULL) {
            int start = i;
            while(i < 48 && ((slots >> i) & 1ULL)) i++;
            int end = i; 
            char bufStart[6], bufEnd[6];
            get_time_string_from_slot(start, bufStart);
            get_time_string_from_slot(end, bufEnd);
            count++;
            text += "Orario " + String(count) + ": " + String(bufStart) + " - " + String(bufEnd) + "\n";
        } else i++;
    }
    if (text == "") text = "Nessuna fascia oraria impostata";
    lv_label_set_text(lbl_intervals[ui_idx], text.c_str());
}

static void timeline_draw_event_cb(lv_event_t * e) {
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);
    int day_idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_area_t obj_coords; lv_obj_get_coords(obj, &obj_coords);
    int w = lv_obj_get_width(obj); int h = lv_obj_get_height(obj);
    int x_start_draw = obj_coords.x1 + TIMELINE_PAD_X;
    int w_draw = w - (2 * TIMELINE_PAD_X);
    int y_start = obj_coords.y1;
    uint64_t slots = configManager.data.weekSchedule[day_idx].timeSlots;
    float slot_w = (float)w_draw / 48.0;
    int bar_h = 57; /* +50% thickness for easier touch interaction */
    int bar_y = y_start + 18;

    lv_draw_rect_dsc_t dsc_bg; lv_draw_rect_dsc_init(&dsc_bg);
    dsc_bg.bg_color = lv_color_hex(0x333333); dsc_bg.bg_opa = LV_OPA_COVER; dsc_bg.radius = 4; 
    lv_area_t bg_area; bg_area.x1 = x_start_draw; bg_area.x2 = x_start_draw + w_draw - 1; bg_area.y1 = bar_y; bg_area.y2 = bar_y + bar_h;
    lv_draw_rect(layer, &dsc_bg, &bg_area);
    
    lv_draw_rect_dsc_t dsc_on; lv_draw_rect_dsc_init(&dsc_on);
    dsc_on.bg_color = lv_color_hex(0x27AE60); dsc_on.bg_opa = LV_OPA_COVER; dsc_on.radius = 4;
    int i = 0;
    while(i < 48) {
        if ((slots >> i) & 1ULL) {
            int start_i = i; while(i < 48 && ((slots >> i) & 1ULL)) i++; int end_i = i; 
            int x1 = x_start_draw + (int)(start_i * slot_w); int x2 = x_start_draw + (int)(end_i * slot_w) - 1;
            lv_area_t on_area; on_area.x1 = x1; on_area.x2 = x2; on_area.y1 = bar_y; on_area.y2 = bar_y + bar_h;
            lv_draw_rect(layer, &dsc_on, &on_area);
        } else i++;
    }

    lv_draw_line_dsc_t dsc_tick;
    lv_draw_line_dsc_init(&dsc_tick);
    dsc_tick.color = lv_color_hex(0xA0A0A0);
    dsc_tick.opa = LV_OPA_70;
    dsc_tick.width = 1;

    lv_draw_label_dsc_t dsc_hour;
    lv_draw_label_dsc_init(&dsc_hour);
    dsc_hour.color = lv_color_hex(0xBEBEBE);
    dsc_hour.font = &lv_font_montserrat_14;
    dsc_hour.align = LV_TEXT_ALIGN_CENTER;

    for (int slot = 0; slot <= 48; slot += 6) { // ogni 3 ore
        int x = x_start_draw + (int)(slot * slot_w);
        if (slot == 48) x = x_start_draw + w_draw;

        dsc_tick.p1.x = x;
        dsc_tick.p1.y = bar_y + bar_h + 2;
        dsc_tick.p2.x = x;
        dsc_tick.p2.y = bar_y + bar_h + 8;
        lv_draw_line(layer, &dsc_tick);

        char hour_txt[4];
        snprintf(hour_txt, sizeof(hour_txt), "%02d", slot / 2);
        dsc_hour.text = hour_txt;
        lv_area_t txt_area;
        txt_area.x1 = x - 12;
        txt_area.x2 = x + 12;
        txt_area.y1 = bar_y + bar_h + 10;
        txt_area.y2 = txt_area.y1 + 14;
        lv_draw_label(layer, &dsc_hour, &txt_area);
    }
}

static int get_timeline_slot_idx(lv_obj_t *obj) {
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return -1;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    int w = lv_obj_get_width(obj);
    int w_draw = w - (2 * TIMELINE_PAD_X);
    if (w_draw <= 0) return -1;

    int rel_x = p.x - coords.x1 - TIMELINE_PAD_X;
    if (rel_x < 0) rel_x = 0;
    if (rel_x >= w_draw) rel_x = w_draw - 1;

    int slot_idx = (rel_x * 48) / w_draw;
    if (slot_idx < 0) slot_idx = 0;
    if (slot_idx > 47) slot_idx = 47;
    return slot_idx;
}

static void toggle_slot(uint64_t *slots, int slot_idx) {
    if (!slots || slot_idx < 0 || slot_idx > 47) return;
    *slots ^= (1ULL << slot_idx);
}

static void invalidate_timeline_bar_range(lv_obj_t *obj, int slot_a, int slot_b) {
    if (!obj) return;
    if (slot_a > slot_b) {
        int t = slot_a;
        slot_a = slot_b;
        slot_b = t;
    }

    if (slot_a < 0) slot_a = 0;
    if (slot_b > 47) slot_b = 47;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int w = lv_obj_get_width(obj);
    int w_draw = w - (2 * TIMELINE_PAD_X);
    if (w_draw <= 0) return;

    float slot_w = (float)w_draw / 48.0f;
    int x_start_draw = coords.x1 + TIMELINE_PAD_X;
    int bar_y = coords.y1 + 18;
    int bar_h = 57;

    lv_area_t inv;
    inv.x1 = x_start_draw + (int)(slot_a * slot_w) - 2;
    inv.x2 = x_start_draw + (int)((slot_b + 1) * slot_w) + 2;
    inv.y1 = bar_y - 2;
    inv.y2 = bar_y + bar_h + 2;

    if (inv.x1 < coords.x1) inv.x1 = coords.x1;
    if (inv.x2 > coords.x2) inv.x2 = coords.x2;
    if (inv.y1 < coords.y1) inv.y1 = coords.y1;
    if (inv.y2 > coords.y2) inv.y2 = coords.y2;

    lv_obj_invalidate_area(obj, &inv);
}

static void timeline_input_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
        code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
        return;
    }

    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    int ui_idx = (int)(intptr_t)lv_event_get_user_data(e);
    int config_idx = ui_idx;
    uint64_t *slots = &configManager.data.weekSchedule[config_idx].timeSlots;

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        prev_drag_slot_idx = -1;
        drag_day_idx = -1;
        set_label_text_if_changed(lbl_drag_info[ui_idx], "Trascina per selezionare orari");
        if (drag_changed) {
            refresh_intervals_display(ui_idx);
            configManager.saveConfig();
            update_main_info_label(true);
            drag_changed = false;
        }
        return;
    }

    int current_slot_idx = get_timeline_slot_idx(obj);
    if (current_slot_idx < 0) return;

    if (code == LV_EVENT_PRESSED) {
        prev_drag_slot_idx = current_slot_idx;
        drag_day_idx = config_idx;
        toggle_slot(slots, current_slot_idx);
        drag_changed = true;
        set_label_text_if_changed(lbl_drag_info[ui_idx], "Modifica fascia in corso...");
        invalidate_timeline_bar_range(obj, current_slot_idx - 1, current_slot_idx + 1);
        return;
    }

    if (code == LV_EVENT_PRESSING && drag_day_idx == config_idx && prev_drag_slot_idx != current_slot_idx) {
        int step = (current_slot_idx > prev_drag_slot_idx) ? 1 : -1;
        int first_toggled = -1;
        int last_toggled = -1;
        for (int i = prev_drag_slot_idx + step; ; i += step) {
            toggle_slot(slots, i);
            if (first_toggled < 0) first_toggled = i;
            last_toggled = i;
            if (i == current_slot_idx) break;
        }
        drag_changed = true;
        prev_drag_slot_idx = current_slot_idx;
        if (first_toggled >= 0) {
            invalidate_timeline_bar_range(obj, first_toggled - 1, last_toggled + 1);
        }
    }
}

static void clear_prog_cb(lv_event_t * e) {
    int ui_idx = (int)(intptr_t)lv_event_get_user_data(e);
    int config_idx = ui_idx;
    configManager.data.weekSchedule[config_idx].timeSlots = 0;
    if(timelines[ui_idx]) lv_obj_invalidate(timelines[ui_idx]);
    refresh_intervals_display(ui_idx);
    configManager.saveConfig();
}

static void copy_confirm_cb(lv_event_t * e) {
    lv_obj_t * win = (lv_obj_t *)lv_event_get_user_data(e);
    uint64_t pattern = configManager.data.weekSchedule[source_day_for_copy].timeSlots;
    for(int i=0; i<7; i++) {
        if(lv_obj_has_state(checkboxes[i], LV_STATE_CHECKED)) {
            int config_idx = i;
            configManager.data.weekSchedule[config_idx].timeSlots = pattern;
            if(timelines[i]) lv_obj_invalidate(timelines[i]);
            refresh_intervals_display(i);
        }
    }
    update_main_info_label(true);
    configManager.saveConfig();
    lv_obj_delete(win);
}

static void copy_cancel_cb(lv_event_t * e) { lv_obj_t * win = (lv_obj_t *)lv_event_get_user_data(e); lv_obj_delete(win); }

void create_copy_popup(int uiDayIndex) {
    int configDayIndex = uiDayIndex;
    source_day_for_copy = configDayIndex;
    lv_obj_t * win = lv_obj_create(lv_scr_act()); lv_obj_set_size(win, 500, 350); lv_obj_center(win);
    lv_obj_set_style_bg_color(win, lv_color_hex(0x202020), 0);
    lv_obj_set_flex_flow(win, LV_FLEX_FLOW_COLUMN);
    
    lv_obj_t * title = lv_label_create(win); lv_label_set_text(title, "Copia su altri giorni:");
    lv_obj_t * cont_checks = lv_obj_create(win); lv_obj_set_size(cont_checks, 450, 180); lv_obj_set_flex_flow(cont_checks, LV_FLEX_FLOW_ROW_WRAP);
    const char* names[] = {"Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"};
    for(int i=0; i<7; i++) {
        checkboxes[i] = lv_checkbox_create(cont_checks); lv_checkbox_set_text(checkboxes[i], names[i]);
    }
    lv_obj_t * btn_ok = lv_button_create(win); lv_obj_add_event_cb(btn_ok, copy_confirm_cb, LV_EVENT_CLICKED, win);
    lv_label_set_text(lv_label_create(btn_ok), "CONFERMA");
}

static void open_copy_popup_cb(lv_event_t * e) { int ui_idx = (int)(intptr_t)lv_event_get_user_data(e); create_copy_popup(ui_idx); }

static void build_interval_time_options_once() {
    if (interval_time_options_ready) return;
    size_t used = 0;
    for (int slot = 0; slot <= 48; slot++) {
        int h = slot / 2;
        int m = (slot % 2) * 30;
        int n = snprintf(
            interval_time_options + used,
            sizeof(interval_time_options) - used,
            (slot < 48) ? "%02d:%02d\n" : "%02d:%02d",
            h, m
        );
        if (n <= 0) break;
        used += (size_t)n;
        if (used >= sizeof(interval_time_options)) break;
    }
    interval_time_options[sizeof(interval_time_options) - 1] = '\0';
    interval_time_options_ready = true;
}

static void interval_mode_refresh_btns() {
    if (!interval_mode_on_btn || !interval_mode_off_btn) return;
    lv_obj_set_style_bg_color(interval_mode_on_btn, lv_color_hex(interval_mode_set_on ? 0x27AE60 : 0x555555), 0);
    lv_obj_set_style_bg_color(interval_mode_off_btn, lv_color_hex(interval_mode_set_on ? 0x555555 : 0xC0392B), 0);
}

static void interval_mode_on_cb(lv_event_t * e) {
    (void)e;
    interval_mode_set_on = true;
    interval_mode_refresh_btns();
}

static void interval_mode_off_cb(lv_event_t * e) {
    (void)e;
    interval_mode_set_on = false;
    interval_mode_refresh_btns();
}

static void interval_editor_deleted_cb(lv_event_t * e) {
    (void)e;
    interval_editor_win = NULL;
    interval_start_roller = NULL;
    interval_end_roller = NULL;
    interval_mode_on_btn = NULL;
    interval_mode_off_btn = NULL;
}

static void interval_editor_close_cb(lv_event_t * e) {
    (void)e;
    if (interval_editor_win) lv_obj_delete(interval_editor_win);
}

static void interval_editor_apply_cb(lv_event_t * e) {
    int ui_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (ui_idx < 0 || ui_idx > 6) return;
    if (!interval_start_roller || !interval_end_roller) return;

    int start_slot = lv_roller_get_selected(interval_start_roller);
    int end_slot = lv_roller_get_selected(interval_end_roller);

    if (start_slot < 0) start_slot = 0;
    if (start_slot > 47) start_slot = 47;
    if (end_slot < 1) end_slot = 1;
    if (end_slot > 48) end_slot = 48;
    if (end_slot <= start_slot) {
        end_slot = start_slot + 1;
        if (end_slot > 48) end_slot = 48;
        lv_roller_set_selected(interval_end_roller, end_slot, LV_ANIM_OFF);
    }

    uint64_t *slots = &configManager.data.weekSchedule[ui_idx].timeSlots;
    for (int i = start_slot; i < end_slot; i++) {
        if (interval_mode_set_on) *slots |= (1ULL << i);
        else *slots &= ~(1ULL << i);
    }

    if (timelines[ui_idx]) {
        invalidate_timeline_bar_range(timelines[ui_idx], start_slot - 1, end_slot + 1);
    }
    refresh_intervals_display(ui_idx);
    configManager.saveConfig();
    update_main_info_label(true);

    set_label_text_if_changed(
        lbl_drag_info[ui_idx],
        interval_mode_set_on ? "Intervallo ON applicato" : "Intervallo OFF applicato"
    );
}

void create_interval_editor_popup(int ui_idx) {
    if (ui_idx < 0 || ui_idx > 6) return;
    if (interval_editor_win) lv_obj_delete(interval_editor_win);

    build_interval_time_options_once();
    interval_mode_set_on = true;

    interval_editor_win = lv_obj_create(lv_scr_act());
    lv_obj_set_size(interval_editor_win, 560, 340);
    lv_obj_center(interval_editor_win);
    lv_obj_remove_flag(interval_editor_win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(interval_editor_win, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(interval_editor_win, lv_color_hex(0x1F1F1F), 0);
    lv_obj_set_style_border_color(interval_editor_win, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_border_width(interval_editor_win, 2, 0);
    lv_obj_set_style_pad_all(interval_editor_win, 12, 0);
    lv_obj_set_flex_flow(interval_editor_win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(interval_editor_win, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(interval_editor_win, interval_editor_deleted_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *title = lv_label_create(interval_editor_win);
    lv_label_set_text(title, "Editor Intervalli");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *hint = lv_label_create(interval_editor_win);
    lv_label_set_text(hint, "Seleziona DA/A e scegli ON o OFF");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xBEBEBE), 0);

    lv_obj_t *row = lv_obj_create(interval_editor_win);
    lv_obj_set_size(row, 520, 190);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *col_start = lv_obj_create(row);
    lv_obj_set_size(col_start, 160, 180);
    lv_obj_remove_flag(col_start, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(col_start, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(col_start, 0, 0);
    lv_obj_set_style_border_width(col_start, 0, 0);
    lv_obj_set_flex_flow(col_start, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_start, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_label_set_text(lv_label_create(col_start), "DA");

    interval_start_roller = lv_roller_create(col_start);
    lv_roller_set_options(interval_start_roller, interval_time_options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(interval_start_roller, 130, 130);
    lv_roller_set_visible_row_count(interval_start_roller, 4);

    lv_obj_t *col_end = lv_obj_create(row);
    lv_obj_set_size(col_end, 160, 180);
    lv_obj_remove_flag(col_end, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(col_end, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(col_end, 0, 0);
    lv_obj_set_style_border_width(col_end, 0, 0);
    lv_obj_set_flex_flow(col_end, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_end, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_label_set_text(lv_label_create(col_end), "A");

    interval_end_roller = lv_roller_create(col_end);
    lv_roller_set_options(interval_end_roller, interval_time_options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(interval_end_roller, 130, 130);
    lv_roller_set_visible_row_count(interval_end_roller, 4);

    lv_obj_t *col_mode = lv_obj_create(row);
    lv_obj_set_size(col_mode, 160, 180);
    lv_obj_remove_flag(col_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(col_mode, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(col_mode, 0, 0);
    lv_obj_set_style_border_width(col_mode, 0, 0);
    lv_obj_set_flex_flow(col_mode, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_mode, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_label_set_text(lv_label_create(col_mode), "MODALITA'");

    interval_mode_on_btn = lv_button_create(col_mode);
    lv_obj_set_size(interval_mode_on_btn, 110, 45);
    lv_obj_add_event_cb(interval_mode_on_btn, interval_mode_on_cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(interval_mode_on_btn), "ON");

    interval_mode_off_btn = lv_button_create(col_mode);
    lv_obj_set_size(interval_mode_off_btn, 110, 45);
    lv_obj_add_event_cb(interval_mode_off_btn, interval_mode_off_cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(interval_mode_off_btn), "OFF");
    interval_mode_refresh_btns();

    lv_obj_t *actions = lv_obj_create(interval_editor_win);
    lv_obj_set_size(actions, 520, 58);
    lv_obj_remove_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(actions, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(actions, 0, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *btn_apply = lv_button_create(actions);
    lv_obj_set_size(btn_apply, 180, 48);
    lv_obj_set_style_bg_color(btn_apply, lv_color_hex(0x2980B9), 0);
    lv_obj_add_event_cb(btn_apply, interval_editor_apply_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ui_idx);
    lv_label_set_text(lv_label_create(btn_apply), "APPLICA");

    lv_obj_t *btn_close = lv_button_create(actions);
    lv_obj_set_size(btn_close, 180, 48);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_close, interval_editor_close_cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btn_close), "CHIUDI");

    uint64_t slots = configManager.data.weekSchedule[ui_idx].timeSlots;
    int first_on = -1;
    int first_off_after = -1;
    for (int i = 0; i < 48; i++) {
        if (((slots >> i) & 1ULL) != 0) {
            if (first_on < 0) first_on = i;
            continue;
        }
        if (first_on >= 0) {
            first_off_after = i;
            break;
        }
    }
    if (first_on < 0) {
        first_on = 12;        // 06:00
        first_off_after = 16; // 08:00
    }
    if (first_off_after < 0) first_off_after = 48;
    lv_roller_set_selected(interval_start_roller, first_on, LV_ANIM_OFF);
    lv_roller_set_selected(interval_end_roller, first_off_after, LV_ANIM_OFF);
}

static void open_interval_editor_cb(lv_event_t * e) {
    int ui_idx = (int)(intptr_t)lv_event_get_user_data(e);
    create_interval_editor_popup(ui_idx);
}

static lv_obj_t *wizard_day_checks[7] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static constexpr int WIZARD_RANGE_COUNT = 2;
static lv_obj_t *wizard_slot_enable[WIZARD_RANGE_COUNT] = {NULL, NULL};
static lv_obj_t *wizard_slot_start[WIZARD_RANGE_COUNT] = {NULL, NULL};
static lv_obj_t *wizard_slot_end[WIZARD_RANGE_COUNT] = {NULL, NULL};
static lv_obj_t *wizard_status_lbl = NULL;
static int wizard_slot_start_sel[WIZARD_RANGE_COUNT] = {0, 0};
static int wizard_slot_end_sel[WIZARD_RANGE_COUNT] = {0, 0};

static void wizard_roller_event_cb(lv_event_t * e) {
    lv_obj_t *roller = (lv_obj_t *)lv_event_get_target(e);
    const intptr_t packed = (intptr_t)lv_event_get_user_data(e);
    const int idx = (int)(packed >> 1);
    const bool is_end = (packed & 1) != 0;
    if (idx < 0 || idx >= WIZARD_RANGE_COUNT) return;

    int *sel_store = is_end ? &wizard_slot_end_sel[idx] : &wizard_slot_start_sel[idx];
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        *sel_store = lv_roller_get_selected(roller);
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        const int now_sel = lv_roller_get_selected(roller);
        if (now_sel != *sel_store) {
            lv_roller_set_selected(roller, (uint16_t)*sel_store, LV_ANIM_OFF);
        }
    }
}

static void wizard_set_days_mask(uint8_t mask) {
    for (int i = 0; i < 7; i++) {
        if (!wizard_day_checks[i]) continue;
        if (mask & (1U << i)) lv_obj_add_state(wizard_day_checks[i], LV_STATE_CHECKED);
        else lv_obj_remove_state(wizard_day_checks[i], LV_STATE_CHECKED);
    }
}

static uint8_t wizard_get_days_mask() {
    uint8_t mask = 0;
    for (int i = 0; i < 7; i++) {
        if (wizard_day_checks[i] && lv_obj_has_state(wizard_day_checks[i], LV_STATE_CHECKED)) {
            mask |= (uint8_t)(1U << i);
        }
    }
    return mask;
}

static void wizard_day_preset_cb(lv_event_t * e) {
    const int preset = (int)(intptr_t)lv_event_get_user_data(e);
    switch (preset) {
        case 0: wizard_set_days_mask(0x7F); break; // Tutti
        case 1: wizard_set_days_mask(0x1F); break; // Lun-Ven
        case 2: wizard_set_days_mask(0x60); break; // Sab-Dom
        default: wizard_set_days_mask(0x00); break; // Nessuno
    }
}

static void wizard_apply_cb(lv_event_t * e) {
    (void)e;
    const uint8_t days_mask = wizard_get_days_mask();
    if (days_mask == 0U) {
        if (wizard_status_lbl) lv_label_set_text(wizard_status_lbl, "Seleziona almeno un giorno.");
        return;
    }

    uint64_t slots_mask = 0;
    int enabled_ranges = 0;
    for (int i = 0; i < WIZARD_RANGE_COUNT; i++) {
        if (!wizard_slot_enable[i] || !wizard_slot_start[i] || !wizard_slot_end[i]) continue;
        if (!lv_obj_has_state(wizard_slot_enable[i], LV_STATE_CHECKED)) continue;

        int start_slot = lv_roller_get_selected(wizard_slot_start[i]);
        int end_slot = lv_roller_get_selected(wizard_slot_end[i]);

        if (start_slot < 0) start_slot = 0;
        if (start_slot > 47) start_slot = 47;
        if (end_slot < 1) end_slot = 1;
        if (end_slot > 48) end_slot = 48;
        if (end_slot <= start_slot) end_slot = start_slot + 1;
        if (end_slot > 48) end_slot = 48;

        for (int s = start_slot; s < end_slot; s++) {
            slots_mask |= (1ULL << s);
        }
        enabled_ranges++;
    }

    if (enabled_ranges == 0) {
        if (wizard_status_lbl) lv_label_set_text(wizard_status_lbl, "Attiva almeno una fascia oraria.");
        return;
    }

    int selected_days = 0;
    for (int day = 0; day < 7; day++) {
        if ((days_mask & (1U << day)) == 0U) continue;
        configManager.data.weekSchedule[day].timeSlots = slots_mask;
        refresh_intervals_display(day);
        selected_days++;
    }

    configManager.saveConfig();
    update_main_info_label(true);

    char status[96];
    snprintf(
        status,
        sizeof(status),
        "Wizard applicato: %d giorni, %d fasce.",
        selected_days,
        enabled_ranges
    );
    if (wizard_status_lbl) lv_label_set_text(wizard_status_lbl, status);
}

static void build_program_wizard_content(lv_obj_t *parent) {
    build_interval_time_options_once();
    lv_obj_set_style_base_dir(parent, LV_BASE_DIR_LTR, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    for (int i = 0; i < 7; i++) wizard_day_checks[i] = NULL;
    for (int i = 0; i < WIZARD_RANGE_COUNT; i++) {
        wizard_slot_enable[i] = NULL;
        wizard_slot_start[i] = NULL;
        wizard_slot_end[i] = NULL;
        wizard_slot_start_sel[i] = 0;
        wizard_slot_end_sel[i] = 1;
    }
    wizard_status_lbl = NULL;

    // Vertical tuning knob for the whole wizard content.
    const int content_y_shift = -10;

    lv_obj_t *preset_row = lv_obj_create(parent);
    lv_obj_set_size(preset_row, 730, 40);
    lv_obj_align(preset_row, LV_ALIGN_TOP_LEFT, 14, 12 + content_y_shift);
    lv_obj_remove_flag(preset_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(preset_row, 0, 0);
    lv_obj_set_style_border_width(preset_row, 0, 0);
    lv_obj_set_style_base_dir(preset_row, LV_BASE_DIR_LTR, 0);
    lv_obj_set_flex_flow(preset_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(preset_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char *preset_labels[4] = {"Tutti", "Lun-Ven", "Sab-Dom", "Nessuno"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_button_create(preset_row);
        lv_obj_set_size(btn, 120, 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2F4254), 0);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_14, 0);
        lv_obj_add_event_cb(btn, wizard_day_preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_label_set_text(lv_label_create(btn), preset_labels[i]);
    }

    lv_obj_t *btn_apply = lv_button_create(preset_row);
    lv_obj_set_size(btn_apply, 190, 32);
    lv_obj_set_style_bg_color(btn_apply, lv_color_hex(0x1F8A4C), 0);
    lv_obj_set_style_text_font(btn_apply, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(btn_apply, wizard_apply_cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btn_apply), "APPLICA AI GIORNI");

    lv_obj_t *days_row = lv_obj_create(parent);
    lv_obj_set_size(days_row, 730, 34);
    lv_obj_align(days_row, LV_ALIGN_TOP_LEFT, 14, 56 + content_y_shift);
    lv_obj_remove_flag(days_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(days_row, 0, 0);
    lv_obj_set_style_border_width(days_row, 0, 0);
    lv_obj_set_style_base_dir(days_row, LV_BASE_DIR_LTR, 0);
    lv_obj_set_flex_flow(days_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(days_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char *days_short[7] = {"Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t *cb = lv_checkbox_create(days_row);
        lv_checkbox_set_text(cb, days_short[i]);
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(cb, lv_color_hex(0xE3EDF7), 0);
        wizard_day_checks[i] = cb;
    }
    wizard_set_days_mask(0x1F);

    lv_obj_t *ranges = lv_obj_create(parent);
    lv_obj_set_size(ranges, 730, 286);
    lv_obj_align(ranges, LV_ALIGN_TOP_MID, 0, 90 + content_y_shift);
    lv_obj_remove_flag(ranges, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ranges, 0, 0);
    lv_obj_set_style_border_width(ranges, 0, 0);
    lv_obj_set_style_pad_all(ranges, 0, 0);
    lv_obj_set_style_pad_column(ranges, 24, 0);
    lv_obj_set_style_base_dir(ranges, LV_BASE_DIR_LTR, 0);
    lv_obj_set_flex_flow(ranges, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ranges, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    const int start_defaults[WIZARD_RANGE_COUNT] = {12, 24};
    const int end_defaults[WIZARD_RANGE_COUNT] = {16, 28};
    const int card_w = 330;
    const int card_h = 280;
    const int roller_w = 106;
    const int roller_h = card_h - 12;
    const int roller_gap = 16;
    const int rollers_x = (card_w - (roller_w * 2) - roller_gap) / 2;
    const int roller_y = (card_h - roller_h) / 2;
    for (int i = 0; i < WIZARD_RANGE_COUNT; i++) {
        lv_obj_t *card = lv_obj_create(ranges);
        lv_obj_set_size(card, card_w, card_h);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1D2A36), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x41596F), 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_style_base_dir(card, LV_BASE_DIR_LTR, 0);

        lv_obj_t *r1 = lv_roller_create(card);
        lv_roller_set_options(r1, interval_time_options, LV_ROLLER_MODE_NORMAL);
        lv_obj_set_style_text_font(r1, &lv_font_montserrat_24, 0);
        lv_obj_set_size(r1, roller_w, roller_h);
        lv_roller_set_visible_row_count(r1, 5);
        lv_obj_set_pos(r1, rollers_x, roller_y);
        lv_roller_set_selected(r1, start_defaults[i], LV_ANIM_OFF);
        lv_obj_add_event_cb(r1, wizard_roller_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)(i << 1));
        lv_obj_add_event_cb(r1, wizard_roller_event_cb, LV_EVENT_RELEASED, (void *)(intptr_t)(i << 1));
        lv_obj_add_event_cb(r1, wizard_roller_event_cb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)(i << 1));
        wizard_slot_start[i] = r1;
        wizard_slot_start_sel[i] = start_defaults[i];

        lv_obj_t *r2 = lv_roller_create(card);
        lv_roller_set_options(r2, interval_time_options, LV_ROLLER_MODE_NORMAL);
        lv_obj_set_style_text_font(r2, &lv_font_montserrat_24, 0);
        lv_obj_set_size(r2, roller_w, roller_h);
        lv_roller_set_visible_row_count(r2, 5);
        lv_obj_set_pos(r2, rollers_x + roller_w + roller_gap, roller_y);
        lv_roller_set_selected(r2, end_defaults[i], LV_ANIM_OFF);
        lv_obj_add_event_cb(r2, wizard_roller_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)((i << 1) | 1));
        lv_obj_add_event_cb(r2, wizard_roller_event_cb, LV_EVENT_RELEASED, (void *)(intptr_t)((i << 1) | 1));
        lv_obj_add_event_cb(r2, wizard_roller_event_cb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)((i << 1) | 1));
        wizard_slot_end[i] = r2;
        wizard_slot_end_sel[i] = end_defaults[i];

        lv_obj_t *lbl_dash = lv_label_create(card);
        lv_label_set_text(lbl_dash, "-");
        lv_obj_set_style_text_font(lbl_dash, &lv_font_montserrat_18, 0);
        lv_obj_align(lbl_dash, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t *cb = lv_checkbox_create(card);
        lv_checkbox_set_text(cb, i == 0 ? "F1" : "F2");
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_20, 0);
        lv_obj_align(cb, LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_obj_add_state(cb, LV_STATE_CHECKED);
        wizard_slot_enable[i] = cb;
    }
}

void build_scr_program() {
    lv_obj_set_style_bg_color(scr_program, lv_color_hex(0x0D141C), 0);
    lv_obj_set_style_bg_grad_color(scr_program, lv_color_hex(0x162433), 0);
    lv_obj_set_style_bg_grad_dir(scr_program, LV_GRAD_DIR_VER, 0);

    tv_days = NULL;
    for (int i = 0; i < 7; i++) {
        timelines[i] = NULL;
        lbl_intervals[i] = NULL;
        lbl_drag_info[i] = NULL;
    }

    lv_obj_t *title = lv_label_create(scr_program);
    lv_label_set_text(title, "Programmazione - Wizard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 14);

    lv_obj_t *hint = lv_label_create(scr_program);
    lv_label_set_text(hint, "Seleziona giorni e fasce ON/OFF, poi applica ai giorni scelti");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xB7C3CF), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 20, 48);

    lv_obj_t *content = lv_obj_create(scr_program);
    lv_obj_set_size(content, 760, 370);
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, 20, 78);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(content, LV_OPA_20, 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x314355), 0);
    lv_obj_set_style_base_dir(content, LV_BASE_DIR_LTR, 0);
    build_program_wizard_content(content);
    create_home_button(scr_program);
}

void build_scr_impegni() {
    lv_obj_set_style_bg_color(scr_impegni, lv_color_hex(0x051005), 0); 
    lv_obj_t *title = lv_label_create(scr_impegni); lv_label_set_text(title, "Impegni");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);
    list_impegni = lv_list_create(scr_impegni);
    lv_obj_set_size(list_impegni, 700, 350);
    lv_obj_align(list_impegni, LV_ALIGN_CENTER, 0, 20);
    create_home_button(scr_impegni);
}

static void btn_reset_event_cb(lv_event_t * e) {
    (void)e;
    wifi_reset_settings();
}

void build_scr_setup() {
    lv_obj_set_style_bg_color(scr_setup, lv_color_hex(0x1C1E26), 0);
    lv_obj_t *title = lv_label_create(scr_setup); lv_label_set_text(title, "Impostazioni");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    lbl_setup_ssid = lv_label_create(scr_setup); 
    lv_obj_align(lbl_setup_ssid, LV_ALIGN_CENTER, 0, -40);
    
    lbl_setup_ip = lv_label_create(scr_setup); 
    lv_obj_align(lbl_setup_ip, LV_ALIGN_CENTER, 0, 0);
    
    lbl_setup_gw = lv_label_create(scr_setup); 
    lv_obj_align(lbl_setup_gw, LV_ALIGN_CENTER, 0, 40);

    lv_obj_t *btn_reset = lv_button_create(scr_setup); 
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_LEFT, 50, -50); 
    lv_obj_add_event_cb(btn_reset, btn_reset_event_cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btn_reset), "RESET WIFI");
    
    create_home_button(scr_setup);
}

void update_main_info_label(bool force) {
    static int last_day_idx = -1;
    static uint64_t last_slots = UINT64_MAX;

    if(!lbl_date) return;
    time_t now; time(&now); struct tm *timeinfo = localtime(&now);
    if(!timeinfo) return;
    int day_idx = timeinfo->tm_wday; 
    int config_day_idx = (day_idx + 6) % 7;

    uint64_t slots = configManager.data.weekSchedule[config_day_idx].timeSlots;
    if (!force && day_idx == last_day_idx && slots == last_slots) return;

    String info;
    if (slots == 0) {
        info = "Accensioni per oggi: nessuna";
    } else {
        String line1 = "Accensioni: ";
        String line2 = "";
        bool overflow = false;

        int i = 0;
        while (i < 48) {
            if (((slots >> i) & 1ULL) == 0ULL) {
                i++;
                continue;
            }

            int start = i;
            while (i < 48 && ((slots >> i) & 1ULL) != 0ULL) i++;
            int end = i;

            char buf_start[6];
            char buf_end[6];
            get_time_string_from_slot(start, buf_start);
            get_time_string_from_slot(end, buf_end);

            String chunk = "ON ";
            chunk += buf_start;
            chunk += " OFF ";
            chunk += buf_end;

            String chunk_sep = (line1 == "Accensioni: " && line2.length() == 0) ? chunk : " | " + chunk;

            if ((line1.length() + chunk_sep.length()) <= 62) {
                line1 += chunk_sep;
                continue;
            }
            if (line2.length() == 0) {
                line2 = chunk;
                continue;
            }
            if ((line2.length() + 3 + chunk.length()) <= 62) {
                line2 += " | ";
                line2 += chunk;
                continue;
            }

            overflow = true;
            break;
        }

        info = line1;
        if (line2.length() > 0) {
            info += "\n";
            info += line2;
        }
        if (overflow) info += " ...";
    }

    set_label_text_if_changed(lbl_date, info.c_str());
    last_day_idx = day_idx;
    last_slots = slots;
}

void ui_init_all() {
    scr_main = lv_obj_create(NULL);
    scr_program = lv_obj_create(NULL);
    scr_setup = lv_obj_create(NULL);
    scr_impegni = lv_obj_create(NULL);

    build_scr_program();
    build_scr_impegni();
    build_scr_setup();
    build_scr_main();

    lv_scr_load(scr_main);
}

void update_current_weather(String temp, String desc, String iconCode) {
    const lv_image_dsc_t *icon = map_weather_icon(iconCode);
    if (img_weather_today && icon) lv_image_set_src(img_weather_today, icon);
    if(lbl_weather_today_val) lv_label_set_text_fmt(lbl_weather_today_val, "%s°C %s", temp.c_str(), desc.c_str());
}

void update_forecast_item(int index, String day, String temp, String desc, String iconCode) {
    (void)day;
    if (index == 1) { 
        const lv_image_dsc_t *icon = map_weather_icon(iconCode);
        if (img_weather_tmrw && icon) lv_image_set_src(img_weather_tmrw, icon);
        if(lbl_weather_tmrw_val) lv_label_set_text_fmt(lbl_weather_tmrw_val, "%s°C %s", temp.c_str(), desc.c_str());
    }
}

void update_ui() {
    lv_obj_t *act = lv_scr_act();
    if (act == scr_program) {
        static int last_program_tab = -1;
        if (tv_days) {
            const uint32_t idx = lv_tabview_get_tab_active(tv_days);
            if (idx < 7 && (int)idx != last_program_tab) {
                refresh_intervals_display((int)idx);
                if (timelines[idx]) lv_obj_invalidate(timelines[idx]);
                last_program_tab = (int)idx;
            }
        }
        return;
    }

    if (act != scr_main && act != scr_setup) return;

    if (act == scr_main) {
        static char last_daytime[64] = {0};
        static char last_climate[96] = {0};
        static int last_boost_visual_state = -1;
        static char last_boost_text[32] = {0};

        time_t now = time(NULL);
        struct tm timeinfo;
        if (localtime_r(&now, &timeinfo)) {
            if (ui_lbl_hour) {
                const char* days[] = {"Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab"};
                const char* months[] = {"gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic"};
                int day_idx = (timeinfo.tm_wday >= 0 && timeinfo.tm_wday <= 6) ? timeinfo.tm_wday : 0;
                int month_idx = (timeinfo.tm_mon >= 0 && timeinfo.tm_mon <= 11) ? timeinfo.tm_mon : 0;

                char daytime_buf[64];
                snprintf(
                    daytime_buf, sizeof(daytime_buf),
                    "%s %02d/%s %02d:%02d",
                    days[day_idx], timeinfo.tm_mday, months[month_idx], timeinfo.tm_hour, timeinfo.tm_min
                );

                if (strcmp(daytime_buf, last_daytime) != 0) {
                    set_label_text_if_changed(ui_lbl_hour, daytime_buf);
                    strncpy(last_daytime, daytime_buf, sizeof(last_daytime) - 1);
                    last_daytime[sizeof(last_daytime) - 1] = '\0';
                }
            }
        }

        float t = thermo.getCurrentTemp();
        float h = thermo.getHumidity();
        float p = thermo.getPressure(); 

        if(ui_lbl_temp_val) {
            char temp_buf[12];
            char hum_buf[12];
            char press_buf[12];

            if (isnan(t)) snprintf(temp_buf, sizeof(temp_buf), "--.-C");
            else snprintf(temp_buf, sizeof(temp_buf), "%.1fC", t);

            if (isnan(h)) snprintf(hum_buf, sizeof(hum_buf), "--%%");
            else snprintf(hum_buf, sizeof(hum_buf), "%.0f%%", h);

            if (isnan(p) || p <= 0) snprintf(press_buf, sizeof(press_buf), "--");
            else snprintf(press_buf, sizeof(press_buf), "%.0f", p);

            char climate_buf[96];
            snprintf(
                climate_buf, sizeof(climate_buf),
                "Clima Interno: T %s  H %s  P %s hPa",
                temp_buf, hum_buf, press_buf
            );

            if (strcmp(climate_buf, last_climate) != 0) {
                set_label_text_if_changed(ui_lbl_temp_val, climate_buf);
                strncpy(last_climate, climate_buf, sizeof(last_climate) - 1);
                last_climate[sizeof(last_climate) - 1] = '\0';
            }
        }

        int boost_visual_state = 0;
        char boost_text[32] = {0};

        if (!thermo.isRelayOnline()) {
            boost_visual_state = 0;
            snprintf(boost_text, sizeof(boost_text), "Verifica\nraggiungibilita' rele'");
        } else if (thermo.isBoostActive()) {
            boost_visual_state = 1;
            long rem = thermo.getBoostRemainingSeconds();
            long rem_min = (rem + 59) / 60; // arrotondamento per eccesso
            if (rem_min < 1) rem_min = 1;
            snprintf(boost_text, sizeof(boost_text), "Che caldo!!!\n-%ld min", rem_min);
        } else if (thermo.isHeatingState()) {
            boost_visual_state = 2;
            snprintf(boost_text, sizeof(boost_text), "Acceso\n(Spegni)");
        } else {
            boost_visual_state = 3;
            snprintf(boost_text, sizeof(boost_text), "Brr che freddo!!!");
        }

        if (btn_boost && boost_visual_state != last_boost_visual_state) {
            lv_color_t color = lv_color_hex(0x3498DB);
            if (boost_visual_state == 0) color = lv_color_hex(0x555555);
            else if (boost_visual_state == 1) color = lv_color_hex(0xE67E22);
            else if (boost_visual_state == 2) color = lv_color_hex(0xC0392B);
            lv_obj_set_style_bg_color(btn_boost, color, 0);
            if (boost_visual_state == 0) lv_obj_add_state(btn_boost, LV_STATE_DISABLED);
            else lv_obj_remove_state(btn_boost, LV_STATE_DISABLED);
            last_boost_visual_state = boost_visual_state;
        }

        if (lbl_boost_status && strcmp(boost_text, last_boost_text) != 0) {
            set_label_text_if_changed(lbl_boost_status, boost_text);
            strncpy(last_boost_text, boost_text, sizeof(last_boost_text) - 1);
            last_boost_text[sizeof(last_boost_text) - 1] = '\0';
        }
        
        update_main_info_label(false);
    }
    
    if (act == scr_setup) {
        static char last_setup_ssid[48] = {0};
        static char last_setup_ip[48] = {0};
        static char last_setup_gw[48] = {0};

        if (WiFi.status() == WL_CONNECTED) {
            char ssid_buf[48];
            char ip_buf[48];
            char gw_buf[48];
            snprintf(ssid_buf, sizeof(ssid_buf), "SSID: %s", WiFi.SSID().c_str());
            snprintf(ip_buf, sizeof(ip_buf), "IP: %s", WiFi.localIP().toString().c_str());
            snprintf(gw_buf, sizeof(gw_buf), "Gateway: %s", WiFi.gatewayIP().toString().c_str());

            if (strcmp(ssid_buf, last_setup_ssid) != 0) {
                set_label_text_if_changed(lbl_setup_ssid, ssid_buf);
                strncpy(last_setup_ssid, ssid_buf, sizeof(last_setup_ssid) - 1);
                last_setup_ssid[sizeof(last_setup_ssid) - 1] = '\0';
            }
            if (strcmp(ip_buf, last_setup_ip) != 0) {
                set_label_text_if_changed(lbl_setup_ip, ip_buf);
                strncpy(last_setup_ip, ip_buf, sizeof(last_setup_ip) - 1);
                last_setup_ip[sizeof(last_setup_ip) - 1] = '\0';
            }
            if (strcmp(gw_buf, last_setup_gw) != 0) {
                set_label_text_if_changed(lbl_setup_gw, gw_buf);
                strncpy(last_setup_gw, gw_buf, sizeof(last_setup_gw) - 1);
                last_setup_gw[sizeof(last_setup_gw) - 1] = '\0';
            }
        } else {
            set_label_text_if_changed(lbl_setup_ssid, "SSID: Disconnesso");
        }
    }
}
