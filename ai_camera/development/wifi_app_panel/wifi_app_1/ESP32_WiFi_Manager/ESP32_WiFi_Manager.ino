/*
 * ESP32 WiFi Configuration Manager
 * 
 * Features:
 * - Captive Portal for easy setup
 * - WiFi network scanning
 * - EEPROM-based configuration storage
 * - Static IP or DHCP selection
 * - Beautiful, beginner-friendly web interface
 * 
 * Required Libraries:
 * - WiFi (built-in)
 * - WebServer (built-in)
 * - DNSServer (built-in)
 * - EEPROM (built-in)
 * - Preferences (built-in, better than EEPROM)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// Configuration
#define AP_SSID "ESP32-Setup"
#define AP_PASSWORD ""  // Open network for easy access
#define DNS_PORT 53
#define WEB_PORT 80
#define CONFIG_TIMEOUT 300000  // 5 minutes in AP mode before retry

// Global objects
WebServer server(WEB_PORT);
DNSServer dnsServer;
Preferences preferences;

// Network configuration structure
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

// HTML/CSS/JS for the configuration page
const char SETUP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 WiFi Setup</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&family=Fira+Code:wght@400;500&display=swap');
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        :root {
            --primary: #00d4ff;
            --primary-dark: #0099cc;
            --bg-dark: #0a0e1a;
            --bg-card: #141824;
            --text: #e8eaed;
            --text-dim: #9aa0a6;
            --success: #34d399;
            --warning: #fbbf24;
            --error: #ef4444;
            --border: rgba(255, 255, 255, 0.1);
        }
        
        body {
            font-family: 'Outfit', sans-serif;
            background: var(--bg-dark);
            color: var(--text);
            min-height: 100vh;
            padding: 20px;
            background-image: 
                radial-gradient(circle at 20% 50%, rgba(0, 212, 255, 0.08) 0%, transparent 50%),
                radial-gradient(circle at 80% 80%, rgba(52, 211, 153, 0.06) 0%, transparent 50%);
        }
        
        .container {
            max-width: 600px;
            margin: 0 auto;
            animation: fadeIn 0.6s ease-out;
        }
        
        @keyframes fadeIn {
            from {
                opacity: 0;
                transform: translateY(20px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }
        
        .header {
            text-align: center;
            margin-bottom: 40px;
            padding: 30px 20px;
        }
        
        .logo {
            width: 60px;
            height: 60px;
            margin: 0 auto 20px;
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            border-radius: 16px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 30px;
            animation: pulse 2s ease-in-out infinite;
        }
        
        @keyframes pulse {
            0%, 100% {
                box-shadow: 0 0 0 0 rgba(0, 212, 255, 0.7);
            }
            50% {
                box-shadow: 0 0 0 15px rgba(0, 212, 255, 0);
            }
        }
        
        h1 {
            font-size: 2rem;
            font-weight: 700;
            margin-bottom: 10px;
            background: linear-gradient(135deg, var(--primary), #34d399);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }
        
        .subtitle {
            color: var(--text-dim);
            font-size: 1rem;
        }
        
        .card {
            background: var(--bg-card);
            border-radius: 20px;
            padding: 30px;
            margin-bottom: 20px;
            border: 1px solid var(--border);
            animation: slideUp 0.6s ease-out;
            animation-delay: 0.2s;
            animation-fill-mode: backwards;
        }
        
        @keyframes slideUp {
            from {
                opacity: 0;
                transform: translateY(30px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }
        
        .section-title {
            font-size: 1.2rem;
            font-weight: 600;
            margin-bottom: 20px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .section-icon {
            font-size: 1.5rem;
        }
        
        .form-group {
            margin-bottom: 24px;
        }
        
        label {
            display: block;
            margin-bottom: 8px;
            color: var(--text-dim);
            font-size: 0.9rem;
            font-weight: 500;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        
        input, select {
            width: 100%;
            padding: 14px 16px;
            background: var(--bg-dark);
            border: 2px solid var(--border);
            border-radius: 12px;
            color: var(--text);
            font-size: 1rem;
            font-family: 'Fira Code', monospace;
            transition: all 0.3s ease;
        }
        
        input:focus, select:focus {
            outline: none;
            border-color: var(--primary);
            box-shadow: 0 0 0 4px rgba(0, 212, 255, 0.1);
        }
        
        .network-list {
            max-height: 300px;
            overflow-y: auto;
            margin-bottom: 20px;
            padding: 10px;
            background: var(--bg-dark);
            border-radius: 12px;
        }
        
        .network-item {
            padding: 16px;
            margin-bottom: 8px;
            background: var(--bg-card);
            border: 2px solid var(--border);
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s ease;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .network-item:hover {
            border-color: var(--primary);
            transform: translateX(5px);
        }
        
        .network-item.selected {
            border-color: var(--primary);
            background: rgba(0, 212, 255, 0.1);
        }
        
        .network-name {
            font-weight: 600;
            font-family: 'Fira Code', monospace;
        }
        
        .network-strength {
            display: flex;
            align-items: center;
            gap: 8px;
            color: var(--text-dim);
        }
        
        .signal-bars {
            display: flex;
            gap: 2px;
            align-items: flex-end;
        }
        
        .signal-bar {
            width: 4px;
            background: var(--text-dim);
            border-radius: 2px;
        }
        
        .toggle-group {
            display: flex;
            gap: 10px;
            margin-bottom: 20px;
        }
        
        .toggle-btn {
            flex: 1;
            padding: 14px;
            background: var(--bg-dark);
            border: 2px solid var(--border);
            border-radius: 12px;
            color: var(--text);
            cursor: pointer;
            transition: all 0.3s ease;
            font-weight: 600;
            text-align: center;
        }
        
        .toggle-btn:hover {
            border-color: var(--primary);
        }
        
        .toggle-btn.active {
            background: var(--primary);
            border-color: var(--primary);
            color: var(--bg-dark);
        }
        
        .static-ip-fields {
            display: none;
            animation: slideDown 0.3s ease-out;
        }
        
        .static-ip-fields.show {
            display: block;
        }
        
        @keyframes slideDown {
            from {
                opacity: 0;
                max-height: 0;
            }
            to {
                opacity: 1;
                max-height: 500px;
            }
        }
        
        .btn {
            width: 100%;
            padding: 16px;
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            border: none;
            border-radius: 12px;
            color: white;
            font-size: 1.1rem;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(0, 212, 255, 0.3);
        }
        
        .btn:active {
            transform: translateY(0);
        }
        
        .btn-secondary {
            background: var(--bg-dark);
            border: 2px solid var(--border);
            margin-top: 10px;
        }
        
        .btn-secondary:hover {
            border-color: var(--primary);
            box-shadow: none;
        }
        
        .status-message {
            padding: 16px;
            border-radius: 12px;
            margin-bottom: 20px;
            display: none;
            animation: slideDown 0.3s ease-out;
        }
        
        .status-message.show {
            display: block;
        }
        
        .status-message.success {
            background: rgba(52, 211, 153, 0.1);
            border: 2px solid var(--success);
            color: var(--success);
        }
        
        .status-message.error {
            background: rgba(239, 68, 68, 0.1);
            border: 2px solid var(--error);
            color: var(--error);
        }
        
        .loading {
            display: none;
            text-align: center;
            padding: 20px;
        }
        
        .loading.show {
            display: block;
        }
        
        .spinner {
            width: 40px;
            height: 40px;
            margin: 0 auto 10px;
            border: 4px solid var(--border);
            border-top-color: var(--primary);
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        
        @keyframes spin {
            to {
                transform: rotate(360deg);
            }
        }
        
        .footer {
            text-align: center;
            margin-top: 40px;
            color: var(--text-dim);
            font-size: 0.9rem;
        }
        
        @media (max-width: 640px) {
            .container {
                padding: 10px;
            }
            
            .card {
                padding: 20px;
            }
            
            h1 {
                font-size: 1.5rem;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="logo">📡</div>
            <h1>ESP32 WiFi Setup</h1>
            <p class="subtitle">Connect your device to your network</p>
        </div>
        
        <div id="statusMessage" class="status-message"></div>
        
        <div class="card">
            <div class="section-title">
                <span class="section-icon">🔍</span>
                Select Network
            </div>
            
            <button class="btn-secondary btn" onclick="scanNetworks()">Scan for Networks</button>
            
            <div id="loading" class="loading">
                <div class="spinner"></div>
                <p>Scanning for networks...</p>
            </div>
            
            <div id="networkList" class="network-list"></div>
            
            <div class="form-group">
                <label for="ssid">Network Name (SSID)</label>
                <input type="text" id="ssid" placeholder="Enter WiFi network name" readonly>
            </div>
            
            <div class="form-group">
                <label for="password">Network Password</label>
                <input type="password" id="password" placeholder="Enter WiFi password">
            </div>
        </div>
        
        <div class="card">
            <div class="section-title">
                <span class="section-icon">🌐</span>
                IP Configuration
            </div>
            
            <div class="toggle-group">
                <div class="toggle-btn active" id="dhcpBtn" onclick="selectIPMode('dhcp')">
                    DHCP (Automatic)
                </div>
                <div class="toggle-btn" id="staticBtn" onclick="selectIPMode('static')">
                    Static IP
                </div>
            </div>
            
            <div id="staticIPFields" class="static-ip-fields">
                <div class="form-group">
                    <label for="staticIP">IP Address</label>
                    <input type="text" id="staticIP" placeholder="192.168.1.100">
                </div>
                
                <div class="form-group">
                    <label for="gateway">Gateway</label>
                    <input type="text" id="gateway" placeholder="192.168.1.1">
                </div>
                
                <div class="form-group">
                    <label for="subnet">Subnet Mask</label>
                    <input type="text" id="subnet" placeholder="255.255.255.0">
                </div>
                
                <div class="form-group">
                    <label for="dns1">Primary DNS</label>
                    <input type="text" id="dns1" placeholder="8.8.8.8">
                </div>
                
                <div class="form-group">
                    <label for="dns2">Secondary DNS</label>
                    <input type="text" id="dns2" placeholder="8.8.4.4">
                </div>
            </div>
        </div>
        
        <button class="btn" onclick="saveConfig()">Connect to Network</button>
        
        <div class="footer">
            <p>ESP32 WiFi Manager v1.0</p>
        </div>
    </div>
    
    <script>
        let selectedSSID = '';
        let ipMode = 'dhcp';
        
        // Scan for networks on page load
        window.addEventListener('load', () => {
            scanNetworks();
        });
        
        function showStatus(message, type) {
            const status = document.getElementById('statusMessage');
            status.textContent = message;
            status.className = 'status-message show ' + type;
            setTimeout(() => {
                status.className = 'status-message';
            }, 5000);
        }
        
        function scanNetworks() {
            document.getElementById('loading').className = 'loading show';
            document.getElementById('networkList').innerHTML = '';
            
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('loading').className = 'loading';
                    displayNetworks(data.networks);
                })
                .catch(error => {
                    document.getElementById('loading').className = 'loading';
                    showStatus('Failed to scan networks', 'error');
                });
        }
        
        function displayNetworks(networks) {
            const list = document.getElementById('networkList');
            list.innerHTML = '';
            
            if (networks.length === 0) {
                list.innerHTML = '<p style="text-align: center; padding: 20px; color: var(--text-dim);">No networks found</p>';
                return;
            }
            
            networks.forEach(network => {
                const item = document.createElement('div');
                item.className = 'network-item';
                item.onclick = () => selectNetwork(network.ssid);
                
                const strength = getSignalStrength(network.rssi);
                const bars = generateSignalBars(strength);
                
                item.innerHTML = `
                    <div class="network-name">${network.ssid}</div>
                    <div class="network-strength">
                        ${network.encryption ? '🔒' : '🔓'}
                        ${bars}
                        <span>${network.rssi} dBm</span>
                    </div>
                `;
                
                list.appendChild(item);
            });
        }
        
        function getSignalStrength(rssi) {
            if (rssi >= -50) return 4;
            if (rssi >= -60) return 3;
            if (rssi >= -70) return 2;
            return 1;
        }
        
        function generateSignalBars(strength) {
            const heights = ['8px', '12px', '16px', '20px'];
            let html = '<div class="signal-bars">';
            for (let i = 0; i < 4; i++) {
                const active = i < strength ? 'var(--primary)' : 'var(--border)';
                html += `<div class="signal-bar" style="height: ${heights[i]}; background: ${active};"></div>`;
            }
            html += '</div>';
            return html;
        }
        
        function selectNetwork(ssid) {
            selectedSSID = ssid;
            document.getElementById('ssid').value = ssid;
            
            // Update UI
            document.querySelectorAll('.network-item').forEach(item => {
                item.classList.remove('selected');
                if (item.querySelector('.network-name').textContent === ssid) {
                    item.classList.add('selected');
                }
            });
            
            // Focus password field
            document.getElementById('password').focus();
        }
        
        function selectIPMode(mode) {
            ipMode = mode;
            
            // Update buttons
            document.getElementById('dhcpBtn').classList.toggle('active', mode === 'dhcp');
            document.getElementById('staticBtn').classList.toggle('active', mode === 'static');
            
            // Show/hide static IP fields
            const fields = document.getElementById('staticIPFields');
            if (mode === 'static') {
                fields.classList.add('show');
            } else {
                fields.classList.remove('show');
            }
        }
        
        function saveConfig() {
            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            
            if (!ssid) {
                showStatus('Please select a network', 'error');
                return;
            }
            
            const config = {
                ssid: ssid,
                password: password,
                useStaticIP: ipMode === 'static'
            };
            
            if (ipMode === 'static') {
                config.staticIP = document.getElementById('staticIP').value;
                config.gateway = document.getElementById('gateway').value;
                config.subnet = document.getElementById('subnet').value;
                config.dns1 = document.getElementById('dns1').value;
                config.dns2 = document.getElementById('dns2').value;
                
                // Validate IP addresses
                if (!config.staticIP || !config.gateway || !config.subnet) {
                    showStatus('Please fill in all required IP fields', 'error');
                    return;
                }
            }
            
            // Show loading
            showStatus('Connecting to network...', 'success');
            
            // Send configuration
            fetch('/save', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showStatus('Successfully connected! The device will restart...', 'success');
                    setTimeout(() => {
                        showStatus('You can now close this page. Device IP: ' + data.ip, 'success');
                    }, 3000);
                } else {
                    showStatus('Failed to connect: ' + data.message, 'error');
                }
            })
            .catch(error => {
                showStatus('Connection error. Please try again.', 'error');
            });
        }
    </script>
</body>
</html>
)rawliteral";

// Function prototypes
void startConfigMode();
void connectToWiFi();
void saveConfiguration();
void loadConfiguration();
void handleRoot();
void handleScan();
void handleSave();
void handleNotFound();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32 WiFi Manager ===");
  
  // Initialize Preferences (better than EEPROM)
  preferences.begin("wifi-config", false);
  
  // Load saved configuration
  loadConfiguration();
  
  // Try to connect to saved WiFi
  if (strlen(config.ssid) > 0) {
    Serial.println("Found saved WiFi credentials");
    connectToWiFi();
    
    // If connection fails, start config mode
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to connect to saved network");
      startConfigMode();
    }
  } else {
    Serial.println("No saved WiFi credentials found");
    startConfigMode();
  }
}

void loop() {
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Timeout check
    if (millis() - configStartTime > CONFIG_TIMEOUT) {
      Serial.println("Configuration timeout, restarting...");
      ESP.restart();
    }
  }
  
  // Your main application code here
  // This runs when successfully connected to WiFi
  if (!configMode && WiFi.status() == WL_CONNECTED) {
    // Add your application logic here
  }
  
  delay(10);
}

void startConfigMode() {
  configMode = true;
  configStartTime = millis();
  
  Serial.println("\n=== Starting Configuration Mode ===");
  
  // Start Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP Address: ");
  Serial.println(apIP);
  Serial.println("Connect to WiFi: " + String(AP_SSID));
  
  // Start DNS Server for captive portal
  dnsServer.start(DNS_PORT, "*", apIP);
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started");
  Serial.println("Navigate to http://192.168.4.1 or connect to see captive portal");
}

void connectToWiFi() {
  Serial.println("\n=== Connecting to WiFi ===");
  Serial.println("SSID: " + String(config.ssid));
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Configure static IP if enabled
  if (config.useStaticIP) {
    Serial.println("Using Static IP Configuration:");
    Serial.println("IP: " + config.staticIP.toString());
    Serial.println("Gateway: " + config.gateway.toString());
    Serial.println("Subnet: " + config.subnet.toString());
    
    if (!WiFi.config(config.staticIP, config.gateway, config.subnet, 
                     config.dns1, config.dns2)) {
      Serial.println("Failed to configure static IP");
    }
  } else {
    Serial.println("Using DHCP");
  }
  
  WiFi.begin(config.ssid, config.password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    configMode = false;
  } else {
    Serial.println("\n✗ WiFi Connection Failed");
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
  
  Serial.println("Configuration saved to EEPROM");
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
  
  Serial.println("Configuration loaded from EEPROM");
}

void handleRoot() {
  server.send(200, "text/html", SETUP_PAGE);
}

void handleScan() {
  Serial.println("Scanning WiFi networks...");
  
  int n = WiFi.scanNetworks();
  
  String json = "{\"networks\":[";
  
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"encryption\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    json += "}";
  }
  
  json += "]}";
  
  WiFi.scanDelete();
  
  server.send(200, "application/json", json);
}

void handleSave() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    Serial.println("Received configuration:");
    Serial.println(body);
    
    // Parse JSON manually (simple parsing for this use case)
    // In production, consider using ArduinoJson library
    
    int ssidStart = body.indexOf("\"ssid\":\"") + 8;
    int ssidEnd = body.indexOf("\"", ssidStart);
    String ssid = body.substring(ssidStart, ssidEnd);
    
    int passStart = body.indexOf("\"password\":\"") + 12;
    int passEnd = body.indexOf("\"", passStart);
    String password = body.substring(passStart, passEnd);
    
    int staticStart = body.indexOf("\"useStaticIP\":") + 14;
    bool useStatic = body.substring(staticStart, staticStart + 4) == "true";
    
    ssid.toCharArray(config.ssid, 32);
    password.toCharArray(config.password, 64);
    config.useStaticIP = useStatic;
    
    if (useStatic) {
      int ipStart = body.indexOf("\"staticIP\":\"") + 12;
      int ipEnd = body.indexOf("\"", ipStart);
      String staticIP = body.substring(ipStart, ipEnd);
      config.staticIP.fromString(staticIP);
      
      int gwStart = body.indexOf("\"gateway\":\"") + 11;
      int gwEnd = body.indexOf("\"", gwStart);
      String gateway = body.substring(gwStart, gwEnd);
      config.gateway.fromString(gateway);
      
      int snStart = body.indexOf("\"subnet\":\"") + 10;
      int snEnd = body.indexOf("\"", snStart);
      String subnet = body.substring(snStart, snEnd);
      config.subnet.fromString(subnet);
      
      int dns1Start = body.indexOf("\"dns1\":\"") + 8;
      int dns1End = body.indexOf("\"", dns1Start);
      String dns1 = body.substring(dns1Start, dns1End);
      config.dns1.fromString(dns1);
      
      int dns2Start = body.indexOf("\"dns2\":\"") + 8;
      int dns2End = body.indexOf("\"", dns2Start);
      String dns2 = body.substring(dns2Start, dns2End);
      config.dns2.fromString(dns2);
    }
    
    saveConfiguration();
    
    // Try to connect
    WiFi.mode(WIFI_STA);
    connectToWiFi();
    
    String response;
    if (WiFi.status() == WL_CONNECTED) {
      response = "{\"success\":true,\"message\":\"Connected\",\"ip\":\"" + 
                 WiFi.localIP().toString() + "\"}";
    } else {
      response = "{\"success\":false,\"message\":\"Connection failed\"}";
    }
    
    server.send(200, "application/json", response);
    
    // Restart after a delay if successful
    if (WiFi.status() == WL_CONNECTED) {
      delay(2000);
      ESP.restart();
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"No data\"}");
  }
}

void handleNotFound() {
  // Redirect to root for captive portal
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
