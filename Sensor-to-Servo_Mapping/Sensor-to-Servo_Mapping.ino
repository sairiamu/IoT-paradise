#include <Servo.h>

#define SENSOR_PIN A0
#define SERVO_PIN 9
#define SENSOR_MIN 0
#define SENSOR_MAX 1023

Servo actuator;

void setup() {
  actuator.attach(SERVO_PIN);
  Serial.begin(9600);
}

void loop() {
  int raw = analogRead(SENSOR_PIN);

  // constrain to expected range to avoid unsafe positions
  raw = constrain(raw, SENSOR_MIN, SENSOR_MAX);

  int angle = map(raw, SENSOR_MIN, SENSOR_MAX, 0, 180);
  actuator.write(angle);

  Serial.print("Raw: "); Serial.print(raw);
  Serial.print(" Angle: "); Serial.println(angle);
  delay(200);
}
