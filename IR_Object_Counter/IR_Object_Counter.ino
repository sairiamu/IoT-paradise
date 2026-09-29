#define IR_PIN 7

int count = 0;
int lastState = HIGH;

void setup() {
  pinMode(IR_PIN, INPUT);
  Serial.begin(9600);
}

void loop() {
  int currentState = digitalRead(IR_PIN);

  // detect transition from not-detected to detected
  if (currentState == LOW && lastState == HIGH) {
    count++;
    Serial.print("Object count: ");
    Serial.println(count);
  }

  lastState = currentState;
  delay(50);   // simple debounce
}