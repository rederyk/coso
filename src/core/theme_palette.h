#pragma once

#include <cstdint>
#include <string>

struct ThemePalette {
    std::string name;
    uint32_t primary = 0;
    uint32_t accent = 0;
    uint32_t card = 0;
    uint32_t dock = 0;
    uint32_t dockIconBackground = 0;
    uint32_t dockIconSymbol = 0;
    uint8_t dockIconRadius = 24;
};
