const int led1 = 7; // green
const int led2 = 6; // red
const int button = 2;

bool led1On = false;
bool led2On = false;

void setup() {
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(button, INPUT_PULLUP);
}

void loop() {
  if (digitalRead(button) == LOW) {   // button pressed
    delay(50);                        // simple debounce
    while (digitalRead(button) == LOW); // wait for release

    // check if a second press follows within 350ms
    unsigned long pressTime = millis();
    bool secondPress = false;

    while (millis() - pressTime < 350) {
      if (digitalRead(button) == LOW) {
        secondPress = true;
        delay(50);
        while (digitalRead(button) == LOW); // wait for release
        break;
      }
    }

    if (secondPress) {
      led1On = false;
      led2On = true;
    } else {
      led1On = true;
      led2On = false;
    }

    digitalWrite(led1, led1On);
    digitalWrite(led2, led2On);
  }
}



