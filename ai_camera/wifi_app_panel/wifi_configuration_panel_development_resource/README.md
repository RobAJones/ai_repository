# WiFi Manager for ESP32

A complete, reusable WiFi configuration solution for ESP32 projects with a beautiful web interface.

![Version](https://img.shields.io/badge/version-1.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![ESP32](https://img.shields.io/badge/platform-ESP32-orange.svg)

---

## 🌟 Features

- **🔧 Zero Hardcoding** - No WiFi credentials in your code
- **📱 Mobile-Friendly** - Responsive web interface
- **🌐 Network Scanning** - See and select available networks
- **🔒 Static IP Support** - Optional manual IP configuration
- **💾 Non-Volatile Storage** - Settings persist across reboots
- **🔄 Factory Reset** - Easy reset via button hold
- **🎯 Captive Portal** - Automatic redirect to config page
- **🎨 Beautiful UI** - Modern gradient design
- **📊 Status Monitoring** - Real-time connection feedback
- **🐛 Debug Output** - Detailed serial logging

---

## 📦 Package Contents

```
WiFiManager_ESP32/
├── WiFiManager_ESP32.h          # Header file
├── WiFiManager_ESP32.cpp        # Implementation
├── IMPLEMENTATION_GUIDE.md      # Detailed documentation
├── EXAMPLE_BASIC.ino           # Minimal example
├── EXAMPLE_ADVANCED.ino        # Full-featured example
└── README.md                   # This file
```

---

## 🚀 Quick Start

### 1. Installation

**Option A: Manual Installation**
1. Download this package
2. Copy `WiFiManager_ESP32.h` and `WiFiManager_ESP32.cpp` to your project folder

**Option B: Arduino Library**
1. Copy entire `WiFiManager_ESP32` folder to your Arduino libraries directory
   - Windows: `Documents/Arduino/libraries/`
   - macOS: `~/Documents/Arduino/libraries/`
   - Linux: `~/Arduino/libraries/`

### 2. Basic Usage

```cpp
#include "WiFiManager_ESP32.h"

// Required global objects
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
NetworkConfig netConfig;

// State variables
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
    if (WiFi.status() == WL_CONNECTED) {
      // Connected - start your application
      server.begin();
    } else {
      startConfigMode();
    }
  } else {
    startConfigMode();
  }
}

void loop() {
  checkResetButton();
  
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }
  
  // Your application code here
  server.handleClient();
}
```

See `EXAMPLE_BASIC.ino` for a complete working example.

---

## 📖 Usage Guide

### First Time Setup

1. **Flash Your Code**
   - Upload your sketch with WiFi Manager integrated
   - Open Serial Monitor (115200 baud)

2. **Connect to Setup Portal**
   - Device creates WiFi AP: `ESP32-Setup`
   - Connect with phone/computer
   - Browser opens automatically (or go to any website)

3. **Configure WiFi**
   - Click "Scan Networks"
   - Select your network
   - Enter password
   - (Optional) Configure static IP
   - Click "Save & Connect"

4. **Done!**
   - Device restarts and connects to your network
   - Settings saved permanently

### Factory Reset

Hold the **BOOT button** (GPIO 0) for **3 seconds** to clear all WiFi settings and return to setup mode.

---

## ⚙️ Configuration

### Customize Access Point Name

Edit `WiFiManager_ESP32.h`:

```cpp
#define WM_AP_SSID "MyDevice-Setup"    // Your custom name
```

### Change Reset Button

Edit `WiFiManager_ESP32.h`:

```cpp
#define WM_RESET_BUTTON_PIN 15    // Use different GPIO
```

### Adjust Timeouts

Edit `WiFiManager_ESP32.h`:

```cpp
#define WM_WIFI_TIMEOUT 30000       // 30 seconds
#define WM_CONFIG_TIMEOUT 600000    // 10 minutes
#define WM_RESET_HOLD_TIME 5000     // 5 seconds
```

---

## 🔌 Hardware Requirements

### Minimum
- **ESP32, ESP32-C3, ESP32-S2, or ESP32-S3**
- Built-in BOOT button (or external button on GPIO 0)

### Optional
- LED on GPIO 2 (for status indication)
- OLED display (for visual feedback)

---

## 📱 Supported Devices

✅ ESP32 WROOM  
✅ ESP32-C3  
✅ ESP32-S2  
✅ ESP32-S3  
✅ ESP32 DevKit  
✅ ESP32 Thing  

---

## 🎯 Use Cases

Perfect for:
- **IoT Devices** - Easy WiFi setup for end users
- **Home Automation** - Install without hardcoded credentials
- **Sensors & Monitors** - Field deployment without reprogramming
- **Prototypes** - Quick WiFi configuration during development
- **Products** - Professional WiFi setup experience

---

## 🔍 Advanced Features

### Custom Branding

Modify `getConfigHTML()` in `WiFiManager_ESP32.cpp` to match your brand colors and logo.

### Additional Configuration

Extend `NetworkConfig` structure to store additional settings:

```cpp
struct NetworkConfig {
  char ssid[32];
  char password[64];
  bool useStaticIP;
  // ... existing fields ...
  
  // Add your custom fields
  char deviceName[32];
  uint16_t sampleRate;
  bool enableFeature;
};
```

### Reconnection Logic

```cpp
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt > 30000) {
      lastAttempt = millis();
      connectToWiFi();
    }
  }
}
```

---

## 🐛 Troubleshooting

### Can't see Access Point
- Check Serial Monitor for errors
- Verify ESP32 is powered properly
- Try restarting device

### Can't Connect to WiFi
- Verify credentials are correct
- Check WiFi signal strength
- Try factory reset (hold BOOT 3 seconds)

### Configuration Not Saving
- Check Serial Monitor for "Configuration saved" message
- Verify ESP32 has sufficient power
- Try erasing flash: `esptool.py erase_flash`

---

## 📚 Documentation

- **[IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)** - Comprehensive integration guide
- **[EXAMPLE_BASIC.ino](EXAMPLE_BASIC.ino)** - Minimal working example
- **[EXAMPLE_ADVANCED.ino](EXAMPLE_ADVANCED.ino)** - Full-featured example

---

## 📝 API Reference

### Core Functions

| Function | Description |
|----------|-------------|
| `loadNetworkConfiguration()` | Load saved WiFi settings |
| `saveNetworkConfiguration()` | Save WiFi settings |
| `connectToWiFi()` | Attempt WiFi connection |
| `startConfigMode()` | Enter configuration portal |
| `factoryReset()` | Clear all settings |
| `checkResetButton()` | Monitor reset button |
| `setupWebServerRoutes()` | Setup config portal routes |

See `WiFiManager_ESP32.h` for complete API documentation.

---

## 🤝 Contributing

Contributions welcome! Feel free to:
- Report bugs
- Suggest features
- Submit pull requests
- Improve documentation

---

## 📄 License

MIT License - Free to use in personal and commercial projects.

---

## 🎖️ Version History

**v1.0** (January 2026)
- Initial release
- Core WiFi management
- Web configuration portal
- Static IP support
- Factory reset
- Captive portal
- Responsive UI

---

## ⭐ Credits

Developed for ESP32 environmental monitoring and IoT projects.  
Successfully deployed in weather stations and sensor networks.

---

## 📧 Support

For detailed implementation help, see **[IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)**

For examples, see **EXAMPLE_BASIC.ino** and **EXAMPLE_ADVANCED.ino**

---

Made with ❤️ for the ESP32 community
