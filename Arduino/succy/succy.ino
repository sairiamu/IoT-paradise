#include <SoftwareSerial.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

SoftwareSerial esp8266(10, 11); // RX, TX

const String ssid = "melon";
const String password = "vyron123";
const String apiKey = "JH5AALDPVGUSKSJ6";
const String host = "api.thingspeak.com";

void setup() {
  Serial.begin(115200);
  esp8266.begin(115200); // must match your ESP8266's configured AT baud rate

  dht.begin();

  sendCommand("AT+RST", 2000);
  delay(2000);
  sendCommand("AT+CWMODE=1", 1000);
  sendCommand("AT+CWJAP=\"" + ssid + "\",\"" + password + "\"", 8000);
  delay(2000);
  sendCommand("AT+CIFSR", 1000); // prints assigned IP, useful for debugging
}

void loop() {
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

  sendCommand("AT+CIPSTART=\"TCP\",\"" + host + "\",80", 3000);

  String cmd = "AT+CIPSEND=" + String(httpRequest.length());
  sendCommand(cmd, 2000);

  esp8266.print(httpRequest); // print, not println — we control every \r\n ourselves
  delay(3000);

  if (esp8266.find("SEND OK")) {
    Serial.println("Data sent to ThingSpeak");
  }

  readEspResponse(3000); // watch here for "200 OK" from ThingSpeak
  sendCommand("AT+CIPCLOSE", 1000);

  delay(20000); // ThingSpeak free tier: minimum 15s between updates
}

void sendCommand(String command, int timeout) {
  esp8266.println(command);
  readEspResponse(timeout);
}

void readEspResponse(int timeout) {
  long int time = millis();
  while ((time + timeout) > millis()) {
    while (esp8266.available()) {
      char c = esp8266.read();
      Serial.write(c);
    }
  }
}