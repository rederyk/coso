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
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    buffer_ = nullptr;
    min_level_ = AppLogLevel::Error; // irrelevant in nolog
#else
    buffer_ = static_cast<BufferEntry*>(
        heap_caps_calloc(BUFFER_LINES, sizeof(BufferEntry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer_) {
        buffer_ = static_cast<BufferEntry*>(
            heap_caps_calloc(BUFFER_LINES, sizeof(BufferEntry), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
#if APP_LOG_MODE == APP_LOG_MODE_ALL
    min_level_ = AppLogLevel::Trace;
#elif APP_LOG_MODE == APP_LOG_MODE_ERROR
    min_level_ = AppLogLevel::Error;
#elif APP_LOG_MODE == APP_LOG_MODE_DEFAULT
    min_level_ = AppLogLevel::Info;
#else
    min_level_ = AppLogLevel::Error; // fallback
#endif
#endif
}

Logger::~Logger() {
    if (buffer_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }
}

void Logger::begin(unsigned long baud_rate) {
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    (void)baud_rate;
#else
    Serial.begin(baud_rate);
#endif
}

void Logger::setLevel(AppLogLevel level) {
    min_level_ = level;
}

void Logger::log(AppLogLevel level, const char* message) {
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    (void)level;
    (void)message;
    return;
#endif
    if (static_cast<int>(level) < static_cast<int>(min_level_)) {
        return;
    }
    if (!message) {
        return;
    }
    const uint32_t timestamp = millis();
    const String formatted = formatLine(level, message);
    Serial.println(formatted);
    appendToBuffer(message, level, timestamp);
}

void Logger::log(AppLogLevel level, const String& message) {
    log(level, message.c_str());
}

void Logger::logf(AppLogLevel level, const char* fmt, ...) {
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    (void)level;
    (void)fmt;
    return;
#endif
    if (!fmt) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    logv(level, fmt, args);
    va_end(args);
}

void Logger::debug(const char* message) {
    log(AppLogLevel::Debug, message);
}

void Logger::debugf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(AppLogLevel::Debug, fmt, args);
    va_end(args);
}

void Logger::trace(const char* message) {
    log(AppLogLevel::Trace, message);
}

void Logger::tracef(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(AppLogLevel::Trace, fmt, args);
    va_end(args);
}

void Logger::info(const char* message) {
    log(AppLogLevel::Info, message);
}

void Logger::infof(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(AppLogLevel::Info, fmt, args);
    va_end(args);
}

void Logger::warn(const char* message) {
    log(AppLogLevel::Warn, message);
}

void Logger::warnf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(AppLogLevel::Warn, fmt, args);
    va_end(args);
}

void Logger::error(const char* message) {
    log(AppLogLevel::Error, message);
}

void Logger::errorf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(AppLogLevel::Error, fmt, args);
    va_end(args);
}

void Logger::user(const char* message) {
    log(AppLogLevel::User, message);
}

void Logger::userf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(AppLogLevel::User, fmt, args);
    va_end(args);
}

void Logger::logv(AppLogLevel level, const char* fmt, va_list args) {
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    (void)level;
    (void)fmt;
    (void)args;
    return;
#endif
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

void Logger::appendToBuffer(const String& message, AppLogLevel level, uint32_t timestamp) {
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    (void)message;
    (void)level;
    (void)timestamp;
    return;
#endif
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

String Logger::formatLine(AppLogLevel level, const char* message) const {
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

const char* Logger::levelToString(AppLogLevel level) const {
    switch (level) {
        case AppLogLevel::Trace: return "TRACE";
        case AppLogLevel::Debug: return "DEBUG";
        case AppLogLevel::Info:  return "INFO";
        case AppLogLevel::Warn:  return "WARN";
        case AppLogLevel::Error: return "ERROR";
        case AppLogLevel::User:  return "USER";
        default:               return "LOG";
    }
}

const char* Logger::levelToShortString(AppLogLevel level) const {
    switch (level) {
        case AppLogLevel::Trace: return "T";
        case AppLogLevel::Debug: return "D";
        case AppLogLevel::Info:  return "I";
        case AppLogLevel::Warn:  return "W";
        case AppLogLevel::Error: return "E";
        case AppLogLevel::User:  return "U";
        default:               return "?";
    }
}

String Logger::formatLineCompact(AppLogLevel level, uint32_t timestamp, const char* message) const {
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
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    return {};
#endif
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

std::vector<String> Logger::getBufferedLogs(AppLogLevel min_level) const {
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    (void)min_level;
    return {};
#endif
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
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    return;
#endif
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
#if APP_LOG_MODE == APP_LOG_MODE_NOLOG
    return;
#endif
    const auto snapshot = getBufferedLogs();
    Serial.println(F("[Logger] Dump buffered logs ↓"));
    for (const auto& line : snapshot) {
        Serial.println(line);
    }
    Serial.println(F("[Logger] End buffer dump ↑"));
}
