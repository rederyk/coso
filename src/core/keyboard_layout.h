#pragma once

#include <cstdint>
#include <map>

struct KeyInfo {
    uint8_t keycode;
    uint8_t modifier;
};

class KeyboardLayout {
public:
    KeyboardLayout(const std::map<uint16_t, KeyInfo>& mapping)
        : layout_mapping(mapping) {}

    KeyInfo getKey(uint16_t btn_id) const {
        auto it = layout_mapping.find(btn_id);
        if (it != layout_mapping.end()) {
            return it->second;
        }
        return {0, 0}; // Return 0 if not found
    }

private:
    const std::map<uint16_t, KeyInfo> layout_mapping;
};
