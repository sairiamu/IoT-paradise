/*
  Smart Digital Menu - ESP32 Prototype
  Components: 16x2 LCD (parallel mode), TTP223 touch sensor, 2 push buttons
  Board: DOIT ESP32 DEVKIT V1
*/

#include <LiquidCrystal.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------- LCD PINS (parallel mode) ----------
// LiquidCrystal(RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(21, 22, 19, 18, 5, 4);

// ---------- INPUT PINS ----------
const int touchPin   = 13;  // TTP223 SIG (moved from 15 to avoid strapping pin issues)
const int nextPin    = 27;  // "Next item" button
const int prevPin    = 26;  // "Previous item" button

// ---------- WIFI CREDENTIALS ----------
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ---------- BACKEND ENDPOINT ----------
// Replace with your actual server/Firebase REST URL
const char* serverUrl = "https://your-backend-url.com/api/order";

// ---------- MENU DATA ----------
struct MenuItem {
  String name;
  int stock;
};

MenuItem menu[] = {
  {"Chicken Rice", 5},
  {"Beef Stew", 3},
  {"Veg Curry", 0},   // 0 = sold out
  {"Fish Fry", 7}
};
const int menuSize = sizeof(menu) / sizeof(menu[0]);
int currentIndex = 0;

// ---------- DEBOUNCE STATE ----------
unsigned long lastTouchTime = 0;
unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 400; // ms

void setup() {
  Serial.begin(115200);

  // LCD init (parallel mode uses begin(), NOT init())
  lcd.begin(16, 2);
  lcd.print("Connecting...");

  pinMode(touchPin, INPUT);
  pinMode(nextPin, INPUT_PULLUP);
  pinMode(prevPin, INPUT_PULLUP);

  connectWiFi();
  displayCurrentItem();
}

void loop() {
  // --- Handle "Next" button ---
  if (digitalRead(nextPin) == LOW && millis() - lastButtonTime > debounceDelay) {
    lastButtonTime = millis();
    currentIndex = (currentIndex + 1) % menuSize;
    displayCurrentItem();
  }

  // --- Handle "Previous" button ---
  if (digitalRead(prevPin) == LOW && millis() - lastButtonTime > debounceDelay) {
    lastButtonTime = millis();
    currentIndex = (currentIndex - 1 + menuSize) % menuSize;
    displayCurrentItem();
  }

  // --- Handle touch confirm ---
  if (digitalRead(touchPin) == HIGH && millis() - lastTouchTime > debounceDelay) {
    lastTouchTime = millis();
    confirmOrder();
  }
}

void displayCurrentItem() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(menu[currentIndex].name);

  lcd.setCursor(0, 1);
  if (menu[currentIndex].stock <= 0) {
    lcd.print("SOLD OUT");
  } else {
    lcd.print("Stock: ");
    lcd.print(menu[currentIndex].stock);
  }
}

void confirmOrder() {
  if (menu[currentIndex].stock <= 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(menu[currentIndex].name);
    lcd.setCursor(0, 1);
    lcd.print("Unavailable");
    delay(1200);
    displayCurrentItem();
    return;
  }

  // Reduce local stock immediately for responsiveness
  menu[currentIndex].stock--;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Order sent:");
  lcd.setCursor(0, 1);
  lcd.print(menu[currentIndex].name);

  sendOrderToServer(menu[currentIndex].name);

  delay(1500);
  displayCurrentItem();
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("WiFi Connected");
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  } else {
    lcd.print("WiFi Failed");
    Serial.println("\nWiFi connection failed");
  }
  delay(1000);
}

void sendOrderToServer(String itemName) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping server update");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  doc["item"] = itemName;
  doc["action"] = "order_confirmed";

  String payload;
  serializeJson(doc, payload);

  int httpResponseCode = http.POST(payload);
  Serial.print("HTTP Response: ");
  Serial.println(httpResponseCode);

  http.end();
}
