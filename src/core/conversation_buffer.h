#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

struct ConversationEntry {
    std::string role;
    std::string text;
    std::string output;
    uint32_t timestamp = 0;
    std::string command;
    std::vector<std::string> args;
    std::string transcription;
};

class ConversationBuffer {
public:
    static ConversationBuffer& getInstance();

    bool begin();

    bool addUserMessage(const std::string& text, const std::string& transcription = std::string());
    bool addAssistantMessage(const std::string& response_text,
                             const std::string& command,
                             const std::vector<std::string>& args,
                             const std::string& transcription = std::string(),
                             const std::string& output = std::string());
    bool addEntry(ConversationEntry entry);

    bool clear();
    bool setLimit(size_t new_limit);
    size_t getLimit() const;
    size_t size() const;

    std::vector<ConversationEntry> getEntries() const;

private:
    ConversationBuffer() = default;
    ConversationBuffer(const ConversationBuffer&) = delete;
    ConversationBuffer& operator=(const ConversationBuffer&) = delete;

    bool ensureReady() const;
    bool loadLocked();
    bool persistLocked();

    static constexpr const char* FILE_PATH = "/assistant_conversation.json";
    static constexpr size_t DEFAULT_LIMIT = 30;
    static constexpr size_t MIN_LIMIT = 10;
    static constexpr size_t MAX_LIMIT = 100;

    mutable std::mutex mutex_;
    mutable bool initialized_ = false;
    size_t limit_ = DEFAULT_LIMIT;
    std::deque<ConversationEntry> entries_;
};
