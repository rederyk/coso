# Architettura UI e Theme System

**Data:** 2025-11-18
**Versione:** 1.0.0
**Status:** 📋 PIANIFICATO

> **⚠️ NOTA:** Questo documento descrive l'architettura UI TARGET con sistema di temi JSON.
> Per lo stato attuale del progetto, consultare **[STATO_ATTUALE_PROGETTO.md](STATO_ATTUALE_PROGETTO.md)**

---

## 1. Executive Summary

Questo documento definisce l'architettura del sistema UI per l'OS ESP32-S3, con focus su:

- **Theme System JSON-based** - Temi configurabili via file JSON
- **Asset Manager** - Gestione centralizzata risorse grafiche (icone, font, immagini)
- **Style Manager** - Applicazione dinamica stili LVGL
- **Hot-reload** - Cambio tema runtime senza riavvio
- **Modularità** - Temi completamente separati dal codice
- **Estensibilità** - Supporto pacchetti grafici custom

### Vantaggi Chiave

✅ **Designer-friendly** - I designer possono creare temi senza toccare il codice C++
✅ **Consistenza** - Stili centralizzati applicati a tutti i widget
✅ **Personalizzazione** - Utente può scegliere tema preferito
✅ **Manutenibilità** - Modifiche UI isolate in JSON
✅ **Storage efficiente** - Temi caricati on-demand da SPIFFS/LittleFS

---

## 2. Architettura Generale

### 2.1 Stack UI Completo

```
┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ Dashboard    │  │ Settings     │  │ Custom Apps  │          │
│  │              │  │              │  │              │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                  │                  │                   │
└─────────┼──────────────────┼──────────────────┼───────────────────┘
          │                  │                  │
┌─────────▼──────────────────▼──────────────────▼───────────────────┐
│                        WIDGET LAYER                                │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐                  │
│  │ WiFiWidget │  │ ClockWidget│  │ InfoWidget │  ...              │
│  └────────────┘  └────────────┘  └────────────┘                  │
└────────────────────────────────────────────────────────────────────┘
          │                  │                  │
┌─────────▼──────────────────▼──────────────────▼───────────────────┐
│                     UI FRAMEWORK LAYER                             │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │                    Style Manager                              │ │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐             │ │
│  │  │ Apply      │  │ Transition │  │ State      │             │ │
│  │  │ Styles     │  │ Animator   │  │ Manager    │             │ │
│  │  └────────────┘  └────────────┘  └────────────┘             │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                │                                    │
│  ┌──────────────────────────────▼──────────────────────────────┐  │
│  │                      Theme Manager                            │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐             │  │
│  │  │ Load Theme │  │ Parse JSON │  │ Hot Reload │             │  │
│  │  │ from JSON  │  │ Validation │  │ Runtime    │             │  │
│  │  └────────────┘  └────────────┘  └────────────┘             │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                │                                    │
│  ┌──────────────────────────────▼──────────────────────────────┐  │
│  │                      Asset Manager                            │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐             │  │
│  │  │ Icon Cache │  │ Font Loader│  │ Image      │             │  │
│  │  │            │  │            │  │ Decoder    │             │  │
│  │  └────────────┘  └────────────┘  └────────────┘             │  │
│  └──────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────┘
          │                  │                  │
┌─────────▼──────────────────▼──────────────────▼───────────────────┐
│                         LVGL CORE                                  │
│  - Object System                                                   │
│  - Style System (lv_style_t)                                       │
│  - Draw Engine                                                     │
│  - Event System                                                    │
└────────────────────────────────────────────────────────────────────┘
          │
┌─────────▼──────────────────────────────────────────────────────────┐
│                     FILE SYSTEM LAYER                              │
│  SPIFFS / LittleFS                                                 │
│  └─ /data/themes/                                                  │
│     ├─ light.json       (Tema chiaro)                              │
│     ├─ dark.json        (Tema scuro)                               │
│     ├─ custom.json      (Temi utente)                              │
│  └─ /data/assets/                                                  │
│     ├─ icons/           (Icone PNG/C array)                        │
│     ├─ fonts/           (Font binari)                              │
│     └─ images/          (Background, splash)                       │
└────────────────────────────────────────────────────────────────────┘
```

---

## 3. Theme Manager

### 3.1 Classe ThemeManager

```cpp
// core/ui/ThemeManager.h
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <lvgl.h>
#include <map>
#include <string>

// Struct per rappresentare un tema completo
struct Theme {
  String name;
  String author;
  String version;

  // Colors (palette principale)
  struct Colors {
    lv_color_t primary;
    lv_color_t secondary;
    lv_color_t accent;
    lv_color_t background;
    lv_color_t surface;
    lv_color_t text_primary;
    lv_color_t text_secondary;
    lv_color_t error;
    lv_color_t success;
    lv_color_t warning;
    lv_color_t info;
  } colors;

  // Typography
  struct Typography {
    const lv_font_t* font_small;     // 12px
    const lv_font_t* font_normal;    // 16px
    const lv_font_t* font_large;     // 20px
    const lv_font_t* font_title;     // 24px
    const lv_font_t* font_display;   // 32px
  } typography;

  // Spacing & Sizing
  struct Dimensions {
    uint8_t padding_small;      // 4px
    uint8_t padding_medium;     // 8px
    uint8_t padding_large;      // 16px
    uint8_t border_radius;      // 8px
    uint8_t border_width;       // 2px
  } dimensions;

  // Component-specific styles
  struct Components {
    lv_style_t button_primary;
    lv_style_t button_secondary;
    lv_style_t card;
    lv_style_t input;
    lv_style_t navbar;
    lv_style_t statusbar;
    lv_style_t modal;
    lv_style_t list_item;
  } components;

  // Animations
  struct Animations {
    uint16_t duration_fast;     // 150ms
    uint16_t duration_normal;   // 300ms
    uint16_t duration_slow;     // 500ms
    lv_anim_path_cb_t easing;   // ease-in-out, linear, etc.
  } animations;
};

class ThemeManager {
public:
  static ThemeManager& getInstance();

  // Lifecycle
  bool begin();
  void end();

  // Theme management
  bool loadTheme(const String& themePath);
  bool loadThemeFromMemory(const char* jsonString);
  bool applyTheme();
  bool setActiveTheme(const String& themeName);
  String getActiveThemeName() const;

  // Theme listing
  std::vector<String> getAvailableThemes();
  bool isThemeLoaded() const;

  // Getters
  const Theme& getCurrentTheme() const { return currentTheme_; }
  lv_color_t getColor(const String& colorName) const;
  const lv_font_t* getFont(const String& fontName) const;
  const lv_style_t* getStyle(const String& styleName) const;

  // Hot reload
  bool reloadTheme();
  void setThemeChangeCallback(std::function<void(const String&)> callback);

  // Theme creation helpers
  static bool createDefaultThemes();
  static bool exportTheme(const Theme& theme, const String& outputPath);

private:
  ThemeManager() = default;
  ~ThemeManager() = default;
  ThemeManager(const ThemeManager&) = delete;
  ThemeManager& operator=(const ThemeManager&) = delete;

  // Parsing
  bool parseThemeJSON(const JsonDocument& doc);
  bool parseColors(const JsonObject& colorsObj);
  bool parseTypography(const JsonObject& typObj);
  bool parseDimensions(const JsonObject& dimObj);
  bool parseComponents(const JsonObject& compObj);
  bool parseAnimations(const JsonObject& animObj);

  // Style builders
  void buildComponentStyles();
  void buildButtonStyles();
  void buildCardStyles();
  void buildInputStyles();
  void buildNavbarStyles();

  // Helpers
  lv_color_t hexToLvColor(const String& hex) const;
  const lv_font_t* loadFontByName(const String& fontName) const;

  Theme currentTheme_;
  String activeThemePath_;
  std::function<void(const String&)> onThemeChanged_;
  bool initialized_ = false;
};
```

### 3.2 Utilizzo ThemeManager

```cpp
// In setup()
ThemeManager& themeMgr = ThemeManager::getInstance();
themeMgr.begin();

// Carica tema dark
if (themeMgr.loadTheme("/data/themes/dark.json")) {
  themeMgr.applyTheme();
  Serial.println("Tema Dark caricato con successo");
}

// Callback per cambio tema
themeMgr.setThemeChangeCallback([](const String& themeName) {
  Serial.printf("Tema cambiato: %s\n", themeName.c_str());
  // Aggiorna tutti i widget
  EventRouter::publish("theme.changed", themeName);
});

// Nel codice widget/screen
lv_obj_t* btn = lv_btn_create(parent);
lv_obj_add_style(btn, themeMgr.getCurrentTheme().components.button_primary, 0);

// Accesso diretto ai colori
lv_obj_set_style_bg_color(label, themeMgr.getColor("primary"), 0);
lv_obj_set_style_text_font(label, themeMgr.getFont("title"), 0);
```

---

## 4. Struttura JSON del Tema

### 4.1 Schema Completo Theme JSON

```json
{
  "theme": {
    "metadata": {
      "name": "Dark Professional",
      "version": "1.0.0",
      "author": "ESP32 Design Team",
      "description": "Tema scuro professionale con accenti blu",
      "created": "2025-11-18",
      "compatible_ui_version": "1.0.0"
    },

    "colors": {
      "primary": "#2196F3",
      "secondary": "#FF5722",
      "accent": "#FFC107",
      "background": "#121212",
      "surface": "#1E1E1E",
      "surface_variant": "#2C2C2C",
      "text_primary": "#FFFFFF",
      "text_secondary": "#B0B0B0",
      "text_disabled": "#666666",
      "error": "#F44336",
      "success": "#4CAF50",
      "warning": "#FF9800",
      "info": "#2196F3",
      "divider": "#333333",
      "overlay": "#00000080"
    },

    "typography": {
      "font_small": {
        "name": "Roboto_12",
        "size": 12,
        "file": "/data/fonts/roboto_12.bin"
      },
      "font_normal": {
        "name": "Roboto_16",
        "size": 16,
        "file": "/data/fonts/roboto_16.bin"
      },
      "font_large": {
        "name": "Roboto_20",
        "size": 20,
        "file": "/data/fonts/roboto_20.bin"
      },
      "font_title": {
        "name": "Roboto_Bold_24",
        "size": 24,
        "file": "/data/fonts/roboto_bold_24.bin"
      },
      "font_display": {
        "name": "Roboto_Bold_32",
        "size": 32,
        "file": "/data/fonts/roboto_bold_32.bin"
      }
    },

    "dimensions": {
      "padding_small": 4,
      "padding_medium": 8,
      "padding_large": 16,
      "padding_xlarge": 24,
      "border_radius_small": 4,
      "border_radius_medium": 8,
      "border_radius_large": 16,
      "border_width": 2,
      "elevation_1": 2,
      "elevation_2": 4,
      "elevation_3": 8
    },

    "components": {
      "button": {
        "primary": {
          "bg_color": "$primary",
          "text_color": "#FFFFFF",
          "border_color": "$primary",
          "border_width": 0,
          "border_radius": "$border_radius_medium",
          "padding_horizontal": "$padding_large",
          "padding_vertical": "$padding_medium",
          "shadow_width": "$elevation_2",
          "shadow_color": "#00000040",
          "hover": {
            "bg_color": "#1976D2"
          },
          "pressed": {
            "bg_color": "#0D47A1"
          },
          "disabled": {
            "bg_color": "#333333",
            "text_color": "$text_disabled"
          }
        },
        "secondary": {
          "bg_color": "transparent",
          "text_color": "$primary",
          "border_color": "$primary",
          "border_width": "$border_width",
          "border_radius": "$border_radius_medium",
          "padding_horizontal": "$padding_large",
          "padding_vertical": "$padding_medium",
          "hover": {
            "bg_color": "#1976D220"
          }
        }
      },

      "card": {
        "bg_color": "$surface",
        "border_radius": "$border_radius_large",
        "padding": "$padding_large",
        "shadow_width": "$elevation_1",
        "shadow_color": "#00000060"
      },

      "input": {
        "bg_color": "$surface_variant",
        "text_color": "$text_primary",
        "placeholder_color": "$text_secondary",
        "border_color": "#444444",
        "border_width": 1,
        "border_radius": "$border_radius_medium",
        "padding": "$padding_medium",
        "focus": {
          "border_color": "$primary",
          "border_width": 2
        },
        "error": {
          "border_color": "$error"
        }
      },

      "navbar": {
        "bg_color": "$surface",
        "height": 50,
        "padding": "$padding_medium",
        "shadow_width": "$elevation_2",
        "title_color": "$text_primary",
        "title_font": "$font_large",
        "icon_color": "$text_primary"
      },

      "statusbar": {
        "bg_color": "$background",
        "height": 24,
        "text_color": "$text_secondary",
        "text_font": "$font_small",
        "icon_size": 16
      },

      "modal": {
        "overlay_color": "$overlay",
        "bg_color": "$surface",
        "border_radius": "$border_radius_large",
        "padding": "$padding_large",
        "shadow_width": "$elevation_3"
      },

      "list_item": {
        "bg_color": "transparent",
        "text_color": "$text_primary",
        "padding": "$padding_medium",
        "divider_color": "$divider",
        "hover": {
          "bg_color": "$surface_variant"
        },
        "selected": {
          "bg_color": "$primary",
          "text_color": "#FFFFFF"
        }
      },

      "switch": {
        "track_bg_off": "#444444",
        "track_bg_on": "$primary",
        "thumb_bg": "#FFFFFF",
        "width": 40,
        "height": 20,
        "thumb_size": 16
      },

      "slider": {
        "track_bg": "#444444",
        "indicator_bg": "$primary",
        "knob_bg": "#FFFFFF",
        "height": 4,
        "knob_size": 20
      }
    },

    "animations": {
      "duration_fast": 150,
      "duration_normal": 300,
      "duration_slow": 500,
      "easing": "ease_in_out"
    },

    "assets": {
      "icons": {
        "wifi": "/data/icons/wifi.png",
        "bluetooth": "/data/icons/bluetooth.png",
        "battery": "/data/icons/battery.png",
        "settings": "/data/icons/settings.png",
        "home": "/data/icons/home.png",
        "back": "/data/icons/back.png"
      },
      "images": {
        "splash": "/data/images/splash.png",
        "logo": "/data/images/logo.png",
        "background": "/data/images/bg_pattern.png"
      }
    }
  }
}
```

### 4.2 Sistema di Riferimenti ($variables)

I temi supportano **variabili** per riuso valori:

```json
{
  "colors": {
    "primary": "#2196F3",
    "background": "#121212"
  },
  "components": {
    "button": {
      "bg_color": "$primary",        // Riusa colors.primary
      "hover": {
        "bg_color": "#1976D2"         // Override esplicito
      }
    }
  }
}
```

**Parser risolve automaticamente le reference con prefisso `$`**

---

## 5. Asset Manager

### 5.1 Classe AssetManager

```cpp
// core/ui/AssetManager.h
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <map>
#include <FS.h>

enum class AssetType {
  ICON,
  IMAGE,
  FONT,
  ANIMATION
};

struct Asset {
  String path;
  AssetType type;
  void* data;           // Puntatore a lv_img_dsc_t, lv_font_t, etc.
  size_t size;
  bool loaded;
  uint32_t last_accessed;  // Per LRU cache
};

class AssetManager {
public:
  static AssetManager& getInstance();

  bool begin();
  void end();

  // Icon loading
  const lv_img_dsc_t* getIcon(const String& iconName);
  bool loadIcon(const String& name, const String& path);
  bool unloadIcon(const String& name);

  // Image loading
  const lv_img_dsc_t* getImage(const String& imageName);
  bool loadImage(const String& name, const String& path);
  bool unloadImage(const String& name);

  // Font loading
  const lv_font_t* getFont(const String& fontName);
  bool loadFont(const String& name, const String& path);
  bool loadFontFromMemory(const String& name, const uint8_t* data, size_t size);
  bool unloadFont(const String& name);

  // Cache management
  void clearCache();
  void clearUnusedAssets(uint32_t maxAgeMs = 60000);
  size_t getCacheSize() const;
  size_t getCacheUsage() const;

  // Pre-loading
  bool preloadAssetPack(const String& packPath);

  // Asset info
  bool isLoaded(const String& name, AssetType type) const;
  std::vector<String> getLoadedAssets(AssetType type) const;

private:
  AssetManager() = default;
  ~AssetManager();

  std::map<String, Asset> icons_;
  std::map<String, Asset> images_;
  std::map<String, Asset> fonts_;

  size_t maxCacheSize_ = 512 * 1024;  // 512KB max cache
  bool initialized_ = false;

  // Helpers
  lv_img_dsc_t* loadImageFromFile(const String& path);
  lv_font_t* loadFontFromFile(const String& path);
  void freeAsset(Asset& asset);
};
```

### 5.2 Utilizzo AssetManager

```cpp
// Caricamento icone
AssetManager& assetMgr = AssetManager::getInstance();
assetMgr.begin();

assetMgr.loadIcon("wifi", "/data/icons/wifi.png");
assetMgr.loadIcon("bluetooth", "/data/icons/bluetooth.png");

// Uso in widget
lv_obj_t* icon = lv_img_create(parent);
lv_img_set_src(icon, assetMgr.getIcon("wifi"));

// Pre-caricamento pack
assetMgr.preloadAssetPack("/data/asset_packs/dashboard.json");

// Cleanup automatico LRU
assetMgr.clearUnusedAssets(60000);  // Rimuove asset non usati da 60s
```

---

## 6. Style Manager

### 6.1 Classe StyleManager

```cpp
// core/ui/StyleManager.h
#pragma once

#include <lvgl.h>
#include "ThemeManager.h"

class StyleManager {
public:
  static StyleManager& getInstance();

  // Applica stili a oggetti LVGL
  void applyButtonPrimary(lv_obj_t* obj);
  void applyButtonSecondary(lv_obj_t* obj);
  void applyCard(lv_obj_t* obj);
  void applyInput(lv_obj_t* obj);
  void applyNavbar(lv_obj_t* obj);
  void applyStatusbar(lv_obj_t* obj);
  void applyListItem(lv_obj_t* obj);

  // Applica stile generico per nome
  bool applyStyle(lv_obj_t* obj, const String& styleName);

  // Transizioni animate
  void transitionStyle(lv_obj_t* obj, const lv_style_t* newStyle, uint16_t duration);

  // Utility
  void setGlobalTextColor(lv_color_t color);
  void setGlobalBackgroundColor(lv_color_t color);

private:
  StyleManager() = default;
  ThemeManager& themeMgr_ = ThemeManager::getInstance();
};
```

### 6.2 Helper Macro per Stili

```cpp
// core/ui/StyleHelpers.h
#pragma once

#define APPLY_THEME_COLOR(obj, property, colorName) \
  lv_obj_set_style_##property(obj, ThemeManager::getInstance().getColor(colorName), 0)

#define APPLY_THEME_FONT(obj, fontName) \
  lv_obj_set_style_text_font(obj, ThemeManager::getInstance().getFont(fontName), 0)

#define CREATE_STYLED_BUTTON(parent, text, isPrimary) \
  ({ \
    lv_obj_t* btn = lv_btn_create(parent); \
    lv_obj_t* label = lv_label_create(btn); \
    lv_label_set_text(label, text); \
    lv_obj_center(label); \
    if (isPrimary) { \
      StyleManager::getInstance().applyButtonPrimary(btn); \
    } else { \
      StyleManager::getInstance().applyButtonSecondary(btn); \
    } \
    btn; \
  })

// Uso
lv_obj_t* btn = CREATE_STYLED_BUTTON(parent, "Conferma", true);
APPLY_THEME_COLOR(label, text_color, "text_primary");
APPLY_THEME_FONT(label, "title");
```

---

## 7. Esempio Temi Predefiniti

### 7.1 Tema Light (light.json)

```json
{
  "theme": {
    "metadata": {
      "name": "Light Clean",
      "version": "1.0.0",
      "author": "ESP32 Team",
      "description": "Tema chiaro minimalista"
    },
    "colors": {
      "primary": "#1976D2",
      "secondary": "#FF5722",
      "accent": "#FFC107",
      "background": "#FAFAFA",
      "surface": "#FFFFFF",
      "surface_variant": "#F5F5F5",
      "text_primary": "#212121",
      "text_secondary": "#757575",
      "text_disabled": "#BDBDBD",
      "error": "#D32F2F",
      "success": "#388E3C",
      "warning": "#F57C00",
      "info": "#1976D2",
      "divider": "#E0E0E0",
      "overlay": "#00000040"
    },
    "typography": {
      "font_small": {"name": "Roboto_12", "size": 12, "file": "/data/fonts/roboto_12.bin"},
      "font_normal": {"name": "Roboto_16", "size": 16, "file": "/data/fonts/roboto_16.bin"},
      "font_large": {"name": "Roboto_20", "size": 20, "file": "/data/fonts/roboto_20.bin"},
      "font_title": {"name": "Roboto_Bold_24", "size": 24, "file": "/data/fonts/roboto_bold_24.bin"}
    },
    "dimensions": {
      "padding_small": 4,
      "padding_medium": 8,
      "padding_large": 16,
      "border_radius_medium": 8,
      "border_width": 2,
      "elevation_1": 2,
      "elevation_2": 4
    },
    "components": {
      "button": {
        "primary": {
          "bg_color": "$primary",
          "text_color": "#FFFFFF",
          "border_radius": "$border_radius_medium",
          "shadow_width": "$elevation_2"
        }
      },
      "card": {
        "bg_color": "$surface",
        "border_radius": 12,
        "shadow_width": "$elevation_1"
      }
    },
    "animations": {
      "duration_fast": 150,
      "duration_normal": 300,
      "easing": "ease_in_out"
    }
  }
}
```

### 7.2 Tema Dark (dark.json)

Vedi sezione 4.1 per esempio completo.

### 7.3 Tema High Contrast (high_contrast.json)

```json
{
  "theme": {
    "metadata": {
      "name": "High Contrast",
      "description": "Tema ad alto contrasto per accessibilità"
    },
    "colors": {
      "primary": "#FFFF00",
      "background": "#000000",
      "surface": "#1A1A1A",
      "text_primary": "#FFFFFF",
      "text_secondary": "#FFFF00",
      "error": "#FF0000",
      "success": "#00FF00"
    },
    "components": {
      "button": {
        "primary": {
          "bg_color": "$primary",
          "text_color": "#000000",
          "border_color": "#FFFFFF",
          "border_width": 3
        }
      }
    }
  }
}
```

---

## 8. Pacchetti Grafici (Asset Packs)

### 8.1 Struttura Asset Pack JSON

```json
{
  "asset_pack": {
    "metadata": {
      "name": "Dashboard Icons",
      "version": "1.0.0",
      "author": "Design Team",
      "description": "Icone per dashboard principale"
    },
    "icons": [
      {
        "name": "wifi_full",
        "path": "/data/icons/wifi_full.png",
        "size": "24x24",
        "preload": true
      },
      {
        "name": "wifi_low",
        "path": "/data/icons/wifi_low.png",
        "size": "24x24",
        "preload": true
      },
      {
        "name": "battery_full",
        "path": "/data/icons/battery_full.png",
        "size": "24x24",
        "preload": true
      },
      {
        "name": "battery_charging",
        "path": "/data/icons/battery_charging.png",
        "size": "24x24",
        "preload": false
      }
    ],
    "images": [
      {
        "name": "splash_screen",
        "path": "/data/images/splash.png",
        "size": "320x240",
        "preload": true
      }
    ],
    "fonts": [
      {
        "name": "icons_font",
        "path": "/data/fonts/material_icons_24.bin",
        "size": 24,
        "preload": true
      }
    ]
  }
}
```

### 8.2 AssetPackManager

```cpp
// core/ui/AssetPackManager.h
#pragma once

#include "AssetManager.h"
#include <ArduinoJson.h>

class AssetPackManager {
public:
  static AssetPackManager& getInstance();

  // Carica pack da file JSON
  bool loadPack(const String& packPath);

  // Scarica pack
  bool unloadPack(const String& packName);

  // Lista pack caricati
  std::vector<String> getLoadedPacks() const;

private:
  AssetManager& assetMgr_ = AssetManager::getInstance();
  std::vector<String> loadedPacks_;
};
```

---

## 9. Integrazione con LVGL

### 9.1 Inizializzazione UI System

```cpp
// In setup() - main.cpp

void setupUISystem() {
  // 1. Inizializza LVGL
  lv_init();

  // 2. Inizializza display e touch driver
  lv_disp_t* disp = setupDisplay();
  lv_indev_t* touch = setupTouch();

  // 3. Inizializza Asset Manager
  AssetManager& assetMgr = AssetManager::getInstance();
  if (!assetMgr.begin()) {
    Serial.println("ERRORE: AssetManager init failed");
    return;
  }

  // 4. Carica asset pack base
  AssetPackManager& packMgr = AssetPackManager::getInstance();
  packMgr.loadPack("/data/asset_packs/base.json");
  packMgr.loadPack("/data/asset_packs/dashboard.json");

  // 5. Inizializza Theme Manager
  ThemeManager& themeMgr = ThemeManager::getInstance();
  if (!themeMgr.begin()) {
    Serial.println("ERRORE: ThemeManager init failed");
    return;
  }

  // 6. Carica tema da Settings (o default)
  String themeName = SettingsManager::getInstance().getString("ui.theme", "dark");
  String themePath = "/data/themes/" + themeName + ".json";

  if (!themeMgr.loadTheme(themePath)) {
    Serial.println("WARN: Tema preferito non trovato, carico dark.json");
    themeMgr.loadTheme("/data/themes/dark.json");
  }

  // 7. Applica tema
  themeMgr.applyTheme();

  // 8. Setup callback cambio tema
  themeMgr.setThemeChangeCallback([](const String& newTheme) {
    Serial.printf("Tema cambiato: %s\n", newTheme.c_str());

    // Notifica tutti i widget
    EventRouter::getInstance().publish("ui.theme.changed", newTheme);

    // Salva preferenza
    SettingsManager::getInstance().setString("ui.theme", newTheme);
  });

  Serial.println("UI System inizializzato con successo");
}
```

### 9.2 Applicazione Stili in Widget

```cpp
// widgets/WiFiStatusWidget.cpp

void WiFiStatusWidget::onCreate() {
  ThemeManager& theme = ThemeManager::getInstance();
  StyleManager& style = StyleManager::getInstance();

  // Container principale con stile card
  container_ = lv_obj_create(parent_);
  style.applyCard(container_);
  lv_obj_set_size(container_, 150, 80);

  // Icona WiFi
  iconWiFi_ = lv_img_create(container_);
  lv_img_set_src(iconWiFi_, AssetManager::getInstance().getIcon("wifi_full"));
  lv_obj_align(iconWiFi_, LV_ALIGN_TOP_LEFT, 10, 10);

  // Label SSID con font e colore dal tema
  labelSSID_ = lv_label_create(container_);
  lv_label_set_text(labelSSID_, "Not Connected");
  lv_obj_set_style_text_font(labelSSID_, theme.getFont("normal"), 0);
  lv_obj_set_style_text_color(labelSSID_, theme.getColor("text_primary"), 0);
  lv_obj_align(labelSSID_, LV_ALIGN_TOP_LEFT, 45, 10);

  // Label IP con font small
  labelIP_ = lv_label_create(container_);
  lv_label_set_text(labelIP_, "0.0.0.0");
  lv_obj_set_style_text_font(labelIP_, theme.getFont("small"), 0);
  lv_obj_set_style_text_color(labelIP_, theme.getColor("text_secondary"), 0);
  lv_obj_align(labelIP_, LV_ALIGN_TOP_LEFT, 45, 35);

  // Signal strength bar
  barSignal_ = lv_bar_create(container_);
  lv_obj_set_size(barSignal_, 120, 8);
  lv_obj_set_style_bg_color(barSignal_, theme.getColor("surface_variant"), 0);
  lv_obj_set_style_bg_color(barSignal_, theme.getColor("primary"), LV_PART_INDICATOR);
  lv_obj_align(barSignal_, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_bar_set_value(barSignal_, 75, LV_ANIM_OFF);
}

void WiFiStatusWidget::onThemeChanged(const String& themeName) {
  // Riapplica colori dal nuovo tema
  ThemeManager& theme = ThemeManager::getInstance();

  lv_obj_set_style_text_color(labelSSID_, theme.getColor("text_primary"), 0);
  lv_obj_set_style_text_color(labelIP_, theme.getColor("text_secondary"), 0);
  lv_obj_set_style_bg_color(barSignal_, theme.getColor("primary"), LV_PART_INDICATOR);

  // Ricarica icona se il tema ha asset diversi
  String iconPath = theme.getCurrentTheme().assets.icons["wifi"];
  if (!iconPath.isEmpty()) {
    lv_img_set_src(iconWiFi_, AssetManager::getInstance().getIcon("wifi_full"));
  }
}
```

---

## 10. Settings UI per Cambio Tema

### 10.1 Settings Screen con Theme Selector

```cpp
// apps/settings/SettingsScreen.cpp

void SettingsScreen::createThemeSelector() {
  // Sezione Tema
  lv_obj_t* sectionTheme = createSection(container_, "Tema");

  // Dropdown temi
  lv_obj_t* dropdown = lv_dropdown_create(sectionTheme);

  // Popola con temi disponibili
  ThemeManager& themeMgr = ThemeManager::getInstance();
  std::vector<String> themes = themeMgr.getAvailableThemes();

  String options = "";
  for (size_t i = 0; i < themes.size(); i++) {
    options += themes[i];
    if (i < themes.size() - 1) options += "\n";
  }

  lv_dropdown_set_options(dropdown, options.c_str());

  // Seleziona tema attivo
  String activeTheme = themeMgr.getActiveThemeName();
  for (size_t i = 0; i < themes.size(); i++) {
    if (themes[i] == activeTheme) {
      lv_dropdown_set_selected(dropdown, i);
      break;
    }
  }

  // Callback selezione
  lv_obj_add_event_cb(dropdown, [](lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    uint16_t idx = lv_dropdown_get_selected(dd);

    char buf[64];
    lv_dropdown_get_selected_str(dd, buf, sizeof(buf));

    String themeName = String(buf);
    String themePath = "/data/themes/" + themeName + ".json";

    ThemeManager& themeMgr = ThemeManager::getInstance();

    // Mostra loading
    lv_obj_t* spinner = lv_spinner_create(lv_scr_act(), 1000, 60);
    lv_obj_center(spinner);

    // Carica nuovo tema (in task separato per non bloccare UI)
    xTaskCreate([](void* param) {
      String* path = (String*)param;
      ThemeManager& mgr = ThemeManager::getInstance();

      if (mgr.loadTheme(*path)) {
        mgr.applyTheme();
        Serial.println("Tema applicato con successo");
      } else {
        Serial.println("ERRORE: Caricamento tema fallito");
      }

      delete path;
      vTaskDelete(NULL);
    }, "LoadTheme", 4096, new String(themePath), 5, NULL);

  }, LV_EVENT_VALUE_CHANGED, NULL);

  // Preview tema
  createThemePreview(sectionTheme);
}

void SettingsScreen::createThemePreview(lv_obj_t* parent) {
  lv_obj_t* preview = lv_obj_create(parent);
  lv_obj_set_size(preview, 200, 100);
  StyleManager::getInstance().applyCard(preview);

  ThemeManager& theme = ThemeManager::getInstance();

  // Mostra campioni colore
  const char* colorNames[] = {"primary", "secondary", "accent", "background"};
  for (int i = 0; i < 4; i++) {
    lv_obj_t* colorSample = lv_obj_create(preview);
    lv_obj_set_size(colorSample, 40, 40);
    lv_obj_set_style_bg_color(colorSample, theme.getColor(colorNames[i]), 0);
    lv_obj_set_pos(colorSample, 10 + i * 45, 10);
  }

  // Testo esempio
  lv_obj_t* label = lv_label_create(preview);
  lv_label_set_text(label, "Esempio Testo");
  lv_obj_set_style_text_font(label, theme.getFont("normal"), 0);
  lv_obj_set_style_text_color(label, theme.getColor("text_primary"), 0);
  lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -10);
}
```

---

## 11. File System Layout

### 11.1 Struttura Directory SPIFFS/LittleFS

```
/data/
├── themes/
│   ├── light.json              (Tema chiaro - 8KB)
│   ├── dark.json               (Tema scuro - 8KB)
│   ├── high_contrast.json      (Alto contrasto - 6KB)
│   └── custom/                 (Temi utente)
│       └── my_theme.json
│
├── asset_packs/
│   ├── base.json               (Asset pack base - 2KB)
│   ├── dashboard.json          (Asset pack dashboard - 3KB)
│   └── settings.json           (Asset pack settings - 2KB)
│
├── icons/
│   ├── wifi_full.png           (24x24 - 1KB)
│   ├── wifi_low.png            (24x24 - 1KB)
│   ├── bluetooth.png           (24x24 - 1KB)
│   ├── battery_full.png        (24x24 - 1KB)
│   ├── battery_low.png         (24x24 - 1KB)
│   ├── settings.png            (24x24 - 1KB)
│   ├── home.png                (24x24 - 1KB)
│   └── back.png                (24x24 - 1KB)
│
├── fonts/
│   ├── roboto_12.bin           (LVGL binary font - 15KB)
│   ├── roboto_16.bin           (LVGL binary font - 20KB)
│   ├── roboto_20.bin           (LVGL binary font - 25KB)
│   ├── roboto_bold_24.bin      (LVGL binary font - 30KB)
│   └── material_icons_24.bin   (Icon font - 50KB)
│
└── images/
    ├── splash.png              (320x240 - 30KB)
    ├── logo.png                (100x100 - 5KB)
    └── bg_pattern.png          (Texture - 10KB)
```

**Totale stimato:** ~250KB per setup completo

### 11.2 Partizionamento Flash

Per supportare il file system, modificare `platformio.ini`:

```ini
[env:freenove-esp32-s3-display]
board_build.partitions = custom_partitions.csv
```

**File `custom_partitions.csv`:**
```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000   # 20KB NVS
otadata,  data, ota,     0xe000,  0x2000   # 8KB OTA data
app0,     app,  ota_0,   0x10000, 0x300000 # 3MB App partition
app1,     app,  ota_1,   0x310000,0x300000 # 3MB OTA partition
spiffs,   data, spiffs,  0x610000,0x9F0000 # ~10MB SPIFFS
```

---

## 12. Ottimizzazioni e Best Practices

### 12.1 Performance

**1. Lazy Loading Asset**
```cpp
// Carica asset solo quando necessario
const lv_img_dsc_t* WiFiWidget::getWiFiIcon(int signalStrength) {
  static const lv_img_dsc_t* cachedIcon = nullptr;

  String iconName;
  if (signalStrength > 75) iconName = "wifi_full";
  else if (signalStrength > 50) iconName = "wifi_medium";
  else iconName = "wifi_low";

  const lv_img_dsc_t* icon = AssetManager::getInstance().getIcon(iconName);

  if (icon != cachedIcon) {
    cachedIcon = icon;
  }

  return cachedIcon;
}
```

**2. Preload Asset Critici**
```cpp
// In onCreate() delle screen critiche
void DashboardScreen::onCreate() {
  // Preload asset che verranno sicuramente usati
  AssetManager& mgr = AssetManager::getInstance();
  mgr.getIcon("wifi_full");    // Trigger lazy load
  mgr.getIcon("battery_full");
  mgr.getIcon("bluetooth");

  // Poi crea UI
  createWidgets();
}
```

**3. Cache LRU Automatica**
```cpp
// Cleanup periodico in loop()
void loop() {
  static unsigned long lastCleanup = 0;

  if (millis() - lastCleanup > 60000) {  // Ogni 60 secondi
    AssetManager::getInstance().clearUnusedAssets(30000);  // Rimuovi non usati da 30s
    lastCleanup = millis();
  }

  lv_timer_handler();
  delay(5);
}
```

### 12.2 Memory Management

**1. Usa PSRAM per Asset Grandi**
```cpp
// In AssetManager::loadImageFromFile()
lv_img_dsc_t* AssetManager::loadImageFromFile(const String& path) {
  // Alloca in PSRAM
  lv_img_dsc_t* img = (lv_img_dsc_t*)ps_malloc(sizeof(lv_img_dsc_t));

  // Carica da file...

  return img;
}
```

**2. Libera Risorse Non Utilizzate**
```cpp
void ScreenManager::changeScreen(Screen* newScreen) {
  if (currentScreen_) {
    currentScreen_->onDestroy();

    // Libera asset della vecchia screen
    AssetManager::getInstance().clearUnusedAssets(0);
  }

  currentScreen_ = newScreen;
  currentScreen_->onCreate();
}
```

### 12.3 Theme Authoring Guidelines

**DO:**
- ✅ Usa variabili `$` per colori riutilizzati
- ✅ Mantieni nomi colori semantici (`primary`, `error`) non descrittivi (`blue`, `red`)
- ✅ Testa tema su device reale, non solo emulatore
- ✅ Verifica contrasto testo (min 4.5:1 per accessibilità)
- ✅ Usa font con supporto caratteri speciali

**DON'T:**
- ❌ Non hardcodare colori hex in più punti
- ❌ Non creare temi con file >50KB
- ❌ Non usare font >100KB (troppo pesanti per ESP32)
- ❌ Non dimenticare stati hover/pressed/disabled
- ❌ Non mescolare unità (px, %, em) - usa solo px

---

## 13. Roadmap Implementazione UI System

### Fase 1: Theme Manager Base (2-3 giorni)
- [ ] Implementare classe `ThemeManager`
- [ ] Parser JSON con ArduinoJson
- [ ] Sistema risoluzione variabili `$`
- [ ] Caricamento da SPIFFS
- [ ] Test con tema dark.json base

### Fase 2: Asset Manager (2 giorni)
- [ ] Implementare classe `AssetManager`
- [ ] Caricamento icone PNG
- [ ] Caricamento font LVGL binary
- [ ] LRU cache automatica
- [ ] Test memory usage

### Fase 3: Style Manager (1-2 giorni)
- [ ] Implementare classe `StyleManager`
- [ ] Builder per component styles
- [ ] Macro helper per applicazione rapida
- [ ] Test con widget reali

### Fase 4: Temi Predefiniti (1 giorno)
- [ ] Creare light.json completo
- [ ] Creare dark.json completo
- [ ] Creare high_contrast.json
- [ ] Validazione JSON schema

### Fase 5: Asset Packs (1 giorno)
- [ ] Implementare `AssetPackManager`
- [ ] Creare base.json pack
- [ ] Creare dashboard.json pack
- [ ] Pre-loading automatico

### Fase 6: Integrazione Settings (1 giorno)
- [ ] Theme selector dropdown
- [ ] Theme preview
- [ ] Salvataggio preferenza NVS
- [ ] Hot reload funzionante

### Fase 7: Testing & Docs (1-2 giorni)
- [ ] Test cambio tema runtime
- [ ] Memory leak testing
- [ ] Performance profiling
- [ ] Documentazione API

**Totale:** 9-12 giorni lavorativi

---

## 14. Esempi di Codice Completi

### 14.1 Widget con Supporto Temi

```cpp
// widgets/StatusWidget.h
#pragma once

#include "Widget.h"
#include "core/ui/ThemeManager.h"
#include "core/ui/StyleManager.h"
#include "core/ui/AssetManager.h"

class StatusWidget : public Widget {
public:
  StatusWidget(lv_obj_t* parent);
  ~StatusWidget();

  void onCreate() override;
  void onDestroy() override;
  void onUpdate() override;

  void setStatus(const String& status, bool isError = false);

private:
  lv_obj_t* container_;
  lv_obj_t* iconStatus_;
  lv_obj_t* labelStatus_;

  void onThemeChanged(const String& themeName);

  String eventSubscriptionId_;
};

// widgets/StatusWidget.cpp
#include "StatusWidget.h"
#include "core/EventRouter.h"

StatusWidget::StatusWidget(lv_obj_t* parent) : Widget(parent) {}

StatusWidget::~StatusWidget() {
  // Unsubscribe da eventi
  EventRouter::getInstance().unsubscribe("ui.theme.changed", eventSubscriptionId_);
}

void StatusWidget::onCreate() {
  ThemeManager& theme = ThemeManager::getInstance();
  StyleManager& style = StyleManager::getInstance();
  AssetManager& assets = AssetManager::getInstance();

  // Container
  container_ = lv_obj_create(parent_);
  style.applyCard(container_);
  lv_obj_set_size(container_, 200, 60);

  // Icona
  iconStatus_ = lv_img_create(container_);
  lv_img_set_src(iconStatus_, assets.getIcon("info"));
  lv_obj_align(iconStatus_, LV_ALIGN_LEFT_MID, 15, 0);

  // Label
  labelStatus_ = lv_label_create(container_);
  lv_label_set_text(labelStatus_, "Ready");
  lv_obj_set_style_text_font(labelStatus_, theme.getFont("normal"), 0);
  lv_obj_set_style_text_color(labelStatus_, theme.getColor("text_primary"), 0);
  lv_obj_align(labelStatus_, LV_ALIGN_LEFT_MID, 55, 0);

  // Subscribe a cambio tema
  eventSubscriptionId_ = EventRouter::getInstance().subscribe(
    "ui.theme.changed",
    [this](const String& data) {
      onThemeChanged(data);
    }
  );
}

void StatusWidget::onDestroy() {
  lv_obj_del(container_);
  container_ = nullptr;
}

void StatusWidget::onUpdate() {
  // Update logic...
}

void StatusWidget::setStatus(const String& status, bool isError) {
  lv_label_set_text(labelStatus_, status.c_str());

  ThemeManager& theme = ThemeManager::getInstance();
  AssetManager& assets = AssetManager::getInstance();

  if (isError) {
    lv_obj_set_style_text_color(labelStatus_, theme.getColor("error"), 0);
    lv_img_set_src(iconStatus_, assets.getIcon("error"));
  } else {
    lv_obj_set_style_text_color(labelStatus_, theme.getColor("success"), 0);
    lv_img_set_src(iconStatus_, assets.getIcon("success"));
  }
}

void StatusWidget::onThemeChanged(const String& themeName) {
  ThemeManager& theme = ThemeManager::getInstance();

  // Riapplica colori
  lv_obj_set_style_text_color(labelStatus_, theme.getColor("text_primary"), 0);

  // Riapplica stili
  StyleManager::getInstance().applyCard(container_);
}
```

### 14.2 Creazione Temi Custom Dinamicamente

```cpp
// tools/ThemeBuilder.h
#pragma once

#include "core/ui/ThemeManager.h"
#include <ArduinoJson.h>

class ThemeBuilder {
public:
  ThemeBuilder& setName(const String& name);
  ThemeBuilder& setAuthor(const String& author);
  ThemeBuilder& setPrimaryColor(const String& hex);
  ThemeBuilder& setBackgroundColor(const String& hex);
  ThemeBuilder& setTextColor(const String& hex);

  bool saveToFile(const String& path);
  String toJSON();

private:
  DynamicJsonDocument doc_{4096};
};

// Uso
ThemeBuilder builder;
builder.setName("My Custom Theme")
       .setAuthor("User")
       .setPrimaryColor("#FF6B6B")
       .setBackgroundColor("#1A1A2E")
       .setTextColor("#EAEAEA")
       .saveToFile("/data/themes/custom.json");
```

---

## 15. Conclusioni

### Vantaggi dell'Architettura

✅ **Separazione Design/Codice** - Designer possono lavorare su temi senza conoscere C++
✅ **Hot Reload** - Cambio tema istantaneo senza ricompilazione
✅ **Estensibilità** - Facile aggiungere nuovi temi e asset pack
✅ **Manutenibilità** - Modifiche UI centralizzate in JSON
✅ **Performance** - LRU cache e lazy loading ottimizzano memoria
✅ **Accessibilità** - Supporto temi alto contrasto
✅ **User Experience** - Personalizzazione tema da Settings

### Prossimi Passi

1. **Implementare Fase 1-3** del roadmap (Theme + Asset + Style Manager)
2. **Creare temi light e dark** completi
3. **Integrare in Dashboard** esistente
4. **Testing su hardware reale** con profiling memoria
5. **Documentare API** per sviluppatori terzi

### Risorse Utili

- [LVGL Styling](https://docs.lvgl.io/8.4/overview/style.html)
- [ArduinoJson Assistant](https://arduinojson.org/v6/assistant/)
- [Material Design Color System](https://material.io/design/color/the-color-system.html)
- [WCAG Contrast Checker](https://webaim.org/resources/contrastchecker/)

---

**Documento redatto il:** 2025-11-18
**Versione:** 1.0.0
**Status:** 📋 PIANIFICATO - Da implementare dopo Core Managers (Fase 5-6 roadmap generale)
