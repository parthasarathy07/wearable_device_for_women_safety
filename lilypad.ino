#include <WiFi.h>

const char* ssid = "BharatPi_AP";
const char* password = "12345678";
const char* serverIP = "192.168.4.1"; // Bharat Pi's AP IP
const uint16_t serverPort = 3333;

WiFiClient client;

#define VIBRATION_PIN 13
#define BUTTON 19

void setup() {
  Serial.begin(115200);
  connectWifi();
  pinMode(VIBRATION_PIN, INPUT);
  Serial.println("Vibration sensor monitoring started");
  Serial.println("pulse sensor monitoring started");
  Serial.println("buttton monitoring started");
  pinMode(BUTTON,INPUT_PULLUP);
}

void loop() {
  int vibration = digitalRead(VIBRATION_PIN);
  if (vibration == 1) {
    delay(500);
    vibration = digitalRead(VIBRATION_PIN);
    if (vibration == 1) {
    Serial.println("ALERT: Vibration detected!");
    sentData(3);
    }
  }

  int button=digitalRead(BUTTON);
  if(button==0){
    Serial.println("ALERT: button pressed");
    sentData(1);
    delay(500);
  }
  delay(100);
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

void sentData(int i){
  if (client.connect(serverIP, serverPort)) {
    Serial.println("sended to Bharat Pi Server.");
    client.println(i);
    client.stop();
  }else {
    Serial.println("Connection to server failed.");
    connectWifi();
  }
}