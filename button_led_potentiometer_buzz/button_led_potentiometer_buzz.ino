int potPin = A0;
int ledPin = 9;
int buttonPin = 2;
int buzzerPin = 8;

bool ledState = false;
int lastButtonState = HIGH;

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  // Button imebonyezwa
  if (lastButtonState == HIGH && buttonState == LOW) {

    // Badilisha LED ON/OFF
    ledState = !ledState;

    // Beep yenye "bass" zaidi - masafa ya chini na tabaka mbili
    tone(buzzerPin, 220);   // sauti ya chini (deep)
    delay(90);
    tone(buzzerPin, 150);   // chini zaidi kwa hisia ya "bass"
    delay(140);
    noTone(buzzerPin);

    delay(50); // debounce
  }

  lastButtonState = buttonState;

  // Potentiometer inadhibiti brightness
  if (ledState) {
    int potValue = analogRead(potPin);
    int brightness = map(potValue, 0, 1023, 0, 255);

    analogWrite(ledPin, brightness);
  } 
  else {
    analogWrite(ledPin, 0);
  }
}


// int potPin = A0;
// int ledPin = 9;
// int buttonPin = 2;
// int buzzerPin = 8;

// bool ledState = false;
// int lastButtonState = HIGH;

// void setup() {
//   pinMode(ledPin, OUTPUT);
//   pinMode(buttonPin, INPUT_PULLUP);
//   pinMode(buzzerPin, OUTPUT);
// }

// void loop() {
//   int buttonState = digitalRead(buttonPin);

//   // Button imebonyezwa
//   if (lastButtonState == HIGH && buttonState == LOW) {

//     // Badilisha LED ON/OFF
//     ledState = !ledState;

//     // Toa beep
//     tone(buzzerPin, 1000);
//     delay(150);
//     noTone(buzzerPin);

//     delay(50); // debounce
//   }

//   lastButtonState = buttonState;

//   // Potentiometer inadhibiti brightness
//   if (ledState) {
//     int potValue = analogRead(potPin);
//     int brightness = map(potValue, 0, 1023, 0, 255);

//     analogWrite(ledPin, brightness);
//   } 
//   else {
//     analogWrite(ledPin, 0);
//   }
// }


