#pragma once

#include "core/screen.h"
#include "core/settings_manager.h"
#include <lvgl.h>
#include <string>
#include <vector>

class BleRemoteScreen : public Screen {
public:
    ~BleRemoteScreen() override;

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;

private:
    enum class Shortcut : uint8_t {
        Esc,
        Enter,
        Tab,
        Copy,
        Paste,
        SelectAll
    };

    void applyTheme(const SettingsSnapshot& snapshot);
    void updateLayout(bool landscape);
    void updateStatus();
    void setControlsEnabled(bool enabled);
    void dispatchMouse(int8_t dx, int8_t dy, int8_t wheel = 0, uint8_t buttons = 0);
    void dispatchClick(uint8_t buttons);
    void dispatchShortcut(Shortcut s);
    void sendTextFromField();
    bool canSendCommands() const;
    void refreshHint(const char* text);

    // Event callbacks
    static void statusTimerCb(lv_timer_t* timer);
    static void touchpadEventCb(lv_event_t* e);
    static void clickButtonCb(lv_event_t* e);
    static void wheelButtonCb(lv_event_t* e);
    static void shortcutButtonCb(lv_event_t* e);
    static void keyboardToggleCb(lv_event_t* e);
    static void sendTextCb(lv_event_t* e);

    // UI nodes
    lv_obj_t* header_container = nullptr;
    lv_obj_t* header_label = nullptr;
    lv_obj_t* status_chip = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* status_body_label = nullptr;
    lv_obj_t* hint_label = nullptr;
    lv_obj_t* content_container = nullptr;
    lv_obj_t* status_card = nullptr;
    lv_obj_t* mouse_card = nullptr;
    lv_obj_t* keyboard_card = nullptr;
    lv_obj_t* shortcuts_card = nullptr;
    lv_obj_t* touchpad_area = nullptr;
    lv_obj_t* keyboard_textarea = nullptr;
    lv_obj_t* keyboard_toggle_btn = nullptr;
    lv_obj_t* send_text_btn = nullptr;

    std::vector<lv_obj_t*> control_buttons;

    lv_timer_t* status_timer = nullptr;
    uint32_t settings_listener_id = 0;
    bool landscape_layout = true;
    bool controls_enabled = false;
};
