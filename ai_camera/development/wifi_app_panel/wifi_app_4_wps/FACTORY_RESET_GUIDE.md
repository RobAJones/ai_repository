# Factory Reset Button Guide

## Overview

The ESP32 WiFi Manager includes a built-in factory reset feature that allows users to completely clear all stored WiFi settings and return the device to its initial state. This is essential for troubleshooting, device redeployment, or when you need to reconfigure WiFi credentials.

## Hardware Requirements

### Built-in BOOT Button (Most Common)

Most ESP32 development boards include a button labeled "BOOT" connected to GPIO 0. This is the default reset button.

**Common ESP32 Boards with BOOT Button:**
- ESP32 DevKit V1
- ESP32-WROOM-32
- NodeMCU-32S
- ESP32-WROVER
- DOIT ESP32 DevKit
- Adafruit HUZZAH32

**Location:**
- Usually next to the USB port
- Often labeled "BOOT", "FLASH", or "IO0"
- Sometimes unlabeled but positioned opposite the "EN" button

### Custom Button (Optional)

You can connect an external button to any GPIO pin:

```
         ESP32
           |
    GPIO X |----/\/\/----[Button]----GND
           |    10kΩ
           |
```

**Wiring:**
- One side of button → GPIO pin
- Other side of button → GND
- Built-in INPUT_PULLUP resistor is used (no external resistor needed)

**To change the pin in code:**
```cpp
#define RESET_BUTTON_PIN 15  // Change to your GPIO pin
```

## How to Use Factory Reset

### Method 1: Reset During Normal Operation

**Steps:**
1. While ESP32 is running (connected or in config mode)
2. Press and **HOLD** the BOOT button
3. Serial Monitor will show: "⚠ Reset button pressed..."
4. Continue holding for **3 seconds**
5. LED will blink rapidly (if available)
6. Serial Monitor shows: "✓ Reset triggered!"
7. Device clears all settings and restarts

### Method 2: Reset During Boot

**Steps:**
1. Press and **HOLD** the BOOT button
2. While holding, press the EN (reset) button briefly to restart
3. Keep holding BOOT button
4. Serial Monitor will show: "⚠ Reset button detected during boot!"
5. Keep holding for **3 full seconds**
6. LED blinks while waiting
7. Release after 3 seconds
8. Device clears settings and restarts in config mode

### Method 3: Power-On Reset

**Steps:**
1. Unplug ESP32 from power
2. Press and **HOLD** the BOOT button
3. While holding, plug in power
4. Keep holding for **3 full seconds**
5. LED blinks as confirmation
6. Device clears settings and restarts

## What Gets Cleared

When factory reset is performed, the following is erased:

### WiFi Credentials
- Network SSID (name)
- Network password
- All saved networks

### IP Configuration
- Static IP address
- Gateway address
- Subnet mask
- DNS server addresses
- DHCP/Static selection

### All Preferences
- Everything stored in ESP32's Preferences (EEPROM replacement)
- Device returns to factory defaults

### What's NOT Cleared
- Your uploaded Arduino code
- ESP32 firmware
- Flash memory partitions

## Visual & Serial Feedback

### Serial Monitor Output

**During Reset:**
```
⚠ Reset button pressed...
Hold for 2 more seconds...
Hold for 1 more second...

✓ Reset triggered! Clearing WiFi settings...

╔════════════════════════════════╗
║   FACTORY RESET IN PROGRESS   ║
╚════════════════════════════════╝

=== Clearing WiFi Settings ===
✓ WiFi settings cleared!

✓ Factory reset complete!
Restarting in 2 seconds...
```

**After Restart:**
```
=== ESP32 WiFi Manager ===
No saved WiFi credentials found

=== Starting Configuration Mode ===
AP IP Address: 192.168.4.1
Connect to WiFi: ESP32-Setup
```

### LED Feedback

**During Button Hold:**
- Fast blinking (10 Hz) while waiting for 3 seconds
- Indicates button press is detected

**During Reset:**
- Rapid blinks (5 Hz) for 2 seconds
- Confirms reset is in progress

**After Reset:**
- LED turns off
- Normal operation resumes after restart

## Timing Configuration

### Adjusting Hold Time

Default hold time is 3 seconds. To change:

```cpp
#define RESET_HOLD_TIME 3000  // Time in milliseconds

// Examples:
#define RESET_HOLD_TIME 5000  // 5 seconds - more deliberate
#define RESET_HOLD_TIME 1000  // 1 second - quick reset
#define RESET_HOLD_TIME 10000 // 10 seconds - prevent accidents
```

**Recommendations:**
- **1-2 seconds**: Easy for experienced users, risk of accidental reset
- **3 seconds**: Good balance (default)
- **5-10 seconds**: Prevents accidental resets in production

## Use Cases

### 1. Changing WiFi Networks

**Scenario:** You moved to a new location with different WiFi

**Solution:**
1. Press BOOT button for 3 seconds
2. Device resets and enters config mode
3. Connect to "ESP32-Setup"
4. Configure new WiFi credentials

### 2. Forgot WiFi Password

**Scenario:** You configured device but forgot what password you entered

**Solution:**
1. Factory reset clears stored password
2. Reconfigure with correct credentials

### 3. Device Not Connecting

**Scenario:** Device keeps failing to connect after router change

**Solution:**
1. Factory reset clears potentially corrupted settings
2. Fresh configuration often resolves issues

### 4. Selling/Giving Away Device

**Scenario:** Transferring device to someone else

**Solution:**
1. Factory reset removes your WiFi credentials
2. New owner can configure their own network

### 5. Network Migration

**Scenario:** Company/home network infrastructure changed

**Solution:**
1. Factory reset all devices
2. Reconfigure with new network settings

### 6. Troubleshooting Connection Issues

**Scenario:** Device won't connect after router firmware update

**Solution:**
1. Reset clears potentially incompatible cached settings
2. Fresh DHCP lease and association

## Safety Features

### Accidental Press Prevention

**3-Second Hold Requirement:**
- Brief button presses are ignored
- Prevents accidental resets from bumps or touches
- User must intentionally hold button

**Serial Feedback:**
- Clear messages show reset progress
- User can cancel by releasing button early

**LED Blinking:**
- Visual confirmation reset is pending
- Gives user chance to cancel

### Debouncing

Button state is checked continuously with proper debouncing to prevent:
- Electrical noise triggering reset
- Mechanical bounce causing false triggers
- Rapid press/release being misinterpreted

## Integration with Your Application

### Preserving Application Data

If you have application-specific data to preserve during WiFi reset:

```cpp
void factoryReset() {
  // Clear only WiFi settings, preserve your data
  preferences.begin("wifi-config", false);
  preferences.clear();
  preferences.end();
  
  // Your app data in different namespace is safe
  preferences.begin("my-app-data", false);
  // This data is NOT cleared
  preferences.end();
  
  ESP.restart();
}
```

### Complete Factory Reset (WiFi + App Data)

To clear everything including your application data:

```cpp
void completeFactoryReset() {
  // Clear WiFi config
  preferences.begin("wifi-config", false);
  preferences.clear();
  preferences.end();
  
  // Clear your app data
  preferences.begin("my-app-data", false);
  preferences.clear();
  preferences.end();
  
  // Clear any other namespaces
  preferences.begin("sensor-calibration", false);
  preferences.clear();
  preferences.end();
  
  ESP.restart();
}
```

### Custom Reset Actions

Add custom logic before reset:

```cpp
void factoryReset() {
  Serial.println("Performing custom actions...");
  
  // Save important data to SD card
  saveToSDCard();
  
  // Send notification to server
  sendResetNotification();
  
  // Turn off outputs
  digitalWrite(RELAY_PIN, LOW);
  
  // Clear WiFi settings
  clearWiFiSettings();
  
  // Restart
  ESP.restart();
}
```

## Troubleshooting

### Button Not Working

**Check Pin Configuration:**
```cpp
// Verify correct GPIO pin
#define RESET_BUTTON_PIN 0  // Should match your hardware
```

**Test Button:**
```cpp
void setup() {
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    Serial.println("Button pressed!");
  }
  delay(100);
}
```

### Reset Not Triggering

**Possible Causes:**

1. **Not holding long enough:**
   - Must hold for full 3 seconds
   - Watch Serial Monitor for feedback

2. **Wrong GPIO pin:**
   - Verify BOOT button is on GPIO 0
   - Check your board's schematic

3. **Serial Monitor not visible:**
   - Open Serial Monitor at 115200 baud
   - Messages confirm reset progress

4. **Button wired incorrectly:**
   - BOOT button should be built-in
   - External button: One side to GPIO, other to GND

### LED Not Blinking

**LED Pin Configuration:**
```cpp
// Some boards use different LED pins
pinMode(2, OUTPUT);   // Most common
pinMode(LED_BUILTIN, OUTPUT);  // Some boards
pinMode(5, OUTPUT);   // NodeMCU boards
```

### Settings Not Actually Cleared

**Verify Preferences Cleared:**
```cpp
void clearWiFiSettings() {
  preferences.begin("wifi-config", false);
  preferences.clear();
  preferences.end();
  
  // Verify cleared
  preferences.begin("wifi-config", false);
  String ssid = preferences.getString("ssid", "");
  Serial.println("SSID after clear: '" + ssid + "'");
  preferences.end();
}
```

## Advanced Configuration

### Dual-Button Reset (More Safety)

Require two buttons pressed simultaneously:

```cpp
#define RESET_BUTTON_1 0   // BOOT button
#define RESET_BUTTON_2 15  // Custom button

void checkResetButton() {
  bool button1 = (digitalRead(RESET_BUTTON_1) == LOW);
  bool button2 = (digitalRead(RESET_BUTTON_2) == LOW);
  
  if (button1 && button2) {
    // Both buttons pressed - start reset timer
  }
}
```

### Progressive Reset Levels

Different hold times for different actions:

```cpp
if (holdTime >= 3000 && holdTime < 6000) {
  // 3 seconds: WiFi reset only
  clearWiFiSettings();
} else if (holdTime >= 6000 && holdTime < 10000) {
  // 6 seconds: WiFi + app settings
  clearAllSettings();
} else if (holdTime >= 10000) {
  // 10 seconds: Complete factory reset + format flash
  completeFactoryReset();
}
```

### Remote Reset (via Web)

Add a reset button to your web interface:

```cpp
server.on("/reset", HTTP_POST, []() {
  server.send(200, "text/plain", "Resetting...");
  delay(1000);
  factoryReset();
});
```

HTML button:
```html
<button onclick="fetch('/reset', {method: 'POST'})">
  Factory Reset
</button>
```

## Best Practices

### For Beginners

1. ✅ Keep default 3-second hold time
2. ✅ Watch Serial Monitor for feedback
3. ✅ Use built-in BOOT button
4. ✅ Test reset before deploying device

### For Production

1. ✅ Increase hold time to 5-10 seconds
2. ✅ Add warning labels near reset button
3. ✅ Implement dual-button reset for critical devices
4. ✅ Log reset events to track troubleshooting
5. ✅ Add web-based reset with authentication

### For Battery-Powered

1. ✅ Reduce LED feedback to save power
2. ✅ Enter deep sleep after reset instead of restart
3. ✅ Consider wake-up button as reset button

## Security Considerations

### WiFi Password Storage

**Current Implementation:**
- Passwords stored in ESP32 flash (encrypted by hardware)
- Cleared completely on reset
- Not accessible via serial or web interface

**Security Notes:**
- Someone with physical access can reset device
- If device is stolen, reset prevents WiFi password theft
- Consider additional security for sensitive deployments

### Production Hardening

For high-security applications:

```cpp
// Require password for reset
bool authenticateReset() {
  Serial.println("Enter reset password:");
  // Wait for serial input
  String password = Serial.readStringUntil('\n');
  return (password == "MySecurePassword");
}

void factoryReset() {
  if (!authenticateReset()) {
    Serial.println("❌ Authentication failed!");
    return;
  }
  clearWiFiSettings();
  ESP.restart();
}
```

## Summary

The factory reset button provides:
- ✅ Easy recovery from configuration issues
- ✅ Safe device redeployment
- ✅ Protection against accidental resets
- ✅ Clear visual and serial feedback
- ✅ Customizable timing and behavior

**Quick Reference:**
- **Button:** GPIO 0 (BOOT button)
- **Hold Time:** 3 seconds
- **Feedback:** LED blinks + Serial messages
- **Result:** All WiFi settings cleared, restart in config mode

Now you can confidently reset and reconfigure your ESP32 anytime!
