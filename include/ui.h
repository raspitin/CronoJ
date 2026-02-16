#ifndef SMART_THERMO_UI_H  // CAMBIATO: Nome unico per evitare conflitti
#define SMART_THERMO_UI_H

#include <Arduino.h>
#include <lvgl.h>

// --- VARIABILI GLOBALI UI ---
extern lv_obj_t *scr_splash;
extern lv_obj_t *scr_main;
extern lv_obj_t *scr_program;
extern lv_obj_t *scr_setup;
extern lv_obj_t *scr_impegni;

// Widget Home Accessibili
extern lv_obj_t *ui_lbl_temp_val;
extern lv_obj_t *ui_lbl_hum_val;
extern lv_obj_t *ui_lbl_press_val; // NUOVO: Pressione

// --- FUNZIONI DI GESTIONE UI ---
void ui_init_all();
void ui_show_splash();
void ui_splash_config_mode(); // Ora visibile correttamente al Network Manager
void update_ui(); 
void ui_show_ota_screen(uint8_t progress_pct);
void ui_hide_ota_screen();

// --- FUNZIONI METEO ---
void update_current_weather(String temp, String desc, String iconCode);
void update_forecast_item(int index, String day, String temp, String desc, String iconCode);

void show_relay_error_popup();

#endif
