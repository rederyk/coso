# Piano per Sistema Scheduler/Cron-Job
## ESP32-S3 Voice Assistant - Task Scheduling Module

---

## 📋 PANORAMICA

Implementazione di un sistema di scheduling tasks simile a cron per l'ESP32-S3, con interfacce complete per AI e utente (Web UI + Touch Display).

### Obiettivi
- ✅ Creare task schedulati con sintassi cron-like
- ✅ Esecuzione automatica basata su timer
- ✅ Gestione completa via Web UI
- ✅ Gestione completa via Touch Display (LVGL)
- ✅ Controllo AI per creare/modificare/eliminare task
- ✅ Persistenza su filesystem (LittleFS + backup SD)
- ✅ Supporto per comandi semplici e script Lua complessi
- ✅ Visualizzazione storico esecuzioni
- ✅ Notifiche e logging eventi

---

## 🏗️ ARCHITETTURA

### Componenti Principali

```
┌─────────────────────────────────────────────────────────────┐
│                    SCHEDULER SYSTEM                          │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────────┐      ┌──────────────────────┐        │
│  │ SchedulerManager │◄─────┤  CronExpression      │        │
│  │  (Core Logic)    │      │  (Parser & Matcher)  │        │
│  └────────┬─────────┘      └──────────────────────┘        │
│           │                                                  │
│           ├──► CommandCenter (esecuzione comandi)          │
│           ├──► StorageManager (persistenza LittleFS)       │
│           ├──► SettingsManager (configurazione)            │
│           └──► EventLogger (storico esecuzioni)            │
│                                                               │
├─────────────────────────────────────────────────────────────┤
│                     INTERFACCE                               │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌───────────────┐   ┌───────────────┐   ┌──────────────┐ │
│  │   Web UI      │   │  Touch UI     │   │  AI Voice    │ │
│  │  (REST API)   │   │   (LVGL)      │   │  Assistant   │ │
│  └───────────────┘   └───────────────┘   └──────────────┘ │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 STRUTTURE DATI

### 1. ScheduledTask (struct)

```cpp
struct ScheduledTask {
    std::string id;              // UUID unico (es: "task_1732456789")
    std::string name;            // Nome leggibile (es: "Morning Routine")
    std::string description;     // Descrizione dettagliata

    // Scheduling
    std::string cronExpression;  // Espressione cron (es: "0 7 * * *")
    bool enabled;                // Task attivo/disattivo

    // Comando
    std::string command;         // Nome comando (es: "lua_script")
    std::vector<std::string> args;  // Argomenti comando

    // Metadata
    uint32_t createdAt;          // Timestamp creazione (Unix epoch)
    uint32_t lastRun;            // Timestamp ultima esecuzione
    uint32_t nextRun;            // Timestamp prossima esecuzione calcolata
    uint32_t executionCount;     // Numero esecuzioni totali

    // Configurazione avanzata
    bool runOnBoot;              // Esegui al boot del sistema
    uint8_t maxRetries;          // Tentativi massimi in caso di errore
    uint32_t timeoutMs;          // Timeout esecuzione (ms)

    // Stato runtime
    enum class Status {
        Idle,                    // In attesa
        Running,                 // In esecuzione
        Success,                 // Ultima esecuzione OK
        Failed,                  // Ultima esecuzione fallita
        Disabled                 // Disabilitato
    };
    Status status;
    std::string lastError;       // Ultimo messaggio di errore
};
```

### 2. TaskExecution (struct) - Storico Esecuzioni

```cpp
struct TaskExecution {
    std::string taskId;          // ID del task
    uint32_t timestamp;          // Timestamp esecuzione
    bool success;                // Esito esecuzione
    std::string output;          // Output comando
    std::string error;           // Messaggio errore (se fallito)
    uint32_t durationMs;         // Durata esecuzione (ms)
};
```

### 3. Formato Persistenza JSON

**File: `/scheduled_tasks.json`**

```json
{
  "version": 1,
  "tasks": [
    {
      "id": "task_1732456789",
      "name": "Morning Routine",
      "description": "Accendi LED e aumenta volume al mattino",
      "cronExpression": "0 7 * * *",
      "enabled": true,
      "command": "lua_script",
      "args": [
        "led.set_brightness(100); delay(500); audio.volume_up(); audio.volume_up();"
      ],
      "createdAt": 1732456789,
      "lastRun": 1732543200,
      "nextRun": 1732629600,
      "executionCount": 15,
      "runOnBoot": false,
      "maxRetries": 3,
      "timeoutMs": 30000,
      "status": "Success",
      "lastError": ""
    },
    {
      "id": "task_1732456800",
      "name": "BLE Mouse Test",
      "description": "Test movimento mouse BLE ogni 5 minuti",
      "cronExpression": "*/5 * * * *",
      "enabled": true,
      "command": "bt_mouse_move",
      "args": ["10", "10"],
      "createdAt": 1732456800,
      "lastRun": 1732543500,
      "nextRun": 1732543800,
      "executionCount": 288,
      "runOnBoot": false,
      "maxRetries": 1,
      "timeoutMs": 5000,
      "status": "Success",
      "lastError": ""
    }
  ],
  "history": [
    {
      "taskId": "task_1732456789",
      "timestamp": 1732543200,
      "success": true,
      "output": "LED brightness set to 100\nVolume increased to 85",
      "error": "",
      "durationMs": 1250
    }
  ],
  "settings": {
    "enabled": true,
    "checkIntervalMs": 60000,
    "maxHistoryEntries": 100,
    "logExecutions": true
  }
}
```

---

## 🔧 IMPLEMENTAZIONE DETTAGLIATA

### FASE 1: Core Scheduler Engine

#### 1.1 CronExpression Parser (`include/core/cron_expression.h`)

```cpp
class CronExpression {
public:
    // Parse espressione cron: "MIN HOUR DAY MONTH WEEKDAY"
    // Supporto: */5, 0-30, 1,3,5, *
    bool parse(const std::string& expression);

    // Calcola prossima esecuzione da un timestamp
    uint32_t getNextRun(uint32_t from_timestamp) const;

    // Verifica se timestamp corrisponde all'espressione
    bool matches(uint32_t timestamp) const;

    // Validazione espressione
    bool isValid() const;
    std::string getError() const;

private:
    struct CronField {
        std::vector<uint8_t> values;  // Valori specifici
        bool wildcard = false;         // Usa '*'
        uint8_t step = 1;              // Step per '*/N'
    };

    CronField minute_;    // 0-59
    CronField hour_;      // 0-23
    CronField day_;       // 1-31
    CronField month_;     // 1-12
    CronField weekday_;   // 0-6 (0 = Domenica)

    bool parseField(const std::string& field, CronField& out, uint8_t min, uint8_t max);
};
```

**Esempi di espressioni supportate:**
- `0 7 * * *` - Ogni giorno alle 7:00
- `*/5 * * * *` - Ogni 5 minuti
- `0 9-17 * * 1-5` - Ore 9-17, Lunedì-Venerdì
- `30 */2 * * *` - Ogni 2 ore al minuto 30
- `0 0 1 * *` - Primo giorno del mese a mezzanotte

#### 1.2 SchedulerManager (`include/core/scheduler_manager.h`)

```cpp
class SchedulerManager {
public:
    static SchedulerManager& getInstance();

    // Lifecycle
    bool begin();
    void end();
    bool isRunning() const;

    // Task Management
    std::string createTask(const ScheduledTask& task);  // Ritorna ID
    bool updateTask(const std::string& id, const ScheduledTask& task);
    bool deleteTask(const std::string& id);
    bool enableTask(const std::string& id, bool enabled);

    // Query
    std::vector<ScheduledTask> listTasks() const;
    bool getTask(const std::string& id, ScheduledTask& task) const;

    // Execution
    bool executeTaskNow(const std::string& id);  // Esecuzione manuale
    std::vector<TaskExecution> getHistory(const std::string& taskId = "", uint16_t limit = 50) const;

    // Settings
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setCheckInterval(uint32_t intervalMs);
    uint32_t getCheckInterval() const;

private:
    SchedulerManager();

    // FreeRTOS Task
    static void schedulerTask(void* param);
    void checkAndRunTasks();
    void executeTask(ScheduledTask& task);

    // Persistence
    void loadFromStorage();
    void saveToStorage();
    void addToHistory(const TaskExecution& execution);

    // Helpers
    std::string generateTaskId() const;
    void calculateNextRun(ScheduledTask& task);

    std::vector<ScheduledTask> tasks_;
    std::vector<TaskExecution> history_;
    SemaphoreHandle_t mutex_;
    TaskHandle_t task_handle_;

    bool enabled_ = true;
    uint32_t check_interval_ms_ = 60000;  // 1 minuto
    uint16_t max_history_entries_ = 100;
    bool log_executions_ = true;
};
```

**Flusso esecuzione:**

```
┌─────────────────────┐
│  schedulerTask()    │  ← FreeRTOS task, Core 0, Priority 2
│  (ogni 60s)         │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────────────────────┐
│  checkAndRunTasks()                 │
│  • Itera su tutti i task enabled   │
│  • Controlla se nextRun <= now()    │
│  • Crea task FreeRTOS temporaneo    │
└──────────┬──────────────────────────┘
           │
           ▼
┌─────────────────────────────────────┐
│  executeTask(task)                  │
│  • CommandCenter::executeCommand()  │
│  • Cattura output e errori          │
│  • Salva in history                 │
│  • Calcola nextRun                  │
│  • Notifica via UI message          │
└─────────────────────────────────────┘
```

---

### FASE 2: Web UI Interface

#### 2.1 REST API Endpoints (`WebServerManager`)

Aggiungere in `web_server_manager.cpp`:

```cpp
// Task CRUD
server_->on("/api/scheduler/tasks", HTTP_GET, [this]() { handleSchedulerList(); });
server_->on("/api/scheduler/tasks", HTTP_POST, [this]() { handleSchedulerCreate(); });
server_->on("/api/scheduler/tasks/*", HTTP_GET, [this]() { handleSchedulerGet(); });
server_->on("/api/scheduler/tasks/*", HTTP_PUT, [this]() { handleSchedulerUpdate(); });
server_->on("/api/scheduler/tasks/*", HTTP_DELETE, [this]() { handleSchedulerDelete(); });
server_->on("/api/scheduler/tasks/*/execute", HTTP_POST, [this]() { handleSchedulerExecute(); });

// History & Settings
server_->on("/api/scheduler/history", HTTP_GET, [this]() { handleSchedulerHistory(); });
server_->on("/api/scheduler/settings", HTTP_GET, [this]() { handleSchedulerSettingsGet(); });
server_->on("/api/scheduler/settings", HTTP_POST, [this]() { handleSchedulerSettingsPost(); });

// Validation
server_->on("/api/scheduler/validate-cron", HTTP_POST, [this]() { handleSchedulerValidateCron(); });
```

**Esempi di richieste:**

```bash
# Lista tutti i task
GET /api/scheduler/tasks
Response:
{
  "success": true,
  "tasks": [
    {
      "id": "task_1732456789",
      "name": "Morning Routine",
      "cronExpression": "0 7 * * *",
      "enabled": true,
      "nextRun": 1732629600,
      "status": "Idle"
    }
  ]
}

# Crea nuovo task
POST /api/scheduler/tasks
Body:
{
  "name": "Test Task",
  "description": "Muovi il mouse ogni minuto",
  "cronExpression": "* * * * *",
  "command": "bt_mouse_move",
  "args": ["5", "5"]
}
Response:
{
  "success": true,
  "id": "task_1732543210"
}

# Esegui task manualmente
POST /api/scheduler/tasks/task_1732456789/execute
Response:
{
  "success": true,
  "output": "Command executed successfully"
}

# Valida espressione cron
POST /api/scheduler/validate-cron
Body: {"expression": "0 7 * * *"}
Response:
{
  "valid": true,
  "nextRun": 1732629600,
  "description": "Every day at 07:00"
}
```

#### 2.2 Web UI Page (`data/www/scheduler.html`)

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Task Scheduler - ESP32-S3</title>
    <link rel="stylesheet" href="app-nav.css">
    <style>
        /* Stile moderno simile agli altri file */
        .task-card {
            background: #1a1a2e;
            border-radius: 12px;
            padding: 16px;
            margin-bottom: 16px;
            border-left: 4px solid #5df4ff;
        }

        .task-card.disabled {
            opacity: 0.5;
            border-left-color: #666;
        }

        .task-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .task-name {
            font-size: 1.2em;
            font-weight: bold;
            color: #5df4ff;
        }

        .task-status {
            padding: 4px 12px;
            border-radius: 16px;
            font-size: 0.85em;
        }

        .status-idle { background: #333; color: #fff; }
        .status-running { background: #ff9800; color: #000; }
        .status-success { background: #4caf50; color: #fff; }
        .status-failed { background: #f44336; color: #fff; }

        .cron-editor {
            display: grid;
            grid-template-columns: repeat(5, 1fr);
            gap: 8px;
            margin: 16px 0;
        }

        .cron-preview {
            background: #0b2035;
            padding: 12px;
            border-radius: 8px;
            margin-top: 8px;
            font-family: monospace;
        }

        .history-entry {
            background: #16213e;
            padding: 8px;
            border-radius: 8px;
            margin-bottom: 8px;
            font-size: 0.9em;
        }

        .history-entry.success { border-left: 3px solid #4caf50; }
        .history-entry.failed { border-left: 3px solid #f44336; }
    </style>
</head>
<body>
    <div id="app-nav-container"></div>

    <div class="container">
        <div class="header">
            <h1>Task Scheduler</h1>
            <button id="btn-add-task" class="btn-primary">➕ New Task</button>
        </div>

        <div class="settings-panel">
            <label>
                <input type="checkbox" id="scheduler-enabled" checked>
                Scheduler Enabled
            </label>
            <label>
                Check Interval: <input type="number" id="check-interval" value="60"> seconds
            </label>
        </div>

        <div id="tasks-container">
            <!-- Task cards dinamiche -->
        </div>

        <div class="section">
            <h2>Execution History</h2>
            <div id="history-container">
                <!-- Storico esecuzioni -->
            </div>
        </div>
    </div>

    <!-- Modal per creazione/modifica task -->
    <div id="task-modal" class="modal hidden">
        <div class="modal-content">
            <h2 id="modal-title">Create New Task</h2>

            <label>Name:</label>
            <input type="text" id="task-name" placeholder="Morning Routine">

            <label>Description:</label>
            <textarea id="task-description" rows="2" placeholder="What does this task do?"></textarea>

            <label>Cron Expression:</label>
            <div class="cron-editor">
                <input type="text" id="cron-minute" placeholder="Minute">
                <input type="text" id="cron-hour" placeholder="Hour">
                <input type="text" id="cron-day" placeholder="Day">
                <input type="text" id="cron-month" placeholder="Month">
                <input type="text" id="cron-weekday" placeholder="Weekday">
            </div>
            <div class="cron-preview" id="cron-preview">
                Next run: <span id="next-run-time">-</span>
            </div>

            <label>Command:</label>
            <select id="task-command">
                <!-- Popolato dinamicamente da /api/commands -->
            </select>

            <label>Arguments (JSON array):</label>
            <textarea id="task-args" rows="3" placeholder='["arg1", "arg2"]'>[]</textarea>

            <label>
                <input type="checkbox" id="task-run-on-boot">
                Run on boot
            </label>

            <div class="modal-actions">
                <button id="btn-save-task" class="btn-primary">Save</button>
                <button id="btn-cancel" class="btn-secondary">Cancel</button>
            </div>
        </div>
    </div>

    <script src="app-nav.js"></script>
    <script src="scheduler.js"></script>
</body>
</html>
```

**JavaScript: `scheduler.js`**

```javascript
// Funzioni principali:
// - loadTasks() - Carica lista task da API
// - loadHistory() - Carica storico esecuzioni
// - showTaskModal(taskId?) - Mostra modal crea/modifica
// - saveTask() - Salva task (POST o PUT)
// - deleteTask(id) - Elimina task
// - executeTask(id) - Esegui task manualmente
// - validateCron() - Valida espressione cron in real-time
// - updateCronPreview() - Mostra descrizione cron leggibile

// Refresh automatico ogni 30 secondi
setInterval(() => {
    loadTasks();
    loadHistory();
}, 30000);
```

---

### FASE 3: Touch Display UI (LVGL)

#### 3.1 SchedulerScreen (`src/screens/scheduler_screen.h`)

```cpp
class SchedulerScreen : public ScreenBase {
public:
    void onCreate() override;
    void onResume() override;
    void onPause() override;

private:
    void createTaskList();
    void createTaskEditor();
    void refreshTaskList();
    void showTaskDetails(const std::string& taskId);
    void showCreateTaskDialog();

    // Event handlers
    static void onTaskClicked(lv_event_t* e);
    static void onAddTaskClicked(lv_event_t* e);
    static void onDeleteTaskClicked(lv_event_t* e);
    static void onToggleTaskClicked(lv_event_t* e);
    static void onExecuteTaskClicked(lv_event_t* e);
    static void onSaveTaskClicked(lv_event_t* e);

    lv_obj_t* task_list_;
    lv_obj_t* editor_panel_;
    lv_obj_t* status_label_;

    std::vector<ScheduledTask> tasks_;
};
```

**Layout UI:**

```
┌─────────────────────────────────────────┐
│  🕐 Task Scheduler        [➕ New]      │
├─────────────────────────────────────────┤
│                                          │
│  ┌────────────────────────────────────┐ │
│  │ 🌅 Morning Routine          [✓]   │ │
│  │ 0 7 * * *                          │ │
│  │ Next: Today 07:00                  │ │
│  │ [▶️ Run] [✏️ Edit] [🗑️ Delete]    │ │
│  └────────────────────────────────────┘ │
│                                          │
│  ┌────────────────────────────────────┐ │
│  │ 🖱️ BLE Mouse Test          [✓]    │ │
│  │ */5 * * * *                        │ │
│  │ Next: 14:25                        │ │
│  │ [▶️ Run] [✏️ Edit] [🗑️ Delete]    │ │
│  └────────────────────────────────────┘ │
│                                          │
│  ┌────────────────────────────────────┐ │
│  │ 💡 LED Blink             [  ]      │ │
│  │ 0 */2 * * *                        │ │
│  │ Disabled                           │ │
│  │ [▶️ Run] [✏️ Edit] [🗑️ Delete]    │ │
│  └────────────────────────────────────┘ │
│                                          │
│  Status: 3 tasks, 2 active             │
└─────────────────────────────────────────┘
```

#### 3.2 Task Editor Dialog

```
┌─────────────────────────────────────────┐
│  Create Scheduled Task           [✕]    │
├─────────────────────────────────────────┤
│                                          │
│  Name: [Morning Routine            ]    │
│                                          │
│  Cron Expression:                       │
│  Minute: [0   ]  Hour: [7   ]          │
│  Day:    [*   ]  Month: [*   ]         │
│  Weekday: [*   ]                        │
│                                          │
│  ℹ️ Runs every day at 07:00             │
│                                          │
│  Command: [lua_script           ▼]      │
│                                          │
│  Script:                                │
│  ┌────────────────────────────────────┐ │
│  │led.set_brightness(100);            │ │
│  │delay(500);                         │ │
│  │audio.volume_up();                  │ │
│  └────────────────────────────────────┘ │
│                                          │
│  ☑️ Run on boot                         │
│                                          │
│  [💾 Save]  [❌ Cancel]                 │
└─────────────────────────────────────────┘
```

---

### FASE 4: AI Integration

#### 4.1 Nuovi Comandi AI

Registrare in `CommandCenter`:

```cpp
// Lista task schedulati
registerCommand("scheduler_list", "List all scheduled tasks", [](auto& args) {
    auto& scheduler = SchedulerManager::getInstance();
    auto tasks = scheduler.listTasks();

    std::string output = "Scheduled Tasks:\n";
    for (const auto& task : tasks) {
        output += "- " + task.name + " (" + task.cronExpression + ") ";
        output += task.enabled ? "[ENABLED]" : "[DISABLED]";
        output += "\n";
    }

    return CommandResult{true, output};
});

// Crea nuovo task
registerCommand("scheduler_create", "Create a new scheduled task", [](auto& args) {
    // args: [name, cron, command, args_json]
    if (args.size() < 3) {
        return CommandResult{false, "Usage: scheduler_create <name> <cron> <command> [args]"};
    }

    ScheduledTask task;
    task.name = args[0];
    task.cronExpression = args[1];
    task.command = args[2];

    if (args.size() >= 4) {
        // Parse JSON args
        cJSON* json = cJSON_Parse(args[3].c_str());
        if (json && cJSON_IsArray(json)) {
            for (int i = 0; i < cJSON_GetArraySize(json); i++) {
                task.args.push_back(cJSON_GetArrayItem(json, i)->valuestring);
            }
        }
        cJSON_Delete(json);
    }

    auto& scheduler = SchedulerManager::getInstance();
    std::string id = scheduler.createTask(task);

    return CommandResult{true, "Task created: " + id};
});

// Elimina task
registerCommand("scheduler_delete", "Delete a scheduled task", [](auto& args) {
    if (args.empty()) {
        return CommandResult{false, "Usage: scheduler_delete <task_id>"};
    }

    auto& scheduler = SchedulerManager::getInstance();
    bool ok = scheduler.deleteTask(args[0]);

    return CommandResult{ok, ok ? "Task deleted" : "Task not found"};
});

// Abilita/disabilita task
registerCommand("scheduler_enable", "Enable or disable a scheduled task", [](auto& args) {
    if (args.size() < 2) {
        return CommandResult{false, "Usage: scheduler_enable <task_id> <true|false>"};
    }

    bool enable = (args[1] == "true" || args[1] == "1");
    auto& scheduler = SchedulerManager::getInstance();
    bool ok = scheduler.enableTask(args[0], enable);

    return CommandResult{ok, ok ? "Task updated" : "Task not found"};
});

// Esegui task ora
registerCommand("scheduler_run", "Execute a scheduled task immediately", [](auto& args) {
    if (args.empty()) {
        return CommandResult{false, "Usage: scheduler_run <task_id>"};
    }

    auto& scheduler = SchedulerManager::getInstance();
    bool ok = scheduler.executeTaskNow(args[0]);

    return CommandResult{ok, ok ? "Task executed" : "Task not found or failed"};
});
```

#### 4.2 Aggiornamento System Prompt

Aggiungere alla sezione comandi disponibili in `voice_assistant_prompt.h`:

```
## SCHEDULED TASKS

You can manage scheduled tasks (cron-like automation):

- scheduler_list - List all scheduled tasks
- scheduler_create <name> <cron> <command> [args] - Create new task
  Examples:
  • "0 7 * * *" = Every day at 7:00 AM
  • "*/5 * * * *" = Every 5 minutes
  • "0 9-17 * * 1-5" = 9 AM to 5 PM, Monday to Friday

- scheduler_delete <task_id> - Delete a task
- scheduler_enable <task_id> <true|false> - Enable/disable task
- scheduler_run <task_id> - Execute task immediately

Example conversation:
User: "Create a task that moves the mouse every 5 minutes"
AI: {
  "command": "scheduler_create",
  "args": ["Mouse Test", "*/5 * * * *", "bt_mouse_move", "[\"10\", \"10\"]"],
  "text": "Ho creato un task che muove il mouse di 10 pixel ogni 5 minuti."
}
```

#### 4.3 Esempi Conversazione AI

```
User: "Crea un task che accende il LED ogni mattina alle 7"

AI Response:
{
  "command": "scheduler_create",
  "args": [
    "Morning LED",
    "0 7 * * *",
    "lua_script",
    "[\"led.set_brightness(100); delay(2000); led.set_brightness(50);\"]"
  ],
  "text": "Ho creato un task chiamato 'Morning LED' che accende il LED al 100% ogni mattina alle 7:00 e poi lo riporta al 50% dopo 2 secondi."
}

---

User: "Mostrami i task schedulati"

AI Response:
{
  "command": "scheduler_list",
  "args": [],
  "text": "Ecco i task schedulati che hai configurato."
}

Output:
Scheduled Tasks:
- Morning LED (0 7 * * *) [ENABLED]
- Mouse Test (*/5 * * * *) [ENABLED]
- Backup Settings (0 0 * * 0) [DISABLED]

---

User: "Disabilita il task del mouse"

AI Response:
{
  "command": "scheduler_enable",
  "args": ["task_1732456800", "false"],
  "text": "Ho disabilitato il task 'Mouse Test'."
}

---

User: "Esegui subito il task Morning LED"

AI Response:
{
  "command": "scheduler_run",
  "args": ["task_1732456789"],
  "text": "Sto eseguendo il task 'Morning LED' immediatamente."
}
```

---

### FASE 5: Integrazione Sistema

#### 5.1 Modifiche a SettingsSnapshot

Aggiungere in `settings_manager.h`:

```cpp
struct SettingsSnapshot {
    // ... campi esistenti ...

    // Scheduler
    bool schedulerEnabled = true;
    uint32_t schedulerCheckIntervalMs = 60000;  // 1 minuto
    uint16_t schedulerMaxHistoryEntries = 100;
    bool schedulerLogExecutions = true;
};
```

#### 5.2 Inizializzazione in main.cpp

```cpp
void setup() {
    // ... init esistenti ...

    // Initialize Scheduler
    auto& scheduler = SchedulerManager::getInstance();
    if (scheduler.begin()) {
        Serial.println("✓ Scheduler started");
    } else {
        Serial.println("✗ Scheduler failed to start");
    }
}
```

#### 5.3 Aggiunta Dock Icon (LVGL)

Aggiungere icona scheduler nella dock:

```cpp
// In AppManager::createDock()
DockIcon schedulerIcon{
    .screen_index = static_cast<uint8_t>(ScreenName::Scheduler),
    .icon_path = "/icons/scheduler.png",
    .symbol = LV_SYMBOL_REFRESH,  // Oppure custom icon
    .label_text = "Schedule"
};
dock_icons.push_back(schedulerIcon);
```

---

## 🔄 FLUSSI OPERATIVI

### Flusso 1: Creazione Task via Web UI

```
1. User apre /scheduler.html
2. Click su "New Task"
3. Compila form (nome, cron, comando)
4. JavaScript chiama POST /api/scheduler/tasks
5. WebServerManager → SchedulerManager::createTask()
6. Task salvato in /scheduled_tasks.json
7. Backup automatico su SD card
8. Risposta JSON con ID task
9. UI aggiorna lista task
```

### Flusso 2: Esecuzione Automatica Task

```
1. SchedulerManager::schedulerTask() si sveglia ogni 60s
2. Itera su tutti i task enabled
3. Per ogni task:
   - Controlla se task.nextRun <= millis()
   - Se sì, crea temporaneo FreeRTOS task
4. executeTask():
   - CommandCenter::executeCommand(task.command, task.args)
   - Cattura CommandResult.output
   - Calcola durata esecuzione
5. Salva TaskExecution in history
6. Aggiorna task.lastRun e task.nextRun
7. Persiste su filesystem
8. Invia UI message per aggiornare display
```

### Flusso 3: Gestione Task via Voice AI

```
User: "Crea un task che muove il mouse ogni minuto"
   ↓
1. VoiceAssistant → Whisper API → Trascrizione
2. LLM riceve system prompt con comandi scheduler
3. LLM genera:
   {
     "command": "scheduler_create",
     "args": ["Mouse Move", "* * * * *", "bt_mouse_move", "[\"5\", \"5\"]"],
     "text": "Ho creato il task per muovere il mouse ogni minuto."
   }
4. CommandCenter esegue "scheduler_create"
5. SchedulerManager crea task e ritorna ID
6. Output mostrato in Web UI e conversation buffer
7. Prossima esecuzione: calcolata da CronExpression
```

---

## 🧪 TESTING & VALIDATION

### Test Cases

1. **Cron Expression Parser**
   - `"0 7 * * *"` → 07:00 ogni giorno
   - `"*/5 * * * *"` → Ogni 5 minuti
   - `"0 9-17 * * 1-5"` → Lun-Ven 9-17
   - Invalid: `"99 * * * *"` → Error

2. **Task Execution**
   - Simple command: `bt_mouse_move 10 10`
   - Lua script: Multi-line con delay
   - Timeout: Script che eccede 30s
   - Error handling: Comando non esistente

3. **Persistence**
   - Salva 10 task → Reboot → Verifica caricamento
   - Backup su SD card → Ripristino da SD
   - History rotation: Max 100 entry

4. **Concurrency**
   - 2 task schedulati nello stesso minuto
   - Task in esecuzione durante shutdown
   - Mutex su lettura/scrittura tasks_

5. **UI Integration**
   - Web UI: CRUD completo
   - Touch UI: Lista e dettagli
   - AI: Crea task via voce

---

## 📊 METRICHE & MONITORING

### Logging Events

```cpp
enum class SchedulerEvent {
    TaskCreated,
    TaskUpdated,
    TaskDeleted,
    TaskExecuted,
    TaskFailed,
    SchedulerStarted,
    SchedulerStopped
};

// Log format
// [SCHEDULER] 2024-01-15 14:23:45 | TaskExecuted | task_1732456789 | success=true | duration=1250ms
```

### Status API

```cpp
GET /api/scheduler/status

Response:
{
  "enabled": true,
  "running": true,
  "tasksCount": 5,
  "activeTasksCount": 3,
  "nextTaskRun": 1732543800,
  "totalExecutions": 1234,
  "failedExecutions": 12,
  "uptimeMs": 3600000
}
```

---

## 🚀 ROADMAP IMPLEMENTAZIONE

### Sprint 1: Core Engine (5-7 giorni)
- [ ] CronExpression parser & tests
- [ ] SchedulerManager base
- [ ] Persistenza JSON
- [ ] Integrazione CommandCenter

### Sprint 2: Web Interface (3-4 giorni)
- [ ] REST API endpoints
- [ ] scheduler.html UI
- [ ] scheduler.js logic
- [ ] Testing CRUD operations

### Sprint 3: Touch UI (4-5 giorni)
- [ ] SchedulerScreen LVGL
- [ ] Task list widget
- [ ] Task editor dialog
- [ ] Dock integration

### Sprint 4: AI Integration (2-3 giorni)
- [ ] Nuovi comandi scheduler_*
- [ ] System prompt update
- [ ] Conversation examples
- [ ] Testing voice commands

### Sprint 5: Polish & Testing (2-3 giorni)
- [ ] Error handling robusto
- [ ] Logging completo
- [ ] Performance testing
- [ ] Documentation

**Totale stimato: 16-22 giorni di sviluppo**

---

## 🎯 DELIVERABLE FINALI

1. **Source Code**
   - `include/core/cron_expression.h/cpp`
   - `include/core/scheduler_manager.h`
   - `src/core/scheduler_manager.cpp`
   - `src/screens/scheduler_screen.h/cpp`

2. **Web Interface**
   - `data/www/scheduler.html`
   - `data/www/scheduler.js`
   - `data/www/scheduler.css`

3. **Data Files**
   - `/scheduled_tasks.json` (template)

4. **Documentation**
   - `docs/scheduler_api.md` - REST API reference
   - `docs/scheduler_cron.md` - Cron expression guide
   - `docs/scheduler_examples.md` - Use cases

5. **Tests**
   - Unit tests per CronExpression
   - Integration tests per SchedulerManager
   - E2E tests per Web UI

---

## ⚠️ CONSIDERAZIONI TECNICHE

### Memory Usage
- Ogni ScheduledTask: ~200 bytes
- 10 task = 2KB RAM
- History (100 entry): ~8KB RAM
- **Totale stimato: ~15KB RAM** (accettabile su ESP32-S3 con 8MB PSRAM)

### CPU Impact
- Scheduler check ogni 60s: ~50ms CPU time
- Task execution: Variabile (dipende dal comando)
- Esecuzione in task FreeRTOS separato: No blocking

### Storage Usage
- `/scheduled_tasks.json`: ~5KB (10 task + 100 history)
- Backup SD: ~5KB
- **Totale: ~10KB flash/SD**

### Network Impact
- Web API: +8 endpoint (~2KB code)
- JSON parsing: Usa cJSON esistente
- HTTP timeout: 45s (già configurato)

---

## 🎉 CONCLUSIONE

Questo sistema fornirà:

✅ **Automazione completa** - Esegui task a intervalli regolari senza intervento umano
✅ **Gestione flessibile** - 3 interfacce (Web, Touch, Voice AI)
✅ **Persistenza robusta** - Dati salvati su LittleFS + backup SD
✅ **Integrazione nativa** - Usa CommandCenter e LuaSandbox esistenti
✅ **Scalabilità** - Architettura modulare, facile da estendere
✅ **Monitoring** - Storico esecuzioni e logging dettagliato

**Il sistema sarà production-ready e completamente integrato con l'architettura esistente dell'ESP32-S3 Voice Assistant.**

---

**Domande per l'utente:**

1. ✅ Approvazione del piano generale?
2. ⚙️ Priorità: Quale interfaccia sviluppare per prima? (Web UI, Touch UI, o AI?)
3. 🕐 Cron syntax: Supportare anche alias come "@daily", "@hourly"?
4. 📊 History retention: 100 entry è sufficiente?
5. 🔔 Notifiche: Aggiungere notifiche push o email per task falliti?
6. 🔒 Sicurezza: Limitare quali comandi possono essere schedulati?

**Ready to implement! 🚀**
