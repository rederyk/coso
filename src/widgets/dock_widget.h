#pragma once

#include <lvgl.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

class DockView {
public:
    DockView();
    ~DockView();

    void create(lv_obj_t* launcher_layer);
    void destroy();
    void clearIcons();
    void addIcon(const char* app_id, const char* emoji, const char* name);

    void show();
    void hide();
    void toggle();
    void onOrientationChanged(bool landscape);
    void updateColors(uint32_t dock_color, uint8_t border_radius);

    void setIconTapCallback(std::function<void(const char* app_id)> callback);
    bool isVisible() const { return is_visible_; }
    bool isReady() const { return dock_container_ != nullptr; }

private:
    struct IconEntry {
        std::string id;
        lv_obj_t* button = nullptr;
    };

    void ensureCreated(lv_obj_t* launcher_layer);
    void updateHandlePosition();
    void destroyIcons();
    void handleIconTriggered(lv_obj_t* target);
    void handleHandleGesture(lv_event_t* e);
    void handleEdgeSwipe(lv_event_t* e);
    void handleDockSwipe(lv_event_t* e);
    void handlePress();
    void handleRelease();
    void animateHandlePulse();

    static void iconEvent(lv_event_t* e);
    static void handleGestureEvent(lv_event_t* e);
    static void edgeSwipeEvent(lv_event_t* e);
    static void dockSwipeEvent(lv_event_t* e);
    static void handlePressEvent(lv_event_t* e);
    static void handleReleaseEvent(lv_event_t* e);

    lv_obj_t* launcher_layer_ = nullptr;
    lv_obj_t* dock_container_ = nullptr;
    lv_obj_t* icon_container_ = nullptr;
    lv_obj_t* handle_button_ = nullptr;
    lv_obj_t* edge_detector_ = nullptr;
    lv_obj_t* visual_bar_ = nullptr;
    lv_anim_t show_anim_;
    lv_anim_t hide_anim_;
    bool is_visible_ = false;
    bool landscape_mode_ = true;
    std::vector<IconEntry> icons_;
    std::function<void(const char* app_id)> icon_callback_;
};

class DockController {
public:
    DockController();
    ~DockController();

    void init();
    void registerLauncherItem(const char* app_id, const char* emoji, const char* name);
    void onOrientationChanged(bool landscape);
    void show();
    void hide();
    void toggle();
    void setLaunchHandler(std::function<void(const char* app_id)> handler);
    void updateColors(uint32_t dock_color, uint8_t border_radius);

private:
    struct LauncherItem {
        std::string emoji;
        std::string name;
    };

    DockView view_;
    std::function<void(const char* app_id)> launch_handler_;
    std::map<std::string, LauncherItem> items_;
};
