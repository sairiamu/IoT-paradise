/*
  Smart Digital Menu — Prototype Stage
  ESP32 + 16x2 I2C LCD + Touch sensor (e.g. TTP223) + 2 push buttons for scrolling

  Behavior:
  - LCD shows the currently selected menu item and its stock count
  - Buttons scroll left/right through the menu
  - Touch sensor confirms the order for the item on screen
  - Serial Monitor logs the "order event" (this is where you'll later add
    a WiFi HTTP/MQTT call to push the order to your cloud backend)

  Wiring (adjust pins to your board):
    LCD (I2C)      -> SDA = GPIO21, SCL = GPIO22
    Touch sensor   -> OUT = GPIO27
    Button NEXT    -> GPIO25 (INPUT_PULLUP, other leg to GND)
    Button PREV    -> GPIO26 (INPUT_PULLUP, other leg to GND)

  Libraries needed (Arduino IDE Library Manager):
    - LiquidCrystal_I2C
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Configuration ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);   // change 0x27 if your I2C scanner finds a different address

const int TOUCH_PIN = 27;
const int BTN_NEXT   = 25;
const int BTN_PREV   = 26;

const unsigned long DEBOUNCE_MS = 400;   // prevents one touch registering as multiple orders

struct MenuItem {
  const char* name;
  int stock;
};

MenuItem menu[] = {
  {"Chicken Rice", 8},
  {"Beef Stew",     5},
  {"Veg Curry",     6}
};
const int MENU_SIZE = sizeof(menu) / sizeof(menu[0]);

int currentIndex = 0;
unsigned long lastTouchTime = 0;
bool lastTouchState = LOW;

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

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
  if (millis() - lastPress < 250) return;   // simple button debounce

  if (digitalRead(BTN_NEXT) == LOW) {
    currentIndex = (currentIndex + 1) % MENU_SIZE;
    showMenuItem();
    lastPress = millis();
  } else if (digitalRead(BTN_PREV) == LOW) {
    currentIndex = (currentIndex - 1 + MENU_SIZE) % MENU_SIZE;
    showMenuItem();
    lastPress = millis();
  }
}

// ---------- Touch confirm ----------
void handleTouchConfirm() {
  bool touched = digitalRead(TOUCH_PIN);

  // Rising edge + debounce window, so one touch = one order
  if (touched == HIGH && lastTouchState == LOW &&
      (millis() - lastTouchTime > DEBOUNCE_MS)) {

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

  // TODO: replace this Serial log with an HTTP POST or MQTT publish
  // to your cloud backend, e.g.:
  //   POST /order { "item": item.name, "remaining": item.stock }
  Serial.print("ORDER CONFIRMED: ");
  Serial.print(item.name);
  Serial.print(" | remaining stock: ");
  Serial.println(item.stock);

  showMenuItem();          // refresh LCD with new stock count
  lcd.setCursor(0, 1);
  lcd.print("Order sent!     ");
  delay(800);              // brief confirmation message
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