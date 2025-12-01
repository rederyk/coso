// Copyright (c) 2025 rederyk
// Licensed under the MIT License. See LICENSE file for details.


#include "logger.h"
#include <cstdarg>
#include <cstdio>

namespace openespaudio {

// Livello di logging corrente
static LogLevel current_level = LogLevel::ERROR;

void log_message(LogLevel level, const char* format, ...) {
#if OPENESPAUDIO_LOG_MODE == OPENESPAUDIO_LOG_MODE_NOLOG
    (void)level;
    (void)format;
    return;
#else
    if (level > current_level) {
        return;  // Non loggare messaggi sotto il livello corrente
    }

    va_list args;
    va_start(args, format);
    char buffer[256];  // Buffer per la stringa formattata
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.print(buffer);  // Output senza newline, poiché includiamo \n nel format
#endif
}

void set_log_level(LogLevel level) {
#if OPENESPAUDIO_LOG_MODE == OPENESPAUDIO_LOG_MODE_NOLOG
    (void)level;
#else
    current_level = level;
#endif
}

LogLevel get_log_level() {
#if OPENESPAUDIO_LOG_MODE == OPENESPAUDIO_LOG_MODE_NOLOG
    return LogLevel::ERROR;
#else
    return current_level;
#endif
}

} // namespace openespaudio
