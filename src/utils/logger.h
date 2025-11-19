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

    void begin(unsigned long baud_rate = 115200);

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

private:
    Logger();

    void logv(LogLevel level, const char* fmt, va_list args);
    void appendToBuffer(const String& message);
    String formatLine(LogLevel level, const char* message) const;
    const char* levelToString(LogLevel level) const;

    static constexpr size_t BUFFER_LINES = 128;
    static constexpr size_t MAX_SERIAL_LINE = 256;

    std::array<String, BUFFER_LINES> buffer_{};
    size_t next_index_;
    bool buffer_filled_;
    mutable portMUX_TYPE spinlock_;
};
