# Web Dashboard & API Architecture

## Obiettivi
- Servire una dashboard JS reale (SPA leggera) senza caricare il core ESP con rendering o asset pesanti.
- Esporre API HTTP sicure e versionate per status di sistema, comandi e, in seguito, file manager.
- Riutilizzare LittleFS solo per gli asset statici minificati e usare la SD per i file dell'utente.

## Stack HTTP (ESP)
- `WebServerManager` eseguito sul core di lavoro; server asincrono per evitare blocchi della UI.
- Static handler con cache forte (`ETag` + `max-age`) per asset fingerprinted (`index-xxxxx.js/css`), già gzip/brotli compressi in fase di build.
- Routing `/api/v1/*` con parser JSON a stream per non allocare grandi buffer.
- Eventi push: `EventSource`/SSE per log e cambi stato (WiFi/BLE) con code dedicate, così la dashboard resta realtime senza polling aggressivo.

## Sicurezza
- Autenticazione tramite API key salvata in Settings e inviata in `Authorization: Bearer <token>`; opzionale session cookie HttpOnly generato via `/api/v1/auth/login`.
- Anti-CSRF: token di sessione a nonce singolo per le POST (header `X-CSRF`); CORS disabilitato di default.
- Validazione path per API file (`/sd/...`), con normalizzazione e blocco traversal (`..`, doppie slash).
- Header di hardening: `Content-Security-Policy: default-src 'self'; upgrade-insecure-requests`, `X-Content-Type-Options: nosniff`, `Cache-Control` adeguato per JSON (no-store dove serve).
- Rate limit leggero per endpoint sensibili (login, upload) con contatore sliding window per IP.

## Frontend (Dashboard SPA)
- Build offline (Vite/Rollup) con framework minimale (es. Preact/Lit o vanilla + htm) per restare <200KB gzip.
- Struttura: shell `index.html` + bundle JS modulare (`dashboard`, `fs`, `logs`); CSS generato via PostCSS senza framework pesanti.
- Data layer: un client API centralizzato con retry/backoff e gestione token; websocket/SSE client per eventi.
- Layout: pannelli status (WiFi/BLE, memoria, storage), comandi rapidi, sezione log live; il file manager è un modulo separato caricato on-demand.
- UX offline-first: caching statico tramite `Cache-Control`; niente service worker obbligatorio per evitare RAM extra sul device.

## API di base (fase 1)
- `GET /api/v1/health` → `{uptime, heap, psram, wifi:{status,ip}, sd:{mounted,free}}`.
- `POST /api/v1/auth/login` → rilascia sessione; `POST /api/v1/auth/logout`.
- `GET /api/v1/commands` + `POST /api/v1/commands/execute` → ponte CommandCenter (JSON streaming).
- `GET /api/v1/logs/tail` (SSE) → ultimi N log + stream in tempo reale.

## API File Manager (fase 2, post-dashboard)
- `GET /api/v1/fs/list?path=/` → listing ricorsivo opzionale con limiti di profondità.
- `POST /api/v1/fs/upload` → upload chunked verso SD (nessuna allocazione completa in RAM).
- `POST /api/v1/fs/mkdir`, `POST /api/v1/fs/rename`, `DELETE /api/v1/fs` → operazioni atomiche, con lock sul driver SD.
- `GET /api/v1/fs/download?path=...` → stream chunked, `Content-Disposition` sicuro.

## Performance & Memory
- Upload/download a chunk di 4-8KB, zero-copy verso SD per evitare spike di heap.
- Logica HTTP su task dedicato con stack dimensionato e pool di connessioni limitato.
- Compress assets in fase di build; sul device si serve direttamente la variante compressa con `Content-Encoding: gzip` senza ricomprimere.
- Timeout di inattività per socket, keep-alive limitato per non saturare i file descriptor.

## Pipeline Build & Deploy
- Repo frontend separato o `/web` con `npm run build` → output in `data/www/` (fingerprint, gzip).
- Script `tools/sync_www.sh` (da host) per copiare bundle e rigenerare `littlefs` image; nessun tooling npm sull'ESP.
- Verifica manuale: `curl -I` per cache headers, `ab`/`wrk` leggero per check concorrenza, uso di `esp_get_free_heap_size` loggato su ogni richiesta critica.
