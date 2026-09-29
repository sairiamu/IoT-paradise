

#include <WiFi.h>

// Credentials for the AP this ESP32 will broadcast
const char* AP_SSID     = "ESP32_ChatNet";
const char* AP_PASSWORD = "chat12345";   // min 8 chars, or "" for open network

const uint16_t PORT = 8888;

WiFiServer server(PORT);
WiFiClient client;

String inputBuffer = "";

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();

  Serial.println("=== ESP32 CHAT SERVER ===");
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Server IP: ");
  Serial.println(ip);
  Serial.println("Waiting for client to connect...");

  server.begin();
}

void loop() {
  // Accept a new client if we don't already have one connected
  if (!client || !client.connected()) {
    WiFiClient newClient = server.available();
    if (newClient) {
      client = newClient;
      Serial.println("\n>>> Client connected! Start chatting. <<<\n");
    }
  }

  // --- Receive: incoming data from client -> print to Serial ---
  if (client && client.connected() && client.available()) {
    String msg = client.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      Serial.print("Client: ");
      Serial.println(msg);
    }
  }

  // --- Send: Serial input -> send to client ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        if (client && client.connected()) {
          client.print(inputBuffer);
          client.print("\n");
          Serial.print("Me: ");
          Serial.println(inputBuffer);
        } else {
          Serial.println("(No client connected yet)");
        }
      }
      inputBuffer = "";
    } else if (c != '\r') {
      inputBuffer += c;
    }
  }
}


