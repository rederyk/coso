#pragma once

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
    DockWidget* getDock() { return &dock; }

private:
    AppManager() = default;
    static AppManager* instance;

    struct AppInfo {
        const char* emoji;
        const char* name;
        Screen* screen;
    };

    lv_obj_t* root_parent = nullptr;
    DockWidget dock;
    Screen* current_screen = nullptr;
    std::map<std::string, AppInfo> apps;
};
