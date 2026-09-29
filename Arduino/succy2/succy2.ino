#include <SoftwareSerial.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

SoftwareSerial esp8266(7,8); // RX, TX

const String ssid = "melon";
const String password = "vyron123";
const String apiKey = "V6Z7MK1F0D3TYKNU"; //CO2QHSMCEF7ZJIMH 
const String host = "api.thingspeak.com";

bool wifiConnected = false;

void setup() {
  Serial.begin(115200);
  esp8266.begin(115200); // confirmed working baud for your module

  dht.begin();

  Serial.println("=== Booting ===");
  sendCommand("AT+RST", "ready", 3000);
  delay(2000);
  sendCommand("AT+CWMODE=1", "OK", 1000);

  Serial.println("Connecting to WiFi: " + ssid);
  wifiConnected = sendCommand("AT+CWJAP=\"" + ssid + "\",\"" + password + "\"", "OK", 10000);

  if (wifiConnected) {
    Serial.println(">>> WiFi CONNECTED <<<");
    sendCommand("AT+CIFSR", "OK", 2000); // check the IP printed here is NOT 0.0.0.0
  } else {
    Serial.println(">>> WiFi CONNECTION FAILED <<<");
  }
}

void loop() {
  if (!wifiConnected) {
    wifiConnected = sendCommand("AT+CWJAP=\"" + ssid + "\",\"" + password + "\"", "OK", 10000);
    if (!wifiConnected) { delay(5000); return; }
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(2000);
    return;
  }
  Serial.println("Temp: " + String(t) + "C  Humidity: " + String(h) + "%");

  String httpRequest = "GET /update?api_key=" + apiKey +
                        "&field1=" + String(t) +
                        "&field2=" + String(h) +
                        " HTTP/1.1\r\n" +
                        "Host: " + host + "\r\n" +
                        "Connection: close\r\n\r\n";

  bool started = sendCommand("AT+CIPSTART=\"TCP\",\"" + host + "\",80", "OK", 5000);
  if (!started) {
    Serial.println(">>> TCP connect FAILED — likely no internet route from this network <<<");
    sendCommand("AT+CIPCLOSE", "OK", 1000);
    delay(20000);
    return;
  }

  esp8266.println("AT+CIPSEND=" + String(httpRequest.length()));
  if (!waitForResponse(">", 3000)) {
    Serial.println(">>> No '>' prompt, CIPSEND failed <<<");
    sendCommand("AT+CIPCLOSE", "OK", 1000);
    delay(20000);
    return;
  }

  esp8266.print(httpRequest);

  if (waitForResponse("SEND OK", 3000)) {
    Serial.println(">>> Bytes sent to ESP <<<");
  } else {
    Serial.println(">>> SEND FAILED <<<");
  }

  String response = readEspResponse(3000);
  if (response.indexOf("200 OK") >= 0) {
    Serial.println(">>> ThingSpeak confirmed 200 OK <<<");
  } else {
    Serial.println(">>> No 200 OK — check response above for the actual error <<<");
  }

  sendCommand("AT+CIPCLOSE", "OK", 1000);
  delay(20000);
}

bool sendCommand(String command, String expected, int timeout) {
  esp8266.println(command);
  return waitForResponse(expected, timeout);
}

bool waitForResponse(String expected, int timeout) {
  String response = "";
  long start = millis();
  while (millis() - start < timeout) {
    while (esp8266.available()) {
      char c = esp8266.read();
      Serial.write(c);
      response += c;
      if (response.indexOf(expected) != -1) {
        delay(20);
        while (esp8266.available()) Serial.write(esp8266.read());
        return true;
      }
    }
  }
  return false;
}

String readEspResponse(int timeout) {
  String response = "";
  long start = millis();
  while (millis() - start < timeout) {
    while (esp8266.available()) {
      char c = esp8266.read();
      Serial.write(c);
      response += c;
    }
  }
  return response;
}

