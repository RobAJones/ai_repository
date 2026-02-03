# WPS (Wi-Fi Protected Setup) Guide

## What is WPS?

**WPS (Wi-Fi Protected Setup)** is a network security standard that makes it easy to connect devices to a WiFi network without typing passwords. You simply press a button on your router, then press a button on your device, and they connect automatically.

### Benefits
- ✅ **No password typing** - Eliminates typos and forgotten passwords
- ✅ **Super fast** - Connect in seconds
- ✅ **Beginner-friendly** - Just press buttons
- ✅ **Automatic configuration** - Device gets SSID and password automatically
- ✅ **Secure** - Uses strong encryption (WPA2)

### Requirements
- Router with WPS button (most modern routers have this)
- ESP32 with WPS-enabled code (this project!)
- Physical access to both devices

---

## How to Use WPS

### Method 1: Hardware Button (Recommended)

**Hardware Setup:**
```
Connect a button to GPIO 4 and GND:

ESP32          Button
GPIO 4 --------●/●-------- GND
               Button
```

**Steps:**
1. **Press and hold** the button on GPIO 4 for **1 second**
2. Serial Monitor shows: "✓ Starting WPS..."
3. **Immediately press** the WPS button on your router
4. Wait up to 2 minutes for connection
5. LED blinks slowly during connection
6. Serial Monitor shows success message and IP address
7. Configuration automatically saved!

**Serial Monitor Output:**
```
📶 WPS button pressed...
✓ Starting WPS...

╔════════════════════════════════╗
║      WPS CONNECTION MODE       ║
╚════════════════════════════════╝

📶 WPS (Wi-Fi Protected Setup) Starting...

Instructions:
1. Press the WPS button on your WiFi router
2. Wait for connection (up to 2 minutes)
3. ESP32 will auto-configure and connect

⏱️  Timeout: 2 minutes

🔄 Starting WPS handshake...
✓ WPS initiated successfully
⏳ Waiting for router WPS button press...
.......................

✓ WPS Connection Successful!
════════════════════════════════
Network: MyHomeNetwork
IP Address: 192.168.1.100
Gateway: 192.168.1.1
Signal Strength: -55 dBm
════════════════════════════════
✓ Configuration saved!

WPS setup complete. Device will use this network from now on.
```

### Method 2: Web Interface

**Steps:**
1. Connect to ESP32-Setup WiFi network
2. Open configuration page (auto-opens or go to 192.168.4.1)
3. Scroll down and click **"📶 Use WPS (Push Button)"**
4. **Immediately press** WPS button on your router
5. Wait for connection confirmation
6. Done!

**Web Interface Shows:**
```
WPS started! Press the WPS button on your router now. 
Waiting up to 2 minutes...

WPS in progress... (10s / 120s)
WPS in progress... (15s / 120s)
...
Successfully connected! Device IP: 192.168.1.100
```

---

## Finding Your Router's WPS Button

### Common Router Locations

**Most Routers:**
- Physical button on back or side
- Usually labeled "WPS", "Wi-Fi Protected Setup", or "Easy Connect"
- Sometimes has WiFi symbol with circular arrows: ⟲

**Router Brand Examples:**

**TP-Link:**
- Button on back panel labeled "WPS/Reset"
- Press briefly for WPS (long press = reset)

**Netgear:**
- WPS button on side or back
- May be labeled "Push 'N' Connect"

**Linksys:**
- Usually on side panel
- Labeled "Wi-Fi Protected Setup"

**ASUS:**
- Back panel, labeled "WPS"
- Sometimes shares button with reset

**D-Link:**
- Usually on back
- May be combined with WiFi on/off

**Google WiFi/Nest WiFi:**
- No physical WPS button
- Use Google Home app instead

**Eero:**
- No WPS support
- Must use manual configuration

### Virtual WPS (Some Routers)

Some routers don't have physical buttons:
- Log into router admin page
- Look for "WPS" or "Easy Connect" section
- Click "Start WPS" or "Push Button Connect"
- Then start WPS on ESP32

---

## WPS Timing & Process

### Timing is Critical!

**Order of Operations:**
```
1. Start WPS on ESP32 (press button or use web)
2. Within 30 seconds, press router WPS button
3. Wait for handshake (5-30 seconds)
4. Connection completes
```

**If Timing is Off:**
- ESP32 started too early → Router not ready
- ESP32 started too late → Router timeout
- **Solution:** Start both within 30 seconds of each other

### Connection Timeline

```
0s    ESP32 WPS started
      ↓
0-30s Press router WPS button (do this quickly!)
      ↓
5-30s Devices exchange credentials
      ↓ (LED blinks during this)
30-60s Connection established
      ↓
60s   IP address obtained
      ↓
      Configuration saved
      ↓
      Done!
```

### Timeout Behavior

**2-Minute Timeout:**
- If no connection after 2 minutes
- WPS automatically stops
- Error message displayed
- Can try again immediately

**Automatic Fallback:**
- If WPS fails and no saved config
- ESP32 enters configuration mode
- You can use manual setup instead

---

## Hardware Setup

### Default Pin Configuration

```cpp
#define WPS_BUTTON_PIN 4    // GPIO 4 (can be changed)
```

### Wiring Diagram

```
     ESP32
       │
 GPIO4 │────────●/●──────── GND
       │        Button
       │
   Built-in pull-up resistor
```

**No external resistor needed!** The ESP32's internal pull-up is used.

### Changing the WPS Pin

Edit the code to use a different GPIO:

```cpp
#define WPS_BUTTON_PIN 15   // Change to your preferred GPIO
```

**Recommended GPIOs:**
- GPIO 4, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33
- Avoid: GPIO 0 (BOOT), GPIO 1/3 (Serial), GPIO 6-11 (Flash)

### Using BOOT Button for WPS

Some users prefer using the existing BOOT button:

**Option 1 - Short Press = WPS, Long Press = Reset:**
```cpp
// In checkResetButton() function:
if (holdTime >= 1000 && holdTime < 3000) {
  startWPS();
  buttonPressed = false;
} else if (holdTime >= 3000) {
  factoryReset();
}
```

**Option 2 - Double-Press for WPS:**
```cpp
// Detect double-press pattern
// Implementation left as exercise
```

---

## Troubleshooting

### WPS Not Starting

**Symptom:** Nothing happens when pressing WPS button

**Solutions:**
1. Check Serial Monitor at 115200 baud
2. Verify correct GPIO pin in code
3. Test button with multimeter
4. Check wiring (button to GPIO and GND)
5. Ensure button press detected: "📶 WPS button pressed..."

**Button Test Code:**
```cpp
void loop() {
  if (digitalRead(WPS_BUTTON_PIN) == LOW) {
    Serial.println("Button pressed!");
  }
  delay(100);
}
```

### WPS Timeout

**Symptom:** "✗ WPS Connection Timeout" after 2 minutes

**Common Causes:**

1. **Router WPS Not Pressed:**
   - Must press router button within 30 seconds
   - Try again with better timing

2. **Router WPS Disabled:**
   - Check router settings
   - Enable WPS in admin panel

3. **Router Out of Range:**
   - Move ESP32 closer to router
   - Check signal strength

4. **Router WPS Cooldown:**
   - Some routers limit WPS attempts
   - Wait 5 minutes between attempts

5. **Router MAC Filtering:**
   - Router blocking unknown devices
   - Temporarily disable MAC filtering

### WPS Succeeds But Doesn't Save

**Symptom:** Connected via WPS but credentials not saved

**Solution:**
```cpp
// Verify preferences namespace
preferences.begin("wifi-config", false);

// Check save function is called
saveConfiguration();
```

### Router WPS Button Not Working

**Test Router WPS:**
1. Try connecting phone/tablet via WPS
2. If phone works, ESP32 should too
3. If phone fails, router WPS may be broken

**Router Settings:**
- Log into router admin (usually 192.168.1.1)
- Check WPS status (should be "Enabled")
- Some routers have WPS timeout setting
- Increase timeout if available

### Multiple WPS Attempts Failing

**Systematic Approach:**

1. **Test Manual Connection First:**
   - Use normal password method
   - Confirms WiFi and ESP32 work

2. **Test Router WPS:**
   - Connect another device via WPS
   - Confirms router WPS functional

3. **Check ESP32 WPS Code:**
   - Open Serial Monitor
   - Watch for error messages
   - esp_wifi_wps_enable() should return ESP_OK

4. **Try Different Router:**
   - If available, test with different router
   - Isolates router vs ESP32 issue

---

## Security Considerations

### Is WPS Secure?

**Modern WPS (Push Button) - SAFE ✅**
- Uses strong WPA2 encryption
- Button press required (physical access)
- 2-minute timeout window
- Cannot be attacked remotely

**WPS PIN Method - VULNERABLE ❌**
- This project does NOT use PIN method
- We only use Push Button Configuration (PBC)
- PIN method has known security issues
- Our implementation is secure!

### Best Practices

1. **Disable WPS After Setup:**
   - If security-critical application
   - WPS only needed once
   - Disable in router after configuration

2. **Physical Security:**
   - Router should be in secure location
   - ESP32 WPS button should not be accessible to public

3. **Network Segmentation:**
   - Use guest network for IoT devices
   - Keep IoT separate from main network

---

## Advanced Configuration

### Adjusting WPS Timeout

Default is 2 minutes (120 seconds):

```cpp
#define WPS_TIMEOUT 120000  // 2 minutes

// Examples:
#define WPS_TIMEOUT 60000   // 1 minute (faster failure)
#define WPS_TIMEOUT 300000  // 5 minutes (more patience)
```

### WPS with Static IP

**Current Behavior:**
- WPS always uses DHCP (automatic IP)
- This is standard WPS behavior
- Most users want DHCP anyway

**To Set Static IP After WPS:**
1. Let WPS connect with DHCP
2. Note the IP address assigned
3. Configure router to reserve that IP (DHCP reservation)
4. Or manually configure static IP in code afterward

### Disabling WPS

To remove WPS functionality:

```cpp
// Comment out or remove:
// #define WPS_BUTTON_PIN 4
// checkWPSButton();
// handleWPS();
// startWPS();
```

### WPS Status Indicator

Add visual feedback with LED:

```cpp
void handleWPS() {
  // Slow blink during WPS
  if (millis() % 1000 < 500) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
  
  // ... rest of function
}
```

---

## WPS vs Manual Configuration

### When to Use WPS

✅ **Perfect for:**
- Quick temporary connections
- Testing new networks
- Non-technical users
- Demonstrations
- Home networks
- Locations where typing is difficult

✅ **Advantages:**
- Faster than typing password
- No password errors
- No need to know password
- Works with complex passwords

### When to Use Manual

✅ **Better for:**
- Enterprise networks (WPA2-Enterprise)
- Networks without WPS
- When static IP required
- Remote configuration via web
- Hidden SSID networks
- Networks with special characters

✅ **Advantages:**
- Works on all networks
- Can set static IP
- Can configure remotely
- More control over settings

---

## Integration Examples

### WPS + LED Feedback

```cpp
void startWPS() {
  pinMode(LED_BUILTIN, OUTPUT);
  wpsRunning = true;
  
  // Rapid blink = WPS starting
  for(int i = 0; i < 6; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
  
  // Start actual WPS...
}
```

### WPS with Notification

```cpp
void handleWPS() {
  if (WiFi.status() == WL_CONNECTED) {
    // Send notification
    sendPushNotification("ESP32 connected via WPS!");
    sendEmail("WiFi configured via WPS");
    updateCloudStatus("online");
    
    // ... rest of success handling
  }
}
```

### Dual-Mode Button

One button for both WPS and reset:

```cpp
// Short press (1 sec) = WPS
// Long press (3 sec) = Reset

void checkDualButton() {
  if (holdTime >= 1000 && holdTime < 3000 && !wpsTriggered) {
    wpsTriggered = true;
    startWPS();
  } else if (holdTime >= 3000 && !resetTriggered) {
    resetTriggered = true;
    factoryReset();
  }
}
```

---

## FAQ

**Q: Does WPS work with 5GHz WiFi?**  
A: Yes, if your ESP32 supports 5GHz (most don't - they're 2.4GHz only).

**Q: Can I use WPS in config mode?**  
A: WPS automatically exits config mode when successful.

**Q: Does WPS work with hidden networks?**  
A: Yes! WPS can connect to hidden SSID networks.

**Q: What if my router doesn't have WPS?**  
A: Use the manual configuration method instead.

**Q: Can I use WPS multiple times?**  
A: Yes, each WPS connection replaces the previous configuration.

**Q: Does WPS work with mesh networks?**  
A: Yes, press WPS on any mesh node.

**Q: Is WPS faster than manual config?**  
A: Usually yes - 30 seconds vs 2-3 minutes typing.

**Q: Can I disable WPS after setup?**  
A: Yes, the saved credentials remain. WPS only needed once.

---

## Summary

### Quick Reference

**Start WPS:**
- Hardware: Press GPIO 4 button for 1 second
- Web: Click "Use WPS" button
- Immediately press router WPS button
- Wait up to 2 minutes

**Requirements:**
- Router with WPS button
- Button on GPIO 4 (or custom pin)
- Physical access to router

**Benefits:**
- No password typing
- Faster setup
- Fewer errors
- Beginner-friendly

**Troubleshooting:**
- Check Serial Monitor
- Verify timing (30 second window)
- Ensure router WPS enabled
- Try manual config if fails

---

**WPS makes WiFi setup effortless! 📶✨**
