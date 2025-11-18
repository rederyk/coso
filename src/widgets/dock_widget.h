#pragma once

#include <lvgl.h>
#include <functional>

class DockWidget {
public:
    struct AppIcon {
        const char* emoji;
        const char* name;
        std::function<void()> callback;
    };

    DockWidget();
    ~DockWidget();

    void create(lv_obj_t* parent);
    void addApp(const char* emoji, const char* name, std::function<void()> callback);
    void show();
    void hide();

private:
    lv_obj_t* dock_container = nullptr;
    lv_obj_t* icon_container = nullptr;
    lv_anim_t show_anim;
    lv_anim_t hide_anim;
    bool is_visible = false;

    static void swipeUpEventHandler(lv_event_t* e);
    static void iconClickHandler(lv_event_t* e);
};
