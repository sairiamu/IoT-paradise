

#include <DHT.h>
#include <SoftwareSerial.h>

// ============================================================
// DHT11 SENSOR CONFIGURATION
// ============================================================

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dhtSensor(DHTPIN, DHTTYPE);

// ============================================================
// ESP8266 SERIAL CONFIGURATION
// ============================================================

// Arduino D3 -> ESP8266 TX
// Arduino D4 -> ESP8266 RX
SoftwareSerial wifiBridge(2, 3);

// ============================================================
// WI-FI / THINGSPEAK CONFIGURATION
// ============================================================

String networkSSID     = "~moran-327~";
String networkPASSWORD = "o2006wifi";
String writeApiKey     = "3LXPSTH773AR5O0M";

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);
  wifiBridge.begin(115200);

  Serial.println();
  Serial.println("================================");
  Serial.println(" Arduino IoT System Starting");
  Serial.println("================================");

  // Start DHT11
  dhtSensor.begin();

  delay(1000);

  // ----------------------------------------------------------
  // Check ESP8266
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("[1] Checking ESP8266...");

  if (sendCommand("AT", "OK", 3000)) {
    Serial.println("ESP8266: OK");
  } else {
    Serial.println("ERROR: ESP8266 is not responding.");
  }

  // ----------------------------------------------------------
  // Reset ESP8266
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("[2] Resetting ESP8266...");

  sendCommand("AT+RST", "ready", 5000);

  delay(1000);

  // ----------------------------------------------------------
  // Set Wi-Fi Station Mode
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("[3] Setting Wi-Fi Station Mode...");

  sendCommand("AT+CWMODE=1", "OK", 3000);

  // ----------------------------------------------------------
  // Use single TCP connection
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("[4] Configuring TCP mode...");

  sendCommand("AT+CIPMUX=0", "OK", 3000);

  // ----------------------------------------------------------
  // Connect to Wi-Fi
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("[5] Connecting to Wi-Fi...");

  String connectCommand =
      "AT+CWJAP=\"" +
      networkSSID +
      "\",\"" +
      networkPASSWORD +
      "\"";

  if (sendCommand(connectCommand, "WIFI GOT IP", 20000)) {
    Serial.println();
    Serial.println("Wi-Fi connection successful!");
  } else {
    Serial.println();
    Serial.println("WARNING: Wi-Fi connection was not confirmed.");
  }

  // ----------------------------------------------------------
  // Show IP address
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("[6] Checking IP address...");

  sendCommand("AT+CIFSR", "OK", 5000);

  Serial.println();
  Serial.println("================================");
  Serial.println(" Initialization Complete");
  Serial.println("================================");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  // ----------------------------------------------------------
  // Read DHT11
  // ----------------------------------------------------------

  float temperature = dhtSensor.readTemperature();
  float humidity = dhtSensor.readHumidity();

  // ----------------------------------------------------------
  // Validate sensor data
  // ----------------------------------------------------------

  if (isnan(temperature) || isnan(humidity)) {

    Serial.println();
    Serial.println("ERROR: DHT11 reading failed.");

    delay(3000);
    return;
  }

  // ----------------------------------------------------------
  // Display sensor values
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("--------------------------------");
  Serial.println(" New Sensor Reading");
  Serial.println("--------------------------------");

  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" °C");

  Serial.print("Humidity:    ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  // ----------------------------------------------------------
  // Close any previous TCP connection
  // ----------------------------------------------------------

  sendCommand("AT+CIPCLOSE", "OK", 2000);

  // ----------------------------------------------------------
  // Connect to ThingSpeak
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("Connecting to ThingSpeak...");

  String tcpCommand =
      "AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80";

  if (!sendCommand(tcpCommand, "CONNECT", 8000)) {

    Serial.println("ERROR: Could not connect to ThingSpeak.");

    delay(30000);
    return;
  }

  Serial.println("TCP connection established.");

  // ----------------------------------------------------------
  // Build HTTP request
  // ----------------------------------------------------------

  String httpPayload =
      "GET /update?api_key=" +
      writeApiKey +
      "&field1=" +
      String(temperature, 1) +
      "&field2=" +
      String(humidity, 1) +
      " HTTP/1.1\r\n"
      "Host: api.thingspeak.com\r\n"
      "Connection: close\r\n"
      "\r\n";

  // ----------------------------------------------------------
  // Tell ESP8266 how many bytes we're sending
  // ----------------------------------------------------------

  Serial.println();
  Serial.print("Sending ");
  Serial.print(httpPayload.length());
  Serial.println(" bytes to ThingSpeak...");

  String sendCommandText =
      "AT+CIPSEND=" +
      String(httpPayload.length());

  if (!sendCommand(sendCommandText, ">", 5000)) {

    Serial.println("ERROR: ESP8266 did not give the SEND prompt.");

    sendCommand("AT+CIPCLOSE", "OK", 2000);

    delay(30000);
    return;
  }

  // ----------------------------------------------------------
  // Send actual HTTP request
  // ----------------------------------------------------------

  wifiBridge.print(httpPayload);

  Serial.println("HTTP request sent.");
  Serial.println();
  Serial.println("Waiting for ThingSpeak response...");
  Serial.println("--------------------------------");

  // ----------------------------------------------------------
  // Read ThingSpeak response
  // ----------------------------------------------------------

  bool uploadSuccessful = waitForThingSpeakResponse(10000);

  Serial.println("--------------------------------");

  if (uploadSuccessful) {

    Serial.println();
    Serial.println("================================");
    Serial.println("       UPLOAD SUCCESS!");
    Serial.println("================================");

  } else {

    Serial.println();
    Serial.println("================================");
    Serial.println("       UPLOAD FAILED!");
    Serial.println("================================");
  }

  // ----------------------------------------------------------
  // Close connection
  // ----------------------------------------------------------

  sendCommand("AT+CIPCLOSE", "OK", 3000);

  // ----------------------------------------------------------
  // ThingSpeak update interval
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("Waiting 30 seconds before next upload...");

  delay(30000);
}

// ============================================================
// SEND AT COMMAND AND WAIT FOR RESPONSE
// ============================================================

bool sendCommand(
    String command,
    String expectedResponse,
    unsigned long timeout
) {

  // Clear old ESP8266 data
  while (wifiBridge.available()) {
    wifiBridge.read();
  }

  Serial.print(">> ");
  Serial.println(command);

  wifiBridge.println(command);

  unsigned long startTime = millis();

  String response = "";

  while (millis() - startTime < timeout) {

    while (wifiBridge.available()) {

      char c = wifiBridge.read();

      Serial.write(c);

      response += c;

      // Prevent String from becoming excessively large
      if (response.length() > 300) {
        response.remove(0, 100);
      }

      if (response.indexOf(expectedResponse) != -1) {
        return true;
      }

      if (response.indexOf("ERROR") != -1) {
        return false;
      }

      if (response.indexOf("FAIL") != -1) {
        return false;
      }
    }
  }

  return false;
}

// ============================================================
// READ THINGSPEAK HTTP RESPONSE
// ============================================================

bool waitForThingSpeakResponse(unsigned long timeout) {

  unsigned long startTime = millis();

  String response = "";

  while (millis() - startTime < timeout) {

    while (wifiBridge.available()) {

      char c = wifiBridge.read();

      Serial.write(c);

      response += c;

      // ThingSpeak returns the entry ID in the response body.
      //
      // Successful example:
      //
      // HTTP/1.1 200 OK
      //
      // 42
      //
      // Failed update:
      //
      // 0

      if (response.indexOf("200 OK") != -1) {

        // Keep reading until the response body arrives.
        delay(500);

        while (wifiBridge.available()) {

          char bodyChar = wifiBridge.read();

          Serial.write(bodyChar);

          response += bodyChar;
        }

        // Look for a successful ThingSpeak entry ID.
        //
        // A successful update returns a positive entry ID.
        // A failed update returns 0.

        int bodyStart = response.lastIndexOf("\r\n\r\n");

        if (bodyStart != -1) {

          String body =
              response.substring(bodyStart + 4);

          body.trim();

          Serial.println();
          Serial.print("ThingSpeak response body: ");
          Serial.println(body);

          int entryID = body.toInt();

          if (entryID > 0) {

            Serial.print("ThingSpeak Entry ID: ");
            Serial.println(entryID);

            return true;
          }

          if (entryID == 0) {

            Serial.println(
              "ThingSpeak rejected the update."
            );

            return false;
          }
        }
      }
    }
  }

  return false;
}