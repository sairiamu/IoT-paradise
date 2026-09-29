/*
  Challenge 21: IoT Temperature Monitor
  Hardware: Arduino Uno + DHT11 + ESP-01 (ESP8266) as WiFi bridge
  Sends temperature & humidity to ThingSpeak on an interval,
  and prints the same readings locally so you can compare
  Serial Monitor values against the ThingSpeak dashboard.
*/

#include <SoftwareSerial.h>
#include <WiFiEsp.h>
#include <DHT.h>
#include "ThingSpeak.h"

// ---------- USER SETTINGS - EDIT THESE ----------
char ssid[] = "Evansi's A54";
char pass[] = "Mwisa123";

unsigned long channelID = 3459358;              // your ThingSpeak Channel ID
const char * writeAPIKey = "3LXPSTH773AR5O0M"; // from the API Keys tab

const unsigned long SEND_INTERVAL_MS = 20000;    // ThingSpeak free tier needs >=15s between updates
// -------------------------------------------------

#define DHTPIN 7
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ESP-01 connected via SoftwareSerial: Uno RX=2 (from ESP TX), Uno TX=3 (to ESP RX via divider)
SoftwareSerial espSerial(2, 3);

WiFiEspClient client;
unsigned long lastSend = 0;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);

  Serial.println(F("Booting IoT Temperature Monitor..."));

  dht.begin();

  WiFi.init(&espSerial);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("ESP8266 not detected. Check wiring."));
    while (true) { delay(1000); }
  }

  Serial.print(F("Connecting to WiFi: "));
  Serial.println(ssid);
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
    delay(2000);
  }
  Serial.println(F("WiFi connected."));

  ThingSpeak.begin(client);
}

void loop() {
  unsigned long now = millis();

  if (now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;

    float humidity = dht.readHumidity();
    float tempC = dht.readTemperature();

    if (isnan(humidity) || isnan(tempC)) {
      Serial.println(F("Sensor read failed - skipping this cycle."));
      return;
    }

    // ---- LOCAL READING (compare this against the ThingSpeak dashboard) ----
    Serial.print(F("Local reading  -> Temp: "));
    Serial.print(tempC);
    Serial.print(F(" C, Humidity: "));
    Serial.print(humidity);
    Serial.println(F(" %"));

    // ---- SEND TO THINGSPEAK ----
    ThingSpeak.setField(1, tempC);
    ThingSpeak.setField(2, humidity);

    int result = ThingSpeak.writeFields(channelID, writeAPIKey);

    if (result == 200) {
      Serial.println(F("Sent to ThingSpeak successfully."));
    } else {
      Serial.print(F("ThingSpeak send failed. HTTP error code: "));
      Serial.println(result);
    }
  }
}
