#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"

class ThemeSettingsScreen : public Screen {
public:
    ~ThemeSettingsScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    enum class ColorTarget {
        Primary,
        Accent,
        Card,
        Dock,
        DockIconBackground,
        DockIconSymbol
    };

    struct TargetButton {
        lv_obj_t* button = nullptr;
        ColorTarget target = ColorTarget::Primary;
    };

    void applySnapshot(const SettingsSnapshot& snapshot);
    void applyLiveTheme(const SettingsSnapshot& snapshot);
    void updatePreview(const SettingsSnapshot& snapshot);
    void populateQuickPalettes();
    void setActiveColorTarget(ColorTarget target, const SettingsSnapshot& snapshot);
    void refreshColorPickerForCurrentTarget(const SettingsSnapshot& snapshot);
    void updateTargetButtonStates();
    const char* getTargetLabel(ColorTarget target) const;
    uint32_t getColorForTarget(const SettingsSnapshot& snapshot, ColorTarget target) const;
    void updateTargetButtonColors(const SettingsSnapshot& snapshot);

    static void handleUnifiedColorPicker(lv_event_t* e);
    static void handleColorTargetButton(lv_event_t* e);
    static void handleBorderRadius(lv_event_t* e);
    static void handleDockIconRadius(lv_event_t* e);
    static void handleOrientation(lv_event_t* e);
    static void handlePaletteButton(lv_event_t* e);

    lv_obj_t* color_target_selector_container = nullptr;
    lv_obj_t* color_picker_container = nullptr;
    lv_obj_t* color_picker_label = nullptr;
    lv_obj_t* color_picker_widget = nullptr;
    lv_obj_t* border_slider = nullptr;
    lv_obj_t* dock_icon_radius_slider = nullptr;
    lv_obj_t* orientation_switch = nullptr;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* orientation_card_container = nullptr;
    lv_obj_t* orientation_hint_label = nullptr;
    lv_obj_t* border_card_container = nullptr;
    lv_obj_t* dock_icon_card_container = nullptr;
    lv_obj_t* color_palette_card_container = nullptr;
    lv_obj_t* palette_section_container = nullptr;
    lv_obj_t* palette_header_label = nullptr;
    lv_obj_t* quick_palette_container = nullptr;
    lv_obj_t* preview_card = nullptr;
    lv_obj_t* preview_header = nullptr;
    lv_obj_t* preview_body = nullptr;
    lv_obj_t* preview_card_demo = nullptr;
    lv_obj_t* preview_dock_demo = nullptr;

    uint32_t settings_listener_id = 0;
    bool updating_from_manager = false;
    lv_color_hsv_t current_primary_hsv = {0, 0, 70};  // Store HSV for brightness control
    ColorTarget current_target_ = ColorTarget::Primary;
    std::vector<TargetButton> color_target_buttons_;
    std::vector<ThemePalette> quick_palettes_;
};
