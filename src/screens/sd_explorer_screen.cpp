#include "screens/sd_explorer_screen.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "drivers/sd_card_driver.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"

namespace {
std::string formatBytes(uint64_t bytes) {
    constexpr const char* SUFFIXES[] = {"B", "KB", "MB", "GB", "TB"};
    constexpr size_t SUFFIX_COUNT = sizeof(SUFFIXES) / sizeof(SUFFIXES[0]);
    double value = static_cast<double>(bytes);
    size_t idx = 0;
    while (value >= 1024.0 && idx < (SUFFIX_COUNT - 1)) {
        value /= 1024.0;
        ++idx;
    }

    char buffer[32];
    if (idx == 0) {
        snprintf(buffer,
                 sizeof(buffer),
                 "%llu %s",
                 static_cast<unsigned long long>(bytes),
                 SUFFIXES[idx]);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f %s", value, SUFFIXES[idx]);
    }
    return std::string(buffer);
}
} // namespace

void SdExplorerScreen::build(lv_obj_t* parent) {
    if (!parent) {
        return;
    }

    root = lv_obj_create(parent);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 14, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x111827), 0);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(root, 10, 0);

    lv_obj_t* header = lv_label_create(root);
    lv_label_set_text_static(header, UI_SYMBOL_STORAGE " SD Explorer");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0xf3f4f6), 0);

    lv_obj_t* info_card = lv_obj_create(root);
    lv_obj_set_width(info_card, lv_pct(100));
    lv_obj_set_style_bg_color(info_card, lv_color_hex(0x1f2937), 0);
    lv_obj_set_style_border_width(info_card, 0, 0);
    lv_obj_set_style_radius(info_card, 14, 0);
    lv_obj_set_style_pad_all(info_card, 12, 0);
    lv_obj_set_layout(info_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(info_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(info_card, 4, 0);

    status_label = lv_label_create(info_card);
    lv_label_set_text_static(status_label, "Status: --");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);

    capacity_label = lv_label_create(info_card);
    lv_label_set_text_static(capacity_label, "Used: -- / --");
    lv_obj_set_style_text_font(capacity_label, &lv_font_montserrat_16, 0);

    type_label = lv_label_create(info_card);
    lv_label_set_text_static(type_label, "Card: --");
    lv_obj_set_style_text_font(type_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(type_label, lv_color_hex(0x9ca3af), 0);

    lv_obj_t* controls = lv_obj_create(root);
    lv_obj_set_width(controls, lv_pct(100));
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_layout(controls, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* refresh_btn = lv_btn_create(controls);
    lv_obj_add_event_cb(refresh_btn, onRefreshClicked, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(refresh_btn, lv_color_hex(0x2563eb), LV_PART_MAIN);
    lv_obj_set_style_radius(refresh_btn, 12, 0);

    lv_obj_t* refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text_static(refresh_label, UI_SYMBOL_REFRESH " Refresh");
    lv_obj_set_style_text_color(refresh_label, lv_color_hex(0xf8fafc), 0);
    lv_obj_center(refresh_label);

    lv_obj_t* format_btn = lv_btn_create(controls);
    lv_obj_add_event_cb(format_btn, onFormatClicked, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color(format_btn, lv_color_hex(0xb91c1c), LV_PART_MAIN);
    lv_obj_set_style_radius(format_btn, 12, 0);

    lv_obj_t* format_label = lv_label_create(format_btn);
    lv_label_set_text_static(format_label, UI_SYMBOL_TRASH " Format");
    lv_obj_set_style_text_color(format_label, lv_color_hex(0xfef2f2), 0);
    lv_obj_center(format_label);

    message_label = lv_label_create(root);
    lv_label_set_text_static(message_label, "Insert microSD card to browse files.");
    lv_obj_set_style_text_font(message_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(message_label, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_width(message_label, lv_pct(100));
    lv_label_set_long_mode(message_label, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(message_label, LV_OBJ_FLAG_HIDDEN);

    file_list = lv_list_create(root);
    lv_obj_set_size(file_list, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(file_list, 1);
    lv_obj_set_style_bg_color(file_list, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_border_width(file_list, 0, 0);
    lv_obj_add_flag(file_list, LV_OBJ_FLAG_SCROLLABLE);
}

void SdExplorerScreen::onShow() {
    Logger::getInstance().info("[SD] Explorer screen shown");
    refreshCardInfo();
    if (!refresh_timer) {
        refresh_timer = lv_timer_create(onTimerTick, REFRESH_MS, this);
    }
}

void SdExplorerScreen::onHide() {
    Logger::getInstance().info("[SD] Explorer screen hidden");
    if (refresh_timer) {
        lv_timer_del(refresh_timer);
        refresh_timer = nullptr;
    }
}

void SdExplorerScreen::refreshCardInfo() {
    SdCardDriver& driver = SdCardDriver::getInstance();
    if (!driver.isMounted()) {
        driver.begin();
    } else {
        driver.refreshStats();
    }

    if (!status_label || !capacity_label || !type_label) {
        return;
    }

    if (driver.isMounted()) {
        hideMessage();
        lv_label_set_text_fmt(status_label, "Status: %s", "Mounted");
        lv_label_set_text_fmt(type_label, "Card: %s", driver.cardTypeString().c_str());
        lv_label_set_text_fmt(capacity_label,
                              "Used: %s / %s",
                              formatBytes(driver.usedBytes()).c_str(),
                              formatBytes(driver.totalBytes()).c_str());

        if (file_list) {
            lv_obj_clear_flag(file_list, LV_OBJ_FLAG_HIDDEN);
            populateFileList();
        }
    } else {
        lv_label_set_text_fmt(status_label, "Status: %s", "Missing");
        lv_label_set_text_fmt(capacity_label, "Used: -- / --");
        lv_label_set_text_fmt(type_label, "Card: --");
        showMessage(driver.lastError().empty() ? "Insert microSD card to browse files."
                                               : driver.lastError().c_str());
        if (file_list) {
            lv_obj_clean(file_list);
            lv_obj_add_flag(file_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void SdExplorerScreen::populateFileList() {
    if (!file_list) {
        return;
    }

    SdCardDriver& driver = SdCardDriver::getInstance();
    auto entries = driver.listDirectory("/");
    lv_obj_clean(file_list);

    if (entries.empty()) {
        showMessage("No files found in /");
        return;
    }

    hideMessage();
    for (const auto& entry : entries) {
        std::string label = entry.name;
        if (entry.isDirectory) {
            label += "/";
        } else {
            label += " (" + formatBytes(entry.sizeBytes) + ")";
        }
        const char* icon = entry.isDirectory ? UI_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
        lv_list_add_btn(file_list, icon, label.c_str());
    }
}

void SdExplorerScreen::showMessage(const char* text) {
    if (!message_label) {
        return;
    }
    lv_label_set_text(message_label, text ? text : "");
    lv_obj_clear_flag(message_label, LV_OBJ_FLAG_HIDDEN);
    if (file_list) {
        lv_obj_add_flag(file_list, LV_OBJ_FLAG_HIDDEN);
    }
}

void SdExplorerScreen::hideMessage() {
    if (message_label) {
        lv_obj_add_flag(message_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (file_list) {
        lv_obj_clear_flag(file_list, LV_OBJ_FLAG_HIDDEN);
    }
}

void SdExplorerScreen::onRefreshClicked(lv_event_t* event) {
    if (!event || event->code != LV_EVENT_CLICKED) {
        return;
    }
    auto* screen = static_cast<SdExplorerScreen*>(lv_event_get_user_data(event));
    if (screen) {
        screen->refreshCardInfo();
    }
}

void SdExplorerScreen::performFormat() {
    SdCardDriver& driver = SdCardDriver::getInstance();
    showMessage("Formatting card...");
    if (driver.formatCard()) {
        showMessage("Format complete.");
        refreshCardInfo();
    } else {
        const char* err =
            driver.lastError().empty() ? "Format failed." : driver.lastError().c_str();
        showMessage(err);
    }
}

void SdExplorerScreen::onFormatClicked(lv_event_t* event) {
    if (!event || event->code != LV_EVENT_CLICKED) {
        return;
    }
    auto* screen = static_cast<SdExplorerScreen*>(lv_event_get_user_data(event));
    if (!screen) {
        return;
    }
    if (screen->pending_msgbox) {
        lv_obj_del(screen->pending_msgbox);
        screen->pending_msgbox = nullptr;
    }
    static const char* btns[] = {"Cancel", "Format", ""};
    lv_obj_t* mbox = lv_msgbox_create(nullptr,
                                      "Format SD",
                                      "Delete ALL files on the card?",
                                      btns,
                                      true);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, onFormatConfirm, LV_EVENT_VALUE_CHANGED, screen);
    screen->pending_msgbox = mbox;
}

void SdExplorerScreen::onFormatConfirm(lv_event_t* event) {
    if (!event || event->code != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    auto* screen = static_cast<SdExplorerScreen*>(lv_event_get_user_data(event));
    if (!screen) {
        return;
    }
    lv_obj_t* mbox = event->current_target;
    const char* btn_txt = lv_msgbox_get_active_btn_text(mbox);
    if (btn_txt && strcmp(btn_txt, "Format") == 0) {
        screen->performFormat();
    }
    lv_obj_del(mbox);
    screen->pending_msgbox = nullptr;
}

void SdExplorerScreen::onTimerTick(lv_timer_t* timer) {
    if (!timer) {
        return;
    }
    auto* screen = static_cast<SdExplorerScreen*>(timer->user_data);
    if (screen) {
        screen->refreshCardInfo();
    }
}
