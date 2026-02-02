# Timeout and Disconnect Handling - Technical Documentation

## Overview

This document explains the robust timeout and disconnect handling mechanisms built into the ESP32 WiFi Manager. These features ensure reliable network connectivity with automatic recovery from common failure scenarios.

## Architecture

### State Machine

```
[Power On]
    |
    v
[Load Config] --> [No Config] --> [Config Mode]
    |
    |[Config Exists]
    v
[Attempt Connect] --[Timeout]--> [Config Mode]
    |
    |[Success]
    v
[Connected] --[Disconnect Event]--> [Reconnect Logic]
    |                                      |
    |                                      v
    |                              [Try Reconnect]
    |                                      |
    |                              [Max Attempts?]
    |                                      |
    |                              [Yes]--> [Config Mode]
    |                              [No]---> [Wait & Retry]
    v
[Normal Operation]
```

## Timeout Mechanisms

### 1. Configuration Mode Timeout

**Purpose**: Prevents ESP32 from staying in config mode indefinitely.

**Parameters**:
```cpp
#define CONFIG_TIMEOUT 300000  // 5 minutes (300,000 ms)
```

**Behavior**:
- Timer starts when config mode begins
- If no configuration received within timeout period
- ESP32 automatically restarts
- Useful for unattended deployments

**Use Cases**:
- User forgets to configure device
- Configuration page isn't accessed
- Prevents device stuck in AP mode

**Code Location**:
```cpp
void loop() {
  if (configMode) {
    if (millis() - configStartTime > CONFIG_TIMEOUT) {
      Serial.println("Configuration timeout, restarting...");
      ESP.restart();
    }
  }
}
```

### 2. WiFi Connection Timeout

**Purpose**: Prevents infinite waiting during connection attempts.

**Parameters**:
```cpp
#define WIFI_CONNECT_TIMEOUT 20000  // 20 seconds
```

**Behavior**:
- Timeout applies to each connection attempt
- After 20 seconds, connection attempt fails gracefully
- System doesn't hang waiting for unreachable network
- Allows system to handle failure and take corrective action

**Scenarios Handled**:
- Wrong password (WPA handshake timeout)
- Network out of range
- Router not responding
- DHCP server issues

**Code Location**:
```cpp
void connectToWiFi() {
  WiFi.begin(config.ssid, config.password);
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && 
         millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Success
  } else {
    // Timeout - handle failure
  }
}
```

### 3. Reconnection Interval

**Purpose**: Controls how often reconnection is attempted.

**Parameters**:
```cpp
#define WIFI_RECONNECT_INTERVAL 30000  // 30 seconds
```

**Behavior**:
- Minimum time between reconnection attempts
- Prevents rapid connection flooding
- Balances responsiveness vs. power consumption
- Gives network/router time to recover

**Rationale**:
- Too fast: Wastes power, floods router
- Too slow: Extended downtime
- 30 seconds: Good balance for most scenarios

## Disconnect Handling

### WiFi Event System

The ESP32 provides a sophisticated event system that notifies the application of WiFi state changes in real-time.

**Registered Events**:

```cpp
void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      // Successfully obtained IP address
      break;
      
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      // Lost connection to AP
      handleDisconnect();
      break;
      
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      // Lost IP lease (DHCP renewal failed)
      break;
      
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      // Connected to AP (but no IP yet)
      break;
  }
}
```

### Disconnect Detection

**Methods**:

1. **Event-Based** (Primary):
   - WiFi library triggers `ARDUINO_EVENT_WIFI_STA_DISCONNECTED`
   - Immediate notification
   - Most reliable method

2. **Polling-Based** (Secondary):
   - Checks `WiFi.status()` every 5 seconds
   - Backup detection mechanism
   - Catches edge cases

```cpp
void loop() {
  if (millis() - lastWiFiCheck >= 5000) {
    lastWiFiCheck = millis();
    checkWiFiConnection();
  }
}
```

### Reconnection State Machine

**State Variables**:
```cpp
int reconnectAttempts = 0;           // Current attempt counter
bool shouldReconnect = true;          // Enable/disable reconnection
unsigned long lastReconnectAttempt = 0;  // Timestamp of last attempt
```

**Flow**:

1. **Disconnect Detected**
   ```
   WiFi.status() != WL_CONNECTED
   └─> Set shouldReconnect = true
   └─> Record timestamp
   └─> Reset attempt counter
   ```

2. **Wait Period**
   ```
   Check: (currentTime - lastAttempt) >= RECONNECT_INTERVAL
   └─> If true: Proceed to reconnection
   └─> If false: Keep waiting
   ```

3. **Reconnection Attempt**
   ```
   Increment reconnectAttempts
   └─> Call connectToWiFi()
   └─> Wait for result (with timeout)
       ├─> Success: Reset counter, resume normal operation
       └─> Failure: Check attempt count
           ├─> < MAX_ATTEMPTS: Schedule retry
           └─> >= MAX_ATTEMPTS: Enter config mode
   ```

### Reconnection Algorithm

```cpp
void attemptReconnect() {
  reconnectAttempts++;
  lastReconnectAttempt = millis();
  
  // Check if max attempts reached
  if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
    Serial.println("Max attempts reached");
    shouldReconnect = false;
    reconnectAttempts = 0;
    startConfigMode();  // Give user chance to reconfigure
    return;
  }
  
  // Attempt reconnection
  Serial.println("Reconnection attempt #" + String(reconnectAttempts));
  WiFi.disconnect();
  delay(100);
  connectToWiFi();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Reconnection successful!");
    reconnectAttempts = 0;  // Reset counter
  } else {
    Serial.println("Reconnection failed, will retry...");
  }
}
```

## Signal Quality Monitoring

### Purpose
Monitor WiFi signal strength to predict and prevent disconnections.

### Implementation

```cpp
void checkWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    
    if (rssi < -80) {
      Serial.println("⚠ Warning: Weak WiFi signal (" + String(rssi) + " dBm)");
    }
  }
}
```

### RSSI Interpretation

| RSSI (dBm) | Quality | Description | Action |
|------------|---------|-------------|---------|
| -30 to -50 | Excellent | Strong signal | None |
| -50 to -60 | Good | Adequate signal | None |
| -60 to -70 | Fair | Minimum reliable | Monitor |
| -70 to -80 | Weak | Marginal | Warning |
| -80 to -90 | Very Weak | Unreliable | Alert |
| < -90 | No Signal | Connection failing | Reconnect |

### Predictive Actions

Based on signal strength, the system can:
1. Log warnings for weak signals
2. Increase reconnection frequency
3. Suggest static IP (reduces DHCP overhead)
4. Prompt user to move device closer to router

## Configuration Parameters

### Recommended Settings by Use Case

**Home/Office (Stable Network)**:
```cpp
#define WIFI_CONNECT_TIMEOUT 20000
#define WIFI_RECONNECT_INTERVAL 30000
#define MAX_RECONNECT_ATTEMPTS 5
```

**Mobile/Vehicle (Unstable Network)**:
```cpp
#define WIFI_CONNECT_TIMEOUT 15000        // Faster timeout
#define WIFI_RECONNECT_INTERVAL 10000     // More frequent attempts
#define MAX_RECONNECT_ATTEMPTS 10         // More patience
```

**Battery-Powered (Power Saving)**:
```cpp
#define WIFI_CONNECT_TIMEOUT 30000        // Allow longer connection time
#define WIFI_RECONNECT_INTERVAL 120000    // Reconnect every 2 minutes
#define MAX_RECONNECT_ATTEMPTS 3          // Fail faster, save power
```

**Industrial/Critical (High Reliability)**:
```cpp
#define WIFI_CONNECT_TIMEOUT 30000
#define WIFI_RECONNECT_INTERVAL 5000      // Very aggressive
#define MAX_RECONNECT_ATTEMPTS 999        // Effectively infinite
```

## Error Handling

### Connection Failure Reasons

The ESP32 WiFi library provides status codes:

```cpp
typedef enum {
    WL_IDLE_STATUS      = 0,  // WiFi is idle
    WL_NO_SSID_AVAIL    = 1,  // SSID not found
    WL_SCAN_COMPLETED   = 2,  // Scan complete
    WL_CONNECTED        = 3,  // Successfully connected
    WL_CONNECT_FAILED   = 4,  // Connection failed
    WL_CONNECTION_LOST  = 5,  // Connection lost
    WL_DISCONNECTED     = 6,  // Disconnected
} wl_status_t;
```

### Logging Strategy

```cpp
void handleDisconnect() {
  Serial.println("\n=== Handling Disconnect ===");
  Serial.println("Disconnect reason: " + String(WiFi.status()));
  
  // Log additional diagnostics
  Serial.println("RSSI at disconnect: " + String(WiFi.RSSI()));
  Serial.println("Uptime: " + String(millis() / 1000) + " seconds");
  
  // Trigger reconnection logic
  lastReconnectAttempt = millis();
  shouldReconnect = true;
}
```

## Power Management Considerations

### Impact on Battery Life

**Connection Attempts**:
- Active WiFi radio: ~200-300mA
- Each 20-second attempt: ~1.1-1.7 mAh
- 5 attempts: ~5.5-8.5 mAh

**Optimization Strategies**:

1. **Increase Interval**: Longer waits = less power
2. **Reduce Attempts**: Fail faster, enter deep sleep
3. **Deep Sleep Between Attempts**:
   ```cpp
   if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
     ESP.deepSleep(300e6);  // Sleep 5 minutes, then retry
   }
   ```

## Testing Scenarios

### Test 1: Router Reboot
```
Action: Reboot WiFi router
Expected: 
  - Disconnect detected within 5 seconds
  - First reconnection attempt after 30 seconds
  - Successful reconnection when router online
Result: ✓ Pass
```

### Test 2: Out of Range
```
Action: Move ESP32 far from router
Expected:
  - Signal degradation warnings
  - Eventual disconnect
  - Reconnection attempts (will fail)
  - Config mode after max attempts
Result: ✓ Pass
```

### Test 3: Wrong Password
```
Action: Change router password
Expected:
  - Disconnect on router change
  - Failed reconnection attempts (authentication error)
  - Config mode after max attempts
  - User can enter new password
Result: ✓ Pass
```

### Test 4: Network Congestion
```
Action: Heavy network load
Expected:
  - Possible slow responses
  - Connection maintained
  - RSSI fluctuations logged
Result: ✓ Pass
```

### Test 5: Power Cycle
```
Action: Remove and restore power
Expected:
  - Load saved config from EEPROM
  - Automatic connection on boot
  - Resume normal operation
Result: ✓ Pass
```

## Advanced Features

### Custom Disconnect Handler

You can add custom logic when disconnect occurs:

```cpp
void handleDisconnect() {
  if (configMode) return;
  
  // Your custom actions
  digitalWrite(STATUS_LED, LOW);     // Turn off status LED
  stopWebServer();                   // Stop web server
  pauseDataLogging();                // Pause sensor logging
  sendDisconnectAlert();             // Send notification
  
  // Standard reconnection logic
  lastReconnectAttempt = millis();
  shouldReconnect = true;
}
```

### Graceful Degradation

When WiFi is unavailable, your application can:

1. **Queue Data Locally**:
   ```cpp
   if (WiFi.status() != WL_CONNECTED) {
     saveDataToSD(sensorData);  // Save to SD card
   } else {
     sendDataToServer(sensorData);  // Send when connected
   }
   ```

2. **Switch to Backup Network**:
   ```cpp
   if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
     tryBackupNetwork();  // Try alternate WiFi
   }
   ```

3. **Enter Low-Power Mode**:
   ```cpp
   if (WiFi.status() != WL_CONNECTED) {
     reduceProcessingRate();  // Slow down CPU
     disableNonEssentialSensors();
   }
   ```

## Debugging

### Enable Verbose Logging

Add this to see detailed WiFi events:

```cpp
void setup() {
  Serial.begin(115200);
  WiFi.setAutoReconnect(false);  // We handle reconnection manually
  
  // Enable WiFi debug output
  esp_log_level_set("wifi", ESP_LOG_VERBOSE);
}
```

### Monitor Connection Quality

```cpp
void printWiFiDiagnostics() {
  Serial.println("\n=== WiFi Diagnostics ===");
  Serial.println("Status: " + String(WiFi.status()));
  Serial.println("SSID: " + String(WiFi.SSID()));
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
  Serial.println("Channel: " + String(WiFi.channel()));
  Serial.println("Gateway: " + WiFi.gatewayIP().toString());
  Serial.println("DNS: " + WiFi.dnsIP().toString());
  Serial.println("MAC: " + WiFi.macAddress());
}
```

## Best Practices

1. **Always Use Timeouts**: Never wait indefinitely
2. **Log Everything**: Helps debug real-world issues
3. **Test Edge Cases**: Router reboots, out of range, etc.
4. **Monitor Signal**: Prevent issues before they occur
5. **Fail Gracefully**: Always have a recovery path
6. **User Feedback**: Indicate connection status visually (LED)
7. **Document Behavior**: Clear serial output helps users debug

## Conclusion

This comprehensive timeout and disconnect handling system ensures:
- ✅ Robust operation in unstable networks
- ✅ Automatic recovery from common failures
- ✅ User-friendly fallback to configuration mode
- ✅ Predictable behavior with configurable parameters
- ✅ Production-ready reliability

The system is designed to "just work" for beginners while providing advanced customization options for experienced users.
