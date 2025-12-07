#pragma once

#include <string>
#include <vector>

/**
 * Shared voice assistant constants, placeholders are replaced at runtime.
 * The descriptive prompt now lives on LittleFS at /voice_assistant_prompt.json.
 */
constexpr const char* VOICE_ASSISTANT_COMMAND_LIST_PLACEHOLDER = "{{COMMAND_LIST}}";
constexpr const char* VOICE_ASSISTANT_LUA_API_LIST_PLACEHOLDER = "{{LUA_API_LIST}}";
constexpr const char* VOICE_ASSISTANT_BLE_HOSTS_PLACEHOLDER = "{{BLE_HOSTS}}";

struct AutoPopulateCommand {
    std::string command;
    std::vector<std::string> args;
};

struct VoiceAssistantPromptDefinition {
    std::string prompt_template;
    std::vector<std::string> sections;
    std::vector<AutoPopulateCommand> auto_populate;
};

extern const char VOICE_ASSISTANT_PROMPT_JSON_PATH[];
