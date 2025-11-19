#pragma once

#include "core/display_manager.h"
#include "core/screen.h"
#include "widgets/dock_widget.h"
#include <map>
#include <string>

class AppManager {
public:
    static AppManager* getInstance();

    void init(lv_obj_t* root_parent);
    void registerApp(const char* id, const char* emoji, const char* name, Screen* screen);
    void launchApp(const char* id);
    void reloadScreens();
    void requestReload();
    DockController* getDock() { return &dock; }

private:
    AppManager() = default;
    static AppManager* instance;

    struct AppInfo {
        const char* emoji;
        const char* name;
        Screen* screen;
    };

    lv_obj_t* root_parent = nullptr;
    DockController dock;
    Screen* current_screen = nullptr;
    std::string current_app_id;
    bool reload_pending = false;
    std::map<std::string, AppInfo> apps;
    uint32_t settings_listener_id = 0;

    static void handleAsyncReload(void* user_data);
};
