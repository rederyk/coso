# Emoji Support Setup Guide

## Current Status
✅ LVGL Symbols implemented (FontAwesome icons)
🔄 Real emoji support - IN PROGRESS

## Required Emojis

Unicode characters needed for the UI:

```
🏠 U+1F3E0 - Home
⚙️ U+2699  - Settings (+ U+FE0F variation selector)
🛰️ U+1F6F0 - Satellite (+ U+FE0F variation selector)
ℹ️ U+2139  - Info (+ U+FE0F variation selector)
❌ U+274C  - Cross Mark
⚠️ U+26A0  - Warning (+ U+FE0F variation selector)
🎨 U+1F3A8 - Artist Palette
📶 U+1F4F6 - Antenna Bars
💡 U+1F4A1 - Light Bulb
🖥️ U+1F5A5 - Desktop Computer (+ U+FE0F variation selector)
⚡ U+26A1  - High Voltage
📊 U+1F4CA - Bar Chart
💾 U+1F4BE - Floppy Disk
🔧 U+1F527 - Wrench
🔄 U+1F504 - Counterclockwise Arrows
👆 U+1F446 - Backhand Index Pointing Up
🖌️ U+1F58C - Paintbrush (+ U+FE0F variation selector)
```

Total: 17 unique emoji + variation selectors

## Method 1: LVGL Online Font Converter (Recommended)

### Step 1: Open Font Converter
https://lvgl.io/tools/fontconverter

### Step 2: Configure Settings

**Font Settings:**
- Name: `emoji_montserrat_14` (or desired size)
- Size: 14, 16, 20, 22, 24 (create one for each size)
- Bpp: 4 bit-per-pixel
- TTF Font: Upload Noto Color Emoji or Noto Emoji (download from Google Fonts)

**Range Settings:**
Select "Symbols" and add these Unicode ranges:
```
0x1F3E0        # 🏠
0x2699         # ⚙️
0x1F6F0        # 🛰️
0x2139         # ℹ️
0x274C         # ❌
0x26A0         # ⚠️
0x1F3A8        # 🎨
0x1F4F6        # 📶
0x1F4A1        # 💡
0x1F5A5        # 🖥️
0x26A1         # ⚡
0x1F4CA        # 📊
0x1F4BE        # 💾
0x1F527        # 🔧
0x1F504        # 🔄
0x1F446        # 👆
0x1F58C        # 🖌️
0xFE0F         # Variation selector
```

Also add ASCII printable range for fallback text:
```
0x20-0x7E      # ASCII printable
```

### Step 3: Generate and Download
- Click "Convert"
- Download the `.c` file
- Save to `src/ui/fonts/emoji_montserrat_14.c`

### Step 4: Declare Font
Create `src/ui/fonts/emoji_fonts.h`:
```c
#ifndef EMOJI_FONTS_H
#define EMOJI_FONTS_H

#include <lvgl.h>

LV_FONT_DECLARE(emoji_montserrat_14);
LV_FONT_DECLARE(emoji_montserrat_16);
LV_FONT_DECLARE(emoji_montserrat_20);
LV_FONT_DECLARE(emoji_montserrat_22);
LV_FONT_DECLARE(emoji_montserrat_24);

#endif
```

## Method 2: Use Pre-built LVGL Emoji Font (Quick)

Some LVGL distributions include emoji fonts. Check:
```
.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/include/lvgl/src/font/
```

## Method 3: Font Fallback Chain (Advanced)

Configure LVGL to use fallback fonts:
1. Primary: Montserrat (ASCII)
2. Fallback: Emoji font (Unicode emoji range)

This requires modifying `lv_conf.h` to enable `LV_FONT_CUSTOM_DECLARE`.

## Integration Steps

Once you have the emoji font files:

1. **Add to project structure:**
   ```
   src/ui/fonts/
   ├── emoji_montserrat_14.c
   ├── emoji_montserrat_16.c
   ├── emoji_montserrat_20.c
   ├── emoji_montserrat_22.c
   ├── emoji_montserrat_24.c
   └── emoji_fonts.h
   ```

2. **Update ui_symbols.h to use real emoji:**
   ```c
   #define UI_SYMBOL_HOME          "🏠"
   #define UI_SYMBOL_SETTINGS      "⚙️"
   // etc...
   ```

3. **Replace font references:**
   Change `&lv_font_montserrat_14` to `&emoji_montserrat_14`

4. **Build and test**

## Estimated Memory Impact

- Each emoji glyph: ~500-1000 bytes (4bpp, ~20x20 pixels)
- 17 emojis × 5 sizes = 85 glyphs
- Estimated: **40-85 KB** of Flash

With 16MB Flash available, this is totally acceptable!

## Alternative: External Font File

Instead of compiling into firmware, load from LittleFS:
- Store `.bin` font file in filesystem
- Use `lv_font_load()` at runtime
- Pros: Reduces firmware size, easier updates
- Cons: Slightly slower first load

## Resources

- LVGL Font Converter: https://lvgl.io/tools/fontconverter
- Noto Emoji Font: https://fonts.google.com/noto/specimen/Noto+Emoji
- Noto Color Emoji: https://github.com/googlefonts/noto-emoji
