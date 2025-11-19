#include "utils/logger.h"

#include <algorithm>
#include <cstdio>
#include <vector>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : next_index_(0),
      buffer_filled_(false),
      spinlock_(portMUX_INITIALIZER_UNLOCKED) {}

void Logger::begin(unsigned long baud_rate) {
    Serial.begin(baud_rate);
}

void Logger::log(LogLevel level, const char* message) {
    if (!message) {
        return;
    }
    const String formatted = formatLine(level, message);
    Serial.println(formatted);
    appendToBuffer(formatted);
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

void Logger::appendToBuffer(const String& message) {
    portENTER_CRITICAL(&spinlock_);
    buffer_[next_index_] = message;
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

std::vector<String> Logger::getBufferedLogs() const {
    std::vector<String> logs;
    logs.reserve(buffer_filled_ ? BUFFER_LINES : next_index_);

    portENTER_CRITICAL(&spinlock_);
    const size_t count = buffer_filled_ ? BUFFER_LINES : next_index_;
    for (size_t i = 0; i < count; ++i) {
        size_t index = buffer_filled_ ? (next_index_ + i) % BUFFER_LINES : i;
        logs.push_back(buffer_[index]);
    }
    portEXIT_CRITICAL(&spinlock_);
    return logs;
}
