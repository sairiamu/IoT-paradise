/* 
  Smart Digital Menu — Prototype Stage 
  ESP32 + 16x2 Parallel LCD (16-Pin) + Touch sensor (TTP223) + 2 push buttons
*/

#include <LiquidCrystal.h>

// ---------- Configuration ----------
// Pins: RS, EN, D4, D5, D6, D7
// Connect VSS/RW/K to GND, VDD to 5V/3.3V, A to 5V (via resistor)
LiquidCrystal lcd(21, 22, 19, 18, 5, 4); 

const int TOUCH_PIN = 27;
const int BTN_NEXT = 25;
const int BTN_PREV = 26;
const unsigned long DEBOUNCE_MS = 400; 

struct MenuItem {
  const char* name;
  int stock;
};

MenuItem menu[] = {
  {"Chicken Rice", 8},
  {"Beef Stew", 5},
  {"Veg Curry", 6}
};

const int MENU_SIZE = sizeof(menu) / sizeof(menu[0]);
int currentIndex = 0;
unsigned long lastTouchTime = 0;
bool lastTouchState = LOW;

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  
  // Initialize 16x2 parallel LCD
  lcd.begin(16, 2); 
  lcd.print("Menu Ready");
  delay(1000);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);
  
  showMenuItem();
}

// ---------- Main loop ----------
void loop() {
  handleScrollButtons();
  handleTouchConfirm();
}

// ---------- Scrolling ----------
void handleScrollButtons() {
  static unsigned long lastPress = 0;
  if (millis() - lastPress < 250) return; 

  if (digitalRead(BTN_NEXT) == LOW) {
    currentIndex = (currentIndex + 1) % MENU_SIZE;
    showMenuItem();
    lastPress = millis();
  } 
  else if (digitalRead(BTN_PREV) == LOW) {
    currentIndex = (currentIndex - 1 + MENU_SIZE) % MENU_SIZE;
    showMenuItem();
    lastPress = millis();
  }
}

// ---------- Touch confirm ----------
void handleTouchConfirm() {
  bool touched = digitalRead(TOUCH_PIN); 
  
  if (touched == HIGH && lastTouchState == LOW && (millis() - lastTouchTime > DEBOUNCE_MS)) {
    lastTouchTime = millis();
    confirmOrder();
  }
  lastTouchState = touched;
}

void confirmOrder() {
  MenuItem &item = menu[currentIndex];
  
  if (item.stock <= 0) {
    lcd.setCursor(0, 1);
    lcd.print("Sold out!       ");
    Serial.println("Order rejected: item sold out -> " + String(item.name));
    return;
  }
  
  item.stock--; 
  
  Serial.print("ORDER CONFIRMED: ");
  Serial.print(item.name);
  Serial.print(" | remaining stock: ");
  Serial.println(item.stock);
  
  lcd.setCursor(0, 1);
  lcd.print("Order sent!     ");
  delay(800); 
  
  showMenuItem(); 
}

// ---------- Display ----------
void showMenuItem() {
  MenuItem &item = menu[currentIndex];
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(item.name);
  lcd.setCursor(0, 1);
  
  if (item.stock > 0) {
    lcd.print("Stock: ");
    lcd.print(item.stock);
  } else {
    lcd.print("Sold out");
  }
}
