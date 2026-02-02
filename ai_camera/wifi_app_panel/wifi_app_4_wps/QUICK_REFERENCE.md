# ESP32 WiFi Manager - Quick Reference Card

## 📋 Essential Information

**Access Point Name:** `ESP32-Setup`  
**Configuration URL:** `http://192.168.4.1`  
**Reset Button:** BOOT button (GPIO 0) - Hold 3 seconds  
**WPS Button:** GPIO 4 - Hold 1 second  
**Serial Baud Rate:** 115200  

---

## 📶 WPS Quick Setup (Easiest Method!)

### One-Button Connection
```
1. Press GPIO 4 button for 1 second
2. Immediately press WPS on router
3. Wait 30-60 seconds
4. Connected! (credentials auto-saved)
```

### Web Interface WPS
```
1. Connect to ESP32-Setup
2. Open config page
3. Click "Use WPS" button
4. Press router WPS button
5. Wait for success message
```

### WPS Button Locations
- **TP-Link:** Back panel, "WPS/Reset"
- **Netgear:** Side/back, "Push 'N' Connect"
- **Linksys:** Side, "Wi-Fi Protected Setup"
- **ASUS:** Back, "WPS"

---

## 🚀 First Time Setup (1-2-3)

1. **Power on** ESP32
2. **Connect** phone/computer to WiFi: `ESP32-Setup`
3. **Configure** WiFi in auto-opened page

---

## 🔴 Factory Reset (Clear All Settings)

### Quick Method
```
1. Press and HOLD BOOT button
2. Keep holding for 3 seconds
3. LED blinks → Settings cleared
4. Device restarts in config mode
```

### Boot Reset Method
```
1. Hold BOOT button
2. Press EN button (quick tap)
3. Keep holding BOOT for 3 seconds
4. Release
```

---

## 🔧 Common Operations

### View Connection Status
```
Open Serial Monitor → 115200 baud
Look for IP address and connection status
```

### Change WiFi Network
```
Option 1: Factory reset (BOOT button 3 sec)
Option 2: Wait for 5 reconnect failures
Option 3: Upload code with preferences.clear()
```

### Check Signal Strength
```
Serial Monitor shows:
  -30 to -50 dBm = Excellent
  -50 to -70 dBm = Good
  -70 to -80 dBm = Fair
  -80+ dBm = Weak (warnings appear)
```

---

## 🐛 Quick Troubleshooting

| Problem | Solution |
|---------|----------|
| Can't connect to WiFi | • Check password<br>• Try factory reset<br>• Try WPS button<br>• Move closer to router |
| Captive portal not opening | • Go to http://192.168.4.1<br>• Forget network and reconnect |
| Frequent disconnects | • Check signal strength<br>• Use Static IP<br>• Move closer to router |
| Reset button not working | • Hold BOOT (not EN)<br>• Full 3 seconds<br>• Watch Serial Monitor |
| WPS timeout | • Press router WPS within 30s<br>• Check router WPS enabled<br>• Try manual config |
| ESP32 keeps restarting | • Check power supply<br>• Check Serial Monitor<br>• Try factory reset |

---

## ⚙️ Configuration Timeouts

| Timeout | Default | Purpose |
|---------|---------|---------|
| Config Mode | 5 minutes | Time in AP mode before restart |
| Connection | 20 seconds | Max time per connect attempt |
| Reconnect Interval | 30 seconds | Time between retry attempts |
| Max Reconnect Attempts | 5 attempts | Before entering config mode |

---

## 📊 Status Indicators

### Serial Monitor Messages

✓ **WiFi Connected** → Successfully connected  
✗ **Connection Failed** → Wrong password or network  
⚠ **Weak signal** → Signal below -80 dBm  
🔄 **Reconnecting** → Auto-reconnect in progress  
🔴 **Reset triggered** → Factory reset started  

### LED Behavior (if available)

- **Slow blink** → Normal operation / LED control
- **Fast blink** → Reset button held (waiting)
- **Rapid blinks** → Factory reset in progress
- **Off** → Disconnected or LED control off

---

## 🔑 Configuration Files

### Pin Definitions
```cpp
RESET_BUTTON_PIN = 0    // BOOT button
WPS_BUTTON_PIN = 4      // WPS button
LED_PIN = 2             // Built-in LED
```

### Timing Constants
```cpp
RESET_HOLD_TIME = 3000          // 3 seconds
WPS_TIMEOUT = 120000            // 2 minutes
WIFI_CONNECT_TIMEOUT = 20000    // 20 seconds
WIFI_RECONNECT_INTERVAL = 30000 // 30 seconds
MAX_RECONNECT_ATTEMPTS = 5      // 5 attempts
CONFIG_TIMEOUT = 300000         // 5 minutes
```

---

## 📱 Web Interface Access

### Configuration Mode
```
SSID: ESP32-Setup
URL:  http://192.168.4.1
Page: Auto-opens (captive portal)
```

### Normal Operation
```
URL:  http://[device-ip-address]
Find IP: Check Serial Monitor or router
```

---

## 💾 What's Stored in Memory

### Saved (Survives restart)
- ✅ WiFi SSID
- ✅ WiFi Password
- ✅ Static IP settings
- ✅ Network preferences

### Not Saved (Reset each boot)
- ❌ Current connection state
- ❌ Temporary statistics
- ❌ Session data

---

## 🎯 Network Configuration

### DHCP (Automatic) - Recommended
```
Just select network and enter password
Router assigns IP automatically
```

### Static IP - Advanced
```
Need to know:
• IP Address (e.g., 192.168.1.100)
• Gateway (usually 192.168.1.1)
• Subnet Mask (usually 255.255.255.0)
• DNS (can use 8.8.8.8)
```

---

## 🔒 Security Notes

- WiFi passwords stored encrypted in flash
- Captive portal is open (no AP password) by default
- Physical access allows factory reset
- Change AP_PASSWORD in code for production

---

## 📞 Support Checklist

Before asking for help, check:

1. ✅ Serial Monitor open at 115200 baud
2. ✅ Copied error messages
3. ✅ Tried factory reset
4. ✅ Verified BOOT button works
5. ✅ Checked WiFi signal strength
6. ✅ Confirmed ESP32 board model
7. ✅ Power supply adequate (500mA+)

---

## 🎓 Learning Resources

- **Full Setup Guide:** README.md
- **Disconnect Handling:** TIMEOUT_DISCONNECT_GUIDE.md
- **Factory Reset:** FACTORY_RESET_GUIDE.md
- **Example Application:** ESP32_Example_Application.ino

---

## ⚡ Quick Code Snippets

### Clear Settings Programmatically
```cpp
preferences.begin("wifi-config", false);
preferences.clear();
preferences.end();
ESP.restart();
```

### Check Connection Status
```cpp
if (WiFi.status() == WL_CONNECTED) {
  Serial.println("Connected: " + WiFi.localIP().toString());
} else {
  Serial.println("Disconnected");
}
```

### Get Signal Strength
```cpp
int rssi = WiFi.RSSI();
Serial.println("Signal: " + String(rssi) + " dBm");
```

### Manual Reconnect
```cpp
WiFi.disconnect();
delay(100);
WiFi.begin(ssid, password);
```

---

## 🎉 Pro Tips

1. **Serial Monitor is your friend** → Always keep it open during setup
2. **Signal matters** → Keep ESP32 within good WiFi range
3. **Test reset** → Try factory reset before deploying
4. **Static IP for reliability** → Use if DHCP causes issues
5. **Label the BOOT button** → Help future users find it
6. **Document your network** → Save WiFi credentials externally too
7. **Power supply matters** → Use quality USB cable and power source

---

**Made with ❤️ for ESP32 enthusiasts**  
*Happy Making! 🚀*
