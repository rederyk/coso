#include "screens/ble_settings_screen.h"
#include "core/app_manager.h"
#include "core/ble_hid_manager.h"
#include "core/keyboard_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"

namespace {
lv_obj_t* create_card(lv_obj_t* parent, const char* title, lv_color_t bg_color) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_style_bg_color(card, bg_color, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
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
} // namespace

BleSettingsScreen::~BleSettingsScreen() {
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
    if (status_timer) {
        lv_timer_del(status_timer);
        status_timer = nullptr;
    }
}

void BleSettingsScreen::build(lv_obj_t* parent) {
    if (!parent) return;

    SettingsManager& manager = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = manager.getSnapshot();
    BleHidManager& ble = BleHidManager::getInstance();

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
    lv_label_set_text(header_label, LV_SYMBOL_BLUETOOTH " BLE Settings");
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

    // Enable BLE Card
    enable_card = create_card(content_container, nullptr, lv_color_hex(0x1a2332));
    lv_obj_t* enable_row = lv_obj_create(enable_card);
    lv_obj_remove_style_all(enable_row);
    lv_obj_set_width(enable_row, lv_pct(100));
    lv_obj_set_height(enable_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(enable_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(enable_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(enable_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    enable_label = lv_label_create(enable_row);
    lv_label_set_text(enable_label, "Abilita BLE");
    lv_obj_set_style_text_font(enable_label, &lv_font_montserrat_16, 0);

    enable_switch = lv_switch_create(enable_row);
    lv_obj_add_event_cb(enable_switch, handleEnableToggle, LV_EVENT_VALUE_CHANGED, this);

    // BLE HID stato iniziale
    ble_enabled = ble.isEnabled();
    if (ble_enabled) {
        lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
    }

    // Status Card
    status_card = create_card(content_container, "Stato Connessione", lv_color_hex(0x1a2332));

    status_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(status_label, "BLE HID pronto");

    clients_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(clients_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(clients_label, lv_color_hex(0xa0a0a0), 0);
    lv_label_set_text(clients_label, "Client connessi: 0");

    mac_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mac_label, lv_color_hex(0x808080), 0);
    lv_label_set_text(mac_label, "BLE Address: ---");

    // Configuration Card
    config_card = create_card(content_container, "Configurazione", lv_color_hex(0x1a2332));

    lv_obj_t* name_label = lv_label_create(config_card);
    lv_label_set_text(name_label, "Nome Dispositivo:");
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);

    device_name_input = lv_textarea_create(config_card);
    lv_textarea_set_one_line(device_name_input, true);
    lv_textarea_set_max_length(device_name_input, 32);
    lv_textarea_set_placeholder_text(device_name_input, "ESP32-S3");
    lv_obj_set_width(device_name_input, lv_pct(100));

    // Aggiorna il nome solo quando l'utente chiude il campo (evita spam log)
    lv_obj_add_event_cb(device_name_input, handleDeviceNameInput, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_event_cb(device_name_input, [](lv_event_t* e) {
        lv_obj_t* ta = lv_event_get_target(e);
        KeyboardManager::getInstance().showForTextArea(ta);
    }, LV_EVENT_FOCUSED, nullptr);

    lv_textarea_set_text(device_name_input, ble.getDeviceName().c_str());
    last_device_name_ = ble.getDeviceName();

    lv_obj_t* uuid_label = lv_label_create(config_card);
    lv_label_set_text(uuid_label, "Servizio HID:");
    lv_obj_set_style_text_font(uuid_label, &lv_font_montserrat_14, 0);

    service_uuid_label = lv_label_create(config_card);
    lv_obj_set_style_text_font(service_uuid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(service_uuid_label, lv_color_hex(0xc0c0c0), 0);
    lv_label_set_text(service_uuid_label, "0x1812 (Keyboard + Mouse)");

    lv_obj_t* char_label = lv_label_create(config_card);
    lv_label_set_text(char_label, "Report ID:");
    lv_obj_set_style_text_font(char_label, &lv_font_montserrat_14, 0);

    char_uuid_label = lv_label_create(config_card);
    lv_obj_set_style_text_font(char_uuid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(char_uuid_label, lv_color_hex(0xc0c0c0), 0);
    lv_label_set_text(char_uuid_label, "Keyboard #1 / Mouse #2");

    // Advertising Card
    advertising_card = create_card(content_container, "Advertising", lv_color_hex(0x1a2332));

    lv_obj_t* adv_row = lv_obj_create(advertising_card);
    lv_obj_remove_style_all(adv_row);
    lv_obj_set_width(adv_row, lv_pct(100));
    lv_obj_set_height(adv_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(adv_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(adv_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(adv_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* adv_label = lv_label_create(adv_row);
    lv_label_set_text(adv_label, "Advertising Attivo");
    lv_obj_set_style_text_font(adv_label, &lv_font_montserrat_14, 0);

    advertising_switch = lv_switch_create(adv_row);
    lv_obj_add_event_cb(advertising_switch, handleAdvertisingToggle, LV_EVENT_VALUE_CHANGED, this);

    // Advertising stato iniziale
    is_advertising = ble.isAdvertising();
    if (is_advertising) {
        lv_obj_add_state(advertising_switch, LV_STATE_CHECKED);
    }

    advertising_status_label = lv_label_create(advertising_card);
    lv_obj_set_style_text_font(advertising_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(advertising_status_label, lv_color_hex(0x00ffff), 0);
    lv_label_set_text(advertising_status_label, "● Il dispositivo è visibile ad altri dispositivi BLE");

    // Info Card
    info_card = create_card(content_container, UI_SYMBOL_INFO " Informazioni", lv_color_hex(0x1a2332));

    info_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xa0a0a0), 0);
    lv_label_set_text(info_label,
        "Profilo HID (tastiera + mouse) basato su NimBLE.\n\n"
        "Nota: Le modifiche al nome richiedono riavvio dell'advertising.");

    // Apply theme
    applySnapshot(snapshot);
    applyThemeStyles(snapshot);

    // Create status update timer (every 2 seconds)
    status_timer = lv_timer_create(updateStatusTimer, 2000, this);

    // Register settings listener
    if (settings_listener_id == 0) {
        settings_listener_id = manager.addListener([this](SettingsManager::SettingKey, const SettingsSnapshot& snap) {
            if (!root) return;
            applySnapshot(snap);
        });
    }

    updateBleStatus();
}

void BleSettingsScreen::onShow() {
    Logger::getInstance().info("[BLE Settings] Screen shown");
    updateBleStatus();
}

void BleSettingsScreen::onHide() {
    Logger::getInstance().info("[BLE Settings] Screen hidden");
}

void BleSettingsScreen::applySnapshot(const SettingsSnapshot& snapshot) {
    updating_from_manager = true;
    // BLE settings could be added to SettingsSnapshot in the future
    updating_from_manager = false;
}

void BleSettingsScreen::applyThemeStyles(const SettingsSnapshot& snapshot) {
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
    }
    if (header_label) {
        lv_obj_set_style_text_color(header_label, accent, 0);
    }
    if (back_btn) {
        lv_obj_set_style_bg_color(back_btn, accent, 0);
    }
}

void BleSettingsScreen::updateBleStatus() {
    if (!status_label) return;

    BleHidManager& ble = BleHidManager::getInstance();
    ble_enabled = ble.isEnabled();
    bool initialized = ble.isInitialized();
    is_advertising = ble.isAdvertising();

    if (enable_switch) {
        if (ble_enabled) {
            lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(enable_switch, LV_STATE_CHECKED);
        }
    }
    if (advertising_switch) {
        if (is_advertising) {
            lv_obj_add_state(advertising_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(advertising_switch, LV_STATE_CHECKED);
        }
        if (!ble_enabled) {
            lv_obj_clear_state(advertising_switch, LV_STATE_CHECKED);
        }
    }

    if (!initialized) {
        lv_label_set_text(status_label, "○ BLE HID non inizializzato");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xa0a0a0), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Client connessi: ---");
        }
        if (mac_label) {
            lv_label_set_text(mac_label, "BLE Address: ---");
        }
        return;
    } else if (!ble_enabled) {
        lv_label_set_text(status_label, "○ BLE HID disabilitato");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xa0a0a0), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Client connessi: 0");
        }
        if (mac_label) {
            lv_label_set_text(mac_label, "BLE Address: ---");
        }
        return;
    }

    if (ble.isConnected()) {
        lv_label_set_text(status_label, "● Connesso a un host");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00ff80), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Client connessi: 1");
        }
    } else if (is_advertising) {
        lv_label_set_text(status_label, "◌ In attesa di connessioni...");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00ffff), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Client connessi: 0");
        }
    } else {
        lv_label_set_text(status_label, "○ Advertising disabilitato");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xffaa00), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Client connessi: 0");
        }
    }

    if (mac_label) {
        lv_label_set_text_fmt(mac_label, "BLE Address: %s", ble.getAddress().c_str());
    }

    if (advertising_status_label) {
        if (is_advertising) {
            lv_label_set_text(advertising_status_label, "● Il dispositivo è visibile ad altri dispositivi BLE");
            lv_obj_set_style_text_color(advertising_status_label, lv_color_hex(0x00ffff), 0);
        } else {
            lv_label_set_text(advertising_status_label, "○ Il dispositivo non è visibile");
            lv_obj_set_style_text_color(advertising_status_label, lv_color_hex(0xa0a0a0), 0);
        }
    }
}

void BleSettingsScreen::handleEnableToggle(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    bool enabled = lv_obj_has_state(screen->enable_switch, LV_STATE_CHECKED);
    screen->ble_enabled = enabled;

    BleHidManager& ble = BleHidManager::getInstance();
    ble.setEnabled(enabled);
    if (!enabled) {
        Logger::getInstance().info("[BLE] HID disabilitato");
    } else {
        Logger::getInstance().info("[BLE] HID abilitato (advertising attivo)");
    }

    screen->updateBleStatus();
}

void BleSettingsScreen::handleDeviceNameInput(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    const char* text = lv_textarea_get_text(screen->device_name_input);
    std::string name = text ? text : "";
    if (name.empty() || name == screen->last_device_name_) {
        return;
    }

    BleHidManager::getInstance().setDeviceName(name);
    screen->last_device_name_ = name;
    Logger::getInstance().infof("[BLE HID] Nome dispositivo impostato su: %s", name.c_str());
}

void BleSettingsScreen::handleAdvertisingToggle(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    bool enabled = lv_obj_has_state(screen->advertising_switch, LV_STATE_CHECKED);
    screen->is_advertising = enabled;

    BleHidManager& ble = BleHidManager::getInstance();
    if (!ble.isInitialized()) {
        Logger::getInstance().warn("[BLE HID] Impossibile gestire advertising: non inizializzato");
        screen->updateBleStatus();
        return;
    }

    if (enabled) {
        ble.startAdvertising();
        Logger::getInstance().info("[BLE HID] Advertising avviato");
    } else {
        ble.stopAdvertising();
        Logger::getInstance().info("[BLE HID] Advertising fermato");
    }

    screen->updateBleStatus();
}

void BleSettingsScreen::handleBackButton(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    // Navigate back to settings screen
    Logger::getInstance().info("[BLE Settings] Returning to Settings...");
    AppManager* app_manager = AppManager::getInstance();
    if (app_manager) {
        app_manager->launchApp("settings");
    }
}

void BleSettingsScreen::updateStatusTimer(lv_timer_t* timer) {
    auto* screen = static_cast<BleSettingsScreen*>(timer->user_data);
    if (!screen) return;

    screen->updateBleStatus();
}
