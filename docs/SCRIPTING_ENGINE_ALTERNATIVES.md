# ESP32 Scripting Engine Alternatives

## Overview

Questo documento esplora alternative al Python bridge per fornire scripting capabilities all'ESP32 voice assistant. L'obiettivo è sostituire il pesante bridge Python con soluzioni più leggere ma flessibili.

## 1. DSL (Domain-Specific Language) Semplice

### 1.1 Sintassi Proposta

Il DSL è progettato per essere semplice ma potente per sequenze di comandi ESP32:

#### Comandi Base
```dsl
// GPIO operations
gpio_write 23 1        // Scrive HIGH su pin 23
gpio_read 25           // Legge pin 25
gpio_toggle 23         // Toggle pin 23

// Timing
delay 1000             // Delay in millisecondi
delay_us 500           // Delay in microsecondi

// LED commands
led_on 23              // Accende LED su pin 23
led_off 23             // Spegne LED su pin 23
led_toggle 23          // Toggle LED

// Audio
play_beep 1000         // Beep 1kHz per 1000ms
play_file beep.wav     // Riproduce file audio

// BLE
ble_type "Hello"       // Digita testo via BLE
ble_press enter        // Premo tasto enter

// Conditional
if gpio_read 25 == 1 then led_on 23

// Loops
repeat 3 times { led_toggle 23; delay 500 }

// Variables
var counter = 0
while counter < 5 {
    led_toggle 23
    counter = counter + 1
    delay 200
}
```

#### Strutture di Controllo Avanzate
```dsl
// If-else
if gpio_read 25 == 1 {
    led_on 23
    play_beep 500
} else {
    led_off 23
}

// For loop
for i in 1..5 {
    gpio_write i 1
    delay 100
    gpio_write i 0
}

// Functions (macro)
macro blink_led(pin, times) {
    repeat times {
        led_toggle pin
        delay 200
    }
}

blink_led(23, 5)
```

### 1.2 Implementazione Parser

```cpp
class CommandDSL {
private:
    CommandCenter& cmd_center_;
    std::map<std::string, int> variables_;

    enum TokenType {
        COMMAND, NUMBER, STRING, VARIABLE,
        LPAREN, RPAREN, LBRACE, RBRACE,
        SEMICOLON, EQUALS, OPERATOR
    };

    struct Token {
        TokenType type;
        std::string value;
        int int_value;
    };

public:
    CommandDSL(CommandCenter& cmd) : cmd_center_(cmd) {}

    CommandResult execute(const std::string& script) {
        auto tokens = tokenize(script);
        return executeTokens(tokens);
    }

private:
    std::vector<Token> tokenize(const std::string& script) {
        std::vector<Token> tokens;
        std::istringstream iss(script);
        std::string token;

        while (iss >> token) {
            if (token == ";") {
                tokens.push_back({SEMICOLON, token});
            } else if (token == "{") {
                tokens.push_back({LBRACE, token});
            } else if (token == "}") {
                tokens.push_back({RBRACE, token});
            } else if (token == "(") {
                tokens.push_back({LPAREN, token});
            } else if (token == ")") {
                tokens.push_back({RPAREN, token});
            } else if (token == "=") {
                tokens.push_back({EQUALS, token});
            } else if (isNumber(token)) {
                tokens.push_back({NUMBER, token, std::stoi(token)});
            } else if (token.substr(0, 3) == "var") {
                tokens.push_back({VARIABLE, token.substr(4)});
            } else {
                tokens.push_back({COMMAND, token});
            }
        }

        return tokens;
    }

    CommandResult executeTokens(const std::vector<Token>& tokens) {
        size_t i = 0;
        return executeSequence(tokens, i);
    }

    CommandResult executeSequence(const std::vector<Token>& tokens, size_t& i) {
        while (i < tokens.size()) {
            if (tokens[i].type == SEMICOLON) {
                i++;
                continue;
            }

            auto result = executeCommand(tokens, i);
            if (!result.success) {
                return result;
            }

            if (tokens[i].type == SEMICOLON) {
                i++;
            }
        }

        return {true, "Sequence executed"};
    }

    CommandResult executeCommand(const std::vector<Token>& tokens, size_t& i) {
        if (tokens[i].type != COMMAND) {
            return {false, "Expected command"};
        }

        std::string cmd = tokens[i].value;
        i++;

        std::vector<std::string> args;

        // Parse arguments until semicolon or brace
        while (i < tokens.size() &&
               tokens[i].type != SEMICOLON &&
               tokens[i].type != LBRACE) {

            if (tokens[i].type == NUMBER) {
                args.push_back(std::to_string(tokens[i].int_value));
            } else if (tokens[i].type == STRING) {
                args.push_back(tokens[i].value);
            } else if (tokens[i].type == VARIABLE) {
                // Handle variables
                auto it = variables_.find(tokens[i].value);
                if (it != variables_.end()) {
                    args.push_back(std::to_string(it->second));
                } else {
                    return {false, "Undefined variable: " + tokens[i].value};
                }
            }

            i++;
        }

        return cmd_center_.executeCommand(cmd, args);
    }

    bool isNumber(const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
    }
};
```

### 1.3 Integrazione con CommandCenter

```cpp
// Nel setup dell'applicazione
CommandCenter& cmd_center = CommandCenter::getInstance();

// Registra comandi GPIO
cmd_center.registerCommand("gpio_write", "Scrive valore GPIO",
    [](const std::vector<std::string>& args) -> CommandResult {
        if (args.size() < 2) {
            return {false, "Usage: gpio_write <pin> <value>"};
        }

        int pin = std::stoi(args[0]);
        bool value = args[1] == "1" || args[1] == "HIGH";

        auto* gpio = GPIOManager::getInstance()->requestGPIO(
            pin, PERIPH_GPIO_OUTPUT, "dsl"
        );

        if (!gpio) {
            return {false, "GPIO not available"};
        }

        gpio->write(value);
        GPIOManager::getInstance()->releaseGPIO(pin, "dsl");

        return {true, "GPIO written"};
    });

cmd_center.registerCommand("gpio_read", "Legge valore GPIO",
    [](const std::vector<std::string>& args) -> CommandResult {
        if (args.empty()) {
            return {false, "Usage: gpio_read <pin>"};
        }

        int pin = std::stoi(args[0]);

        auto* gpio = GPIOManager::getInstance()->requestGPIO(
            pin, PERIPH_GPIO_INPUT, "dsl"
        );

        if (!gpio) {
            return {false, "GPIO not available"};
        }

        bool value = gpio->read();
        GPIOManager::getInstance()->releaseGPIO(pin, "dsl");

        return {true, std::to_string(value)};
    });

cmd_center.registerCommand("gpio_toggle", "Toggle GPIO",
    [](const std::vector<std::string>& args) -> CommandResult {
        if (args.empty()) {
            return {false, "Usage: gpio_toggle <pin>"};
        }

        int pin = std::stoi(args[0]);

        auto* gpio = GPIOManager::getInstance()->requestGPIO(
            pin, PERIPH_GPIO_OUTPUT, "dsl"
        );

        if (!gpio) {
            return {false, "GPIO not available"};
        }

        gpio->toggle();
        GPIOManager::getInstance()->releaseGPIO(pin, "dsl");

        return {true, "GPIO toggled"};
    });

cmd_center.registerCommand("delay", "Delay in milliseconds",
    [](const std::vector<std::string>& args) -> CommandResult {
        if (args.empty()) {
            return {false, "Usage: delay <milliseconds>"};
        }

        int ms = std::stoi(args[0]);
        delay(ms);

        return {true, "Delay executed"};
    });
```

## 2. Sandbox per Codice Avanzato

### 2.1 Lua Integration (Raccomandato)

#### Setup Lua nell'ESP32
```cpp
// Aggiungi a platformio.ini
lib_deps =
    https://github.com/smittytone/esp32-lua.git

// Nel codice
#include <lua.hpp>

class LuaSandbox {
private:
    lua_State* L;

    void setupSandbox() {
        // Rimuovi funzioni pericolose
        luaL_dostring(L, R"(
            -- Disable dangerous functions
            os.execute = nil
            io.popen = nil
            os.remove = nil
            os.rename = nil
            loadfile = nil
            dofile = nil

            -- Safe GPIO API
            gpio = {
                write = function(pin, value)
                    return esp32_gpio_write(pin, value)
                end,
                read = function(pin)
                    return esp32_gpio_read(pin)
                end,
                toggle = function(pin)
                    return esp32_gpio_toggle(pin)
                end
            }

            -- Safe timing
            delay = function(ms)
                esp32_delay(ms)
            end
        )");
    }

public:
    LuaSandbox() {
        L = luaL_newstate();
        luaL_openlibs(L);
        setupSandbox();

        // Registra funzioni ESP32
        lua_register(L, "esp32_gpio_write", lua_gpio_write);
        lua_register(L, "esp32_gpio_read", lua_gpio_read);
        lua_register(L, "esp32_gpio_toggle", lua_gpio_toggle);
        lua_register(L, "esp32_delay", lua_delay);
    }

    ~LuaSandbox() {
        lua_close(L);
    }

    CommandResult execute(const std::string& script) {
        int result = luaL_dostring(L, script.c_str());

        if (result != LUA_OK) {
            const char* error = lua_tostring(L, -1);
            return {false, std::string("Lua error: ") + error};
        }

        return {true, "Lua script executed"};
    }

    // Lua C API bindings
    static int lua_gpio_write(lua_State* L) {
        int pin = luaL_checkinteger(L, 1);
        bool value = lua_toboolean(L, 2);

        auto* gpio = GPIOManager::getInstance()->requestGPIO(
            pin, PERIPH_GPIO_OUTPUT, "lua"
        );

        if (gpio) {
            gpio->write(value);
            GPIOManager::getInstance()->releaseGPIO(pin, "lua");
            lua_pushboolean(L, true);
        } else {
            lua_pushboolean(L, false);
        }

        return 1;
    }

    static int lua_gpio_read(lua_State* L) {
        int pin = luaL_checkinteger(L, 1);

        auto* gpio = GPIOManager::getInstance()->requestGPIO(
            pin, PERIPH_GPIO_INPUT, "lua"
        );

        if (gpio) {
            bool value = gpio->read();
            GPIOManager::getInstance()->releaseGPIO(pin, "lua");
            lua_pushboolean(L, value);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    static int lua_gpio_toggle(lua_State* L) {
        int pin = luaL_checkinteger(L, 1);

        auto* gpio = GPIOManager::getInstance()->requestGPIO(
            pin, PERIPH_GPIO_OUTPUT, "lua"
        );

        if (gpio) {
            gpio->toggle();
            GPIOManager::getInstance()->releaseGPIO(pin, "lua");
            lua_pushboolean(L, true);
        } else {
            lua_pushboolean(L, false);
        }

        return 1;
    }

    static int lua_delay(lua_State* L) {
        int ms = luaL_checkinteger(L, 1);
        delay(ms);
        return 0;
    }
};
```

#### Esempio Script Lua
```lua
-- Blink LED with pattern
local pin = 23
local pattern = {1, 0, 1, 0, 1}

for i, state in ipairs(pattern) do
    gpio.write(pin, state)
    delay(500)
end

-- Read button and respond
local button_pin = 25
local led_pin = 23

if gpio.read(button_pin) then
    gpio.write(led_pin, true)
    delay(1000)
    gpio.write(led_pin, false)
end
```

### 2.2 WebAssembly (WASM) Alternative

#### Setup WASM
```cpp
// Usa https://github.com/bytecodealliance/wasm-micro-runtime

#include "wasm_export.h"

class WasmSandbox {
private:
    wasm_module_t module_;
    wasm_module_inst_t instance_;

public:
    WasmSandbox() {
        // Initialize WASM runtime
        wasm_runtime_init();

        // Load WASM module (pre-compiled C/C++/Rust)
        char* buffer = readWasmFile("script.wasm");
        module_ = wasm_runtime_load(buffer, file_size, error_buf, sizeof(error_buf));
        instance_ = wasm_runtime_instantiate(module_, 8192, 8192, NULL, 0);
    }

    CommandResult execute() {
        // Call WASM main function
        wasm_function_inst_t func = wasm_runtime_lookup_function(instance_, "main");
        if (!func) {
            return {false, "WASM main function not found"};
        }

        wasm_runtime_call_wasm(func, NULL, 0);

        return {true, "WASM executed"};
    }

    // WASM host functions (GPIO access)
    static void host_gpio_write(wasm_exec_env_t exec_env, int pin, int value) {
        auto* gpio = GPIOManager::getInstance()->requestGPIO(
            pin, PERIPH_GPIO_OUTPUT, "wasm"
        );

        if (gpio) {
            gpio->write(value);
            GPIOManager::getInstance()->releaseGPIO(pin, "wasm");
        }
    }
};
```

### 2.3 Interprete Custom Leggero

Per un approccio minimalista:

```cpp
class SimpleInterpreter {
private:
    std::map<std::string, int> variables_;
    std::map<std::string, std::function<int(const std::vector<int>&)>> functions_;

public:
    SimpleInterpreter() {
        // Registra funzioni built-in
        functions_["gpio_write"] = [](const std::vector<int>& args) {
            if (args.size() < 2) return -1;

            auto* gpio = GPIOManager::getInstance()->requestGPIO(
                args[0], PERIPH_GPIO_OUTPUT, "interpreter"
            );

            if (gpio) {
                gpio->write(args[1]);
                GPIOManager::getInstance()->releaseGPIO(args[0], "interpreter");
                return 0;
            }

            return -1;
        };

        functions_["delay"] = [](const std::vector<int>& args) {
            if (args.empty()) return -1;
            delay(args[0]);
            return 0;
        };
    }

    CommandResult execute(const std::string& script) {
        // Parser molto semplice: solo chiamate funzione
        // Es: gpio_write(23, 1); delay(1000);

        // Implementazione semplificata...
        return {true, "Executed"};
    }
};
```

## 3. Architettura Integrata

### 3.1 Voice Assistant con Scripting

```cpp
class VoiceAssistant {
private:
    CommandCenter& cmd_center_;
    CommandDSL dsl_;
    LuaSandbox lua_;
    // WasmSandbox wasm_;  // Opzionale

public:
    VoiceAssistant(CommandCenter& cmd)
        : cmd_center_(cmd), dsl_(cmd) {}

    CommandResult processVoiceCommand(const std::string& text) {
        // Rilevamento tipo comando
        if (text.find("esegui script") != std::string::npos) {
            // Estrai script dal testo
            std::string script = extractScript(text);

            // Determina tipo script
            if (script.find("function") != std::string::npos ||
                script.find("local") != std::string::npos) {
                // Lua script
                return lua_.execute(script);
            } else {
                // DSL semplice
                return dsl_.execute(script);
            }
        } else {
            // Comando singolo normale
            return cmd_center_.executeCommand(text, {});
        }
    }

private:
    std::string extractScript(const std::string& text) {
        // Estrae parte script dal comando vocale
        // "esegui script led_on 23; delay 1000; led_off 23"
        // → "led_on 23; delay 1000; led_off 23"
        return text.substr(text.find("script") + 7);
    }
};
```

### 3.2 Memory Management

```cpp
class ScriptMemoryManager {
private:
    size_t dsl_heap_used_ = 0;
    size_t lua_heap_used_ = 0;
    const size_t max_heap_ = 64 * 1024;  // 64KB limit

public:
    bool canAllocate(size_t size, ScriptType type) {
        size_t current = (type == SCRIPT_DSL) ? dsl_heap_used_ : lua_heap_used_;
        return (current + size) <= max_heap_;
    }

    void allocate(size_t size, ScriptType type) {
        if (type == SCRIPT_DSL) {
            dsl_heap_used_ += size;
        } else {
            lua_heap_used_ += size;
        }
    }

    void deallocate(size_t size, ScriptType type) {
        if (type == SCRIPT_DSL) {
            dsl_heap_used_ -= size;
        } else {
            lua_heap_used_ -= size;
        }
    }
};
```

## 4. Confronto Implementazioni

| Caratteristica | DSL Semplice | Lua Sandbox | Python Bridge | WASM |
|----------------|-------------|-------------|--------------|------|
| **Memoria Base** | ~20KB | ~200KB | ~2MB | ~500KB |
| **Velocità Avvio** | Istantaneo | ~100ms | ~2s | ~500ms |
| **Sicurezza** | Limitata | Alta | Alta | Molto Alta |
| **Flessibilità** | Media | Alta | Molto Alta | Alta |
| **Complessità** | Bassa | Media | Alta | Alta |
| **Debugging** | Facile | Medio | Facile | Difficile |
| **ESP32 Ready** | ✅ | ✅ | ⚠️ | ✅ |

## 5. Implementazione Prioritaria

### Fase 1: DSL + GPIO Integration (1-2 giorni)
- Implementare parser DSL semplice
- Integrare GPIO manager nei comandi
- Test con sequenze base

### Fase 2: Lua Sandbox (2-3 giorni)
- Aggiungere supporto Lua
- Creare API bindings sicuri
- Test con script complessi

### Fase 3: Ottimizzazioni (1 giorno)
- Memory management
- Error handling
- Performance profiling

## 6. Esempi di Uso

### DSL per Sequenze Semplici
```
Utente: "Esegui script: accendi LED rosso, aspetta 1 secondo, spegni"
Sistema: led_on 23; delay 1000; led_off 23
```

### Lua per Logica Complessa
```
Utente: "Script Lua: blink pattern"
Sistema:
```lua
local pins = {23, 25, 26}
for _, pin in ipairs(pins) do
    gpio.write(pin, true)
    delay(200)
    gpio.write(pin, false)
end
```
```

### Confronto con Python Bridge
- **DSL**: Perfetto per comandi rapidi, zero overhead
- **Lua**: Bilancia potenza e leggerezza
- **Python**: Troppo pesante per ESP32, meglio su server esterno

Questa architettura fornisce scripting potente mantenendo l'ESP32 efficiente e responsive.
