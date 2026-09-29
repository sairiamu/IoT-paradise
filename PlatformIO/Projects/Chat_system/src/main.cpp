#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <vector>

// ================= CONFIG =================
const char* AP_SSID     = "lwr";
const char* AP_PASSWORD = "chatpass123";   // 8+ chars, or "" for an open network
const int   MAX_CLIENTS = 8;

const size_t MAX_MSG_CHARS   = 1000;
const size_t MAX_FILE_CHARS  = 4000;    // text files
const size_t MAX_IMAGE_CHARS = 24000;   // base64 photo payload (~18KB raw)
// ============================================

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;
IPAddress apIP(192, 168, 4, 1);

struct ChatUser { uint32_t id; String name; };
std::vector<ChatUser> users;
std::vector<String> groups = { "Main" };

String nameForId(uint32_t id) {
  for (auto &u : users) if (u.id == id) return u.name;
  return "";
}
bool nameTaken(const String &n) {
  for (auto &u : users) if (u.name.equalsIgnoreCase(n)) return true;
  return false;
}
void sendError(AsyncWebSocketClient* client, const String &msg) {
  StaticJsonDocument<256> doc;
  doc["type"] = "error"; doc["text"] = msg;
  String out; serializeJson(doc, out);
  client->text(out);
}
void broadcastUserList() {
  StaticJsonDocument<1024> doc;
  doc["type"] = "users";
  JsonArray arr = doc.createNestedArray("list");
  for (auto &u : users) arr.add(u.name);
  String out; serializeJson(doc, out);
  ws.textAll(out);
}
void broadcastGroupList() {
  StaticJsonDocument<1024> doc;
  doc["type"] = "groups";
  JsonArray arr = doc.createNestedArray("list");
  for (auto &g : groups) arr.add(g);
  String out; serializeJson(doc, out);
  ws.textAll(out);
}
void broadcastSystem(const String &room, const String &text) {
  StaticJsonDocument<256> doc;
  doc["type"] = "system"; doc["room"] = room; doc["text"] = text;
  String out; serializeJson(doc, out);
  ws.textAll(out);
}

void handleWsMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
  DynamicJsonDocument doc(len + 2048);
  if (deserializeJson(doc, data, len)) return;
  String type = doc["type"] | "";

  if (type == "login") {
    String name = String((const char*)(doc["name"] | ""));
    name.trim();
    if (name.length() == 0 || name.length() > 24) { sendError(client, "Name must be 1-24 characters."); return; }
    if (users.size() >= MAX_CLIENTS) { sendError(client, "Chat room is full."); return; }
    if (nameTaken(name)) { sendError(client, "That name is already taken."); return; }
    users.push_back({ client->id(), name });

    StaticJsonDocument<256> wl;
    wl["type"] = "welcome"; wl["name"] = name;
    String out; serializeJson(wl, out);
    client->text(out);

    broadcastUserList();
    broadcastGroupList();
    broadcastSystem("Main", name + " joined the chat");
  }
  else if (type == "create_group") {
    String from = nameForId(client->id());
    if (from.length() == 0) { sendError(client, "Please log in first."); return; }
    String g = String((const char*)(doc["name"] | ""));
    g.trim();
    if (g.length() == 0 || g.length() > 24) { sendError(client, "Group name must be 1-24 characters."); return; }
    for (auto &ex : groups) if (ex.equalsIgnoreCase(g)) { sendError(client, "Group already exists."); return; }
    groups.push_back(g);
    broadcastGroupList();
  }
  else if (type == "msg") {
    String from = nameForId(client->id());
    if (from.length() == 0) { sendError(client, "Please log in first."); return; }
    String room = doc["room"] | "Main";
    String text = doc["text"] | "";
    if (text.length() == 0) return;
    if (text.length() > MAX_MSG_CHARS) { sendError(client, "Message too long."); return; }
    DynamicJsonDocument out(text.length() + 512);
    out["type"] = "msg"; out["room"] = room; out["from"] = from; out["text"] = text;
    String o; serializeJson(out, o);
    ws.textAll(o);
  }
  else if (type == "file") {
    String from = nameForId(client->id());
    if (from.length() == 0) { sendError(client, "Please log in first."); return; }
    String room = doc["room"] | "Main";
    String fname = doc["name"] | "file.txt";
    const char* content = doc["data"] | "";
    if (strlen(content) > MAX_FILE_CHARS) { sendError(client, "File too large (max 4000 characters)."); return; }
    DynamicJsonDocument out(strlen(content) + 1024);
    out["type"] = "file"; out["room"] = room; out["from"] = from; out["name"] = fname; out["data"] = content;
    String o; serializeJson(out, o);
    ws.textAll(o);
  }
  else if (type == "image") {
    String from = nameForId(client->id());
    if (from.length() == 0) { sendError(client, "Please log in first."); return; }
    String room = doc["room"] | "Main";
    String mime = doc["mime"] | "image/jpeg";
    const char* content = doc["data"] | "";
    if (strlen(content) > MAX_IMAGE_CHARS) { sendError(client, "Image too large."); return; }
    DynamicJsonDocument out(strlen(content) + 1024);
    out["type"] = "image"; out["room"] = room; out["from"] = from; out["mime"] = mime; out["data"] = content;
    String o; serializeJson(out, o);
    ws.textAll(o);
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DISCONNECT) {
    String leftName = nameForId(client->id());
    users.erase(std::remove_if(users.begin(), users.end(),
      [&](ChatUser &u){ return u.id == client->id(); }), users.end());
    if (leftName.length()) {
      broadcastUserList();
      broadcastSystem("Main", leftName + " left the chat");
    }
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      handleWsMessage(client, data, len);
    }
  }
}

// ================= WEB UI =================
const char INDEX_HTML[] PROGMEM = R"INDEXHTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>ESP32 Chat</title>
<style>
  :root{
    --primary:#4f46e5; --primary-dark:#4338ca; --bg:#f3f4f6; --panel:#ffffff;
    --border:#e5e7eb; --text:#1f2937; --muted:#6b7280; --bubble-me:#4f46e5;
    --bubble-them:#ffffff; --online:#22c55e;
  }
  *{box-sizing:border-box;margin:0;padding:0;}
  html,body{height:100%;}
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;
    background:var(--bg);color:var(--text);height:100vh;height:100dvh;overflow:hidden;}

  /* ---------- LOGIN ---------- */
  #loginScreen{height:100vh;height:100dvh;display:flex;align-items:center;justify-content:center;
    background:linear-gradient(135deg,var(--primary),#7c3aed);padding:20px;}
  .loginCard{background:var(--panel);border-radius:16px;padding:36px 32px;width:100%;max-width:380px;
    box-shadow:0 20px 60px rgba(0,0,0,.25);text-align:center;}
  .loginCard h1{font-size:22px;margin-bottom:6px;}
  .loginCard p{color:var(--muted);font-size:14px;margin-bottom:24px;}
  .loginCard input{width:100%;padding:13px 14px;border:1.5px solid var(--border);border-radius:10px;
    font-size:15px;margin-bottom:12px;outline:none;transition:.15s;}
  .loginCard input:focus{border-color:var(--primary);}
  .loginCard button{width:100%;padding:13px;border:none;border-radius:10px;background:var(--primary);
    color:#fff;font-size:15px;font-weight:600;cursor:pointer;transition:.15s;}
  .loginCard button:hover{background:var(--primary-dark);}
  #loginError{color:#ef4444;font-size:13px;min-height:18px;margin-top:4px;}

  /* ---------- APP LAYOUT ---------- */
  #app{display:none;height:100vh;height:100dvh;}
  .layout{display:flex;height:100vh;height:100dvh;min-height:0;}
  .sidebar{width:300px;flex-shrink:0;background:var(--panel);border-right:1px solid var(--border);
    display:flex;flex-direction:column;min-height:0;transition:transform .25s ease;}
  .sidebar-header{padding:16px;border-bottom:1px solid var(--border);display:flex;align-items:center;
    justify-content:space-between;gap:8px;}
  .me{display:flex;align-items:center;gap:10px;min-width:0;}
  .avatar{width:36px;height:36px;border-radius:50%;display:flex;align-items:center;justify-content:center;
    color:#fff;font-weight:700;font-size:14px;flex-shrink:0;}
  .sec{padding:10px 16px 4px;font-size:12px;font-weight:700;color:var(--muted);text-transform:uppercase;
    letter-spacing:.05em;display:flex;justify-content:space-between;align-items:center;}
  .addBtn{background:none;border:none;color:var(--primary);font-size:18px;cursor:pointer;line-height:1;padding:2px 6px;}
  .closeSidebarBtn{display:none;background:none;border:none;font-size:24px;line-height:1;
    color:var(--muted);cursor:pointer;padding:2px 6px;flex-shrink:0;}
  .list{overflow-y:auto;flex:1;min-height:0;}
  .item{display:flex;align-items:center;gap:10px;padding:10px 16px;cursor:pointer;transition:.1s;}
  .item:hover{background:#f9fafb;}
  .item.active{background:#eef2ff;}
  .item .name{font-size:14px;font-weight:500;flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
  .badge{background:var(--primary);color:#fff;font-size:11px;font-weight:700;border-radius:10px;
    min-width:18px;height:18px;display:flex;align-items:center;justify-content:center;padding:0 5px;}
  .dot{width:8px;height:8px;border-radius:50%;background:var(--online);flex-shrink:0;}

  .main{flex:1;display:flex;flex-direction:column;min-width:0;min-height:0;}
  .topbar{background:var(--panel);border-bottom:1px solid var(--border);padding:12px 16px;
    display:flex;align-items:center;gap:12px;flex-shrink:0;}
  .hamburger{display:none;background:none;border:none;font-size:22px;cursor:pointer;color:var(--text);}
  .roomTitle{font-weight:700;font-size:16px;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
  .statusDot{width:9px;height:9px;border-radius:50%;background:var(--online);margin-right:2px;flex-shrink:0;}
  .iconBtn{background:var(--primary);color:#fff;border:none;border-radius:8px;padding:8px 14px;
    font-size:13px;font-weight:600;cursor:pointer;display:flex;align-items:center;gap:6px;flex-shrink:0;}
  .iconBtn:hover{background:var(--primary-dark);}

  .messages{flex:1;min-height:0;overflow-y:auto;padding:18px;display:flex;flex-direction:column;gap:10px;
    background:
      radial-gradient(circle at 20% 20%, #eef2ff 0%, transparent 40%),
      radial-gradient(circle at 80% 80%, #f5f3ff 0%, transparent 40%), var(--bg);}
  .msgRow{display:flex;max-width:75%;}
  .msgRow.me{align-self:flex-end;flex-direction:row-reverse;}
  .bubble{padding:9px 13px;border-radius:14px;font-size:14px;line-height:1.4;box-shadow:0 1px 2px rgba(0,0,0,.06);}
  .msgRow.me .bubble{background:var(--bubble-me);color:#fff;border-bottom-right-radius:4px;}
  .msgRow:not(.me) .bubble{background:var(--bubble-them);color:var(--text);border-bottom-left-radius:4px;}
  .msgMeta{font-size:11px;opacity:.7;margin-top:3px;}
  .msgSender{font-size:12px;font-weight:700;margin-bottom:2px;color:var(--primary);}
  .msgRow.me .msgSender{display:none;}
  .system{align-self:center;font-size:12px;color:var(--muted);background:#e5e7eb;padding:4px 10px;
    border-radius:10px;}
  .fileCard{display:flex;align-items:center;gap:10px;background:rgba(0,0,0,.04);border-radius:10px;
    padding:8px 10px;text-decoration:none;color:inherit;}
  .msgRow.me .fileCard{background:rgba(255,255,255,.15);}
  .fileIcon{font-size:20px;}
  .fileName{font-size:13px;font-weight:600;word-break:break-all;}
  .fileHint{font-size:11px;opacity:.75;}
  .chatImg{max-width:220px;max-height:220px;border-radius:10px;display:block;margin-top:2px;cursor:pointer;}

  .inputBar{background:var(--panel);border-top:1px solid var(--border);padding:10px 14px;
    display:flex;align-items:flex-end;gap:8px;flex-shrink:0;}
  .inputBar textarea{flex:1;resize:none;border:1.5px solid var(--border);border-radius:12px;
    padding:10px 14px;font-size:14px;font-family:inherit;max-height:100px;outline:none;min-height:42px;}
  .inputBar textarea:focus{border-color:var(--primary);}
  .roundBtn{width:42px;height:42px;border-radius:50%;border:none;background:var(--primary);color:#fff;
    font-size:18px;cursor:pointer;flex-shrink:0;display:flex;align-items:center;justify-content:center;}
  .roundBtn:hover{background:var(--primary-dark);}
  .roundBtn.secondary{background:#e5e7eb;color:var(--text);}
  #fileInput,#imageInput{display:none;}

  /* ---------- MODAL ---------- */
  .overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);z-index:60;
    align-items:center;justify-content:center;padding:20px;}
  .overlay.show{display:flex;}
  .modal{background:#fff;border-radius:16px;padding:28px;max-width:340px;width:100%;text-align:center;position:relative;}
  .modal h3{margin-bottom:4px;}
  .modal p{color:var(--muted);font-size:13px;margin-bottom:16px;}
  .modal canvas{border:8px solid #fff;box-shadow:0 0 0 1px var(--border);border-radius:8px;margin:0 auto 16px;}
  .modal img.zoomed{max-width:100%;max-height:80vh;border-radius:8px;}
  .credRow{background:var(--bg);border-radius:10px;padding:10px 14px;font-size:13px;margin-bottom:8px;
    display:flex;justify-content:space-between;text-align:left;gap:10px;}
  .credRow b{color:var(--text);word-break:break-all;text-align:right;}
  .closeX{position:absolute;top:14px;right:16px;background:none;border:none;font-size:20px;cursor:pointer;color:var(--muted);}

  /* ---------- MOBILE ---------- */
  .backdrop{display:none;position:fixed;inset:0;background:rgba(0,0,0,.35);z-index:39;}
  .backdrop.show{display:block;}
  @media (max-width:768px){
    .hamburger{display:block;}
    .closeSidebarBtn{display:block;}
    .sidebar{position:fixed;top:0;left:0;bottom:0;z-index:40;transform:translateX(-100%);
      box-shadow:0 0 30px rgba(0,0,0,.2);width:82%;max-width:320px;}
    .sidebar.open{transform:translateX(0);}
    .msgRow{max-width:88%;}
  }
</style>
</head>
<body>

<div id="loginScreen">
  <div class="loginCard">
    <h1>ESP32 Chat Room</h1>
    <p>Enter a display name to join the local chat</p>
    <input id="nameInput" maxlength="24" placeholder="Your name" autocomplete="off">
    <button id="joinBtn">Join Chat</button>
    <div id="loginError"></div>
  </div>
</div>

<div id="app">
  <div class="layout">
    <div class="backdrop" id="sidebarBackdrop"></div>
    <div class="sidebar" id="sidebar">
      <div class="sidebar-header">
        <div class="me">
          <div class="avatar" id="myAvatar"></div>
          <div style="min-width:0;">
            <div style="font-weight:700;font-size:14px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;" id="myNameLabel"></div>
            <div style="font-size:11px;color:var(--muted);">online</div>
          </div>
        </div>
        <button class="closeSidebarBtn" id="closeSidebarBtn" title="Close menu">&times;</button>
      </div>
      <div class="sec">Groups <button class="addBtn" id="addGroupBtn" title="New group">+</button></div>
      <div class="list" id="groupList"></div>
      <div class="sec">Direct Messages</div>
      <div class="list" id="userList"></div>
    </div>

    <div class="main">
      <div class="topbar">
        <button class="hamburger" id="hamburgerBtn">&#9776;</button>
        <div class="statusDot" id="statusDot"></div>
        <div class="roomTitle" id="roomTitle">Main</div>
        <button class="iconBtn" id="inviteBtn">&#128247; Invite</button>
      </div>
      <div class="messages" id="messages"></div>
      <div class="inputBar">
        <button class="roundBtn secondary" id="attachBtn" title="Share a text file">&#128206;</button>
        <button class="roundBtn secondary" id="cameraBtn" title="Share a photo">&#128248;</button>
        <input type="file" id="fileInput" accept=".txt,text/plain">
        <input type="file" id="imageInput" accept="image/*" capture="environment">
        <textarea id="msgInput" rows="1" placeholder="Type a message..."></textarea>
        <button class="roundBtn" id="sendBtn" title="Send">&#10148;</button>
      </div>
    </div>
  </div>
</div>

<div class="overlay" id="inviteOverlay">
  <div class="modal">
    <button class="closeX" id="closeInvite">&times;</button>
    <h3>Invite people</h3>
    <p>Scan to join the WiFi — the chat page should open automatically</p>
    <canvas id="qrCanvas" width="220" height="220"></canvas>
    <div class="credRow"><span>Network</span><b id="ssidLabel">-</b></div>
    <div class="credRow"><span>Password</span><b id="passLabel">-</b></div>
    <div class="credRow"><span>Page</span><b id="urlLabel">-</b></div>
  </div>
</div>

<div class="overlay" id="imgOverlay">
  <div class="modal" style="padding:12px;max-width:90vw;">
    <button class="closeX" id="closeImgOverlay" style="background:#fff;border-radius:50%;">&times;</button>
    <img id="zoomedImg" class="zoomed" src="">
  </div>
</div>

<script>
// ---------------------------------------------------------------
// Minimal offline QR encoder: versions 1-5, EC level L, byte mode,
// fixed mask 0. Enough capacity (~100 bytes) for a WiFi invite string.
// ---------------------------------------------------------------
function makeQR(text) {
  var DATA_CAP = [19, 34, 55, 80, 108];
  var EC_CAP   = [7, 10, 15, 20, 26];
  var ALIGN    = [null, 18, 22, 26, 30];

  var bytes = [];
  for (var i = 0; i < text.length; i++) bytes.push(text.charCodeAt(i) & 0xff);

  var version = -1;
  for (var v = 0; v < 5; v++) {
    var neededBits = 4 + 8 + bytes.length * 8;
    if (neededBits <= DATA_CAP[v] * 8) { version = v + 1; break; }
  }
  if (version === -1) throw new Error("Text too long for QR (max ~104 bytes)");

  var dataCount = DATA_CAP[version - 1];
  var ecCount = EC_CAP[version - 1];

  var bits = [];
  function put(val, len) { for (var i = len - 1; i >= 0; i--) bits.push((val >>> i) & 1); }
  put(0x4, 4);
  put(bytes.length, 8);
  for (var i = 0; i < bytes.length; i++) put(bytes[i], 8);

  var maxBits = dataCount * 8;
  for (var i = 0; i < 4 && bits.length < maxBits; i++) bits.push(0);
  while (bits.length % 8 !== 0) bits.push(0);
  var padAlt = [0xEC, 0x11], pi = 0;
  while (bits.length < maxBits) { put(padAlt[pi % 2], 8); pi++; }

  var dataCodewords = [];
  for (var i = 0; i < bits.length; i += 8) {
    var b = 0;
    for (var j = 0; j < 8; j++) b = (b << 1) | bits[i + j];
    dataCodewords.push(b);
  }

  var EXP = new Array(256), LOG = new Array(256);
  var x = 1;
  for (var i = 0; i < 255; i++) {
    EXP[i] = x; LOG[x] = i; x <<= 1;
    if (x & 0x100) x ^= 0x11D;
  }
  for (var i = 255; i < 512; i++) EXP[i] = EXP[i - 255];
  function gMul(a, b) { if (a === 0 || b === 0) return 0; return EXP[LOG[a] + LOG[b]]; }

  var gen = [1];
  for (var i = 0; i < ecCount; i++) {
    var next = new Array(gen.length + 1).fill(0);
    for (var j = 0; j < gen.length; j++) {
      next[j] ^= gMul(gen[j], 1);
      next[j + 1] ^= gMul(gen[j], EXP[i]);
    }
    gen = next;
  }

  var msg = dataCodewords.concat(new Array(ecCount).fill(0));
  for (var i = 0; i < dataCodewords.length; i++) {
    var coef = msg[i];
    if (coef === 0) continue;
    for (var j = 0; j < gen.length; j++) msg[i + j] ^= gMul(gen[j], coef);
  }
  var ecCodewords = msg.slice(dataCodewords.length);
  var allCodewords = dataCodewords.concat(ecCodewords);

  var allBits = [];
  function put2(byteVal) { for (var i = 7; i >= 0; i--) allBits.push((byteVal >>> i) & 1); }
  for (var i = 0; i < allCodewords.length; i++) put2(allCodewords[i]);

  var REM = [0, 7, 7, 7, 7];
  for (var i = 0; i < REM[version - 1]; i++) allBits.push(0);

  var N = version * 4 + 17;
  var mat = [], reserved = [];
  for (var i = 0; i < N; i++) { mat.push(new Array(N).fill(0)); reserved.push(new Array(N).fill(false)); }
  function setModule(r, c, val) { mat[r][c] = val ? 1 : 0; reserved[r][c] = true; }

  function placeFinder(r, c) {
    for (var i = -1; i <= 7; i++) {
      for (var j = -1; j <= 7; j++) {
        var rr = r + i, cc = c + j;
        if (rr < 0 || rr >= N || cc < 0 || cc >= N) continue;
        var dark = (i >= 0 && i <= 6 && j >= 0 && j <= 6) &&
          (i === 0 || i === 6 || j === 0 || j === 6 || (i >= 2 && i <= 4 && j >= 2 && j <= 4));
        setModule(rr, cc, dark);
      }
    }
  }
  placeFinder(0, 0); placeFinder(0, N - 7); placeFinder(N - 7, 0);

  for (var i = 8; i < N - 8; i++) { setModule(6, i, i % 2 === 0); setModule(i, 6, i % 2 === 0); }

  var ac = ALIGN[version - 1];
  if (ac !== null) {
    for (var i = -2; i <= 2; i++) for (var j = -2; j <= 2; j++) {
      var dark = (i === -2 || i === 2 || j === -2 || j === 2 || (i === 0 && j === 0));
      setModule(ac + i, ac + j, dark);
    }
  }
  setModule(4 * version + 9, 8, true);

  for (var i = 0; i < 9; i++) { reserved[8][i] = true; reserved[i][8] = true; }
  for (var i = 0; i < 8; i++) { reserved[8][N - 1 - i] = true; reserved[N - 1 - i][8] = true; }
  reserved[N - 8][8] = true;

  var bitIndex = 0, dir = -1, col = N - 1;
  while (col > 0) {
    if (col === 6) col--;
    for (var count = 0; count < N; count++) {
      var row = (dir === -1) ? (N - 1 - count) : count;
      for (var cOff = 0; cOff < 2; cOff++) {
        var c = col - cOff;
        if (reserved[row][c]) continue;
        var bitVal = bitIndex < allBits.length ? allBits[bitIndex] : 0;
        bitIndex++;
        var maskBit = ((row + c) % 2 === 0) ? 1 : 0;
        mat[row][c] = bitVal ^ maskBit;
      }
    }
    dir = -dir; col -= 2;
  }

  var G15 = 0x537, G15_MASK = 0x5412;
  var fData = (1 << 3) | 0;
  function bchDigit(d) { var digits = 0; while (d !== 0) { digits++; d >>>= 1; } return digits; }
  var d = fData << 10;
  while (bchDigit(d) - bchDigit(G15) >= 0) d ^= (G15 << (bchDigit(d) - bchDigit(G15)));
  var fBits = ((fData << 10) | d) ^ G15_MASK;

  for (var i = 0; i < 15; i++) {
    var bit = (fBits >> i) & 1;
    if (i < 6) mat[i][8] = bit; else if (i < 8) mat[i + 1][8] = bit; else mat[N - 15 + i][8] = bit;
  }
  for (var i = 0; i < 15; i++) {
    var bit = (fBits >> i) & 1;
    if (i < 8) mat[8][N - 1 - i] = bit;
    else if (i < 9) mat[8][15 - i - 1 + 1] = bit;
    else mat[8][15 - i - 1] = bit;
  }
  return mat;
}
function drawQR(canvas, text) {
  var mat = makeQR(text);
  var N = mat.length;
  var ctx = canvas.getContext('2d');
  var scale = Math.floor(canvas.width / (N + 8));
  var quiet = 4 * scale;
  canvas.width = canvas.height = N * scale + quiet * 2;
  ctx.fillStyle = '#fff'; ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = '#111';
  for (var r = 0; r < N; r++) for (var c = 0; c < N; c++)
    if (mat[r][c]) ctx.fillRect(quiet + c * scale, quiet + r * scale, scale, scale);
}

// ---------------------------------------------------------------
// Chat app
// ---------------------------------------------------------------
let ws, myName = "", currentRoom = "Main";
const rooms = { Main: [] };
const unread = {};
let groups = ["Main"];
let onlineUsers = [];

function colorFor(name){
  let h=0; for(let i=0;i<name.length;i++) h=name.charCodeAt(i)+((h<<5)-h);
  const hue = Math.abs(h)%360;
  return `hsl(${hue},60%,50%)`;
}
function dmRoomId(a,b){ return [a,b].sort().join("|"); }
function isDM(room){ return room.indexOf("|")>-1; }
function otherPartyOf(room){ const p=room.split("|"); return p[0]===myName?p[1]:p[0]; }
function escapeHtml(s){
  return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

document.getElementById('joinBtn').onclick = login;
document.getElementById('nameInput').addEventListener('keydown', e=>{ if(e.key==='Enter') login(); });

function login(){
  const name = document.getElementById('nameInput').value.trim();
  if(!name){ document.getElementById('loginError').textContent = "Please enter a name."; return; }
  myName = name;
  connectWs();
}

function connectWs(){
  const proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
  ws = new WebSocket(proto + location.host + '/ws');
  ws.onopen = () => { ws.send(JSON.stringify({type:'login', name: myName})); };
  ws.onmessage = (e) => handleServerMessage(JSON.parse(e.data));
  ws.onclose = () => {
    document.getElementById('statusDot').style.background = '#ef4444';
    setTimeout(()=>{ if(myName) connectWs(); }, 2000);
  };
}

function handleServerMessage(m){
  if(m.type === 'welcome'){
    document.getElementById('loginScreen').style.display='none';
    document.getElementById('app').style.display='block';
    document.getElementById('myNameLabel').textContent = myName;
    const av = document.getElementById('myAvatar');
    av.textContent = myName[0].toUpperCase();
    av.style.background = colorFor(myName);
    document.getElementById('statusDot').style.background = '#22c55e';
    loadWifiInfo();
  } else if(m.type === 'error'){
    if(document.getElementById('app').style.display==='block'){ alert(m.text); }
    else document.getElementById('loginError').textContent = m.text;
  } else if(m.type === 'users'){
    onlineUsers = m.list.filter(n => n !== myName);
    renderUserList();
  } else if(m.type === 'groups'){
    groups = m.list;
    renderGroupList();
  } else if(m.type === 'system'){
    pushMessage(m.room, {system:true, text:m.text});
  } else if(m.type === 'msg'){
    pushMessage(m.room, {from:m.from, text:m.text, ts:Date.now()});
  } else if(m.type === 'file'){
    pushMessage(m.room, {from:m.from, file:true, name:m.name, data:m.data, ts:Date.now()});
  } else if(m.type === 'image'){
    pushMessage(m.room, {from:m.from, image:true, mime:m.mime, data:m.data, ts:Date.now()});
  }
}

function pushMessage(room, msg){
  if(!rooms[room]) rooms[room] = [];
  rooms[room].push(msg);
  if(room === currentRoom){ renderMessages(); }
  else { unread[room] = (unread[room]||0)+1; renderGroupList(); renderUserList(); }
}

function renderGroupList(){
  const el = document.getElementById('groupList');
  el.innerHTML = '';
  groups.forEach(g=>{
    const div = document.createElement('div');
    div.className = 'item' + (g===currentRoom && !isDM(currentRoom) ? ' active' : '');
    div.innerHTML = `<div class="avatar" style="background:${colorFor(g)}">${g[0].toUpperCase()}</div>
      <div class="name">${escapeHtml(g)}</div>` + (unread[g] ? `<div class="badge">${unread[g]}</div>` : '');
    div.onclick = () => switchRoom(g);
    el.appendChild(div);
  });
}
function renderUserList(){
  const el = document.getElementById('userList');
  el.innerHTML = '';
  if(onlineUsers.length===0){
    el.innerHTML = '<div style="padding:10px 16px;color:var(--muted);font-size:13px;">No one else online</div>';
    return;
  }
  onlineUsers.forEach(u=>{
    const room = dmRoomId(myName, u);
    const div = document.createElement('div');
    div.className = 'item' + (room===currentRoom ? ' active' : '');
    div.innerHTML = `<div class="avatar" style="background:${colorFor(u)}">${u[0].toUpperCase()}</div>
      <div class="name">${escapeHtml(u)}</div><div class="dot"></div>` +
      (unread[room] ? `<div class="badge">${unread[room]}</div>` : '');
    div.onclick = () => switchRoom(room);
    el.appendChild(div);
  });
}

function openSidebar(){
  document.getElementById('sidebar').classList.add('open');
  document.getElementById('sidebarBackdrop').classList.add('show');
}
function closeSidebar(){
  document.getElementById('sidebar').classList.remove('open');
  document.getElementById('sidebarBackdrop').classList.remove('show');
}
document.getElementById('hamburgerBtn').onclick = () => {
  document.getElementById('sidebar').classList.contains('open') ? closeSidebar() : openSidebar();
};
document.getElementById('closeSidebarBtn').onclick = closeSidebar;
document.getElementById('sidebarBackdrop').onclick = closeSidebar;

function switchRoom(room){
  currentRoom = room;
  unread[room] = 0;
  document.getElementById('roomTitle').textContent = isDM(room) ? otherPartyOf(room) : room;
  renderMessages();
  renderGroupList();
  renderUserList();
  closeSidebar();
}
function renderMessages(){
  const el = document.getElementById('messages');
  el.innerHTML = '';
  const list = rooms[currentRoom] || [];
  list.forEach(m=>{
    if(m.system){
      const d = document.createElement('div');
      d.className = 'system'; d.textContent = m.text;
      el.appendChild(d); return;
    }
    const row = document.createElement('div');
    row.className = 'msgRow' + (m.from===myName ? ' me' : '');
    const bubble = document.createElement('div');
    bubble.className = 'bubble';
    let inner = '';
    if(!isDM(currentRoom)) inner += `<div class="msgSender">${escapeHtml(m.from)}</div>`;
    if(m.image){
      inner += `<img class="chatImg" src="data:${m.mime};base64,${m.data}" data-full="data:${m.mime};base64,${m.data}">`;
    } else if(m.file){
      const blob = new Blob([m.data], {type:'text/plain'});
      const url = URL.createObjectURL(blob);
      inner += `<a class="fileCard" href="${url}" download="${escapeHtml(m.name)}">
        <div class="fileIcon">&#128196;</div>
        <div><div class="fileName">${escapeHtml(m.name)}</div><div class="fileHint">Tap to download</div></div>
      </a>`;
    } else {
      inner += `<div>${escapeHtml(m.text)}</div>`;
    }
    inner += `<div class="msgMeta">${new Date(m.ts||Date.now()).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'})}</div>`;
    bubble.innerHTML = inner;
    row.appendChild(bubble);
    el.appendChild(row);
  });
  el.scrollTop = el.scrollHeight;
  el.querySelectorAll('.chatImg').forEach(img=>{
    img.onclick = () => {
      document.getElementById('zoomedImg').src = img.dataset.full;
      document.getElementById('imgOverlay').classList.add('show');
    };
  });
}
document.getElementById('closeImgOverlay').onclick = () => document.getElementById('imgOverlay').classList.remove('show');
document.getElementById('imgOverlay').onclick = (e) => { if(e.target.id==='imgOverlay') e.currentTarget.classList.remove('show'); };

// send text message
document.getElementById('sendBtn').onclick = sendCurrentMessage;
document.getElementById('msgInput').addEventListener('keydown', e=>{
  if(e.key==='Enter' && !e.shiftKey){ e.preventDefault(); sendCurrentMessage(); }
});
function sendCurrentMessage(){
  const input = document.getElementById('msgInput');
  const text = input.value.trim();
  if(!text || !ws || ws.readyState!==1) return;
  ws.send(JSON.stringify({type:'msg', room:currentRoom, text}));
  input.value='';
}

// text file share
document.getElementById('attachBtn').onclick = () => document.getElementById('fileInput').click();
document.getElementById('fileInput').onchange = (e)=>{
  const file = e.target.files[0];
  if(!file) return;
  const reader = new FileReader();
  reader.onload = () => {
    const content = reader.result;
    if(content.length > 4000){ alert('File is too large (max 4000 characters).'); return; }
    ws.send(JSON.stringify({type:'file', room:currentRoom, name:file.name, data:content}));
  };
  reader.readAsText(file);
  e.target.value = '';
};

// camera / photo share — resized & compressed client-side before sending
document.getElementById('cameraBtn').onclick = () => document.getElementById('imageInput').click();
document.getElementById('imageInput').onchange = (e)=>{
  const file = e.target.files[0];
  if(!file) return;
  compressImage(file, (base64) => {
    if(!base64){ alert('Could not compress this image small enough to send. Try a simpler photo.'); return; }
    ws.send(JSON.stringify({type:'image', room:currentRoom, mime:'image/jpeg', data:base64}));
  });
  e.target.value = '';
};
function compressImage(file, cb){
  const reader = new FileReader();
  reader.onload = () => {
    const img = new Image();
    img.onload = () => {
      let w = img.width, h = img.height;
      const maxDim = 480;
      if (w > maxDim || h > maxDim) {
        if (w > h) { h = Math.round(h * maxDim / w); w = maxDim; }
        else { w = Math.round(w * maxDim / h); h = maxDim; }
      }
      const canvas = document.createElement('canvas');
      canvas.width = w; canvas.height = h;
      canvas.getContext('2d').drawImage(img, 0, 0, w, h);
      let quality = 0.6;
      let dataUrl = canvas.toDataURL('image/jpeg', quality);
      while (dataUrl.length > 24000 && quality > 0.15) {
        quality -= 0.1;
        dataUrl = canvas.toDataURL('image/jpeg', quality);
      }
      if (dataUrl.length > 24000) { cb(null); return; }
      cb(dataUrl.split(',')[1]);
    };
    img.onerror = () => cb(null);
    img.src = reader.result;
  };
  reader.onerror = () => cb(null);
  reader.readAsDataURL(file);
}

// new group
document.getElementById('addGroupBtn').onclick = () => {
  const name = prompt('New group name:');
  if(name && name.trim()) ws.send(JSON.stringify({type:'create_group', name:name.trim()}));
};

// invite modal
let wifiInfo = {ssid:'', password:''};
async function loadWifiInfo(){
  try{
    const r = await fetch('/wifiinfo');
    wifiInfo = await r.json();
  }catch(e){}
}
document.getElementById('inviteBtn').onclick = () => {
  const url = location.origin + '/';
  document.getElementById('ssidLabel').textContent = wifiInfo.ssid || '-';
  document.getElementById('passLabel').textContent = wifiInfo.password || '(open network)';
  document.getElementById('urlLabel').textContent = url;
  const qrText = `WIFI:T:WPA;S:${wifiInfo.ssid};P:${wifiInfo.password};;`;
  try{ drawQR(document.getElementById('qrCanvas'), qrText); }
  catch(err){ drawQR(document.getElementById('qrCanvas'), url); }
  document.getElementById('inviteOverlay').classList.add('show');
};
document.getElementById('closeInvite').onclick = () => document.getElementById('inviteOverlay').classList.remove('show');
document.getElementById('inviteOverlay').onclick = (e) => { if(e.target.id==='inviteOverlay') e.currentTarget.classList.remove('show'); };
</script>
</body>
</html>
)INDEXHTML";
// ============================================

void setup() {
  Serial.begin(115200);
  delay(300);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP started. IP: ");
  Serial.println(WiFi.softAPIP());

  // Captive portal: redirect all DNS queries to this device
  dnsServer.start(53, "*", apIP);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/wifiinfo", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc;
    doc["ssid"] = AP_SSID;
    doc["password"] = AP_PASSWORD;
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // --- Captive portal probe endpoints ---
  // These are the URLs iOS / Android / Windows silently ping right after
  // joining a WiFi network to check "is this a captive portal?". Answering
  // them with a redirect (instead of letting them 404) is what makes the
  // "Sign in to network" popup appear automatically after scanning the QR code.
  auto redirectHome = [](AsyncWebServerRequest *request){ request->redirect("/"); };
  server.on("/generate_204", HTTP_GET, redirectHome);          // Android
  server.on("/gen_204", HTTP_GET, redirectHome);                // Android (older)
  server.on("/hotspot-detect.html", HTTP_GET, redirectHome);    // iOS / macOS
  server.on("/library/test/success.html", HTTP_GET, redirectHome); // iOS (older)
  server.on("/connecttest.txt", HTTP_GET, redirectHome);        // Windows
  server.on("/ncsi.txt", HTTP_GET, redirectHome);               // Windows
  server.on("/success.txt", HTTP_GET, redirectHome);            // some Android builds

  // Anything else (unknown paths, direct IP hits) -> chat page
  server.onNotFound(redirectHome);

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  ws.cleanupClients();
}

