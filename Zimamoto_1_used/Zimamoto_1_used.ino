// #include <DHT.h>
// #include <Servo.h>
// #include <SoftwareSerial.h>

// // ================= USER CONFIGURATION =================
// const String WIFI_SSID = "~moran-327~";
// const String WIFI_PASS = "o2006wifi!00x";
// const String API_KEY   = "L72BHGUS5FUW4XQ0";
// // ======================================================

#include <DHT.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ================= USER CONFIGURATION =================
const String WIFI_SSID    = "~moran-327~";
const String WIFI_PASS    = "o2006wifi!00x";
const String API_KEY      = "L72BHGUS5FUW4XQ0";
const String CHANNEL_ID   = "3464994";
// ======================================================

#define DHTPIN 2
#define DHTTYPE DHT11
#define PIR_PIN 3
#define TRIG_PIN 4
#define ECHO_PIN 5
#define SERVO_PIN 6
#define RELAY_PIN 7

#define TARGET_DISTANCE 5   // cm - the ONLY distance at which the pump may fire

SoftwareSerial esp8266(10, 11);
DHT dht(DHTPIN, DHTTYPE);
Servo pipeServo;

int servoAngle = 90;
int fireCount = 0;
bool fireDetected = false;
unsigned long lastSerialPrintTime = 0;
const unsigned long SERIAL_PRINT_INTERVAL = 1500;

long getDistance();
void initESP8266();
void sendToThingSpeak(int count, float temp, long dist);
void setRelay(bool on); // helper to make relay state explicit everywhere

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  Serial.println(F("\n=========================================="));
  Serial.println(F("   FIRE EXTINGUISHER GATTY SYSTEM START   "));
  Serial.println(F("=========================================="));
  Serial.print(F("[CONFIG] ThingSpeak Channel ID: "));
  Serial.println(CHANNEL_ID);

  esp8266.begin(9600);

  dht.begin();
  pipeServo.attach(SERVO_PIN);
  pipeServo.write(servoAngle);

  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  setRelay(false); // pump OFF by default

  initESP8266();
}

void loop() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int pirState = digitalRead(PIR_PIN);
  long currentDistance = getDistance();

  // Safety net: unless we are EXACTLY at the target distance, pump stays off.
  // This runs every loop iteration regardless of what branch below does.
  if (currentDistance != TARGET_DISTANCE) {
    setRelay(false);
  }

  if (millis() - lastSerialPrintTime >= SERIAL_PRINT_INTERVAL) {
    lastSerialPrintTime = millis();
    Serial.println(F("------------------------------------------"));
    Serial.print(F("[STATUS] Temp: "));
    if (isnan(temp)) Serial.print(F("Error"));
    else { Serial.print(temp); Serial.print(F(" C")); }
    Serial.print(F(" | Humidity: "));
    if (isnan(humidity)) Serial.print(F("Error"));
    else { Serial.print(humidity); Serial.print(F("%")); }
    Serial.print(F(" | Motion (PIR): "));
    Serial.print(pirState == HIGH ? F("DETECTED") : F("CLEAR"));
    Serial.print(F(" | Distance: "));
    Serial.print(currentDistance);
    Serial.println(F(" cm"));
    Serial.print(F("[SERVO] Angle: "));
    Serial.print(servoAngle);
    Serial.print(F(" deg | Fire Count: "));
    Serial.println(fireCount);
  }

  if (!isnan(temp) && temp > 45.0) {
    if (!fireDetected) {
      fireCount++;
      fireDetected = true;
      Serial.println(F("\n!!! EMERGENCY: FIRE DETECTED !!!"));
      Serial.print(F(">>> Total Fire Occurrences: "));
      Serial.println(fireCount);
      sendToThingSpeak(fireCount, temp, currentDistance);
    }

    if (pirState == HIGH) {
      Serial.println(F("[ACTION] Person detected during fire! Aligning nozzle..."));

      // Align servo step by step until distance reads exactly TARGET_DISTANCE
      while (currentDistance != TARGET_DISTANCE) {
        currentDistance = getDistance();

        if (currentDistance < TARGET_DISTANCE && servoAngle > 0) {
          servoAngle -= 5;
          Serial.print(F("[SERVO] Rotating Backward -> Angle: "));
          Serial.print(servoAngle);
          Serial.print(F(" deg | Distance: "));
          Serial.print(currentDistance);
          Serial.println(F(" cm"));
        }
        else if (currentDistance > TARGET_DISTANCE && servoAngle < 180) {
          servoAngle += 5;
          Serial.print(F("[SERVO] Rotating Forward -> Angle: "));
          Serial.print(servoAngle);
          Serial.print(F(" deg | Distance: "));
          Serial.print(currentDistance);
          Serial.println(F(" cm"));
        }

        pipeServo.write(servoAngle);
        delay(150);

        if (currentDistance > 200 || currentDistance <= 0) {
          Serial.println(F("[WARNING] Target lost or Out of Range!"));
          break; // exits without ever firing the pump
        }
      }

      // Re-measure right before firing — only fire if EXACTLY at target.
      long confirmDistance = getDistance();
      if (confirmDistance == TARGET_DISTANCE) {
        Serial.println(F("\n>>> ALIGNED AT 5 CM EXACTLY! ACTIVATING WATER PUMP... <<<"));
        setRelay(true);
        delay(3000);

        // Re-check after the pour too — if something moved, cut it immediately.
        setRelay(false);
        Serial.println(F(">>> WATER PUMP DEACTIVATED <<<\n"));
      } else {
        Serial.print(F("[INFO] Distance drifted to "));
        Serial.print(confirmDistance);
        Serial.println(F(" cm before firing — pump NOT activated."));
      }
    }
  } else {
    if (fireDetected) {
      Serial.println(F("[INFO] Fire extinguished. System returning to normal."));
    }
    fireDetected = false;
    setRelay(false);
  }

  delay(100);
}

// ================= HELPER FUNCTIONS =================

// Centralizes relay control so pump state is always explicit and consistent.
// Active-LOW relay assumed: LOW = pump ON, HIGH = pump OFF.
void setRelay(bool on) {
  digitalWrite(RELAY_PIN, on ? LOW : HIGH);
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 0;

  return (duration * 0.034) / 2;
}

void initESP8266() {
  Serial.println(F("[ESP8266] Initializing Wi-Fi Module..."));
  esp8266.println("AT");
  delay(1000);
  esp8266.println("AT+CWMODE=1");
  delay(1000);

  String cmd = "AT+CWJAP=\"" + WIFI_SSID + "\",\"" + WIFI_PASS + "\"";
  Serial.print(F("[ESP8266] Connecting to Wi-Fi: "));
  Serial.println(WIFI_SSID);
  esp8266.println(cmd);
  delay(6000);

  Serial.println(F("[ESP8266] Wi-Fi Connection process completed."));
}

void sendToThingSpeak(int count, float temp, long dist) {
  Serial.print(F("\n[THINGSPEAK] Preparing packet for Channel ID: "));
  Serial.println(CHANNEL_ID);

  esp8266.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80");
  delay(2000);

  String url = "GET /update?api_key=" + API_KEY +
               "&channel_id=" + CHANNEL_ID +
               "&field1=" + String(count) +
               "&field2=" + String(temp) +
               "&field3=" + String(dist) + "\r\n";

  String sendCmd = "AT+CIPSEND=" + String(url.length());
  esp8266.println(sendCmd);
  delay(1000);
  esp8266.print(url);
  delay(2000);

  Serial.print(F("[THINGSPEAK] Data Sent -> Channel: "));
  Serial.print(CHANNEL_ID);
  Serial.print(F(" | Fire Occurrences: "));
  Serial.print(count);
  Serial.print(F(" | Temp: "));
  Serial.print(temp);
  Serial.print(F(" C | Dist: "));
  Serial.print(dist);
  Serial.println(F(" cm"));

  esp8266.println("AT+CIPCLOSE");
  delay(1000);
}


