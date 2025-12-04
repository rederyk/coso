#include "core/command_center.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include "drivers/sd_card_driver.h"
#include "drivers/rgb_led_driver.h"
#include "core/ble_hid_manager.h"
#include "core/audio_manager.h"
#include "core/backlight_manager.h"
#include "screens/ble_manager.h"
#include "utils/logger.h"

namespace {
constexpr size_t LOG_TAIL_LINES = 10;
constexpr uint8_t HID_MOD_CTRL = 0x01;
constexpr uint8_t HID_MOD_SHIFT = 0x02;
constexpr uint8_t HID_MOD_ALT = 0x04;
constexpr uint8_t HID_MOD_GUI = 0x08;

constexpr uint8_t HID_KEY_ENTER = 0x28;
constexpr uint8_t HID_KEY_ESC = 0x29;
constexpr uint8_t HID_KEY_BACKSPACE = 0x2A;
constexpr uint8_t HID_KEY_TAB = 0x2B;
constexpr uint8_t HID_KEY_SPACE = 0x2C;
constexpr uint8_t HID_KEY_INSERT = 0x49;
constexpr uint8_t HID_KEY_HOME = 0x4A;
constexpr uint8_t HID_KEY_PAGE_UP = 0x4B;
constexpr uint8_t HID_KEY_DELETE = 0x4C;
constexpr uint8_t HID_KEY_END = 0x4D;
constexpr uint8_t HID_KEY_PAGE_DOWN = 0x4E;
constexpr uint8_t HID_KEY_ARROW_RIGHT = 0x4F;
constexpr uint8_t HID_KEY_ARROW_LEFT = 0x50;
constexpr uint8_t HID_KEY_ARROW_DOWN = 0x51;
constexpr uint8_t HID_KEY_ARROW_UP = 0x52;
constexpr uint8_t HID_KEY_CAPS_LOCK = 0x39;
constexpr uint8_t HID_KEY_PRINT_SCREEN = 0x46;
constexpr uint8_t HID_KEY_SCROLL_LOCK = 0x47;
constexpr uint8_t HID_KEY_PAUSE = 0x48;
constexpr uint8_t HID_KEY_NUM_LOCK = 0x53;
constexpr uint8_t HID_KEY_MENU = 0x65;

std::string joinArgs(const std::vector<std::string>& args, size_t start) {
    if (start >= args.size()) {
        return {};
    }

    std::string out;
    for (size_t i = start; i < args.size(); ++i) {
        if (!out.empty()) {
            out.push_back(' ');
        }
        out += args[i];
    }
    return out;
}

std::string normalizeMacAddress(const std::string& raw) {
    std::string hex;
    hex.reserve(12);

    for (char c : raw) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isxdigit(uc)) {
            hex.push_back(static_cast<char>(std::toupper(uc)));
        } else if (uc == ':' || uc == '-' || std::isspace(uc)) {
            continue;
        } else {
            return {};
        }
    }

    if (hex.size() != 12) {
        return {};
    }

    std::string normalized;
    normalized.reserve(17);
    for (size_t i = 0; i < 6; ++i) {
        if (i > 0) {
            normalized.push_back(':');
        }
        normalized.push_back(hex[i * 2]);
        normalized.push_back(hex[i * 2 + 1]);
    }
    return normalized;
}

CommandResult validateBleTargetHost(const std::string& raw_mac, std::string& normalized_mac) {
    BleHidManager& ble = BleHidManager::getInstance();

    if (!ble.isInitialized()) {
        return {false, "BLE stack not initialized"};
    }
    if (!ble.isEnabled()) {
        return {false, "BLE disabled"};
    }

    normalized_mac = normalizeMacAddress(raw_mac);
    if (normalized_mac.empty()) {
        return {false, "Invalid MAC address"};
    }

    const auto bonded = ble.getBondedPeers();
    if (bonded.empty()) {
        return {false, "No bonded BLE hosts"};
    }

    auto match = std::find_if(bonded.begin(), bonded.end(),
        [&](const BleHidManager::BondedPeer& peer) {
            const std::string candidate = normalizeMacAddress(peer.address.toString());
            return !candidate.empty() && candidate == normalized_mac;
        });

    if (match == bonded.end()) {
        return {false, "Host " + normalized_mac + " not bonded"};
    }

    if (!match->isConnected) {
        return {false, "Host " + normalized_mac + " not connected"};
    }

    normalized_mac = match->address.toString();
    return {true, ""};
}

bool parseByteToken(const std::string& token, uint8_t& value) {
    if (token.empty()) {
        return false;
    }
    char* end = nullptr;
    unsigned long parsed = std::strtoul(token.c_str(), &end, 0);
    if (!end || *end != '\0') {
        return false;
    }
    if (parsed > 0xFF) {
        return false;
    }
    value = static_cast<uint8_t>(parsed);
    return true;
}

bool parseInt8Token(const std::string& token, int8_t& value) {
    if (token.empty()) {
        return false;
    }
    char* end = nullptr;
    long parsed = std::strtol(token.c_str(), &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    if (parsed < -128 || parsed > 127) {
        return false;
    }
    value = static_cast<int8_t>(parsed);
    return true;
}

bool parseMouseButtonsToken(const std::string& token, uint8_t& mask) {
    if (parseByteToken(token, mask)) {
        return true;
    }

    std::string lower;
    lower.reserve(token.size());
    for (char c : token) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isspace(uc)) {
            continue;
        }
        lower.push_back(static_cast<char>(std::tolower(uc)));
    }

    if (lower.empty()) {
        return false;
    }

    auto decode = [](const std::string& part, uint8_t& bit) -> bool {
        if (part == "left" || part == "l" || part == "primary") {
            bit = 0x01;
            return true;
        }
        if (part == "right" || part == "r" || part == "secondary") {
            bit = 0x02;
            return true;
        }
        if (part == "middle" || part == "m" || part == "wheel") {
            bit = 0x04;
            return true;
        }
        return false;
    };

    size_t start = 0;
    mask = 0;
    while (start < lower.size()) {
        size_t pos = lower.find('+', start);
        std::string part = lower.substr(start, pos - start);
        if (part.empty()) {
            return false;
        }
        uint8_t bit = 0;
        if (!decode(part, bit)) {
            return false;
        }
        mask |= bit;
        if (pos == std::string::npos) {
            break;
        }
        start = pos + 1;
    }

    return mask != 0;
}

std::string trimCopy(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string canonicalizeKeyToken(const std::string& token) {
    std::string out;
    out.reserve(token.size());
    for (char c : token) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            out.push_back(static_cast<char>(std::tolower(uc)));
        }
    }
    return out;
}

struct KeyNameEntry {
    const char* name;
    uint8_t keycode;
    uint8_t implicit_modifier;
};

constexpr KeyNameEntry NAMED_KEY_ENTRIES[] = {
    {"enter", HID_KEY_ENTER, 0},
    {"return", HID_KEY_ENTER, 0},
    {"escape", HID_KEY_ESC, 0},
    {"esc", HID_KEY_ESC, 0},
    {"space", HID_KEY_SPACE, 0},
    {"spacebar", HID_KEY_SPACE, 0},
    {"tab", HID_KEY_TAB, 0},
    {"backspace", HID_KEY_BACKSPACE, 0},
    {"delete", HID_KEY_DELETE, 0},
    {"del", HID_KEY_DELETE, 0},
    {"insert", HID_KEY_INSERT, 0},
    {"ins", HID_KEY_INSERT, 0},
    {"home", HID_KEY_HOME, 0},
    {"end", HID_KEY_END, 0},
    {"pageup", HID_KEY_PAGE_UP, 0},
    {"pgup", HID_KEY_PAGE_UP, 0},
    {"pagedown", HID_KEY_PAGE_DOWN, 0},
    {"pgdown", HID_KEY_PAGE_DOWN, 0},
    {"arrowup", HID_KEY_ARROW_UP, 0},
    {"up", HID_KEY_ARROW_UP, 0},
    {"arrowdown", HID_KEY_ARROW_DOWN, 0},
    {"down", HID_KEY_ARROW_DOWN, 0},
    {"arrowleft", HID_KEY_ARROW_LEFT, 0},
    {"left", HID_KEY_ARROW_LEFT, 0},
    {"arrowright", HID_KEY_ARROW_RIGHT, 0},
    {"right", HID_KEY_ARROW_RIGHT, 0},
    {"capslock", HID_KEY_CAPS_LOCK, 0},
    {"printscreen", HID_KEY_PRINT_SCREEN, 0},
    {"prtsc", HID_KEY_PRINT_SCREEN, 0},
    {"scrolllock", HID_KEY_SCROLL_LOCK, 0},
    {"pause", HID_KEY_PAUSE, 0},
    {"break", HID_KEY_PAUSE, 0},
    {"numlock", HID_KEY_NUM_LOCK, 0},
    {"menu", HID_KEY_MENU, 0}
};

bool lookupNamedKeyCode(const std::string& canonical, uint8_t& keycode, uint8_t& implicit_modifier) {
    implicit_modifier = 0;

    if (canonical.size() == 1) {
        char c = canonical[0];
        if (c >= 'a' && c <= 'z') {
            keycode = static_cast<uint8_t>(0x04 + (c - 'a'));
            return true;
        }
        if (c >= '1' && c <= '9') {
            keycode = static_cast<uint8_t>(0x1D + (c - '0'));
            return true;
        }
        if (c == '0') {
            keycode = 0x27;
            return true;
        }
    }

    if (canonical.size() > 1 && canonical[0] == 'f') {
        char* end = nullptr;
        long number = std::strtol(canonical.c_str() + 1, &end, 10);
        if (end && *end == '\0' && number >= 1 && number <= 12) {
            keycode = static_cast<uint8_t>(0x3A + (number - 1));
            return true;
        }
    }

    for (const auto& entry : NAMED_KEY_ENTRIES) {
        if (canonical == entry.name) {
            keycode = entry.keycode;
            implicit_modifier = entry.implicit_modifier;
            return true;
        }
    }

    return false;
}

bool parseKeyComboToken(const std::string& token, uint8_t& keycode, uint8_t& modifier) {
    std::string trimmed = trimCopy(token);
    if (trimmed.empty()) {
        return false;
    }

    keycode = 0;
    modifier = 0;
    bool found_key = false;

    size_t start = 0;
    while (start < trimmed.size()) {
        size_t plus_pos = trimmed.find('+', start);
        std::string part = trimCopy(trimmed.substr(start, plus_pos - start));
        if (part.empty()) {
            return false;
        }

        std::string canonical = canonicalizeKeyToken(part);
        if (canonical.empty()) {
            return false;
        }

        if (canonical == "ctrl" || canonical == "control") {
            modifier |= HID_MOD_CTRL;
        } else if (canonical == "shift") {
            modifier |= HID_MOD_SHIFT;
        } else if (canonical == "alt" || canonical == "option") {
            modifier |= HID_MOD_ALT;
        } else if (canonical == "super" || canonical == "meta" || canonical == "cmd" ||
                   canonical == "win" || canonical == "gui") {
            modifier |= HID_MOD_GUI;
        } else {
            uint8_t implicit_modifier = 0;
            uint8_t candidate = 0;
            if (!lookupNamedKeyCode(canonical, candidate, implicit_modifier)) {
                return false;
            }
            if (found_key) {
                return false;
            }
            keycode = candidate;
            modifier |= implicit_modifier;
            found_key = true;
        }

        if (plus_pos == std::string::npos) {
            break;
        }
        start = plus_pos + 1;
    }

    return found_key;
}

bool parseKeyToken(const std::string& token, uint8_t& keycode, uint8_t& modifier) {
    modifier = 0;
    if (parseByteToken(token, keycode)) {
        return true;
    }
    return parseKeyComboToken(token, keycode, modifier);
}
}  // namespace

CommandCenter& CommandCenter::getInstance() {
    static CommandCenter instance;
    return instance;
}

CommandCenter::CommandCenter()
    : mutex_(xSemaphoreCreateMutex()) {
    registerBuiltins();
}

bool CommandCenter::registerCommand(const std::string& name,
                                    const std::string& description,
                                    CommandHandler handler) {
    if (!handler || name.empty() || !mutex_) {
        return false;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }

    // Prevent duplicate registration
    auto exists = std::any_of(commands_.begin(), commands_.end(), [&](const CommandEntry& entry) {
        return entry.name == name;
    });

    if (exists) {
        xSemaphoreGive(mutex_);
        return false;
    }

    commands_.push_back(CommandEntry{
        .name = name,
        .description = description,
        .handler = std::move(handler)
    });

    xSemaphoreGive(mutex_);
    return true;
}

CommandResult CommandCenter::executeCommand(const std::string& name,
                                            const std::vector<std::string>& args) const {
    if (!mutex_) {
        return {false, "CommandCenter not initialized"};
    }

    CommandEntry entry{};
    bool found = false;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        auto it = std::find_if(commands_.begin(), commands_.end(), [&](const CommandEntry& cmd) {
            return cmd.name == name;
        });
        if (it != commands_.end()) {
            entry = *it;
            found = true;
        }
        xSemaphoreGive(mutex_);
    }

    if (!found) {
        return {false, "Command not found"};
    }

    return entry.handler(args);
}

std::vector<CommandInfo> CommandCenter::listCommands() const {
    std::vector<CommandInfo> out;

    if (!mutex_) {
        return out;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        out.reserve(commands_.size());
        for (const auto& entry : commands_) {
            out.push_back(CommandInfo{
                .name = entry.name,
                .description = entry.description
            });
        }
        xSemaphoreGive(mutex_);
    }

    return out;
}

void CommandCenter::registerBuiltins() {
    // Ping
    registerCommand("ping", "Simple connectivity check", [](const std::vector<std::string>&) {
        return CommandResult{true, "pong"};
    });

    // Uptime
    registerCommand("uptime", "Return uptime in seconds", [](const std::vector<std::string>&) {
        const uint64_t micros = esp_timer_get_time();
        const uint64_t seconds = micros / 1000000ULL;
        return CommandResult{true, "uptime_seconds=" + std::to_string(seconds)};
    });

    // Heap/PSRAM stats
    registerCommand("heap", "Current heap and PSRAM stats", [](const std::vector<std::string>&) {
        const size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        std::string msg = "heap_free=" + std::to_string(heap_free) +
                          " heap_largest=" + std::to_string(heap_largest);
        msg += " psram_free=" + std::to_string(psram_free) +
               " psram_largest=" + std::to_string(psram_largest);
        return CommandResult{true, msg};
    });

    // SD card status
    registerCommand("sd_status", "Report SD card mount status", [](const std::vector<std::string>&) {
        SdCardDriver& sd = SdCardDriver::getInstance();
        if (sd.isMounted()) {
            std::string msg = "mounted=true total=" + std::to_string(sd.totalBytes()) +
                              " used=" + std::to_string(sd.usedBytes());
            return CommandResult{true, msg};
        }
        return CommandResult{false, "SD card not mounted"};
    });

    // Tail logs
    registerCommand("log_tail", "Return the last buffered log lines",
        [](const std::vector<std::string>& args) {
            size_t lines = LOG_TAIL_LINES;
            if (!args.empty()) {
                lines = std::max<size_t>(1, std::min<size_t>(50, std::strtoul(args[0].c_str(), nullptr, 10)));
            }

            auto logs = Logger::getInstance().getBufferedLogs();
            if (logs.empty()) {
                return CommandResult{true, "log buffer empty"};
            }

            std::string output;
            const size_t start = logs.size() > lines ? logs.size() - lines : 0;
            for (size_t i = start; i < logs.size(); ++i) {
                output += logs[i].c_str();
                if (i + 1 < logs.size()) {
                    output.push_back('\n');
                }
            }
            return CommandResult{true, output};
        });

    // Voice assistant commands

    // Radio control
    registerCommand("radio_play", "Play radio station by name (e.g., 'jazz', 'rock', 'news')",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                return CommandResult{false, "Usage: radio_play <genre/name>"};
            }
            std::string genre = args[0];
            // TODO: Implement radio selection logic
            std::string msg = "Playing " + genre + " station (placeholder)";
            return CommandResult{true, msg};
        });

    // WiFi control
    registerCommand("wifi_switch", "Switch to different WiFi network",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                return CommandResult{false, "Usage: wifi_switch <ssid>"};
            }
            std::string ssid = args[0];
            std::string msg = "Switching to WiFi '" + ssid + "' (confirmation required)";
            // TODO: Implement WiFi switching with confirmation
            return CommandResult{true, msg};
        });

    // Bluetooth pairing
    registerCommand("bt_pair", "Pair with Bluetooth device",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                return CommandResult{false, "Usage: bt_pair <device_name>"};
            }
            std::string device_name = args[0];
            std::string msg = "Pairing with '" + device_name + "' (placeholder)";
            // TODO: Implement BT pairing logic
            return CommandResult{true, msg};
        });

    registerCommand("bt_type", "Send text to a bonded BLE host",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                return CommandResult{false, "Usage: bt_type <host_mac> <text>"};
            }

            std::string normalized_mac;
            CommandResult validation = validateBleTargetHost(args[0], normalized_mac);
            if (!validation.success) {
                return validation;
            }

            std::string text = joinArgs(args, 1);
            if (text.empty()) {
                return CommandResult{false, "Text payload required"};
            }

            std::string preview = text;
            constexpr size_t kPreviewLen = 48;
            if (preview.size() > kPreviewLen) {
                preview = preview.substr(0, kPreviewLen) + "...";
            }

            if (!BleManager::getInstance().sendText(text, BleHidTarget::ALL, normalized_mac)) {
                return CommandResult{false, "BLE queue busy, try again"};
            }

            Logger::getInstance().infof("[CommandCenter] bt_type -> %s : %s",
                normalized_mac.c_str(), preview.c_str());
            return CommandResult{true, "Text sent to " + normalized_mac};
        });

    registerCommand("bt_send_key", "Send HID keycode to bonded BLE host",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                return CommandResult{false, "Usage: bt_send_key <host_mac> <keycode_or_combo> [modifier]"};
            }

            std::string normalized_mac;
            CommandResult validation = validateBleTargetHost(args[0], normalized_mac);
            if (!validation.success) {
                return validation;
            }

            uint8_t keycode = 0;
            uint8_t modifier = 0;
            if (!parseKeyToken(args[1], keycode, modifier)) {
                return CommandResult{false, "Invalid key token (use HID code or combo like ctrl+enter)"};
            }

            if (args.size() >= 3) {
                if (!parseByteToken(args[2], modifier)) {
                    return CommandResult{false, "Invalid modifier (use decimal or 0xNN)"};
                }
            }

            if (!BleManager::getInstance().sendKey(keycode, modifier, BleHidTarget::ALL, normalized_mac)) {
                return CommandResult{false, "BLE queue busy, try again"};
            }

            Logger::getInstance().infof("[CommandCenter] bt_send_key -> %s key=0x%02X mod=0x%02X",
                normalized_mac.c_str(), keycode, modifier);
            return CommandResult{true, "Key sent to " + normalized_mac};
        });

    registerCommand("bt_mouse_move", "Send relative mouse movement to BLE host",
        [](const std::vector<std::string>& args) {
            if (args.size() < 3) {
                return CommandResult{false, "Usage: bt_mouse_move <host_mac> <dx> <dy> [wheel] [buttons]"};
            }

            std::string normalized_mac;
            CommandResult validation = validateBleTargetHost(args[0], normalized_mac);
            if (!validation.success) {
                return validation;
            }

            int8_t dx = 0;
            int8_t dy = 0;
            if (!parseInt8Token(args[1], dx) || !parseInt8Token(args[2], dy)) {
                return CommandResult{false, "dx/dy must be integers between -128 and 127"};
            }

            int8_t wheel = 0;
            if (args.size() >= 4) {
                if (!parseInt8Token(args[3], wheel)) {
                    return CommandResult{false, "wheel must be integer between -128 and 127"};
                }
            }

            uint8_t buttons = 0;
            if (args.size() >= 5) {
                if (!parseMouseButtonsToken(args[4], buttons)) {
                    return CommandResult{false, "Invalid mouse buttons (use 0xNN or left/right/middle)"};
                }
            }

            if (!BleManager::getInstance().sendMouseMove(dx, dy, wheel, buttons, BleHidTarget::ALL, normalized_mac)) {
                return CommandResult{false, "BLE queue busy, try again"};
            }

            Logger::getInstance().infof("[CommandCenter] bt_mouse_move -> %s dx=%d dy=%d wheel=%d btn=%u",
                normalized_mac.c_str(), dx, dy, wheel, buttons);
            return CommandResult{true, "Mouse event sent to " + normalized_mac};
        });

    registerCommand("bt_click", "Send mouse click to BLE host",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                return CommandResult{false, "Usage: bt_click <host_mac> <buttons>"};
            }

            std::string normalized_mac;
            CommandResult validation = validateBleTargetHost(args[0], normalized_mac);
            if (!validation.success) {
                return validation;
            }

            uint8_t buttons = 0;
            if (!parseMouseButtonsToken(args[1], buttons)) {
                return CommandResult{false, "Invalid buttons (use 0xNN or left/right/middle)"};
            }
            if (buttons == 0) {
                return CommandResult{false, "Button mask must be > 0"};
            }

            if (!BleManager::getInstance().mouseClick(buttons, BleHidTarget::ALL, normalized_mac)) {
                return CommandResult{false, "BLE queue busy, try again"};
            }

            Logger::getInstance().infof("[CommandCenter] bt_click -> %s buttons=0x%02X",
                normalized_mac.c_str(), buttons);
            return CommandResult{true, "Click sent to " + normalized_mac};
        });

    // Volume control
    registerCommand("volume_up", "Increase audio volume",
        [](const std::vector<std::string>& args) {
            auto& audio_mgr = AudioManager::getInstance();
            int current = audio_mgr.getVolume();
            int new_volume = std::min(100, current + 10);
            audio_mgr.setVolume(new_volume);
            return CommandResult{true, "Volume set to " + std::to_string(new_volume) + "%"};
        });

    registerCommand("volume_down", "Decrease audio volume",
        [](const std::vector<std::string>& args) {
            auto& audio_mgr = AudioManager::getInstance();
            int current = audio_mgr.getVolume();
            int new_volume = std::max(0, current - 10);
            audio_mgr.setVolume(new_volume);
            return CommandResult{true, "Volume set to " + std::to_string(new_volume) + "%"};
        });

    // Brightness control
    registerCommand("brightness_up", "Increase display brightness",
        [](const std::vector<std::string>& args) {
            auto& backlight = BacklightManager::getInstance();
            uint8_t current = backlight.getBrightness();
            uint8_t new_brightness = std::min<uint8_t>(100, current + 20);
            backlight.setBrightness(new_brightness);
            return CommandResult{true, "Brightness set to " + std::to_string(new_brightness) + "%"};
        });

    registerCommand("brightness_down", "Decrease display brightness",
        [](const std::vector<std::string>& args) {
            auto& backlight = BacklightManager::getInstance();
            uint8_t current = backlight.getBrightness();
            uint8_t new_brightness = std::max<uint8_t>(10, current - 20);
            backlight.setBrightness(new_brightness);
            return CommandResult{true, "Brightness set to " + std::to_string(new_brightness) + "%"};
        });

    // LED brightness
    registerCommand("led_brightness", "Set LED brightness",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                return CommandResult{false, "Usage: led_brightness <percentage>"};
            }
            uint8_t brightness = std::min<uint8_t>(100, std::max<uint8_t>(0,
                static_cast<uint8_t>(std::strtoul(args[0].c_str(), nullptr, 10))));
            RgbLedManager::getInstance().setBrightness(brightness);
            return CommandResult{true, "LED brightness set to " + std::to_string(brightness) + "%"};
        });

    // System status
    registerCommand("system_status", "Get combined system status (heap, wifi, sd)",
        [](const std::vector<std::string>& args) {
            const size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

            SdCardDriver& sd = SdCardDriver::getInstance();
            std::string sd_status = sd.isMounted() ? "mounted" : "not_mounted";

            // TODO: WiFi status from WiFi manager
            std::string wifi_status = "unknown"; // Placeholder

            std::string msg = "heap_free=" + std::to_string(heap_free) +
                             " psram_free=" + std::to_string(psram_free) +
                             " sd_card=" + sd_status +
                             " wifi=" + wifi_status;
            return CommandResult{true, msg};
        });
}
