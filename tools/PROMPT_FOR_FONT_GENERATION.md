# Prompt per Generazione Font Emoji

Copia e incolla questo prompt in una nuova chat per generare i 5 font emoji necessari.

---

## PROMPT DA COPIARE

```
Ho bisogno di generare 5 font custom per LVGL 8.4.0 con supporto emoji per un progetto ESP32-S3.

**Specifiche tecniche:**
- Display: ILI9341 240x320px
- LVGL: versione 8.4.0
- Framework: Arduino + PlatformIO
- Font base: Montserrat
- Font emoji: Noto Emoji monocromatico

**Font sizes richiesti:**
1. emoji_14 (14px) - UI text, logs
2. emoji_20 (20px) - Card titles
3. emoji_24 (24px) - Headers, dock icons
4. emoji_48 (48px) - Large numbers
5. emoji_96 (96px) - Hero emoji

**Emoji da includere (subset ottimizzato - 100 emoji):**

Attualmente usate (17):
🏠 ⚙️ 🛰️ ℹ️ ❌ ⚠️ 🎨 📶 💡 🖥️ ⚡ 📊 💾 🔧 🔄 👆 🖌️

Facce ed emozioni:
😀 😁 😂 😊 😍 🤔 😎 😢 😡 🤯 😴 🤗

Gesti:
👍 👎 👌 ✌️ 👏 🙏 👋 🤝

Tech:
📱 💻 ⌨️ 🖱️ 🔌 🔋 📡 🛜 💿 📀 🖨️ 📷

Simboli UI:
✅ ✔️ ⭐ 💯 🔥 💧 🌙 ☀️ 🔒 🔓 🔑 🚀

Frecce:
⬆️ ⬇️ ⬅️ ➡️ ↗️ ↘️ ↙️ ↖️ 🔼 🔽 ◀️ ▶️

Status:
✨ 💫 🔴 🟢 🟡 🔵 ⚫ ⚪ 🟠 🟣 🟤

Tempo:
⏰ ⏱️ ⏲️ 🕐 🌡️ 🌤️ ⛅ 🌧️ ⛈️ 🌩️ ❄️ 💨

Ufficio:
📁 📂 📄 📃 📋 📌 📍 🗂️

**Range Unicode completo da includere:**
0x20-0x7E,0x2139,0x2197-0x2199,0x23F0-0x23F3,0x2328,0x2600-0x2604,0x2614-0x2615,0x261D,0x2638-0x263A,0x2640,0x2642,0x2665-0x2666,0x2692-0x2697,0x2699,0x26A0-0x26A1,0x26AA-0x26AB,0x26C4-0x26C5,0x26C8,0x26FD,0x2702,0x2705,0x270C,0x2714,0x2716,0x2728,0x2744,0x274C,0x2753-0x2755,0x2757,0x2763-0x2764,0x2795-0x2797,0x27A1,0x2B05-0x2B07,0x2B1B-0x2B1C,0x2B50,0x2B55,0x1F300-0x1F321,0x1F324-0x1F393,0x1F3A8,0x1F3E0,0x1F3F3-0x1F3F5,0x1F400-0x1F4FD,0x1F4FF-0x1F53D,0x1F54E,0x1F550-0x1F567,0x1F573-0x1F57A,0x1F587,0x1F58A-0x1F58D,0x1F590,0x1F595-0x1F596,0x1F5A4-0x1F5A5,0x1F5A8,0x1F5B1-0x1F5B2,0x1F5BC,0x1F5C2-0x1F5C4,0x1F5D1-0x1F5D3,0x1F5E1,0x1F5E3,0x1F5E8,0x1F5EF,0x1F5F3,0x1F5FA-0x1F64F,0x1F680-0x1F6C5,0x1F6CB-0x1F6D2,0x1F6DC,0x1F6E9,0x1F6EB-0x1F6EC,0x1F6F0,0x1F6F3-0x1F6FC,0x1F7E0-0x1F7EB,0x1F90C-0x1F93A,0x1F93C-0x1F945,0x1F947-0x1F9FF

**Requisiti per la generazione:**
- Usa LVGL Online Font Converter: https://lvgl.io/tools/fontconverter
- Font sorgente: Noto Emoji Regular (https://github.com/googlefonts/noto-emoji/raw/main/fonts/NotoEmoji-Regular.ttf)
- BPP: 4 (4 bits per pixel)
- Formato: LVGL (C array)
- Include ASCII printable (0x20-0x7E) + emoji ranges sopra

**Output richiesto:**
Genera i 5 file .c in formato LVGL 8.4.0:
- emoji_14.c
- emoji_20.c
- emoji_24.c
- emoji_48.c
- emoji_96.c

**Configurazione per ogni font:**
Nome: emoji_14 (sostituire con size appropriata)
Size: 14 (sostituire con size appropriata)
Bpp: 4
Range: (usa il range Unicode completo sopra)
Font fallback: opzionale lv_font_montserrat_{size}

Puoi guidarmi passo-passo nella generazione di questi font usando l'LVGL Online Font Converter?
Oppure, se hai accesso a lv_font_conv CLI, puoi generare i comandi necessari?

**Obiettivo finale:**
Ottenere 5 file .c pronti da integrare nel progetto PlatformIO ESP32-S3.

Grazie!
```

---

## Istruzioni

1. **Apri una nuova chat** (Claude, ChatGPT, o altro AI assistant)
2. **Copia il prompt** dalla sezione sopra
3. **Incolla** nella nuova chat
4. **Segui le istruzioni** per generare i font
5. **Scarica i 5 file .c** generati
6. **Copia i file** in `src/ui/fonts/` di questo progetto
7. **Torna qui** e io integrerò i font nel progetto

## Alternative Veloci

Se preferisci usare il web tool direttamente:

1. Vai su: https://lvgl.io/tools/fontconverter
2. Scarica Noto Emoji: https://github.com/googlefonts/noto-emoji/raw/main/fonts/NotoEmoji-Regular.ttf
3. Per ogni size (14, 20, 24, 48, 96):
   - Name: `emoji_{size}`
   - Size: `{size}`
   - Bpp: `4`
   - Upload TTF: NotoEmoji-Regular.ttf
   - Range: Copia il range Unicode dal prompt sopra
   - Click "Convert"
   - Download il file .c
   - Rinomina in `emoji_{size}.c`

Dopo aver generato tutti i 5 font, dimmi e li integro nel progetto!
