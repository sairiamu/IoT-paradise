#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
#define IR_PIN 7
#define ALARM_PIN 8
#define TEMP_THRESHOLD 30

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  dht.begin();
  pinMode(IR_PIN, INPUT);
  pinMode(ALARM_PIN, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  float temp = dht.readTemperature();
  int objectDetected = digitalRead(IR_PIN) == LOW;

  if (isnan(temp)) {
    delay(1000);
    Serial.println("Cant read data!");
    return;
  }

  bool tempCondition = temp > TEMP_THRESHOLD;

  // alarm only when BOTH conditions are true
  if (tempCondition && objectDetected) {
    digitalWrite(ALARM_PIN, HIGH);
    Serial.println("ALARM: both conditions met");
  } else {
    digitalWrite(ALARM_PIN, LOW);
  }

  Serial.print("Temp: "); Serial.print(temp);
  Serial.print(" | Object: "); Serial.println(objectDetected);
  delay(500);
}