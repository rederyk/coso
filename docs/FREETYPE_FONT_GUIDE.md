# FreeType Dynamic Font Loading Guide

Complete guide for using dynamic TTF font loading with LVGL FreeType on ESP32-S3.
> Status: FreeType integration and color-emoji loading are still aspirational. No FreeType pipeline or NotoColorEmoji font is compiled in the current firmware—treat this guide as future work notes.

## Overview

This project now supports **dynamic loading of TrueType fonts** from LittleFS at runtime using LVGL's FreeType integration. This allows you to:

- ✅ Use multiple font families and styles without recompiling
- ✅ Load color emoji fonts (NotoColorEmoji.ttf)
- ✅ Switch fonts dynamically from settings
- ✅ Save flash space (fonts are loaded on-demand)
- ✅ Use thousands of Google Fonts without conversion

## Quick Start

### 1. Download Fonts

```bash
cd /home/reder/Documents/PlatformIO/Projects/esp-s3-touc-2.8
./tools/download_fonts.sh
```

This downloads recommended fonts:
- **Inter** (modern sans-serif)
- **Montserrat** (clean UI font)
- **Source Sans 3** (readable body text)
- **Roboto Slab** (friendly serif)
- **Playfair Display** (elegant display)

### 2. Upload to ESP32-S3

```bash
pio run --target uploadfs
```

This uploads the `data/` directory (including fonts) to LittleFS on the ESP32-S3.

### 3. Use in Code

```cpp
#include "core/font_manager.h"

// In your screen's onCreate() or similar:
FontManager& fm = FontManager::getInstance();

// Load a font
lv_font_t* my_font = fm.loadFont(FontManager::FontFamily::INTER,
                                  FontManager::FontWeight::REGULAR,
                                  16);

// Apply to a label
if (my_font) {
    lv_obj_set_style_text_font(my_label, my_font, 0);
}
```

## Architecture

### Components

1. **FontManager** (`src/core/font_manager.h`)
   - Singleton class managing FreeType fonts
   - Provides high-level API for loading/caching fonts
   - Handles font lifecycle and memory management

2. **LVGL LittleFS Driver** (`src/utils/lv_fs_littlefs.h`)
   - Bridges LittleFS with LVGL filesystem API
   - Registers drive 'L:' for font file access
   - Implements read-only file operations

3. **FreeType Integration** (LVGL library)
   - Renders TTF fonts at runtime
   - Caches rendered glyphs for performance
   - Supports multiple font faces and sizes

### Data Flow

```
TTF File (LittleFS)
    ↓
LVGL FS Driver (L:/fonts/...)
    ↓
FreeType Library
    ↓
Rendered Glyphs (cached)
    ↓
LVGL Display
```

## Font Manager API

### Loading Fonts

```cpp
FontManager& fm = FontManager::getInstance();

// Initialize (done in main.cpp automatically)
if (!fm.begin()) {
    // Handle error
}

// Load font with family, weight, and size
lv_font_t* font = fm.loadFont(
    FontManager::FontFamily::INTER,      // Family
    FontManager::FontWeight::BOLD,       // Weight
    20                                    // Size in pixels
);
```

### Available Fonts

#### Font Families
```cpp
enum class FontFamily {
    INTER,              // Modern, legible sans-serif
    MONTSERRAT,         // Clean UI font (matches LVGL default)
    SOURCE_SANS_3,      // Readable body text
    ROBOTO_SLAB,        // Friendly serif for headings
    PLAYFAIR_DISPLAY,   // Elegant display font
    NOTO_EMOJI          // Color emoji (if uploaded)
};
```

#### Font Weights
```cpp
enum class FontWeight {
    LIGHT,              // Light weight
    REGULAR,            // Normal/regular weight
    BOLD                // Bold weight
};
```

### Caching

Fonts are cached automatically:
```cpp
// First call loads from file
lv_font_t* font1 = fm.loadFont(FontFamily::INTER, FontWeight::REGULAR, 16);

// Second call returns cached font (instant)
lv_font_t* font2 = fm.loadFont(FontFamily::INTER, FontWeight::REGULAR, 16);

// Check cache size
size_t count = fm.getCacheSize();  // Returns 1
```

### Memory Management

```cpp
// Unload specific font
fm.unloadFont(FontFamily::INTER, FontWeight::BOLD, 20);

// Unload all fonts (useful when switching screens)
fm.unloadAll();
```

## Usage Patterns

### Pattern 1: Screen-Specific Fonts

```cpp
class MyScreen : public Screen {
public:
    void onCreate(lv_obj_t* parent) override {
        FontManager& fm = FontManager::getInstance();

        // Load fonts for this screen
        title_font_ = fm.loadFont(FontFamily::ROBOTO_SLAB, FontWeight::BOLD, 24);
        body_font_ = fm.loadFont(FontFamily::INTER, FontWeight::REGULAR, 16);

        // Create UI
        lv_obj_t* title = lv_label_create(parent);
        lv_label_set_text(title, "My Screen");
        lv_obj_set_style_text_font(title, title_font_, 0);
    }

    void onDestroy() override {
        // Optional: unload fonts to save memory
        // FontManager::getInstance().unloadAll();
    }

private:
    lv_font_t* title_font_;
    lv_font_t* body_font_;
};
```

### Pattern 2: Theme-Based Font Selection

```cpp
class ThemeManager {
public:
    enum class Theme {
        MODERN,     // Inter
        CLASSIC,    // Montserrat
        ELEGANT     // Playfair Display
    };

    void applyTheme(Theme theme) {
        FontManager& fm = FontManager::getInstance();

        switch (theme) {
            case Theme::MODERN:
                body_font_ = fm.loadFont(FontFamily::INTER, FontWeight::REGULAR, 16);
                break;
            case Theme::CLASSIC:
                body_font_ = fm.loadFont(FontFamily::MONTSERRAT, FontWeight::REGULAR, 16);
                break;
            case Theme::ELEGANT:
                body_font_ = fm.loadFont(FontFamily::PLAYFAIR_DISPLAY, FontWeight::REGULAR, 16);
                break;
        }

        // Apply to all UI elements
        updateAllLabels();
    }
};
```

### Pattern 3: Dynamic Font Switching

```cpp
void Settings::onFontFamilyChanged(FontFamily new_family) {
    FontManager& fm = FontManager::getInstance();

    // Load new font
    lv_font_t* new_font = fm.loadFont(new_family, FontWeight::REGULAR, 16);

    if (new_font) {
        // Apply to all labels in app
        lv_obj_t* screen = lv_scr_act();
        updateAllChildrenFonts(screen, new_font);

        // Save preference
        SettingsManager::getInstance().setFontFamily((uint8_t)new_family);
    }
}
```

## Color Emoji Support

### Using Noto Color Emoji

⚠️ **Warning:** NotoColorEmoji.ttf is ~10MB - too large for ESP32-S3 flash! Consider using PNG emoji with imgfont instead.

If you have enough flash space:

```cpp
// Load emoji font as fallback
lv_font_t* emoji_font = fm.loadFont(FontFamily::NOTO_EMOJI, FontWeight::REGULAR, 20);

// Use with text
lv_obj_t* label = lv_label_create(parent);
lv_label_set_text(label, "Hello 👋 World 🌍");
lv_obj_set_style_text_font(label, emoji_font, 0);
```

### Alternative: PNG Emoji (Recommended)

For better memory efficiency, use PNG images with LVGL's imgfont feature:

```cpp
#define LV_USE_IMGFONT 1  // in lv_conf.h

// Create imgfont from PNG files
lv_imgfont_t* img_font = lv_imgfont_create(32, imgfont_get_path_cb);

// Use like regular font
lv_obj_set_style_text_font(label, img_font, 0);
```

See `data/fonts/README.md` for PNG emoji resources.

## Performance Considerations

### FreeType Cache Settings

In [src/config/lv_conf.h](src/config/lv_conf.h:669):

```c
#define LV_USE_FREETYPE 1
#define LV_FREETYPE_CACHE_SIZE (32 * 1024)  // 32KB cache
#define LV_FREETYPE_SBIT_CACHE 1            // Efficient for small fonts
#define LV_FREETYPE_CACHE_FT_FACES 8        // Max 8 font faces
#define LV_FREETYPE_CACHE_FT_SIZES 8        // Max 8 font sizes
```

### Memory Usage

| Component | Memory | Location |
|-----------|--------|----------|
| FreeType library | ~80KB | Flash |
| Glyph cache | 32KB | PSRAM/RAM |
| Font face (TTF) | Variable | LittleFS (on-demand) |
| Rendered glyphs | Cached | PSRAM/RAM |

### Optimization Tips

1. **Preload common fonts** during setup to avoid delays
2. **Unload unused fonts** when switching screens
3. **Use smaller sizes** (14-20px instead of 24-48px)
4. **Limit font variations** (2-3 families max)
5. **Cache efficiently** - reuse same font objects

## Troubleshooting

### Font Not Loading

**Problem:** `loadFont()` returns `nullptr`

**Solutions:**
1. Check file exists: `ls data/fonts/`
2. Upload filesystem: `pio run --target uploadfs`
3. Check serial logs for FreeType errors
4. Verify path format: `L:/fonts/FontName.ttf`

```bash
# List uploaded files
pio device monitor --filter log2file

# In ESP32 serial console, check LittleFS:
# (Add this to your code for debugging)
File root = LittleFS.open("/fonts");
File file = root.openNextFile();
while(file) {
    Serial.println(file.name());
    file = root.openNextFile();
}
```

### Out of Memory

**Problem:** ESP32 crashes or fonts fail to load

**Solutions:**
1. Reduce cache size in `lv_conf.h`
2. Unload fonts when not needed
3. Use smaller font sizes
4. Reduce number of cached faces

```cpp
// Before loading many fonts
FontManager::getInstance().unloadAll();

// Load only what you need
auto font = fm.loadFont(family, weight, size);
// Use immediately
// Then unload if needed
```

### Slow Font Rendering

**Problem:** Text rendering is laggy

**Solutions:**
1. Increase FreeType cache size
2. Preload fonts during boot
3. Avoid reloading fonts frequently
4. Use built-in LVGL fonts for UI chrome

```cpp
// Preload in setup()
void setup() {
    // ... other setup ...

    FontManager& fm = FontManager::getInstance();
    fm.begin();

    // Preload common fonts
    fm.loadFont(FontFamily::INTER, FontWeight::REGULAR, 16);
    fm.loadFont(FontFamily::INTER, FontWeight::BOLD, 20);
}
```

### File System Errors

**Problem:** `[LittleFS] Failed to initialize`

**Solutions:**
1. Check partition table has LittleFS partition
2. Format filesystem: `LittleFS.begin(true)` (true = format if needed)
3. Verify board configuration in platformio.ini

## Examples

See [src/examples/font_test_example.cpp](src/examples/font_test_example.cpp) for complete examples:

- Basic font loading
- Dynamic font switching
- Cache management
- Typical screen implementation

To test:
```cpp
#include "examples/font_test_example.h"

// In your app:
FontTestExample::createTestScreen(screen);
```

## File Structure

```
esp-s3-touc-2.8/
├── data/
│   └── fonts/
│       ├── Inter-Regular.ttf
│       ├── Inter-Bold.ttf
│       ├── Montserrat-Regular.ttf
│       └── ...
├── src/
│   ├── core/
│   │   ├── font_manager.h         # Font manager API
│   │   └── font_manager.cpp
│   ├── utils/
│   │   ├── lv_fs_littlefs.h       # LittleFS driver
│   │   └── lv_fs_littlefs.cpp
│   ├── examples/
│   │   ├── font_test_example.h    # Usage examples
│   │   └── font_test_example.cpp
│   └── config/
│       └── lv_conf.h               # LVGL config (FreeType enabled)
├── tools/
│   └── download_fonts.sh           # Font download script
└── docs/
    └── FREETYPE_FONT_GUIDE.md      # This file
```

## Next Steps

1. **Download fonts:** `./tools/download_fonts.sh`
2. **Upload filesystem:** `pio run --target uploadfs`
3. **Test fonts:** Use `FontTestExample::createTestScreen()`
4. **Integrate into settings:** Add font family dropdown to Settings screen
5. **Create theme system:** Map font families to theme presets

## Further Reading

- [LVGL FreeType Documentation](https://docs.lvgl.io/master/details/libs/font_support/freetype.html)
- [LVGL Font Documentation](https://docs.lvgl.io/9.2/overview/font.html)
- [Google Fonts](https://fonts.google.com/) - Source for TTF fonts
- [Noto Emoji](https://github.com/googlefonts/noto-emoji) - Open source emoji font

---

**Generated:** 2025-11-19
**Project:** ESP32-S3 Touch Display (Freenove FNK0104)
**LVGL Version:** 8.4.0
