int redLED = 10;
int yellowLED = 11;
int greenLED = 12;

void setup() {
  pinMode(redLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
}

void loop() {

  // RED
  digitalWrite(redLED, HIGH);
  digitalWrite(yellowLED, LOW);
  digitalWrite(greenLED, LOW);
  delay(500);

  // GREEN
  digitalWrite(redLED, LOW);
  digitalWrite(greenLED, HIGH);
  delay(500);

  // YELLOW
  digitalWrite(greenLED, LOW);
  digitalWrite(yellowLED, HIGH);
  delay(200);

  // Back to RED
  digitalWrite(yellowLED, LOW);
}