#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

// ===================================
// ACCESS POINT CONFIGURATION
// ===================================

const char* apSSID = "ESP8266_Setup";
const char* apPassword = "12345678";

// ===================================
// WEB SERVER
// ===================================

ESP8266WebServer server(80);

bool connecting = false;
unsigned long connectionStart = 0;
const unsigned long connectionTimeout = 20000;

// ===================================
// ROOT PAGE
// ===================================

void handleRoot() {
    server.sendHeader("Cache-Control", "no-cache");
    if (WiFi.status() == WL_CONNECTED) {
        server.sendHeader("Location", "/index.html");
    } else {
        server.sendHeader("Location", "/setup.html");
    }
    server.send(302, "text/plain", "");
}

// ===================================
// WIFI CONNECTION
// ===================================

void handleConnect() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    if (ssid.length() == 0) {
        server.send(400, "application/json",
                    "{\"success\":false,\"message\":\"SSID is required.\"}");
        return;
    }

    Serial.println();
    Serial.print("Connecting to: ");
    Serial.println(ssid);

    // Keep Access Point active
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(false);
    WiFi.begin(ssid.c_str(), password.c_str());

    connecting = true;
    connectionStart = millis();

    server.send(200, "application/json",
                "{\"success\":true,\"message\":\"Attempting Wi-Fi connection...\"}");
}

// ===================================
// CONNECTION STATUS API
// ===================================

void handleStatus() {
    bool connected = WiFi.status() == WL_CONNECTED;

    String json = "{";
    json += "\"connected\":";
    json += connected ? "true" : "false";
    json += ",\"ssid\":\"";
    json += connected ? WiFi.SSID() : "";
    json += "\",\"staIP\":\"";
    json += connected ? WiFi.localIP().toString() : "Not assigned";
    json += "\",\"apIP\":\"";
    json += WiFi.softAPIP().toString();
    json += "\"}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}

// ===================================
// SETUP
// ===================================

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("Starting ESP8266...");

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed!");
        return;
    }
    Serial.println("LittleFS Mounted Successfully!");

    // Check files are really in flash
    Serial.println(LittleFS.exists("/setup.html") ? "setup.html OK" : "setup.html MISSING");
    Serial.println(LittleFS.exists("/index.html") ? "index.html OK" : "index.html MISSING");

    // Enable AP + STA
    WiFi.mode(WIFI_AP_STA);

    // Create Access Point
    bool apStarted = WiFi.softAP(apSSID, apPassword);
    if (apStarted) {
        Serial.println("Access Point Started!");
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("Access Point Failed!");
    }

    // ===================================
    // REGISTER ROUTES
    // ===================================

    server.on("/", HTTP_GET, handleRoot);
    server.on("/connect", HTTP_POST, handleConnect);
    server.on("/api/status", HTTP_GET, handleStatus);

    // Serve HTML, CSS and JavaScript from flash
    server.serveStatic("/setup.html", LittleFS, "/setup.html");
    server.serveStatic("/index.html", LittleFS, "/index.html");
    server.serveStatic("/style.css", LittleFS, "/style.css");
    server.serveStatic("/script.js", LittleFS, "/script.js");

    // Start server
    server.begin();

    Serial.println("Web Server Started!");
    Serial.println("Open http://192.168.4.1");
}

// ===================================
// LOOP
// ===================================

void loop() {
    server.handleClient();

    if (connecting) {
        // Successful connection
        if (WiFi.status() == WL_CONNECTED) {
            connecting = false;

            Serial.println();
            Serial.println("WiFi Connected Successfully!");
            Serial.print("Connected SSID: ");
            Serial.println(WiFi.SSID());
            Serial.print("STA IP: ");
            Serial.println(WiFi.localIP());
            Serial.print("AP IP: ");
            Serial.println(WiFi.softAPIP());
        }
        // Connection timeout
        else if (millis() - connectionStart >= connectionTimeout) {
            connecting = false;

            Serial.println();
            Serial.println("WiFi Connection Timeout!");
            Serial.println("Check your SSID and password.");
        }
    }
}