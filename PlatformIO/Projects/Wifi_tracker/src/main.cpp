#include <WiFi.h>
#include <WebServer.h>

const char* AP_SSID = "ESP32_Radar";
const char* AP_PASS = "radar1234";

WebServer server(80);

String buildDashboard() {
  int n = WiFi.scanNetworks();
  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 WiFi Radar</title>
<style>
body{font-family:monospace;background:#0d1117;color:#c9d1d9;margin:0;padding:16px}
h1{color:#58a6ff}
.bar-bg{background:#21262d;border-radius:4px;overflow:hidden;height:18px;margin:4px 0}
.bar{height:100%;background:linear-gradient(90deg,#f85149,#e3b341,#3fb950)}
.net{margin-bottom:10px;padding:8px;background:#161b22;border-radius:6px}
.stats{display:flex;gap:20px;flex-wrap:wrap;margin-bottom:20px}
.stat{background:#161b22;padding:10px 16px;border-radius:6px;color:#79c0ff}
</style></head><body>
<h1>📡 ESP32 WiFi Radar</h1>
<div class="stats">
)rawliteral";

  html += "<div class='stat'>Free Heap: " + String(ESP.getFreeHeap()) + " bytes</div>";
  html += "<div class='stat'>Uptime: " + String(millis() / 1000) + " s</div>";
  html += "<div class='stat'>Hall Sensor: " + String(hallRead()) + "</div>";
  html += "<div class='stat'>Networks Found: " + String(n) + "</div>";
  html += "</div>";

  for (int i = 0; i < n; i++) {
    int rssi = WiFi.RSSI(i);
    int pct = constrain(map(rssi, -100, -30, 0, 100), 0, 100);
    html += "<div class='net'>";
    html += "<b>" + WiFi.SSID(i) + "</b> — Ch " + String(WiFi.channel(i));
    html += " — " + String(rssi) + " dBm";
    html += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? " 🔓" : " 🔒");
    html += "<div class='bar-bg'><div class='bar' style='width:" + String(pct) + "%'></div></div>";
    html += "</div>";
  }

  html += "<script>setTimeout(()=>location.reload(),3000);</script></body></html>";
  return html;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);          // AP for hosting page + STA for scanning
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println("=== ESP32 WiFi RADAR ===");
  Serial.print("Connect to WiFi: "); Serial.println(AP_SSID);
  Serial.print("Password: "); Serial.println(AP_PASS);
  Serial.print("Then browse to: http://"); Serial.println(WiFi.softAPIP());

  server.on("/", []() {
    server.send(200, "text/html", buildDashboard());
  });
  server.begin();
}

void loop() {
  server.handleClient();
}