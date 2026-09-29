/* ============================================================
   SMART MENU — Standalone ESP32 Digital Ordering System
   - ESP32 runs its own WiFi Access Point + Web Server + WebSocket
   - Customers connect their phone to the ESP32 WiFi and browse the menu
   - A touch sensor wired to the ESP32 confirms the order
   - Kitchen staff connect to the same WiFi and open /kitchen for the
     live order board + menu management
   ============================================================ */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ---------------- CONFIG ----------------
const char* AP_SSID     = "SmartMenu_WiFi";
const char* AP_PASSWORD = "menu12345";      // min 8 chars, or "" for open network

#define TOUCH_PIN   4     // TTP223 OUT pin  (digital touch module)
#define LED_PIN     2     // status LED (+ 220ohm resistor) - optional
#define BUZZER_PIN  5     // optional confirmation beep
#define USE_NATIVE_TOUCH false   // set true to use a bare ESP32 touch pad instead of TTP223
#define NATIVE_TOUCH_THRESHOLD 40

// ---------------- DATA MODELS ----------------
struct MenuItem {
  int id;
  String name;
  float price;
  String category;
  bool available;
};

enum OrderStatus { WAITING_TOUCH, CONFIRMED, PREPARING, COMPLETED, CANCELLED };

struct Order {
  int id;
  int foodId;
  String foodName;
  float price;
  String customerName;
  String seatNumber;
  OrderStatus status;
  unsigned long createdAt;
};

#define MAX_MENU_ITEMS  50
#define MAX_ORDERS      100

MenuItem menu[MAX_MENU_ITEMS];
int menuCount = 0;

Order orders[MAX_ORDERS];
int orderCount = 0;

int nextMenuId  = 1;
int nextOrderId = 1;

// The order currently awaiting a physical touch to confirm it
int pendingOrderIndex = -1;

Preferences prefs;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------------- TOUCH DEBOUNCE ----------------
bool lastTouchState = false;
unsigned long lastTouchMillis = 0;
const unsigned long DEBOUNCE_MS = 300;

// ============================================================
// PERSISTENCE (menu survives reboot, stored as JSON in NVS)
// ============================================================
void saveMenuToFlash() {
  StaticJsonDocument<4096> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < menuCount; i++) {
    JsonObject o = arr.createNestedObject();
    o["id"] = menu[i].id;
    o["name"] = menu[i].name;
    o["price"] = menu[i].price;
    o["category"] = menu[i].category;
    o["available"] = menu[i].available;
  }
  String out;
  serializeJson(doc, out);
  prefs.putString("menu", out);
  prefs.putInt("nextMenuId", nextMenuId);
}

void loadMenuFromFlash() {
  String saved = prefs.getString("menu", "");
  nextMenuId = prefs.getInt("nextMenuId", 1);
  if (saved.length() == 0) {
    // seed default menu on first boot
    addMenuItem("Grilled Chicken", 12.50, "Main", true);
    addMenuItem("Beef Burger", 9.00, "Main", true);
    addMenuItem("Caesar Salad", 7.50, "Starter", true);
    addMenuItem("French Fries", 3.50, "Sides", true);
    addMenuItem("Chocolate Cake", 5.00, "Dessert", true);
    addMenuItem("Fresh Juice", 4.00, "Drinks", true);
    return;
  }
  StaticJsonDocument<4096> doc;
  deserializeJson(doc, saved);
  JsonArray arr = doc.as<JsonArray>();
  menuCount = 0;
  for (JsonObject o : arr) {
    if (menuCount >= MAX_MENU_ITEMS) break;
    menu[menuCount].id = o["id"];
    menu[menuCount].name = o["name"].as<String>();
    menu[menuCount].price = o["price"];
    menu[menuCount].category = o["category"].as<String>();
    menu[menuCount].available = o["available"];
    menuCount++;
  }
}

// ============================================================
// MENU HELPERS
// ============================================================
int addMenuItem(String name, float price, String category, bool available) {
  if (menuCount >= MAX_MENU_ITEMS) return -1;
  menu[menuCount].id = nextMenuId++;
  menu[menuCount].name = name;
  menu[menuCount].price = price;
  menu[menuCount].category = category;
  menu[menuCount].available = available;
  menuCount++;
  saveMenuToFlash();
  return menu[menuCount - 1].id;
}

bool removeMenuItem(int id) {
  for (int i = 0; i < menuCount; i++) {
    if (menu[i].id == id) {
      for (int j = i; j < menuCount - 1; j++) menu[j] = menu[j + 1];
      menuCount--;
      saveMenuToFlash();
      return true;
    }
  }
  return false;
}

int findMenuIndex(int id) {
  for (int i = 0; i < menuCount; i++) if (menu[i].id == id) return i;
  return -1;
}

// ============================================================
// JSON SERIALIZATION
// ============================================================
String menuToJson() {
  StaticJsonDocument<4096> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < menuCount; i++) {
    JsonObject o = arr.createNestedObject();
    o["id"] = menu[i].id;
    o["name"] = menu[i].name;
    o["price"] = menu[i].price;
    o["category"] = menu[i].category;
    o["available"] = menu[i].available;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

String statusToStr(OrderStatus s) {
  switch (s) {
    case WAITING_TOUCH: return "waiting_touch";
    case CONFIRMED: return "confirmed";
    case PREPARING: return "preparing";
    case COMPLETED: return "completed";
    case CANCELLED: return "cancelled";
  }
  return "unknown";
}

String orderToJsonObj(Order &o) {
  StaticJsonDocument<512> doc;
  doc["id"] = o.id;
  doc["foodId"] = o.foodId;
  doc["foodName"] = o.foodName;
  doc["price"] = o.price;
  doc["customerName"] = o.customerName;
  doc["seatNumber"] = o.seatNumber;
  doc["status"] = statusToStr(o.status);
  doc["createdAt"] = o.createdAt;
  String out;
  serializeJson(doc, out);
  return out;
}

String ordersToJson(bool activeOnly) {
  StaticJsonDocument<8192> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < orderCount; i++) {
    if (activeOnly && (orders[i].status == COMPLETED || orders[i].status == CANCELLED)) continue;
    JsonObject o = arr.createNestedObject();
    o["id"] = orders[i].id;
    o["foodId"] = orders[i].foodId;
    o["foodName"] = orders[i].foodName;
    o["price"] = orders[i].price;
    o["customerName"] = orders[i].customerName;
    o["seatNumber"] = orders[i].seatNumber;
    o["status"] = statusToStr(orders[i].status);
    o["createdAt"] = orders[i].createdAt;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

// ============================================================
// WEBSOCKET BROADCAST
// ============================================================
void broadcast(String type, String payloadJson) {
  String msg = "{\"type\":\"" + type + "\",\"data\":" + payloadJson + "}";
  ws.textAll(msg);
}

// ============================================================
// TOUCH SENSOR HANDLING  (called every loop())
// ============================================================
void handleTouchSensor() {
  bool touched;
  if (USE_NATIVE_TOUCH) {
    touched = touchRead(TOUCH_PIN) < NATIVE_TOUCH_THRESHOLD;
  } else {
    touched = digitalRead(TOUCH_PIN) == HIGH;
  }

  if (touched && !lastTouchState && (millis() - lastTouchMillis > DEBOUNCE_MS)) {
    lastTouchMillis = millis();
    onTouchConfirm();
  }
  lastTouchState = touched;
}

void onTouchConfirm() {
  if (pendingOrderIndex == -1) return;   // nothing waiting to be confirmed

  orders[pendingOrderIndex].status = CONFIRMED;
  Order &o = orders[pendingOrderIndex];

  // feedback: LED flash + beep
  digitalWrite(LED_PIN, HIGH);
  tone(BUZZER_PIN, 2000, 150);
  delay(150);
  digitalWrite(LED_PIN, LOW);

  broadcast("order_confirmed", orderToJsonObj(o));
  pendingOrderIndex = -1;
}

// ============================================================
// EMBEDDED WEB PAGES  (HTML + CSS + JS as raw strings)
// ============================================================
const char CUSTOMER_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Menu</title>
<style>
  :root{--bg:#12141a;--card:#1c1f28;--accent:#ff8a3d;--text:#f2f2f2;--muted:#9098a8;}
  *{box-sizing:border-box;margin:0;padding:0;font-family:'Segoe UI',Roboto,sans-serif;}
  body{background:var(--bg);color:var(--text);min-height:100vh;padding:16px;}
  h1{font-size:24px;margin-bottom:4px;}
  .sub{color:var(--muted);font-size:13px;margin-bottom:20px;}
  .cats{display:flex;gap:8px;overflow-x:auto;margin-bottom:16px;}
  .cat{background:var(--card);padding:8px 16px;border-radius:20px;font-size:13px;white-space:nowrap;cursor:pointer;border:1px solid #2c2f3a;}
  .cat.active{background:var(--accent);color:#1a1a1a;font-weight:600;border-color:var(--accent);}
  .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px;}
  .food{background:var(--card);border-radius:14px;padding:14px;cursor:pointer;border:1px solid #2c2f3a;transition:transform .15s;}
  .food:active{transform:scale(0.96);}
  .food.unavail{opacity:0.35;pointer-events:none;}
  .food h3{font-size:15px;margin-bottom:6px;}
  .food .price{color:var(--accent);font-weight:700;font-size:16px;}
  .food .tag{font-size:11px;color:var(--muted);margin-top:4px;}
  .overlay{position:fixed;inset:0;background:rgba(0,0,0,0.7);display:none;align-items:center;justify-content:center;padding:20px;z-index:10;}
  .overlay.show{display:flex;}
  .modal{background:var(--card);border-radius:16px;padding:24px;width:100%;max-width:340px;}
  .modal h2{font-size:18px;margin-bottom:16px;}
  .modal label{font-size:12px;color:var(--muted);display:block;margin:12px 0 4px;}
  .modal input{width:100%;padding:12px;border-radius:10px;border:1px solid #333;background:#12141a;color:var(--text);font-size:15px;}
  .modal .btns{display:flex;gap:10px;margin-top:20px;}
  .btn{flex:1;padding:12px;border-radius:10px;border:none;font-weight:600;font-size:14px;cursor:pointer;}
  .btn-cancel{background:#2c2f3a;color:var(--text);}
  .btn-confirm{background:var(--accent);color:#1a1a1a;}
  .waiting{position:fixed;inset:0;background:var(--bg);display:none;flex-direction:column;align-items:center;justify-content:center;text-align:center;padding:20px;z-index:20;}
  .waiting.show{display:flex;}
  .pulse{width:110px;height:110px;border-radius:50%;background:var(--accent);display:flex;align-items:center;justify-content:center;font-size:40px;margin-bottom:24px;animation:pulse 1.4s infinite;}
  @keyframes pulse{0%{box-shadow:0 0 0 0 rgba(255,138,61,0.5);}70%{box-shadow:0 0 0 30px rgba(255,138,61,0);}100%{box-shadow:0 0 0 0 rgba(255,138,61,0);}}
  .waiting h2{font-size:20px;margin-bottom:8px;}
  .waiting p{color:var(--muted);font-size:14px;}
  .toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%);background:#1fbf6a;color:#08301c;padding:14px 24px;border-radius:12px;font-weight:600;display:none;z-index:30;}
  .toast.show{display:block;}
</style></head>
<body>
  <h1>Table Menu</h1>
  <div class="sub">Tap a dish, enter your details, then touch the sensor to confirm</div>
  <div class="cats" id="cats"></div>
  <div class="grid" id="grid"></div>

  <div class="overlay" id="overlay">
    <div class="modal">
      <h2 id="modalTitle">Item</h2>
      <label>Your name</label>
      <input id="custName" placeholder="e.g. Alex" maxlength="24">
      <label>Seat number</label>
      <input id="seatNum" placeholder="e.g. 12" maxlength="10">
      <div class="btns">
        <button class="btn btn-cancel" onclick="closeModal()">Cancel</button>
        <button class="btn btn-confirm" onclick="submitOrder()">Place order</button>
      </div>
    </div>
  </div>

  <div class="waiting" id="waiting">
    <div class="pulse">👆</div>
    <h2>Touch the sensor to confirm</h2>
    <p id="waitingSub">Waiting for confirmation...</p>
  </div>

  <div class="toast" id="toast">Order confirmed! Sent to the kitchen.</div>

<script>
let menuData = [];
let selectedFood = null;
let pendingId = null;
let ws;

function connectWS(){
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onmessage = (e)=>{
    const msg = JSON.parse(e.data);
    if(msg.type === 'menu_updated'){ menuData = msg.data; render(); }
    if(msg.type === 'order_confirmed' && msg.data.id === pendingId){
      document.getElementById('waiting').classList.remove('show');
      showToast();
      pendingId = null;
    }
  };
  ws.onclose = ()=> setTimeout(connectWS, 1500);
}

async function loadMenu(){
  const res = await fetch('/api/menu');
  menuData = await res.json();
  render();
}

function render(){
  const cats = [...new Set(menuData.map(m=>m.category))];
  document.getElementById('cats').innerHTML = cats.map(c=>`<div class="cat">${c}</div>`).join('');
  document.getElementById('grid').innerHTML = menuData.map(item => `
    <div class="food ${item.available ? '' : 'unavail'}" onclick="openModal(${item.id})">
      <h3>${item.name}</h3>
      <div class="price">$${item.price.toFixed(2)}</div>
      <div class="tag">${item.category}${item.available ? '' : ' - Unavailable'}</div>
    </div>`).join('');
}

function openModal(id){
  selectedFood = menuData.find(m=>m.id===id);
  document.getElementById('modalTitle').innerText = selectedFood.name + '  $' + selectedFood.price.toFixed(2);
  document.getElementById('custName').value='';
  document.getElementById('seatNum').value='';
  document.getElementById('overlay').classList.add('show');
}
function closeModal(){ document.getElementById('overlay').classList.remove('show'); }

async function submitOrder(){
  const name = document.getElementById('custName').value.trim();
  const seat = document.getElementById('seatNum').value.trim();
  if(!name || !seat){ alert('Please enter your name and seat number'); return; }

  const res = await fetch('/api/order', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({foodId: selectedFood.id, customerName: name, seatNumber: seat})
  });
  const order = await res.json();
  pendingId = order.id;
  closeModal();
  document.getElementById('waitingSub').innerText = `${name}, table item: ${selectedFood.name}`;
  document.getElementById('waiting').classList.add('show');
}

function showToast(){
  const t = document.getElementById('toast');
  t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'), 3000);
}

loadMenu();
connectWS();
</script>
</body></html>
)HTML";

const char KITCHEN_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kitchen Dashboard</title>
<style>
  :root{--bg:#0f1115;--card:#1a1d24;--accent:#ff8a3d;--text:#f2f2f2;--muted:#9098a8;--ok:#1fbf6a;}
  *{box-sizing:border-box;margin:0;padding:0;font-family:'Segoe UI',Roboto,sans-serif;}
  body{background:var(--bg);color:var(--text);padding:16px;}
  header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;}
  h1{font-size:22px;}
  .tabs{display:flex;gap:8px;margin-bottom:16px;}
  .tab{padding:8px 16px;border-radius:8px;background:var(--card);cursor:pointer;font-size:13px;border:1px solid #2a2d36;}
  .tab.active{background:var(--accent);color:#1a1a1a;font-weight:600;}
  .panel{display:none;} .panel.active{display:block;}
  .board{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:14px;}
  .card{background:var(--card);border-radius:14px;padding:16px;border-left:4px solid var(--accent);}
  .card.preparing{border-left-color:#3d9bff;}
  .card h3{font-size:16px;margin-bottom:6px;}
  .meta{font-size:13px;color:var(--muted);margin-bottom:4px;}
  .badge{display:inline-block;font-size:11px;padding:3px 8px;border-radius:12px;background:#2a2d36;margin-top:8px;}
  .btnrow{display:flex;gap:8px;margin-top:12px;}
  .btn{flex:1;padding:9px;border-radius:8px;border:none;font-weight:600;font-size:12px;cursor:pointer;}
  .btn-prep{background:#3d9bff;color:#08182e;}
  .btn-done{background:var(--ok);color:#08301c;}
  .empty{color:var(--muted);text-align:center;padding:40px;grid-column:1/-1;}

  .menuForm{background:var(--card);border-radius:14px;padding:16px;margin-bottom:16px;display:grid;grid-template-columns:2fr 1fr 1fr auto;gap:10px;}
  .menuForm input{padding:10px;border-radius:8px;border:1px solid #333;background:#0f1115;color:var(--text);}
  .menuForm button{background:var(--accent);border:none;border-radius:8px;color:#1a1a1a;font-weight:700;cursor:pointer;}
  .mlist{display:flex;flex-direction:column;gap:8px;}
  .mrow{background:var(--card);padding:12px 16px;border-radius:10px;display:flex;justify-content:space-between;align-items:center;}
  .mrow .name{font-weight:600;} .mrow .price{color:var(--accent);margin-left:10px;}
  .mrow .actions{display:flex;gap:8px;}
  .mini{padding:6px 10px;font-size:12px;border-radius:6px;border:none;cursor:pointer;}
  .mini.toggle{background:#2a2d36;color:var(--text);}
  .mini.del{background:#e0473f;color:#fff;}
</style></head>
<body>
<header><h1>Kitchen Dashboard</h1><div class="meta" id="clock"></div></header>
<div class="tabs">
  <div class="tab active" onclick="showTab('orders')">Live orders</div>
  <div class="tab" onclick="showTab('menu')">Manage menu</div>
</div>

<div class="panel active" id="ordersPanel">
  <div class="board" id="board"><div class="empty">No active orders</div></div>
</div>

<div class="panel" id="menuPanel">
  <div class="menuForm">
    <input id="mName" placeholder="Food name">
    <input id="mPrice" placeholder="Price" type="number" step="0.01">
    <input id="mCat" placeholder="Category">
    <button onclick="addItem()">Add</button>
  </div>
  <div class="mlist" id="mlist"></div>
</div>

<script>
let ws, allOrders = [], menuData = [];

function connectWS(){
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onmessage=(e)=>{
    const msg = JSON.parse(e.data);
    if(msg.type==='order_confirmed'){ allOrders.push(msg.data); renderBoard(); }
    if(msg.type==='order_status'){ 
      const i = allOrders.findIndex(o=>o.id===msg.data.id);
      if(i>=0) allOrders[i]=msg.data; else allOrders.push(msg.data);
      renderBoard();
    }
    if(msg.type==='menu_updated'){ menuData = msg.data; renderMenu(); }
  };
  ws.onclose=()=>setTimeout(connectWS,1500);
}

async function loadOrders(){
  const res = await fetch('/api/orders?active=1');
  allOrders = await res.json();
  renderBoard();
}
async function loadMenu(){
  const res = await fetch('/api/menu');
  menuData = await res.json();
  renderMenu();
}

function renderBoard(){
  const active = allOrders.filter(o=>o.status==='confirmed'||o.status==='preparing');
  const board = document.getElementById('board');
  if(active.length===0){ board.innerHTML='<div class="empty">No active orders</div>'; return; }
  board.innerHTML = active.map(o=>`
    <div class="card ${o.status}">
      <h3>${o.foodName}</h3>
      <div class="meta">Customer: ${o.customerName}</div>
      <div class="meta">Seat: ${o.seatNumber}</div>
      <span class="badge">${o.status}</span>
      <div class="btnrow">
        ${o.status==='confirmed' ? `<button class="btn btn-prep" onclick="setStatus(${o.id},'preparing')">Start preparing</button>`:''}
        <button class="btn btn-done" onclick="setStatus(${o.id},'completed')">Mark done</button>
      </div>
    </div>`).join('');
}

async function setStatus(id, status){
  await fetch('/api/order/status', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({id, status})});
}

function renderMenu(){
  document.getElementById('mlist').innerHTML = menuData.map(m=>`
    <div class="mrow">
      <div><span class="name">${m.name}</span><span class="price">$${m.price.toFixed(2)}</span> <span style="color:var(--muted);font-size:12px">${m.category}</span></div>
      <div class="actions">
        <button class="mini toggle" onclick="toggleItem(${m.id})">${m.available?'Hide':'Show'}</button>
        <button class="mini del" onclick="delItem(${m.id})">Delete</button>
      </div>
    </div>`).join('');
}

async function addItem(){
  const name = document.getElementById('mName').value.trim();
  const price = parseFloat(document.getElementById('mPrice').value);
  const category = document.getElementById('mCat').value.trim() || 'Other';
  if(!name || isNaN(price)){ alert('Enter a name and valid price'); return; }
  await fetch('/api/menu', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name, price, category})});
  document.getElementById('mName').value='';
  document.getElementById('mPrice').value='';
  document.getElementById('mCat').value='';
}
async function toggleItem(id){
  await fetch('/api/menu/toggle', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({id})});
}
async function delItem(id){
  if(!confirm('Delete this item?')) return;
  await fetch('/api/menu/' + id, {method:'DELETE'});
}

function showTab(t){
  document.querySelectorAll('.tab').forEach(el=>el.classList.remove('active'));
  document.querySelectorAll('.panel').forEach(el=>el.classList.remove('active'));
  event.target.classList.add('active');
  document.getElementById(t+'Panel').classList.add('active');
}

setInterval(()=>{document.getElementById('clock').innerText=new Date().toLocaleTimeString();},1000);
loadOrders(); loadMenu(); connectWS();
</script>
</body></html>
)HTML";

// ============================================================
// ROUTE HANDLERS
// ============================================================
void setupRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", CUSTOMER_HTML);
  });

  server.on("/kitchen", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", KITCHEN_HTML);
  });

  // GET /api/menu
  server.on("/api/menu", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", menuToJson());
  });

  // POST /api/menu  { name, price, category }
  server.on("/api/menu", HTTP_POST, [](AsyncWebServerRequest *req) {},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      StaticJsonDocument<256> doc;
      deserializeJson(doc, data, len);
      addMenuItem(doc["name"].as<String>(), doc["price"], doc["category"].as<String>(), true);
      broadcast("menu_updated", menuToJson());
      req->send(200, "application/json", "{\"ok\":true}");
    });

  // POST /api/menu/toggle { id }
  server.on("/api/menu/toggle", HTTP_POST, [](AsyncWebServerRequest *req) {},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      StaticJsonDocument<128> doc;
      deserializeJson(doc, data, len);
      int idx = findMenuIndex(doc["id"]);
      if (idx >= 0) { menu[idx].available = !menu[idx].available; saveMenuToFlash(); }
      broadcast("menu_updated", menuToJson());
      req->send(200, "application/json", "{\"ok\":true}");
    });

  // DELETE /api/menu/:id
  server.on("^\\/api\\/menu\\/([0-9]+)$", HTTP_DELETE, [](AsyncWebServerRequest *req) {
    int id = req->pathArg(0).toInt();
    removeMenuItem(id);
    broadcast("menu_updated", menuToJson());
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // GET /api/orders?active=1
  server.on("/api/orders", HTTP_GET, [](AsyncWebServerRequest *req) {
    bool activeOnly = req->hasParam("active");
    req->send(200, "application/json", ordersToJson(activeOnly));
  });

  // POST /api/order  { foodId, customerName, seatNumber } -> creates order, waits for touch
  server.on("/api/order", HTTP_POST, [](AsyncWebServerRequest *req) {},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      StaticJsonDocument<256> doc;
      deserializeJson(doc, data, len);
      int foodId = doc["foodId"];
      int mi = findMenuIndex(foodId);
      if (mi < 0 || orderCount >= MAX_ORDERS) {
        req->send(400, "application/json", "{\"error\":\"invalid item\"}");
        return;
      }
      Order o;
      o.id = nextOrderId++;
      o.foodId = foodId;
      o.foodName = menu[mi].name;
      o.price = menu[mi].price;
      o.customerName = doc["customerName"].as<String>();
      o.seatNumber = doc["seatNumber"].as<String>();
      o.status = WAITING_TOUCH;
      o.createdAt = millis();
      orders[orderCount] = o;
      pendingOrderIndex = orderCount;   // ESP32 now watches the touch sensor for this order
      orderCount++;

      req->send(200, "application/json", orderToJsonObj(orders[orderCount - 1]));
    });

  // POST /api/order/status  { id, status }
  server.on("/api/order/status", HTTP_POST, [](AsyncWebServerRequest *req) {},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      StaticJsonDocument<128> doc;
      deserializeJson(doc, data, len);
      int id = doc["id"];
      String st = doc["status"].as<String>();
      for (int i = 0; i < orderCount; i++) {
        if (orders[i].id == id) {
          if (st == "preparing") orders[i].status = PREPARING;
          else if (st == "completed") orders[i].status = COMPLETED;
          else if (st == "cancelled") orders[i].status = CANCELLED;
          broadcast("order_status", orderToJsonObj(orders[i]));
          break;
        }
      }
      req->send(200, "application/json", "{\"ok\":true}");
    });
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, USE_NATIVE_TOUCH ? INPUT : INPUT_PULLDOWN);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  prefs.begin("smartmenu", false);
  loadMenuFromFlash();

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP started. Connect to WiFi \"");
  Serial.print(AP_SSID);
  Serial.print("\" then open http://");
  Serial.println(ip);

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      client->text("{\"type\":\"menu_updated\",\"data\":" + menuToJson() + "}");
    }
  });
  server.addHandler(&ws);

  setupRoutes();
  server.begin();
}

void loop() {
  handleTouchSensor();
  ws.cleanupClients();
}

