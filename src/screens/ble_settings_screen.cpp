#include "screens/ble_settings_screen.h"
#include "screens/ble_manager.h"
#include "core/app_manager.h"
#include "core/ble_hid_manager.h"
#include "core/keyboard_manager.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"

namespace {
lv_obj_t* create_card(lv_obj_t* parent, const char* title, lv_color_t bg_color) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, bg_color, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    if (title) {
        lv_obj_t* title_lbl = lv_label_create(card);
        lv_label_set_text(title_lbl, title);
        lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
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
    lv_obj_set_style_pad_all(root, 4, 0);
    lv_obj_set_style_pad_row(root, 4, 0);

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
    lv_obj_set_size(back_btn, 36, 36);
    lv_obj_add_event_cb(back_btn, handleBackButton, LV_EVENT_CLICKED, this);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    header_label = lv_label_create(header_container);
    lv_label_set_text(header_label, LV_SYMBOL_BLUETOOTH " BLE");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_left(header_label, 8, 0);

    // Content container
    content_container = lv_obj_create(root);
    lv_obj_remove_style_all(content_container);
    lv_obj_set_width(content_container, lv_pct(100));
    lv_obj_set_height(content_container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(content_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_pad_row(content_container, 6, 0);

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
    lv_label_set_text(enable_label, "Abilita");
    lv_obj_set_style_text_font(enable_label, &lv_font_montserrat_14, 0);

    enable_switch = lv_switch_create(enable_row);
    lv_obj_add_event_cb(enable_switch, handleEnableToggle, LV_EVENT_VALUE_CHANGED, this);

    // BLE HID stato iniziale
    ble_enabled = ble.isEnabled();
    if (ble_enabled) {
        lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
    }

    // Status Card
    status_card = create_card(content_container, "Stato", lv_color_hex(0x1a2332));

    status_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(status_label, "Pronto");

    clients_label = lv_label_create(status_card);
    lv_obj_set_style_text_font(clients_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(clients_label, lv_color_hex(0xa0a0a0), 0);
    lv_label_set_text(clients_label, "Host: 0");

    // Configuration Card
    config_card = create_card(content_container, "Config", lv_color_hex(0x1a2332));

    lv_obj_t* name_label = lv_label_create(config_card);
    lv_label_set_text(name_label, "Nome:");
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

    // Advertising Card
    advertising_card = create_card(content_container, "Adv", lv_color_hex(0x1a2332));

    lv_obj_t* adv_row = lv_obj_create(advertising_card);
    lv_obj_remove_style_all(adv_row);
    lv_obj_set_width(adv_row, lv_pct(100));
    lv_obj_set_height(adv_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(adv_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(adv_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(adv_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* adv_label = lv_label_create(adv_row);
    lv_label_set_text(adv_label, "Visibile");
    lv_obj_set_style_text_font(adv_label, &lv_font_montserrat_14, 0);

    advertising_switch = lv_switch_create(adv_row);
    lv_obj_add_event_cb(advertising_switch, handleAdvertisingToggle, LV_EVENT_VALUE_CHANGED, this);

    // Advertising stato iniziale
    is_advertising = ble.isAdvertising();
    if (is_advertising) {
        lv_obj_add_state(advertising_switch, LV_STATE_CHECKED);
    }

    // Bonded peers Card - spostata in fondo
    bonded_card = create_card(content_container, "Host", lv_color_hex(0x1a2332));

    bonded_list = lv_obj_create(bonded_card);
    lv_obj_remove_style_all(bonded_list);
    lv_obj_set_width(bonded_list, lv_pct(100));
    lv_obj_set_height(bonded_list, LV_SIZE_CONTENT);
    lv_obj_set_layout(bonded_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bonded_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(bonded_list, 0, 0);
    lv_obj_set_style_pad_row(bonded_list, 4, 0);

    lv_obj_t* bonded_actions = lv_obj_create(bonded_card);
    lv_obj_remove_style_all(bonded_actions);
    lv_obj_set_width(bonded_actions, lv_pct(100));
    lv_obj_set_height(bonded_actions, LV_SIZE_CONTENT);
    lv_obj_set_layout(bonded_actions, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bonded_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bonded_actions, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(bonded_actions, 4, 0);

    disconnect_btn = lv_btn_create(bonded_actions);
    lv_obj_set_size(disconnect_btn, 75, 28);
    lv_obj_add_event_cb(disconnect_btn, handleDisconnectCurrent, LV_EVENT_CLICKED, this);
    lv_obj_t* disconnect_label = lv_label_create(disconnect_btn);
    lv_label_set_text(disconnect_label, "Disconn");
    lv_obj_set_style_text_font(disconnect_label, &lv_font_montserrat_14, 0);
    lv_obj_center(disconnect_label);

    // Apply theme
    applySnapshot(snapshot);
    applyThemeStyles(snapshot);

    // Populate bonded peers list
    refreshBondedPeers();

    // Create status update timer (every 1 second for more responsive UI)
    status_timer = lv_timer_create(updateStatusTimer, 1000, this);

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

void BleSettingsScreen::refreshBondedPeers() {
    if (!bonded_list) return;

    BleHidManager& ble = BleHidManager::getInstance();
    auto peers = ble.getBondedPeers();

    bonded_addresses_.clear();
    bonded_addresses_.reserve(peers.size());

    lv_obj_clean(bonded_list);

    if (peers.empty()) {
        lv_obj_t* empty = lv_label_create(bonded_list);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(0xa0a0a0), 0);
        lv_label_set_text(empty, "Nessuno");
        return;
    }

    for (const auto& peer : peers) {
        std::string full_addr = peer.address.toString();
        bonded_addresses_.push_back(full_addr);
        size_t index = bonded_addresses_.size() - 1;

        lv_obj_t* row = lv_obj_create(bonded_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_style_pad_row(row, 2, 0);

        lv_obj_t* actions = lv_obj_create(row);
        lv_obj_remove_style_all(actions);
        lv_obj_set_width(actions, LV_SIZE_CONTENT);
        lv_obj_set_layout(actions, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_gap(actions, 3, 0);

        lv_obj_t* connect_btn = lv_btn_create(actions);
        lv_obj_set_size(connect_btn, 55, 24);
        lv_obj_add_event_cb(connect_btn, handlePeerConnect, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(connect_btn, (void*)index);
        lv_obj_t* connect_label = lv_label_create(connect_btn);
        lv_label_set_text(connect_label, "Conn");
        lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_14, 0);
        lv_obj_center(connect_label);
        if (peer.isConnected) {
            lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
        }

        lv_obj_t* forget_btn = lv_btn_create(actions);
        lv_obj_set_size(forget_btn, 60, 24);
        lv_obj_add_event_cb(forget_btn, handlePeerForget, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(forget_btn, (void*)index);
        lv_obj_t* forget_label = lv_label_create(forget_btn);
        lv_label_set_text(forget_label, "Diment");
        lv_obj_set_style_text_font(forget_label, &lv_font_montserrat_14, 0);
        lv_obj_center(forget_label);
        if (peer.isConnected) {
            lv_obj_add_state(forget_btn, LV_STATE_DISABLED);
        }

        lv_obj_t* addr_label = lv_label_create(row);
        lv_obj_set_style_text_font(addr_label, &lv_font_montserrat_14, 0);
        // Abbreviate address: show last 8 chars
        std::string text = full_addr.length() > 12 ? full_addr.substr(full_addr.length() - 11) : full_addr;
        if (peer.isConnected) {
            text = "● " + text;
            lv_obj_set_style_text_color(addr_label, lv_color_hex(0x00ff80), 0);
        } else {
            lv_obj_set_style_text_color(addr_label, lv_color_hex(0xf0f0f0), 0);
        }
        lv_label_set_text(addr_label, text.c_str());
    }
}

void BleSettingsScreen::updateBleStatus() {
    if (!status_label) return;

    BleHidManager& ble = BleHidManager::getInstance();
    ble_enabled = ble.isEnabled();
    bool initialized = ble.isInitialized();
    is_advertising = ble.isAdvertising();
    bool directed = ble.isAdvertisingDirected();
    size_t connected_count = ble.getConnectedCount();
    auto connected_addrs = ble.getConnectedPeerAddresses();
    std::string directed_target = ble.getDirectedTarget();

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
    if (disconnect_btn) {
        if (connected_count > 0) {
            lv_obj_clear_state(disconnect_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(disconnect_btn, LV_STATE_DISABLED);
        }
    }

    if (!initialized) {
        lv_label_set_text(status_label, "○ Non inizializzato");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xa0a0a0), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Host: ---");
        }
        return;
    } else if (!ble_enabled) {
        lv_label_set_text(status_label, "○ Disabilitato");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xa0a0a0), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Host: 0");
        }
        return;
    }

    if (connected_count > 0) {
        std::string text = "● Connesso";
        if (connected_count == 1 && !connected_addrs.empty()) {
            // Show abbreviated address for single connection
            std::string addr = connected_addrs[0];
            if (addr.length() > 12) {
                addr = addr.substr(addr.length() - 8);
            }
            text += " (" + addr + ")";
        } else if (connected_count > 1) {
            text += " (" + std::to_string(connected_count) + " host)";
        }
        lv_label_set_text(status_label, text.c_str());
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00ff80), 0);
        if (clients_label) {
            lv_label_set_text_fmt(clients_label, "Host: %d", connected_count);
        }
    } else if (directed) {
        lv_label_set_text(status_label, "◌ Adv diretto");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00ffff), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Host: 0");
        }
    } else if (is_advertising) {
        lv_label_set_text(status_label, "◌ In attesa...");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00ffff), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Host: 0");
        }
    } else {
        lv_label_set_text(status_label, "○ Adv off");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xffaa00), 0);
        if (clients_label) {
            lv_label_set_text(clients_label, "Host: 0");
        }
    }

    refreshBondedPeers();
}

void BleSettingsScreen::handleEnableToggle(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    bool enabled = lv_obj_has_state(screen->enable_switch, LV_STATE_CHECKED);
    screen->ble_enabled = enabled;

    BleManager::getInstance().enable(enabled);
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

    BleManager::getInstance().setDeviceName(name);
    screen->last_device_name_ = name;
    Logger::getInstance().infof("[BLE HID] Nome dispositivo impostato su: %s", name.c_str());
}

void BleSettingsScreen::handleAdvertisingToggle(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    bool enabled = lv_obj_has_state(screen->advertising_switch, LV_STATE_CHECKED);
    screen->is_advertising = enabled;

    if (!BleHidManager::getInstance().isInitialized()) {
        Logger::getInstance().warn("[BLE HID] Impossibile gestire advertising: non inizializzato");
        screen->updateBleStatus();
        return;
    }

    if (enabled && screen->ble_enabled) { // Allow starting advertising only if BLE is globally enabled
        BleManager::getInstance().startAdvertising();
        Logger::getInstance().info("[BLE HID] Advertising avviato");
    } else {
        BleManager::getInstance().stopAdvertising();
        Logger::getInstance().info("[BLE HID] Advertising fermato");
    }

    screen->updateBleStatus();
}

void BleSettingsScreen::handleDisconnectCurrent(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    BleManager::getInstance().disconnectAll();
    Logger::getInstance().info("[BLE HID] Disconnessione richiesta dall'utente");
    screen->updateBleStatus();
}

void BleSettingsScreen::handlePeerConnect(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    size_t index = (size_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (index >= screen->bonded_addresses_.size()) return;

    NimBLEAddress target(screen->bonded_addresses_[index]);
    BleManager::getInstance().startDirectedAdvertising(target, 30); // Increase timeout to 30s
    Logger::getInstance().infof("[BLE HID] In attesa di connessione da %s", target.toString().c_str());
    screen->updateBleStatus();
}

void BleSettingsScreen::handlePeerForget(lv_event_t* e) {
    auto* screen = static_cast<BleSettingsScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    size_t index = (size_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (index >= screen->bonded_addresses_.size()) return;

    NimBLEAddress target(screen->bonded_addresses_[index]);
    BleManager::getInstance().forgetPeer(target);
    Logger::getInstance().infof("[BLE HID] Host rimosso: %s", target.toString().c_str());
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
