#include <WiFi.h>

// Must match the Server's AP credentials
const char* AP_SSID     = "ESP32_ChatNet";
const char* AP_PASSWORD = "chat12345";

// Server's IP is fixed when using softAP — this is the default gateway IP
IPAddress serverIP(192, 168, 4, 1);
const uint16_t PORT = 8888;

WiFiClient client;
String inputBuffer = "";

void connectToServer() {
  Serial.print("Connecting to server socket");
  while (!client.connect(serverIP, PORT)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n>>> Connected to server! Start chatting. <<<\n");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("=== ESP32 CHAT CLIENT ===");
  Serial.print("Connecting to WiFi: ");
  Serial.println(AP_SSID);

  WiFi.begin(AP_SSID, AP_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("My IP: ");
  Serial.println(WiFi.localIP());

  connectToServer();
}

void loop() {
  // Reconnect if socket drops
  if (!client.connected()) {
    Serial.println("\n[Disconnected from server, retrying...]");
    delay(1000);
    connectToServer();
  }

  // --- Receive: incoming data from server -> print to Serial ---
  if (client.available()) {
    String msg = client.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      Serial.print("Server: ");
      Serial.println(msg);
    }
  }

  // --- Send: Serial input -> send to server ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        if (client.connected()) {
          client.print(inputBuffer);
          client.print("\n");
          Serial.print("Me: ");
          Serial.println(inputBuffer);
        }
      }
      inputBuffer = "";
    } else if (c != '\r') {
      inputBuffer += c;
    }
  }
}


