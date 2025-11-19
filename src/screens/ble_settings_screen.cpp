#include "screens/ble_settings_screen.h"
#include "core/app_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include <BLEDevice.h>
#include <BLEServer.h>

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

    // BLE is enabled by default (started in main.cpp)
    ble_enabled = BLEDevice::getInitialized();
    if (ble_enabled) {
        lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
    }

    // Status Card
    status_card = create_card(content_container, "Stato Connessione", lv_color_hex(0x1a2332));

    status_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(status_label, "BLE inizializzato");

    clients_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(clients_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(clients_label, lv_color_hex(0xa0a0a0), 0);
    lv_label_set_text(clients_label, "Client connessi: 0");

    mac_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(mac_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mac_label, lv_color_hex(0x808080), 0);

    if (BLEDevice::getInitialized()) {
        std::string address = BLEDevice::getAddress().toString();
        lv_label_set_text_fmt(mac_label, "BLE Address: %s", address.c_str());
    } else {
        lv_label_set_text(mac_label, "BLE Address: ---");
    }

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
    lv_obj_add_event_cb(device_name_input, handleDeviceNameInput, LV_EVENT_VALUE_CHANGED, this);

    // Show current device name
    if (BLEDevice::getInitialized()) {
        // Note: BLEDevice doesn't provide a getter, so we'll use the default
        lv_textarea_set_text(device_name_input, "ESP32-S3");
    }

    lv_obj_t* uuid_label = lv_label_create(config_card);
    lv_label_set_text(uuid_label, "UUID Servizio:");
    lv_obj_set_style_text_font(uuid_label, &lv_font_montserrat_14, 0);

    service_uuid_label = lv_label_create(config_card);
    lv_obj_set_style_text_font(service_uuid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(service_uuid_label, lv_color_hex(0xc0c0c0), 0);
    lv_label_set_text(service_uuid_label, "6E400001-B5A3-F393-E0A9-E50E24DCCA9E");

    lv_obj_t* char_label = lv_label_create(config_card);
    lv_label_set_text(char_label, "UUID Characteristic:");
    lv_obj_set_style_text_font(char_label, &lv_font_montserrat_14, 0);

    char_uuid_label = lv_label_create(config_card);
    lv_obj_set_style_text_font(char_uuid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(char_uuid_label, lv_color_hex(0xc0c0c0), 0);
    lv_label_set_text(char_uuid_label, "6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

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

    // Advertising is on by default
    if (BLEDevice::getInitialized()) {
        lv_obj_add_state(advertising_switch, LV_STATE_CHECKED);
        is_advertising = true;
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
        "BLE permette la comunicazione wireless a basso consumo.\n\n"
        "Nota: Le modifiche al nome richiedono il riavvio del BLE.");

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

    if (!BLEDevice::getInitialized()) {
        lv_label_set_text(status_label, "○ BLE non inizializzato");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xa0a0a0), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Client connessi: ---");
        }
        return;
    }

    // Since we can't query connected clients from BLEDevice API,
    // we show status based on advertising state
    if (is_advertising) {
        lv_label_set_text(status_label, "◌ In attesa di connessioni...");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00ffff), 0);
    } else {
        lv_label_set_text(status_label, "○ Advertising disabilitato");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xffaa00), 0);
    }

    if (clients_label) {
        lv_label_set_text(clients_label, "Client connessi: Consultare i log");
    }
}

void BleSettingsScreen::handleEnableToggle(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    bool enabled = lv_obj_has_state(screen->enable_switch, LV_STATE_CHECKED);
    screen->ble_enabled = enabled;

    if (!enabled) {
        if (BLEDevice::getInitialized()) {
            BLEDevice::deinit();
            Logger::getInstance().info("[BLE] Disabled");
        }
    } else {
        Logger::getInstance().warn("[BLE] Riavvio richiesto per riabilitare BLE");
        // BLE requires restart to reinitialize properly
        if (screen->status_label) {
            lv_label_set_text(screen->status_label, "⚠ Riavvio richiesto");
            lv_obj_set_style_text_color(screen->status_label, lv_color_hex(0xffaa00), 0);
        }
    }

    screen->updateBleStatus();
}

void BleSettingsScreen::handleDeviceNameInput(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->updating_from_manager) return;

    const char* text = lv_textarea_get_text(screen->device_name_input);
    Logger::getInstance().infof("[BLE] Device name changed to: %s (requires restart)", text ? text : "");

    // TODO: Add to SettingsManager for persistence
    // Note: BLE device name change requires BLE restart
}

void BleSettingsScreen::handleAdvertisingToggle(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    bool enabled = lv_obj_has_state(screen->advertising_switch, LV_STATE_CHECKED);
    screen->is_advertising = enabled;

    if (!BLEDevice::getInitialized()) {
        Logger::getInstance().warn("[BLE] Cannot toggle advertising - BLE not initialized");
        return;
    }

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    if (pAdvertising) {
        if (enabled) {
            pAdvertising->start();
            Logger::getInstance().info("[BLE] Advertising started");
            if (screen->advertising_status_label) {
                lv_label_set_text(screen->advertising_status_label, "● Il dispositivo è visibile ad altri dispositivi BLE");
                lv_obj_set_style_text_color(screen->advertising_status_label, lv_color_hex(0x00ffff), 0);
            }
        } else {
            pAdvertising->stop();
            Logger::getInstance().info("[BLE] Advertising stopped");
            if (screen->advertising_status_label) {
                lv_label_set_text(screen->advertising_status_label, "○ Il dispositivo non è visibile");
                lv_obj_set_style_text_color(screen->advertising_status_label, lv_color_hex(0xa0a0a0), 0);
            }
        }
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
