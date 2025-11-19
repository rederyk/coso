#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"
#include <lvgl.h>
#include <cstddef>
#include <vector>

/**
 * @brief Advanced LED RGB Settings screen with CircularColorPicker
 *
 * Features:
 * - Pattern buttons: Pulse1, Pulse2, Rainbow, Strobe1, Strobe2, Strobe3
 * - CircularColorPicker for pattern color customization
 * - Each pattern stores its own color
 * - Button colors update dynamically
 * - Brightness and speed controls
 * - Idle timeout configuration
 */
class LedSettingsScreen : public Screen {
public:
    ~LedSettingsScreen() override;
    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    enum class PatternType {
        Pulse,
        Rainbow,
        Strobe,
    };

    struct PatternButton {
        lv_obj_t* button;
        int pattern_id;
        const char* label;
        uint32_t color; // Stores RGB color for this pattern
        int variant_index;
        PatternType type;
    };

    void applySnapshot(const SettingsSnapshot& snapshot);
    void applyThemeStyles(const SettingsSnapshot& snapshot);
    void updateBrightnessLabel(uint8_t value);
    void updateSpeedLabel(uint8_t value);
    void updateTimeoutLabel(uint32_t value);
    void updateColorPicker();
    void updatePatternButtonColor(int button_index, uint32_t color);
    std::vector<uint32_t> collectPatternColors(PatternType type, size_t button_index, size_t& selected_position) const;
    void syncStrobePalette(size_t button_index_override = SIZE_MAX);
    void syncPulsePalette(size_t button_index_override = SIZE_MAX);
    void applyPatternSelection(int button_index);
    void configureColorPickerForType(PatternType type);

    // Event handlers
    static void handlePatternButton(lv_event_t* e);
    static void handleColorPickerChanged(lv_event_t* e);
    static void handleBrightnessChanged(lv_event_t* e);
    static void handleSpeedChanged(lv_event_t* e);
    static void handleTimeoutChanged(lv_event_t* e);
    static void handleBackButton(lv_event_t* e);

    // UI components
    lv_obj_t* header_label = nullptr;
    lv_obj_t* back_btn = nullptr;
    lv_obj_t* content_container = nullptr;

    // Pattern Selection Card
    lv_obj_t* pattern_card = nullptr;
    std::vector<PatternButton> pattern_buttons_;

    // Color Picker Card
    lv_obj_t* color_picker_card = nullptr;
    lv_obj_t* color_picker_widget = nullptr;
    lv_obj_t* color_picker_label = nullptr;

    // Brightness Card
    lv_obj_t* brightness_card = nullptr;
    lv_obj_t* brightness_slider = nullptr;
    lv_obj_t* brightness_value_label = nullptr;

    // Speed Card
    lv_obj_t* speed_card = nullptr;
    lv_obj_t* speed_slider = nullptr;
    lv_obj_t* speed_value_label = nullptr;

    // Idle Timeout Card
    lv_obj_t* timeout_card = nullptr;
    lv_obj_t* timeout_slider = nullptr;
    lv_obj_t* timeout_value_label = nullptr;

    // State
    bool updating_from_manager = false;
    uint32_t settings_listener_id = 0;
    int current_pattern_index = 0; // Index of last selected button
    PatternType current_pattern_type = PatternType::Pulse;
};
