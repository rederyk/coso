#include "widgets/dock_widget.h"
#include <Arduino.h>

DockWidget::DockWidget() {}

DockWidget::~DockWidget() {
    if (dock_container) {
        lv_obj_del(dock_container);
        dock_container = nullptr;
    }
    if (handle_button) {
        lv_obj_del(handle_button);
        handle_button = nullptr;
    }
}

void DockWidget::create(lv_obj_t* parent) {
    if (!parent) return;

    // Container principale del dock (nascosto inizialmente)
    dock_container = lv_obj_create(parent);
    lv_obj_set_size(dock_container, LV_HOR_RES_MAX, 80);
    lv_obj_set_pos(dock_container, 0, LV_VER_RES_MAX); // Nascosto sotto lo schermo
    lv_obj_set_style_bg_color(dock_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(dock_container, LV_OPA_90, 0);
    lv_obj_set_style_border_width(dock_container, 0, 0);
    lv_obj_set_style_radius(dock_container, 16, 0);
    lv_obj_set_style_pad_all(dock_container, 8, 0);
    lv_obj_add_flag(dock_container, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(dock_container, LV_OBJ_FLAG_SCROLLABLE);

    // Container per le icone (layout orizzontale)
    icon_container = lv_obj_create(dock_container);
    lv_obj_set_size(icon_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(icon_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_container, 0, 0);
    lv_obj_set_style_pad_all(icon_container, 0, 0);
    lv_obj_set_layout(icon_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(icon_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icon_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(icon_container);

    // Aggiungi gesture per far apparire il dock
    lv_obj_add_event_cb(parent, swipeUpEventHandler, LV_EVENT_GESTURE, this);

    // Handle flottante per aprire il dock
    handle_button = lv_btn_create(parent);
    lv_obj_set_size(handle_button, 80, 24);
    lv_obj_add_flag(handle_button, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(handle_button, lv_color_hex(0x1f2a44), 0);
    lv_obj_set_style_bg_opa(handle_button, LV_OPA_80, 0);
    lv_obj_set_style_radius(handle_button, 12, 0);
    lv_obj_set_style_border_width(handle_button, 0, 0);
    lv_obj_align(handle_button, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_add_event_cb(handle_button, handleButtonHandler, LV_EVENT_CLICKED, this);

    lv_obj_t* handle_bar = lv_label_create(handle_button);
    lv_label_set_text(handle_bar, "⋮⋮");
    lv_obj_set_style_text_font(handle_bar, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(handle_bar, lv_color_hex(0xf0f0f0), 0);
    lv_obj_center(handle_bar);
}

void DockWidget::addApp(const char* emoji, const char* name, std::function<void()> callback) {
    if (!icon_container) return;

    // Container per singola icona
    lv_obj_t* app_btn = lv_obj_create(icon_container);
    lv_obj_set_size(app_btn, 60, 60);
    lv_obj_set_style_bg_color(app_btn, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_bg_opa(app_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(app_btn, 12, 0);
    lv_obj_set_style_border_width(app_btn, 1, 0);
    lv_obj_set_style_border_color(app_btn, lv_color_hex(0x0f3460), 0);
    lv_obj_add_flag(app_btn, LV_OBJ_FLAG_CLICKABLE);

    // Hover effect
    lv_obj_set_style_bg_color(app_btn, lv_color_hex(0x0f3460), LV_STATE_PRESSED);

    // Emoji come icona
    lv_obj_t* icon = lv_label_create(app_btn);
    lv_label_set_text(icon, emoji);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_center(icon);

    // Salva callback come user_data
    auto* cb_ptr = new std::function<void()>(callback);
    lv_obj_add_event_cb(app_btn, iconClickHandler, LV_EVENT_CLICKED, cb_ptr);
}

void DockWidget::show() {
    if (is_visible || !dock_container) return;

    lv_coord_t start_y = LV_VER_RES_MAX;
    lv_coord_t end_y = LV_VER_RES_MAX - lv_obj_get_height(dock_container) - 5;

    lv_anim_init(&show_anim);
    lv_anim_set_var(&show_anim, dock_container);
    lv_anim_set_values(&show_anim, start_y, end_y);
    lv_anim_set_time(&show_anim, 300);
    lv_anim_set_exec_cb(&show_anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&show_anim, lv_anim_path_ease_out);
    lv_anim_start(&show_anim);

    is_visible = true;
}

void DockWidget::hide() {
    if (!is_visible || !dock_container) return;

    lv_coord_t end_y = LV_VER_RES_MAX;
    lv_coord_t start_y = LV_VER_RES_MAX - lv_obj_get_height(dock_container) - 5;

    lv_anim_init(&hide_anim);
    lv_anim_set_var(&hide_anim, dock_container);
    lv_anim_set_values(&hide_anim, start_y, end_y);
    lv_anim_set_time(&hide_anim, 300);
    lv_anim_set_exec_cb(&hide_anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&hide_anim, lv_anim_path_ease_in);
    lv_anim_start(&hide_anim);

    is_visible = false;
}

void DockWidget::toggle() {
    if (is_visible) {
        hide();
    } else {
        show();
    }
}

void DockWidget::swipeUpEventHandler(lv_event_t* e) {
    DockWidget* dock = static_cast<DockWidget*>(lv_event_get_user_data(e));
    if (!dock) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    if (dir == LV_DIR_TOP && !dock->is_visible) {
        dock->show();
    } else if (dir == LV_DIR_BOTTOM && dock->is_visible) {
        dock->hide();
    }
}

void DockWidget::iconClickHandler(lv_event_t* e) {
    auto* callback = static_cast<std::function<void()>*>(lv_event_get_user_data(e));
    if (callback && *callback) {
        (*callback)();
    }
}

void DockWidget::handleButtonHandler(lv_event_t* e) {
    auto* dock = static_cast<DockWidget*>(lv_event_get_user_data(e));
    if (!dock) {
        return;
    }
    dock->toggle();
}
