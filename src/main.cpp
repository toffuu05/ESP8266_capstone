#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

// ============================================================
//  Config
// ============================================================
namespace cfg {
  const char* const AP_SSID = "ESP8266_Setup";
  const char* const AP_PASS = "12345678";            // 8+ characters
  const unsigned long CONNECT_TIMEOUT_MS = 20000;
  const size_t MIN_PASSWORD_LEN = 8;
}

// ============================================================
//  State
// ============================================================
ESP8266WebServer server(80);

namespace conn {
  bool connecting = false;
  unsigned long startedAt = 0;
  String lastError;
}

// ============================================================
//  Small helpers
// ============================================================
const char* boolStr(bool v) { return v ? "true" : "false"; }

bool isOnline() { return WiFi.status() == WL_CONNECTED; }

String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c >= 0x20) { out += c; }
  }
  return out;
}

void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void sendReply(int code, bool success, const String& message) {
  sendJson(code, String("{\"success\":") + boolStr(success) +
                 ",\"message\":\"" + jsonEscape(message) + "\"}");
}

// ============================================================
//  Wi-Fi logic
// ============================================================
bool validateCredentials(const String& ssid, const String& pass, String& error) {
  if (ssid.length() == 0) {
    error = "Network name is required.";
    return false;
  }
  if (pass.length() > 0 && pass.length() < cfg::MIN_PASSWORD_LEN) {
    error = "Password must be at least 8 characters, or empty for an open network.";
    return false;
  }
  return true;
}

void startConnect(const String& ssid, const String& pass) {
  Serial.printf("Connecting to: %s\n", ssid.c_str());
  conn::lastError = "";
  WiFi.disconnect(false);                  // keep the setup access point running
  WiFi.begin(ssid.c_str(), pass.c_str());
  conn::connecting = true;
  conn::startedAt = millis();
}

void updateConnection() {
  if (!conn::connecting) return;

  if (isOnline()) {
    conn::connecting = false;
    conn::lastError = "";
    Serial.print("Connected. Device IP: ");
    Serial.println(WiFi.localIP());
  } else if (millis() - conn::startedAt > cfg::CONNECT_TIMEOUT_MS) {
    conn::connecting = false;
    conn::lastError = "Could not connect. Check the name and password (2.4 GHz only).";
    WiFi.disconnect(false);
    Serial.println("Connect timeout");
  }
}

// ============================================================
//  JSON builders
// ============================================================
String buildStatusJson() {
  const bool online = isOnline();

  String json = "{";
  json += "\"connected\":";   json += boolStr(online);
  json += ",\"connecting\":"; json += boolStr(conn::connecting);
  json += ",\"ssid\":\"";     if (online) json += jsonEscape(WiFi.SSID());
  json += "\",\"staIP\":\"";  if (online) json += WiFi.localIP().toString();
  json += "\",\"apIP\":\"";   json += WiFi.softAPIP().toString();
  json += "\",\"rssi\":";     json += String(online ? WiFi.RSSI() : 0);
  json += ",\"error\":\"";    json += jsonEscape(conn::lastError);
  json += "\"}";
  return json;
}

String buildScanJson(int count) {
  String json = "{\"scanning\":false,\"networks\":[";
  for (int i = 0; i < count; i++) {
    if (i) json += ",";
    json += "{\"ssid\":\"";  json += jsonEscape(WiFi.SSID(i));
    json += "\",\"rssi\":";  json += String(WiFi.RSSI(i));
    json += ",\"secure\":";  json += boolStr(WiFi.encryptionType(i) != ENC_TYPE_NONE);
    json += "}";
  }
  json += "]}";
  return json;
}

// ============================================================
//  Route handlers
// ============================================================
void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Location", isOnline() ? "/index.html" : "/setup.html");
  server.send(302, "text/plain", "");
}

void handleStatus() {
  sendJson(200, buildStatusJson());
}

void handleScan() {
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_FAILED) {             // no scan yet: start one
    WiFi.scanNetworks(true);               // async, does not block the server
    n = WIFI_SCAN_RUNNING;
  }
  if (n == WIFI_SCAN_RUNNING) {
    sendJson(200, "{\"scanning\":true,\"networks\":[]}");
    return;
  }

  sendJson(200, buildScanJson(n));
  WiFi.scanDelete();
}

void handleConnect() {
  String ssid = server.arg("ssid");
  String pass = server.arg("password");
  ssid.trim();

  String error;
  if (!validateCredentials(ssid, pass, error)) {
    sendReply(400, false, error);
    return;
  }

  startConnect(ssid, pass);
  sendReply(200, true, "Connecting to Wi-Fi...");
}

void handleNotFound() {
  server.send(404, "text/plain", "404: " + server.uri() + " not found");
}

// ============================================================
//  Setup helpers
// ============================================================
void mountFilesystem() {
  if (LittleFS.begin()) {
    Serial.println("LittleFS OK");
  } else {
    Serial.println("LittleFS mount FAILED. Run: Upload Filesystem Image");
  }
}

void startWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(cfg::AP_SSID, cfg::AP_PASS);
  WiFi.begin();                            // reuse saved credentials, if any

  Serial.print("Setup Wi-Fi: ");
  Serial.print(cfg::AP_SSID);
  Serial.print("  http://");
  Serial.println(WiFi.softAPIP());
}

void startServer() {
  server.on("/", HTTP_GET, handleRoot);    // register before serveStatic
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.serveStatic("/", LittleFS, "/", "no-cache");
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started");
}

// ============================================================
//  Arduino entry points
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nStarting...");

  mountFilesystem();
  startWiFi();
  startServer();
}

void loop() {
  server.handleClient();
  updateConnection();
}