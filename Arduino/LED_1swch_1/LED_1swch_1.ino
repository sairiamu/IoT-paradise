const int BUTTON_PIN = 2;
const int LED_PIN1 = 7;

int state = 0;          
int lastButtonState = HIGH;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN1, OUTPUT);
  digitalWrite(LED_PIN1, LOW);
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);

 
  if (reading == LOW && lastButtonState == HIGH) {
    state = !state;              
    digitalWrite(LED_PIN1, state);
    delay(200);                  
  }

  lastButtonState = reading;
}

