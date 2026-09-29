/*
  ESP32 Slave - Water Sensor
  --------------------------

  Sends water/rain sensor readings
  to Raspberry Pi FastAPI server.

  Data:

    device
    sensor_type
    date
    day
    timestamp
    value
    status

  Status:

    WET -> ADC value < wetThreshold
    DRY -> ADC value >= wetThreshold
*/


#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>


// ==================================================
// WIFI
// ==================================================

const char* ssid = "Blizzie";

const char* password =
    "brk7thegreat";


// ==================================================
// RASPBERRY PI SERVER
// ==================================================

const char* serverUrl =
    "http://10.239.156.187:8000/data";


// ==================================================
// SENSOR
// ==================================================

const int waterPin = 34;

const char* deviceName =
    "esp32_water";


// Tune this after testing your sensor
const int wetThreshold = 2500;


// ==================================================
// SEND INTERVAL
// ==================================================

unsigned long lastSend = 0;

const unsigned long sendInterval =
    2000;


// ==================================================
// TIME
// Tanzania = UTC+3
// ==================================================

const char* ntpServer =
    "pool.ntp.org";

const long gmtOffset_sec =
    3 * 3600;

const int daylightOffset_sec =
    0;


// ==================================================
// WIFI CONNECTION
// ==================================================

void connectWiFi()
{
    Serial.print(
        "Connecting to WiFi"
    );

    WiFi.begin(
        ssid,
        password
    );

    while (
        WiFi.status() != WL_CONNECTED
    )
    {
        delay(500);

        Serial.print(".");
    }

    Serial.println();

    Serial.println(
        "WiFi connected!"
    );

    Serial.print(
        "ESP32 IP: "
    );

    Serial.println(
        WiFi.localIP()
    );
}


// ==================================================
// GET CURRENT TIMESTAMP
// ==================================================

String getTimestamp()
{
    struct tm timeinfo;


    if (
        !getLocalTime(&timeinfo)
    )
    {
        return "0000-00-00T00:00:00";
    }


    char buffer[30];


    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%dT%H:%M:%S",
        &timeinfo
    );


    return String(buffer);
}


// ==================================================
// GET WEEKDAY
// ==================================================

String getDay()
{
    struct tm timeinfo;


    if (
        !getLocalTime(&timeinfo)
    )
    {
        return "Unknown";
    }


    char buffer[20];


    strftime(
        buffer,
        sizeof(buffer),
        "%A",
        &timeinfo
    );


    return String(buffer);
}


// ==================================================
// SEND WATER READING
// ==================================================

void sendReading(
    int value,
    String status,
    String timestamp,
    String day
)
{
    // Reconnect if necessary

    if (
        WiFi.status() != WL_CONNECTED
    )
    {
        connectWiFi();
    }


    HTTPClient http;


    http.begin(
        serverUrl
    );


    http.addHeader(
        "Content-Type",
        "application/json"
    );


    // Date = first 10 characters
    //
    // Example:
    //
    // 2026-09-10T17:45:30
    //
    // becomes:
    //
    // 2026-09-10

    String date =
        timestamp.substring(0, 10);


    // ==================================================
    // CORRECT WATER PAYLOAD
    // ==================================================

    String jsonPayload =
        "{"
        "\"device\":\"" +
        String(deviceName) +
        "\","
        "\"sensor_type\":\"water\","
        "\"date\":\"" +
        date +
        "\","
        "\"day\":\"" +
        day +
        "\","
        "\"timestamp\":\"" +
        timestamp +
        "\","
        "\"value\":" +
        String(value) +
        ","
        "\"status\":\"" +
        status +
        "\""
        "}";


    Serial.println();

    Serial.println(
        "Sending to Raspberry Pi:"
    );

    Serial.println(
        jsonPayload
    );


    // ==================================================
    // POST
    // ==================================================

    int httpCode =
        http.POST(
            jsonPayload
        );


    if (httpCode > 0)
    {
        Serial.printf(
            "HTTP response: %d\n",
            httpCode
        );


        String response =
            http.getString();


        Serial.println(
            "Server response:"
        );

        Serial.println(
            response
        );
    }
    else
    {
        Serial.printf(
            "POST failed: %s\n",
            http.errorToString(
                httpCode
            ).c_str()
        );
    }


    http.end();
}


// ==================================================
// SETUP
// ==================================================

void setup()
{
    Serial.begin(
        115200
    );


    pinMode(
        waterPin,
        INPUT
    );


    connectWiFi();


    // Configure Tanzania time

    configTime(
        gmtOffset_sec,
        daylightOffset_sec,
        ntpServer
    );


    Serial.println(
        "Time synchronization started."
    );
}


// ==================================================
// LOOP
// ==================================================

void loop()
{
    if (
        millis() - lastSend >=
        sendInterval
    )
    {
        lastSend = millis();


        // ----------------------------------------------
        // Read water sensor
        // ----------------------------------------------

        int value =
            analogRead(
                waterPin
            );


        // ----------------------------------------------
        // Determine status
        // ----------------------------------------------

        String status;


        if (
            value < wetThreshold
        )
        {
            status = "DRY";
        }
        else
        {
            status = "WET";
        }


        // ----------------------------------------------
        // Time
        // ----------------------------------------------

        String timestamp =
            getTimestamp();


        String day =
            getDay();


        // ----------------------------------------------
        // Serial monitor
        // ----------------------------------------------

        Serial.printf(
            "Water: %d | %s | %s | %s\n",
            value,
            status.c_str(),
            day.c_str(),
            timestamp.c_str()
        );


        // ----------------------------------------------
        // Send
        // ----------------------------------------------

        sendReading(
            value,
            status,
            timestamp,
            day
        );
    }
}

