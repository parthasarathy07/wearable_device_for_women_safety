// LilyPad_Client.ino
#include <WiFi.h>
#include "MAX30105.h"

MAX30105 particleSensor;

const char* ssid       = "BharatPi_AP";
const char* password   = "12345678";
const char* serverIP   = "192.168.4.1";
const uint16_t serverPort = 3333;

WiFiClient client;

#define BUTTON_PIN   5
#define DEBOUNCE_MS  200

// MAX30102: SDA=21, SCL=22 (default Wire pins on ESP32)

unsigned long lastButtonTime = 0;
unsigned long lastPulseAlert = 0;
#define PULSE_ALERT_COOLDOWN_MS 20000

// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  connectWifi();
  setupPins();
  Serial.println("Monitoring started: button + pulse");
}

// ─────────────────────────────────────────────
void loop() {
  // Re-connect WiFi if it dropped
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost – reconnecting...");
    connectWifi();
  }

  handle_button();
  handle_pulse();
}

// ─────────────────────────────────────────────
/* ---------- WiFi ---------- */
void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to Bharat Pi AP...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect (timeout). Will retry later.");
  }
}

// ─────────────────────────────────────────────
/* ---------- Pins / Sensors ---------- */
void setupPins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // LOW when pressed

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found – check wiring!");
    while (1) { delay(1000); }
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);
  Serial.println("MAX30102 initialised.");
}

// ─────────────────────────────────────────────
/* ---------- Sensor Handlers ---------- */
void handle_button() {
  if (digitalRead(BUTTON_PIN) == LOW &&
      millis() - lastButtonTime > DEBOUNCE_MS) {
    lastButtonTime = millis();
    Serial.println("ALERT: Button pressed");
    sendData("1");
  }
}

void handle_pulse() {
  long irValue = particleSensor.getIR();

  if (irValue < 50000 &&
      millis() - lastPulseAlert > PULSE_ALERT_COOLDOWN_MS) {
    lastPulseAlert = millis();
    Serial.print("ALERT: Abnormal pulse (IR=");
    Serial.print(irValue);
    Serial.println(")");
    sendData("2");
  }
}

// ─────────────────────────────────────────────
/* ---------- Networking ---------- */
void sendData(const char* data) {
  // Ensure WiFi is up
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Not connected to AP – cannot send.");
      return;
    }
  }

  // Always open a fresh connection, send, then close cleanly
  if (client.connected()) {
    client.stop();
  }

  if (client.connect(serverIP, serverPort)) {
    Serial.print("Sending to Bharat Pi: ");
    Serial.println(data);
    client.print(data);
    client.print("\n");
    client.flush();
    delay(100);    
    client.stop();
    Serial.println("Data sent & connection closed.");
  } else {
    Serial.println("Connection to server failed.");
  }
}
