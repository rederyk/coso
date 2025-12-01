#include "core/command_center.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include "drivers/sd_card_driver.h"
#include "drivers/rgb_led_driver.h"
#include "core/audio_manager.h"
#include "core/backlight_manager.h"
#include "utils/logger.h"

namespace {
constexpr size_t LOG_TAIL_LINES = 10;
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
