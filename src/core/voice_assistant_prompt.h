#pragma once

#include <string>
#include <vector>

/**
 * Shared voice assistant constants, placeholders are replaced at runtime.
 * The descriptive prompt now lives on LittleFS at /voice_assistant_prompt.json.
 */
constexpr const char* VOICE_ASSISTANT_COMMAND_LIST_PLACEHOLDER = "{{COMMAND_LIST}}";
constexpr const char* VOICE_ASSISTANT_BLE_HOSTS_PLACEHOLDER = "{{BLE_HOSTS}}";

struct VoiceAssistantPromptDefinition {
    std::string prompt_template;
    std::vector<std::string> sections;
};

extern const char VOICE_ASSISTANT_PROMPT_JSON_PATH[];
