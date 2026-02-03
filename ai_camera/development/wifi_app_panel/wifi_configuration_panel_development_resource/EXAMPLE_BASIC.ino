/*
 * EXAMPLE_BASIC.ino
 * 
 * Minimal WiFi Manager Implementation
 * Demonstrates core functionality with minimal code
 * 
 * Hardware Required:
 * - ESP32 or ESP32-C3
 * - Built-in BOOT button (GPIO 0)
 * 
 * Usage:
 * 1. Upload this sketch
 * 2. Open Serial Monitor (115200 baud)
 * 3. Device will create "ESP32-Setup" access point
 * 4. Connect to AP and configure WiFi
 * 5. Hold BOOT button 3 seconds to factory reset
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "WiFiManager_ESP32.h"

// Create required global objects
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
NetworkConfig netConfig;

// Configuration state variables
bool configMode = false;
unsigned long configStartTime = 0;
unsigned long buttonPressStartTime = 0;
bool buttonPressed = false;
bool resetTriggered = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║  WiFi Manager Basic Example   ║");
  Serial.println("╚════════════════════════════════╝\n");
  
  // Initialize reset button
  pinMode(WM_RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // Load saved WiFi configuration
  loadNetworkConfiguration();
  
  // Try to connect to saved network
  if (strlen(netConfig.ssid) > 0) {
    Serial.println("Attempting to connect to saved network...");
    connectToWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✓ WiFi connected successfully!");
      Serial.print("✓ IP Address: ");
      Serial.println(WiFi.localIP());
      
      // Setup basic web server
      server.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>ESP32 WiFi Manager</title></head><body>";
        html += "<h1>WiFi Manager Active!</h1>";
        html += "<p><strong>Device:</strong> ESP32</p>";
        html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
        html += "<p><strong>SSID:</strong> " + String(netConfig.ssid) + "</p>";
        html += "<p><strong>Signal:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
        html += "<p><strong>IP Mode:</strong> " + String(netConfig.useStaticIP ? "Static" : "DHCP") + "</p>";
        html += "<hr><p><em>Hold BOOT button 3 seconds to reset WiFi settings</em></p>";
        html += "</body></html>";
        
        server.send(200, "text/html", html);
      });
      
      server.begin();
      Serial.println("✓ Web server started");
      Serial.println("\n╔════════════════════════════════╗");
      Serial.println("║  Setup Complete - Ready!       ║");
      Serial.println("╚════════════════════════════════╝\n");
      
    } else {
      Serial.println("✗ Connection failed - entering config mode");
      startConfigMode();
    }
  } else {
    Serial.println("No saved network found - entering config mode");
    startConfigMode();
  }
}

void loop() {
  // Check for factory reset button press
  checkResetButton();
  
  // Handle configuration mode
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Auto-exit config mode after timeout
    if (millis() - configStartTime > WM_CONFIG_TIMEOUT) {
      Serial.println("Config mode timeout - restarting...");
      ESP.restart();
    }
    return;
  }
  
  // Normal operation mode
  server.handleClient();
  
  // Optional: Monitor WiFi connection
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 30000) {  // Check every 30 seconds
    lastCheck = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("✓ WiFi OK - IP: ");
      Serial.print(WiFi.localIP());
      Serial.print(" | Signal: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("⚠ WiFi disconnected - attempting reconnect...");
      connectToWiFi();
    }
  }
}
