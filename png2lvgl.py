#!/usr/bin/env python3
"""
Convertitore PNG per LVGL 9 - Versione Corretta per ESP32
Richiede: pip install Pillow
"""

from PIL import Image
import sys
import os
import argparse

def convert_to_argb8888(img_path, output_path, var_name="img"):
    """Converte a ARGB8888 (32-bit: Blue, Green, Red, Alpha)"""
    try:
        img = Image.open(img_path).convert('RGBA')
        w, h = img.size
        pixels = list(img.getdata())
        
        print(f"📐 Dimensioni: {w}x{h}")
        print(f"🎨 Formato: ARGB8888 (LVGL 9 native)")
        
        with open(output_path, 'w') as f:
            # Intestazione file
            f.write(f'#include <lvgl.h>\n\n')
            f.write(f'#ifndef LV_ATTRIBUTE_MEM_ALIGN\n')
            f.write(f'#define LV_ATTRIBUTE_MEM_ALIGN\n')
            f.write(f'#endif\n\n')
            
            # Array dati
            f.write(f'/* Dati immagine: {w}x{h} ARGB8888 */\n')
            f.write(f'const uint8_t {var_name}_map[] LV_ATTRIBUTE_MEM_ALIGN = {{\n')
            
            # Scrive i pixel in ordine B, G, R, A (Little Endian)
            count = 0
            for r, g, b, a in pixels:
                if count % 4 == 0:
                    f.write('    ')
                # Ordine IMPORTANTE per ESP32: B, G, R, A
                f.write(f'0x{b:02X}, 0x{g:02X}, 0x{r:02X}, 0x{a:02X}, ')
                count += 1
                if count % 4 == 0:
                    f.write('\n')
            
            f.write('};\n\n')
            
            # Descriptor LVGL 9 (lv_image_dsc_t)
            stride = w * 4
            f.write(f'const lv_image_dsc_t {var_name} = {{\n')
            f.write(f'    .header = {{\n')
            f.write(f'        .magic = LV_IMAGE_HEADER_MAGIC,\n')
            f.write(f'        .cf = LV_COLOR_FORMAT_ARGB8888,\n')
            f.write(f'        .flags = 0,\n')
            f.write(f'        .w = {w},\n')
            f.write(f'        .h = {h},\n')
            f.write(f'        .stride = {stride},\n')
            f.write(f'        .reserved_2 = 0\n')
            f.write(f'    }},\n')
            f.write(f'    .data_size = {stride * h},\n')
            f.write(f'    .data = {var_name}_map,\n')
            f.write(f'}};\n')
        
        print(f"✅ File generato: {output_path}")
        return True
        
    except Exception as e:
        print(f"❌ Errore: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Convertitore PNG a C per LVGL 9')
    parser.add_argument('input', help='File PNG di input')
    parser.add_argument('-n', '--name', default='logo_splash', help='Nome variabile (default: logo_splash)')
    
    args = parser.parse_args()
    
    # Determina output
    output_path = os.path.join("src", f"{args.name}.c")
    
    # Crea cartella src se non esiste (caso di test isolati)
    if not os.path.exists("src") and not os.path.exists("platformio.ini"):
         output_path = f"{args.name}.c"

    convert_to_argb8888(args.input, output_path, args.name)

if __name__ == "__main__":
    main()