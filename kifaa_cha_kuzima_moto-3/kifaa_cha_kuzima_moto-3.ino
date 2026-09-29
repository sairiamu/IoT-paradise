/*
  ===================================================================
  KIFAA CHA KUZIMA MOTO KIOTOMATIKI (Automatic Fire Extinguisher Bot)
  ===================================================================
  Vifaa (Components):
   - Arduino Uno
   - ESP8266 (ESP-01) - kutuma data ThingSpeak
   - DHT11 - kutambua ongezeko la joto (moto)
   - Ultrasonic Sensor HC-SR04 - kupima umbali wa kitu/mtu
   - Servo Motor (SG90) - kuelekeza bomba la maji
   - Relay Module + Mini Water Pump - kumwaga maji
   - Power supply ya nje (5V/1-2A) kwa pump na servo

  MUUNGANISHO (Wiring):
   DHT11         -> DATA: D5 , VCC: 5V , GND: GND
   HC-SR04       -> TRIG: D9 , ECHO: D10 , VCC: 5V , GND: GND
   Servo         -> Signal: D6 , VCC: 5V(nje) , GND: GND
   Relay(Pump)   -> IN: D7 , VCC: 5V , GND: GND
   ESP8266(ESP01)-> RX: D3 (kupitia voltage divider 5V->3.3V)
                    TX: D4
                    VCC & CH_PD: 3.3V (TUMIA REGULATOR TOFAUTI, si 5V!)
                    GND: GND (share GND na Arduino)

  MANTIKI (Logic):
   1. DHT11 ikisoma joto zaidi ya TEMP_THRESHOLD -> tunathibitisha
      kuna moto -> ongeza kihesabio cha "fire occurrence"
   2. Ultrasonic sensor inapima umbali wa kitu (mtu) mbele ya bomba
   3. Lengo (TARGET_CM) ni sentimita 5:
        - Kama umbali > TARGET_CM  -> servo inazunguka MBELE kutafuta lengo
        - Kama umbali < TARGET_CM  -> servo inazunguka NYUMA (backward) kutafuta lengo
        - Kama umbali == TARGET_CM (ndani ya tolerance) -> ACHA servo, WASHA pump
   4. Baada ya muda wa kumwagilia, pump inazimika na servo inarudi
      kwenye position ya "kusubiri" (home position)
   5. Data (fire status, distance, fire count, joto) inatumwa
      Arduino->ESP8266 kwa AT commands kila baada ya SEND_INTERVAL,
      kisha ESP inatuma ThingSpeak (Field1=Fire status, Field2=Distance,
      Field3=FireCount, Field4=Temperature)
  ===================================================================
*/

#include <Servo.h>
#include <SoftwareSerial.h>
#include <DHT.h>

// ---------- PINS ----------
#define TRIG_PIN      9
#define ECHO_PIN      10
#define SERVO_PIN     6
#define PUMP_RELAY_PIN 7
#define DHT_PIN       5
#define DHTTYPE       DHT11

// ESP8266 kwa SoftwareSerial
SoftwareSerial espSerial(3, 4); // RX, TX (Arduino)

Servo waterServo;
DHT dht(DHT_PIN, DHTTYPE);

// ---------- MIPANGILIO (settings) ----------
const float TARGET_CM   = 5.0;   // umbali lengwa (cm)
const float TOLERANCE   = 0.5;   // uvumilivu wa kufikia lengo
const int   SERVO_MIN   = 20;    // angle ya chini ya servo
const int   SERVO_MAX   = 160;   // angle ya juu ya servo
const int   SERVO_STEP  = 2;     // hatua ya kuzunguka kila mzunguko
const float TEMP_THRESHOLD = 20.0; // nyuzi joto (°C) - thibitisho la pili la moto
const unsigned long PUMP_ON_TIME   = 4000;  // muda pump inamwaga maji (ms)
const unsigned long SEND_INTERVAL  = 15000; // muda wa kutuma ThingSpeak (ms)
const unsigned long DHT_READ_INTERVAL = 2000; // DHT11 haiwezi kusomwa haraka zaidi ya hii

// ---------- THINGSPEAK ----------
const char* WIFI_SSID   = "~~moran-327";
const char* WIFI_PASS   = "o2006wifi!00x";
const char* TS_API_KEY  = "YQ8MFSPI0PMWZ21N";   // ThingSpeak Write API Key
const char* TS_HOST     = "api.thingspeak.com";

// ---------- HALI (state) ----------
int   servoAngle   = 90;
bool  servoForward  = true;
int   fireCount     = 0;  
bool  fireActive    = false;
bool  pumpOn        = false;
unsigned long pumpStartTime = 0;
unsigned long lastSend      = 0;
unsigned long lastDHTRead   = 0;
float currentTemp = 0;

// =========================================================
void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, LOW); // pump imezimwa mwanzoni

  waterServo.attach(SERVO_PIN);
  waterServo.write(servoAngle);

  dht.begin();

  Serial.println("Inaanzisha WiFi ya ESP8266...");
  connectWiFi();
}

// =========================================================
void loop() {
  // soma DHT11 kila DHT_READ_INTERVAL (haiwezi kusomwa haraka zaidi)
  if (millis() - lastDHTRead > DHT_READ_INTERVAL) {
    float t = dht.readTemperature();
    if (!isnan(t)) currentTemp = t; // ikishindwa kusoma, tunabaki na thamani ya awali
    lastDHTRead = millis();
  }

  // MOTO: joto la DHT11 limevuka kiwango cha TEMP_THRESHOLD
  bool fireDetected = (currentTemp >= TEMP_THRESHOLD);

  if (fireDetected) {
    if (!fireActive) {
      fireActive = true;
      fireCount++;                 // hesabu tukio jipya la moto
      Serial.print("MOTO UMEGUNDULIWA! Idadi ya matukio: ");
      Serial.println(fireCount);
    }

    float distance = getDistanceCM();
    Serial.print("Umbali wa kitu/mtu: ");
    Serial.print(distance);
    Serial.println(" cm");

    handleAiming(distance);

  } else {
    fireActive = false;
    // hakuna moto -> zima pump na rudisha servo home
    if (pumpOn) {
      digitalWrite(PUMP_RELAY_PIN, LOW);
      pumpOn = false;
    }
    waterServo.write(90); // position ya kusubiri
  }

  // tuma data ThingSpeak kila SEND_INTERVAL
  if (millis() - lastSend > SEND_INTERVAL) {
    float d = fireDetected ? getDistanceCM() : -1;
    sendToThingSpeak(fireDetected ? 1 : 0, d, fireCount, currentTemp);
    lastSend = millis();
  }

  delay(100);
}

// =========================================================
// Kupima umbali kwa HC-SR04
float getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  if (duration == 0) return -1; // hakuna kitu kilichogunduliwa

  float distance = duration * 0.0343 / 2.0;
  return distance;
}

// =========================================================
// Mantiki ya kuelekeza servo na kuwasha pump
void handleAiming(float distance) {
  if (distance < 0) {
    // hakuna usomaji sahihi, endelea kutafuta
    sweepServo();
    return;
  }

  float diff = distance - TARGET_CM;

  if (abs(diff) <= TOLERANCE) {
    // ---- LENGO LIMEFIKIWA (5cm) ----
    if (!pumpOn) {
      digitalWrite(PUMP_RELAY_PIN, HIGH);
      pumpOn = true;
      pumpStartTime = millis();
      Serial.println("Lengo limefikiwa (5cm) -> PUMP IMEWASHWA, inamwaga maji.");
    }
    // zima pump baada ya muda uliopangwa
    if (pumpOn && millis() - pumpStartTime >= PUMP_ON_TIME) {
      digitalWrite(PUMP_RELAY_PIN, LOW);
      pumpOn = false;
      Serial.println("Kumwagilia kumekamilika -> PUMP IMEZIMWA.");
    }

  } else if (diff < 0) {
    // Mtu/kitu kiko KARIBU zaidi ya 5cm (chini ya cm 5)
    // -> servo izunguke NYUMA (backward) kutafuta hizo 5cm
    if (pumpOn) { digitalWrite(PUMP_RELAY_PIN, LOW); pumpOn = false; }
    moveServoBackward();

  } else {
    // Mtu/kitu yuko MBALI zaidi ya 5cm
    // -> servo izunguke MBELE kutafuta hizo 5cm
    if (pumpOn) { digitalWrite(PUMP_RELAY_PIN, LOW); pumpOn = false; }
    moveServoForward();
  }
}

void moveServoForward() {
  servoAngle += SERVO_STEP;
  if (servoAngle > SERVO_MAX) servoAngle = SERVO_MAX;
  waterServo.write(servoAngle);
  delay(40);
}

void moveServoBackward() {
  servoAngle -= SERVO_STEP;
  if (servoAngle < SERVO_MIN) servoAngle = SERVO_MIN;
  waterServo.write(servoAngle);
  delay(40);
}

void sweepServo() {
  // kutafuta kitu wakati hakuna usomaji wa ultrasonic
  if (servoForward) {
    servoAngle += SERVO_STEP;
    if (servoAngle >= SERVO_MAX) servoForward = false;
  } else {
    servoAngle -= SERVO_STEP;
    if (servoAngle <= SERVO_MIN) servoForward = true;
  }
  waterServo.write(servoAngle);
  delay(40);
}

// =========================================================
// ESP8266: kuunganisha WiFi kwa AT commands
void connectWiFi() {
  sendATCommand("AT+RST", 2000);
  sendATCommand("AT+CWMODE=1", 1000);
  String cmd = "AT+CWJAP=\"" + String(WIFI_SSID) + "\",\"" + String(WIFI_PASS) + "\"";
  sendATCommand(cmd, 8000);
}

// Kutuma data ThingSpeak (Field1=Fire, Field2=Distance, Field3=FireCount, Field4=Temperature)
void sendToThingSpeak(int fireStatus, float distance, int count, float temperature) {
  espSerial.println("AT+CIPSTART=\"TCP\",\"" + String(TS_HOST) + "\",80");
  delay(1000);

  String getStr = "GET /update?api_key=" + String(TS_API_KEY) +
                   "&field1=" + String(fireStatus) +
                   "&field2=" + String(distance) +
                   "&field3=" + String(count) +
                   "&field4=" + String(temperature) +
                   "\r\n\r\n";

  String cipSend = "AT+CIPSEND=" + String(getStr.length());
  espSerial.println(cipSend);
  delay(500);
  espSerial.print(getStr);
  delay(1500);

  Serial.println("Data imetumwa ThingSpeak.");
}

// Kazi ya msaada: kutuma AT command na kusoma majibu
void sendATCommand(String cmd, int waitTime) {
  espSerial.println(cmd);
  long t = millis();
  while (millis() - t < waitTime) {
    if (espSerial.available()) {
      Serial.write(espSerial.read());
    }
  }
}
