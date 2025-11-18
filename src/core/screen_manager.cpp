#include "core/screen_manager.h"
#include <lvgl.h>

ScreenManager* ScreenManager::instance = nullptr;

ScreenManager* ScreenManager::getInstance() {
    if (instance == nullptr) {
        instance = new ScreenManager();
    }
    return instance;
}

bool ScreenManager::pushScreen(Screen* screen) {
    if (screen == nullptr) {
        return false;
    }

    lv_obj_t* parent = lv_scr_act();
    screen->build(parent);

    if (screen->getRoot() == nullptr) {
        return false;
    }

    stack.push_back(screen);
    lv_scr_load(screen->getRoot());
    screen->onShow();
    return true;
}
