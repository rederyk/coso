#include "screens/theme_settings_screen.h"
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
    const uint32_t r = static_cast<uint32_t>(LV_COLOR_GET_R(color)) & 0xFF;
    const uint32_t g = static_cast<uint32_t>(LV_COLOR_GET_G(color)) & 0xFF;
    const uint32_t b = static_cast<uint32_t>(LV_COLOR_GET_B(color)) & 0xFF;
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

    // Griglia 2 colonne responsive per le ruote colore
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_SIZE_CONTENT, 8, LV_SIZE_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(color_content, col_dsc, row_dsc);

    lv_obj_set_style_pad_row(color_content, 0, 0);
    lv_obj_set_style_pad_column(color_content, 16, 0);
    lv_obj_set_style_grid_row_align(color_content, LV_GRID_ALIGN_START, 0);
    lv_obj_set_style_grid_cell_column_pos(color_content, 0, 0);
    lv_obj_set_style_grid_cell_row_pos(color_content, 0, 0);

    // Lambda per creare sezioni ruota colore con grid positioning
    int grid_col = 0;
    auto makeWheelSection = [&grid_col](lv_obj_t* parent, const char* title,
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
                                      LV_GRID_ALIGN_START, 0, 1);

        lv_obj_t* lbl = lv_label_create(section);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xf0f0f0), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

        *target = lv_colorwheel_create(section, true);
        lv_obj_set_size(*target, 110, 110);  // Dimensione fissa ottimale per wheel
        lv_obj_add_event_cb(*target, handler, LV_EVENT_VALUE_CHANGED, user_data);

        grid_col++;
    };

    // Ruote colore posizionate automaticamente in griglia
    makeWheelSection(color_content, "Primario", &primary_wheel, handlePrimaryColor, this);
    makeWheelSection(color_content, "Accento", &accent_wheel, handleAccentColor, this);

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

    applySnapshot(snapshot);

    if (settings_listener_id == 0) {
        settings_listener_id = manager.addListener([this](SettingsManager::SettingKey, const SettingsSnapshot& snap) {
            if (!root) return;
            applySnapshot(snap);
        });
    }
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

    if (primary_wheel) {
        lv_colorwheel_set_rgb(primary_wheel, toLvColor(snapshot.primaryColor));
    }
    if (accent_wheel) {
        lv_colorwheel_set_rgb(accent_wheel, toLvColor(snapshot.accentColor));
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
}

void ThemeSettingsScreen::handlePrimaryColor(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;
    lv_color_t color = lv_colorwheel_get_rgb(screen->primary_wheel);
    SettingsManager::getInstance().setPrimaryColor(toHex(color));
}

void ThemeSettingsScreen::handleAccentColor(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;
    lv_color_t color = lv_colorwheel_get_rgb(screen->accent_wheel);
    SettingsManager::getInstance().setAccentColor(toHex(color));
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
}
