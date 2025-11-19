#include "screens/theme_settings_screen.h"
#include "widgets/circular_color_picker.h"
#include <Arduino.h>

namespace {
struct PalettePreset {
    const char* name;
    uint32_t primary;
    uint32_t accent;
};

constexpr PalettePreset PRESETS[] = {
    {"Aurora", 0x0b2035, 0x5df4ff},
    {"Sunset", 0x2b1f3a, 0xff7f50},
    {"Forest", 0x0f2d1c, 0x7ed957},
    {"Mono", 0x1a1a1a, 0xffffff}
};

lv_color_t toLvColor(uint32_t hex) {
    return lv_color_hex(hex);
}

uint32_t toHex(lv_color_t color) {
    // Use LVGL's native conversion function which handles RGB565->RGB888 properly
    uint32_t color32 = lv_color_to32(color);

    // lv_color32_t.full is in BGRA format (Blue at LSB, Alpha at MSB)
    // Extract: B = bits 0-7, G = bits 8-15, R = bits 16-23, A = bits 24-31
    uint32_t b = (color32 >> 0) & 0xFF;
    uint32_t g = (color32 >> 8) & 0xFF;
    uint32_t r = (color32 >> 16) & 0xFF;

    // Return in RGB format (R at MSB)
    return (r << 16) | (g << 8) | b;
}

lv_obj_t* create_card(lv_obj_t* parent, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_style_bg_color(card, lv_color_hex(0x10182c), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_outline_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 8, 0);

    lv_obj_t* header = lv_label_create(card);
    lv_label_set_text(header, title);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xf0f0f0), 0);

    return card;
}
} // namespace

ThemeSettingsScreen::~ThemeSettingsScreen() {
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void ThemeSettingsScreen::build(lv_obj_t* parent) {
    if (!parent) {
        return;
    }

    SettingsManager& manager = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = manager.getSnapshot();

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x040b18), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_set_style_pad_row(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_outline_width(root, 0, 0);

    lv_obj_t* title = lv_label_create(root);
    lv_label_set_text(title, "🎨 Theme Studio");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_width(title, lv_pct(100));

    lv_obj_t* content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Orientation
    lv_obj_t* orientation_card = create_card(content, "Orientamento UI");
    lv_obj_set_height(orientation_card, LV_SIZE_CONTENT);
    orientation_switch = lv_switch_create(orientation_card);
    lv_obj_add_event_cb(orientation_switch, handleOrientation, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_t* orient_hint = lv_label_create(orientation_card);
    lv_label_set_text(orient_hint, "Landscape / Portrait");
    lv_obj_set_style_text_color(orient_hint, lv_color_hex(0xa0a0a0), 0);

    // Border radius
    lv_obj_t* border_card = create_card(content, "Raggio Bordi");
    lv_obj_set_height(border_card, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(border_card, 8, 0);
    lv_obj_set_style_pad_row(border_card, 4, 0);
    border_slider = lv_slider_create(border_card);
    lv_slider_set_range(border_slider, 0, 30);
    lv_obj_set_width(border_slider, lv_pct(100));
    lv_obj_set_height(border_slider, 16);
    lv_obj_add_event_cb(border_slider, handleBorderRadius, LV_EVENT_VALUE_CHANGED, this);


    // Combined color customization container (wheels + quick palettes) - IN FONDO
    lv_obj_t* color_palette_card = create_card(content, "Colori Rapidi & Custom");
    lv_obj_set_size(color_palette_card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(color_palette_card, 12, 0);

    // Grid container: dimensionamento automatico intelligente
    lv_obj_t* color_content = lv_obj_create(color_palette_card);
    lv_obj_remove_style_all(color_content);
    lv_obj_set_size(color_content, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(color_content, LV_LAYOUT_GRID);

    // Griglia 2x2 per 4 ruote colore
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_SIZE_CONTENT, 12, LV_SIZE_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(color_content, col_dsc, row_dsc);

    lv_obj_set_style_pad_row(color_content, 0, 0);
    lv_obj_set_style_pad_column(color_content, 16, 0);
    lv_obj_set_style_grid_row_align(color_content, LV_GRID_ALIGN_START, 0);
    lv_obj_set_style_grid_cell_column_pos(color_content, 0, 0);
    lv_obj_set_style_grid_cell_row_pos(color_content, 0, 0);

    // Lambda per creare sezioni color picker 2D con grid positioning
    int grid_col = 0;
    int grid_row = 0;
    auto makePickerSection = [&grid_col, &grid_row](lv_obj_t* parent, const char* title,
                                        lv_obj_t** target,
                                        lv_event_cb_t handler,
                                        void* user_data) {
        // Contenitore sezione auto-centrato nella cella grid
        lv_obj_t* section = lv_obj_create(parent);
        lv_obj_remove_style_all(section);
        lv_obj_set_size(section, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_layout(section, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(section, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(section, 8, 0);

        // Posizionamento grid automatico
        lv_obj_set_grid_cell(section, LV_GRID_ALIGN_CENTER, grid_col, 1,
                                      LV_GRID_ALIGN_START, grid_row, 1);

        lv_obj_t* lbl = lv_label_create(section);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xf0f0f0), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

        *target = CircularColorPicker::create(section, 110, 70);
        lv_obj_add_event_cb(*target, handler, LV_EVENT_VALUE_CHANGED, user_data);

        grid_col++;
        if (grid_col >= 2) {
            grid_col = 0;
            grid_row += 2;  // Skip the spacing row
        }
    };

    // Color pickers
    makePickerSection(color_content, "Primario", &primary_wheel, handlePrimaryColor, this);
    makePickerSection(color_content, "Accento", &accent_wheel, handleAccentColor, this);
    makePickerSection(color_content, "Card", &card_wheel, handleCardColor, this);
    makePickerSection(color_content, "Dock", &dock_wheel, handleDockColor, this);

    // Container palette rapide: occupa tutta la riga sotto le ruote
    lv_obj_t* palette_section = lv_obj_create(color_palette_card);
    lv_obj_remove_style_all(palette_section);
    lv_obj_set_size(palette_section, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(palette_section, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(palette_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(palette_section, 10, 0);
    lv_obj_set_style_pad_top(palette_section, 16, 0);

    lv_obj_t* palette_label = lv_label_create(palette_section);
    lv_label_set_text(palette_label, "Palette Rapide");
    lv_obj_set_style_text_color(palette_label, lv_color_hex(0x9fb0c8), 0);
    lv_obj_set_style_text_font(palette_label, &lv_font_montserrat_14, 0);

    lv_obj_t* palette_container = lv_obj_create(palette_section);
    lv_obj_remove_style_all(palette_container);
    lv_obj_set_size(palette_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(palette_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(palette_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(palette_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(palette_container, 8, 0);
    lv_obj_set_style_pad_column(palette_container, 8, 0);

    for (const auto& preset : PRESETS) {
        lv_obj_t* btn = lv_btn_create(palette_container);
        lv_obj_set_size(btn, 90, 36);
        lv_obj_set_style_bg_color(btn, toLvColor(preset.primary), 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_add_event_cb(btn, handlePaletteButton, LV_EVENT_CLICKED, (void*)&preset);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, preset.name);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
        lv_obj_center(lbl);
    }
    // Preview
    preview_card = lv_obj_create(content);
    lv_obj_remove_style_all(preview_card);
    lv_obj_set_width(preview_card, lv_pct(100));
    lv_obj_set_style_bg_color(preview_card, lv_color_hex(0x10182c), 0);
    lv_obj_set_style_bg_opa(preview_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(preview_card, 18, 0);
    lv_obj_set_style_pad_all(preview_card, 14, 0);
    lv_obj_clear_flag(preview_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(preview_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(preview_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(preview_card, 10, 0);

    preview_header = lv_label_create(preview_card);

    preview_body = lv_obj_create(preview_card);
    lv_obj_remove_style_all(preview_body);
    lv_obj_set_size(preview_body, lv_pct(100), 60);
    lv_obj_set_style_bg_color(preview_body, lv_color_hex(0x0f2030), 0);
    lv_obj_set_style_bg_opa(preview_body, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(preview_body, 8, 0);

    // Card color demo
    preview_card_demo = lv_obj_create(preview_card);
    lv_obj_remove_style_all(preview_card_demo);
    lv_obj_set_size(preview_card_demo, lv_pct(100), 40);
    lv_obj_set_style_bg_color(preview_card_demo, lv_color_hex(0x10182c), 0);
    lv_obj_set_style_bg_opa(preview_card_demo, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(preview_card_demo, 8, 0);
    lv_obj_set_style_pad_all(preview_card_demo, 6, 0);

    lv_obj_t* card_label = lv_label_create(preview_card_demo);
    lv_label_set_text(card_label, "Card");
    lv_obj_set_style_text_color(card_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(card_label, &lv_font_montserrat_14, 0);

    // Dock color demo
    preview_dock_demo = lv_obj_create(preview_card);
    lv_obj_remove_style_all(preview_dock_demo);
    lv_obj_set_size(preview_dock_demo, lv_pct(100), 40);
    lv_obj_set_style_bg_color(preview_dock_demo, lv_color_hex(0x1a2332), 0);
    lv_obj_set_style_bg_opa(preview_dock_demo, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(preview_dock_demo, 8, 0);
    lv_obj_set_style_pad_all(preview_dock_demo, 6, 0);

    lv_obj_t* dock_label = lv_label_create(preview_dock_demo);
    lv_label_set_text(dock_label, "Dock");
    lv_obj_set_style_text_color(dock_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(dock_label, &lv_font_montserrat_14, 0);

    if (settings_listener_id == 0) {
        settings_listener_id = manager.addListener([this](SettingsManager::SettingKey key, const SettingsSnapshot& snap) {
            if (!root || updating_from_manager) return;

            // Only update UI elements that don't trigger color changes (sliders, switches)
            // Color wheels are NEVER updated after initialization to avoid feedback loops
            if (key == SettingsManager::SettingKey::ThemeBorderRadius && border_slider) {
                updating_from_manager = true;
                lv_slider_set_value(border_slider, snap.borderRadius, LV_ANIM_OFF);
                updating_from_manager = false;
            } else if (key == SettingsManager::SettingKey::LayoutOrientation && orientation_switch) {
                updating_from_manager = true;
                if (snap.landscapeLayout) {
                    lv_obj_add_state(orientation_switch, LV_STATE_CHECKED);
                } else {
                    lv_obj_clear_state(orientation_switch, LV_STATE_CHECKED);
                }
                updating_from_manager = false;
            }

            // Always update preview (not the wheels)
            updatePreview(snap);
        });
    }

    // Apply snapshot AFTER all widgets are created (especially color wheels)
    applySnapshot(snapshot);
}

void ThemeSettingsScreen::onShow() {
    Serial.println("🎨 Theme settings opened");
    applySnapshot(SettingsManager::getInstance().getSnapshot());
}

void ThemeSettingsScreen::onHide() {
    Serial.println("🎨 Theme settings closed");
}

void ThemeSettingsScreen::applySnapshot(const SettingsSnapshot& snapshot) {
    updating_from_manager = true;

    Serial.printf("🔄 applySnapshot - Primary: 0x%06X, Accent: 0x%06X, Card: 0x%06X, Dock: 0x%06X\n",
                  snapshot.primaryColor, snapshot.accentColor, snapshot.cardColor, snapshot.dockColor);

    // Initialize color pickers ONCE with approximate position based on saved color
    // After this, pickers are NEVER updated - they control the color, not vice versa
    if (primary_wheel) {
        lv_color_t color = toLvColor(snapshot.primaryColor);
        lv_color_hsv_t hsv = lv_color_rgb_to_hsv(LV_COLOR_GET_R(color),
                                                   LV_COLOR_GET_G(color),
                                                   LV_COLOR_GET_B(color));
        current_primary_hsv = hsv;

        Serial.printf("🎨 Initializing primary picker to: 0x%06X (H:%d S:%d V:%d)\n",
                      snapshot.primaryColor, hsv.h, hsv.s, hsv.v);
        CircularColorPicker::set_hsv(primary_wheel, hsv);
    }

    if (accent_wheel) {
        Serial.printf("🎨 Initializing accent picker to: 0x%06X\n", snapshot.accentColor);
        CircularColorPicker::set_rgb(accent_wheel, toLvColor(snapshot.accentColor));
    }

    if (card_wheel) {
        Serial.printf("🎨 Initializing card picker to: 0x%06X\n", snapshot.cardColor);
        CircularColorPicker::set_rgb(card_wheel, toLvColor(snapshot.cardColor));
    }

    if (dock_wheel) {
        Serial.printf("🎨 Initializing dock picker to: 0x%06X\n", snapshot.dockColor);
        CircularColorPicker::set_rgb(dock_wheel, toLvColor(snapshot.dockColor));
    }

    if (border_slider) {
        lv_slider_set_value(border_slider, snapshot.borderRadius, LV_ANIM_OFF);
    }

    if (orientation_switch) {
        if (snapshot.landscapeLayout) {
            lv_obj_add_state(orientation_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(orientation_switch, LV_STATE_CHECKED);
        }
    }

    updatePreview(snapshot);
    updating_from_manager = false;
}

void ThemeSettingsScreen::updatePreview(const SettingsSnapshot& snapshot) {
    if (!preview_card) return;
    lv_color_t primary = toLvColor(snapshot.primaryColor);
    lv_color_t accent = toLvColor(snapshot.accentColor);
    lv_color_t card = toLvColor(snapshot.cardColor);
    lv_color_t dock = toLvColor(snapshot.dockColor);

    lv_obj_set_style_bg_color(preview_card, primary, 0);
    if (preview_header) {
        lv_label_set_text(preview_header,
                          snapshot.landscapeLayout ? "Layout: Landscape" : "Layout: Portrait");
        lv_obj_set_style_text_color(preview_header, accent, 0);
    }
    if (preview_body) {
        lv_obj_set_style_bg_color(preview_body, accent, 0);
        lv_obj_set_style_radius(preview_body, snapshot.borderRadius, 0);
    }
    if (preview_card_demo) {
        lv_obj_set_style_bg_color(preview_card_demo, card, 0);
        lv_obj_set_style_radius(preview_card_demo, snapshot.borderRadius, 0);
    }
    if (preview_dock_demo) {
        lv_obj_set_style_bg_color(preview_dock_demo, dock, 0);
        lv_obj_set_style_radius(preview_dock_demo, snapshot.borderRadius, 0);
    }
}

void ThemeSettingsScreen::handlePrimaryColor(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_color_t color = CircularColorPicker::get_rgb(screen->primary_wheel);
    lv_color_hsv_t hsv = CircularColorPicker::get_hsv(screen->primary_wheel);
    screen->current_primary_hsv = hsv;

    uint32_t hex = toHex(color);
    Serial.printf("🎨 Primary color: 0x%06X (H:%d S:%d V:%d)\n", hex, hsv.h, hsv.s, hsv.v);

    SettingsManager::getInstance().setPrimaryColor(hex);
}

void ThemeSettingsScreen::handleAccentColor(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_color_t color = CircularColorPicker::get_rgb(screen->accent_wheel);
    uint32_t hex = toHex(color);

    Serial.printf("🎨 Accent color: 0x%06X\n", hex);

    SettingsManager::getInstance().setAccentColor(hex);
}

void ThemeSettingsScreen::handleCardColor(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_color_t color = CircularColorPicker::get_rgb(screen->card_wheel);
    uint32_t hex = toHex(color);

    Serial.printf("🎨 Card color: 0x%06X\n", hex);

    SettingsManager::getInstance().setCardColor(hex);
}

void ThemeSettingsScreen::handleDockColor(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    // Read color from picker - this always returns a valid RGB based on cursor position
    lv_color_t color = CircularColorPicker::get_rgb(screen->dock_wheel);
    uint32_t hex = toHex(color);

    Serial.printf("🎨 Dock color: 0x%06X\n", hex);

    // Save to settings - this triggers listeners to update dock and preview
    SettingsManager::getInstance().setDockColor(hex);
}

void ThemeSettingsScreen::handleBorderRadius(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;
    int32_t value = lv_slider_get_value(screen->border_slider);
    SettingsManager::getInstance().setBorderRadius(static_cast<uint8_t>(value));
}

void ThemeSettingsScreen::handleOrientation(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;
    static uint32_t last_toggle_ms = 0;
    uint32_t now = millis();
    if (now - last_toggle_ms < 400) {
        return;
    }
    last_toggle_ms = now;
    bool checked = lv_obj_has_state(screen->orientation_switch, LV_STATE_CHECKED);
    SettingsManager::getInstance().setLandscapeLayout(checked);
}

void ThemeSettingsScreen::handlePaletteButton(lv_event_t* e) {
    auto* preset = static_cast<const PalettePreset*>(lv_event_get_user_data(e));
    if (!preset) return;

    SettingsManager& manager = SettingsManager::getInstance();
    manager.setPrimaryColor(preset->primary);
    manager.setAccentColor(preset->accent);

    // The settings listener will trigger applySnapshot() which updates the color pickers
}
