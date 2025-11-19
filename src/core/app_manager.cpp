#include "core/app_manager.h"
#include "core/settings_manager.h"
#include <Arduino.h>
#include <lvgl.h>

AppManager* AppManager::instance = nullptr;

AppManager* AppManager::getInstance() {
    if (!instance) {
        instance = new AppManager();
    }
    return instance;
}

void AppManager::init(lv_obj_t* parent) {
    root_parent = parent;
    dock.init();
    dock.setLaunchHandler([this](const char* app_id) {
        launchApp(app_id);
    });

    // Register settings listener to update dock colors
    SettingsManager& settings = SettingsManager::getInstance();
    if (settings_listener_id == 0) {
        settings_listener_id = settings.addListener(
            [this](SettingsManager::SettingKey key, const SettingsSnapshot& snapshot) {
                if (key == SettingsManager::SettingKey::ThemeDockColor ||
                    key == SettingsManager::SettingKey::ThemeBorderRadius) {
                    dock.updateColors(snapshot.dockColor, snapshot.borderRadius);
                }
            });
    }
}

void AppManager::registerApp(const char* id, const char* emoji, const char* name, Screen* screen) {
    apps[id] = {emoji, name, screen};

    dock.registerLauncherItem(id, emoji, name);
}

void AppManager::launchApp(const char* id) {
    auto it = apps.find(id);
    if (it == apps.end()) {
        Serial.printf("App '%s' not found\n", id);
        return;
    }

    // Nascondi schermo corrente
    if (current_screen) {
        current_screen->onHide();
        lv_obj_t* old_root = current_screen->getRoot();
        if (old_root) {
            lv_obj_add_flag(old_root, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Mostra nuovo schermo
    Screen* new_screen = it->second.screen;
    lv_obj_t* new_root = new_screen->getRoot();

    if (!new_root) {
        // Prima volta: costruisci la schermata
        new_screen->build(root_parent);
        new_root = new_screen->getRoot();
    }

    if (new_root) {
        lv_obj_clear_flag(new_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(new_root);
        DisplayManager& display = DisplayManager::getInstance();
        display.getOverlayLayer();
        display.getLauncherLayer();
    }

    new_screen->onShow();
    current_screen = new_screen;
    current_app_id = id ? id : "";
    Serial.printf("Launched app: %s %s\n", it->second.emoji, it->second.name);
}

void AppManager::reloadScreens() {
    if (apps.empty()) {
        return;
    }

    std::string previous = current_app_id;
    for (auto& entry : apps) {
        Screen* screen = entry.second.screen;
        if (screen && screen->getRoot()) {
            screen->onHide();
            screen->destroyRoot();
        }
    }

    current_screen = nullptr;

    if (!previous.empty()) {
        launchApp(previous.c_str());
    }
}

void AppManager::requestReload() {
    if (reload_pending) {
        return;
    }
    reload_pending = true;
    lv_async_call(handleAsyncReload, this);
}

void AppManager::handleAsyncReload(void* user_data) {
    auto* manager = static_cast<AppManager*>(user_data);
    if (!manager) {
        return;
    }
    manager->reloadScreens();
    manager->reload_pending = false;
}
