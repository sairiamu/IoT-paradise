#include <SoftwareSerial.h>

// Wi-Fi Credentials
const char* ssid = "~moran-327~";
const char* password = "o2006wifi!";

// ThingSpeak Details
const char* host = "api.thingspeak.com"; 
const char* readAPIKey = "S94RW66AD4EFHNYP"; 
const char* channelID = "3463754"; 

SoftwareSerial espSerial(2, 3); // RX, TX
int ledPin = 13;

void setup() {
 pinMode(ledPin, OUTPUT);
 digitalWrite(ledPin, LOW);
 
 Serial.begin(115200);
 espSerial.begin(115200); 

 Serial.println("Anza maandalizi...");
 delay(3000); 

 sendCommand("AT+CWMODE=1", 2000);
 
 String connectWiFi = "AT+CWJAP=\"" + String(ssid) + "\",\"" + String(password) + "\"";
 sendCommand(connectWiFi, 7000); 
 
 Serial.println("Mfumo uko tayari!");
}

void loop() {
 String cmd = "AT+CIPSTART=\"TCP\",\"" + String(host) + "\",80";
 espSerial.println(cmd);
 delay(2000);
 
 if (espSerial.find("Error")) {
 Serial.println("Imeshindwa kuunganisha na ThingSpeak Server");
 return;
 }

 String getRequest = "GET /channels/" + String(channelID) + "/fields/1/last.txt?api_key=" + String(readAPIKey) + " HTTP/1.0\r\n\r\n";

 cmd = "AT+CIPSEND=" + String(getRequest.length());
 espSerial.println(cmd);
 delay(1000);


 if (espSerial.find(">")) {
  espSerial.print(getRequest);
 Serial.println("Ombi limetumwa kwa ThingSpeak...");
 }

 long timeout = millis();
 String response = "";
 while (millis() - timeout < 5000) { // Subiri sekunde 5 kukusanya data
 while (espSerial.available()) {
 char c = espSerial.read();
 response += c;
 }
 }


 if (response.length() > 0) {
 Serial.println("--- JIBU KUTOKA SEVA ---");
 Serial.println(response);
 Serial.println("------------------------");

 if (response.endsWith("1") || response.indexOf("\r\n1") >= 0) {
 digitalWrite(ledPin, HIGH); Serial.println("MATOKEO YAKO: LED IMEWAKA (1)");
 } 
 else if (response.endsWith("0") || response.indexOf("\r\n0") >= 0) {
 digitalWrite(ledPin, LOW);
 Serial.println("MATOKEO YAKO: LED IMEZIMWA (0)");
 } else {
 Serial.println("Seva imejibu lakini field haina '1' wala '0'. Angalia ThingSpeak.");
 }
 } else {
 Serial.println("Seva haijajibu kabisa.");
 }

 espSerial.println("AT+CIPCLOSE"); 
 

 delay(15000); 
}

void sendCommand(String command, int timeout) {
 espSerial.println(command);
 long int time = millis();
 while ((time + timeout) > millis()) {
  while (espSerial.available()) {
 char c = espSerial.read();
 Serial.print(c); 
 }
  }
}

