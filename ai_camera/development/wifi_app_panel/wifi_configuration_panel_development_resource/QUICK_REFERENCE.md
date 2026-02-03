# WiFi Manager - Quick Reference Card

## 📋 Minimum Required Code

```cpp
#include "WiFiManager_ESP32.h"

WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
NetworkConfig netConfig;
bool configMode = false;
unsigned long configStartTime = 0;
unsigned long buttonPressStartTime = 0;
bool buttonPressed = false;
bool resetTriggered = false;

void setup() {
  Serial.begin(115200);
  pinMode(WM_RESET_BUTTON_PIN, INPUT_PULLUP);
  loadNetworkConfiguration();
  
  if (strlen(netConfig.ssid) > 0) {
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) startConfigMode();
  } else {
    startConfigMode();
  }
  
  if (!configMode) server.begin();
}

void loop() {
  checkResetButton();
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    if (millis() - configStartTime > WM_CONFIG_TIMEOUT) ESP.restart();
    return;
  }
  server.handleClient();
}
```

---

## 🔧 Required Global Declarations

```cpp
extern WebServer server;
extern DNSServer dnsServer;
extern Preferences preferences;
extern NetworkConfig netConfig;
extern bool configMode;
extern unsigned long configStartTime;
extern unsigned long buttonPressStartTime;
extern bool buttonPressed;
extern bool resetTriggered;
```

---

## 📞 Essential Functions

| Function | When to Call | Purpose |
|----------|--------------|---------|
| `loadNetworkConfiguration()` | `setup()` start | Load saved WiFi settings |
| `connectToWiFi()` | After loading config | Attempt connection |
| `startConfigMode()` | On connection failure | Enter config portal |
| `checkResetButton()` | Every `loop()` | Monitor reset button |
| `setupWebServerRoutes()` | In config mode | Setup portal routes |

---

## ⚙️ Configuration Constants

```cpp
#define WM_AP_SSID "ESP32-Setup"        // AP name
#define WM_AP_PASSWORD ""               // AP password
#define WM_RESET_BUTTON_PIN 0           // Reset button GPIO
#define WM_RESET_HOLD_TIME 3000         // Hold time (ms)
#define WM_WIFI_TIMEOUT 20000           // Connect timeout
#define WM_CONFIG_TIMEOUT 300000        // Config mode timeout
```

---

## 🚦 WiFi Status Check

```cpp
if (WiFi.status() == WL_CONNECTED) {
  // Connected
} else {
  // Not connected
}
```

---

## 🔄 Reconnection Pattern

```cpp
void loop() {
  static unsigned long lastAttempt = 0;
  
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastAttempt > 30000) {
      lastAttempt = millis();
      connectToWiFi();
    }
  }
}
```

---

## 💾 NetworkConfig Structure

```cpp
struct NetworkConfig {
  char ssid[32];           // Network name
  char password[64];       // Network password
  bool useStaticIP;        // true=static, false=DHCP
  IPAddress staticIP;      // Static IP (if enabled)
  IPAddress gateway;       // Gateway address
  IPAddress subnet;        // Subnet mask
  IPAddress dns1;          // Primary DNS
  IPAddress dns2;          // Secondary DNS
};
```

---

## 🔍 Common Serial Debug Output

```
✓ Network configuration loaded
  SSID: MyNetwork
  IP Mode: Static

╔════════════════════════════════╗
║  Connecting to WiFi Network   ║
╚════════════════════════════════╝
SSID: MyNetwork
Using Static IP configuration
  IP: 192.168.1.100
  Gateway: 192.168.1.1
Connecting........
✓ Connected successfully!
  IP Address: 192.168.1.100
  Signal Strength: -45 dBm
```

---

## 🎯 Integration Checklist

- [ ] Add #include "WiFiManager_ESP32.h"
- [ ] Declare required global objects
- [ ] Initialize reset button in setup()
- [ ] Call loadNetworkConfiguration()
- [ ] Call connectToWiFi() if config exists
- [ ] Call startConfigMode() if needed
- [ ] Call checkResetButton() in loop()
- [ ] Handle config mode in loop()
- [ ] Add reconnection logic (optional)

---

## 🐛 Quick Troubleshooting

| Problem | Solution |
|---------|----------|
| Can't see AP | Check Serial Monitor, verify power |
| Won't connect | Try factory reset (hold BOOT 3s) |
| Config not saving | Check "Configuration saved" in Serial |
| Reset not working | Verify WM_RESET_BUTTON_PIN setting |

---

## 🌐 URLs in Config Mode

- **Main Page:** `http://192.168.4.1`
- **Any URL:** Redirects to config (captive portal)

---

## 📱 User Instructions

1. **Setup:**
   - Connect to WiFi AP "ESP32-Setup"
   - Browser opens automatically
   - Scan and select network
   - Enter password
   - Save & Connect

2. **Reset:**
   - Hold BOOT button 3 seconds
   - Device clears WiFi and restarts

---

## 🎨 Customization Quick Tips

**Change AP Name:**
```cpp
#define WM_AP_SSID "MyDevice-Setup"
```

**Change Reset Pin:**
```cpp
#define WM_RESET_BUTTON_PIN 15
```

**Add Status LED:**
```cpp
void loop() {
  if (configMode) digitalWrite(LED, millis() % 500 < 250);
  else digitalWrite(LED, WiFi.status() == WL_CONNECTED);
}
```

---

## 📊 Status Indicators

| LED Pattern | Meaning |
|-------------|---------|
| Fast Blink | Config mode |
| Solid On | Connected |
| Slow Blink | Disconnected |
| Off | Powered off |

---

## 🔐 Security Notes

- Default AP has no password (open network)
- Add password: `#define WM_AP_PASSWORD "yourpassword"`
- Config portal auto-closes after 5 minutes
- Credentials stored in encrypted preferences

---

## 📦 File Structure Reference

```
YourProject/
├── YourProject.ino
├── WiFiManager_ESP32.h
└── WiFiManager_ESP32.cpp
```

Or:

```
Arduino/libraries/
└── WiFiManager_ESP32/
    ├── WiFiManager_ESP32.h
    ├── WiFiManager_ESP32.cpp
    └── examples/
```

---

## 🚀 Performance Tips

- Call `checkResetButton()` every loop iteration
- Only call `connectToWiFi()` during reconnect attempts
- Use `WiFi.status()` cached value (don't call repeatedly)
- Process DNS requests in config mode only

---

**For full documentation, see IMPLEMENTATION_GUIDE.md**
