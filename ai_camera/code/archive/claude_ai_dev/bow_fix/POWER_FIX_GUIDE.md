# Brownout Detector Error - Power Supply Fix

## Error Message
```
E BOD: Brownout detector was triggered
```

This means the voltage dropped below ~2.43V, causing the ESP32 to reset for safety.

## Why This Happens

The ESP32-S3 with camera draws significant current, especially during:
- Camera initialization
- WiFi connection
- Taking photos
- Combined operations

**Typical current draw:**
- Idle: ~80mA
- WiFi active: ~150-200mA
- Camera active: +100-150mA
- **Peak (camera init + WiFi)**: 400-500mA

USB ports often can't provide stable current above 500mA.

## Solutions

### Solution 1: Better USB Cable ⚡
**Try this first!**
- Use a **short, thick USB cable** (< 3 feet)
- Thin/long cables have voltage drop
- Try a different USB cable

### Solution 2: Different USB Port 🔌
- **Avoid USB hubs** - connect directly to computer
- Try **USB 3.0 ports** (blue) - better power delivery
- Try **rear panel USB ports** on desktop (better than front)
- Try **powered USB hub** with external adapter

### Solution 3: External 5V Power Supply (BEST) 🔋
Use a quality 5V power adapter:
- **Minimum**: 5V 2A (2000mA)
- **Recommended**: 5V 2.5A or 3A
- Connect to **5V and GND pins** on ESP32

**Good options:**
- Phone charger (5V 2A minimum)
- USB wall adapter rated 2A+
- Bench power supply set to 5V

### Solution 4: Reduce Camera Resolution 📷
Lower resolution = less power during init:

In `camera.h`, change:
```cpp
config.frame_size = FRAMESIZE_QVGA;  // 320x240 instead of VGA
config.jpeg_quality = 15;             // Lower quality = less processing
```

### Solution 5: Disable Brownout Detector (Risky) ⚠️
**Already in the new code**, but be careful:
- This disables the safety feature
- ESP32 may behave erratically if voltage is too low
- Only use with decent power supply

The new code (`imageAnswering_brownout_fix.ino`) disables it at the very start of setup().

### Solution 6: Add Capacitor (Hardware Fix) 🔧
Add a **100-470µF capacitor** across 5V and GND near the ESP32:
- Smooths power spikes
- Prevents voltage dips during camera init
- Use electrolytic capacitor rated 16V or higher
- **Watch polarity!** (+ to 5V, - to GND)

## Testing Power Supply

Upload this test sketch to measure voltage:
```cpp
void setup() {
  Serial.begin(115200);
}

void loop() {
  float voltage = analogRead(A0) * (3.3 / 4095.0) * 2;  // Adjust multiplier
  Serial.printf("Voltage: %.2fV\n", voltage);
  delay(1000);
}
```

Voltage should stay above **4.5V** during operation.

## Recommended Approach

1. **Upload** `imageAnswering_brownout_fix.ino` (brownout disabled)
2. **Try different USB cable** first
3. **Try direct USB 3.0 port** on computer
4. **If still resetting**: Use external 5V 2A+ power supply
5. **Monitor Serial output** - should boot fully now

## What to Expect After Fix

With proper power, you should see:
```
=== ESP32-S3 Voice-to-Vision System ===
Version: Camera Rotated 180° + Brownout Fix

[1/4] Initializing camera...
PSRAM found! Using PSRAM for frame buffers
Camera initialized successfully!
✓ Camera rotated 180 degrees
✓ Camera initialized with 180° rotation!

[2/4] Initializing audio input...
I2S Config: Clock=GPIO38, Data=GPIO39
✓ I2S initialized
✓ Audio input initialized!

[3/4] Testing microphone...
Microphone test: XXXXX bytes captured
✓ Microphone working!

[4/4] Connecting to WiFi...
.....
✓ WiFi connected!
IP Address: 192.168.0.XXX
```

If you see this complete output, the power issue is resolved!

## Long-term Solution

For a production device:
- Use quality 5V 2A+ power adapter
- Add 100µF+ capacitor near ESP32
- Use proper power regulation circuit
- Consider battery with voltage regulator

## Still Having Issues?

If brownout persists even with good power:
1. Check camera module connection (loose = draws more current)
2. Try lower camera resolution (QVGA instead of VGA)
3. Initialize camera after WiFi (spreads out power demand)
4. Check for short circuits on the board
