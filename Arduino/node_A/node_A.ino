#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "GroupA_Network";
const char* password = "12345678"; // must match Group A exactly
const char* serverIP = "192.168.4.1"; // Group A's AP IP (confirm from its Serial Monitor)

int counter = 0;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Group A");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String message = "Hello_" + String(counter);
    String url = "http://" + String(serverIP) + "/message?msg=" + message;

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("Server replied: " + response);
    } else {
      Serial.println("Failed to send, error code: " + String(httpCode));
    }

    http.end();
    counter++;
  }

  delay(3000); // send every 3 seconds
}



