// Copyright (c) 2025 rederyk
// Licensed under the MIT License. See LICENSE file for details.


#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

#ifndef OPENESPAUDIO_LOG_MODE_DEFAULT
#define OPENESPAUDIO_LOG_MODE_DEFAULT 0
#endif
#ifndef OPENESPAUDIO_LOG_MODE_NOLOG
#define OPENESPAUDIO_LOG_MODE_NOLOG 1
#endif
#ifndef OPENESPAUDIO_LOG_MODE
#define OPENESPAUDIO_LOG_MODE OPENESPAUDIO_LOG_MODE_DEFAULT
#endif
#if (OPENESPAUDIO_LOG_MODE != OPENESPAUDIO_LOG_MODE_DEFAULT) && (OPENESPAUDIO_LOG_MODE != OPENESPAUDIO_LOG_MODE_NOLOG)
#error "OPENESPAUDIO_LOG_MODE must be OPENESPAUDIO_LOG_MODE_DEFAULT or OPENESPAUDIO_LOG_MODE_NOLOG"
#endif

namespace openespaudio {

// Livelli di logging
enum class LogLevel {
    ERROR = 0,
    WARN = 1,
    INFO = 2,
    DEBUG = 3
};

// Funzione principale per loggare
void log_message(LogLevel level, const char* format, ...);

// Funzioni di convenienza
#define LOG_ERROR(format, ...) openespaudio::log_message(openespaudio::LogLevel::ERROR, "[ERROR] " format "\n", ##__VA_ARGS__)
#define LOG_WARN(format, ...)  openespaudio::log_message(openespaudio::LogLevel::WARN,  "[WARN]  " format "\n", ##__VA_ARGS__)
#define LOG_INFO(format, ...)  openespaudio::log_message(openespaudio::LogLevel::INFO,  "[INFO]  " format "\n", ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) openespaudio::log_message(openespaudio::LogLevel::DEBUG, "[DEBUG] " format "\n", ##__VA_ARGS__)

// Imposta livello minimo di logging (solo messaggi >= livello verranno mostrati)
void set_log_level(LogLevel level);
LogLevel get_log_level();

} // namespace openespaudio

#endif // LOG_H
