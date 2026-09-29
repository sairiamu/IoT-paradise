#include <Servo.h>

#define IR_PIN 7
#define SERVO_PIN 9
#define OPEN_ANGLE 180
#define CLOSED_ANGLE 0
#define OPEN_TIME 3000

Servo door;

void setup() {
  pinMode(IR_PIN, INPUT);
  door.attach(SERVO_PIN);
  door.write(CLOSED_ANGLE);
  Serial.begin(9600);
}

void loop() {
  int detected = digitalRead(IR_PIN);

  if (detected == LOW) {
    Serial.println("Object detected - opening door");
    door.write(OPEN_ANGLE);
    delay(OPEN_TIME);

    while (digitalRead(IR_PIN) == LOW) {
      delay(200);
    }

    Serial.println("Path clear - closing door");
    door.write(CLOSED_ANGLE);
  }

  delay(200);
}
