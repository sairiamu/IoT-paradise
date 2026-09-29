/*
  =====================================================================
  FIRE EXTINGUISHER ROBOT - STEP 1  (FIXED)
  Central Logic + Static Sensor Integration (no chassis/drive motors)
  Board: Arduino Uno
  =====================================================================

  WIRING SUMMARY (see companion wiring guide for full detail):
    PIR OUT              -> D3
    DHT11 DATA           -> D2  (10k pull-up to 5V if not on breakout)
    Servo signal         -> D6  (servo power from EXTERNAL battery, not Arduino 5V)
    NPN base (pump)      -> D7  (via 1k resistor)
    HC-SR04 TRIG         -> D8
    HC-SR04 ECHO         -> D9
    ESP8266 TX           -> D10 (Arduino RX)
    Arduino TX (D11)     -> ESP8266 RX (THROUGH VOLTAGE DIVIDER, 5V->3.3V)
    Flame sensor node    -> A0  (reverse-biased photodiode + 10k pull-up to 5V)

  Flame sensor logic: with this wiring, MORE fire intensity = LOWER analog
  reading. Recalibrate FLAME_THRESHOLD to your actual sensor/lighting.

  =====================================================================
  WHAT WAS ACTUALLY BROKEN (compared against your working succy2.ino):
  =====================================================================
  1) SERVO vs SOFTWARESERIAL TIMER CONFLICT (root cause of failed sends)
     succy2.ino never uses Servo.h. This file calls pipeServo.attach()
     once in setup() and leaves it attached FOREVER. The AVR Servo
     library hijacks Timer1 and fires a compare-match interrupt every
     ~20ms for as long as a servo is attached. SoftwareSerial at
     115200 baud needs to sample each bit within ~8.7 microseconds -
     any interrupt jitter from that Servo ISR landing mid-byte is
     enough to corrupt what the ESP8266 sends back (or what it
     receives), which silently breaks the AT-command / CIPSEND
     handshake even though WiFi/TCP itself is fine. That's exactly why
     "same connection, same credentials" behaved differently between
     the two sketches - succy2 has no servo running.
     FIX: detach the servo right before starting a ThingSpeak
     transaction and re-attach it afterward, so Timer1 stays quiet
     during the critical serial window.

  2) TWO DIFFERENT API KEYS IN THE FILE
     const char* TS_API_KEY = "W46Q562TBG473MR7"; //  87JKK48RA4ITOD9F
     Only one of these was active; the other sat commented out. If the
     active one is wrong (or is a Read key instead of a Write key),
     the ESP still connects over TCP fine (looks "connected" on your
     router) but ThingSpeak silently rejects the write and returns "0"
     instead of an entry id - indistinguishable from a code bug unless
     you actually look at the response body. Per your instructions I
     left the currently active key untouched, but added logging of the
     raw ThingSpeak response so you can see immediately whether it's
     accepting or rejecting the write. >>> Please double check which
     of those two strings is your channel's actual Write API Key. <<<

  3) UNBOUNDED RX BUFFER / NO FAIL-FAST ON TCP ERRORS
     espRxBuffer was never trimmed while the state machine was idle,
     and ESP_CIPSTART only checked for success, never for "ERROR" /
     "CLOSED" - a failed TCP connect just silently burned the full
     4-second timeout instead of failing fast and telling you why.
     FIX: buffer is now capped, and CIPSTART failures are detected and
     logged immediately.
  =====================================================================
*/

#include <DHT.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ================= PIN DEFINITIONS =================
#define PIN_PIR         3
#define PIN_DHT         2
#define PIN_SERVO       6
#define PIN_PUMP_BASE   7
#define PIN_TRIG        8
#define PIN_ECHO        9
#define PIN_ESP_RX      10   // Arduino RX  <-- ESP8266 TX
#define PIN_ESP_TX      11   // Arduino TX  --> ESP8266 RX (via divider)
#define PIN_FLAME       A0

#define DHTTYPE DHT11

// ================= THRESHOLDS (calibrate per Part 2 of the guide) =================
int         FLAME_THRESHOLD = 400;   // below this raw value = active fire
const float DIST_MIN_CM     = 4.5;   // engagement window lower bound
const float DIST_MAX_CM     = 5.5;   // engagement window upper bound
const int   SERVO_AIM_ANGLE  = 180;
const int   SERVO_REST_ANGLE = 0;

// ================= NON-BLOCKING TIMING =================
const unsigned long SENSOR_INTERVAL     = 2000;
const unsigned long DHT_INTERVAL        = 4000;
const unsigned long THINGSPEAK_INTERVAL = 20000; // ThingSpeak free tier min ~15s

unsigned long lastSensorRead = 0;
unsigned long lastDHTRead    = 0;
unsigned long lastThingSpeak = 0;

// ================= THINGSPEAK / WIFI CONFIG (unchanged, per your request) =================
const char* WIFI_SSID  = "melon";
const char* WIFI_PASS  = "vyron123";
const char* TS_API_KEY = "W46Q562TBG473MR7"; //  87JKK48RA4ITOD9F  <-- VERIFY which is the real Write API Key
const char* TS_HOST    = "api.thingspeak.com";

const unsigned long ESP_BAUD = 115200; // must match your module's actual baud
bool wifiConnected = false;

// ================= GLOBAL STATE =================
DHT dht(PIN_DHT, DHTTYPE);
Servo pipeServo;
SoftwareSerial espSerial(PIN_ESP_RX, PIN_ESP_TX);

bool  humanPresent      = false;
int   flameRaw          = 1023;
bool  fireActive        = false;
float distanceCM        = -1;
float temperatureC      = 0;
float humidityPct       = 0;
bool  pumpOn            = false;
unsigned long fireCount = 0;
int   currentServoAngle = SERVO_REST_ANGLE;
bool  servoAttached     = false;

// ---- ESP8266 non-blocking state machine ----
enum EspState {
  ESP_IDLE,
  ESP_CIPSTART,
  ESP_SEND_CMD,
  ESP_WAIT_SEND_PROMPT,
  ESP_WAIT_RESPONSE
};
EspState espState = ESP_IDLE;
bool espBusy = false;
unsigned long espStateTimer = 0;
String espRxBuffer = "";
String espPayload = "";

const size_t ESP_RX_BUFFER_CAP = 300; // guard against unbounded growth on the Uno's 2KB SRAM

// =====================================================================
void setup() {
  Serial.begin(9600);
  espSerial.begin(ESP_BAUD);
  dht.begin();

  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_PUMP_BASE, OUTPUT);
  digitalWrite(PIN_PUMP_BASE, LOW);

  // Servo is attached on demand now (see setServoAttached()) so that its
  // Timer1 ISR isn't running while we're not actively driving it -
  // this also keeps it quiet by default during ESP telemetry windows.
  setServoAttached(true);
  pipeServo.write(SERVO_REST_ANGLE);

  Serial.println(F("=== Fire Extinguisher Robot - Step 1 Boot ==="));
  connectWiFi();
}

// =====================================================================
void loop() {
  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    checkSensors();
    handleFireLogic();
  }

  if (now - lastDHTRead >= DHT_INTERVAL) {
    lastDHTRead = now;
    readDHT();
  }

  if (now - lastThingSpeak >= THINGSPEAK_INTERVAL && !espBusy) {
    lastThingSpeak = now;
    if (wifiConnected) {
      beginThingSpeakUpdate();
    } else {
      Serial.println(F("[ESP] Skipping telemetry - WiFi not connected."));
    }
  }

  serviceEspStateMachine();
}

// =====================================================================
// SERVO HELPERS
// =====================================================================
// Detach while idle / while talking to the ESP so its Timer1 interrupt
// doesn't jitter SoftwareSerial's bit sampling at 115200 baud. Re-attach
// only when we actually need to move it.
void setServoAttached(bool attach) {
  if (attach && !servoAttached) {
    pipeServo.attach(PIN_SERVO);
    servoAttached = true;
  } else if (!attach && servoAttached) {
    pipeServo.detach();
    servoAttached = false;
  }
}

// =====================================================================
// SENSOR READING
// =====================================================================
void checkSensors() {
  humanPresent = digitalRead(PIN_PIR) == HIGH;
  flameRaw = analogRead(PIN_FLAME);
  distanceCM = readDistanceCM();

  Serial.print(F("[SENSORS] PIR="));
  Serial.print(humanPresent);
  Serial.print(F(" Flame="));
  Serial.print(flameRaw);
  Serial.print(F(" Dist="));
  Serial.print(distanceCM);
  Serial.println(F("cm"));
}

float readDistanceCM() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000UL);
  if (duration == 0) return -1;
  return duration * 0.0343 / 2.0;
}

void readDHT() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println(F("[DHT] Read failed"));
    return;
  }
  humidityPct = h;
  temperatureC = t;

  Serial.print(F("[DHT] Temp="));
  Serial.print(temperatureC);
  Serial.print(F("C Hum="));
  Serial.print(humidityPct);
  Serial.println(F("%"));
}

// =====================================================================
// FIRE / HUMAN ENGAGEMENT LOGIC
// =====================================================================
void handleFireLogic() {
  bool wasFireActive = fireActive;
  fireActive = flameRaw < FLAME_THRESHOLD;

  if (fireActive && !wasFireActive) {
    fireCount++;
    Serial.print(F("[FIRE] New fire event detected. Count="));
    Serial.println(fireCount);
  }

  bool engageCondition = humanPresent && fireActive;

  if (engageCondition) {
    // Servo aims as soon as human+fire are both true, and STAYS aimed
    // through every distance sub-case below. It only returns to rest
    // in the else branch, once engageCondition itself goes false.
    setServoAttached(true);
    if (currentServoAngle != SERVO_AIM_ANGLE) {
      Serial.println(F("[SERVO] Rotating to aim (180 deg)"));
      pipeServo.write(SERVO_AIM_ANGLE);
      currentServoAngle = SERVO_AIM_ANGLE;
    }

    Serial.println(F("[LOGIC] Human + Fire detected. Evaluating distance..."));

    if (distanceCM < 0) {
      Serial.println(F("[LOGIC] Invalid distance reading - holding pump OFF."));
      setPump(false);
    } else if (distanceCM >= DIST_MIN_CM && distanceCM <= DIST_MAX_CM) {
      Serial.println(F("ALERT!! DANGER - TAKE OFF FIRE"));
      setPump(true);
    } else if (distanceCM < DIST_MIN_CM) {
      Serial.println(F("Move backward to attain standard distance"));
      setPump(false);
    } else {
      Serial.println(F("[LOGIC] Too far from target range - pump OFF."));
      setPump(false);
    }
  } else {
    setPump(false);
    if (currentServoAngle != SERVO_REST_ANGLE) {
      pipeServo.write(SERVO_REST_ANGLE);
      currentServoAngle = SERVO_REST_ANGLE;
      Serial.println(F("[SERVO] Conditions false - returning to rest position"));
    }
    // Nothing engaged and servo is back at rest - free up Timer1 so it
    // isn't fighting SoftwareSerial during the next ThingSpeak send.
    if (!espBusy) {
      setServoAttached(false);
    }
  }
}

void setPump(bool on) {
  if (pumpOn == on) return;
  pumpOn = on;
  digitalWrite(PIN_PUMP_BASE, on ? HIGH : LOW);
  Serial.print(F("[PUMP] "));
  Serial.println(on ? F("ON") : F("OFF"));
}

// =====================================================================
// WIFI / THINGSPEAK (ESP8266 AT firmware)
// =====================================================================
void connectWiFi() {
  Serial.println(F("[ESP] Initializing..."));

  sendATSetupCommand("AT+RST", "ready", 3000);
  delay(2000);
  sendATSetupCommand("AT+CWMODE=1", "OK", 1000);
  sendATSetupCommand("AT+CIPMUX=0", "OK", 1000); // single-connection mode, required for CIPSTART without a link ID

  String cmd = String("AT+CWJAP=\"") + WIFI_SSID + "\",\"" + WIFI_PASS + "\"";
  wifiConnected = sendATSetupCommand(cmd.c_str(), "OK", 12000);

  if (wifiConnected) {
    Serial.println(F(">>> WiFi CONNECTED <<<"));
    sendATSetupCommand("AT+CIFSR", "OK", 2000); // confirm IP printed above isn't 0.0.0.0
  } else {
    Serial.println(F(">>> WiFi CONNECTION FAILED - check SSID/PASS and ESP_BAUD <<<"));
  }
}

bool sendATSetupCommand(const char* cmd, const char* expected, unsigned long timeout) {
  espSerial.println(cmd);
  Serial.print(F("[ESP][TX] "));
  Serial.println(cmd);

  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (espSerial.available()) {
      char c = espSerial.read();
      Serial.write(c);
      response += c;
      if (response.indexOf(expected) != -1) {
        delay(20);
        while (espSerial.available()) Serial.write(espSerial.read());
        return true;
      }
    }
  }
  return false;
}

// Kicks off a non-blocking telemetry send.
void beginThingSpeakUpdate() {
  if (espState != ESP_IDLE) return;

  // Make sure Timer1 is free of the Servo ISR for the entire transaction -
  // this is the fix for the corrupted-response bug described at the top
  // of this file. We restore the servo's last commanded position when the
  // transaction finishes (see ESP_WAIT_RESPONSE below).
  setServoAttached(false);

  espBusy = true;
  espRxBuffer = "";

  espPayload = String("GET /update?api_key=") + TS_API_KEY +
               "&field1=" + String(temperatureC) +
               "&field2=" + String(fireCount) +
               "&field3=" + String(distanceCM) +
               "&field4=" + String(pumpOn ? 1 : 0) +
               "&field5=" + String(flameRaw) +
               " HTTP/1.1\r\n" +
               "Host: " + TS_HOST + "\r\n" +
               "Connection: close\r\n\r\n";

  String startCmd = String("AT+CIPSTART=\"TCP\",\"") + TS_HOST + "\",80";
  Serial.print(F("[ESP][TX] "));
  Serial.println(startCmd);
  espSerial.println(startCmd);

  espState = ESP_CIPSTART;
  espStateTimer = millis();
}

void pollEspSerial() {
  while (espSerial.available()) {
    char c = espSerial.read();
    espRxBuffer += c;
    Serial.write(c);
  }
  // Guard against unbounded growth (e.g. unsolicited ESP chatter while
  // ESP_IDLE) eating into the Uno's 2KB of SRAM over a long uptime.
  if (espState == ESP_IDLE && espRxBuffer.length() > 0) {
    espRxBuffer = "";
  } else if (espRxBuffer.length() > ESP_RX_BUFFER_CAP) {
    espRxBuffer = espRxBuffer.substring(espRxBuffer.length() - ESP_RX_BUFFER_CAP);
  }
}

void serviceEspStateMachine() {
  pollEspSerial();

  switch (espState) {
    case ESP_IDLE:
      break;

    case ESP_CIPSTART:
      if (espRxBuffer.indexOf("OK") != -1 || espRxBuffer.indexOf("ALREADY") != -1) {
        espRxBuffer = "";
        String sendCmd = String("AT+CIPSEND=") + espPayload.length();
        Serial.print(F("[ESP][TX] "));
        Serial.println(sendCmd);
        espSerial.println(sendCmd);
        espState = ESP_WAIT_SEND_PROMPT;
        espStateTimer = millis();
      } else if (espRxBuffer.indexOf("ERROR") != -1 || espRxBuffer.indexOf("CLOSED") != -1) {
        // Fail fast instead of burning the full timeout when the module
        // has already told us the connect failed.
        Serial.println(F("[ESP] CIPSTART returned ERROR/CLOSED - aborting this telemetry cycle."));
        espRxBuffer = "";
        espState = ESP_IDLE;
        espBusy = false;
        restoreServoAfterEspTransaction();
      } else if (millis() - espStateTimer > 4000) {
        Serial.println(F("[ESP] CIPSTART timeout - aborting this telemetry cycle."));
        espRxBuffer = "";
        espState = ESP_IDLE;
        espBusy = false;
        restoreServoAfterEspTransaction();
      }
      break;

    case ESP_WAIT_SEND_PROMPT:
      if (espRxBuffer.indexOf(">") != -1) {
        Serial.print(F("[ESP][TX] "));
        Serial.println(espPayload);
        espSerial.print(espPayload);
        espRxBuffer = "";
        espState = ESP_WAIT_RESPONSE;
        espStateTimer = millis();
      } else if (millis() - espStateTimer > 3000) {
        Serial.println(F("[ESP] Send-prompt timeout - aborting this telemetry cycle."));
        espState = ESP_IDLE;
        espBusy = false;
        restoreServoAfterEspTransaction();
      }
      break;

    case ESP_WAIT_RESPONSE:
      if (millis() - espStateTimer > 3000) {
        if (espRxBuffer.indexOf("200 OK") != -1) {
          Serial.println(F("[ESP] >>> ThingSpeak confirmed 200 OK <<<"));
        } else {
          Serial.println(F("[ESP] >>> No 200 OK - response was: <<<"));
          Serial.println(espRxBuffer);
        }
        // Diagnostic for bug #2 above: ThingSpeak's HTTP body is the entry
        // id (e.g. "1234") on success, or "0" if the write was rejected
        // (most commonly a wrong/expired API key). Watch this line.
        Serial.print(F("[ESP] ThingSpeak body suggests: "));
        if (espRxBuffer.indexOf("\r\n\r\n0") != -1 || espRxBuffer.endsWith("0")) {
          Serial.println(F("write REJECTED (check TS_API_KEY - see header comment)."));
        } else {
          Serial.println(F("write likely accepted."));
        }

        espSerial.println("AT+CIPCLOSE");
        espRxBuffer = "";
        espState = ESP_IDLE;
        espBusy = false;
        restoreServoAfterEspTransaction();
      }
      break;

    default:
      espState = ESP_IDLE;
      espBusy = false;
      restoreServoAfterEspTransaction();
      break;
  }
}

// Re-attach the servo (if the current fire/human logic still wants it
// aimed) now that the ESP transaction is done and Timer1 is safe to use
// again.
void restoreServoAfterEspTransaction() {
  if (currentServoAngle == SERVO_AIM_ANGLE) {
    setServoAttached(true);
    pipeServo.write(SERVO_AIM_ANGLE);
  }
  // else: leave it detached at rest until handleFireLogic() needs it again.
}
