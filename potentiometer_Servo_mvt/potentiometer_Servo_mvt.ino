#include <Servo.h>

// int readings = A0;

Servo myServo;

void setup() {
  pinMode(A0, INPUT);
  pinMode(13, OUTPUT);

  myServo.attach(13);
  myServo.write(0);

}

void loop() {
  float pReadings = analogRead(A0);

  int angle = map(pReadings, 1, 1023, 0, 180);
  myServo.write(angle);

}


