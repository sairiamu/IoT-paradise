// #include <DHT.h>
// #include <Servo.h>
// #include <SoftwareSerial.h>

// // ================= USER CONFIGURATION =================
// const String WIFI_SSID    = "wifi";
// const String WIFI_PASS    = "neyjudey@123";
// const String API_KEY      = "L72BHGUS5FUW4XQ0";
// const String CHANNEL_ID   = "3464994";
// // ======================================================

#include <DHT.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ================= USER CONFIGURATION =================
// Fill in your real values locally — do not commit these to any repo.
const String WIFI_SSID   = "wifi";
const String WIFI_PASS   = "neyjudey@123";
// const String WIFI_PASS    = "neyjudey@123";
const String API_KEY     = "L72BHGUS5FUW4XQ0";
const String CHANNEL_ID  = "3464994";
// ======================================================

// Pin Definitions
#define DHTPIN 2
#define DHTTYPE DHT11
#define PIR_PIN 3
#define TRIG_PIN 4
#define ECHO_PIN 5
#define SERVO_PIN 6
#define PUMP_PIN 7          // drives transistor base, NOT a relay
#define LED_PIN 8           // ON when all 3 conditions are true

#define TARGET_DISTANCE 5        // cm - exact distance required
#define FIRE_TEMP_THRESHOLD 10.0 // see note below — 10.0 in your last version
                                  // triggers "fire" at normal room temperature
#define POUR_ANGLE 180
#define REST_ANGLE 0

#define MIN_THINGSPEAK_INTERVAL 15000UL // ThingSpeak free tier requires >=15s between updates
#define WIFI_CONNECT_RETRIES 3

SoftwareSerial esp8266(10, 11);
DHT dht(DHTPIN, DHTTYPE);
Servo pipeServo;

int fireCount = 0;
bool wasPouring = false;
bool wifiConnected = false;
unsigned long lastPrintTime = 0;
unsigned long lastUploadTime = 0;
const unsigned long PRINT_INTERVAL = 1000;

// Last known-good sensor readings, used if a read glitches
float lastValidTemp = NAN;

long getDistance();
bool initESP8266();
String sendATCommand(const String &cmd, unsigned long timeoutMs);
void sendToThingSpeak(int count, float temp, long dist);
void setPump(bool on);
void setIndicators(bool conditionsMet);
float readTemperatureStable();

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  Serial.println(F("=========================================="));
  Serial.println(F("   FIRE EXTINGUISHER SYSTEM - v3 START    "));
  Serial.println(F("=========================================="));
  Serial.print(F("[CONFIG] ThingSpeak Channel ID: "));
  Serial.println(CHANNEL_ID);

  esp8266.begin(9600);

  dht.begin();
  pipeServo.attach(SERVO_PIN);
  pipeServo.write(REST_ANGLE);

  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  setPump(false);
  digitalWrite(LED_PIN, LOW);

  wifiConnected = initESP8266();
  if (!wifiConnected) {
    Serial.println(F("[WARN] Wi-Fi not confirmed connected. System will still run locally; ThingSpeak uploads will be skipped/retried."));
  }
}

void loop() {
  float temp = readTemperatureStable();
  bool pirDetected = (digitalRead(PIR_PIN) == HIGH);
  long distance = getDistance();

  bool fireDetected = (!isnan(temp) && temp > FIRE_TEMP_THRESHOLD);
  bool distanceOK   = (distance == TARGET_DISTANCE);

  bool allConditionsMet = fireDetected && pirDetected && distanceOK;

  // ---- Periodic status log ----
  if (millis() - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = millis();
    Serial.println(F("------------------------------------------"));
    Serial.print(F("Temp: "));
    Serial.print(isnan(temp) ? -1 : temp);
    Serial.print(F(" C | Fire: "));
    Serial.print(fireDetected ? F("YES") : F("NO"));
    Serial.print(F(" | PIR: "));
    Serial.print(pirDetected ? F("HUMAN") : F("CLEAR"));
    Serial.print(F(" | Distance: "));
    Serial.print(distance);
    Serial.println(F(" cm"));
  }

  // ---- Guidance: too close, human present ----
  if (distance < TARGET_DISTANCE && distance > 0 && pirDetected) {
    Serial.print(F("[GUIDANCE] Move backwards - current distance: "));
    Serial.print(distance);
    Serial.println(F(" cm"));
  }

  // ---- Main decision ----
  if (allConditionsMet) {
    if (!wasPouring) {
      fireCount++;
      Serial.println(F("\n!!! ALL CONDITIONS MET - EXTINGUISHING !!!"));
      Serial.print(F(">>> Fire Occurrence #"));
      Serial.println(fireCount);

      // Rate-limit ThingSpeak uploads so a flickering sensor can't spam
      // the API and get throttled/blocked by ThingSpeak.
      if (millis() - lastUploadTime >= MIN_THINGSPEAK_INTERVAL) {
        sendToThingSpeak(fireCount, temp, distance);
        lastUploadTime = millis();
      } else {
        Serial.println(F("[THINGSPEAK] Skipped - within rate-limit window."));
      }
    }

    pipeServo.write(POUR_ANGLE);
    setPump(true);
    setIndicators(true);
    wasPouring = true;

  } else {
    if (wasPouring) {
      Serial.println(F("[INFO] Conditions no longer met - stopping pump."));
    } else {
      Serial.println(F("[STATUS] No harm - system normal."));
    }

    setPump(false);
    pipeServo.write(REST_ANGLE);
    setIndicators(false);
    wasPouring = false;
  }

  delay(200); // shorter loop delay keeps PIR/distance checks responsive
}

// ================= HELPER FUNCTIONS =================

// Direct transistor switch — NOT active-low like a relay.
// HIGH = pump ON, LOW = pump OFF.
void setPump(bool on) {
  digitalWrite(PUMP_PIN, on ? HIGH : LOW);
}

// LED mirrors the "all conditions true" state.
void setIndicators(bool conditionsMet) {
  digitalWrite(LED_PIN, conditionsMet ? HIGH : LOW);
}

// DHT sensors occasionally return NaN on a bad read. Retry once, and if
// that also fails, fall back to the last known-good value rather than
// treating a transient glitch as "no fire" or crashing downstream logic.
float readTemperatureStable() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    delay(50);
    t = dht.readTemperature();
  }
  if (!isnan(t)) {
    lastValidTemp = t;
    return t;
  }
  return lastValidTemp; // may still be NAN on first-ever failed read; handled by isnan() checks upstream
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 0; // timeout / out of range - never equals TARGET_DISTANCE, so pump stays off. Safe default.

  return (duration * 0.034) / 2;
}

// Sends an AT command and waits (up to timeoutMs) for a response, so we can
// actually detect failure instead of blindly delaying and hoping it worked.
String sendATCommand(const String &cmd, unsigned long timeoutMs) {
  esp8266.println(cmd);
  unsigned long start = millis();
  String response = "";
  while (millis() - start < timeoutMs) {
    while (esp8266.available()) {
      response += (char)esp8266.read();
    }
  }
  return response;
}

bool initESP8266() {
  Serial.println(F("[ESP8266] Initializing Wi-Fi Module..."));

  String resp = sendATCommand("AT", 1000);
  if (resp.indexOf("OK") == -1) {
    Serial.println(F("[ESP8266] Module not responding to AT. Check wiring/power."));
    return false;
  }

  sendATCommand("AT+CWMODE=1", 1000);

  String cmd = "AT+CWJAP=\"" + WIFI_SSID + "\",\"" + WIFI_PASS + "\"";
  Serial.print(F("[ESP8266] Connecting to Wi-Fi: "));
  Serial.println(WIFI_SSID);

  for (int attempt = 1; attempt <= WIFI_CONNECT_RETRIES; attempt++) {
    String joinResp = sendATCommand(cmd, 6000);
    if (joinResp.indexOf("OK") != -1 || joinResp.indexOf("WIFI CONNECTED") != -1) {
      Serial.println(F("[ESP8266] Wi-Fi connected."));
      return true;
    }
    Serial.print(F("[ESP8266] Connect attempt "));
    Serial.print(attempt);
    Serial.println(F(" failed, retrying..."));
    delay(1000);
  }

  Serial.println(F("[ESP8266] Wi-Fi connection failed after retries."));
  return false;
}

void sendToThingSpeak(int count, float temp, long dist) {
  if (!wifiConnected) {
    Serial.println(F("[THINGSPEAK] Skipped - Wi-Fi not connected."));
    return;
  }

  Serial.print(F("\n[THINGSPEAK] Sending to Channel ID: "));
  Serial.println(CHANNEL_ID);

  String startResp = sendATCommand("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80", 2000);
  if (startResp.indexOf("ERROR") != -1) {
    Serial.println(F("[THINGSPEAK] TCP connect failed."));
    return;
  }

  String url = "GET /update?api_key=" + API_KEY +
               "&channel_id=" + CHANNEL_ID +
               "&field1=" + String(count) +
               "&field2=" + String(temp) +
               "&field3=" + String(dist) + "\r\n";

  sendATCommand("AT+CIPSEND=" + String(url.length()), 1000);
  esp8266.print(url);
  delay(1500);

  Serial.println(F("[THINGSPEAK] Data sent."));
  sendATCommand("AT+CIPCLOSE", 1000);
}

