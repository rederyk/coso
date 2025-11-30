#pragma once

#include <Arduino.h>
#include <array>
#include <cstdarg>
#include <vector>

enum class LogLevel : uint8_t {
    Debug = 0,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static Logger& getInstance();
    ~Logger();

    void begin(unsigned long baud_rate = 115200);
    void setLevel(LogLevel level);

    void log(LogLevel level, const char* message);
    void log(LogLevel level, const String& message);
    void logf(LogLevel level, const char* fmt, ...);

    void debug(const char* message);
    void debugf(const char* fmt, ...);

    void info(const char* message);
    void infof(const char* fmt, ...);

    void warn(const char* message);
    void warnf(const char* fmt, ...);

    void error(const char* message);
    void errorf(const char* fmt, ...);

    std::vector<String> getBufferedLogs() const;
    std::vector<String> getBufferedLogs(LogLevel min_level) const;  // Filter by level
    void dumpBufferToSerial() const;
    void clearBuffer();

private:
    Logger();

    void logv(LogLevel level, const char* fmt, va_list args);
    void appendToBuffer(const String& message, LogLevel level, uint32_t timestamp);
    String formatLine(LogLevel level, const char* message) const;
    String formatLineCompact(LogLevel level, uint32_t timestamp, const char* message) const;
    const char* levelToString(LogLevel level) const;
    const char* levelToShortString(LogLevel level) const;

    static constexpr size_t BUFFER_LINES = 128;
    static constexpr size_t MAX_SERIAL_LINE = 256;

    struct BufferEntry {
        std::array<char, MAX_SERIAL_LINE> text{};
        size_t length = 0;
        LogLevel level = LogLevel::Info;
        uint32_t timestamp = 0;
    };

    BufferEntry* buffer_;
    size_t next_index_;
    bool buffer_filled_;
    LogLevel min_level_ = LogLevel::Debug;
    mutable portMUX_TYPE spinlock_;
};
