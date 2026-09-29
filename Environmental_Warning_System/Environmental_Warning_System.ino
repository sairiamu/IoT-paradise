#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11

#define GREEN_LED 8
#define YELLOW_LED 9
#define RED_LED 10
#define BUZZER 11

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  dht.begin();
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  float temp = dht.readTemperature();

  if (isnan(temp)) {
    delay(1000);
    return;
  }

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  if (temp < 28) {
    digitalWrite(GREEN_LED, HIGH);      // Normal
  } else if (temp < 35) {
    digitalWrite(YELLOW_LED, HIGH);     // Warning
    tone(BUZZER, 1000, 200);
  } else {
    digitalWrite(RED_LED, HIGH);        // Critical
    tone(BUZZER, 2000, 500);
  }

  Serial.print("Temp: ");
  Serial.println(temp);
  delay(1000);
}
