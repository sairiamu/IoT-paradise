#include <Arduino.h>
#include <SoftwareSerial.h>
#include <WiFiEspAT.h>
#include <ThingSpeak.h>
#include <stdlib.h>
// #include "secrets.h"

#define WIFI_SSID "melon"
#define WIFI_PASSWORD "vyron123"

#define THINGSPEAK_CHANNEL_ID 3473351UL
#define THINGSPEAK_WRITE_API_KEY "VKV9HG25BQ1URME3"

#define GATEWAY_IP_STR "192.168.137.212"
#define NODE_A_IP_STR "192.168.137.157"
#define NODE_B_IP_STR "192.168.1.52"

#define GATEWAY_PORT 9000
#define NODE_A_PORT 9001
#define NODE_B_PORT 9002

static const uint8_t ESP_RX_PIN = 8; // Uno receives from ESP-01 TX
static const uint8_t ESP_TX_PIN = 7; // Uno transmits to ESP-01 RX (level shift required)

SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);
WiFiServer gatewayServer(GATEWAY_PORT);
WiFiClient tsClient;

float aTemp = NAN;
float aHum = NAN;
float bTemp = NAN;
float bHum = NAN;

// Network addresses
IPAddress gatewayIp;

// Subnet configuration matching your Windows Hotspot
IPAddress routerGateway(192, 168, 137, 1); 
IPAddress subnetMask(255, 255, 255, 0);

bool parseNodePacket(const String &line, char &node, float &t, float &h) {
  char packet[24];
  line.toCharArray(packet, sizeof(packet));

  char *nodeToken = strtok(packet, ",");
  char *temp = strtok(nullptr, ",");
  char *hum = strtok(nullptr, ",");
  if (!nodeToken || !temp || !hum || nodeToken[1] != '\0') return false;

  node = nodeToken[0];
  if (node != 'A' && node != 'B') return false;

  t = atof(temp);
  h = atof(hum);
  return true;
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Gateway connecting WiFi");
  
  // Set the static IP configuration before connecting
  WiFi.config(gatewayIp, routerGateway, subnetMask);

  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (uint8_t i = 0; i < 10; i++) {
      if (WiFi.status() == WL_CONNECTED) break;
      Serial.print('.');
      delay(500);
    }
  }
  Serial.println(" connected");
  
  // Print the IP to verify it worked
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void handleIncomingNodePackets() {
  WiFiClient incoming = gatewayServer.accept();
  if (!incoming) return;

  incoming.setTimeout(100);
  String line = incoming.readStringUntil('\n');
  line.trim();

  char node;
  float t, h;
  if (parseNodePacket(line, node, t, h)) {
    if (node == 'A') {
      aTemp = t;
      aHum = h;
    } else {
      bTemp = t;
      bHum = h;
    }

    Serial.print("RX ");
    Serial.print(node);
    Serial.print(": ");
    Serial.println(line);
  }

  incoming.stop();
}

void sendToThingSpeak() {
  if (!isnan(aTemp)) ThingSpeak.setField(1, aTemp);
  if (!isnan(aHum)) ThingSpeak.setField(2, aHum);
  if (!isnan(bTemp)) ThingSpeak.setField(3, bTemp);
  if (!isnan(bHum)) ThingSpeak.setField(4, bHum);

  int status = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITE_API_KEY);
  Serial.print("ThingSpeak status: ");
  Serial.println(status);
}

void setup() {
  Serial.begin(115200);
  espSerial.begin(115200);
  WiFi.init(espSerial);

  // Parse string into the gatewayIp object
  gatewayIp.fromString(GATEWAY_IP_STR);

  ensureWiFi();
  gatewayServer.begin();
  ThingSpeak.begin(tsClient);
}

void loop() {
  ensureWiFi();
  handleIncomingNodePackets();

  static uint32_t lastUploadMs = 0;
  if (millis() - lastUploadMs >= 20000UL) {
    lastUploadMs = millis();
    sendToThingSpeak();
  }
}
