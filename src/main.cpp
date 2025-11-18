#include <Arduino.h>
#include <TFT_eSPI.h>
#include "peripheral/gpio_manager.h"

namespace {
TFT_eSPI tft;
GPIOPeripheral* button_gpio = nullptr;
volatile bool button_pressed = false;
unsigned long last_button_press = 0;
const unsigned long DEBOUNCE_DELAY = 200; // ms

// Callback per interrupt del pulsante
void IRAM_ATTR onButtonPress() {
  unsigned long now = millis();
  if (now - last_button_press > DEBOUNCE_DELAY) {
    button_pressed = true;
    last_button_press = now;
  }
}

void enableBacklight() {
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
}

void drawHeader() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRoundRect(5, 5, tft.width() - 10, tft.height() - 10, 8, TFT_CYAN);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString("ESP32-S3 OS Demo", tft.width() / 2, 15, 4);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("GPIO Manager Test", tft.width() / 2, 50, 2);
}

void displayButtonStatus(bool pressed) {
  static bool last_state = false;

  if (pressed != last_state) {
    last_state = pressed;

    // Cancella area stato
    tft.fillRect(20, 90, tft.width() - 40, 120, TFT_BLACK);

    if (pressed) {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawCentreString("BUTTON PRESSED!", tft.width() / 2, 100, 4);

      // Mostra contatore presse
      static int press_count = 0;
      press_count++;
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      String msg = "Press count: " + String(press_count);
      tft.drawCentreString(msg, tft.width() / 2, 140, 2);
    } else {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawCentreString("Waiting for button", tft.width() / 2, 110, 2);
    }
  }
}

void displaySystemInfo() {
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Peripheral Manager:", 15, tft.height() - 60, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Active", 180, tft.height() - 60, 2);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Free Heap:", 15, tft.height() - 40, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String heap = String(ESP.getFreeHeap() / 1024) + " KB";
  tft.drawString(heap, 120, tft.height() - 40, 2);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("\n=== ESP32-S3 OS Demo - GPIO Manager ===");

  // Inizializza display
  enableBacklight();
  tft.init();
  tft.setRotation(1);  // Landscape

  drawHeader();
  displaySystemInfo();

  // Ottieni GPIO Manager
  GPIOManager* gpio_mgr = GPIOManager::getInstance();

  // GPIO disponibili su ESP32-S3:
  // Pin liberi comuni: GPIO 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17, 18, 21, 47, 48
  // Pin usati dal display: 10, 11, 12, 13, 45, 46
  // Usiamo GPIO 0 che è il pulsante BOOT su molti ESP32-S3
  const uint8_t BUTTON_PIN = 0;

  // Richiedi GPIO per pulsante con INPUT_PULLUP
  button_gpio = gpio_mgr->requestGPIO(BUTTON_PIN, PERIPH_GPIO_INPUT_PULLUP, "ButtonDemo");

  if (button_gpio) {
    Serial.printf("GPIO %d allocated successfully!\n", BUTTON_PIN);

    // Attacca interrupt per pulsante (FALLING = pressione)
    button_gpio->attachInterrupt(onButtonPress, FALLING);

    // Mostra stato GPIO
    gpio_mgr->printStatus();
  } else {
    Serial.printf("Failed to allocate GPIO %d\n", BUTTON_PIN);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("GPIO Init Failed!", tft.width() / 2, 100, 2);
  }

  // Messaggio iniziale
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString("Press BOOT button", tft.width() / 2, 180, 2);
  tft.drawCentreString("(GPIO 0)", tft.width() / 2, 200, 2);

  Serial.println("Setup complete. Ready!");
}

void loop() {
  // Controlla se il pulsante è stato premuto
  if (button_pressed) {
    displayButtonStatus(true);

    // Reset dopo 500ms
    delay(500);
    button_pressed = false;
    displayButtonStatus(false);

    // Aggiorna heap info
    displaySystemInfo();
  }

  delay(10);
}
