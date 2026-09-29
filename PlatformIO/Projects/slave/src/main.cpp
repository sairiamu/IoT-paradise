
/*
  ESP32 Slave - Water Sensor
  ---------------------------
  Reads an analog water/rain sensor (e.g. FC-37, YL-83 module) and sends
  readings to the Raspberry Pi server over WiFi via HTTP POST.

  Wiring:
    Water sensor AO  -> ESP32 GPIO 34 (ADC1 channel, input-only pin)
    Water sensor VCC -> 3.3V
    Water sensor GND -> GND

  (Water sensors output a variable analog voltage: dry ~ high reading (near
   4095), fully wet ~ low reading (near 0), depending on the module.
   Adjust wetThreshold below after testing your own sensor in air vs water.)

  Before uploading:
    - Install "ESP32" board support in Arduino IDE (Boards Manager)
    - Select your ESP32 board + correct COM port
    - Update ssid, password, and serverUrl below
*/

#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid     = "Blizzie";
const char* password = "brk7thegreat";

// Replace with your Raspberry Pi's IP address (run hostname -I on the Pi)
const char* serverUrl = "http://10.239.156.187:8000/data";

const int waterPin = 34; // must be an ADC1 pin (32-39) if WiFi is used
const char* deviceName = "esp32_water";

const int wetThreshold = 2500; // tune this: below = wet, above = dry

unsigned long lastSend = 0;
const unsigned long sendInterval = 2000; // ms between readings

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  pinMode(waterPin, INPUT);
  connectWiFi();
}

void sendReading(int value, bool wet) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{\"device\":\"" + String(deviceName) +
                        "\",\"sensor_type\":\"water\",\"value\":" + String(value) +
                        ",\"wet\":" + (wet ? "true" : "false") + "}";

  int httpCode = http.POST(jsonPayload);
  if (httpCode > 0) {
    Serial.printf("POST sent, response code: %d\n", httpCode);
  } else {
    Serial.printf("POST failed: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void loop() {
  if (millis() - lastSend >= sendInterval) {
    lastSend = millis();
    int level = analogRead(waterPin);      // raw ADC value, 0-4095
    bool wet = level < wetThreshold;       // lower reading = more water
    Serial.printf("Water level: %d (%s)\n", level, wet ? "WET" : "dry");
    sendReading(level, wet);
  }
}

