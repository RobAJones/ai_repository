/*
 * WiFiManager_ESP32.cpp
 * 
 * Implementation of WiFi Configuration Manager for ESP32
 * See WiFiManager_ESP32.h for usage details
 */

#include "WiFiManager_ESP32.h"

// =============================================================================
// CONFIGURATION MANAGEMENT
// =============================================================================

void loadNetworkConfiguration() {
  preferences.begin("wifi-config", true);  // Read-only mode
  preferences.getString("ssid", netConfig.ssid, 32);
  preferences.getString("password", netConfig.password, 64);
  netConfig.useStaticIP = preferences.getBool("useStatic", false);
  
  if (netConfig.useStaticIP) {
    uint32_t ip = preferences.getUInt("staticIP", 0);
    uint32_t gw = preferences.getUInt("gateway", 0);
    uint32_t sn = preferences.getUInt("subnet", 0);
    uint32_t d1 = preferences.getUInt("dns1", 0);
    uint32_t d2 = preferences.getUInt("dns2", 0);
    
    netConfig.staticIP = IPAddress(ip);
    netConfig.gateway = IPAddress(gw);
    netConfig.subnet = IPAddress(sn);
    netConfig.dns1 = IPAddress(d1);
    netConfig.dns2 = IPAddress(d2);
  }
  preferences.end();
  
  Serial.println("✓ Network configuration loaded");
  if (strlen(netConfig.ssid) > 0) {
    Serial.print("  SSID: ");
    Serial.println(netConfig.ssid);
    Serial.print("  IP Mode: ");
    Serial.println(netConfig.useStaticIP ? "Static" : "DHCP");
  } else {
    Serial.println("  No saved configuration found");
  }
}

void saveNetworkConfiguration() {
  preferences.begin("wifi-config", false);  // Write mode
  preferences.putString("ssid", netConfig.ssid);
  preferences.putString("password", netConfig.password);
  preferences.putBool("useStatic", netConfig.useStaticIP);
  
  if (netConfig.useStaticIP) {
    preferences.putUInt("staticIP", (uint32_t)netConfig.staticIP);
    preferences.putUInt("gateway", (uint32_t)netConfig.gateway);
    preferences.putUInt("subnet", (uint32_t)netConfig.subnet);
    preferences.putUInt("dns1", (uint32_t)netConfig.dns1);
    preferences.putUInt("dns2", (uint32_t)netConfig.dns2);
  }
  preferences.end();
  
  Serial.println("✓ Network configuration saved");
}

// =============================================================================
// WIFI CONNECTION
// =============================================================================

void connectToWiFi() {
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║  Connecting to WiFi Network   ║");
  Serial.println("╚════════════════════════════════╝");
  Serial.print("SSID: ");
  Serial.println(netConfig.ssid);
  
  WiFi.mode(WIFI_STA);
  
  if (netConfig.useStaticIP) {
    Serial.println("Using Static IP configuration");
    Serial.print("  IP: ");
    Serial.println(netConfig.staticIP);
    Serial.print("  Gateway: ");
    Serial.println(netConfig.gateway);
    Serial.print("  Subnet: ");
    Serial.println(netConfig.subnet);
    
    if (!WiFi.config(netConfig.staticIP, netConfig.gateway, netConfig.subnet, 
                     netConfig.dns1, netConfig.dns2)) {
      Serial.println("⚠ Failed to configure static IP");
    }
  } else {
    Serial.println("Using DHCP (Dynamic IP)");
  }
  
  WiFi.begin(netConfig.ssid, netConfig.password);
  
  unsigned long startTime = millis();
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WM_WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ Connected successfully!");
    Serial.print("  IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("  Subnet: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("  DNS: ");
    Serial.println(WiFi.dnsIP());
    Serial.print("  Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ Connection failed");
  }
}

// =============================================================================
// CONFIGURATION MODE
// =============================================================================

void startConfigMode() {
  configMode = true;
  configStartTime = millis();
  
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║  ENTERING CONFIGURATION MODE  ║");
  Serial.println("╚════════════════════════════════╝");
  Serial.println("Starting Access Point...");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WM_AP_SSID, WM_AP_PASSWORD);
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.println("\n✓ Access Point Started");
  Serial.print("  SSID: ");
  Serial.println(WM_AP_SSID);
  Serial.print("  IP Address: ");
  Serial.println(apIP);
  Serial.println("\nConnect to this network and navigate to:");
  Serial.print("  http://");
  Serial.println(apIP);
  
  // Start DNS server for captive portal
  dnsServer.start(WM_DNS_PORT, "*", apIP);
  Serial.println("✓ DNS server started (captive portal enabled)");
  
  // Setup web server routes
  setupWebServerRoutes();
  server.begin();
  Serial.println("✓ Web server started");
  Serial.println("\nWaiting for configuration...");
}

void factoryReset() {
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║   FACTORY RESET IN PROGRESS   ║");
  Serial.println("╚════════════════════════════════╝");
  
  // Clear preferences
  preferences.begin("wifi-config", false);
  preferences.clear();
  preferences.end();
  
  // Clear memory structure
  memset(netConfig.ssid, 0, sizeof(netConfig.ssid));
  memset(netConfig.password, 0, sizeof(netConfig.password));
  netConfig.useStaticIP = false;
  
  // Disconnect WiFi
  WiFi.disconnect(true, true);
  
  Serial.println("\n✓ All WiFi settings cleared");
  Serial.println("✓ Factory reset complete!");
  Serial.println("\nRestarting in 2 seconds...");
  
  delay(2000);
  ESP.restart();
}

void checkResetButton() {
  bool currentState = (digitalRead(WM_RESET_BUTTON_PIN) == LOW);
  
  if (currentState && !buttonPressed) {
    // Button just pressed
    buttonPressed = true;
    buttonPressStartTime = millis();
    Serial.println("\n⚠ Reset button pressed (hold for 3 seconds)...");
  } 
  else if (currentState && buttonPressed) {
    // Button being held
    unsigned long holdTime = millis() - buttonPressStartTime;
    
    if (holdTime >= WM_RESET_HOLD_TIME && !resetTriggered) {
      resetTriggered = true;
      Serial.println("\n✓ Factory reset triggered!");
      factoryReset();
    }
  } 
  else if (!currentState && buttonPressed) {
    // Button released
    buttonPressed = false;
    resetTriggered = false;
    
    unsigned long holdTime = millis() - buttonPressStartTime;
    if (holdTime < WM_RESET_HOLD_TIME) {
      Serial.println("Reset cancelled (button released too early)");
    }
  }
}

// =============================================================================
// WEB SERVER HANDLERS
// =============================================================================

void setupWebServerRoutes() {
  server.on("/", HTTP_GET, handleConfigRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save-network", HTTP_POST, handleSaveNetwork);
  server.on("/network-status", HTTP_GET, handleNetworkStatus);
  
  // Captive portal - redirect all unknown requests to config page
  server.onNotFound([]() {
    server.send(200, "text/html", getConfigHTML());
  });
}

void handleConfigRoot() {
  server.send(200, "text/html", getConfigHTML());
}

void handleScan() {
  Serial.println("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();
  
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"encryption\":" + String(WiFi.encryptionType(i));
    json += "}";
  }
  json += "]";
  
  Serial.print("Found ");
  Serial.print(n);
  Serial.println(" networks");
  
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleSaveNetwork() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    Serial.println("\n═══════════════════════════════");
    Serial.println("Received network configuration:");
    Serial.println(body);
    Serial.println("═══════════════════════════════");
    
    // Parse SSID
    int ssidStart = body.indexOf("\"ssid\":\"") + 8;
    int ssidEnd = body.indexOf("\"", ssidStart);
    String ssid = body.substring(ssidStart, ssidEnd);
    
    // Parse password
    int passStart = body.indexOf("\"password\":\"") + 12;
    int passEnd = body.indexOf("\"", passStart);
    String password = body.substring(passStart, passEnd);
    
    // Parse static IP flag
    int staticStart = body.indexOf("\"useStaticIP\":") + 14;
    bool useStatic = body.substring(staticStart, staticStart + 4) == "true";
    
    // Update configuration
    ssid.toCharArray(netConfig.ssid, 32);
    password.toCharArray(netConfig.password, 64);
    netConfig.useStaticIP = useStatic;
    
    if (useStatic) {
      // Parse static IP settings
      int ipStart = body.indexOf("\"staticIP\":\"") + 12;
      int ipEnd = body.indexOf("\"", ipStart);
      netConfig.staticIP.fromString(body.substring(ipStart, ipEnd));
      
      int gwStart = body.indexOf("\"gateway\":\"") + 11;
      int gwEnd = body.indexOf("\"", gwStart);
      netConfig.gateway.fromString(body.substring(gwStart, gwEnd));
      
      int snStart = body.indexOf("\"subnet\":\"") + 10;
      int snEnd = body.indexOf("\"", snStart);
      netConfig.subnet.fromString(body.substring(snStart, snEnd));
      
      int dns1Start = body.indexOf("\"dns1\":\"") + 8;
      int dns1End = body.indexOf("\"", dns1Start);
      netConfig.dns1.fromString(body.substring(dns1Start, dns1End));
      
      int dns2Start = body.indexOf("\"dns2\":\"") + 8;
      int dns2End = body.indexOf("\"", dns2Start);
      netConfig.dns2.fromString(body.substring(dns2Start, dns2End));
      
      Serial.println("\nStatic IP Configuration:");
      Serial.print("  IP: ");
      Serial.println(netConfig.staticIP);
      Serial.print("  Gateway: ");
      Serial.println(netConfig.gateway);
    }
    
    // Save configuration
    saveNetworkConfiguration();
    
    server.send(200, "application/json", "{\"success\":true}");
    
    Serial.println("\n✓ Configuration saved successfully");
    Serial.println("Restarting in 1 second...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"No data received\"}");
  }
}

void handleNetworkStatus() {
  String json = "{";
  json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI());
  json += "}";
  
  server.send(200, "application/json", json);
}

// =============================================================================
// CONFIGURATION WEB PAGE
// =============================================================================

String getConfigHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi Configuration</title>
<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: #2d3748;
  min-height: 100vh;
  padding: 20px;
}

.container {
  max-width: 600px;
  margin: 0 auto;
  animation: fadeIn 0.6s ease-out;
}

@keyframes fadeIn {
  from { opacity: 0; transform: translateY(20px); }
  to { opacity: 1; transform: translateY(0); }
}

.header {
  text-align: center;
  margin-bottom: 30px;
  padding: 20px;
}

.logo {
  width: 60px;
  height: 60px;
  margin: 0 auto 15px;
  background: rgba(255,255,255,0.2);
  border-radius: 16px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 30px;
  backdrop-filter: blur(10px);
}

h1 {
  font-size: 2rem;
  font-weight: 700;
  color: white;
  margin-bottom: 8px;
}

.subtitle {
  color: rgba(255,255,255,0.9);
  font-size: 1rem;
}

.card {
  background: #ffffff;
  border-radius: 20px;
  padding: 30px;
  margin-bottom: 20px;
  box-shadow: 0 20px 60px rgba(0,0,0,0.3);
}

.section-title {
  font-size: 1.2rem;
  font-weight: 600;
  margin-bottom: 20px;
  display: flex;
  align-items: center;
  gap: 10px;
  color: #667eea;
}

.form-group {
  margin-bottom: 20px;
}

label {
  display: block;
  margin-bottom: 8px;
  color: #64748b;
  font-size: 0.9rem;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

input, select {
  width: 100%;
  padding: 12px 16px;
  background: #f8f9fa;
  border: 2px solid #e2e8f0;
  border-radius: 12px;
  color: #2d3748;
  font-size: 1rem;
  transition: all 0.3s ease;
}

input:focus, select:focus {
  outline: none;
  border-color: #667eea;
  background: #ffffff;
}

.network-list {
  max-height: 300px;
  overflow-y: auto;
  margin-bottom: 20px;
}

.network-item {
  padding: 14px;
  margin-bottom: 8px;
  background: #f8f9fa;
  border: 2px solid #e2e8f0;
  border-radius: 10px;
  cursor: pointer;
  transition: all 0.3s ease;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.network-item:hover {
  border-color: #667eea;
  transform: translateX(5px);
  background: #ffffff;
}

.network-item.selected {
  border-color: #667eea;
  background: rgba(102, 126, 234, 0.1);
}

.network-name {
  font-weight: 600;
}

.network-strength {
  color: #64748b;
  font-size: 0.9rem;
}

.btn {
  width: 100%;
  padding: 14px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  border: none;
  border-radius: 12px;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn:hover {
  transform: translateY(-2px);
  box-shadow: 0 10px 20px rgba(102, 126, 234, 0.3);
}

.btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.status {
  margin-top: 15px;
  padding: 12px;
  border-radius: 10px;
  text-align: center;
  font-weight: 500;
}

.status-success {
  background: #d4edda;
  color: #155724;
}

.status-error {
  background: #f8d7da;
  color: #721c24;
}

.ip-fields {
  display: none;
  animation: slideDown 0.3s ease;
}

.ip-fields.show {
  display: block;
}

@keyframes slideDown {
  from { opacity: 0; max-height: 0; }
  to { opacity: 1; max-height: 500px; }
}

.checkbox-wrapper {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 15px;
}

.checkbox-wrapper input[type="checkbox"] {
  width: auto;
  margin: 0;
}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="logo">📡</div>
    <h1>WiFi Setup</h1>
    <div class="subtitle">Configure your network connection</div>
  </div>

  <div class="card">
    <div class="section-title">📶 Available Networks</div>
    <button class="btn" onclick="scanNetworks()" style="margin-bottom:15px">Scan Networks</button>
    <div class="network-list" id="networkList"></div>
    
    <div class="section-title" style="margin-top:20px">🔐 Network Configuration</div>
    
    <div class="form-group">
      <label>WiFi Network (SSID)</label>
      <input type="text" id="ssid" placeholder="Enter network name">
    </div>
    
    <div class="form-group">
      <label>Password</label>
      <input type="password" id="password" placeholder="Enter password">
    </div>
    
    <div class="checkbox-wrapper">
      <input type="checkbox" id="useStatic" onchange="toggleStaticIP()">
      <label for="useStatic" style="margin:0; text-transform:none">Use Static IP</label>
    </div>
    
    <div class="ip-fields" id="staticFields">
      <div class="form-group">
        <label>Static IP Address</label>
        <input type="text" id="staticIP" placeholder="192.168.1.100">
      </div>
      
      <div class="form-group">
        <label>Gateway</label>
        <input type="text" id="gateway" placeholder="192.168.1.1">
      </div>
      
      <div class="form-group">
        <label>Subnet Mask</label>
        <input type="text" id="subnet" placeholder="255.255.255.0">
      </div>
      
      <div class="form-group">
        <label>Primary DNS</label>
        <input type="text" id="dns1" placeholder="8.8.8.8">
      </div>
      
      <div class="form-group">
        <label>Secondary DNS</label>
        <input type="text" id="dns2" placeholder="8.8.4.4">
      </div>
    </div>
    
    <button class="btn" onclick="saveNetwork()">💾 Save & Connect</button>
    
    <div id="status"></div>
  </div>
</div>

<script>
function scanNetworks() {
  document.getElementById('networkList').innerHTML = '<div style="text-align:center; padding:20px">Scanning...</div>';
  
  fetch('/scan')
    .then(response => response.json())
    .then(networks => {
      let html = '';
      networks.forEach(network => {
        const strength = network.rssi > -60 ? 'Excellent' : 
                         network.rssi > -70 ? 'Good' : 
                         network.rssi > -80 ? 'Fair' : 'Weak';
        html += `
          <div class="network-item" onclick="selectNetwork('${network.ssid}')">
            <div class="network-name">${network.ssid}</div>
            <div class="network-strength">${strength} (${network.rssi} dBm)</div>
          </div>
        `;
      });
      document.getElementById('networkList').innerHTML = html || '<div style="text-align:center; padding:20px">No networks found</div>';
    })
    .catch(error => {
      document.getElementById('networkList').innerHTML = '<div style="text-align:center; padding:20px; color:#e74c3c">Error scanning networks</div>';
    });
}

function selectNetwork(ssid) {
  document.getElementById('ssid').value = ssid;
  document.querySelectorAll('.network-item').forEach(item => {
    item.classList.remove('selected');
  });
  event.target.closest('.network-item').classList.add('selected');
}

function toggleStaticIP() {
  const staticFields = document.getElementById('staticFields');
  const useStatic = document.getElementById('useStatic').checked;
  staticFields.classList.toggle('show', useStatic);
}

function saveNetwork() {
  const ssid = document.getElementById('ssid').value;
  const password = document.getElementById('password').value;
  const useStatic = document.getElementById('useStatic').checked;
  
  if (!ssid) {
    showStatus('Please enter a network name', 'error');
    return;
  }
  
  const config = {
    ssid: ssid,
    password: password,
    useStaticIP: useStatic
  };
  
  if (useStatic) {
    config.staticIP = document.getElementById('staticIP').value;
    config.gateway = document.getElementById('gateway').value;
    config.subnet = document.getElementById('subnet').value;
    config.dns1 = document.getElementById('dns1').value;
    config.dns2 = document.getElementById('dns2').value;
    
    if (!config.staticIP || !config.gateway || !config.subnet) {
      showStatus('Please fill in all static IP fields', 'error');
      return;
    }
  }
  
  showStatus('Saving configuration...', 'success');
  
  fetch('/save-network', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(config)
  })
  .then(response => response.json())
  .then(data => {
    showStatus('Configuration saved! Device is restarting...', 'success');
    setTimeout(() => {
      showStatus('Please reconnect to your network and navigate to the device IP', 'success');
    }, 3000);
  })
  .catch(error => {
    showStatus('Error saving configuration', 'error');
  });
}

function showStatus(message, type) {
  const status = document.getElementById('status');
  status.textContent = message;
  status.className = 'status status-' + type;
}

window.onload = function() {
  scanNetworks();
};
</script>
</body>
</html>
)rawliteral";
}
