#ifndef LV_CONF_H
#define LV_CONF_H

// --- CONFIGURAZIONE MEMORIA ---
// USARE "LV_STDLIB_CLIB" INVECE DI "BUILTIN"
// Questo dice a LVGL di usare malloc() di sistema.
// Grazie al flag -D BOARD_HAS_PSRAM in platformio.ini, 
// malloc() userà automaticamente la PSRAM per le allocazioni grafiche,
// evitando di intasare la piccola RAM interna (DRAM).
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

// Con CLIB, LV_MEM_SIZE viene ignorato (o usato solo per info), 
// ma lo lasciamo definito per compatibilità.
#define LV_MEM_SIZE (256 * 1024U) 

// --- RENDERING ---
#define LV_COLOR_DEPTH 16
#define LV_DEF_REFR_PERIOD 16  // ~60 FPS target
#define LV_DPI_DEF 130

// --- FONT ABILITATI ---
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_36 1

// --- WIDGET & FEATURES ---
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_USE_LOG 0
#define LV_USE_LABEL 1
#define LV_USE_ARC 1
#define LV_USE_QRCODE 1 
#define LV_USE_ROLLER 1
#define LV_USE_MSGBOX 1
#define LV_USE_TABVIEW 1
#define LV_USE_BUTTON 1

// --- OTTIMIZZAZIONI ---
// Disabilita assert pesanti in produzione
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0

#endif
