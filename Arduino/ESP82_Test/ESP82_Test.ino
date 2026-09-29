void setup() {
  Serial.begin(115200); // current confirmed ESP baud
  delay(2000);
  Serial.println("AT+UART_DEF=9600,8,1,0,0");
}

void loop() {
  // nothing else - just watch the response below
}

