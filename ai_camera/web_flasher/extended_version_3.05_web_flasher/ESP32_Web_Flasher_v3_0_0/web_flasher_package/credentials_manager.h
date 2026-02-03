/*
 * Credentials Manager for ESP32-S3 AI Camera
 * 
 * Provides secure storage of WiFi credentials and OpenAI API key in NVS
 * Includes captive portal setup wizard for first boot
 * Factory reset capability with BOOT button hold
 * 
 * Usage:
 * 1. Include this header in your main .ino file
 * 2. Call CredentialsManager::begin() in setup()
 * 3. Access credentials via getter methods
 * 4. Hold BOOT button for 3 seconds to factory reset
 */

#ifndef CREDENTIALS_MANAGER_H
#define CREDENTIALS_MANAGER_H

#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// BOOT button GPIO (standard on ESP32-S3)
#define BOOT_BUTTON_PIN 0
#define FACTORY_RESET_HOLD_TIME 3000  // 3 seconds

class CredentialsManager {
public:
  static bool begin() {
    Serial.println("\n=== Credentials Manager ===");
    
    // Initialize preferences
    prefs.begin("ai_camera", false);
    
    // Check for factory reset button hold
    checkFactoryReset();
    
    // Check if credentials exist
    if (!hasCredentials()) {
      Serial.println("No credentials found - starting setup wizard");
      return startSetupWizard();
    }
    
    // Load existing credentials
    loadCredentials();
    Serial.println("Credentials loaded from NVS");
    return true;
  }
  
  static void checkFactoryReset() {
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      Serial.println("BOOT button pressed - checking for factory reset...");
      unsigned long pressStart = millis();
      
      while (digitalRead(BOOT_BUTTON_PIN) == LOW && (millis() - pressStart) < FACTORY_RESET_HOLD_TIME) {
        delay(100);
      }
      
      if ((millis() - pressStart) >= FACTORY_RESET_HOLD_TIME) {
        Serial.println("\n*** FACTORY RESET TRIGGERED ***");
        factoryReset();
        ESP.restart();
      }
    }
  }
  
  static bool hasCredentials() {
    return prefs.isKey("wifi_ssid") && 
           prefs.isKey("wifi_pass") && 
           prefs.isKey("openai_key");
  }
  
  static void loadCredentials() {
    wifi_ssid = prefs.getString("wifi_ssid", "");
    wifi_password = prefs.getString("wifi_pass", "");
    openai_api_key = prefs.getString("openai_key", "");
    device_name = prefs.getString("device_name", "AI-Camera");
  }
  
  static void saveCredentials(String ssid, String password, String apiKey, String deviceName = "AI-Camera") {
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", password);
    prefs.putString("openai_key", apiKey);
    prefs.putString("device_name", deviceName);
    Serial.println("Credentials saved to NVS");
  }
  
  static void factoryReset() {
    Serial.println("Clearing all stored credentials...");
    prefs.clear();
    WiFi.disconnect(true, true);  // Clear WiFi settings
    delay(1000);
  }
  
  // Getters
  static String getWiFiSSID() { return wifi_ssid; }
  static String getWiFiPassword() { return wifi_password; }
  static String getOpenAIKey() { return openai_api_key; }
  static String getDeviceName() { return device_name; }
  
  // Setup wizard with captive portal
  static bool startSetupWizard() {
    Serial.println("\n========================================");
    Serial.println("Starting Setup Wizard");
    Serial.println("========================================");
    
    // Create AP
    String apSSID = "AI-Camera-Setup";
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID.c_str());
    
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    
    Serial.println("\nSetup WiFi Access Point Created:");
    Serial.println("  SSID: " + apSSID);
    Serial.println("  IP: " + WiFi.softAPIP().toString());
    Serial.println("  Connect to configure your camera\n");
    
    // Start DNS server for captive portal
    dnsServer = new DNSServer();
    dnsServer->start(53, "*", apIP);
    
    // Start web server
    setupServer = new WebServer(80);
    
    // Serve setup page for all routes (captive portal)
    setupServer->onNotFound([]() {
      setupServer->send(200, "text/html", getSetupHTML());
    });
    
    setupServer->on("/", HTTP_GET, []() {
      setupServer->send(200, "text/html", getSetupHTML());
    });
    
    // Handle configuration submission
    setupServer->on("/configure", HTTP_POST, [&apSSID]() {
      String ssid = setupServer->arg("ssid");
      String password = setupServer->arg("password");
      String apiKey = setupServer->arg("apikey");
      String deviceName = setupServer->arg("devicename");
      
      if (deviceName.length() == 0) deviceName = "AI-Camera";
      
      Serial.println("\n=== Configuration Received ===");
      Serial.println("SSID: " + ssid);
      Serial.println("Password: " + String("*").substring(0, password.length()));
      Serial.println("API Key: " + apiKey.substring(0, 10) + "...");
      Serial.println("Device Name: " + deviceName);
      
      // Validate inputs
      if (ssid.length() == 0 || apiKey.length() < 20) {
        setupServer->send(400, "text/html", getErrorHTML("Invalid configuration. Please check all fields."));
        return;
      }
      
      // Test WiFi connection
      setupServer->send(200, "text/html", getConnectingHTML());
      
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid.c_str(), password.c_str());
      
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connection successful!");
        String ipAddress = WiFi.localIP().toString();
        
        // Save credentials
        saveCredentials(ssid, password, apiKey, deviceName);
        
        // Send success page
        setupServer->send(200, "text/html", getSuccessHTML(ipAddress));
        
        delay(3000);
        configComplete = true;
      } else {
        Serial.println("\nWiFi connection failed!");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apSSID.c_str());
        setupServer->send(200, "text/html", getErrorHTML("WiFi connection failed. Please check SSID and password."));
      }
    });
    
    // Scan for available networks
    setupServer->on("/scan", HTTP_GET, []() {
      Serial.println("Scanning for WiFi networks...");
      int n = WiFi.scanNetworks();
      String json = "[";
      for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        json += "}";
      }
      json += "]";
      setupServer->send(200, "application/json", json);
    });
    
    setupServer->begin();
    
    // Wait for configuration
    configComplete = false;
    unsigned long startTime = millis();
    const unsigned long timeout = 600000; // 10 minute timeout
    
    while (!configComplete && (millis() - startTime) < timeout) {
      dnsServer->processNextRequest();
      setupServer->handleClient();
      delay(10);
    }
    
    // Cleanup
    delete dnsServer;
    delete setupServer;
    
    if (configComplete) {
      Serial.println("\nSetup wizard completed successfully");
      loadCredentials();
      return true;
    } else {
      Serial.println("\nSetup wizard timeout - restarting...");
      ESP.restart();
      return false;
    }
  }

private:
  static Preferences prefs;
  static String wifi_ssid;
  static String wifi_password;
  static String openai_api_key;
  static String device_name;
  static DNSServer* dnsServer;
  static WebServer* setupServer;
  static bool configComplete;
  
  static String getSetupHTML() {
    return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AI Camera Setup</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 40px;
            max-width: 500px;
            width: 100%;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            margin-bottom: 10px;
            font-size: 28px;
            text-align: center;
        }
        .subtitle {
            text-align: center;
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 600;
            font-size: 14px;
        }
        input, select {
            width: 100%;
            padding: 12px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 14px;
            transition: border-color 0.3s;
        }
        input:focus, select:focus {
            outline: none;
            border-color: #667eea;
        }
        .btn {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s;
            margin-top: 10px;
        }
        .btn:hover {
            transform: translateY(-2px);
        }
        .btn-scan {
            background: linear-gradient(135deg, #48bb78 0%, #38a169 100%);
        }
        .help-text {
            font-size: 12px;
            color: #999;
            margin-top: 5px;
        }
        .loader {
            border: 3px solid #f3f3f3;
            border-top: 3px solid #667eea;
            border-radius: 50%;
            width: 30px;
            height: 30px;
            animation: spin 1s linear infinite;
            margin: 10px auto;
            display: none;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .network-list {
            max-height: 200px;
            overflow-y: auto;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            margin-top: 10px;
            display: none;
        }
        .network-item {
            padding: 12px;
            border-bottom: 1px solid #f0f0f0;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .network-item:hover {
            background: #f8f8f8;
        }
        .network-item:last-child {
            border-bottom: none;
        }
        .signal-strength {
            font-size: 12px;
            color: #666;
        }
        .lock-icon {
            color: #999;
            margin-left: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🤖 AI Camera Setup</h1>
        <p class="subtitle">Configure your camera to connect to WiFi and OpenAI</p>
        
        <form id="setupForm" action="/configure" method="POST">
            <div class="form-group">
                <label>Device Name (Optional)</label>
                <input type="text" name="devicename" placeholder="AI-Camera" value="AI-Camera">
                <div class="help-text">Friendly name for your camera</div>
            </div>
            
            <div class="form-group">
                <label>WiFi Network</label>
                <input type="text" id="ssid" name="ssid" placeholder="Enter WiFi network name" required>
                <button type="button" class="btn btn-scan" onclick="scanNetworks()">📡 Scan Networks</button>
                <div class="loader" id="scanLoader"></div>
                <div class="network-list" id="networkList"></div>
            </div>
            
            <div class="form-group">
                <label>WiFi Password</label>
                <input type="password" name="password" placeholder="Enter WiFi password">
                <div class="help-text">Leave blank if network is open</div>
            </div>
            
            <div class="form-group">
                <label>OpenAI API Key</label>
                <input type="text" name="apikey" placeholder="sk-..." required>
                <div class="help-text">Get your key at <a href="https://platform.openai.com/api-keys" target="_blank">platform.openai.com/api-keys</a></div>
            </div>
            
            <button type="submit" class="btn">🚀 Connect & Start Camera</button>
        </form>
    </div>

    <script>
        function scanNetworks() {
            const loader = document.getElementById('scanLoader');
            const networkList = document.getElementById('networkList');
            
            loader.style.display = 'block';
            networkList.style.display = 'none';
            networkList.innerHTML = '';
            
            fetch('/scan')
                .then(res => res.json())
                .then(networks => {
                    loader.style.display = 'none';
                    if (networks.length === 0) {
                        networkList.innerHTML = '<div class="network-item">No networks found</div>';
                    } else {
                        networks.forEach(network => {
                            const item = document.createElement('div');
                            item.className = 'network-item';
                            item.onclick = () => selectNetwork(network.ssid);
                            
                            const signalBars = Math.min(4, Math.floor((network.rssi + 100) / 12.5));
                            const signalIcon = '📶'.repeat(signalBars);
                            
                            item.innerHTML = `
                                <span>${network.ssid} ${network.secure ? '🔒' : ''}</span>
                                <span class="signal-strength">${signalIcon} ${network.rssi}dBm</span>
                            `;
                            networkList.appendChild(item);
                        });
                    }
                    networkList.style.display = 'block';
                })
                .catch(err => {
                    loader.style.display = 'none';
                    alert('Network scan failed');
                });
        }
        
        function selectNetwork(ssid) {
            document.getElementById('ssid').value = ssid;
            document.getElementById('networkList').style.display = 'none';
        }
        
        // Auto-scan on page load
        setTimeout(scanNetworks, 500);
    </script>
</body>
</html>
)rawliteral";
  }
  
  static String getConnectingHTML() {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Connecting...</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 0;
        }
        .container {
            background: white;
            padding: 60px 40px;
            border-radius: 20px;
            text-align: center;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        .spinner {
            border: 8px solid #f3f3f3;
            border-top: 8px solid #667eea;
            border-radius: 50%;
            width: 80px;
            height: 80px;
            animation: spin 1s linear infinite;
            margin: 0 auto 30px;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        h1 { color: #333; margin-bottom: 10px; }
        p { color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <div class="spinner"></div>
        <h1>Connecting to WiFi...</h1>
        <p>Please wait while we connect to your network</p>
    </div>
</body>
</html>
)rawliteral";
  }
  
  static String getSuccessHTML(String ipAddress) {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Setup Complete!</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #48bb78 0%, #38a169 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 0;
        }
        .container {
            background: white;
            padding: 50px 40px;
            border-radius: 20px;
            text-align: center;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 500px;
        }
        .checkmark {
            font-size: 80px;
            margin-bottom: 20px;
        }
        h1 { color: #38a169; margin-bottom: 15px; }
        p { color: #666; margin-bottom: 10px; line-height: 1.6; }
        .ip-box {
            background: #f7fafc;
            padding: 15px;
            border-radius: 8px;
            margin: 20px 0;
            font-family: monospace;
            font-size: 18px;
            font-weight: bold;
            color: #2d3748;
        }
        a {
            display: inline-block;
            margin-top: 20px;
            padding: 12px 30px;
            background: #38a169;
            color: white;
            text-decoration: none;
            border-radius: 8px;
            font-weight: 600;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="checkmark">✅</div>
        <h1>Setup Complete!</h1>
        <p>Your AI Camera is now connected to WiFi.</p>
        <p>The camera will restart in a few seconds.</p>
        <div class="ip-box">)rawliteral" + ipAddress + R"rawliteral(</div>
        <p>After restart, open this address in your browser to use the camera interface.</p>
        <a href="http://)rawliteral" + ipAddress + R"rawliteral(">Open Camera Interface</a>
    </div>
</body>
</html>
)rawliteral";
  }
  
  static String getErrorHTML(String message) {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuration Error</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #e74c3c 0%, #c0392b 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 0;
        }
        .container {
            background: white;
            padding: 50px 40px;
            border-radius: 20px;
            text-align: center;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 500px;
        }
        .error-icon { font-size: 80px; margin-bottom: 20px; }
        h1 { color: #e74c3c; margin-bottom: 15px; }
        p { color: #666; line-height: 1.6; }
        a {
            display: inline-block;
            margin-top: 20px;
            padding: 12px 30px;
            background: #e74c3c;
            color: white;
            text-decoration: none;
            border-radius: 8px;
            font-weight: 600;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="error-icon">❌</div>
        <h1>Configuration Error</h1>
        <p>)rawliteral" + message + R"rawliteral(</p>
        <a href="/">Try Again</a>
    </div>
</body>
</html>
)rawliteral";
  }
};

// Static member initialization
Preferences CredentialsManager::prefs;
String CredentialsManager::wifi_ssid = "";
String CredentialsManager::wifi_password = "";
String CredentialsManager::openai_api_key = "";
String CredentialsManager::device_name = "AI-Camera";
DNSServer* CredentialsManager::dnsServer = nullptr;
WebServer* CredentialsManager::setupServer = nullptr;
bool CredentialsManager::configComplete = false;

#endif // CREDENTIALS_MANAGER_H
