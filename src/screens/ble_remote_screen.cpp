#include "screens/ble_remote_screen.h"

#include "core/app_manager.h"
#include "core/ble_hid_manager.h"
#include "core/keyboard_manager.h"
#include "screens/ble_keyboard_screen.h"
#include "screens/ble_manager.h"
#include "screens/ble_mouse_screen.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include <cstdint>
#include <cstdio>
#include <string>

namespace {
constexpr uint8_t MOUSE_BTN_LEFT = 1 << 0;
constexpr uint8_t MOUSE_BTN_RIGHT = 1 << 1;
constexpr uint8_t MOUSE_BTN_MIDDLE = 1 << 2;

// HID Modifier Keys
constexpr uint8_t MOD_CTRL = 0x01;
constexpr uint8_t MOD_SHIFT = 0x02;
constexpr uint8_t MOD_ALT = 0x04;
constexpr uint8_t MOD_SUPER = 0x08;  // GUI/Windows/Command key

// HID Letter Keys
constexpr uint8_t KEY_A = 0x04;
constexpr uint8_t KEY_C = 0x06;
constexpr uint8_t KEY_F = 0x09;
constexpr uint8_t KEY_S = 0x16;
constexpr uint8_t KEY_T = 0x17;
constexpr uint8_t KEY_V = 0x19;
constexpr uint8_t KEY_Y = 0x1C;
constexpr uint8_t KEY_Z = 0x1D;

// HID Special Keys
constexpr uint8_t KEY_ENTER = 0x28;
constexpr uint8_t KEY_ESC = 0x29;
constexpr uint8_t KEY_BACKSPACE = 0x2A;
constexpr uint8_t KEY_TAB = 0x2B;
constexpr uint8_t KEY_SPACE = 0x2C;

// HID Navigation & Editing Keys
constexpr uint8_t KEY_INSERT = 0x49;
constexpr uint8_t KEY_HOME = 0x4A;
constexpr uint8_t KEY_PAGEUP = 0x4B;
constexpr uint8_t KEY_DELETE = 0x4C;
constexpr uint8_t KEY_END = 0x4D;
constexpr uint8_t KEY_PAGEDOWN = 0x4E;
constexpr uint8_t KEY_RIGHT = 0x4F;
constexpr uint8_t KEY_LEFT = 0x50;
constexpr uint8_t KEY_DOWN = 0x51;
constexpr uint8_t KEY_UP = 0x52;

// HID Function Keys
constexpr uint8_t KEY_F1 = 0x3A;
constexpr uint8_t KEY_F2 = 0x3B;
constexpr uint8_t KEY_F3 = 0x3C;
constexpr uint8_t KEY_F4 = 0x3D;
constexpr uint8_t KEY_F5 = 0x3E;
constexpr uint8_t KEY_F6 = 0x3F;
constexpr uint8_t KEY_F7 = 0x40;
constexpr uint8_t KEY_F8 = 0x41;
constexpr uint8_t KEY_F9 = 0x42;
constexpr uint8_t KEY_F10 = 0x43;
constexpr uint8_t KEY_F11 = 0x44;
constexpr uint8_t KEY_F12 = 0x45;

lv_obj_t* create_card(lv_obj_t* parent, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 10, 0);

    if (title) {
        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    }

    return card;
}

lv_obj_t* create_pill(lv_obj_t* parent, const char* text) {
    lv_obj_t* pill = lv_obj_create(parent);
    lv_obj_remove_style_all(pill);
    lv_obj_set_height(pill, LV_SIZE_CONTENT);
    lv_obj_set_width(pill, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(pill, 6, 0);
    lv_obj_set_style_radius(pill, 12, 0);
    lv_obj_set_layout(pill, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_row(pill, 0, 0);
    lv_obj_set_style_pad_column(pill, 6, 0);

    lv_obj_t* lbl = lv_label_create(pill);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    return pill;
}

lv_obj_t* create_chip_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 32);  // Reduced from 40 to 32
    lv_obj_set_style_radius(btn, 8, 0);         // Reduced from 10 to 8
    lv_obj_set_style_pad_all(btn, 6, 0);        // Reduced from 8 to 6
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn, 4, 0);     // Reduced from 6 to 4

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}
} // namespace

BleRemoteScreen::~BleRemoteScreen() {
    if (status_timer) {
        lv_timer_del(status_timer);
        status_timer = nullptr;
    }
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void BleRemoteScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();
    landscape_layout = snapshot.landscapeLayout;

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_pad_row(root, 10, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);

    // Header
    header_container = lv_obj_create(root);
    lv_obj_remove_style_all(header_container);
    lv_obj_set_width(header_container, lv_pct(100));
    lv_obj_set_height(header_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(header_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header_container, 4, 0);
    lv_obj_set_style_pad_column(header_container, 8, 0);

    header_label = lv_label_create(header_container);
    lv_label_set_text(header_label, LV_SYMBOL_BLUETOOTH " BLE Remote");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_20, 0);

    lv_obj_t* spacer = lv_obj_create(header_container);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);

    fullscreen_kb_btn = lv_btn_create(header_container);
    lv_obj_set_size(fullscreen_kb_btn, 40, 40);
    lv_obj_add_event_cb(fullscreen_kb_btn, fullscreen_kb_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* kb_label = lv_label_create(fullscreen_kb_btn);
    lv_label_set_text(kb_label, LV_SYMBOL_KEYBOARD);
    lv_obj_center(kb_label);

    fullscreen_mouse_btn = lv_btn_create(header_container);
    lv_obj_set_size(fullscreen_mouse_btn, 40, 40);
    lv_obj_add_event_cb(fullscreen_mouse_btn, fullscreen_mouse_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* mouse_label = lv_label_create(fullscreen_mouse_btn);
    lv_label_set_text(mouse_label, LV_SYMBOL_DRIVE); // Using drive as a mouse placeholder
    lv_obj_center(mouse_label);

    status_chip = create_pill(header_container, "Stato");
    status_label = lv_obj_get_child(status_chip, 0);

    // Content
    content_container = lv_obj_create(root);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_width(content_container, lv_pct(100));
    lv_obj_set_height(content_container, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_pad_row(content_container, 10, 0);
    lv_obj_set_style_pad_column(content_container, 8, 0);
    lv_obj_set_layout(content_container, LV_LAYOUT_FLEX);

    // Status card
    status_card = create_card(content_container, "Stato");
    lv_obj_set_style_pad_row(status_card, 4, 0);
    status_body_label = lv_label_create(status_card);
    lv_label_set_long_mode(status_body_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(status_body_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(status_body_label, "Connessione HID");

    // Target selection
    target_label = lv_label_create(status_card);
    lv_label_set_text(target_label, "Target host:");
    lv_obj_set_style_text_font(target_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(target_label, 8, 0);

    target_row = lv_obj_create(status_card);
    lv_obj_remove_style_all(target_row);
    lv_obj_set_width(target_row, lv_pct(100));
    lv_obj_set_layout(target_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(target_row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(target_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(target_row, 6, 0);
    lv_obj_set_style_pad_row(target_row, 6, 0);

    hint_label = lv_label_create(status_card);
    lv_label_set_long_mode(hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint_label, lv_pct(100));
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(hint_label, "Trascina per muovere il mouse, oppure usa i pulsanti rapidi.");

    // Touchpad card
    mouse_card = create_card(content_container, "Touchpad & click");
    touchpad_area = lv_obj_create(mouse_card);
    lv_obj_remove_style_all(touchpad_area);
    lv_obj_set_width(touchpad_area, lv_pct(100));
    lv_obj_set_height(touchpad_area, 150);
    lv_obj_set_style_radius(touchpad_area, 12, 0);
    lv_obj_set_style_pad_all(touchpad_area, 10, 0);
    lv_obj_set_style_border_width(touchpad_area, 0, 0);
    lv_obj_add_flag(touchpad_area, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(touchpad_area, touchpadEventCb, LV_EVENT_ALL, this);

    lv_obj_t* pad_hint = lv_label_create(touchpad_area);
    lv_label_set_text(pad_hint, LV_SYMBOL_LEFT " Trascina per muovere\nTap: click sinistro\nDoppio tap: destro");
    lv_obj_set_style_text_font(pad_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(pad_hint, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* click_row = lv_obj_create(mouse_card);
    lv_obj_remove_style_all(click_row);
    lv_obj_set_width(click_row, lv_pct(100));
    lv_obj_set_layout(click_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(click_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(click_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(click_row, 6, 0);

    lv_obj_t* left_btn = create_chip_button(click_row, "Sinistro", clickButtonCb, this);
    lv_obj_set_user_data(left_btn, (void*)(uintptr_t)MOUSE_BTN_LEFT);

    lv_obj_t* middle_btn = create_chip_button(click_row, "Centrale", clickButtonCb, this);
    lv_obj_set_user_data(middle_btn, (void*)(uintptr_t)MOUSE_BTN_MIDDLE);

    lv_obj_t* right_btn = create_chip_button(click_row, "Destro", clickButtonCb, this);
    lv_obj_set_user_data(right_btn, (void*)(uintptr_t)MOUSE_BTN_RIGHT);

    lv_obj_t* wheel_row = lv_obj_create(mouse_card);
    lv_obj_remove_style_all(wheel_row);
    lv_obj_set_width(wheel_row, lv_pct(100));
    lv_obj_set_layout(wheel_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wheel_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wheel_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(wheel_row, 6, 0);

    lv_obj_t* scroll_up = create_chip_button(wheel_row, LV_SYMBOL_UP " Scroll", wheelButtonCb, this);
    lv_obj_set_user_data(scroll_up, (void*)(intptr_t)1);

    lv_obj_t* scroll_down = create_chip_button(wheel_row, LV_SYMBOL_DOWN " Scroll", wheelButtonCb, this);
    lv_obj_set_user_data(scroll_down, (void*)(intptr_t)-1);

    // Keyboard card
    keyboard_card = create_card(content_container, "Tastiera");
    keyboard_textarea = lv_textarea_create(keyboard_card);
    lv_textarea_set_one_line(keyboard_textarea, false);
    lv_textarea_set_max_length(keyboard_textarea, 96);
    lv_textarea_set_placeholder_text(keyboard_textarea, "Digita qui e premi Invia");
    lv_obj_set_width(keyboard_textarea, lv_pct(100));
    lv_obj_set_height(keyboard_textarea, 70);
    lv_obj_add_event_cb(keyboard_textarea, [](lv_event_t* e) {
        auto* ta = lv_event_get_target(e);
        if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
            KeyboardManager::getInstance().showForTextArea(ta);
        }
    }, LV_EVENT_ALL, nullptr);

    lv_obj_t* keyboard_row = lv_obj_create(keyboard_card);
    lv_obj_remove_style_all(keyboard_row);
    lv_obj_set_width(keyboard_row, lv_pct(100));
    lv_obj_set_layout(keyboard_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(keyboard_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(keyboard_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(keyboard_row, 8, 0);

    keyboard_toggle_btn = create_chip_button(keyboard_row, LV_SYMBOL_KEYBOARD " Tastiera", keyboardToggleCb, this);
    send_text_btn = create_chip_button(keyboard_row, LV_SYMBOL_UPLOAD " Invia", sendTextCb, this);

    // Shortcuts card
    shortcuts_card = create_card(content_container, "Scorciatoie");
    lv_obj_t* shortcuts_row = lv_obj_create(shortcuts_card);
    lv_obj_remove_style_all(shortcuts_row);
    lv_obj_set_width(shortcuts_row, lv_pct(100));
    lv_obj_set_height(shortcuts_row, LV_SIZE_CONTENT); // Allow content to expand
    lv_obj_set_layout(shortcuts_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(shortcuts_row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(shortcuts_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(shortcuts_row, 4, 0);
    lv_obj_set_style_pad_column(shortcuts_row, 4, 0);

    auto add_shortcut = [&](const char* label, Shortcut s) {
        lv_obj_t* btn = create_chip_button(shortcuts_row, label, shortcutButtonCb, this);
        lv_obj_set_user_data(btn, (void*)(uintptr_t)s);
        control_buttons.push_back(btn);
    };

    // Basic keys
    add_shortcut("Esc", Shortcut::Esc);
    add_shortcut("Tab", Shortcut::Tab);
    add_shortcut("Enter", Shortcut::Enter);
    add_shortcut("Del", Shortcut::Delete);
    add_shortcut("Ins", Shortcut::Insert);

    // Modifier keys
    add_shortcut("Ctrl", Shortcut::Ctrl);
    add_shortcut("Alt", Shortcut::Alt);
    add_shortcut("Super", Shortcut::Super);

    // Navigation
    add_shortcut(LV_SYMBOL_UP, Shortcut::ArrowUp);
    add_shortcut(LV_SYMBOL_DOWN, Shortcut::ArrowDown);
    add_shortcut(LV_SYMBOL_LEFT, Shortcut::ArrowLeft);
    add_shortcut(LV_SYMBOL_RIGHT, Shortcut::ArrowRight);
    add_shortcut("Home", Shortcut::Home);
    add_shortcut("End", Shortcut::End);
    add_shortcut("PgUp", Shortcut::PageUp);
    add_shortcut("PgDn", Shortcut::PageDown);

    // Function keys (F1-F6 most commonly used)
    add_shortcut("F1", Shortcut::F1);
    add_shortcut("F2", Shortcut::F2);
    add_shortcut("F3", Shortcut::F3);
    add_shortcut("F4", Shortcut::F4);
    add_shortcut("F5", Shortcut::F5);
    add_shortcut("F6", Shortcut::F6);
    add_shortcut("F7", Shortcut::F7);
    add_shortcut("F8", Shortcut::F8);
    add_shortcut("F9", Shortcut::F9);
    add_shortcut("F10", Shortcut::F10);
    add_shortcut("F11", Shortcut::F11);
    add_shortcut("F12", Shortcut::F12);

    // Common combinations
    add_shortcut("Ctrl+C", Shortcut::Copy);
    add_shortcut("Ctrl+V", Shortcut::Paste);
    add_shortcut("Ctrl+A", Shortcut::SelectAll);
    add_shortcut("Ctrl+Z", Shortcut::CtrlZ);
    add_shortcut("Ctrl+Y", Shortcut::CtrlY);
    add_shortcut("Ctrl+F", Shortcut::CtrlF);
    add_shortcut("Ctrl+S", Shortcut::CtrlS);

    // Terminal & System shortcuts
    add_shortcut("C+S+C", Shortcut::CtrlShiftC);
    add_shortcut("C+S+V", Shortcut::CtrlShiftV);
    add_shortcut("Alt+Tab", Shortcut::AltTab);
    add_shortcut(LV_SYMBOL_KEYBOARD " Super+Space", Shortcut::SuperSpace);
    add_shortcut(LV_SYMBOL_SETTINGS " C+A+T", Shortcut::CtrlAltT);

    // Track buttons for enabling/disabling
    control_buttons.push_back(left_btn);
    control_buttons.push_back(middle_btn);
    control_buttons.push_back(right_btn);
    control_buttons.push_back(scroll_up);
    control_buttons.push_back(scroll_down);
    control_buttons.push_back(keyboard_toggle_btn);
    control_buttons.push_back(send_text_btn);
    control_buttons.push_back(touchpad_area);

    applyTheme(snapshot);
    updateLayout(snapshot.landscapeLayout);
    updateStatus();

    // Status refresh
    status_timer = lv_timer_create(statusTimerCb, 1200, this);

    // Listen for palette/orientation changes
    settings_listener_id = settings.addListener([this](SettingsManager::SettingKey key, const SettingsSnapshot& snap) {
        if (key == SettingsManager::SettingKey::LayoutOrientation) {
            updateLayout(snap.landscapeLayout);
        }
        applyTheme(snap);
    });
}

void BleRemoteScreen::onShow() {
    updateStatus();
}

void BleRemoteScreen::onHide() {
    KeyboardManager::getInstance().hide();
}

void BleRemoteScreen::applyTheme(const SettingsSnapshot& snapshot) {
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);
    lv_color_t card = lv_color_hex(snapshot.cardColor);
    lv_color_t text = lv_color_hex(0xffffff);
    lv_color_t subtle = lv_color_mix(accent, text, LV_OPA_30);
    lv_color_t highlight = lv_color_mix(accent, card, LV_OPA_60);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    }
    if (header_label) {
        lv_obj_set_style_text_color(header_label, accent, 0);
    }
    if (status_chip) {
        lv_obj_set_style_bg_color(status_chip, lv_color_mix(accent, primary, LV_OPA_50), 0);
        lv_obj_set_style_text_color(status_chip, text, 0);
    }

    auto style_card = [&](lv_obj_t* card_obj) {
        if (!card_obj) return;
        lv_obj_set_style_bg_color(card_obj, card, 0);
        lv_obj_set_style_bg_opa(card_obj, LV_OPA_80, 0);
        lv_obj_set_style_radius(card_obj, snapshot.borderRadius, 0);
        lv_obj_set_style_shadow_width(card_obj, 8, 0);
        lv_obj_set_style_shadow_opa(card_obj, LV_OPA_20, 0);
        lv_obj_set_style_shadow_color(card_obj, lv_color_mix(accent, lv_color_hex(0x000000), LV_OPA_40), 0);
    };

    style_card(status_card);
    style_card(mouse_card);
    style_card(keyboard_card);
    style_card(shortcuts_card);

    if (hint_label) {
        lv_obj_set_style_text_color(hint_label, subtle, 0);
    }

    if (touchpad_area) {
        lv_obj_set_style_bg_color(touchpad_area, lv_color_mix(card, accent, LV_OPA_30), 0);
        lv_obj_set_style_bg_opa(touchpad_area, LV_OPA_COVER, 0);
    }
    if (status_body_label) {
        lv_obj_set_style_text_color(status_body_label, text, 0);
    }

    auto style_btn = [&](lv_obj_t* btn) {
        if (!btn) return;
        lv_obj_set_style_bg_color(btn, highlight, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, text, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, snapshot.borderRadius / 2 + 6, LV_PART_MAIN);

        // Style for checked state
        lv_obj_set_style_bg_color(btn, accent, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    };
    for (auto* btn : control_buttons) {
        style_btn(btn);
    }
    for (auto* btn : target_buttons) {
        style_btn(btn);
    }

    if (target_label) {
        lv_obj_set_style_text_color(target_label, text, 0);
    }

    if (keyboard_textarea) {
        lv_obj_set_style_bg_color(keyboard_textarea, lv_color_mix(card, accent, LV_OPA_20), LV_PART_MAIN);
        lv_obj_set_style_text_color(keyboard_textarea, text, LV_PART_MAIN);
        lv_obj_set_style_border_width(keyboard_textarea, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(keyboard_textarea, snapshot.borderRadius / 2, LV_PART_MAIN);
    }
}

void BleRemoteScreen::updateLayout(bool landscape) {
    landscape_layout = landscape;
    if (!content_container) return;

    lv_obj_set_flex_flow(content_container, landscape ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);

    auto set_card_width = [&](lv_obj_t* card) {
        if (!card) return;
        lv_obj_set_width(card, landscape ? lv_pct(48) : lv_pct(100));
    };
    set_card_width(status_card);
    set_card_width(mouse_card);
    set_card_width(keyboard_card);
    set_card_width(shortcuts_card);
}

void BleRemoteScreen::updateStatus() {
    BleHidManager& ble = BleHidManager::getInstance();
    bool ready = ble.isInitialized() && ble.isEnabled();
    bool connected = ble.getConnectedCount() > 0;

    std::string text = "BLE non inizializzato";
    lv_color_t chip_color = lv_color_hex(0xffaa00);
    if (!ble.isInitialized()) {
        text = "BLE non inizializzato";
        controls_enabled = false;
    } else if (!ble.isEnabled()) {
        text = "BLE disabilitato";
        chip_color = lv_color_hex(0x888888);
        controls_enabled = false;
    } else if (connected) {
        text = "Connesso a " + std::to_string(ble.getConnectedCount()) + " host";
        chip_color = lv_color_hex(0x00c853);
        controls_enabled = true;
    } else if (ble.isAdvertising()) {
        text = "In advertising...";
        chip_color = lv_color_hex(0xffc400);
        controls_enabled = false;
    } else {
        text = "Pronto (nessun host)";
        chip_color = lv_color_hex(0x8bc34a);
        controls_enabled = true; // allow testing layout even se non connesso
    }

    if (status_label) {
        lv_label_set_text(status_label, text.c_str());
        if (status_chip) {
            lv_obj_set_style_bg_color(status_chip, chip_color, 0);
        }
    }
    if (status_body_label) {
        lv_label_set_text(status_body_label, text.c_str());
    }

    updateTargetButtons();
    setControlsEnabled(controls_enabled && ready);
}

void BleRemoteScreen::updateTargetButtons() {
    if (!target_row) return;

    // Clear existing target buttons
    for (auto* btn : target_buttons) {
        if (btn) {
            lv_obj_del(btn);
        }
    }
    target_buttons.clear();

    BleHidManager& ble = BleHidManager::getInstance();
    auto connected_hosts = ble.getConnectedPeerAddresses();

    if (connected_hosts.empty()) {
        // No hosts connected - show message
        lv_obj_t* no_host_lbl = lv_label_create(target_row);
        lv_label_set_text(no_host_lbl, "Nessun host connesso");
        lv_obj_set_style_text_font(no_host_lbl, &lv_font_montserrat_14, 0);
        target_buttons.push_back(no_host_lbl);
        return;
    }

    // Add "Tutti" button if multiple hosts
    if (connected_hosts.size() > 1) {
        lv_obj_t* all_btn = create_chip_button(target_row, "Tutti", targetButtonCb, this);
        lv_obj_set_user_data(all_btn, nullptr); // nullptr = ALL
        target_buttons.push_back(all_btn);

        if (current_target == BleHidTarget::ALL) {
            lv_obj_add_state(all_btn, LV_STATE_CHECKED);
        }
    }

    // Add button for each connected host
    for (const auto& mac : connected_hosts) {
        // Shorten MAC address for display (last 8 chars)
        std::string short_mac = mac.length() > 8 ? mac.substr(mac.length() - 8) : mac;

        lv_obj_t* host_btn = create_chip_button(target_row, short_mac.c_str(), targetButtonCb, this);

        // Store MAC address in button
        char* mac_copy = new char[mac.length() + 1];
        strcpy(mac_copy, mac.c_str());
        lv_obj_set_user_data(host_btn, mac_copy);

        target_buttons.push_back(host_btn);

        // Check if this is the selected host
        if (selected_host_mac == mac) {
            lv_obj_add_state(host_btn, LV_STATE_CHECKED);
        }
    }

    // Apply theme to new buttons
    SettingsManager& settings = SettingsManager::getInstance();
    applyTheme(settings.getSnapshot());
}

void BleRemoteScreen::setControlsEnabled(bool enabled) {
    for (auto* btn : control_buttons) {
        if (!btn) continue;
        if (enabled) {
            lv_obj_clear_state(btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
    }
    if (keyboard_textarea) {
        if (enabled) {
            lv_obj_clear_state(keyboard_textarea, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(keyboard_textarea, LV_STATE_DISABLED);
        }
    }
}

bool BleRemoteScreen::canSendCommands() const {
    BleHidManager& ble = BleHidManager::getInstance();
    return ble.isInitialized() && ble.isEnabled() && ble.isConnected();
}

void BleRemoteScreen::refreshHint(const char* text) {
    if (hint_label && text) {
        lv_label_set_text(hint_label, text);
    }
}

void BleRemoteScreen::dispatchMouse(int8_t dx, int8_t dy, int8_t wheel, uint8_t buttons) {
    char buf[96];
    snprintf(buf, sizeof(buf), "Δ x:%d y:%d wheel:%d btn:%u", dx, dy, wheel, buttons);
    refreshHint(buf);

    if (!canSendCommands()) {
        Logger::getInstance().debug("[BLE Remote] Mouse gesture ignorato: host non connesso");
        return;
    }

    BleManager::getInstance().sendMouseMove(dx, dy, wheel, buttons, current_target, selected_host_mac);
}

void BleRemoteScreen::dispatchClick(uint8_t buttons) {
    dispatchMouse(0, 0, 0, buttons);
    if (!canSendCommands()) {
        return;
    }
    BleManager::getInstance().mouseClick(buttons, current_target, selected_host_mac);
}

void BleRemoteScreen::dispatchShortcut(Shortcut s) {
    const char* name = "Shortcut";
    uint8_t keycode = 0;
    uint8_t modifier = 0;

    switch (s) {
        // Basic keys
        case Shortcut::Esc: name = "Esc"; keycode = KEY_ESC; break;
        case Shortcut::Enter: name = "Enter"; keycode = KEY_ENTER; break;
        case Shortcut::Tab: name = "Tab"; keycode = KEY_TAB; break;
        case Shortcut::Delete: name = "Delete"; keycode = KEY_DELETE; break;
        case Shortcut::Insert: name = "Insert"; keycode = KEY_INSERT; break;

        // Modifier keys
        case Shortcut::Ctrl: name = "Ctrl"; modifier = MOD_CTRL; break;
        case Shortcut::Alt: name = "Alt"; modifier = MOD_ALT; break;
        case Shortcut::Super: name = "Super"; modifier = MOD_SUPER; break;

        // Navigation keys
        case Shortcut::Home: name = "Home"; keycode = KEY_HOME; break;
        case Shortcut::End: name = "End"; keycode = KEY_END; break;
        case Shortcut::PageUp: name = "Page Up"; keycode = KEY_PAGEUP; break;
        case Shortcut::PageDown: name = "Page Down"; keycode = KEY_PAGEDOWN; break;
        case Shortcut::ArrowUp: name = "Arrow Up"; keycode = KEY_UP; break;
        case Shortcut::ArrowDown: name = "Arrow Down"; keycode = KEY_DOWN; break;
        case Shortcut::ArrowLeft: name = "Arrow Left"; keycode = KEY_LEFT; break;
        case Shortcut::ArrowRight: name = "Arrow Right"; keycode = KEY_RIGHT; break;

        // Function keys
        case Shortcut::F1: name = "F1"; keycode = KEY_F1; break;
        case Shortcut::F2: name = "F2"; keycode = KEY_F2; break;
        case Shortcut::F3: name = "F3"; keycode = KEY_F3; break;
        case Shortcut::F4: name = "F4"; keycode = KEY_F4; break;
        case Shortcut::F5: name = "F5"; keycode = KEY_F5; break;
        case Shortcut::F6: name = "F6"; keycode = KEY_F6; break;
        case Shortcut::F7: name = "F7"; keycode = KEY_F7; break;
        case Shortcut::F8: name = "F8"; keycode = KEY_F8; break;
        case Shortcut::F9: name = "F9"; keycode = KEY_F9; break;
        case Shortcut::F10: name = "F10"; keycode = KEY_F10; break;
        case Shortcut::F11: name = "F11"; keycode = KEY_F11; break;
        case Shortcut::F12: name = "F12"; keycode = KEY_F12; break;

        // Common combinations
        case Shortcut::Copy: name = "Ctrl+C"; keycode = KEY_C; modifier = MOD_CTRL; break;
        case Shortcut::Paste: name = "Ctrl+V"; keycode = KEY_V; modifier = MOD_CTRL; break;
        case Shortcut::SelectAll: name = "Ctrl+A"; keycode = KEY_A; modifier = MOD_CTRL; break;
        case Shortcut::CtrlZ: name = "Ctrl+Z"; keycode = KEY_Z; modifier = MOD_CTRL; break;
        case Shortcut::CtrlY: name = "Ctrl+Y"; keycode = KEY_Y; modifier = MOD_CTRL; break;
        case Shortcut::CtrlF: name = "Ctrl+F"; keycode = KEY_F; modifier = MOD_CTRL; break;
        case Shortcut::CtrlS: name = "Ctrl+S"; keycode = KEY_S; modifier = MOD_CTRL; break;

        // Special combinations
        case Shortcut::SuperSpace: name = "Super+Space"; keycode = KEY_SPACE; modifier = MOD_SUPER; break;
        case Shortcut::CtrlShiftC: name = "Ctrl+Shift+C"; keycode = KEY_C; modifier = MOD_CTRL | MOD_SHIFT; break;
        case Shortcut::CtrlShiftV: name = "Ctrl+Shift+V"; keycode = KEY_V; modifier = MOD_CTRL | MOD_SHIFT; break;
        case Shortcut::AltTab: name = "Alt+Tab"; keycode = KEY_TAB; modifier = MOD_ALT; break;
        case Shortcut::CtrlAltT: name = "Ctrl+Alt+T"; keycode = KEY_T; modifier = MOD_CTRL | MOD_ALT; break;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "Scorciatoia: %s", name);
    refreshHint(buf);

    if (!canSendCommands()) {
        return;
    }

    BleManager::getInstance().sendKey(keycode, modifier, current_target, selected_host_mac);
}

void BleRemoteScreen::sendTextFromField() {
    if (!keyboard_textarea) return;
    const char* text = lv_textarea_get_text(keyboard_textarea);
    if (!text || text[0] == '\0') {
        refreshHint("Nessun testo da inviare");
        return;
    }

    refreshHint("Invio testo...");
    if (!canSendCommands()) {
        Logger::getInstance().info("[BLE Remote] Testo pronto, ma host non connesso");
        return;
    }

    BleManager::getInstance().sendText(text, current_target, selected_host_mac);
}

void BleRemoteScreen::statusTimerCb(lv_timer_t* timer) {
    auto* screen = static_cast<BleRemoteScreen*>(timer->user_data);
    if (!screen) return;
    screen->updateStatus();
}

void BleRemoteScreen::touchpadEventCb(lv_event_t* e) {
    auto* screen = static_cast<BleRemoteScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) {
        return;
    }

    lv_indev_t* indev = lv_event_get_indev(e);
    if (!indev) return;

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    if (code == LV_EVENT_PRESSING) {
        screen->dispatchMouse(static_cast<int8_t>(vect.x), static_cast<int8_t>(vect.y));
    } else if (code == LV_EVENT_RELEASED) {
        screen->refreshHint("Touch rilasciato");
    }
}

void BleRemoteScreen::clickButtonCb(lv_event_t* e) {
    auto* screen = static_cast<BleRemoteScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    uint8_t buttons = static_cast<uint8_t>((uintptr_t)lv_obj_get_user_data(lv_event_get_target(e)));
    screen->dispatchClick(buttons);
}

void BleRemoteScreen::wheelButtonCb(lv_event_t* e) {
    auto* screen = static_cast<BleRemoteScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    int dir = static_cast<int>((intptr_t)lv_obj_get_user_data(lv_event_get_target(e)));
    screen->dispatchMouse(0, 0, dir > 0 ? 10 : -10, 0);
}

void BleRemoteScreen::shortcutButtonCb(lv_event_t* e) {
    auto* screen = static_cast<BleRemoteScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    Shortcut s = static_cast<Shortcut>((uintptr_t)lv_obj_get_user_data(lv_event_get_target(e)));
    screen->dispatchShortcut(s);
}

void BleRemoteScreen::keyboardToggleCb(lv_event_t* e) {
    auto* screen = static_cast<BleRemoteScreen*>(lv_event_get_user_data(e));
    if (!screen || !screen->keyboard_textarea) return;

    KeyboardManager& km = KeyboardManager::getInstance();
    if (km.isVisible()) {
        km.hide();
    } else {
        km.showForTextArea(screen->keyboard_textarea);
    }
}

void BleRemoteScreen::sendTextCb(lv_event_t* e) {
    auto* screen = static_cast<BleRemoteScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    screen->sendTextFromField();
}

void BleRemoteScreen::targetButtonCb(lv_event_t* e) {
    auto* screen = static_cast<BleRemoteScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    lv_obj_t* clicked_btn = lv_event_get_target(e);
    void* user_data = lv_obj_get_user_data(clicked_btn);

    // Clear all button states
    for (auto* btn : screen->target_buttons) {
        if (btn) {
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
        }
    }

    // Set clicked button as checked
    lv_obj_add_state(clicked_btn, LV_STATE_CHECKED);

    if (user_data == nullptr) {
        // "Tutti" button clicked
        screen->current_target = BleHidTarget::ALL;
        screen->selected_host_mac.clear();
        screen->refreshHint("Invio a tutti gli host");
    } else {
        // Specific host button clicked
        const char* mac = static_cast<const char*>(user_data);
        screen->selected_host_mac = mac;
        // When a specific MAC is set, the target enum is ignored
        screen->current_target = BleHidTarget::ALL; // Doesn't matter, specific_mac takes precedence

        // Shorten MAC for display
        std::string short_mac = std::string(mac);
        if (short_mac.length() > 8) {
            short_mac = short_mac.substr(short_mac.length() - 8);
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "Target: %s", short_mac.c_str());
        screen->refreshHint(buf);
    }
}

void BleRemoteScreen::fullscreen_kb_cb(lv_event_t* e) {
    AppManager::getInstance()->launchApp("ble_keyboard");
}

void BleRemoteScreen::fullscreen_mouse_cb(lv_event_t* e) {
    AppManager::getInstance()->launchApp("ble_mouse");
}
