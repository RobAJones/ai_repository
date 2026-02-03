/*
 * ESP32 WiFi Manager - Example Application
 * 
 * This example shows how to integrate the WiFi Manager with your own web application.
 * Once WiFi is configured, it runs a simple web control panel.
 * 
 * Features demonstrated:
 * - LED control via web interface
 * - Temperature display (simulated)
 * - Device status page
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// Pin Configuration
#define LED_PIN 2  // Built-in LED (change if needed)

// Configuration
#define AP_SSID "ESP32-Setup"
#define AP_PASSWORD ""
#define DNS_PORT 53
#define WEB_PORT 80
#define CONFIG_TIMEOUT 300000

// Global objects
WebServer server(WEB_PORT);
DNSServer dnsServer;
Preferences preferences;

// Network configuration
struct NetworkConfig {
  char ssid[32];
  char password[64];
  bool useStaticIP;
  IPAddress staticIP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;
};

NetworkConfig config;
bool configMode = false;
unsigned long configStartTime = 0;
bool ledState = false;

// HTML for configuration page (same as before)
const char SETUP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 WiFi Setup</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&family=Fira+Code:wght@400;500&display=swap');
        * { margin: 0; padding: 0; box-sizing: border-box; }
        :root {
            --primary: #00d4ff;
            --primary-dark: #0099cc;
            --bg-dark: #0a0e1a;
            --bg-card: #141824;
            --text: #e8eaed;
            --text-dim: #9aa0a6;
            --border: rgba(255, 255, 255, 0.1);
        }
        body {
            font-family: 'Outfit', sans-serif;
            background: var(--bg-dark);
            color: var(--text);
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 600px; margin: 0 auto; }
        .header { text-align: center; margin-bottom: 40px; padding: 30px 20px; }
        .logo {
            width: 60px; height: 60px; margin: 0 auto 20px;
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            border-radius: 16px; display: flex; align-items: center;
            justify-content: center; font-size: 30px;
        }
        h1 {
            font-size: 2rem; font-weight: 700; margin-bottom: 10px;
            background: linear-gradient(135deg, var(--primary), #34d399);
            -webkit-background-clip: text; -webkit-text-fill-color: transparent;
        }
        .card {
            background: var(--bg-card); border-radius: 20px;
            padding: 30px; margin-bottom: 20px;
            border: 1px solid var(--border);
        }
        .btn {
            width: 100%; padding: 16px;
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            border: none; border-radius: 12px; color: white;
            font-size: 1.1rem; font-weight: 700; cursor: pointer;
        }
        input { width: 100%; padding: 14px; background: var(--bg-dark);
            border: 2px solid var(--border); border-radius: 12px;
            color: var(--text); font-family: 'Fira Code', monospace; margin: 10px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="logo">📡</div>
            <h1>ESP32 WiFi Setup</h1>
        </div>
        <div class="card">
            <h2>Quick Setup</h2>
            <input type="text" id="ssid" placeholder="WiFi Network Name">
            <input type="password" id="password" placeholder="WiFi Password">
            <button class="btn" onclick="connect()">Connect</button>
        </div>
    </div>
    <script>
        function connect() {
            const ssid = document.getElementById('ssid').value;
            const pass = document.getElementById('password').value;
            if (!ssid) { alert('Enter network name'); return; }
            fetch('/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ssid: ssid, password: pass, useStaticIP: false})
            }).then(r => r.json()).then(d => {
                alert(d.success ? 'Connected! IP: ' + d.ip : 'Failed: ' + d.message);
            });
        }
    </script>
</body>
</html>
)rawliteral";

// HTML for main application page
const char APP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Control Panel</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@400;600;700&display=swap');
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Outfit', sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .panel {
            background: white;
            border-radius: 30px;
            padding: 40px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 {
            text-align: center;
            color: #667eea;
            margin-bottom: 30px;
            font-size: 2rem;
        }
        .status-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 20px;
            margin-bottom: 20px;
        }
        .status-item {
            display: flex;
            justify-content: space-between;
            margin: 10px 0;
            font-size: 1.1rem;
        }
        .control-section {
            margin: 30px 0;
        }
        .control-label {
            font-size: 1.2rem;
            font-weight: 600;
            margin-bottom: 15px;
            color: #333;
        }
        .led-control {
            display: flex;
            gap: 15px;
            justify-content: center;
        }
        .led-btn {
            flex: 1;
            padding: 20px;
            border: none;
            border-radius: 15px;
            font-size: 1.1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            font-family: 'Outfit', sans-serif;
        }
        .led-on {
            background: linear-gradient(135deg, #34d399, #10b981);
            color: white;
        }
        .led-off {
            background: linear-gradient(135deg, #ef4444, #dc2626);
            color: white;
        }
        .led-btn:hover {
            transform: translateY(-3px);
            box-shadow: 0 10px 20px rgba(0,0,0,0.2);
        }
        .temperature {
            text-align: center;
            padding: 30px;
            background: #f8f9fa;
            border-radius: 20px;
            margin: 20px 0;
        }
        .temp-value {
            font-size: 4rem;
            font-weight: 700;
            color: #667eea;
        }
        .temp-label {
            font-size: 1.2rem;
            color: #666;
            margin-top: 10px;
        }
        .footer {
            text-align: center;
            margin-top: 30px;
            color: #999;
            font-size: 0.9rem;
        }
    </style>
</head>
<body>
    <div class="panel">
        <h1>🎛️ ESP32 Control Panel</h1>
        
        <div class="status-card">
            <div class="status-item">
                <span>Device:</span>
                <span><strong>ESP32</strong></span>
            </div>
            <div class="status-item">
                <span>Status:</span>
                <span><strong>Online ✓</strong></span>
            </div>
            <div class="status-item">
                <span>IP Address:</span>
                <span id="ipAddress"><strong>Loading...</strong></span>
            </div>
        </div>
        
        <div class="temperature">
            <div class="temp-value" id="temperature">--</div>
            <div class="temp-label">Temperature (°C)</div>
        </div>
        
        <div class="control-section">
            <div class="control-label">LED Control</div>
            <div class="led-control">
                <button class="led-btn led-on" onclick="setLED(true)">
                    💡 Turn ON
                </button>
                <button class="led-btn led-off" onclick="setLED(false)">
                    ⚫ Turn OFF
                </button>
            </div>
        </div>
        
        <div class="footer">
            <p>ESP32 WiFi Manager Example</p>
        </div>
    </div>
    
    <script>
        function updateData() {
            fetch('/api/status')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('temperature').textContent = 
                        data.temperature.toFixed(1);
                    document.getElementById('ipAddress').innerHTML = 
                        '<strong>' + data.ip + '</strong>';
                });
        }
        
        function setLED(state) {
            fetch('/api/led', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({state: state})
            }).then(r => r.json()).then(data => {
                console.log('LED state:', data.state);
            });
        }
        
        // Update data every 2 seconds
        updateData();
        setInterval(updateData, 2000);
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Setup LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("\n\n=== ESP32 WiFi Manager + Application ===");
  
  preferences.begin("wifi-config", false);
  loadConfiguration();
  
  if (strlen(config.ssid) > 0) {
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      startConfigMode();
    } else {
      setupApplicationServer();
    }
  } else {
    startConfigMode();
  }
}

void loop() {
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (millis() - configStartTime > CONFIG_TIMEOUT) {
      Serial.println("Timeout, restarting...");
      ESP.restart();
    }
  } else {
    // Application is running
    server.handleClient();
    
    // Your application logic here
    // Example: Blink LED if it's on
    static unsigned long lastBlink = 0;
    if (ledState && millis() - lastBlink > 1000) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      lastBlink = millis();
    }
  }
  
  delay(10);
}

void startConfigMode() {
  configMode = true;
  configStartTime = millis();
  
  Serial.println("\n=== Configuration Mode ===");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.println("AP IP: " + apIP.toString());
  Serial.println("Connect to: " + String(AP_SSID));
  
  dnsServer.start(DNS_PORT, "*", apIP);
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Config server started");
}

void setupApplicationServer() {
  Serial.println("\n=== Starting Application ===");
  
  // Setup application routes
  server.on("/", HTTP_GET, handleAppRoot);
  server.on("/api/status", HTTP_GET, handleAPIStatus);
  server.on("/api/led", HTTP_POST, handleAPILED);
  
  server.begin();
  Serial.println("Application server started");
  Serial.println("Access at: http://" + WiFi.localIP().toString());
}

void connectToWiFi() {
  Serial.println("\n=== Connecting to WiFi ===");
  Serial.println("SSID: " + String(config.ssid));
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  if (config.useStaticIP) {
    WiFi.config(config.staticIP, config.gateway, config.subnet, 
                config.dns1, config.dns2);
  }
  
  WiFi.begin(config.ssid, config.password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ Connected!");
    Serial.println("IP: " + WiFi.localIP().toString());
    configMode = false;
  } else {
    Serial.println("\n✗ Connection Failed");
  }
}

void saveConfiguration() {
  preferences.putString("ssid", config.ssid);
  preferences.putString("password", config.password);
  preferences.putBool("useStaticIP", config.useStaticIP);
  
  if (config.useStaticIP) {
    preferences.putUInt("staticIP", (uint32_t)config.staticIP);
    preferences.putUInt("gateway", (uint32_t)config.gateway);
    preferences.putUInt("subnet", (uint32_t)config.subnet);
    preferences.putUInt("dns1", (uint32_t)config.dns1);
    preferences.putUInt("dns2", (uint32_t)config.dns2);
  }
}

void loadConfiguration() {
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  
  ssid.toCharArray(config.ssid, 32);
  password.toCharArray(config.password, 64);
  
  config.useStaticIP = preferences.getBool("useStaticIP", false);
  
  if (config.useStaticIP) {
    config.staticIP = preferences.getUInt("staticIP", 0);
    config.gateway = preferences.getUInt("gateway", 0);
    config.subnet = preferences.getUInt("subnet", 0);
    config.dns1 = preferences.getUInt("dns1", 0);
    config.dns2 = preferences.getUInt("dns2", 0);
  }
}

// Configuration handlers
void handleRoot() {
  server.send(200, "text/html", SETUP_PAGE);
}

void handleSave() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Simple JSON parsing
    int ssidStart = body.indexOf("\"ssid\":\"") + 8;
    int ssidEnd = body.indexOf("\"", ssidStart);
    String ssid = body.substring(ssidStart, ssidEnd);
    
    int passStart = body.indexOf("\"password\":\"") + 12;
    int passEnd = body.indexOf("\"", passStart);
    String password = body.substring(passStart, passEnd);
    
    ssid.toCharArray(config.ssid, 32);
    password.toCharArray(config.password, 64);
    config.useStaticIP = false;
    
    saveConfiguration();
    connectToWiFi();
    
    String response;
    if (WiFi.status() == WL_CONNECTED) {
      response = "{\"success\":true,\"ip\":\"" + WiFi.localIP().toString() + "\"}";
      server.send(200, "application/json", response);
      delay(2000);
      ESP.restart();
    } else {
      response = "{\"success\":false,\"message\":\"Connection failed\"}";
      server.send(200, "application/json", response);
    }
  }
}

void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// Application handlers
void handleAppRoot() {
  server.send(200, "text/html", APP_PAGE);
}

void handleAPIStatus() {
  // Simulate temperature reading (replace with real sensor)
  float temp = 20.0 + random(0, 100) / 10.0;
  
  String json = "{";
  json += "\"temperature\":" + String(temp, 1) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"ledState\":" + String(ledState ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleAPILED() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Parse LED state
    int stateIdx = body.indexOf("\"state\":");
    if (stateIdx != -1) {
      ledState = body.substring(stateIdx + 8, stateIdx + 12) == "true";
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      
      String response = "{\"success\":true,\"state\":" + 
                        String(ledState ? "true" : "false") + "}";
      server.send(200, "application/json", response);
      return;
    }
  }
  
  server.send(400, "application/json", "{\"success\":false}");
}
