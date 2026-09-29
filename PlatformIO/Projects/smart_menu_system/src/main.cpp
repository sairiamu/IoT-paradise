#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>
#include <vector>
#include <algorithm>

#include "index.h"
#include "admin.h"

// Wi-Fi Access Point Configuration
const char* ssid = "ESP32_SmartMenu_AP";
const char* password = "password123";
const String ADMIN_PASSWORD = "admin123";

AsyncWebServer server(80);

// Hardware Pins Configuration
const int TOUCH_PIN = 4;        // Capacitive Touch Pin T0 = GPIO 4
const int STATUS_LED_PIN = 2;   // Onboard LED
const int TOUCH_THRESHOLD = 40; 

const int BTN_UP = 32;          // Navigation Button Up
const int BTN_DOWN = 33;        // Navigation Button Down

// LCD 16x2 pin mapping: (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(13, 12, 14, 27, 26, 25);

// Data Structures
struct MenuItem {
    int id;
    String name;
    float price;
};

struct Order {
    int id;
    String foodName;
    String customerName;
    String seatNumber;
    String status; // "PendingTouch", "Preparing", "Timeout"
};

// In-Memory Storage
std::vector<MenuItem> menu = {
    {1, "Cheeseburger", 8.50},
    {2, "Chicken Wings", 6.00},
    {3, "Pizza Slice", 4.50},
    {4, "Mango Smoothie", 3.50}
};

std::vector<Order> orders;
int nextOrderId = 1;

Order* currentPendingOrder = nullptr;
unsigned long lastTouchTime = 0;
const unsigned long touchCooldown = 1500;
const unsigned long touchTimeoutLimit = 30000;
unsigned long orderStartTime = 0;

// LCD Menu State Variables
int lcdMenuIndex = 0;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;
bool buttonUpLastState = HIGH;
bool buttonDownLastState = HIGH;
bool localOrderSelected = false;
String localCustomerSeat = "Counter / Walk-in";

// --- Helper: build one 16-char-safe LCD row for a menu item ---
// selected = true -> row starts with a ">" cursor to mark the touch-confirm target
String buildMenuRow(const MenuItem &item, bool selected) {
    String cursor = selected ? ">" : " ";
    String priceStr = "$" + String(item.price, 2);

    int nameSpace = 16 - 1 - 1 - priceStr.length();
    if (nameSpace < 1) nameSpace = 1;

    String name = item.name;
    if ((int)name.length() > nameSpace) {
        name = name.substring(0, nameSpace);
    }

    String row = cursor + name;
    while ((int)row.length() < 16 - (int)priceStr.length()) {
        row += " ";
    }
    row += priceStr;

    if ((int)row.length() > 16) row = row.substring(0, 16);
    return row;
}

// --- Prints the full menu as a scrollable 2-row-at-a-time list ---
void printMenuList() {
    int pageStart = (lcdMenuIndex / 2) * 2;

    for (int row = 0; row < 2; row++) {
        int idx = pageStart + row;
        lcd.setCursor(0, row);
        if (idx < (int)menu.size()) {
            lcd.print(buildMenuRow(menu[idx], idx == lcdMenuIndex));
        } else {
            lcd.print("                "); // clear unused row
        }
    }
}

void updateLCDDisplay() {
    lcd.clear();
    if (currentPendingOrder != nullptr && currentPendingOrder->status == "PendingTouch") {
        lcd.setCursor(0, 0);
        lcd.print("Touch Sensor to");
        lcd.setCursor(0, 1);
        lcd.print("Confirm Order!");
    } else if (menu.empty()) {
        lcd.setCursor(0, 0);
        lcd.print("Menu Empty");
    } else {
        printMenuList();
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);

    // Initialize LCD (16 columns, 2 rows)
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("IoT Menu System");
    delay(2000);

    WiFi.softAP(ssid, password);
    Serial.println("\n--- ESP32 Digital Menu IoT System ---");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    auto isAdminAuthenticated = [](AsyncWebServerRequest *request) {
        if (request->hasHeader("Cookie")) {
            String cookie = request->header("Cookie");
            if (cookie.indexOf("admin_session=authenticated") != -1) return true;
        }
        return false;
    };

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.on("/admin", HTTP_GET, [isAdminAuthenticated](AsyncWebServerRequest *request){
        if (!isAdminAuthenticated(request)) {
            request->redirect("/");
            return;
        }
        request->send_P(200, "text/html", admin_html);
    });

    server.on("/api/admin/login", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        DynamicJsonDocument doc(128);
        deserializeJson(doc, data, len);
        String pass = doc["pass"].as<String>();
        if(pass == "") pass = doc["password"].as<String>();

        if (pass == ADMIN_PASSWORD) {
            AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"success\":true}");
            response->addHeader("Set-Cookie", "admin_session=authenticated; Path=/; HttpOnly");
            request->send(response);
        } else {
            request->send(401, "application/json", "{\"success\":false}");
        }
    });

    server.on("/api/admin/logout", HTTP_POST, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"success\":true}");
        response->addHeader("Set-Cookie", "admin_session=; Path=/; Max-Age=0");
        request->send(response);
    });

    server.on("/api/menu", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(1024);
        JsonArray array = doc.to<JsonArray>();
        for(auto const& item : menu) {
            JsonObject obj = array.createNestedObject();
            obj["id"] = item.id;
            obj["name"] = item.name;
            obj["price"] = item.price;
        }
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/menu/add", HTTP_POST, [isAdminAuthenticated](AsyncWebServerRequest *request){}, NULL, [isAdminAuthenticated](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (!isAdminAuthenticated(request)) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        DynamicJsonDocument doc(256);
        deserializeJson(doc, data, len);
        MenuItem newItem;
        newItem.id = menu.empty() ? 1 : menu.back().id + 1;
        newItem.name = doc["name"].as<String>();
        newItem.price = doc["price"].as<float>();
        menu.push_back(newItem);
        request->send(200, "application/json", "{\"status\":\"success\"}");
    });

    server.on("/api/menu/delete", HTTP_POST, [isAdminAuthenticated](AsyncWebServerRequest *request){
        if (!isAdminAuthenticated(request)) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        if(request->hasParam("id")) {
            int id = request->getParam("id")->value().toInt();
            menu.erase(std::remove_if(menu.begin(), menu.end(), [id](MenuItem const& item) {
                return item.id == id;
            }), menu.end());
            // Keep the LCD selection index in bounds after items are removed
            if (!menu.empty() && lcdMenuIndex >= (int)menu.size()) {
                lcdMenuIndex = (int)menu.size() - 1;
            }
            updateLCDDisplay();
        }
        request->send(200, "application/json", "{\"status\":\"success\"}");
    });

    server.on("/api/order", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (currentPendingOrder != nullptr) {
            request->send(400, "application/json", "{\"error\":\"Device busy confirming another order. Please wait.\"}");
            return;
        }
        DynamicJsonDocument doc(256);
        deserializeJson(doc, data, len);
        
        Order newOrder;
        newOrder.id = nextOrderId++;
        newOrder.foodName = doc["foodName"].as<String>();
        newOrder.customerName = doc["customerName"].as<String>();
        newOrder.seatNumber = doc["seatNumber"].as<String>();
        newOrder.status = "PendingTouch";

        orders.push_back(newOrder);
        currentPendingOrder = &orders.back();
        orderStartTime = millis();
        updateLCDDisplay();

        request->send(200, "application/json", "{\"status\":\"waiting_touch\",\"orderId\":" + String(newOrder.id) + "}");
    });

    server.on("/api/order/status", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("id")) {
            int id = request->getParam("id")->value().toInt();
            for(auto const& o : orders) {
                if(o.id == id) {
                    request->send(200, "application/json", "{\"status\":\"" + o.status + "\"}");
                    return;
                }
            }
        }
        request->send(404, "application/json", "{\"status\":\"NotFound\"}");
    });

    // --- FIXED: admin orders endpoint ---
    // Previously only returned orders with status == "Preparing", so any order
    // still waiting on the customer's touch confirmation ("PendingTouch") was
    // invisible to admin. Now both states are returned, and the JSON buffer
    // is bumped from 1024 -> 2048 so it doesn't silently truncate/drop orders
    // once there are several of them.
    server.on("/api/orders", HTTP_GET, [isAdminAuthenticated](AsyncWebServerRequest *request){
        if (!isAdminAuthenticated(request)) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        DynamicJsonDocument doc(2048);
        JsonArray array = doc.to<JsonArray>();
        for(auto const& o : orders) {
            if(o.status == "Preparing" || o.status == "PendingTouch") {
                JsonObject obj = array.createNestedObject();
                obj["id"] = o.id;
                obj["foodName"] = o.foodName;
                obj["customerName"] = o.customerName;
                obj["seatNumber"] = o.seatNumber;
                obj["status"] = o.status;
            }
        }
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/order/complete", HTTP_POST, [isAdminAuthenticated](AsyncWebServerRequest *request){
        if (!isAdminAuthenticated(request)) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        if(request->hasParam("id")) {
            int id = request->getParam("id")->value().toInt();
            orders.erase(std::remove_if(orders.begin(), orders.end(), [id](Order const& o) {
                return o.id == id;
            }), orders.end());
        }
        request->send(200, "application/json", "{\"status\":\"success\"}");
    });

    server.begin();
    updateLCDDisplay();
}

void loop() {
    // Handle physical Up/Down buttons for LCD menu navigation when no order is pending confirmation
    if (currentPendingOrder == nullptr && !menu.empty()) {
        bool btnUpState = digitalRead(BTN_UP);
        bool btnDownState = digitalRead(BTN_DOWN);

        if (btnUpState == LOW && buttonUpLastState == HIGH && (millis() - lastButtonPress > debounceDelay)) {
            lastButtonPress = millis();
            lcdMenuIndex = (lcdMenuIndex - 1 + menu.size()) % menu.size();
            updateLCDDisplay();
        }
        if (btnDownState == LOW && buttonDownLastState == HIGH && (millis() - lastButtonPress > debounceDelay)) {
            lastButtonPress = millis();
            lcdMenuIndex = (lcdMenuIndex + 1) % menu.size();
            updateLCDDisplay();
        }
        buttonUpLastState = btnUpState;
        buttonDownLastState = btnDownState;

        // If user presses touch sensor while browsing the LCD menu, initiate a local order
        // for whichever item is currently marked with ">" (lcdMenuIndex)
        int touchValue = touchRead(TOUCH_PIN);
        if (touchValue < TOUCH_THRESHOLD && (millis() - lastTouchTime > touchCooldown)) {
            lastTouchTime = millis();
            
            Order lcdOrder;
            lcdOrder.id = nextOrderId++;
            lcdOrder.foodName = menu[lcdMenuIndex].name;
            lcdOrder.customerName = "LCD Terminal User";
            lcdOrder.seatNumber = "Walk-In Desk";
            lcdOrder.status = "PendingTouch";

            orders.push_back(lcdOrder);
            currentPendingOrder = &orders.back();
            orderStartTime = millis();
            updateLCDDisplay();
        }
    } 
    // Handle confirmation phase for whichever interface requested an order
    else if (currentPendingOrder != nullptr) {
        if (millis() - orderStartTime > touchTimeoutLimit) {
            currentPendingOrder->status = "Timeout";
            currentPendingOrder = nullptr;
            updateLCDDisplay();
        } else {
            int touchValue = touchRead(TOUCH_PIN);
            if (touchValue < TOUCH_THRESHOLD && (millis() - lastTouchTime > touchCooldown)) {
                lastTouchTime = millis();
                
                currentPendingOrder->status = "Preparing";
                
                digitalWrite(STATUS_LED_PIN, HIGH);
                delay(300);
                digitalWrite(STATUS_LED_PIN, LOW);

                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Order Confirmed!");
                lcd.setCursor(0, 1);
                lcd.print("Sent to Kitchen");
                delay(2000);

                currentPendingOrder = nullptr;
                updateLCDDisplay();
            }
        }
    }
    delay(50);
}

