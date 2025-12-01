#pragma once

#include <Arduino.h>
#include <array>
#include <cstdarg>
#include <vector>

// Compile-time log mode selection (overridden via build flag APP_LOG_MODE)
#ifndef APP_LOG_MODE_DEFAULT
#define APP_LOG_MODE_DEFAULT 0
#endif
#ifndef APP_LOG_MODE_NOLOG
#define APP_LOG_MODE_NOLOG 1
#endif
#ifndef APP_LOG_MODE_ALL
#define APP_LOG_MODE_ALL 2
#endif
#ifndef APP_LOG_MODE_ERROR
#define APP_LOG_MODE_ERROR 3
#endif
#ifndef APP_LOG_MODE
#define APP_LOG_MODE APP_LOG_MODE_DEFAULT
#endif
#if !((APP_LOG_MODE == APP_LOG_MODE_DEFAULT) || (APP_LOG_MODE == APP_LOG_MODE_NOLOG) || \
      (APP_LOG_MODE == APP_LOG_MODE_ALL) || (APP_LOG_MODE == APP_LOG_MODE_ERROR))
#error "APP_LOG_MODE must be APP_LOG_MODE_DEFAULT, APP_LOG_MODE_NOLOG, APP_LOG_MODE_ALL, or APP_LOG_MODE_ERROR"
#endif

enum class AppLogLevel : uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    User
};

class Logger {
public:
    static Logger& getInstance();
    ~Logger();

    void begin(unsigned long baud_rate = 115200);
    void setLevel(AppLogLevel level);

    void log(AppLogLevel level, const char* message);
    void log(AppLogLevel level, const String& message);
    void logf(AppLogLevel level, const char* fmt, ...);

    void trace(const char* message);
    void tracef(const char* fmt, ...);

    void debug(const char* message);
    void debugf(const char* fmt, ...);

    void info(const char* message);
    void infof(const char* fmt, ...);

    void warn(const char* message);
    void warnf(const char* fmt, ...);

    void error(const char* message);
    void errorf(const char* fmt, ...);

    void user(const char* message);
    void userf(const char* fmt, ...);

    std::vector<String> getBufferedLogs() const;
    std::vector<String> getBufferedLogs(AppLogLevel min_level) const;  // Filter by level
    void dumpBufferToSerial() const;
    void clearBuffer();

private:
    Logger();

    void logv(AppLogLevel level, const char* fmt, va_list args);
    void appendToBuffer(const String& message, AppLogLevel level, uint32_t timestamp);
    String formatLine(AppLogLevel level, const char* message) const;
    String formatLineCompact(AppLogLevel level, uint32_t timestamp, const char* message) const;
    const char* levelToString(AppLogLevel level) const;
    const char* levelToShortString(AppLogLevel level) const;

    static constexpr size_t BUFFER_LINES = 128;
    static constexpr size_t MAX_SERIAL_LINE = 256;

    struct BufferEntry {
        std::array<char, MAX_SERIAL_LINE> text{};
        size_t length = 0;
        AppLogLevel level = AppLogLevel::Info;
        uint32_t timestamp = 0;
    };

    BufferEntry* buffer_;
    size_t next_index_;
    bool buffer_filled_;
    AppLogLevel min_level_ = AppLogLevel::Error;
    mutable portMUX_TYPE spinlock_;
};
