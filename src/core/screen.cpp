#include "core/screen.h"

Screen::~Screen() {
    destroyRoot();
}

void Screen::destroyRoot() {
    if (root) {
        lv_obj_del(root);
        root = nullptr;
    }
}
