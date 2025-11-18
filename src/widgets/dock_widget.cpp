#include "widgets/dock_widget.h"

#include "core/display_manager.h"
#include <Arduino.h>
#include <utility>

namespace {
constexpr lv_coord_t DOCK_MARGIN = 5;
constexpr lv_coord_t DOCK_THICKNESS = 88;
constexpr lv_coord_t HANDLE_OFFSET = 6;

lv_coord_t get_display_width() {
    return DisplayManager::getInstance().getWidth();
}

lv_coord_t get_display_height() {
    return DisplayManager::getInstance().getHeight();
}
} // namespace

DockView::DockView() = default;

DockView::~DockView() {
    destroyIcons();
    if (dock_container_) {
        lv_obj_del(dock_container_);
        dock_container_ = nullptr;
    }
    if (handle_button_) {
        lv_obj_del(handle_button_);
        handle_button_ = nullptr;
    }
}

void DockView::create(lv_obj_t* launcher_layer) {
    ensureCreated(launcher_layer);
}

void DockView::destroy() {
    destroyIcons();
    if (dock_container_) {
        lv_obj_del(dock_container_);
        dock_container_ = nullptr;
    }
    if (handle_button_) {
        lv_obj_del(handle_button_);
        handle_button_ = nullptr;
    }
    launcher_layer_ = nullptr;
}

void DockView::ensureCreated(lv_obj_t* launcher_layer) {
    if (!launcher_layer || dock_container_) {
        if (!launcher_layer_) {
            launcher_layer_ = launcher_layer;
        }
        return;
    }

    launcher_layer_ = launcher_layer;

    dock_container_ = lv_obj_create(launcher_layer_);
    lv_obj_set_style_bg_color(dock_container_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(dock_container_, LV_OPA_90, 0);
    lv_obj_set_style_border_width(dock_container_, 0, 0);
    lv_obj_set_style_radius(dock_container_, 16, 0);
    lv_obj_set_style_pad_all(dock_container_, 8, 0);
    lv_obj_add_flag(dock_container_, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(dock_container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(dock_container_);

    icon_container_ = lv_obj_create(dock_container_);
    lv_obj_remove_style_all(icon_container_);
    lv_obj_set_style_bg_opa(icon_container_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_size(icon_container_, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(icon_container_, LV_LAYOUT_FLEX);
    lv_obj_center(icon_container_);

    handle_button_ = lv_btn_create(launcher_layer_);
    lv_obj_set_size(handle_button_, 80, 24);
    lv_obj_add_flag(handle_button_, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(handle_button_, lv_color_hex(0x1f2a44), 0);
    lv_obj_set_style_bg_opa(handle_button_, LV_OPA_80, 0);
    lv_obj_set_style_radius(handle_button_, 12, 0);
    lv_obj_set_style_border_width(handle_button_, 0, 0);
    lv_obj_move_foreground(handle_button_);

    lv_obj_add_event_cb(handle_button_, handleButtonEvent, LV_EVENT_CLICKED, this);

    lv_obj_t* handle_bar = lv_label_create(handle_button_);
    lv_label_set_text(handle_bar, "⋮⋮");
    lv_obj_set_style_text_font(handle_bar, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(handle_bar, lv_color_hex(0xf0f0f0), 0);
    lv_obj_center(handle_bar);

    onOrientationChanged(landscape_mode_);
}

void DockView::clearIcons() {
    destroyIcons();
}

void DockView::destroyIcons() {
    for (auto& entry : icons_) {
        if (entry.button) {
            lv_obj_del(entry.button);
        }
    }
    icons_.clear();
}

void DockView::addIcon(const char* app_id, const char* emoji, const char* name) {
    if (!icon_container_) {
        return;
    }
    if (!app_id || !emoji || !name) {
        return;
    }

    lv_obj_t* app_btn = lv_obj_create(icon_container_);
    lv_obj_set_size(app_btn, 60, 60);
    lv_obj_set_style_bg_color(app_btn, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_bg_opa(app_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(app_btn, 12, 0);
    lv_obj_set_style_border_width(app_btn, 1, 0);
    lv_obj_set_style_border_color(app_btn, lv_color_hex(0x0f3460), 0);
    lv_obj_add_flag(app_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(app_btn, lv_color_hex(0x0f3460), LV_STATE_PRESSED);

    lv_obj_t* icon = lv_label_create(app_btn);
    lv_label_set_text(icon, emoji);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_center(icon);

    lv_obj_add_event_cb(app_btn, iconEvent, LV_EVENT_CLICKED, this);

    IconEntry entry;
    entry.id = app_id;
    entry.button = app_btn;
    icons_.push_back(entry);
}

void DockView::show() {
    if (is_visible_ || !dock_container_) {
        return;
    }

    lv_coord_t screen_h = get_display_height();
    lv_anim_init(&show_anim_);
    lv_anim_set_var(&show_anim_, dock_container_);
    lv_anim_set_time(&show_anim_, 300);
    lv_anim_set_path_cb(&show_anim_, lv_anim_path_ease_out);
    lv_coord_t start_y = screen_h;
    lv_coord_t end_y = screen_h - lv_obj_get_height(dock_container_) - DOCK_MARGIN;
    lv_anim_set_values(&show_anim_, start_y, end_y);
    lv_anim_set_exec_cb(&show_anim_, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_start(&show_anim_);
    is_visible_ = true;
}

void DockView::hide() {
    if (!is_visible_ || !dock_container_) {
        return;
    }

    lv_coord_t screen_h = get_display_height();
    lv_anim_init(&hide_anim_);
    lv_anim_set_var(&hide_anim_, dock_container_);
    lv_anim_set_time(&hide_anim_, 300);
    lv_anim_set_path_cb(&hide_anim_, lv_anim_path_ease_in);
    lv_coord_t start_y = screen_h - lv_obj_get_height(dock_container_) - DOCK_MARGIN;
    lv_coord_t end_y = screen_h;
    lv_anim_set_values(&hide_anim_, start_y, end_y);
    lv_anim_set_exec_cb(&hide_anim_, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_start(&hide_anim_);
    is_visible_ = false;
}

void DockView::toggle() {
    if (is_visible_) {
        hide();
    } else {
        show();
    }
}

void DockView::onOrientationChanged(bool landscape) {
    landscape_mode_ = landscape;
    if (!dock_container_) {
        return;
    }

    lv_anim_del(dock_container_, nullptr);

    lv_coord_t screen_w = get_display_width();
    lv_coord_t screen_h = get_display_height();

    lv_obj_set_size(dock_container_, screen_w, DOCK_THICKNESS);
    lv_obj_set_x(dock_container_, 0);
    lv_coord_t target_y = is_visible_ ? (screen_h - DOCK_THICKNESS - DOCK_MARGIN) : screen_h;
    lv_obj_set_y(dock_container_, target_y);

    if (icon_container_) {
        if (landscape_mode_) {
            lv_obj_set_style_pad_row(icon_container_, 0, 0);
            lv_obj_set_style_pad_column(icon_container_, 0, 0);
            lv_obj_set_flex_flow(icon_container_, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(icon_container_,
                                  LV_FLEX_ALIGN_SPACE_EVENLY,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        } else {
            lv_obj_set_style_pad_row(icon_container_, 6, 0);
            lv_obj_set_style_pad_column(icon_container_, 12, 0);
            lv_obj_set_flex_flow(icon_container_, LV_FLEX_FLOW_COLUMN_WRAP);
            lv_obj_set_flex_align(icon_container_,
                                  LV_FLEX_ALIGN_SPACE_AROUND,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        }
    }
    updateHandlePosition();
}

void DockView::updateHandlePosition() {
    if (!handle_button_) {
        return;
    }
    lv_obj_align(handle_button_, LV_ALIGN_BOTTOM_MID, 0, -HANDLE_OFFSET);
}

void DockView::setHandleCallback(std::function<void()> callback) {
    handle_callback_ = std::move(callback);
}

void DockView::setIconTapCallback(std::function<void(const char* app_id)> callback) {
    icon_callback_ = std::move(callback);
}

void DockView::handleIconTriggered(lv_obj_t* target) {
    if (!target) {
        return;
    }
    for (const auto& entry : icons_) {
        if (entry.button == target) {
            if (icon_callback_) {
                icon_callback_(entry.id.c_str());
            }
            break;
        }
    }
}

void DockView::handleButtonEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    if (view->handle_callback_) {
        view->handle_callback_();
    }
}

void DockView::iconEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    lv_obj_t* target = lv_event_get_target(e);
    view->handleIconTriggered(target);
}

DockController::DockController() = default;
DockController::~DockController() {
    detachGestureSurface();
}

void DockController::init(lv_obj_t* gesture_surface) {
    DisplayManager& display = DisplayManager::getInstance();
    lv_obj_t* launcher_layer = display.getLauncherLayer();
    if (!launcher_layer) {
        Serial.println("[Dock] Launcher layer unavailable");
        return;
    }
    view_.create(launcher_layer);
    view_.setHandleCallback([this]() {
        toggle();
    });
    view_.setIconTapCallback([this](const char* app_id) {
        if (launch_handler_) {
            launch_handler_(app_id);
        }
        view_.hide();
    });
    onOrientationChanged(display.isLandscape());
    setGestureSurface(gesture_surface);

    // Rebuild icons if init happens after registration
    if (!items_.empty()) {
        view_.clearIcons();
        for (const auto& entry : items_) {
            view_.addIcon(entry.first.c_str(), entry.second.emoji.c_str(), entry.second.name.c_str());
        }
    }
}

void DockController::setLaunchHandler(std::function<void(const char* app_id)> handler) {
    launch_handler_ = std::move(handler);
}

void DockController::registerLauncherItem(const char* app_id, const char* emoji, const char* name) {
    if (!app_id || !emoji || !name) {
        return;
    }
    LauncherItem item{emoji, name};
    bool existed = items_.find(app_id) != items_.end();
    items_[app_id] = item;
    if (!existed && view_.isReady()) {
        view_.addIcon(app_id, item.emoji.c_str(), item.name.c_str());
    }
}

void DockController::onOrientationChanged(bool landscape) {
    view_.onOrientationChanged(landscape);
}

void DockController::show() {
    view_.show();
}

void DockController::hide() {
    view_.hide();
}

void DockController::toggle() {
    view_.toggle();
}

void DockController::setGestureSurface(lv_obj_t* target) {
    if (gesture_surface_ == target) {
        return;
    }
    detachGestureSurface();
    gesture_surface_ = target;
    if (gesture_surface_) {
        lv_obj_add_event_cb(gesture_surface_, gestureHandler, LV_EVENT_GESTURE, this);
    }
}

void DockController::detachGestureSurface() {
    if (!gesture_surface_) {
        return;
    }
    lv_obj_remove_event_cb_with_user_data(gesture_surface_, gestureHandler, this);
    gesture_surface_ = nullptr;
}

void DockController::handleGesture(lv_event_t* e) {
    if (!e) {
        return;
    }
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_TOP && !view_.isVisible()) {
        view_.show();
    } else if (dir == LV_DIR_BOTTOM && view_.isVisible()) {
        view_.hide();
    }
}

void DockController::gestureHandler(lv_event_t* e) {
    auto* controller = static_cast<DockController*>(lv_event_get_user_data(e));
    if (!controller) {
        return;
    }
    controller->handleGesture(e);
}
