#define IR_PIN 7

#define GREEN_LED 8
#define RED_LED 9

#define FILTER_DELAY 500

void setup() {
  pinMode(IR_PIN, INPUT);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  
  Serial.begin(9600);
}

void loop() {
  int reading1 = digitalRead(IR_PIN);
  delay(FILTER_DELAY);
  int reading2 = digitalRead(IR_PIN);

  if (reading1 == reading2) {
    if (reading1 == LOW) {
      digitalWrite(RED_LED, HIGH);
      digitalWrite(GREEN_LED, LOW);
      Serial.println("Occupied");
    } else {
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
      Serial.println("Available");
    }
  }

  delay(2000);
}
