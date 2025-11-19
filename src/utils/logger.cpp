#include "utils/logger.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include <esp_heap_caps.h>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : buffer_(nullptr),
      next_index_(0),
      buffer_filled_(false),
      spinlock_(portMUX_INITIALIZER_UNLOCKED) {
    buffer_ = static_cast<BufferEntry*>(
        heap_caps_calloc(BUFFER_LINES, sizeof(BufferEntry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer_) {
        buffer_ = static_cast<BufferEntry*>(
            heap_caps_calloc(BUFFER_LINES, sizeof(BufferEntry), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
}

Logger::~Logger() {
    if (buffer_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }
}

void Logger::begin(unsigned long baud_rate) {
    Serial.begin(baud_rate);
}

void Logger::log(LogLevel level, const char* message) {
    if (!message) {
        return;
    }
    const uint32_t timestamp = millis();
    const String formatted = formatLine(level, message);
    Serial.println(formatted);
    appendToBuffer(message, level, timestamp);
}

void Logger::log(LogLevel level, const String& message) {
    log(level, message.c_str());
}

void Logger::logf(LogLevel level, const char* fmt, ...) {
    if (!fmt) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    logv(level, fmt, args);
    va_end(args);
}

void Logger::debug(const char* message) {
    log(LogLevel::Debug, message);
}

void Logger::debugf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Debug, fmt, args);
    va_end(args);
}

void Logger::info(const char* message) {
    log(LogLevel::Info, message);
}

void Logger::infof(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Info, fmt, args);
    va_end(args);
}

void Logger::warn(const char* message) {
    log(LogLevel::Warn, message);
}

void Logger::warnf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Warn, fmt, args);
    va_end(args);
}

void Logger::error(const char* message) {
    log(LogLevel::Error, message);
}

void Logger::errorf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Error, fmt, args);
    va_end(args);
}

void Logger::logv(LogLevel level, const char* fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed <= 0) {
        return;
    }

    std::vector<char> message(static_cast<size_t>(needed) + 1);
    vsnprintf(message.data(), message.size(), fmt, args);
    log(level, message.data());
}

void Logger::appendToBuffer(const String& message, LogLevel level, uint32_t timestamp) {
    if (!buffer_) {
        return;
    }
    const size_t len = std::min(static_cast<size_t>(message.length()), MAX_SERIAL_LINE - 1);
    portENTER_CRITICAL(&spinlock_);
    BufferEntry& entry = buffer_[next_index_];
    std::memcpy(entry.text.data(), message.c_str(), len);
    entry.text[len] = '\0';
    entry.length = len;
    entry.level = level;
    entry.timestamp = timestamp;
    next_index_ = (next_index_ + 1) % BUFFER_LINES;
    if (next_index_ == 0) {
        buffer_filled_ = true;
    }
    portEXIT_CRITICAL(&spinlock_);
}

String Logger::formatLine(LogLevel level, const char* message) const {
    char line[MAX_SERIAL_LINE];
    unsigned long timestamp = millis();
    snprintf(line,
             sizeof(line),
             "[%08lu ms] [%s] %s",
             timestamp,
             levelToString(level),
             message);
    return String(line);
}

const char* Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default:              return "LOG";
    }
}

const char* Logger::levelToShortString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug: return "D";
        case LogLevel::Info:  return "I";
        case LogLevel::Warn:  return "W";
        case LogLevel::Error: return "E";
        default:              return "?";
    }
}

String Logger::formatLineCompact(LogLevel level, uint32_t timestamp, const char* message) const {
    char line[MAX_SERIAL_LINE];
    // Format: [12345 I] Message
    // Compact format for small displays
    snprintf(line,
             sizeof(line),
             "[%05lu %s] %s",
             timestamp / 1000,  // Show seconds instead of milliseconds
             levelToShortString(level),
             message);
    return String(line);
}

std::vector<String> Logger::getBufferedLogs() const {
    if (!buffer_) {
        return {};
    }

    size_t snap_next_index;
    bool snap_filled;
    std::vector<BufferEntry> snapshot_buffer;

    // Single critical section to capture all state
    portENTER_CRITICAL(&spinlock_);
    snap_next_index = next_index_;
    snap_filled = buffer_filled_;
    const size_t count = snap_filled ? BUFFER_LINES : snap_next_index;
    snapshot_buffer.resize(count);

    // Copy all entries at once while in critical section
    for (size_t i = 0; i < count; ++i) {
        size_t index = snap_filled ? (snap_next_index + i) % BUFFER_LINES : i;
        snapshot_buffer[i] = buffer_[index];
    }
    portEXIT_CRITICAL(&spinlock_);

    // Build string vector outside critical section
    std::vector<String> logs;
    logs.reserve(count);
    for (const auto& entry : snapshot_buffer) {
        logs.emplace_back(entry.text.data());
    }

    return logs;
}

std::vector<String> Logger::getBufferedLogs(LogLevel min_level) const {
    if (!buffer_) {
        return {};
    }

    size_t snap_next_index;
    bool snap_filled;

    // Quick check of count
    portENTER_CRITICAL(&spinlock_);
    snap_next_index = next_index_;
    snap_filled = buffer_filled_;
    const size_t count = snap_filled ? BUFFER_LINES : snap_next_index;
    portEXIT_CRITICAL(&spinlock_);

    if (count == 0) {
        return {};
    }

    std::vector<BufferEntry> snapshot_buffer;
    snapshot_buffer.resize(count);

    // Copy all entries in critical section
    portENTER_CRITICAL(&spinlock_);
    for (size_t i = 0; i < count; ++i) {
        size_t index = snap_filled ? (snap_next_index + i) % BUFFER_LINES : i;
        snapshot_buffer[i] = buffer_[index];
    }
    portEXIT_CRITICAL(&spinlock_);

    // Build filtered string vector outside critical section
    std::vector<String> logs;
    logs.reserve(count);
    for (const auto& entry : snapshot_buffer) {
        if (entry.length > 0 && entry.level >= min_level) {
            // Use compact format for display
            logs.push_back(formatLineCompact(entry.level, entry.timestamp, entry.text.data()));
        }
    }

    return logs;
}

void Logger::clearBuffer() {
    if (!buffer_) {
        return;
    }
    portENTER_CRITICAL(&spinlock_);
    next_index_ = 0;
    buffer_filled_ = false;
    // Clear all entries
    for (size_t i = 0; i < BUFFER_LINES; ++i) {
        buffer_[i].length = 0;
        buffer_[i].text[0] = '\0';
    }
    portEXIT_CRITICAL(&spinlock_);
}

void Logger::dumpBufferToSerial() const {
    const auto snapshot = getBufferedLogs();
    Serial.println(F("[Logger] Dump buffered logs ↓"));
    for (const auto& line : snapshot) {
        Serial.println(line);
    }
    Serial.println(F("[Logger] End buffer dump ↑"));
}
