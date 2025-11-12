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

const char* defaultlatitude = "10.655458650783398";
const char* defaultlongitude = "77.0361596345481";

struct GPSData {
  String latitude;
  String longitude;
  bool valid;
};

void setup() {
  Serial.begin(115200);
  setupWiFiAP();
  setupTCPServer();
  powerOnModem();
  initModem();
  connectToNetwork();
  connectToGPRS();
}

void loop() {
  handleClientConnection();
}

void setupWiFiAP() {
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

void setupTCPServer() {
  server.begin();  // Start TCP server
  Serial.println("TCP Server started.");
}

void powerOnModem() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1000);
  digitalWrite(PWR_PIN, HIGH);
}

void initModem() {
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(3000);
  Serial.println("Initializing modem...");
  if (!modem.init()) {
    Serial.println("Failed to initialize modem!");
    while (true);
  }
}

void connectToNetwork() {
  Serial.println("Connecting to network...");
  if (!modem.waitForNetwork()) {
    Serial.println("Network connection failed!");
    while (true);
  }
  Serial.println("Network connected!");
}

void connectToGPRS() {
  Serial.print("Connecting to APN: ");
  Serial.println(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("APN connect failed");
    while (true);
  }
  Serial.println("APN connect success");
}

void handleClientConnection() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client Connected.");
    while (client.connected()) {
      if (client.available()) {
        String msg = client.readStringUntil('\n');
        msg.trim();
        Serial.print("Received: ");
        Serial.println(msg);
        String meta_msg="This is an emergency alert. Please check on the user immediately. ";
        if (msg == "1") {

          String emergencyMessage ="emergency button is clicked";
          sendSMSWithLocation(emergencyMessage);
          makeCallAndPlayVoiceMessage(meta_msg + emergencyMessage, 30000);

        } else if (msg == "2") {

          String emergencyMessage ="emergency pulse is abnormal";
          sendSMSWithLocation(emergencyMessage);
          makeCallAndPlayVoiceMessage(meta_msg + emergencyMessage, 30000);

        } else if (msg == "3") {

          String emergencyMessage ="vibration detected";
          sendSMSWithLocation(emergencyMessage);
          makeCallAndPlayVoiceMessage(meta_msg + emergencyMessage, 30000);

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

void makeCallAndPlayVoiceMessage(const String& voiceMessage, unsigned int ringTimeout) {
  Serial.print("Attempting to call: ");
  Serial.println(SMS_TARGET);

  if (!modem.callNumber(SMS_TARGET)) {
    Serial.println("Failed to initiate the call.");
    return;
  }

  Serial.println("Call initiated, waiting for answer...");

  bool callAnswered = false;
  unsigned long startTime = millis();
  while (millis() - startTime < ringTimeout) {
    // AT+CLCC checks the status of the current call. A '0' in the 3rd parameter means the call is active.
    modem.sendAT("+CLCC");
    if (modem.waitResponse(",0,") > 0) {
      Serial.println("Call was answered!");
      callAnswered = true;
      break;
    }
    delay(1000);
  }

  // 3. If the call was answered, play the voice message
  if (callAnswered) {
    Serial.println("Playing voice message...");

    // Enable Text-to-Speech mode on the modem
    modem.sendAT("+CTTS=1");
    modem.waitResponse();

    // Construct the AT command to play the message
    String ttsCommand = "AT+CTTS=2,\"" + voiceMessage + "\"";
    modem.sendAT(ttsCommand.c_str());
    modem.waitResponse();

    // Wait for the message to finish playing. This is an estimate.
    // (e.g., 4 seconds base + 150ms per character)
    delay(4000 + voiceMessage.length() * 150);

    Serial.println("Message finished playing.");

  } else {
    Serial.println("Call was not answered within the timeout period.");
  }

  // 4. Hang up the call
  Serial.println("Hanging up the call.");
  modem.callHangup();
}

void sendSMSWithLocation(const String& msg) {
  GPSData gps = getGPSCoordinates();

  if (!gps.valid) {
    Serial.print("GPS not found, using default coordinates: ");
    gps.latitude = defaultlatitude;
    gps.longitude = defaultlongitude;
  } else {
    Serial.print("GPS found: ");
  }

  Serial.print(gps.latitude);
  Serial.print(", ");
  Serial.println(gps.longitude);

  String gmapLink = "https://maps.google.com/?q=" + gps.latitude + "," + gps.longitude;
  String smsText = msg + "\nLocation: " + gmapLink;

  Serial.print("Sending SMS: ");
  Serial.println(smsText);

  if (modem.sendSMS(SMS_TARGET, smsText)) {
    Serial.println("SMS sent successfully!");
  } else {
    Serial.println("SMS failed!");
  }
}

GPSData getGPSCoordinates(String &lat, String &lon) {
  GPSData gps;
  gps.valid = false;
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
      gps.latitude = data.substring(0, comma1);
      gps.longitude = data.substring(comma1 + 1, comma2);

      if (gps.latitude.length() > 0 && gps.longitude.length() > 0 &&
          gps.latitude != "0.0" && gps.longitude != "0.0") {
        gps.valid = true;
      }
    }
  }
  return gps;
}
