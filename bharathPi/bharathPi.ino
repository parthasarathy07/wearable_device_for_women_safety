// bharathPi.ino
#define TINY_GSM_MODEM_SIM7600
#include <WiFi.h>
#include <TinyGsmClient.h>

#define SerialAT   Serial1
#define UART_BAUD  115200
#define PIN_TX     17
#define PIN_RX     16
#define PWR_PIN    32

TinyGsm modem(SerialAT);

const char* ssid     = "BharatPi_AP";
const char* password = "12345678";
WiFiServer server(3333);

const char apn[]      = "airtelgprs.com";
const char gprsUser[] = "";
const char gprsPass[] = "";

#define SMS_TARGET "9944966487"

const char* defaultlatitude  = "10.655458650783398";
const char* defaultlongitude = "77.0361596345481";

struct GPSData {
  String latitude;
  String longitude;
  bool   valid;
};

/* ─── Forward declarations ─────────────────────────────────────────── */
void    setupWiFiAP();
void    setupTCPServer();
void    powerOnModem();
void    initModem();
bool    connectToNetwork(unsigned long timeoutMs);
bool    connectToGPRS(unsigned long timeoutMs);
void    handleClientConnection();
void    makeCall();
void    sendSMSWithLocation(const String& msg);
GPSData getGPSCoordinates(unsigned long timeoutMs);
void    safeDelay(unsigned long ms);
double  nmeaToDegrees(const String& raw); // FIX: NMEA → decimal conversion

/* ─── Helper: convert NMEA DDMM.MMMM to decimal degrees ────────────── */
double nmeaToDegrees(const String& raw) {
  // raw example: "1039.3281" (lat) or "07702.1696" (lon)
  double val = raw.toDouble();
  int    deg = (int)(val / 100);
  double min = val - deg * 100.0;
  return deg + min / 60.0;
}

/* ─── WiFi AP ──────────────────────────────────────────────────────── */
void setupWiFiAP() {
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

/* ─── TCP server ───────────────────────────────────────────────────── */
void setupTCPServer() {
  server.begin();
  Serial.println("TCP Server started.");
}

/* ─── Modem power ──────────────────────────────────────────────────── */
void powerOnModem() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(200);
  digitalWrite(PWR_PIN, HIGH);
  delay(1200);
  digitalWrite(PWR_PIN, LOW);
  delay(2000);
  Serial.println("Power-on pulse sent to modem.");
}

/* ─── Modem init ───────────────────────────────────────────────────── */
void initModem() {
  Serial.println("Initialising modem (TinyGSM)...");
  if (!modem.init()) {
    Serial.println("modem.init() failed – continuing with limited functionality.");
  } else {
    Serial.println("Modem initialised.");
  }
}

/* ─── Network ──────────────────────────────────────────────────────── */
bool connectToNetwork(unsigned long timeoutMs) {
  Serial.println("Waiting for network...");
  unsigned long start = millis();
  while (!modem.waitForNetwork() && (millis() - start) < timeoutMs) {
    delay(1000);
    Serial.print(".");
  }
  if (!modem.isNetworkConnected()) {
    Serial.println("\nNetwork not available.");
    return false;
  }
  Serial.println("\nNetwork connected!");
  return true;
}

/* ─── GPRS ─────────────────────────────────────────────────────────── */
bool connectToGPRS(unsigned long timeoutMs) {
  Serial.print("Connecting to APN: ");
  Serial.println(apn);

  // FIX: removed the duplicate gprsConnect() call inside the loop body
  unsigned long start = millis();
  while ((millis() - start) < timeoutMs) {
    if (modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println("\nAPN connect success.");
      return true;
    }
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nAPN connect failed.");
  return false;
}

/* ─── TCP client handler ───────────────────────────────────────────── */
void handleClientConnection() {
  WiFiClient client = server.available();
  if (!client) return;

  Serial.println("Client connected.");
  client.setTimeout(2000);

  while (client.connected()) {
    if (client.available()) {
      String msg = client.readStringUntil('\n');
      msg.trim();
      Serial.print("Received: ");
      Serial.println(msg);

      if (msg == "1") {
        String em = "emergency button is clicked";
sendSMSWithLocation(em);
delay(2000);
makeCall();
      } else if (msg == "2") {
        String em = "emergency, pulse is abnormal";
sendSMSWithLocation(em);
delay(2000);
makeCall();
      } else if (msg == "3") {
        String em = "emergency, Action is abnormal";
sendSMSWithLocation(em);
delay(2000);
makeCall();
      } else {
        Serial.println("Unknown command received.");
      }
      delay(100);
    } else {
      delay(10);
    }
  }

  client.stop();
  Serial.println("Client disconnected.");
}

/* ─── Voice call + TTS ─────────────────────────────────────────────── */
void makeCall() {

  // Ensure network
  if (!modem.isNetworkConnected()) {
    Serial.println("Reconnecting network...");
    if (!modem.waitForNetwork(10000)) {
      Serial.println("Network failed. Call aborted.");
      return;
    }
  }

  Serial.println("Calling...");

  bool success = false;

  for (int i = 0; i < 2; i++) {   // retry 2 times
    if (modem.callNumber(SMS_TARGET)) {
      success = true;
      break;
    }
    Serial.println("Retrying call...");
    delay(3000);
  }

  if (!success) {
    Serial.println("Call failed.");
    return;
  }

  Serial.println("Call active...");
  delay(10000);  // let it ring/talk

  modem.callHangup();

  Serial.println("Call ended.");
}

/* ─── SMS with GPS location ────────────────────────────────────────── */
void sendSMSWithLocation(const String& msg) {

  if (msg.length() == 0) {
    Serial.println("Empty message. Skipping SMS.");
    return;
  }

  if (!modem.isNetworkConnected()) {
    Serial.println("Reconnecting network...");
    if (!modem.waitForNetwork(10000)) {
      Serial.println("Network failed. SMS aborted.");
      return;
    }
  }

  GPSData gps = getGPSCoordinates(10000);

  if (!gps.valid) {
    gps.latitude  = defaultlatitude;
    gps.longitude = defaultlongitude;
  }

  String link = "https://maps.google.com/?q=" + gps.latitude + "," + gps.longitude;
  String sms  = msg + "\nLocation: " + link;

  Serial.println("Sending SMS...");

  bool success = false;

  for (int i = 0; i < 3; i++) {
    if (modem.sendSMS(SMS_TARGET, sms)) {
      success = true;
      break;
    }
    Serial.println("Retrying SMS...");
    delay(2000);
  }

  Serial.println(success ? "SMS SENT" : "SMS FAILED");
}

/* ─── GPS ──────────────────────────────────────────────────────────── */
GPSData getGPSCoordinates(unsigned long timeoutMs) {
  GPSData gps;
  gps.valid = false;
  return gps;

  SerialAT.print("AT+CGPS=1,1\r\n");
  delay(500);

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    SerialAT.print("AT+CGPSINFO\r\n");

    unsigned long waitStart = millis();
    while (millis() - waitStart < 1500) {
      if (SerialAT.available()) {
        String line = SerialAT.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        Serial.print("GPS RESP: "); Serial.println(line);

        if (line.startsWith("+CGPSINFO:")) {
          String payload = line.substring(strlen("+CGPSINFO:"));
          payload.trim();

          if (payload.length() > 5) {
            int p1 = payload.indexOf(',');
            int p2 = payload.indexOf(',', p1 + 1);
            int p3 = payload.indexOf(',', p2 + 1);
            int p4 = payload.indexOf(',', p3 + 1);

            String lat    = (p1 > 0)            ? payload.substring(0, p1)          : "";
            String latDir = (p2 > p1)            ? payload.substring(p1 + 1, p2)    : "";
            String lon    = (p3 > p2)            ? payload.substring(p2 + 1, p3)    : "";
            String lonDir = (p4 > p3)            ? payload.substring(p3 + 1, p4)    : "";

            lat.trim(); latDir.trim(); lon.trim(); lonDir.trim();

            if (lat.length() > 3 && lon.length() > 3 &&
                lat != "0.0"      && lon != "0.0") {

              // FIX: convert NMEA DDMM.MMMM to decimal degrees
              double latDec = nmeaToDegrees(lat);
              double lonDec = nmeaToDegrees(lon);
              if (latDir == "S") latDec = -latDec;
              if (lonDir == "W") lonDec = -lonDec;

              // Store as decimal-degree strings (6 decimal places)
              char latBuf[20], lonBuf[20];
              snprintf(latBuf, sizeof(latBuf), "%.6f", latDec);
              snprintf(lonBuf, sizeof(lonBuf), "%.6f", lonDec);

              gps.latitude  = String(latBuf);
              gps.longitude = String(lonBuf);
              gps.valid     = true;
              return gps;
            }
          }
        }
      }
      delay(100);
    }
    delay(500);
  }

  return gps;
}

/* ─── safeDelay ────────────────────────────────────────────────────── */
void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) yield();
}

/* ─── setup / loop ─────────────────────────────────────────────────── */
void setup() {
  Serial.begin(115200);
  delay(100);
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(100);

  setupWiFiAP();
  setupTCPServer();
  powerOnModem();
  initModem();

  if (!connectToNetwork(30000)) Serial.println("Warning: network not available.");
  if (!connectToGPRS(20000))    Serial.println("Warning: GPRS not connected.");
}

void loop() {
  handleClientConnection();
  safeDelay(10);
}
