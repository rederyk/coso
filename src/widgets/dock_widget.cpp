#include "widgets/dock_widget.h"

#include "core/display_manager.h"
#include "core/settings_manager.h"
#include <Arduino.h>
#include <algorithm>
#include <utility>
#include "utils/logger.h"

namespace {
constexpr lv_coord_t DOCK_MARGIN = 5;
constexpr lv_coord_t DOCK_THICKNESS = 88;
constexpr lv_coord_t HANDLE_OFFSET = 2;  // Reduced offset - closer to bottom
constexpr lv_coord_t SWIPE_EDGE_HEIGHT = 24; // Android-style edge swipe area
constexpr lv_coord_t HANDLE_TOUCH_WIDTH = 120;  // Wider touch area
constexpr lv_coord_t HANDLE_TOUCH_HEIGHT = 24; // Reduced height - less extension upward
constexpr lv_coord_t OUTSIDE_CLICK_MARGIN = 30; // Margin outside dock area for closing

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
    if (edge_detector_) {
        lv_obj_del(edge_detector_);
        edge_detector_ = nullptr;
    }
    if (click_detector_) {
        lv_obj_del(click_detector_);
        click_detector_ = nullptr;
    }
    visual_bar_ = nullptr; // Child of handle_button_, deleted automatically
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
    if (edge_detector_) {
        lv_obj_del(edge_detector_);
        edge_detector_ = nullptr;
    }
    if (click_detector_) {
        lv_obj_del(click_detector_);
        click_detector_ = nullptr;
    }
    visual_bar_ = nullptr;
    launcher_layer_ = nullptr;
    is_visible_ = false;
}

void DockView::ensureCreated(lv_obj_t* launcher_layer) {
    if (!launcher_layer || dock_container_) {
        if (!launcher_layer_) {
            launcher_layer_ = launcher_layer;
        }
        return;
    }

    launcher_layer_ = launcher_layer;

    // Get colors from settings
    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();
    icon_background_color_hex_ = snapshot.dockIconBackgroundColor;
    icon_symbol_color_hex_ = snapshot.dockIconSymbolColor;
    icon_corner_radius_ = clampIconRadius(snapshot.dockIconRadius);

    dock_container_ = lv_obj_create(launcher_layer_);
    lv_obj_remove_style_all(dock_container_);
    lv_obj_set_style_bg_color(dock_container_, lv_color_hex(snapshot.dockColor), 0);
    lv_obj_set_style_bg_opa(dock_container_, LV_OPA_90, 0);
    lv_obj_set_style_border_width(dock_container_, 0, 0);
    lv_obj_set_style_outline_width(dock_container_, 0, 0);
    lv_obj_set_style_shadow_opa(dock_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(dock_container_, snapshot.borderRadius, 0);
    lv_obj_set_style_pad_all(dock_container_, 8, 0);
    lv_obj_add_flag(dock_container_, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(dock_container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(dock_container_, LV_OBJ_FLAG_GESTURE_BUBBLE); // Dock catches gestures
    lv_obj_move_foreground(dock_container_);

    // Add swipe-down gesture to hide dock
    lv_obj_add_event_cb(dock_container_, dockSwipeEvent, LV_EVENT_GESTURE, this);

    icon_container_ = lv_obj_create(dock_container_);
    lv_obj_remove_style_all(icon_container_);
    lv_obj_set_style_bg_opa(icon_container_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_size(icon_container_, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(icon_container_, LV_LAYOUT_FLEX);
    lv_obj_center(icon_container_);
    // Enable horizontal scrolling for dock icons
    lv_obj_set_scrollbar_mode(icon_container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(icon_container_, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(icon_container_, LV_SCROLL_SNAP_NONE);
    lv_obj_set_scroll_snap_y(icon_container_, LV_SCROLL_SNAP_NONE);
    lv_obj_add_flag(icon_container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(icon_container_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(icon_container_, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    // Allow gestures to bubble up from icons to dock
    lv_obj_add_flag(icon_container_, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Create edge swipe detector - Android-style bottom edge activation
    edge_detector_ = lv_obj_create(launcher_layer_);
    lv_obj_remove_style_all(edge_detector_);
    lv_obj_set_size(edge_detector_, lv_pct(100), SWIPE_EDGE_HEIGHT);
    lv_obj_align(edge_detector_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(edge_detector_, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(edge_detector_, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_style_bg_opa(edge_detector_, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(edge_detector_, edgeSwipeEvent, LV_EVENT_GESTURE, this);

    // Create handle bar - improved Android-style handle
    handle_button_ = lv_obj_create(launcher_layer_);
    lv_obj_remove_style_all(handle_button_);
    lv_obj_set_size(handle_button_, HANDLE_TOUCH_WIDTH, HANDLE_TOUCH_HEIGHT);
    lv_obj_add_flag(handle_button_, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(handle_button_, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_style_bg_opa(handle_button_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(handle_button_, 0, 0);
    lv_obj_set_style_outline_width(handle_button_, 0, 0);
    lv_obj_move_foreground(handle_button_);

    // Create the visible bar with improved styling
    visual_bar_ = lv_obj_create(handle_button_);
    lv_obj_remove_style_all(visual_bar_);
    lv_obj_set_size(visual_bar_, 60, 5);
    lv_obj_set_style_bg_color(visual_bar_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(visual_bar_, LV_OPA_70, 0);
    lv_obj_set_style_radius(visual_bar_, 3, 0);
    lv_obj_set_style_shadow_width(visual_bar_, 6, 0);
    lv_obj_set_style_shadow_color(visual_bar_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(visual_bar_, LV_OPA_30, 0);
    // Position bar at bottom of touch area, very close to screen edge
    lv_obj_align(visual_bar_, LV_ALIGN_BOTTOM_MID, 0, -2);

    // Add gesture and press handlers to the handle bar
    lv_obj_add_event_cb(handle_button_, handleGestureEvent, LV_EVENT_GESTURE, this);
    lv_obj_add_event_cb(handle_button_, handlePressEvent, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(handle_button_, handleReleaseEvent, LV_EVENT_RELEASED, this);

    // Create click detector for outside clicks
    click_detector_ = lv_obj_create(launcher_layer_);
    lv_obj_remove_style_all(click_detector_);
    lv_obj_set_size(click_detector_, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(click_detector_, 0, 0);
    lv_obj_add_flag(click_detector_, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(click_detector_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(click_detector_, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(click_detector_, outsideClickEvent, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(click_detector_, LV_OBJ_FLAG_HIDDEN); // Initially hidden

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
    lv_obj_remove_style_all(app_btn);
    lv_obj_set_size(app_btn, ICON_SIZE, ICON_SIZE);
    lv_obj_set_style_radius(app_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(app_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(app_btn, LV_OBJ_FLAG_GESTURE_BUBBLE); // Allow swipe gestures to bubble

    lv_obj_t* icon = lv_label_create(app_btn);
    lv_label_set_text(icon, emoji);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_center(icon);

    applyIconAppearance(app_btn, icon);

    lv_obj_add_event_cb(app_btn, iconEvent, LV_EVENT_CLICKED, this);

    IconEntry entry;
    entry.id = app_id;
    entry.button = app_btn;
    entry.label = icon;
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
    updateClickDetector();
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
    updateClickDetector();
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
        lv_obj_set_scroll_dir(icon_container_, LV_DIR_HOR);
        lv_obj_set_scroll_snap_x(icon_container_, LV_SCROLL_SNAP_NONE);
        lv_obj_set_scroll_snap_y(icon_container_, LV_SCROLL_SNAP_NONE);

        if (landscape_mode_) {
            lv_obj_set_style_pad_row(icon_container_, 0, 0);
            lv_obj_set_style_pad_column(icon_container_, 8, 0);
            lv_obj_set_style_pad_left(icon_container_, 8, 0);
            lv_obj_set_style_pad_right(icon_container_, 8, 0);
            lv_obj_set_flex_flow(icon_container_, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(icon_container_,
                                  LV_FLEX_ALIGN_SPACE_EVENLY,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        } else {
            lv_obj_set_style_pad_row(icon_container_, 2, 0);
            lv_obj_set_style_pad_column(icon_container_, 10, 0);
            lv_obj_set_style_pad_left(icon_container_, 10, 0);
            lv_obj_set_style_pad_right(icon_container_, 10, 0);
            lv_obj_set_flex_flow(icon_container_, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(icon_container_,
                                  LV_FLEX_ALIGN_START,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        }

        // Reset scroll position so icons start from left
        lv_obj_scroll_to_x(icon_container_, 0, LV_ANIM_OFF);
        lv_obj_scroll_to_y(icon_container_, 0, LV_ANIM_OFF);
    }
    updateHandlePosition();
}

void DockView::updateHandlePosition() {
    if (!handle_button_) {
        return;
    }
    lv_obj_align(handle_button_, LV_ALIGN_BOTTOM_MID, 0, -HANDLE_OFFSET);
}

void DockView::updateColors(uint32_t dock_color,
                            uint32_t icon_bg_color,
                            uint32_t icon_symbol_color,
                            uint8_t border_radius,
                            uint8_t icon_radius) {
    if (!dock_container_) {
        return;
    }
    icon_background_color_hex_ = icon_bg_color;
    icon_symbol_color_hex_ = icon_symbol_color;
    icon_corner_radius_ = clampIconRadius(icon_radius);
    lv_obj_set_style_bg_color(dock_container_, lv_color_hex(dock_color), 0);
    lv_obj_set_style_radius(dock_container_, border_radius, 0);
    refreshIconAppearance();
}

void DockView::refreshIconAppearance() {
    if (icons_.empty()) {
        return;
    }
    for (const auto& entry : icons_) {
        applyIconAppearance(entry.button, entry.label);
    }
}

void DockView::applyIconAppearance(lv_obj_t* button, lv_obj_t* label) const {
    if (!button) {
        return;
    }
    lv_color_t base = lv_color_hex(icon_background_color_hex_);
    lv_color_t symbol = lv_color_hex(icon_symbol_color_hex_);
    lv_color_t border = lv_color_mix(base, symbol, LV_OPA_40);
    lv_color_t pressed = lv_color_mix(base, lv_color_hex(0x000000), LV_OPA_30);

    lv_obj_set_style_bg_color(button, base, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, border, 0);
    lv_obj_set_style_radius(button, icon_corner_radius_, 0);
    lv_obj_set_style_bg_color(button, pressed, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);

    if (label) {
        lv_obj_set_style_text_color(label, symbol, 0);
    }
}

lv_coord_t DockView::clampIconRadius(uint8_t radius) const {
    lv_coord_t max_radius = ICON_SIZE / 2;
    lv_coord_t clamped = std::min<lv_coord_t>(max_radius, radius);
    return clamped;
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

void DockView::handleHandleGesture(lv_event_t* e) {
    if (!e) {
        return;
    }
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    // Swipe up on handle -> show dock
    if (dir == LV_DIR_TOP && !is_visible_) {
        show();
    }
    // Swipe down on handle -> hide dock
    else if (dir == LV_DIR_BOTTOM && is_visible_) {
        hide();
    }
}

void DockView::handleEdgeSwipe(lv_event_t* e) {
    if (!e) {
        return;
    }
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    // Android-style: swipe up from bottom edge shows dock
    if (dir == LV_DIR_TOP && !is_visible_) {
        show();
    }
}

void DockView::handleDockSwipe(lv_event_t* e) {
    if (!e) {
        return;
    }
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    // Swipe down on visible dock -> hide it
    if (dir == LV_DIR_BOTTOM && is_visible_) {
        hide();
    }
}

void DockView::handlePress() {
    if (!visual_bar_) {
        return;
    }
    // Animate handle on press - scale up effect
    lv_obj_set_style_transform_zoom(visual_bar_, 280, 0); // 1.4x scale
    lv_obj_set_style_bg_opa(visual_bar_, LV_OPA_90, 0);
}

void DockView::handleRelease() {
    if (!visual_bar_) {
        return;
    }
    // Restore handle size on release
    lv_obj_set_style_transform_zoom(visual_bar_, 256, 0); // 1.0x scale
    lv_obj_set_style_bg_opa(visual_bar_, LV_OPA_70, 0);
}

void DockView::animateHandlePulse() {
    if (!visual_bar_) {
        return;
    }
    // Subtle pulsing animation to draw attention
    static lv_anim_t pulse_anim;
    lv_anim_init(&pulse_anim);
    lv_anim_set_var(&pulse_anim, visual_bar_);
    lv_anim_set_time(&pulse_anim, 1200);
    lv_anim_set_values(&pulse_anim, LV_OPA_70, LV_OPA_100);
    lv_anim_set_path_cb(&pulse_anim, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&pulse_anim, [](void* var, int32_t value) {
        lv_obj_set_style_bg_opa((lv_obj_t*)var, value, 0);
    });
    lv_anim_set_playback_time(&pulse_anim, 1200);
    lv_anim_set_repeat_count(&pulse_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&pulse_anim);
}

void DockView::updateClickDetector() {
    if (!click_detector_) {
        return;
    }

    if (is_visible_) {
        // Show click detector when dock is visible
        lv_obj_clear_flag(click_detector_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_background(click_detector_); // Behind dock but above apps
    } else {
        // Hide click detector when dock is hidden
        lv_obj_add_flag(click_detector_, LV_OBJ_FLAG_HIDDEN);
    }
}

void DockView::handleOutsideClick(lv_event_t* e) {
    if (!e || !is_visible_ || !dock_container_) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);

    // Get dock boundaries with margin
    lv_coord_t dock_x = lv_obj_get_x(dock_container_);
    lv_coord_t dock_y = lv_obj_get_y(dock_container_);
    lv_coord_t dock_w = lv_obj_get_width(dock_container_);
    lv_coord_t dock_h = lv_obj_get_height(dock_container_);

    // Check if click is outside dock area (with margin)
    bool outside = (point.x < dock_x - OUTSIDE_CLICK_MARGIN ||
                   point.x > dock_x + dock_w + OUTSIDE_CLICK_MARGIN ||
                   point.y < dock_y - OUTSIDE_CLICK_MARGIN ||
                   point.y > dock_y + dock_h + OUTSIDE_CLICK_MARGIN);

    if (outside) {
        hide();
    }
}

void DockView::handleGestureEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    view->handleHandleGesture(e);
}

void DockView::edgeSwipeEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    view->handleEdgeSwipe(e);
}

void DockView::dockSwipeEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    view->handleDockSwipe(e);
}

void DockView::handlePressEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    view->handlePress();
}

void DockView::handleReleaseEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    view->handleRelease();
}

void DockView::iconEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    lv_obj_t* target = lv_event_get_target(e);
    view->handleIconTriggered(target);
}

void DockView::outsideClickEvent(lv_event_t* e) {
    auto* view = static_cast<DockView*>(lv_event_get_user_data(e));
    if (!view) {
        return;
    }
    view->handleOutsideClick(e);
}

DockController::DockController() = default;
DockController::~DockController() = default;

void DockController::init() {
    DisplayManager& display = DisplayManager::getInstance();
    lv_obj_t* launcher_layer = display.getLauncherLayer();
    if (!launcher_layer) {
        Logger::getInstance().warn("[Dock] Launcher layer unavailable");
        return;
    }
    view_.create(launcher_layer);
    view_.setIconTapCallback([this](const char* app_id) {
        if (launch_handler_) {
            launch_handler_(app_id);
        }
        view_.hide();
    });
    onOrientationChanged(display.isLandscape());

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

void DockController::destroy() {
    view_.destroy();
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

void DockController::updateColors(uint32_t dock_color,
                                  uint32_t icon_bg_color,
                                  uint32_t icon_symbol_color,
                                  uint8_t border_radius,
                                  uint8_t icon_radius) {
    view_.updateColors(dock_color, icon_bg_color, icon_symbol_color, border_radius, icon_radius);
}
