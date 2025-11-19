#include "screens/theme_settings_screen.h"
#include "widgets/circular_color_picker.h"
#include <Arduino.h>
#include "ui/ui_symbols.h"
#include "utils/logger.h"

namespace {
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

constexpr uint8_t DOCK_ICON_RADIUS_MAX = 24;

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

    title_label = nullptr;
    orientation_card_container = nullptr;
    orientation_hint_label = nullptr;
    border_card_container = nullptr;
    dock_icon_card_container = nullptr;
    color_palette_card_container = nullptr;
    color_target_selector_container = nullptr;
    color_picker_container = nullptr;
    color_picker_label = nullptr;
    color_picker_widget = nullptr;
    palette_section_container = nullptr;
    palette_header_label = nullptr;
    quick_palette_container = nullptr;
    border_slider = nullptr;
    dock_icon_radius_slider = nullptr;
    color_target_buttons_.clear();
    current_target_ = ColorTarget::Primary;

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

    title_label = lv_label_create(root);
    lv_label_set_text(title_label, UI_SYMBOL_THEME " Theme Studio");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_width(title_label, lv_pct(100));

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
    orientation_card_container = create_card(content, "Orientamento UI");
    lv_obj_set_height(orientation_card_container, LV_SIZE_CONTENT);
    orientation_switch = lv_switch_create(orientation_card_container);
    lv_obj_add_event_cb(orientation_switch, handleOrientation, LV_EVENT_VALUE_CHANGED, this);
    orientation_hint_label = lv_label_create(orientation_card_container);
    lv_label_set_text(orientation_hint_label, "Landscape / Portrait");
    lv_obj_set_style_text_color(orientation_hint_label, lv_color_hex(0xa0a0a0), 0);

    // Border radius
    border_card_container = create_card(content, "Raggio Bordi");
    lv_obj_set_height(border_card_container, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(border_card_container, 8, 0);
    lv_obj_set_style_pad_row(border_card_container, 4, 0);
    border_slider = lv_slider_create(border_card_container);
    lv_slider_set_range(border_slider, 0, 30);
    lv_obj_set_width(border_slider, lv_pct(100));
    lv_obj_set_height(border_slider, 16);
    lv_obj_add_event_cb(border_slider, handleBorderRadius, LV_EVENT_VALUE_CHANGED, this);

    // Dock icon radius
    dock_icon_card_container = create_card(content, "Raggio Icone Dock");
    lv_obj_set_height(dock_icon_card_container, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(dock_icon_card_container, 8, 0);
    lv_obj_set_style_pad_row(dock_icon_card_container, 4, 0);
    dock_icon_radius_slider = lv_slider_create(dock_icon_card_container);
    lv_slider_set_range(dock_icon_radius_slider, 0, DOCK_ICON_RADIUS_MAX);
    lv_obj_set_width(dock_icon_radius_slider, lv_pct(100));
    lv_obj_set_height(dock_icon_radius_slider, 16);
    lv_obj_add_event_cb(dock_icon_radius_slider, handleDockIconRadius, LV_EVENT_VALUE_CHANGED, this);

    // Combined color customization container (selector + quick palettes)
    color_palette_card_container = create_card(content, "Colori Rapidi & Custom");
    lv_obj_set_size(color_palette_card_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(color_palette_card_container, 12, 0);

    color_target_selector_container = lv_obj_create(color_palette_card_container);
    lv_obj_remove_style_all(color_target_selector_container);
    lv_obj_set_size(color_target_selector_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(color_target_selector_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(color_target_selector_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(color_target_selector_container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(color_target_selector_container, 12, 0);

    color_picker_container = lv_obj_create(color_target_selector_container);
    lv_obj_remove_style_all(color_picker_container);
    lv_obj_set_width(color_picker_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(color_picker_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(color_picker_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(color_picker_container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(color_picker_container, 10, 0);

    color_picker_label = lv_label_create(color_picker_container);
    lv_obj_set_style_text_font(color_picker_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(color_picker_label, lv_color_hex(0xf0f0f0), 0);

    color_picker_widget = CircularColorPicker::create(color_picker_container, 160, 70);
    if (color_picker_widget) {
        lv_obj_add_event_cb(color_picker_widget, handleUnifiedColorPicker, LV_EVENT_VALUE_CHANGED, this);
    }

    lv_obj_t* targets_list = lv_obj_create(color_target_selector_container);
    lv_obj_remove_style_all(targets_list);
    lv_obj_set_width(targets_list, lv_pct(100));
    lv_obj_set_layout(targets_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(targets_list, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(targets_list,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(targets_list, 10, 0);
    lv_obj_set_style_pad_column(targets_list, 10, 0);

    struct ColorTargetDescriptor {
        ColorTarget target;
        const char* label;
    };
    static constexpr ColorTargetDescriptor descriptors[] = {
        {ColorTarget::Primary, "Primario"},
        {ColorTarget::Accent, "Accento"},
        {ColorTarget::Card, "Card"},
        {ColorTarget::Dock, "Dock"},
        {ColorTarget::DockIconBackground, "Sfondo Icone Dock"},
        {ColorTarget::DockIconSymbol, "Simbolo Icone Dock"},
    };

    color_target_buttons_.clear();
    for (const auto& desc : descriptors) {
        lv_obj_t* btn = lv_btn_create(targets_list);
        lv_obj_set_width(btn, 140);
        lv_obj_set_height(btn, 38);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_opa(btn, LV_OPA_100, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(btn, handleColorTargetButton, LV_EVENT_CLICKED, this);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, desc.label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
        lv_obj_center(lbl);

        lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);

        TargetButton target_button;
        target_button.button = btn;
        target_button.target = desc.target;
        color_target_buttons_.push_back(target_button);
    }

    updateTargetButtonColors(snapshot);
    setActiveColorTarget(current_target_, snapshot);

    // Container palette rapide: occupa tutta la riga sotto le ruote
    palette_section_container = lv_obj_create(color_palette_card_container);
    lv_obj_remove_style_all(palette_section_container);
    lv_obj_set_size(palette_section_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(palette_section_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(palette_section_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(palette_section_container, 10, 0);
    lv_obj_set_style_pad_top(palette_section_container, 16, 0);

    palette_header_label = lv_label_create(palette_section_container);
    lv_label_set_text(palette_header_label, "Palette Rapide");
    lv_obj_set_style_text_color(palette_header_label, lv_color_hex(0x9fb0c8), 0);
    lv_obj_set_style_text_font(palette_header_label, &lv_font_montserrat_14, 0);

    quick_palette_container = lv_obj_create(palette_section_container);
    lv_obj_remove_style_all(quick_palette_container);
    lv_obj_set_size(quick_palette_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(quick_palette_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(quick_palette_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(quick_palette_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(quick_palette_container, 8, 0);
    lv_obj_set_style_pad_column(quick_palette_container, 8, 0);

    populateQuickPalettes();
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
            } else if (key == SettingsManager::SettingKey::ThemeDockIconRadius && dock_icon_radius_slider) {
                updating_from_manager = true;
                lv_slider_set_value(dock_icon_radius_slider, snap.dockIconRadius, LV_ANIM_OFF);
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

            // Always update preview and button colors (not the wheel)
            updatePreview(snap);
            updateTargetButtonColors(snap);
        });
    }

    // Apply snapshot AFTER all widgets are created (especially color wheels)
    applySnapshot(snapshot);
}

void ThemeSettingsScreen::onShow() {
    Logger::getInstance().info(UI_SYMBOL_THEME " Theme settings opened");
    applySnapshot(SettingsManager::getInstance().getSnapshot());
    populateQuickPalettes();
}

void ThemeSettingsScreen::onHide() {
    Logger::getInstance().info(UI_SYMBOL_THEME " Theme settings closed");
}

void ThemeSettingsScreen::applySnapshot(const SettingsSnapshot& snapshot) {
    updating_from_manager = true;

    Logger::getInstance().debugf(UI_SYMBOL_REFRESH " applySnapshot - Primary: 0x%06X, Accent: 0x%06X, Card: 0x%06X, Dock: 0x%06X",
                                 snapshot.primaryColor,
                                 snapshot.accentColor,
                                 snapshot.cardColor,
                                 snapshot.dockColor);

    lv_color_t primary_color = toLvColor(snapshot.primaryColor);
    lv_color_hsv_t primary_hsv = lv_color_rgb_to_hsv(LV_COLOR_GET_R(primary_color),
                                                     LV_COLOR_GET_G(primary_color),
                                                     LV_COLOR_GET_B(primary_color));
    current_primary_hsv = primary_hsv;

    refreshColorPickerForCurrentTarget(snapshot);
    updateTargetButtonColors(snapshot);

    if (border_slider) {
        lv_slider_set_value(border_slider, snapshot.borderRadius, LV_ANIM_OFF);
    }
    if (dock_icon_radius_slider) {
        lv_slider_set_value(dock_icon_radius_slider, snapshot.dockIconRadius, LV_ANIM_OFF);
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

void ThemeSettingsScreen::applyLiveTheme(const SettingsSnapshot& snapshot) {
    lv_color_t primary = toLvColor(snapshot.primaryColor);
    lv_color_t accent = toLvColor(snapshot.accentColor);
    lv_color_t card = toLvColor(snapshot.cardColor);
    lv_color_t dock = toLvColor(snapshot.dockColor);
    lv_color_t accent_muted = lv_color_mix(accent, lv_color_hex(0xffffff), LV_OPA_40);
    lv_color_t dock_muted = lv_color_mix(dock, lv_color_hex(0x000000), LV_OPA_30);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
    }
    if (title_label) {
        lv_obj_set_style_text_color(title_label, accent, 0);
    }

    auto style_card = [&](lv_obj_t* card_obj) {
        if (!card_obj) return;
        lv_obj_set_style_bg_color(card_obj, card, 0);
        lv_obj_set_style_bg_opa(card_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card_obj, snapshot.borderRadius, 0);
        lv_obj_set_style_text_color(card_obj, accent_muted, 0);
    };

    style_card(orientation_card_container);
    style_card(border_card_container);
    style_card(dock_icon_card_container);
    style_card(color_palette_card_container);

    if (orientation_hint_label) {
        lv_obj_set_style_text_color(orientation_hint_label, accent_muted, 0);
    }
    if (palette_header_label) {
        lv_obj_set_style_text_color(palette_header_label, accent, 0);
    }
    if (color_target_selector_container) {
        lv_obj_set_style_bg_color(color_target_selector_container, dock_muted, 0);
        lv_obj_set_style_bg_opa(color_target_selector_container, LV_OPA_30, 0);
        lv_obj_set_style_radius(color_target_selector_container, snapshot.borderRadius / 2 + 4, 0);
        lv_obj_set_style_pad_all(color_target_selector_container, 12, 0);
    }
    if (color_picker_container) {
        lv_obj_set_style_bg_color(color_picker_container, lv_color_mix(card, dock, LV_OPA_50), 0);
        lv_obj_set_style_bg_opa(color_picker_container, LV_OPA_30, 0);
        lv_obj_set_style_radius(color_picker_container, snapshot.borderRadius / 2, 0);
        lv_obj_set_style_pad_all(color_picker_container, 8, 0);
    }
    if (color_picker_label) {
        lv_obj_set_style_text_color(color_picker_label, accent, 0);
    }
    for (const auto& button : color_target_buttons_) {
        if (!button.button) continue;
        lv_obj_set_style_border_color(button.button, accent, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(button.button, lv_color_hex(0x000000), LV_PART_MAIN);
    }
    if (palette_section_container) {
        lv_obj_set_style_bg_color(palette_section_container, dock, 0);
        lv_obj_set_style_bg_opa(palette_section_container, LV_OPA_40, 0);
        lv_obj_set_style_radius(palette_section_container, snapshot.borderRadius, 0);
    }

    auto style_slider = [&](lv_obj_t* slider) {
        if (!slider) return;
        lv_obj_set_style_bg_color(slider, lv_color_mix(dock, primary, LV_OPA_60), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(slider, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider, accent, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, accent, LV_PART_KNOB);
        lv_obj_set_style_border_width(slider, 0, LV_PART_KNOB);
    };
    style_slider(border_slider);
    style_slider(dock_icon_radius_slider);

    if (orientation_switch) {
        lv_obj_set_style_bg_color(orientation_switch, lv_color_mix(dock, primary, LV_OPA_50), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(orientation_switch, LV_OPA_30, LV_PART_MAIN);
        lv_obj_set_style_bg_color(orientation_switch, accent, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(orientation_switch, lv_color_hex(0xffffff), LV_PART_KNOB);
        lv_obj_set_style_bg_color(orientation_switch, accent, LV_PART_KNOB | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(orientation_switch, 0, LV_PART_KNOB);
    }
}

void ThemeSettingsScreen::updatePreview(const SettingsSnapshot& snapshot) {
    applyLiveTheme(snapshot);

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

void ThemeSettingsScreen::populateQuickPalettes() {
    if (!quick_palette_container) {
        return;
    }

    lv_obj_clean(quick_palette_container);
    quick_palettes_.clear();

    const auto& palettes = SettingsManager::getInstance().getThemePalettes();
    quick_palettes_ = palettes;

    if (quick_palettes_.empty()) {
        lv_obj_t* empty = lv_label_create(quick_palette_container);
        lv_label_set_text(empty, "Nessuna palette trovata");
        lv_obj_set_style_text_color(empty, lv_color_hex(0xc0c0c0), 0);
        return;
    }

    for (size_t i = 0; i < quick_palettes_.size(); ++i) {
        ThemePalette* palette = &quick_palettes_[i];

        lv_obj_t* btn = lv_btn_create(quick_palette_container);
        lv_obj_set_size(btn, 90, 36);
        lv_obj_set_style_bg_color(btn, toLvColor(palette->primary), 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_add_event_cb(btn, handlePaletteButton, LV_EVENT_CLICKED, palette);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, palette->name.c_str());
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
        lv_obj_center(lbl);
    }
}

const char* ThemeSettingsScreen::getTargetLabel(ColorTarget target) const {
    switch (target) {
        case ColorTarget::Primary:
            return "Primario";
        case ColorTarget::Accent:
            return "Accento";
        case ColorTarget::Card:
            return "Card";
        case ColorTarget::Dock:
            return "Dock";
        case ColorTarget::DockIconBackground:
            return "Sfondo Icone Dock";
        case ColorTarget::DockIconSymbol:
            return "Simbolo Icone Dock";
    }
    return "Sconosciuto";
}

uint32_t ThemeSettingsScreen::getColorForTarget(const SettingsSnapshot& snapshot, ColorTarget target) const {
    switch (target) {
        case ColorTarget::Primary:
            return snapshot.primaryColor;
        case ColorTarget::Accent:
            return snapshot.accentColor;
        case ColorTarget::Card:
            return snapshot.cardColor;
        case ColorTarget::Dock:
            return snapshot.dockColor;
        case ColorTarget::DockIconBackground:
            return snapshot.dockIconBackgroundColor;
        case ColorTarget::DockIconSymbol:
            return snapshot.dockIconSymbolColor;
    }
    return snapshot.primaryColor;
}

void ThemeSettingsScreen::updateTargetButtonColors(const SettingsSnapshot& snapshot) {
    for (auto& target_button : color_target_buttons_) {
        if (!target_button.button) continue;
        uint32_t hex = getColorForTarget(snapshot, target_button.target);
        lv_color_t color = toLvColor(hex);
        lv_obj_set_style_bg_color(target_button.button, color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(target_button.button, color, LV_PART_MAIN | LV_STATE_CHECKED);
    }
}

void ThemeSettingsScreen::setActiveColorTarget(ColorTarget target, const SettingsSnapshot& snapshot) {
    current_target_ = target;
    if (color_picker_label) {
        lv_label_set_text_fmt(color_picker_label, "Modifica: %s", getTargetLabel(target));
    }
    updateTargetButtonStates();
    refreshColorPickerForCurrentTarget(snapshot);
}

void ThemeSettingsScreen::refreshColorPickerForCurrentTarget(const SettingsSnapshot& snapshot) {
    if (!color_picker_widget) {
        return;
    }

    uint32_t hex = snapshot.primaryColor;
    switch (current_target_) {
        case ColorTarget::Primary:
            hex = snapshot.primaryColor;
            break;
        case ColorTarget::Accent:
            hex = snapshot.accentColor;
            break;
        case ColorTarget::Card:
            hex = snapshot.cardColor;
            break;
        case ColorTarget::Dock:
            hex = snapshot.dockColor;
            break;
        case ColorTarget::DockIconBackground:
            hex = snapshot.dockIconBackgroundColor;
            break;
        case ColorTarget::DockIconSymbol:
            hex = snapshot.dockIconSymbolColor;
            break;
    }

    lv_color_t color = toLvColor(hex);
    if (current_target_ == ColorTarget::Primary) {
        lv_color_hsv_t hsv = lv_color_rgb_to_hsv(LV_COLOR_GET_R(color),
                                                 LV_COLOR_GET_G(color),
                                                 LV_COLOR_GET_B(color));
        current_primary_hsv = hsv;
        CircularColorPicker::set_hsv(color_picker_widget, hsv);
    } else {
        CircularColorPicker::set_rgb(color_picker_widget, color);
    }
}

void ThemeSettingsScreen::updateTargetButtonStates() {
    for (auto& target_button : color_target_buttons_) {
        if (!target_button.button) {
            continue;
        }
        if (target_button.target == current_target_) {
            lv_obj_add_state(target_button.button, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(target_button.button, LV_STATE_CHECKED);
        }
    }
}

void ThemeSettingsScreen::handleUnifiedColorPicker(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_color_t color = CircularColorPicker::get_rgb(screen->color_picker_widget);
    uint32_t hex = toHex(color);

    SettingsManager& manager = SettingsManager::getInstance();
    switch (screen->current_target_) {
        case ColorTarget::Primary: {
            lv_color_hsv_t hsv = CircularColorPicker::get_hsv(screen->color_picker_widget);
            screen->current_primary_hsv = hsv;
            Logger::getInstance().debugf(UI_SYMBOL_THEME " Primary color: 0x%06X (H:%d S:%d V:%d)",
                                         hex,
                                         hsv.h,
                                         hsv.s,
                                         hsv.v);
            manager.setPrimaryColor(hex);
            break;
        }
        case ColorTarget::Accent:
            Logger::getInstance().debugf(UI_SYMBOL_THEME " Accent color: 0x%06X", hex);
            manager.setAccentColor(hex);
            break;
        case ColorTarget::Card:
            Logger::getInstance().debugf(UI_SYMBOL_THEME " Card color: 0x%06X", hex);
            manager.setCardColor(hex);
            break;
        case ColorTarget::Dock:
            Logger::getInstance().debugf(UI_SYMBOL_THEME " Dock color: 0x%06X", hex);
            manager.setDockColor(hex);
            break;
        case ColorTarget::DockIconBackground:
            Logger::getInstance().debugf(UI_SYMBOL_THEME " Dock icon background: 0x%06X", hex);
            manager.setDockIconBackgroundColor(hex);
            break;
        case ColorTarget::DockIconSymbol:
            Logger::getInstance().debugf(UI_SYMBOL_THEME " Dock icon symbol: 0x%06X", hex);
            manager.setDockIconSymbolColor(hex);
            break;
    }
}

void ThemeSettingsScreen::handleColorTargetButton(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    lv_obj_t* btn = lv_event_get_target(e);
    for (const auto& target_button : screen->color_target_buttons_) {
        if (target_button.button == btn) {
            if (screen->current_target_ != target_button.target) {
                screen->setActiveColorTarget(target_button.target,
                                             SettingsManager::getInstance().getSnapshot());
            }
            break;
        }
    }
}

void ThemeSettingsScreen::handleBorderRadius(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;
    int32_t value = lv_slider_get_value(screen->border_slider);
    SettingsManager::getInstance().setBorderRadius(static_cast<uint8_t>(value));
}

void ThemeSettingsScreen::handleDockIconRadius(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;
    int32_t value = lv_slider_get_value(screen->dock_icon_radius_slider);
    SettingsManager::getInstance().setDockIconRadius(static_cast<uint8_t>(value));
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
    auto* preset = static_cast<const ThemePalette*>(lv_event_get_user_data(e));
    if (!preset) return;

    SettingsManager& manager = SettingsManager::getInstance();
    manager.setPrimaryColor(preset->primary);
    manager.setAccentColor(preset->accent);
    manager.setCardColor(preset->card);
    manager.setDockColor(preset->dock);
    manager.setDockIconBackgroundColor(preset->dockIconBackground);
    manager.setDockIconSymbolColor(preset->dockIconSymbol);
    manager.setDockIconRadius(preset->dockIconRadius);

    // The settings listener will trigger applySnapshot() which updates the color pickers
}
