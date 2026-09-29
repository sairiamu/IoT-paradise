#include <DHT.h>
#include <Servo.h>

const int ventPin = 8;
const int DHT_PIN = 4;
#define DHTTYPE DHT11

DHT dht(DHT_PIN,DHTTYPE);
Servo ventilator;

void setup() {
  pinMode(DHT_PIN, INPUT);
  pinMode(ventPin, OUTPUT);

  ventilator.attach(ventPin);
  // ventilator.begin();
  // ventPin.write(0);

  dht.begin();
  Serial.begin(9600);
}

void loop() {
  float tempC = dht.readTemperature();

  if (isnan(tempC)) {
    Serial.println("Cant read Temperature condition!!, no mechanics can be performed");
  }
  else {

    if (tempC <= 15) {
      ventilator.write(90);

      Serial.print("Temperature: ");
      Serial.print(tempC);
      Serial.println(" C");
    }
    else if (tempC <= 25 && tempC >= 15) {
      ventilator.write(125);

      Serial.print("Temperature: ");
      Serial.print(tempC);
      Serial.println(" C");
    }
    else {
      ventilator.write(180);

      Serial.print("Temperature: ");
      Serial.print(tempC);
      Serial.println(" C");
    }
  }
  delay(5000);

}
