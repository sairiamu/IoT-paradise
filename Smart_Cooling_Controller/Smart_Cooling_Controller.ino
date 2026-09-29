#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
#define FAN_PIN 9

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  dht.begin();
  pinMode(FAN_PIN, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    
    Serial.println("Cant read!!");
    delay(5000);
  }

  // Rule: high humidity makes heat feel worse -> lower threshold
  bool hot = temp > 28;
  bool humid = hum > 60;

  if (hot && humid) {
    analogWrite(FAN_PIN, 255);   // full speed, feels hotter
  } else if (hot || humid) {
    analogWrite(FAN_PIN, 150);   // medium speed
  } else {
    analogWrite(FAN_PIN, 0);     // off
  }

  Serial.print("Temp: "); Serial.print(temp);
  Serial.print(" Hum: "); Serial.println(hum);
  delay(1000);
}
