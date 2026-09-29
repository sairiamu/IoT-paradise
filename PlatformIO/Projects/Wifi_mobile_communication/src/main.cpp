#include <Arduino.h>
#include <WiFi.h>
#include "FS.h"
#include "LittleFS.h"

const char* ssid = "sair";
const char* password = "00000000";

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Anzisha LittleFS ya kuhifadhi mafaili kwenye ESP32 flash memory
  if (!LittleFS.begin(true)) {
    Serial.println("Hitilafu katika kuanzisha LittleFS!");
    return;
  }
  Serial.println("LittleFS imewashwa kwa mafanikio.");

  Serial.println("\nInaanza kutengeneza Wi-Fi Hotspot...");
  bool success = WiFi.softAP(ssid, password);

  if (success) {
    Serial.println("Wi-Fi Hotspot imeanzishwa SUCCESSFULLY!");
    IPAddress IP = WiFi.softAPIP();

    Serial.print("Wi-Fi Name (SSID): ");
    Serial.println(ssid);
    Serial.print("Nenosiri: ");
    Serial.println(password);
    Serial.print("Anwani ya Web Server: http://");
    Serial.println(IP);
    Serial.println("\n--- MAELEZO YA QR CODE ---");
    Serial.println("Kwa sababu ESP32 haina internet, unaweza kutumia jenereta ya QR ya simu yako");
    Serial.println("au kuandika link hii moja kwa moja kwenye kivinjari: http://192.168.4.1");
    Serial.println("----------------------------------------\n");

    server.begin();
  } else {
    Serial.println("ERROR: Wi-Fi Hotspot haijaanzishwa.");
  }
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    String currentLine = "";
    String requestLine = "";
    bool isPost = false;
    boolean currentLineIsBlank = true;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        requestLine += c;

        if (c == '\n' && currentLineIsBlank) {
          // Soma HTTP Request Header
          if (requestLine.startsWith("GET /?text=")) {
            int startIndex = requestLine.indexOf("?text=") + 6;
            int endIndex = requestLine.indexOf(" ", startIndex);
            String message = requestLine.substring(startIndex, endIndex);
            message.replace("+", " ");
            
            Serial.print("[UJUMBE MPYA]: ");
            Serial.println(message);
          }

          // Tuma Kurasa ya HTML yenye fomu ya Ujumbe na Kupakia Faili
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();
          
          client.println("<!DOCTYPE html><html>");
          client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
          client.println("<title>ESP32 File & Message Hub</title></head>");
          client.println("<body style='font-family: Arial; text-align: center; margin-top: 30px; background: #f4f4f9;'>");
          client.println("<div style='background: white; padding: 20px; border-radius: 8px; max-width: 400px; margin: auto; box-shadow: 0 0 10px rgba(0,0,0,0.1);'>");
          client.println("<h2>ESP32 Local Hub</h2>");
          
          // Fomu ya Ujumbe (Text)
          client.println("<h3>Tuma Ujumbe</h3>");
          client.println("<form action='/' method='GET'>");
          client.println("<input type='text' name='text' placeholder='Andika ujumbe...' style='padding: 8px; width: 80%; font-size: 14px;'>");
          client.println("<br><br>");
          client.println("<input type='submit' value='Tuma Ujumbe' style='padding: 8px 15px; background: #007BFF; color: white; border: none; border-radius: 4px;'>");
          client.println("</form>");
          
          client.println("<hr style='margin: 20px 0;'>");
          
          // Ujumbe wa maelezo ya mfumo wa faili
          client.println("<h3>Kushiriki Faili (File Sharing)</h3>");
          client.println("<p style='font-size: 13px; color: #555;'>Faili zilizopakiwa zitahifadhiwa moja kwa moja kwenye kumbukumbu ya ESP32.</p>");
          client.println("<form action='/upload' method='POST' enctype='multipart/form-data'>");
          client.println("<input type='file' name='uploadFile' style='margin-bottom: 10px;'><br>");
          client.println("<input type='submit' value='Pakia Faili' style='padding: 8px 15px; background: #28a745; color: white; border: none; border-radius: 4px;'>");
          client.println("</form>");

          client.println("</div></body></html>");
          break;
        }

        if (c == '\n') {
          currentLineIsBlank = true;
          currentLine = "";
        } else if (c != '\r') {
          currentLineIsBlank = false;
          currentLine += c;
        }
      }
    }
    client.stop();
  }
}

