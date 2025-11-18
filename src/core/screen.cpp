#include "core/screen.h"

Screen::~Screen() {
    if (root) {
        lv_obj_del(root);
        root = nullptr;
    }
}
