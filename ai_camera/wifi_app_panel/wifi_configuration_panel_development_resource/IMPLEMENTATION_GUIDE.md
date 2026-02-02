# WiFi Manager for ESP32 - Implementation Guide

**Version:** 1.0  
**Date:** January 2026  
**Author:** Robert  
**Compatible:** ESP32, ESP32-C3, ESP32-S2, ESP32-S3

---

## Overview

This WiFi Manager provides a complete, reusable solution for ESP32 WiFi configuration with a user-friendly web interface. It eliminates the need to hardcode WiFi credentials and allows end users to configure their devices through a captive portal.

### Key Features

✅ **Automatic Configuration Portal** - First boot launches setup AP  
✅ **Network Scanning** - View and select available WiFi networks  
✅ **Static IP Support** - Optional static IP configuration  
✅ **DHCP Support** - Automatic IP assignment  
✅ **Non-Volatile Storage** - Settings persist across reboots  
✅ **Factory Reset** - 3-second button hold to reset  
✅ **Captive Portal** - Automatic redirect to config page  
✅ **Responsive UI** - Works on mobile and desktop  
✅ **Serial Debugging** - Detailed console output  

---

## File Structure

```
WiFiManager_ESP32/
├── WiFiManager_ESP32.h         # Header file with declarations
├── WiFiManager_ESP32.cpp       # Implementation
├── IMPLEMENTATION_GUIDE.md     # This file
├── EXAMPLE_BASIC.ino          # Basic usage example
└── EXAMPLE_ADVANCED.ino       # Advanced integration example
```

---

## Quick Start

### 1. Add Files to Your Project

Copy `WiFiManager_ESP32.h` and `WiFiManager_ESP32.cpp` to your project folder or Arduino libraries directory.

### 2. Required Libraries

Install these libraries via Arduino Library Manager:
- **WiFi** (built-in with ESP32)
- **WebServer** (built-in with ESP32)
- **DNSServer** (built-in with ESP32)
- **Preferences** (built-in with ESP32)

### 3. Basic Implementation

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "WiFiManager_ESP32.h"

// Create required objects
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
  
  Serial.println("Starting WiFi Manager Example...");
  
  // Initialize reset button
  pinMode(WM_RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // Load saved WiFi configuration
  loadNetworkConfiguration();
  
  // Try to connect to saved network
  if (strlen(netConfig.ssid) > 0) {
    Serial.println("Attempting to connect to saved network...");
    connectToWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✓ Connected to WiFi");
      
      // Your application setup here
      setupWebServerRoutes();  // Add your routes
      server.begin();
    } else {
      Serial.println("✗ Failed to connect, entering config mode");
      startConfigMode();
    }
  } else {
    Serial.println("No saved network, entering config mode");
    startConfigMode();
  }
}

void loop() {
  // Check for factory reset button
  checkResetButton();
  
  // Handle configuration mode
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Auto-exit after timeout
    if (millis() - configStartTime > WM_CONFIG_TIMEOUT) {
      Serial.println("Config timeout, restarting...");
      ESP.restart();
    }
    return;
  }
  
  // Your normal application code here
  server.handleClient();
}
```

---

## Customization

### Change Access Point Name

Edit `WiFiManager_ESP32.h`:

```cpp
#define WM_AP_SSID "MyDevice-Setup"    // Your custom AP name
```

### Change Reset Button Pin

Edit `WiFiManager_ESP32.h`:

```cpp
#define WM_RESET_BUTTON_PIN 15    // Use GPIO 15 instead of 0
```

### Change Timeouts

Edit `WiFiManager_ESP32.h`:

```cpp
#define WM_WIFI_TIMEOUT 30000       // 30 seconds to connect
#define WM_CONFIG_TIMEOUT 600000    // 10 minutes in config mode
#define WM_RESET_HOLD_TIME 5000     // 5 seconds to factory reset
```

---

## Integration with Existing Projects

### Step 1: Replace Hardcoded WiFi Credentials

**Before:**
```cpp
const char* ssid = "MyNetwork";
const char* password = "MyPassword";

void setup() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}
```

**After:**
```cpp
#include "WiFiManager_ESP32.h"

// Declare required objects (see Quick Start)

void setup() {
  pinMode(WM_RESET_BUTTON_PIN, INPUT_PULLUP);
  loadNetworkConfiguration();
  
  if (strlen(netConfig.ssid) > 0) {
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      startConfigMode();
    }
  } else {
    startConfigMode();
  }
}
```

### Step 2: Update Web Server Setup

**Before:**
```cpp
void setup() {
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}
```

**After:**
```cpp
void setup() {
  // ... WiFi Manager setup ...
  
  if (!configMode) {
    // Your normal routes
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();
  }
  // Config mode routes are handled by setupWebServerRoutes()
}
```

### Step 3: Update Loop Function

**Add to beginning of loop():**
```cpp
void loop() {
  checkResetButton();
  
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    if (millis() - configStartTime > WM_CONFIG_TIMEOUT) {
      ESP.restart();
    }
    return;
  }
  
  // Your existing loop code here
  server.handleClient();
  // ... rest of your code ...
}
```

---

## Advanced Features

### Custom Configuration Page

You can modify the HTML/CSS in `WiFiManager_ESP32.cpp` in the `getConfigHTML()` function to match your branding:

```cpp
String getConfigHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>My Custom Setup</title>
  <style>
    /* Your custom CSS here */
  </style>
</head>
<body>
  <!-- Your custom HTML here -->
</body>
</html>
)rawliteral";
}
```

### Add Configuration Validation

In `handleSaveNetwork()`, add validation:

```cpp
void handleSaveNetwork() {
  // ... existing code ...
  
  // Add validation
  if (strlen(netConfig.ssid) < 2) {
    server.send(400, "application/json", 
      "{\"success\":false,\"error\":\"SSID too short\"}");
    return;
  }
  
  if (netConfig.useStaticIP) {
    if (netConfig.staticIP[0] == 0) {
      server.send(400, "application/json", 
        "{\"success\":false,\"error\":\"Invalid IP address\"}");
      return;
    }
  }
  
  // ... rest of code ...
}
```

### Store Additional Settings

Extend `NetworkConfig` structure in `WiFiManager_ESP32.h`:

```cpp
struct NetworkConfig {
  char ssid[32];
  char password[64];
  bool useStaticIP;
  IPAddress staticIP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;
  
  // Add your custom fields
  char deviceName[32];
  uint16_t updateInterval;
  bool enableFeatureX;
};
```

Then update `loadNetworkConfiguration()` and `saveNetworkConfiguration()` accordingly.

---

## Troubleshooting

### Issue: Device won't connect to WiFi

**Solution:**
1. Check Serial Monitor for error messages
2. Verify WiFi credentials are correct
3. Check signal strength (needs > -80 dBm)
4. Try factory reset (hold BOOT button 3 seconds)

### Issue: Can't access configuration portal

**Solution:**
1. Verify AP is broadcasting (check WiFi list for "ESP32-Setup")
2. Connect to AP
3. Try navigating to any website (captive portal should redirect)
4. Manually go to `192.168.4.1`

### Issue: Configuration not saving

**Solution:**
1. Check Serial Monitor for "Configuration saved" message
2. Verify ESP32 has Preferences library support
3. Try erasing flash: `esptool.py erase_flash`

### Issue: Reset button not working

**Solution:**
1. Verify `WM_RESET_BUTTON_PIN` matches your hardware
2. Check if pin has external pull-up/down resistor
3. Increase `WM_RESET_HOLD_TIME` if triggering too easily

---

## Best Practices

### 1. Always Check WiFi Status

```cpp
if (WiFi.status() == WL_CONNECTED) {
  // WiFi is connected - safe to use network features
} else {
  // Handle disconnection
  Serial.println("WiFi disconnected!");
}
```

### 2. Add Reconnection Logic

```cpp
unsigned long lastReconnectAttempt = 0;

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 30000) {  // Try every 30 seconds
      lastReconnectAttempt = now;
      Serial.println("Attempting to reconnect...");
      connectToWiFi();
    }
  }
}
```

### 3. Provide User Feedback

Use an LED or display to show WiFi status:

```cpp
void updateWiFiStatusLED() {
  if (configMode) {
    digitalWrite(LED_PIN, millis() % 500 < 250);  // Fast blink
  } else if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);  // Solid on
  } else {
    digitalWrite(LED_PIN, millis() % 1000 < 500);  // Slow blink
  }
}
```

### 4. Add Hostname

```cpp
void setup() {
  // ... WiFi Manager setup ...
  
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setHostname("my-esp32-device");
    Serial.print("Hostname: ");
    Serial.println(WiFi.getHostname());
  }
}
```

---

## API Reference

### Functions

#### `void loadNetworkConfiguration()`
Load WiFi settings from non-volatile storage.  
**Call:** Once in `setup()` before attempting connection.

#### `void saveNetworkConfiguration()`
Save WiFi settings to non-volatile storage.  
**Call:** Automatically called after web configuration.

#### `void connectToWiFi()`
Attempt connection to WiFi using saved credentials.  
**Returns:** void (check `WiFi.status()` for result)

#### `void startConfigMode()`
Enter configuration portal mode.  
**Call:** When no credentials exist or connection fails.

#### `void factoryReset()`
Clear all WiFi settings and restart.  
**Call:** Automatically called on button hold, or manually.

#### `void checkResetButton()`
Monitor reset button state.  
**Call:** Every loop() iteration.

#### `void setupWebServerRoutes()`
Configure web server for config portal.  
**Call:** After `startConfigMode()`.

#### `String getConfigHTML()`
Generate configuration page HTML.  
**Returns:** Complete HTML page as String.

### Global Variables

```cpp
extern NetworkConfig netConfig;      // Current network configuration
extern bool configMode;              // True if in config mode
extern unsigned long configStartTime;  // Config mode start timestamp
```

### Configuration Constants

All defined in `WiFiManager_ESP32.h`:
- `WM_AP_SSID` - Access point name
- `WM_AP_PASSWORD` - Access point password
- `WM_DNS_PORT` - DNS server port
- `WM_WIFI_TIMEOUT` - Connection timeout (ms)
- `WM_CONFIG_TIMEOUT` - Config mode timeout (ms)
- `WM_RESET_BUTTON_PIN` - GPIO for reset button
- `WM_RESET_HOLD_TIME` - Button hold time (ms)

---

## Examples Included

### EXAMPLE_BASIC.ino
Minimal implementation showing core functionality.

### EXAMPLE_ADVANCED.ino
Complete integration with:
- Web server for data display
- Sensor readings
- OLED display feedback
- Automatic reconnection
- Status LED indicators

---

## Version History

**v1.0** (January 2026)
- Initial release
- Basic WiFi configuration
- Static IP support
- Factory reset
- Captive portal
- Responsive web interface

---

## License

MIT License - Free to use in personal and commercial projects.

---

## Support

For issues, questions, or contributions:
- Check troubleshooting section above
- Review example files
- Check Serial Monitor output for detailed debugging

---

## Credits

Developed for ESP32 environmental monitoring projects.  
Based on successful deployment in weather station and monitoring systems.
