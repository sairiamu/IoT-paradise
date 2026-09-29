int potPin = A0;
int ledPin = 9;
int buttonPin = 2;

bool ledState = false;
int lastButtonState = HIGH;

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  // Detect when button is pressed
  if (lastButtonState == HIGH && buttonState == LOW) {
    ledState = !ledState;
    delay(50);  // debounce
  }

  lastButtonState = buttonState;

  if (ledState) {
    int potValue = analogRead(potPin);
    int brightness = map(potValue, 0, 1023, 0, 255);

    analogWrite(ledPin, brightness);
  } else {
    analogWrite(ledPin, 0);
  }
}


