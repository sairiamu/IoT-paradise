#define MODE_SWITCH 2
#define POT_PIN A0
#define TEMP_PIN A1     // simulate sensor with analog input if no DHT
#define MOTOR_PIN 9

int lastManualSpeed = 0;

void setup() {
  pinMode(MODE_SWITCH, INPUT_PULLUP);
  pinMode(MOTOR_PIN, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  bool autoMode = digitalRead(MODE_SWITCH) == LOW;

  if (autoMode) {
    int sensorVal = analogRead(TEMP_PIN);
    int speed = map(sensorVal, 0, 1023, 0, 255);
    analogWrite(MOTOR_PIN, speed);
    Serial.println("AUTO mode");
  } else {
    int potVal = analogRead(POT_PIN);
    int speed = map(potVal, 0, 1023, 0, 255);
    analogWrite(MOTOR_PIN, speed);
    lastManualSpeed = speed;   // remember for extension challenge
    Serial.println("MANUAL mode");
  }

  delay(200);
}
