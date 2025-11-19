#include "screens/led_settings_screen.h"
#include "core/app_manager.h"
#include "drivers/rgb_led_driver.h"
#include "widgets/circular_color_picker.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include <algorithm>

namespace {
lv_obj_t* create_card(lv_obj_t* parent, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT); // Fix: ensure card wraps content
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

    // ========== Content Container - Main scrollable area ==========
    content_container = lv_obj_create(root);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_width(content_container, lv_pct(100));
    lv_obj_set_height(content_container, LV_SIZE_CONTENT);  // Auto height based on children
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(content_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);  // Stack children vertically
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_pad_row(content_container, 6, 0);  // 6px spacing between cards (reduced)

    // ========== Brightness Card (Compact) ==========
    brightness_card = create_card(content_container, nullptr);  // No title on card
    lv_obj_set_style_pad_all(brightness_card, 8, 0);  // Card padding: 8px all sides (reduced)

    // Row container for icon + title + value label
    lv_obj_t* brightness_row = lv_obj_create(brightness_card);
    lv_obj_remove_style_all(brightness_row);
    lv_obj_set_width(brightness_row, lv_pct(100));
    lv_obj_set_height(brightness_row, LV_SIZE_CONTENT);  // Auto height
    lv_obj_set_layout(brightness_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(brightness_row, LV_FLEX_FLOW_ROW);  // Horizontal layout
    lv_obj_set_flex_align(brightness_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(brightness_row, 3, 0);  // 3px space below row (reduced)

    // Icon label (EYE symbol for brightness)
    lv_obj_t* brightness_icon = lv_label_create(brightness_row);
    lv_label_set_text(brightness_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(brightness_icon, &lv_font_montserrat_16, 0);  // Font size: 16px
    lv_obj_set_style_text_color(brightness_icon, lv_color_hex(0xf0f0f0), 0);

    // Title label (centered)
    lv_obj_t* brightness_title = lv_label_create(brightness_row);
    lv_label_set_text(brightness_title, "Luminosita");
    lv_obj_set_style_text_font(brightness_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(brightness_title, lv_color_hex(0xf0f0f0), 0);

    // Value label (percentage)
    brightness_value_label = lv_label_create(brightness_row);
    lv_label_set_text(brightness_value_label, "50%");
    lv_obj_set_style_text_color(brightness_value_label, lv_color_hex(0x00d4ff), 0);  // Accent color

    // Slider
    brightness_slider = lv_slider_create(brightness_card);
    lv_obj_set_width(brightness_slider, lv_pct(100));
    lv_obj_set_height(brightness_slider, 8);  // Slider height: 8px (compact!)
    lv_obj_set_style_pad_all(brightness_slider, 0, LV_PART_MAIN);  // Remove padding
    lv_obj_set_style_pad_all(brightness_slider, 0, LV_PART_INDICATOR);  // Remove indicator padding
    lv_obj_set_style_pad_all(brightness_slider, 0, LV_PART_KNOB);  // Remove knob padding
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, handleBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

    // ========== Speed Card (Compact) ==========
    speed_card = create_card(content_container, nullptr);  // No title on card
    lv_obj_set_style_pad_all(speed_card, 8, 0);  // Card padding: 8px all sides (reduced)

    // Row container for icon + title + value label
    lv_obj_t* speed_row = lv_obj_create(speed_card);
    lv_obj_remove_style_all(speed_row);
    lv_obj_set_width(speed_row, lv_pct(100));
    lv_obj_set_height(speed_row, LV_SIZE_CONTENT);  // Auto height
    lv_obj_set_layout(speed_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(speed_row, LV_FLEX_FLOW_ROW);  // Horizontal layout
    lv_obj_set_flex_align(speed_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(speed_row, 3, 0);  // 3px space below row (reduced)

    // Icon label (REFRESH symbol)
    lv_obj_t* speed_icon = lv_label_create(speed_row);
    lv_label_set_text(speed_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(speed_icon, &lv_font_montserrat_16, 0);  // Font size: 16px
    lv_obj_set_style_text_color(speed_icon, lv_color_hex(0xf0f0f0), 0);

    // Title label (centered)
    lv_obj_t* speed_title = lv_label_create(speed_row);
    lv_label_set_text(speed_title, "Velocita");
    lv_obj_set_style_text_font(speed_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(speed_title, lv_color_hex(0xf0f0f0), 0);

    // Value label (numeric)
    speed_value_label = lv_label_create(speed_row);
    lv_label_set_text(speed_value_label, "50");
    lv_obj_set_style_text_color(speed_value_label, lv_color_hex(0x00d4ff), 0);  // Accent color

    // Slider
    speed_slider = lv_slider_create(speed_card);
    lv_obj_set_width(speed_slider, lv_pct(100));
    lv_obj_set_height(speed_slider, 8);  // Slider height: 8px (compact!)
    lv_obj_set_style_pad_all(speed_slider, 0, LV_PART_MAIN);  // Remove padding
    lv_obj_set_style_pad_all(speed_slider, 0, LV_PART_INDICATOR);  // Remove indicator padding
    lv_obj_set_style_pad_all(speed_slider, 0, LV_PART_KNOB);  // Remove knob padding
    lv_slider_set_range(speed_slider, 1, 100);
    lv_slider_set_value(speed_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(speed_slider, handleSpeedChanged, LV_EVENT_VALUE_CHANGED, this);

    // ========== Idle Timeout Card (Compact) ==========
    timeout_card = create_card(content_container, nullptr);  // No title on card
    lv_obj_set_style_pad_all(timeout_card, 8, 0);  // Card padding: 8px all sides (reduced)

    // Row container for icon + title + value label
    lv_obj_t* timeout_row = lv_obj_create(timeout_card);
    lv_obj_remove_style_all(timeout_row);
    lv_obj_set_width(timeout_row, lv_pct(100));
    lv_obj_set_height(timeout_row, LV_SIZE_CONTENT);  // Auto height
    lv_obj_set_layout(timeout_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(timeout_row, LV_FLEX_FLOW_ROW);  // Horizontal layout
    lv_obj_set_flex_align(timeout_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(timeout_row, 3, 0);  // 3px space below row (reduced)

    // Icon label (POWER symbol)
    lv_obj_t* timeout_icon = lv_label_create(timeout_row);
    lv_label_set_text(timeout_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(timeout_icon, &lv_font_montserrat_16, 0);  // Font size: 16px
    lv_obj_set_style_text_color(timeout_icon, lv_color_hex(0xf0f0f0), 0);

    // Title label (centered)
    lv_obj_t* timeout_title = lv_label_create(timeout_row);
    lv_label_set_text(timeout_title, "Timeout");
    lv_obj_set_style_text_font(timeout_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(timeout_title, lv_color_hex(0xf0f0f0), 0);

    // Value label (time with unit)
    timeout_value_label = lv_label_create(timeout_row);
    lv_label_set_text(timeout_value_label, "30s");
    lv_obj_set_style_text_color(timeout_value_label, lv_color_hex(0x00d4ff), 0);  // Accent color

    // Slider
    timeout_slider = lv_slider_create(timeout_card);
    lv_obj_set_width(timeout_slider, lv_pct(100));
    lv_obj_set_height(timeout_slider, 8);  // Slider height: 8px (compact!)
    lv_obj_set_style_pad_all(timeout_slider, 0, LV_PART_MAIN);  // Remove padding
    lv_obj_set_style_pad_all(timeout_slider, 0, LV_PART_INDICATOR);  // Remove indicator padding
    lv_obj_set_style_pad_all(timeout_slider, 0, LV_PART_KNOB);  // Remove knob padding
    lv_slider_set_range(timeout_slider, 0, 120);  // 0-120 seconds
    lv_slider_set_value(timeout_slider, 30, LV_ANIM_OFF);
    lv_obj_add_event_cb(timeout_slider, handleTimeoutChanged, LV_EVENT_VALUE_CHANGED, this);

    // ========== Pattern & Color Picker Card (Combined) ==========
    pattern_card = create_card(content_container, "Pattern");  // Shorter title
    lv_obj_set_style_pad_all(pattern_card, 8, 0);  // Reduced padding like theme settings
    lv_obj_set_style_pad_row(pattern_card, 10, 0);  // 10px spacing between children (picker and grid)
    lv_obj_set_flex_align(pattern_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // ========== Color Picker Section (centered at top) ==========
    color_picker_card = lv_obj_create(pattern_card);
    lv_obj_remove_style_all(color_picker_card);
    lv_obj_set_width(color_picker_card, lv_pct(100));
    lv_obj_set_height(color_picker_card, LV_SIZE_CONTENT);  // Auto height based on picker size
    lv_obj_set_layout(color_picker_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(color_picker_card, LV_FLEX_FLOW_COLUMN);  // Stack vertically
    lv_obj_set_flex_align(color_picker_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // Center horizontally
    lv_obj_set_style_pad_all(color_picker_card, 0, 0);  // No padding

    // Circular color picker widget - diameter: 200px (balanced size like theme settings)
    color_picker_widget = CircularColorPicker::create(color_picker_card, 200, 70, false);
    if (color_picker_widget) {
        lv_obj_add_event_cb(color_picker_widget, handleColorPickerChanged, LV_EVENT_VALUE_CHANGED, this);
    }

    // ========== Pattern Buttons Grid (3x2 grid below picker) ==========
    lv_obj_t* pattern_grid = lv_obj_create(pattern_card);
    lv_obj_remove_style_all(pattern_grid);
    lv_obj_set_height(pattern_grid, LV_SIZE_CONTENT);  // Auto height based on  size

    lv_obj_set_width(pattern_grid, lv_pct(100));
    lv_obj_set_layout(pattern_grid, LV_LAYOUT_GRID);  // Grid layout

    // Grid: 3 columns of equal width (1fr each)
    static lv_coord_t pattern_cols[] = {
        LV_GRID_FR(1),  // Column 1
        LV_GRID_FR(1),  // Column 2
        LV_GRID_FR(1),  // Column 3
        LV_GRID_TEMPLATE_LAST,
    };
    // Grid: 2 rows with content-based height
    static lv_coord_t pattern_rows[] = {
        LV_GRID_CONTENT,  // Row 1 (auto height)
        LV_GRID_CONTENT,  // Row 2 (auto height)
        LV_GRID_TEMPLATE_LAST,
    };
    lv_obj_set_grid_dsc_array(pattern_grid, pattern_cols, pattern_rows);
    lv_obj_set_style_pad_row(pattern_grid, 8, 0);     // 8px vertical spacing between buttons
    lv_obj_set_style_pad_column(pattern_grid, 8, 0);  // 8px horizontal spacing between buttons

    // Pattern definitions with default colors grouped by type
    struct PatternDef {
        const char* label;
        uint32_t default_color;
        PatternType type;
        int variant_index;
    };

    static constexpr PatternDef patterns[] = {
        {"Pulse 1", 0xFF64C8, PatternType::Pulse, 0},
        {"Pulse 2", 0x6496FF, PatternType::Pulse, 1},
        {"Rainbow", 0xFF00FF, PatternType::Rainbow, 0},
        {"Strobe 1", 0xFFFFFF, PatternType::Strobe, 0},
        {"Strobe 2", 0xFF0000, PatternType::Strobe, 1},
        {"Strobe 3", 0x00FF00, PatternType::Strobe, 2},
    };

    // Create pattern buttons (3x2 grid)
    pattern_buttons_.clear();
    constexpr size_t pattern_count = sizeof(patterns) / sizeof(patterns[0]);
    for (size_t i = 0; i < pattern_count; ++i) {
        const auto& pat = patterns[i];

        // Create button
        lv_obj_t* btn = lv_btn_create(pattern_grid);
        lv_obj_set_height(btn, 38);
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

        PatternButton pb{};
        pb.button = btn;
        pb.pattern_id = static_cast<int>(i);
        pb.label = pat.label;
        pb.color = pat.default_color;
        pb.variant_index = pat.variant_index;
        pb.type = pat.type;
        pattern_buttons_.push_back(pb);
    }

    if (!pattern_buttons_.empty()) {
        current_pattern_index = 0;
        current_pattern_type = pattern_buttons_[0].type;
        lv_obj_add_state(pattern_buttons_[0].button, LV_STATE_CHECKED);
        applyPatternSelection(0);
    }

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
    if (!color_picker_widget || current_pattern_index < 0 || current_pattern_index >= (int)pattern_buttons_.size()) {
        return;
    }

    configureColorPickerForType(current_pattern_type);
    if (current_pattern_type == PatternType::Rainbow) {
        return;
    }

    uint32_t color = pattern_buttons_[current_pattern_index].color;
    CircularColorPicker::set_rgb(color_picker_widget, lv_color_hex(color));
}

void LedSettingsScreen::updatePatternButtonColor(int button_index, uint32_t color) {
    if (button_index < 0 || button_index >= (int)pattern_buttons_.size()) {
        return;
    }

    auto& pb = pattern_buttons_[button_index];
    pb.color = color;
    lv_obj_set_style_bg_color(pb.button, lv_color_hex(color), 0);
}

void LedSettingsScreen::configureColorPickerForType(PatternType type) {
    if (!color_picker_card) return;

    if (type == PatternType::Rainbow) {
        lv_obj_add_flag(color_picker_card, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(color_picker_card, LV_OBJ_FLAG_HIDDEN);
    }
}

std::vector<uint32_t> LedSettingsScreen::collectPatternColors(PatternType type, size_t button_index, size_t& selected_position) const {
    struct ColorEntry {
        int variant;
        uint32_t color;
        size_t button_index;
    };

    std::vector<ColorEntry> entries;
    entries.reserve(pattern_buttons_.size());
    for (size_t i = 0; i < pattern_buttons_.size(); ++i) {
        const auto& pb = pattern_buttons_[i];
        if (pb.type != type) continue;
        entries.push_back({pb.variant_index, pb.color, i});
    }

    selected_position = 0;
    if (entries.empty()) {
        return {};
    }

    std::sort(entries.begin(), entries.end(), [](const ColorEntry& a, const ColorEntry& b) {
        return a.variant < b.variant;
    });

    size_t match_index = (button_index == SIZE_MAX) ? current_pattern_index : button_index;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].button_index == match_index) {
            selected_position = i;
            break;
        }
    }

    std::vector<uint32_t> colors;
    colors.reserve(entries.size());
    for (const auto& entry : entries) {
        colors.push_back(entry.color);
    }

    return colors;
}

void LedSettingsScreen::syncStrobePalette(size_t button_index_override) {
    size_t selected_position = 0;
    size_t target_index = (button_index_override == SIZE_MAX) ? current_pattern_index : button_index_override;
    auto colors = collectPatternColors(PatternType::Strobe, target_index, selected_position);
    if (colors.empty()) {
        return;
    }

    RgbLedManager::getInstance().setStrobePalette(colors, selected_position);
}

void LedSettingsScreen::applyPatternSelection(int button_index) {
    if (button_index < 0 || button_index >= (int)pattern_buttons_.size()) {
        return;
    }

    const auto& pb = pattern_buttons_[button_index];
    current_pattern_type = pb.type;
    configureColorPickerForType(pb.type);

    RgbLedManager& led = RgbLedManager::getInstance();
    if (pb.type == PatternType::Rainbow) {
        led.setState(RgbLedManager::LedState::RAINBOW);
        return;
    }

    uint32_t color = pb.color;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    if (pb.type == PatternType::Pulse) {
        led.setPulseColor(r, g, b);
    } else if (pb.type == PatternType::Strobe) {
        syncStrobePalette(button_index);
        led.setState(RgbLedManager::LedState::STROBE_CUSTOM);
    }
}

// ========== Event Handlers ==========

void LedSettingsScreen::handlePatternButton(lv_event_t* e) {
    LedSettingsScreen* screen = static_cast<LedSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* clicked_btn = lv_event_get_target(e);

    // Find which pattern was clicked
    int selected_index = -1;
    for (size_t i = 0; i < screen->pattern_buttons_.size(); ++i) {
        if (screen->pattern_buttons_[i].button == clicked_btn) {
            selected_index = static_cast<int>(i);
            lv_obj_add_state(clicked_btn, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(screen->pattern_buttons_[i].button, LV_STATE_CHECKED);
        }
    }

    if (selected_index < 0) return;

    screen->current_pattern_index = selected_index;
    screen->applyPatternSelection(selected_index);
    screen->updateColorPicker();

    const char* label = screen->pattern_buttons_[selected_index].label;
    Logger::getInstance().infof("[LED Settings] Pattern changed to: %s", label ? label : "unknown");
}

void LedSettingsScreen::handleColorPickerChanged(lv_event_t* e) {
    LedSettingsScreen* screen = static_cast<LedSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    lv_obj_t* picker = lv_event_get_target(e);
    lv_color_t color = CircularColorPicker::get_rgb(picker);
    uint32_t color_hex = toLvColorHex(color);

    // Update current pattern's color
    screen->updatePatternButtonColor(screen->current_pattern_index, color_hex);

    // Apply to LED
    uint8_t r = (color_hex >> 16) & 0xFF;
    uint8_t g = (color_hex >> 8) & 0xFF;
    uint8_t b = color_hex & 0xFF;

    RgbLedManager& led = RgbLedManager::getInstance();

    if (screen->current_pattern_type == PatternType::Pulse) {
        led.setPulseColor(r, g, b);
    } else if (screen->current_pattern_type == PatternType::Strobe) {
        screen->syncStrobePalette();
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
