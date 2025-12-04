#include "core/conversation_buffer.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "core/storage_manager.h"
#include "utils/logger.h"

namespace {
constexpr const char* TAG = "ConversationBuffer";
}

constexpr size_t ConversationBuffer::DEFAULT_LIMIT;
constexpr size_t ConversationBuffer::MIN_LIMIT;
constexpr size_t ConversationBuffer::MAX_LIMIT;

ConversationBuffer& ConversationBuffer::getInstance() {
    static ConversationBuffer instance;
    return instance;
}

bool ConversationBuffer::begin() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return true;
    }

    StorageManager::getInstance().begin();

    if (LittleFS.exists(FILE_PATH)) {
        if (!loadLocked()) {
            Logger::getInstance().warnf("[%s] Failed to read existing buffer, starting fresh", TAG);
            entries_.clear();
            limit_ = DEFAULT_LIMIT;
        }
    } else {
        entries_.clear();
        limit_ = DEFAULT_LIMIT;
        persistLocked();
    }

    initialized_ = true;
    return true;
}

bool ConversationBuffer::ensureReady() const {
    if (initialized_) {
        return true;
    }
    return const_cast<ConversationBuffer*>(this)->begin();
}

bool ConversationBuffer::addUserMessage(const std::string& text, const std::string& transcription) {
    if (text.empty()) {
        return false;
    }
    ConversationEntry entry;
    entry.role = "user";
    entry.text = text;
    entry.transcription = transcription;
    return addEntry(std::move(entry));
}

bool ConversationBuffer::addAssistantMessage(const std::string& response_text,
                                             const std::string& command,
                                             const std::vector<std::string>& args,
                                             const std::string& transcription,
                                             const std::string& output) {
    if (response_text.empty() && command.empty()) {
        return false;
    }
    ConversationEntry entry;
    entry.role = "assistant";
    entry.text = response_text;
    entry.command = command;
    entry.args = args;
    entry.transcription = transcription;
    entry.output = output;
    return addEntry(std::move(entry));
}

bool ConversationBuffer::addEntry(ConversationEntry entry) {
    if (!ensureReady()) {
        Logger::getInstance().warnf("[%s] Storage not ready, skipping entry", TAG);
        return false;
    }

    if (entry.timestamp == 0) {
        entry.timestamp = millis();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(std::move(entry));
    while (entries_.size() > limit_) {
        entries_.pop_front();
    }
    return persistLocked();
}

bool ConversationBuffer::clear() {
    if (!ensureReady()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    return persistLocked();
}

bool ConversationBuffer::setLimit(size_t new_limit) {
    if (!ensureReady()) {
        return false;
    }

    size_t clamped = std::max(MIN_LIMIT, std::min(MAX_LIMIT, new_limit));

    std::lock_guard<std::mutex> lock(mutex_);
    if (clamped == limit_) {
        return true;
    }

    limit_ = clamped;
    while (entries_.size() > limit_) {
        entries_.pop_front();
    }
    return persistLocked();
}

size_t ConversationBuffer::getLimit() const {
    if (!ensureReady()) {
        return DEFAULT_LIMIT;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return limit_;
}

size_t ConversationBuffer::size() const {
    if (!ensureReady()) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

std::vector<ConversationEntry> ConversationBuffer::getEntries() const {
    std::vector<ConversationEntry> copy;
    if (!ensureReady()) {
        return copy;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    copy.assign(entries_.begin(), entries_.end());
    return copy;
}

bool ConversationBuffer::loadLocked() {
    File file = LittleFS.open(FILE_PATH, FILE_READ);
    if (!file) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        Logger::getInstance().errorf("[%s] JSON parse error: %s", TAG, err.c_str());
        return false;
    }

    size_t loaded_limit = doc["limit"] | DEFAULT_LIMIT;
    limit_ = std::max(MIN_LIMIT, std::min(MAX_LIMIT, loaded_limit));

    entries_.clear();
    JsonArrayConst arr = doc["messages"].as<JsonArrayConst>();
    if (!arr.isNull()) {
        for (JsonObjectConst item : arr) {
            ConversationEntry entry;
            entry.role = item["role"].as<const char*>() ? item["role"].as<const char*>() : "";
            entry.text = item["text"].as<const char*>() ? item["text"].as<const char*>() : "";
            entry.timestamp = item["timestamp"] | 0u;
            entry.command = item["command"].as<const char*>() ? item["command"].as<const char*>() : "";
            entry.output = item["output"].as<const char*>() ? item["output"].as<const char*>() : "";
            entry.transcription = item["transcription"].as<const char*>() ? item["transcription"].as<const char*>() : "";

            JsonArrayConst args = item["args"].as<JsonArrayConst>();
            if (!args.isNull()) {
                for (JsonVariantConst arg : args) {
                    if (arg.is<const char*>()) {
                        entry.args.emplace_back(arg.as<const char*>());
                    }
                }
            }

            if (!entry.role.empty() || !entry.text.empty()) {
                entries_.push_back(std::move(entry));
            }
        }
    }

    while (entries_.size() > limit_) {
        entries_.pop_front();
    }

    return true;
}

bool ConversationBuffer::persistLocked() {
    JsonDocument doc;
    doc["limit"] = static_cast<uint32_t>(limit_);
    JsonArray arr = doc["messages"].to<JsonArray>();
    for (const auto& entry : entries_) {
        JsonObject obj = arr.add<JsonObject>();
        obj["role"] = entry.role.c_str();
        obj["text"] = entry.text.c_str();
        obj["timestamp"] = entry.timestamp;
        if (!entry.command.empty()) {
            obj["command"] = entry.command.c_str();
        }
        if (!entry.output.empty()) {
            obj["output"] = entry.output.c_str();
        }
        if (!entry.transcription.empty()) {
            obj["transcription"] = entry.transcription.c_str();
        }
        if (!entry.args.empty()) {
            JsonArray args = obj.createNestedArray("args");
            for (const auto& arg : entry.args) {
                args.add(arg.c_str());
            }
        }
    }

    File file = LittleFS.open(FILE_PATH, FILE_WRITE);
    if (!file) {
        Logger::getInstance().errorf("[%s] Unable to open %s for writing", TAG, FILE_PATH);
        return false;
    }

    const bool ok = serializeJson(doc, file) > 0;
    file.close();
    if (!ok) {
        Logger::getInstance().errorf("[%s] Failed to serialize JSON", TAG);
    }
    return ok;
}
