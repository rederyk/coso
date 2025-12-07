#pragma once

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief Sistema di scheduling basato su data/ora per ESP32-S3
 *
 * Supporta:
 * - Eventi ricorrenti (giorni della settimana + ora)
 * - Eventi one-shot (data/ora specifica)
 * - Esecuzione script Lua
 * - Persistenza su filesystem
 */

class TimeScheduler {
public:
    /** Tipo di evento */
    enum class EventType {
        OneShot,     // Eseguito una sola volta
        Recurring    // Ripetuto ogni settimana
    };

    /** Stato evento */
    enum class EventStatus {
        Pending,     // In attesa
        Running,     // In esecuzione
        Completed,   // Completato (solo one-shot)
        Failed,      // Fallito
        Disabled     // Disabilitato
    };

    /** Evento calendario */
    struct CalendarEvent {
        std::string id;              // UUID unico
        std::string name;            // Nome evento
        std::string description;     // Descrizione

        EventType type;              // One-shot o ricorrente
        bool enabled;                // Attivo/disattivo

        // Tempo
        uint8_t hour;                // 0-23
        uint8_t minute;              // 0-59
        uint8_t weekdays;            // Bitmask: bit0=Dom, bit1=Lun, ..., bit6=Sab
                                     // Es: 0b01111110 = Lun-Ven

        // One-shot specific
        uint16_t year;               // Anno (solo one-shot)
        uint8_t month;               // 1-12 (solo one-shot)
        uint8_t day;                 // 1-31 (solo one-shot)

        // Script Lua da eseguire
        std::string lua_script;

        // Metadata
        uint32_t created_at;         // Timestamp creazione
        uint32_t last_run;           // Timestamp ultima esecuzione
        uint32_t next_run;           // Timestamp prossima esecuzione
        uint32_t execution_count;    // Numero esecuzioni

        EventStatus status;
        std::string last_error;
    };

    /** Storico esecuzione */
    struct ExecutionRecord {
        std::string event_id;
        uint32_t timestamp;
        bool success;
        std::string output;
        std::string error;
        uint32_t duration_ms;
    };

    static TimeScheduler& getInstance();

    /** Lifecycle */
    bool begin();
    void end();
    bool isRunning() const;

    /** Event Management */
    std::string createEvent(const CalendarEvent& event);
    bool updateEvent(const std::string& id, const CalendarEvent& event);
    bool deleteEvent(const std::string& id);
    bool enableEvent(const std::string& id, bool enabled);

    /** Query */
    std::vector<CalendarEvent> listEvents() const;
    bool getEvent(const std::string& id, CalendarEvent& event) const;
    std::vector<ExecutionRecord> getHistory(const std::string& event_id = "", uint16_t limit = 50) const;

    /** Execution */
    bool executeEventNow(const std::string& id);

    /** Settings */
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

private:
    TimeScheduler();
    ~TimeScheduler();
    TimeScheduler(const TimeScheduler&) = delete;
    TimeScheduler& operator=(const TimeScheduler&) = delete;

    /** FreeRTOS Task */
    static void schedulerTask(void* param);
    void checkAndRunEvents();
    void executeEvent(CalendarEvent& event);

    /** Time calculations */
    void calculateNextRun(CalendarEvent& event);
    bool shouldRunNow(const CalendarEvent& event, const struct tm& now);
    uint32_t getNextWeekdayTime(uint8_t hour, uint8_t minute, uint8_t weekdays_mask);

    /** Persistence */
    void loadFromStorage();
    void saveToStorage();
    void addToHistory(const ExecutionRecord& record);

    /** Helpers */
    std::string generateEventId() const;
    bool validateEvent(const CalendarEvent& event) const;

    /** Internal state */
    bool initialized_ = false;
    bool enabled_ = true;

    std::map<std::string, CalendarEvent> events_;
    std::vector<ExecutionRecord> history_;

    SemaphoreHandle_t mutex_;
    TaskHandle_t task_handle_;

    uint16_t max_history_entries_ = 100;

    static constexpr const char* LOG_TAG = "TimeScheduler";
    static constexpr const char* STORAGE_PATH = "/calendar_events.json";
};
