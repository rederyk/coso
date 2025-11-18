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
    uint32_t full = lv_color_to32(color);
    return full & 0x00FFFFFF;
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

    // Color wheels
    lv_obj_t* primary_card = create_card(content, "Colore Primario");
    lv_obj_t* primary_holder = lv_obj_create(primary_card);
    lv_obj_remove_style_all(primary_holder);
    lv_obj_set_width(primary_holder, lv_pct(100));
    lv_obj_set_layout(primary_holder, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(primary_holder, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(primary_holder,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    primary_wheel = lv_colorwheel_create(primary_holder, true);
    lv_obj_set_size(primary_wheel, 140, 140);
    lv_obj_add_event_cb(primary_wheel, handlePrimaryColor, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* accent_card = create_card(content, "Colore Accento");
    lv_obj_t* accent_holder = lv_obj_create(accent_card);
    lv_obj_remove_style_all(accent_holder);
    lv_obj_set_width(accent_holder, lv_pct(100));
    lv_obj_set_layout(accent_holder, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(accent_holder, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(accent_holder,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    accent_wheel = lv_colorwheel_create(accent_holder, true);
    lv_obj_set_size(accent_wheel, 140, 140);
    lv_obj_add_event_cb(accent_wheel, handleAccentColor, LV_EVENT_VALUE_CHANGED, this);

    // Border radius
    lv_obj_t* border_card = create_card(content, "Raggio Bordi");
    border_slider = lv_slider_create(border_card);
    lv_slider_set_range(border_slider, 0, 30);
    lv_obj_set_width(border_slider, lv_pct(100));
    lv_obj_add_event_cb(border_slider, handleBorderRadius, LV_EVENT_VALUE_CHANGED, this);

    // Orientation
    lv_obj_t* orientation_card = create_card(content, "Orientamento UI");
    orientation_switch = lv_switch_create(orientation_card);
    lv_obj_add_event_cb(orientation_switch, handleOrientation, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_t* orient_hint = lv_label_create(orientation_card);
    lv_label_set_text(orient_hint, "Landscape / Portrait");
    lv_obj_set_style_text_color(orient_hint, lv_color_hex(0xa0a0a0), 0);

    // Palette presets
    lv_obj_t* palette_card = create_card(content, "Palette Rapide");
    lv_obj_set_width(palette_card, lv_pct(100));
    lv_obj_set_layout(palette_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(palette_card, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(palette_card, 6, 0);
    lv_obj_set_style_pad_column(palette_card, 6, 0);
    lv_obj_clear_flag(palette_card, LV_OBJ_FLAG_SCROLLABLE);

    for (const auto& preset : PRESETS) {
        lv_obj_t* btn = lv_btn_create(palette_card);
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
