/*
  ESP32 Slave - Ultrasonic Sensor
  --------------------------------
  Reads distance from an HC-SR04 ultrasonic sensor and sends
  readings to the Raspberry Pi server over WiFi via HTTP POST.

  Data format:
    date:day:timestamp:distance(cm):status

  Status:
    FAR  -> distance > 15 cm
    NEAR -> distance <= 15 cm

  Wiring (HC-SR04):
    VCC  -> 5V
    GND  -> GND
    TRIG -> ESP32 GPIO 5
    ECHO -> ESP32 GPIO 18

  IMPORTANT:
    HC-SR04 ECHO outputs 5V.
    ESP32 GPIOs are 3.3V logic, so use a voltage divider
    on ECHO before connecting it to GPIO 18.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

const char* ssid     = "Blizzie";
const char* password = "brk7thegreat";

// Raspberry Pi server
const char* serverUrl = "http://10.239.156.187:8000/data";

const char* deviceName = "esp32_ultrasonic";

// Ultrasonic pins
const int trigPin = 5;
const int echoPin = 18;

// Distance threshold
const float distanceThreshold = 15.0;

// Send every 2 seconds
unsigned long lastSend = 0;
const unsigned long sendInterval = 2000;

// Tanzania timezone: UTC+3
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;


// --------------------------------------------------
// WiFi
// --------------------------------------------------

void connectWiFi() {

  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected!");
  Serial.println("ESP32 IP: " + WiFi.localIP().toString());
}


// --------------------------------------------------
// Get date/time
// --------------------------------------------------

String getTimestamp() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "0000-00-00T00:00:00";
  }

  char timestamp[30];

  strftime(
    timestamp,
    sizeof(timestamp),
    "%Y-%m-%dT%H:%M:%S",
    &timeinfo
  );

  return String(timestamp);
}


// --------------------------------------------------
// Measure distance
// --------------------------------------------------

float getDistance() {

  // Make sure trigger starts LOW
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Send 10 microsecond pulse
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure echo duration
  long duration = pulseIn(echoPin, HIGH, 30000);

  // No echo received
  if (duration == 0) {
    return -1;
  }

  // Speed of sound:
  // distance = duration * 0.0343 / 2
  float distance = duration * 0.0343 / 2.0;

  return distance;
}


// --------------------------------------------------
// Send reading to Raspberry Pi
// --------------------------------------------------

void sendReading(float distance, String status, String timestamp) {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  HTTPClient http;

  http.begin(serverUrl);

  http.addHeader("Content-Type", "application/json");

  String jsonPayload =
    "{"
      "\"device\":\"" + String(deviceName) + "\","
      "\"sensor_type\":\"ultrasonic\","
      "\"date\":\"" + timestamp.substring(0, 10) + "\","
      "\"day\":\"" + timestamp.substring(0, 10) + "\","
      "\"timestamp\":\"" + timestamp + "\","
      "\"distance_cm\":" + String(distance, 2) + ","
      "\"status\":\"" + status + "\""
    "}";

  Serial.println("Sending:");
  Serial.println(jsonPayload);

  int httpCode = http.POST(jsonPayload);

  if (httpCode > 0) {

    Serial.printf(
      "POST sent, response code: %d\n",
      httpCode
    );

    String response = http.getString();
    Serial.println("Server response:");
    Serial.println(response);

  } else {

    Serial.printf(
      "POST failed: %s\n",
      http.errorToString(httpCode).c_str()
    );
  }

  http.end();
}


// --------------------------------------------------
// Setup
// --------------------------------------------------

void setup() {

  Serial.begin(115200);

  // Ultrasonic pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // WiFi
  connectWiFi();

  // Configure time
  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );

  Serial.println("Time synchronization started.");
}


// --------------------------------------------------
// Loop
// --------------------------------------------------

void loop() {

  if (millis() - lastSend >= sendInterval) {

    lastSend = millis();

    // Measure distance
    float distance = getDistance();

    // Sensor timeout / invalid reading
    if (distance < 0) {

      Serial.println("Ultrasonic: No echo received");

      return;
    }

    // Determine status
    String status;

    if (distance > distanceThreshold) {
      status = "FAR";
    } else {
      status = "NEAR";
    }

    // Get current timestamp
    String timestamp = getTimestamp();

    // Print
    Serial.printf(
      "Date: %s | Distance: %.2f cm | Status: %s\n",
      timestamp.c_str(),
      distance,
      status.c_str()
    );

    // Send to Raspberry Pi
    sendReading(
      distance,
      status,
      timestamp
    );
  }
}




