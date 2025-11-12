#include <WiFi.h>

const char* ssid = "BharatPi_AP";
const char* password = "12345678";
const char* serverIP = "192.168.4.1"; // Bharat Pi's AP IP
const uint16_t serverPort = 3333;

WiFiClient client;

#define VIBRATION_PIN 13
#define BUTTON_PIN 19

void setup() {
  Serial.begin(115200);
  connectWifi();
  setupPins();
  Serial.println("Monitoring started: vibration, button, pulse");
}

void loop() {
  handle_vibration();
  handle_button();
  delay(100);
}

void handle_vibration(){
  int vibration = digitalRead(VIBRATION_PIN);
  if (vibration == HIGH) {
    delay(500);
    vibration = digitalRead(VIBRATION_PIN);
    if (vibration == HIGH) {
    Serial.println("ALERT: Vibration detected!");
    sentData(3);
    }
  }
}

void handle_button(){
  int button=digitalRead(BUTTON_PIN);
  if(button==LOW){
    Serial.println("ALERT: button pressed");
    sentData(1);
    delay(500);
  }
}

void sentData(int data){
  if (client.connect(serverIP, serverPort)) {
    Serial.println("sended to Bharat Pi Server.");
    client.println(data);
    client.stop();
  }else {
    Serial.println("Connection to server failed.");
    connectWifi();
  }
}

void setupPins() {
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void connectWifi(){
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Bharat Pi AP...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
}
