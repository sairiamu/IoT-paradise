/*
  MASTER — ESP32 acting as:
   - WiFi Station (connects to home WiFi for internet -> ThingSpeak)
   - WiFi Access Point (DHT11 clients connect here to send data)
  Receives sensor data over HTTP, prints to Serial, forwards to ThingSpeak.

  All fixes applied:
   - WiFi.setSleep(false) — prevents power-save from silently dropping traffic
   - STA brought up BEFORE AP — avoids ESP32 defaulting route to the AP side
   - Explicit DNS servers set after STA connects
   - Gateway reachability test — isolates local-network vs internet issues
   - Internet connectivity test before first ThingSpeak post
   - WiFi auto-reconnect handling in the main loop
*/

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// ---------- CONFIG ----------
const char* AP_SSID     = "SensorNet";
const char* AP_PASSWORD = "sensor1234";   // min 8 chars

// const char* HOME_SSID     = "FretNET5G";
// const char* HOME_PASSWORD = "fretnet1234.";

const char* HOME_SSID     = "melon";
const char* HOME_PASSWORD = "vyron123";

// const char* HOME_SSID     = "Fey"
// const char* HOME_PASSWORD = "987654321";

const char* TS_API_KEY = "5J4MT9R7GVYWAIPK";
const unsigned long POST_INTERVAL = 20000; // ms, keep >=15000

#define MAX_CLIENTS 4
const char* knownClientIds[MAX_CLIENTS] = { "client1", "client2", "client3", "client4" };
// -----------------------------

WebServer server(80);

struct ClientReading {
  bool hasData = false;
  float temperature = 0;
  float humidity = 0;
  unsigned long lastSeen = 0;
};
ClientReading readings[MAX_CLIENTS];
unsigned long lastPost = 0;

int slotForId(const String& id) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (id == knownClientIds[i]) return i;
  }
  return -1;
}

void handleUpdate() {
  if (!server.hasArg("id") || !server.hasArg("temp") || !server.hasArg("hum")) {
    server.send(400, "text/plain", "Missing id/temp/hum");
    return;
  }

  String id = server.arg("id");
  float temp = server.arg("temp").toFloat();
  float hum  = server.arg("hum").toFloat();

  Serial.printf("Received <- id=%s temp=%.1fC hum=%.1f%%\n", id.c_str(), temp, hum);

  int slot = slotForId(id);
  if (slot == -1) {
    Serial.printf("  (unknown client id '%s' — add it to knownClientIds[])\n", id.c_str());
    server.send(200, "text/plain", "id not recognized, ignored");
    return;
  }

  readings[slot].hasData = true;
  readings[slot].temperature = temp;
  readings[slot].humidity = hum;
  readings[slot].lastSeen = millis();

  server.send(200, "text/plain", "OK");
}

bool testGatewayConnectivity() {
  IPAddress gw = WiFi.gatewayIP();
  Serial.print("Testing local gateway ");
  Serial.print(gw);
  Serial.print(" ... ");

  WiFiClient client;
  client.setTimeout(5000);
  if (client.connect(gw, 80)) {
    Serial.println("reachable.");
    client.stop();
    return true;
  } else {
    Serial.println("NOT reachable (router may be isolating this device).");
    return false;
  }
}

bool testInternetConnectivity() {
  Serial.print("Testing internet (example.com) ... ");
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.begin("http://example.com");
  int code = http.GET();
  http.end();

  if (code > 0) {
    Serial.printf("OK (HTTP %d)\n", code);
    return true;
  } else {
    Serial.printf("FAILED (%d)\n", code);
    return false;
  }
}

void connectHomeWiFi() {
  WiFi.begin(HOME_SSID, HOME_PASSWORD);
  Serial.print("Connecting to home WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println(" timeout, retrying...");
      WiFi.disconnect();
      WiFi.begin(HOME_SSID, HOME_PASSWORD);
      start = millis();
    }
  }
  Serial.println(" connected!");
  Serial.print("Master internet IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  // Explicit DNS (Google + Cloudflare) — helps on routers with flaky DNS relay
  IPAddress localIP = WiFi.localIP();
  IPAddress gateway  = WiFi.gatewayIP();
  IPAddress subnet   = WiFi.subnetMask();
  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(localIP, gateway, subnet, dns1, dns2);
  delay(200);

  testGatewayConnectivity();
  testInternetConnectivity();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Prevents ESP32 power-save from silently dropping WiFi traffic in AP+STA mode
  WiFi.setSleep(false);

  WiFi.mode(WIFI_AP_STA);

  // STA must come up before AP to avoid routing issues on some ESP32 cores
  connectHomeWiFi();

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP started. Clients connect to SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Master AP IP (clients POST here): ");
  Serial.println(WiFi.softAPIP());

  server.on("/update", HTTP_GET, handleUpdate);
  server.on("/update", HTTP_POST, handleUpdate);
  server.begin();
  Serial.println("HTTP server started, waiting for clients...");
}

void postToThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi dropped, reconnecting...");
    connectHomeWiFi();
    return;
  }

  String url = "http://api.thingspeak.com/update?api_key=" + String(TS_API_KEY);
  bool any = false;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!readings[i].hasData) continue;
    int fieldTemp = i * 2 + 1;
    int fieldHum  = i * 2 + 2;
    url += "&field" + String(fieldTemp) + "=" + String(readings[i].temperature, 1);
    url += "&field" + String(fieldHum)  + "=" + String(readings[i].humidity, 1);
    any = true;
  }

  if (!any) {
    Serial.println("No client data yet, nothing to post");
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(8000);
  http.begin(url);
  int code = http.GET();

  if (code == 200) {
    String body = http.getString();
    Serial.printf("ThingSpeak post OK -> entry #%s\n", body.c_str());
  } else {
    Serial.printf("ThingSpeak post -> HTTP %d\n", code);
    if (code > 0) Serial.println(http.getString());
  }

  http.end();
}

void loop() {
  server.handleClient();

  // Keep home WiFi alive
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectHomeWiFi();
  }

  if (millis() - lastPost >= POST_INTERVAL) {
    postToThingSpeak();
    lastPost = millis();
  }
}

