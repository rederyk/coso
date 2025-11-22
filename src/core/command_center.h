#pragma once

#include <functional>
#include <string>
#include <vector>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct CommandResult {
    bool success{false};
    std::string message;

    CommandResult() = default;
    CommandResult(bool ok, std::string msg) : success(ok), message(std::move(msg)) {}
};

struct CommandInfo {
    std::string name;
    std::string description;
};

class CommandCenter {
public:
    using CommandHandler = std::function<CommandResult(const std::vector<std::string>& args)>;

    static CommandCenter& getInstance();

    bool registerCommand(const std::string& name,
                         const std::string& description,
                         CommandHandler handler);

    CommandResult executeCommand(const std::string& name,
                                 const std::vector<std::string>& args) const;

    std::vector<CommandInfo> listCommands() const;

private:
    CommandCenter();

    struct CommandEntry {
        std::string name;
        std::string description;
        CommandHandler handler;
    };

    void registerBuiltins();

    std::vector<CommandEntry> commands_;
    SemaphoreHandle_t mutex_;
};
