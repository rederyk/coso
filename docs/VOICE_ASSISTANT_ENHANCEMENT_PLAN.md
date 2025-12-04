# Voice Assistant Enhancement Plan

Ottima domanda! Ci sono molti modi interessanti per migliorare l'esperienza utente. Analizziamo le tue proposte e aggiungiamo altre idee:

## 1. RAG (Retrieval-Augmented Generation) 📚

Un sistema RAG potrebbe essere molto utile per:

### Casi d'uso:
- **Memoria persistente personalizzata**: "Ricorda che il mio gatto si chiama Fluffy e ha strisce blu"
- **Documentazione del dispositivo**: Recuperare info su comandi, GPIO pins, configurazioni
- **Cronologia conversazioni avanzata**: Cercare in vecchie conversazioni per contesto
- **Manuale utente dinamico**: "Come collego il GPIO 23?" → recupera info dalla documentazione

### Implementazione suggerita:
📁 **Architettura RAG proposta**:
```
├── ESP32 (Client)
│   └── Invia query + metadata al server RAG
├── Python Server (Raspberry Pi/PC)
│   ├── Vector DB (ChromaDB/Faiss)
│   ├── Embeddings (sentence-transformers)
│   └── Context injection nel prompt LLM
└── LLM (con contesto RAG)
```

## 1.5 Alternative a Ollama con RAG Integrato 🆕

Poiché RAG su ESP32 è pesante, considera queste alternative che hanno RAG già integrato:

### A. **OpenWebUI** (Raccomandato)
- ✅ RAG nativo (9 database vettoriali: ChromaDB, Qdrant, etc.)
- ✅ API REST completa per connessione programmatica
- ✅ Gestione chat con ID persistenti
- ✅ Upload documenti automatico

**Connessione ESP32 via API:**
```python
# Su Raspberry Pi (bridge per ESP32)
import requests

def create_chat_session(title="ESP32 Assistant"):
    """Crea nuova chat e restituisce ID"""
    response = requests.post("http://localhost:8080/api/v1/chats/",
                           json={"title": title})
    return response.json()["id"]

def send_voice_command(message, chat_id):
    """Invia comando vocale a OpenWebUI"""
    response = requests.post(f"http://localhost:8080/api/v1/chats/{chat_id}/messages",
                           json={"content": message})
    return response.json()["choices"][0]["message"]["content"]

def upload_document(file_path, chat_id):
    """Upload documento per RAG"""
    with open(file_path, 'rb') as f:
        files = {'file': f}
        requests.post(f"http://localhost:8080/api/v1/chats/{chat_id}/documents",
                     files=files)

# Esempio uso
chat_id = create_chat_session("ESP32 Commands")
upload_document("gpio_manual.pdf", chat_id)
response = send_voice_command("Come configuro GPIO 23?", chat_id)
```

**Installazione OpenWebUI con RAG:**
```bash
# Con Docker (include Ollama)
docker run -d -p 3000:8080 --gpus=all \
  -v ollama:/root/.ollama \
  -v open-webui:/app/backend/data \
  ghcr.io/open-webui/open-webui:ollama
```

### B. **PrivateGPT**
```bash
pip install privategpt
privategpt ingest docs/  # Carica documenti
privategpt ui            # Interfaccia web con RAG
```
- ✅ RAG completo integrato
- ✅ Nessun setup complesso
- ✅ Supporto PDF, TXT, DOCX

### C. **LocalGPT**
```bash
git clone https://github.com/PromtEngineer/localGPT
cd localGPT
pip install -r requirements.txt
python ingest.py     # Processa documenti
python run_localGPT.py  # Chat con RAG integrato
```
- ✅ RAG completamente locale
- ✅ Nessuna dipendenza cloud
- ✅ Supporto multi-documento

### D. **Khoj AI**
```bash
pip install khoj-assistant
khoj --search
```
- ✅ RAG + ricerca semantica avanzata
- ✅ Integrazione con note (Obsidian), email, etc.

### E. **Quivr**
```bash
# Con Docker
docker run -p 3000:3000 quivrhq/quivr
```
- ✅ RAG enterprise-grade
- ✅ Gestione team e permessi
- ✅ API REST documentata

**Confronto Alternative:**

| Sistema | RAG Integrato | API | Complessità Setup | Risorse Richieste |
|---------|---------------|-----|-------------------|-------------------|
| OpenWebUI | ✅ 9 DB vettoriali | ✅ Completa | Media | Media |
| PrivateGPT | ✅ ChromaDB | ⚠️ Limitata | Bassa | Media |
| LocalGPT | ✅ FAISS/Chroma | ❌ Nessuna | Media | Alta |
| Khoj | ✅ Custom | ✅ REST | Bassa | Bassa |
| Quivr | ✅ Multi-DB | ✅ Completa | Alta | Alta |

## 2. Environment Python per Comandi Complessi 🐍

Questa è un'idea eccellente! Potresti creare un bridge Python che:

### Architettura proposta:
```python
# Server Python che riceve comandi dall'ESP32
class ESP32CommandBridge:
    def execute_script(self, script: str, params: dict):
        """Esegue script Python per comandi complessi"""
        # Sandbox sicuro per eseguire codice

    def chain_commands(self, commands: list):
        """Concatena più comandi ESP32"""
        # es: "accendi LED, aspetta 5 sec, spegni LED, invia notifica"

    def control_gpio_remote(self, pin: int, state: bool):
        """Controlla GPIO da remoto via API"""
```

### Esempi di comandi concatenati:
```json
{
  "command": "python_script",
  "script": "led_sequence",
  "params": {
    "pattern": [1, 0, 1, 0, 1],
    "delay_ms": 500,
    "gpio": 23
  }
}
```

## 3. Altre idee per migliorare l'UX 💡

### A. Sistema di Plugin/Extensions
```cpp
// Plugin system per estendere comandi
class VoiceCommandPlugin {
    virtual bool canHandle(const std::string& command) = 0;
    virtual CommandResult execute(const VoiceCommand& cmd) = 0;
};

// Esempio plugin
class HomeAutomationPlugin : public VoiceCommandPlugin {
    // Gestisce: "accendi tutte le luci", "imposta temperatura a 21°"
};
```

### B. Macro e Routine
Salvare sequenze di comandi come routine nominate:
```json
{
  "routine": "morning",
  "commands": [
    {"command": "ble_keyboard_type", "text": "Good morning"},
    {"command": "led_pattern", "pattern": "sunrise"},
    {"command": "volume_set", "level": 50}
  ]
}
```

### C. Context-Aware Commands
Il sistema "ricorda" il contesto della conversazione:
- User: "Accendi il LED rosso"
- Assistant: "Fatto! LED rosso acceso su GPIO 23"
- User: "E ora quello blu"  // Il sistema sa che stiamo parlando di LED
- Assistant: "LED blu acceso su GPIO 25"
- User: "Spegnili entrambi"  // Tiene traccia dei LED accesi

### D. Voice Feedback via TTS
Aggiungere Text-To-Speech per risposte vocali:
```cpp
// Invece di solo visualizzare la risposta
void speakResponse(const std::string& text) {
    // Invia a Google TTS / Azure TTS / Piper TTS locale
    // Riproduci audio via I2S
}
```

### E. GPIO Controller Web UI
Una dashboard web per controllare GPIO direttamente:
```html
<!-- UI interattiva per GPIO -->
<div class="gpio-pin" data-pin="23">
  <span>GPIO 23</span>
  <button onclick="togglePin(23)">Toggle</button>
  <input type="range" onchange="setPWM(23, value)">
</div>
```

### F. Condizioni e Trigger
Sistema di automazione tipo IFTTT:
```json
{
  "trigger": "time",
  "condition": "20:00",
  "action": {"command": "led_off", "args": ["all"]}
}

{
  "trigger": "voice_command",
  "condition": "contains:emergency",
  "action": {"command": "ble_type", "text": "911"}
}
```

### G. Multi-Language Support
Supporto per più lingue con rilevamento automatico:
```cpp
// Rileva lingua dalla trascrizione
std::string detectLanguage(const std::string& text);

// Prompt dinamico in base alla lingua
std::string getPromptForLanguage(const std::string& lang);
```

### H. Visual Feedback su Display
Se hai il display Freenove:
```cpp
// Mostra stato conversazione sul display
void updateConversationDisplay() {
    // Mostra trascrizione in tempo reale
    // Animazione "thinking" durante elaborazione LLM
    // Visualizza comando eseguito
}
```

## 4. Implementazione Prioritaria Suggerita 🎯

Ti consiglio di iniziare con questi in ordine:

### Fase 1: Command Chaining (più semplice)
```json
// Aggiungi supporto per comandi multipli in una risposta
{
  "command": "chain",
  "commands": [
    {"name": "led_on", "args": ["23"]},
    {"name": "delay", "args": ["1000"]},
    {"name": "led_off", "args": ["23"]}
  ]
}
```

### Fase 2: Python Bridge Server
```python
# server.py
from fastapi import FastAPI
import asyncio

app = FastAPI()

@app.post("/execute_sequence")
async def execute_sequence(commands: list):
    """Esegue sequenza di comandi ESP32"""
    results = []
    for cmd in commands:
        result = await send_to_esp32(cmd)
        results.append(result)
    return {"results": results}

@app.post("/gpio_control")
async def gpio_control(pin: int, value: int):
    """Controlla GPIO via HTTP→ESP32 API"""
    return await send_to_esp32({
        "command": "gpio_set",
        "args": [str(pin), str(value)]
    })
```

### Fase 3: RAG System (più complesso)
```python
# rag_server.py
from chromadb import Client
from sentence_transformers import SentenceTransformer

class ESP32RAG:
    def __init__(self):
        self.client = Client()
        self.collection = self.client.create_collection("esp32_knowledge")
        self.encoder = SentenceTransformer('all-MiniLM-L6-v2')

    def add_memory(self, text: str, metadata: dict):
        """Salva memoria con embedding"""
        embedding = self.encoder.encode(text)
        self.collection.add(
            embeddings=[embedding],
            documents=[text],
            metadatas=[metadata]
        )

    def retrieve_context(self, query: str, k=3):
        """Recupera top-k memorie rilevanti"""
        query_embedding = self.encoder.encode(query)
        results = self.collection.query(
            query_embeddings=[query_embedding],
            n_results=k
        )
        return results
```

## 5. Architettura Completa Proposta 🏗️

```
┌─────────────────────────────────────────────────┐
│                   ESP32-S3                      │
│  ┌──────────────┐         ┌─────────────────┐  │
│  │ Voice Input  │────────▶│ Whisper STT     │  │
│  └──────────────┘         └─────────────────┘  │
│                                    │            │
│                                    ▼            │
│  ┌─────────────────────────────────────────┐   │
│  │   LLM (Ollama) + RAG Context Injection  │   │
│  └─────────────────────────────────────────┘   │
│                    │                            │
│                    ▼                            │
│  ┌─────────────────────────────────────────┐   │
│  │  Command Router                         │   │
│  │  ├─ Simple commands → Execute locally   │   │
│  │  ├─ Complex → Python Server             │   │
│  │  └─ GPIO → Direct / Python Bridge       │   │
│  └─────────────────────────────────────────┘   │
└────────────────────┬────────────────────────────┘
                     │
                     ▼
        ┌────────────────────────────┐
        │   Python Server (Bridge)   │
        │  ┌──────────────────────┐  │
        │  │ RAG System           │  │
        │  │ - ChromaDB           │  │
        │  │ - Embeddings         │  │
        │  └──────────────────────┘  │
        │  ┌──────────────────────┐  │
        │  │ Command Executor     │  │
        │  │ - Chains             │  │
        │  │ - Scripts            │  │
        │  │ - GPIO Control       │  │
        │  └──────────────────────┘  │
        │  ┌──────────────────────┐  │
        │  │ Memory Manager       │  │
        │  │ - Persistent storage │  │
        │  │ - Context tracking   │  │
        │  └──────────────────────┘  │
        └────────────────────────────┘
```

Vuoi che implementiamo una di queste funzionalità?

Ti suggerisco di iniziare con il **Command Chaining** perché:
- ✅ Non richiede server esterni
- ✅ Migliora immediatamente l'UX
- ✅ È la base per feature più complesse

Oppure possiamo partire con il **Python Bridge Server** se vuoi subito la flessibilità di script complessi. Quale ti interessa di più?
