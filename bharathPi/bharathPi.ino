#define TINY_GSM_MODEM_SIM7600
#include <WiFi.h>
#include <TinyGsmClient.h>

// Wi-Fi AP credentials
const char* ssid = "BharatPi_AP";
const char* password = "12345678";

// TCP server on port 3333
WiFiServer server(3333);

// SIMCOM A7672 Serial and modem setup
#define SerialAT Serial1
#define UART_BAUD   115200
#define PIN_TX      17
#define PIN_RX      16
#define PWR_PIN     32

TinyGsm modem(SerialAT);

// Airtel APN
const char apn[] = "airtelgprs.com";
const char gprsUser[] = "";
const char gprsPass[] = "";

#define SMS_TARGET "8144404230"
// Default coordinates if GPS not available
const char* defaultLat = "10.655458650783398";
const char* defaultLon = "77.0361596345481";

// Forward declaration
void sendSMSWithLocation(const String& msg);

void setup() {
  Serial.begin(115200);

  // Start Wi-Fi Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.begin();  // Start TCP server
  Serial.println("TCP Server started.");

  // Power on the modem
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1000);
  digitalWrite(PWR_PIN, HIGH);

  // Init modem
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(3000);
  Serial.println("Initializing modem...");
  if (!modem.init()) {
    Serial.println("Failed to initialize modem!");
    while (true);
  }

  // Connect to mobile network
  Serial.println("Connecting to network...");
  if (!modem.waitForNetwork()) {
    Serial.println("Network connection failed!");
    while (true);
  }
  Serial.println("Network connected!");

  // GPRS connection
  Serial.print("Connecting to APN: ");
  Serial.println(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("APN connect failed");
    while (true);
  }
  Serial.println("APN connect success");
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client Connected.");
    while (client.connected()) {
      if (client.available()) {
        String msg = client.readStringUntil('\n');
        msg.trim();
        Serial.print("Received: ");
        Serial.println(msg);

        if (msg == "1") {
          sendSMSWithLocation("emergency button is clicked");
        } else if (msg == "2") {
          sendSMSWithLocation("emergency pulse is abnormal");
        } else if (msg == "3") {
          sendSMSWithLocation("vibration detected");
        } else {
          Serial.println("Unknown command received.");
        }
        delay(1000);
      }
    }
    client.stop();
    Serial.println("Client Disconnected.");
  }
}

// Function to get GPS coordinates (returns true if valid, false if not)
bool getGPSCoordinates(String &lat, String &lon) {
  modem.sendAT("+CGPS=1,1"); // Power on GNSS
  delay(2000);
  modem.sendAT("+CGPSINFO");
  String res = modem.stream.readStringUntil('\n');
  int idx = res.indexOf(":");
  if (idx > 0 && res.length() > idx + 1) {
    String data = res.substring(idx + 1);
    data.trim();
    if (data.length() > 10 && data.indexOf(",") > 0) {
      int comma1 = data.indexOf(",");
      int comma2 = data.indexOf(",", comma1 + 1);
      lat = data.substring(0, comma1);
      lon = data.substring(comma1 + 1, comma2);
      if (lat.length() > 0 && lon.length() > 0 && lat != "0.0" && lon != "0.0")
        return true;
    }
  }
  return false;
}

// Function to send SMS with location
void sendSMSWithLocation(const String& msg) {
  String latitude, longitude;
  if (!getGPSCoordinates(latitude, longitude)) {
    Serial.print("GPS found: ");
    latitude = defaultLat;
    longitude = defaultLon;
     Serial.print(latitude);
    Serial.print(", ");
    Serial.println(longitude);
  } else {
    Serial.print("GPS found: ");
    Serial.print(latitude);
    Serial.print(", ");
    Serial.println(longitude);
  }

  String gmapLink = "https://maps.google.com/?q=" + latitude + "," + longitude;
  String smsText = msg + "\nLocation: " + gmapLink;

  Serial.print("Sending SMS: ");
  Serial.println(smsText);

  if (modem.sendSMS(SMS_TARGET, smsText)) {
    Serial.println("SMS sent successfully!");
  } else {
    Serial.println("SMS failed!");
  }
}
