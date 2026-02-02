/*
 * EXAMPLE_ADVANCED.ino
 * 
 * Advanced WiFi Manager Implementation
 * Shows integration with sensors, display, and web interface
 * 
 * Hardware Required:
 * - ESP32 or ESP32-C3
 * - OLED Display (SSD1306) - Optional
 * - BME280 Sensor - Optional
 * - LED on GPIO 2 - Optional
 * - Built-in BOOT button (GPIO 0)
 * 
 * Features Demonstrated:
 * - WiFi Manager integration
 * - Display feedback during configuration
 * - Sensor data web server
 * - Automatic reconnection
 * - Status LED indicators
 * - Factory reset functionality
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "WiFiManager_ESP32.h"

// Optional: Uncomment if you have these sensors
// #include <Adafruit_BME280.h>
// #include <Adafruit_SSD1306.h>

// Pin definitions
#define LED_PIN 2

// Create required global objects
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
NetworkConfig netConfig;

// Optional sensor objects
// Adafruit_BME280 bme;
// Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Configuration state variables
bool configMode = false;
unsigned long configStartTime = 0;
unsigned long buttonPressStartTime = 0;
bool buttonPressed = false;
bool resetTriggered = false;

// Application state
unsigned long lastReconnectAttempt = 0;
unsigned long lastDataUpdate = 0;
const unsigned long RECONNECT_INTERVAL = 30000;  // 30 seconds
const unsigned long DATA_UPDATE_INTERVAL = 5000;  // 5 seconds

// Sensor data (simulated if no hardware)
struct SensorData {
  float temperature = 0.0;
  float humidity = 0.0;
  float pressure = 0.0;
} sensorData;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║ WiFi Manager Advanced Example ║");
  Serial.println("╚════════════════════════════════╝\n");
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize reset button
  pinMode(WM_RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize display (if available)
  // if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
  //   display.clearDisplay();
  //   display.setTextSize(1);
  //   display.setTextColor(SSD1306_WHITE);
  //   display.setCursor(0, 0);
  //   display.println("WiFi Manager");
  //   display.println("Starting...");
  //   display.display();
  // }
  
  // Initialize sensor (if available)
  // if (bme.begin(0x76)) {
  //   Serial.println("✓ BME280 sensor found");
  // }
  
  // Load saved WiFi configuration
  loadNetworkConfiguration();
  
  // Try to connect to saved network
  if (strlen(netConfig.ssid) > 0) {
    Serial.println("Attempting to connect to saved network...");
    updateDisplay("Connecting...", netConfig.ssid);
    
    connectToWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✓ WiFi connected successfully!");
      digitalWrite(LED_PIN, HIGH);  // LED on = connected
      
      updateDisplay("Connected!", WiFi.localIP().toString().c_str());
      
      // Setup application web server
      setupApplicationServer();
      server.begin();
      Serial.println("✓ Web server started");
      
    } else {
      Serial.println("✗ Connection failed - entering config mode");
      startConfigMode();
      updateDisplay("Config Mode", "Connect to WiFi");
    }
  } else {
    Serial.println("No saved network - entering config mode");
    startConfigMode();
    updateDisplay("Config Mode", WM_AP_SSID);
  }
  
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║  Setup Complete               ║");
  Serial.println("╚════════════════════════════════╝\n");
}

void loop() {
  // Check for factory reset button press
  checkResetButton();
  
  // Update status LED
  updateStatusLED();
  
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
  
  // Check WiFi connection and attempt reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      Serial.println("⚠ WiFi disconnected - attempting reconnect...");
      updateDisplay("Reconnecting...", "Please wait");
      connectToWiFi();
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✓ Reconnected successfully!");
        updateDisplay("Reconnected!", WiFi.localIP().toString().c_str());
      }
    }
  }
  
  // Update sensor data periodically
  if (millis() - lastDataUpdate > DATA_UPDATE_INTERVAL) {
    lastDataUpdate = millis();
    readSensors();
  }
}

// =============================================================================
// APPLICATION-SPECIFIC FUNCTIONS
// =============================================================================

void setupApplicationServer() {
  // Main page
  server.on("/", HTTP_GET, handleRoot);
  
  // Data API endpoint
  server.on("/data", HTTP_GET, []() {
    String json = "{";
    json += "\"temperature\":" + String(sensorData.temperature, 1) + ",";
    json += "\"humidity\":" + String(sensorData.humidity, 1) + ",";
    json += "\"pressure\":" + String(sensorData.pressure, 1) + ",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    
    server.send(200, "application/json", json);
  });
  
  // Info page
  server.on("/info", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Device Info</title></head><body>";
    html += "<h1>Device Information</h1>";
    html += "<p><strong>SSID:</strong> " + String(netConfig.ssid) + "</p>";
    html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>IP Mode:</strong> " + String(netConfig.useStaticIP ? "Static" : "DHCP") + "</p>";
    html += "<p><strong>MAC Address:</strong> " + WiFi.macAddress() + "</p>";
    html += "<p><strong>Signal Strength:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
    html += "<p><strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</p>";
    html += "<hr><p><a href='/'>Back to Main</a></p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
  });
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Sensor Monitor</title>
<style>
body {
  font-family: Arial, sans-serif;
  max-width: 600px;
  margin: 50px auto;
  padding: 20px;
  background: #f0f0f0;
}
.card {
  background: white;
  padding: 20px;
  border-radius: 10px;
  box-shadow: 0 2px 10px rgba(0,0,0,0.1);
  margin-bottom: 20px;
}
h1 { color: #333; margin-top: 0; }
.sensor-value {
  font-size: 2em;
  font-weight: bold;
  color: #667eea;
  margin: 10px 0;
}
.label { color: #666; font-size: 0.9em; }
button {
  background: #667eea;
  color: white;
  border: none;
  padding: 10px 20px;
  border-radius: 5px;
  cursor: pointer;
  font-size: 1em;
}
button:hover { background: #5568d3; }
</style>
</head>
<body>
<div class="card">
  <h1>🌡️ Sensor Monitor</h1>
  <div>
    <div class="label">Temperature</div>
    <div class="sensor-value" id="temp">--</div>
  </div>
  <div>
    <div class="label">Humidity</div>
    <div class="sensor-value" id="humid">--</div>
  </div>
  <div>
    <div class="label">Pressure</div>
    <div class="sensor-value" id="press">--</div>
  </div>
</div>

<div class="card">
  <h2>📡 Network Info</h2>
  <p><strong>WiFi Signal:</strong> <span id="rssi">--</span> dBm</p>
  <p><strong>Uptime:</strong> <span id="uptime">--</span> seconds</p>
  <button onclick="location.href='/info'">Device Info</button>
</div>

<script>
function updateData() {
  fetch('/data')
    .then(r => r.json())
    .then(data => {
      document.getElementById('temp').textContent = data.temperature.toFixed(1) + '°C';
      document.getElementById('humid').textContent = data.humidity.toFixed(1) + '%';
      document.getElementById('press').textContent = data.pressure.toFixed(1) + ' hPa';
      document.getElementById('rssi').textContent = data.wifi_rssi;
      document.getElementById('uptime').textContent = data.uptime;
    });
}

updateData();
setInterval(updateData, 2000);
</script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void readSensors() {
  // Read from actual sensors if available, otherwise simulate
  // if (bme.begin(0x76)) {
  //   sensorData.temperature = bme.readTemperature();
  //   sensorData.humidity = bme.readHumidity();
  //   sensorData.pressure = bme.readPressure() / 100.0F;
  // } else {
    // Simulated data for testing
    sensorData.temperature = 22.5 + (random(-10, 10) / 10.0);
    sensorData.humidity = 55.0 + (random(-50, 50) / 10.0);
    sensorData.pressure = 1013.25 + (random(-100, 100) / 10.0);
  // }
}

void updateDisplay(const char* line1, const char* line2) {
  // Update OLED display if available
  // display.clearDisplay();
  // display.setCursor(0, 0);
  // display.println(line1);
  // display.println(line2);
  // display.display();
  
  // Also print to serial
  Serial.println(line1);
  Serial.println(line2);
}

void updateStatusLED() {
  if (configMode) {
    // Fast blink in config mode
    digitalWrite(LED_PIN, (millis() / 250) % 2);
  } else if (WiFi.status() == WL_CONNECTED) {
    // Solid on when connected
    digitalWrite(LED_PIN, HIGH);
  } else {
    // Slow blink when disconnected
    digitalWrite(LED_PIN, (millis() / 1000) % 2);
  }
}
