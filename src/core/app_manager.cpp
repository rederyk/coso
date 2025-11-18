#include "core/app_manager.h"
#include <Arduino.h>

AppManager* AppManager::instance = nullptr;

AppManager* AppManager::getInstance() {
    if (!instance) {
        instance = new AppManager();
    }
    return instance;
}

void AppManager::init(lv_obj_t* parent) {
    root_parent = parent;
    dock.create(lv_layer_top());
}

void AppManager::registerApp(const char* id, const char* emoji, const char* name, Screen* screen) {
    apps[id] = {emoji, name, screen};

    // Aggiungi al dock
    dock.addApp(emoji, name, [this, id]() {
        launchApp(id);
        dock.hide();
    });
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
    }

    new_screen->onShow();
    current_screen = new_screen;
    Serial.printf("Launched app: %s %s\n", it->second.emoji, it->second.name);
}
