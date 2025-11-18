#pragma once

#include <vector>

#include "core/screen.h"

class ScreenManager {
public:
    static ScreenManager* getInstance();
    bool pushScreen(Screen* screen);

private:
    ScreenManager() = default;
    static ScreenManager* instance;
    std::vector<Screen*> stack;
};
