
#include <DHT.h>
#include <Servo.h>

#define DHTPIN 2
#define DHTTYPE DHT11
#define SERVO_PIN 9

DHT dht(DHTPIN, DHTTYPE);
Servo vent;

void setup() {
  dht.begin();
  vent.attach(SERVO_PIN);
  Serial.begin(9600);
}

void loop() {
  float temp = dht.readTemperature();

  if (isnan(temp)) {
    delay(1000);
    return;
  }

  if (temp < 20) {
    vent.write(0);        // Closed
  } else if (temp < 25) {
    vent.write(45);       // Slightly open
  } else if (temp < 30) {
    vent.write(90);       // Half open
  } else {
    vent.write(180);      // Fully open
  }

  Serial.print("Temp: ");
  Serial.println(temp);
  delay(1000);
}
