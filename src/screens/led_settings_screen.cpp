#include "screens/led_settings_screen.h"
#include "core/app_manager.h"
#include "drivers/rgb_led_driver.h"
#include "widgets/circular_color_picker.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"

namespace {
lv_obj_t* create_card(lv_obj_t* parent, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_style_bg_color(card, lv_color_hex(0x10182c), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    if (title) {
        lv_obj_t* title_lbl = lv_label_create(card);
        lv_label_set_text(title_lbl, title);
        lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xf0f0f0), 0);
    }

    return card;
}

uint32_t toLvColorHex(lv_color_t color) {
    uint32_t color32 = lv_color_to32(color);
    uint32_t b = (color32 >> 0) & 0xFF;
    uint32_t g = (color32 >> 8) & 0xFF;
    uint32_t r = (color32 >> 16) & 0xFF;
    return (r << 16) | (g << 8) | b;
}

} // namespace

LedSettingsScreen::~LedSettingsScreen() {
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void LedSettingsScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    SettingsManager& manager = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = manager.getSnapshot();

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_set_style_pad_row(root, 8, 0);

    // Header with back button
    lv_obj_t* header_container = lv_obj_create(root);
    lv_obj_remove_style_all(header_container);
    lv_obj_set_width(header_container, lv_pct(100));
    lv_obj_set_height(header_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(header_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header_container, 0, 0);

    back_btn = lv_btn_create(header_container);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_add_event_cb(back_btn, handleBackButton, LV_EVENT_CLICKED, this);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    header_label = lv_label_create(header_container);
    lv_label_set_text(header_label, UI_SYMBOL_LED " LED Studio");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_pad_left(header_label, 12, 0);

    // Content container
    content_container = lv_obj_create(root);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_width(content_container, lv_pct(100));
    lv_obj_set_height(content_container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(content_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_pad_row(content_container, 12, 0);

    // ========== Pattern Selection Card ==========
    pattern_card = create_card(content_container, "Pattern");

    // Pattern buttons grid
    lv_obj_t* pattern_grid = lv_obj_create(pattern_card);
    lv_obj_remove_style_all(pattern_grid);
    lv_obj_set_width(pattern_grid, lv_pct(100));
    lv_obj_set_layout(pattern_grid, LV_LAYOUT_GRID);

    static lv_coord_t pattern_cols[] = {
        LV_GRID_FR(1),
        LV_GRID_FR(1),
        LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST,
    };
    static lv_coord_t pattern_rows[] = {
        LV_GRID_CONTENT,
        LV_GRID_CONTENT,
        LV_GRID_TEMPLATE_LAST,
    };
    lv_obj_set_grid_dsc_array(pattern_grid, pattern_cols, pattern_rows);
    lv_obj_set_style_pad_row(pattern_grid, 8, 0);
    lv_obj_set_style_pad_column(pattern_grid, 8, 0);

    // Pattern definitions with default colors
    struct PatternDef {
        const char* label;
        uint32_t default_color;
        int pattern_id;
    };

    static constexpr PatternDef patterns[] = {
        {"Pulse 1", 0xFF64C8, 0},  // Bright pink
        {"Pulse 2", 0x6496FF, 1},  // Light blue
        {"Rainbow", 0xFF00FF, 2},  // Magenta (multicolor hint)
        {"Strobe 1", 0xFFFFFF, 3}, // White
        {"Strobe 2", 0xFF0000, 4}, // Red
        {"Strobe 3", 0x00FF00, 5}, // Green
    };

    pattern_buttons_.clear();
    for (size_t i = 0; i < 6; ++i) {
        const auto& pat = patterns[i];

        lv_obj_t* btn = lv_btn_create(pattern_grid);
        lv_obj_set_height(btn, 50);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(pat.default_color), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x00d4ff), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_opa(btn, LV_OPA_100, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(btn, handlePatternButton, LV_EVENT_CLICKED, this);

        lv_obj_set_grid_cell(btn,
                             LV_GRID_ALIGN_STRETCH,
                             i % 3,
                             1,
                             LV_GRID_ALIGN_CENTER,
                             i / 3,
                             1);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, pat.label);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
        lv_obj_center(lbl);

        PatternButton pb;
        pb.button = btn;
        pb.pattern_id = pat.pattern_id;
        pb.color = pat.default_color;
        pattern_buttons_.push_back(pb);
    }

    // Set initial pattern (Pulse 1)
    if (!pattern_buttons_.empty()) {
        lv_obj_add_state(pattern_buttons_[0].button, LV_STATE_CHECKED);
        current_pattern = 0;
    }

    // ========== Color Picker Card ==========
    color_picker_card = create_card(content_container, "Color Custom");

    lv_obj_t* picker_container = lv_obj_create(color_picker_card);
    lv_obj_remove_style_all(picker_container);
    lv_obj_set_width(picker_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(picker_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(picker_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(picker_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(picker_container, 10, 0);

    color_picker_label = lv_label_create(picker_container);
    lv_obj_set_style_text_font(color_picker_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(color_picker_label, lv_color_hex(0xf0f0f0), 0);
    lv_label_set_text(color_picker_label, "Seleziona colore pattern");

    color_picker_widget = CircularColorPicker::create(picker_container, 160, 70);
    if (color_picker_widget) {
        lv_obj_add_event_cb(color_picker_widget, handleColorPickerChanged, LV_EVENT_VALUE_CHANGED, this);
    }

    // ========== Brightness Card ==========
    brightness_card = create_card(content_container, "Luminosità");

    lv_obj_t* brightness_row = lv_obj_create(brightness_card);
    lv_obj_remove_style_all(brightness_row);
    lv_obj_set_width(brightness_row, lv_pct(100));
    lv_obj_set_height(brightness_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(brightness_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(brightness_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(brightness_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* brightness_label = lv_label_create(brightness_row);
    lv_label_set_text(brightness_label, "Intensità");
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xf0f0f0), 0);

    brightness_value_label = lv_label_create(brightness_row);
    lv_label_set_text(brightness_value_label, "50%");
    lv_obj_set_style_text_color(brightness_value_label, lv_color_hex(0x00d4ff), 0);

    brightness_slider = lv_slider_create(brightness_card);
    lv_obj_set_width(brightness_slider, lv_pct(100));
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, handleBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

    // ========== Speed Card ==========
    speed_card = create_card(content_container, "Velocità Animazione");

    lv_obj_t* speed_row = lv_obj_create(speed_card);
    lv_obj_remove_style_all(speed_row);
    lv_obj_set_width(speed_row, lv_pct(100));
    lv_obj_set_height(speed_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(speed_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(speed_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(speed_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* speed_label = lv_label_create(speed_row);
    lv_label_set_text(speed_label, "Velocità");
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0xf0f0f0), 0);

    speed_value_label = lv_label_create(speed_row);
    lv_label_set_text(speed_value_label, "50");
    lv_obj_set_style_text_color(speed_value_label, lv_color_hex(0x00d4ff), 0);

    speed_slider = lv_slider_create(speed_card);
    lv_obj_set_width(speed_slider, lv_pct(100));
    lv_slider_set_range(speed_slider, 1, 100);
    lv_slider_set_value(speed_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(speed_slider, handleSpeedChanged, LV_EVENT_VALUE_CHANGED, this);

    // ========== Idle Timeout Card ==========
    timeout_card = create_card(content_container, "Timeout Inattività");

    lv_obj_t* timeout_row = lv_obj_create(timeout_card);
    lv_obj_remove_style_all(timeout_row);
    lv_obj_set_width(timeout_row, lv_pct(100));
    lv_obj_set_height(timeout_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(timeout_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(timeout_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(timeout_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* timeout_label = lv_label_create(timeout_row);
    lv_label_set_text(timeout_label, "Ritorna a idle dopo");
    lv_obj_set_style_text_color(timeout_label, lv_color_hex(0xf0f0f0), 0);

    timeout_value_label = lv_label_create(timeout_row);
    lv_label_set_text(timeout_value_label, "30s");
    lv_obj_set_style_text_color(timeout_value_label, lv_color_hex(0x00d4ff), 0);

    timeout_slider = lv_slider_create(timeout_card);
    lv_obj_set_width(timeout_slider, lv_pct(100));
    lv_slider_set_range(timeout_slider, 0, 120); // 0-120 secondi
    lv_slider_set_value(timeout_slider, 30, LV_ANIM_OFF);
    lv_obj_add_event_cb(timeout_slider, handleTimeoutChanged, LV_EVENT_VALUE_CHANGED, this);

    // Apply theme and initial values
    applySnapshot(snapshot);
    applyThemeStyles(snapshot);

    // Update color picker to match first pattern
    updateColorPicker();

    // Register settings listener
    settings_listener_id = manager.addListener([this](SettingsManager::SettingKey key, const SettingsSnapshot& snap) {
        applySnapshot(snap);
    });
}

void LedSettingsScreen::onShow() {
    Logger::getInstance().info("[LED Settings] Screen shown");
}

void LedSettingsScreen::onHide() {
    Logger::getInstance().info("[LED Settings] Screen hidden");
}

void LedSettingsScreen::applySnapshot(const SettingsSnapshot& snapshot) {
    if (updating_from_manager) return;
    updating_from_manager = true;

    // Apply theme colors
    applyThemeStyles(snapshot);

    // Update brightness from LED manager
    RgbLedManager& led = RgbLedManager::getInstance();
    if (brightness_slider) {
        lv_slider_set_value(brightness_slider, led.getBrightness(), LV_ANIM_OFF);
        updateBrightnessLabel(led.getBrightness());
    }

    if (speed_slider) {
        lv_slider_set_value(speed_slider, led.getAnimationSpeed(), LV_ANIM_OFF);
        updateSpeedLabel(led.getAnimationSpeed());
    }

    if (timeout_slider) {
        lv_slider_set_value(timeout_slider, led.getIdleTimeout() / 1000, LV_ANIM_OFF);
        updateTimeoutLabel(led.getIdleTimeout() / 1000);
    }

    updating_from_manager = false;
}

void LedSettingsScreen::applyThemeStyles(const SettingsSnapshot& snapshot) {
    if (!root) return;

    lv_color_t bg_color = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent_color = lv_color_hex(snapshot.accentColor);

    lv_obj_set_style_bg_color(root, bg_color, 0);

    if (header_label) {
        lv_obj_set_style_text_color(header_label, lv_color_hex(0xffffff), 0);
    }
    if (back_btn) {
        lv_obj_set_style_bg_color(back_btn, accent_color, 0);
    }
}

void LedSettingsScreen::updateBrightnessLabel(uint8_t value) {
    if (brightness_value_label) {
        lv_label_set_text_fmt(brightness_value_label, "%d%%", value);
    }
}

void LedSettingsScreen::updateSpeedLabel(uint8_t value) {
    if (speed_value_label) {
        lv_label_set_text_fmt(speed_value_label, "%d", value);
    }
}

void LedSettingsScreen::updateTimeoutLabel(uint32_t value) {
    if (timeout_value_label) {
        if (value == 0) {
            lv_label_set_text(timeout_value_label, "OFF");
        } else if (value < 60) {
            lv_label_set_text_fmt(timeout_value_label, "%ds", (int)value);
        } else {
            lv_label_set_text_fmt(timeout_value_label, "%dm", (int)(value / 60));
        }
    }
}

void LedSettingsScreen::updateColorPicker() {
    if (!color_picker_widget || current_pattern < 0 || current_pattern >= (int)pattern_buttons_.size()) {
        return;
    }

    uint32_t color = pattern_buttons_[current_pattern].color;
    CircularColorPicker::set_rgb(color_picker_widget, lv_color_hex(color));
}

void LedSettingsScreen::updatePatternButtonColor(int pattern_id, uint32_t color) {
    for (auto& pb : pattern_buttons_) {
        if (pb.pattern_id == pattern_id) {
            pb.color = color;
            lv_obj_set_style_bg_color(pb.button, lv_color_hex(color), 0);
            break;
        }
    }
}

// ========== Event Handlers ==========

void LedSettingsScreen::handlePatternButton(lv_event_t* e) {
    LedSettingsScreen* screen = static_cast<LedSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* clicked_btn = lv_event_get_target(e);

    // Find which pattern was clicked
    int selected_pattern = -1;
    for (size_t i = 0; i < screen->pattern_buttons_.size(); ++i) {
        if (screen->pattern_buttons_[i].button == clicked_btn) {
            selected_pattern = screen->pattern_buttons_[i].pattern_id;
            lv_obj_add_state(clicked_btn, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(screen->pattern_buttons_[i].button, LV_STATE_CHECKED);
        }
    }

    if (selected_pattern < 0) return;

    screen->current_pattern = selected_pattern;
    RgbLedManager& led = RgbLedManager::getInstance();

    // Update color picker to match selected pattern
    screen->updateColorPicker();

    // Apply pattern to LED with stored color
    uint32_t color = screen->pattern_buttons_[selected_pattern].color;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Hide/show color picker for Rainbow
    if (selected_pattern == 2) { // Rainbow
        lv_obj_add_flag(screen->color_picker_card, LV_OBJ_FLAG_HIDDEN);
        led.setState(RgbLedManager::LedState::RAINBOW);
    } else {
        lv_obj_clear_flag(screen->color_picker_card, LV_OBJ_FLAG_HIDDEN);

        // Apply pattern
        if (selected_pattern == 0 || selected_pattern == 1) { // Pulse 1/2
            led.setPulseColor(r, g, b);
        } else if (selected_pattern >= 3 && selected_pattern <= 5) { // Strobe 1/2/3
            led.setStrobeColor(r, g, b);
        }
    }

    Logger::getInstance().infof("[LED Settings] Pattern changed to: %d", selected_pattern);
}

void LedSettingsScreen::handleColorPickerChanged(lv_event_t* e) {
    LedSettingsScreen* screen = static_cast<LedSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* picker = lv_event_get_target(e);
    lv_color_t color = CircularColorPicker::get_rgb(picker);
    uint32_t color_hex = toLvColorHex(color);

    // Update current pattern's color
    screen->updatePatternButtonColor(screen->current_pattern, color_hex);

    // Apply to LED
    uint8_t r = (color_hex >> 16) & 0xFF;
    uint8_t g = (color_hex >> 8) & 0xFF;
    uint8_t b = color_hex & 0xFF;

    RgbLedManager& led = RgbLedManager::getInstance();

    if (screen->current_pattern == 0 || screen->current_pattern == 1) { // Pulse
        led.setPulseColor(r, g, b);
    } else if (screen->current_pattern >= 3 && screen->current_pattern <= 5) { // Strobe
        led.setStrobeColor(r, g, b);
    }

    Logger::getInstance().infof("[LED Settings] Color changed to #%06X", color_hex);
}

void LedSettingsScreen::handleBrightnessChanged(lv_event_t* e) {
    LedSettingsScreen* screen = static_cast<LedSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* slider = lv_event_get_target(e);
    uint8_t value = lv_slider_get_value(slider);

    screen->updateBrightnessLabel(value);
    RgbLedManager::getInstance().setBrightness(value);

    Logger::getInstance().infof("[LED Settings] Brightness changed to: %d%%", value);
}

void LedSettingsScreen::handleSpeedChanged(lv_event_t* e) {
    LedSettingsScreen* screen = static_cast<LedSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* slider = lv_event_get_target(e);
    uint8_t value = lv_slider_get_value(slider);

    screen->updateSpeedLabel(value);
    RgbLedManager::getInstance().setAnimationSpeed(value);

    Logger::getInstance().infof("[LED Settings] Animation speed changed to: %d", value);
}

void LedSettingsScreen::handleTimeoutChanged(lv_event_t* e) {
    LedSettingsScreen* screen = static_cast<LedSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* slider = lv_event_get_target(e);
    uint32_t value = lv_slider_get_value(slider);

    screen->updateTimeoutLabel(value);
    RgbLedManager::getInstance().setIdleTimeout(value * 1000); // Convert to ms

    Logger::getInstance().infof("[LED Settings] Idle timeout changed to: %ds", (int)value);
}

void LedSettingsScreen::handleBackButton(lv_event_t* e) {
    LedSettingsScreen* screen = static_cast<LedSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    Logger::getInstance().info("[LED Settings] Back button pressed");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("settings");
    }
}
