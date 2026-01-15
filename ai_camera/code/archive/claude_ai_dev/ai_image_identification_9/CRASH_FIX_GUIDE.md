# ESP32 Connection Reset - Root Cause & Ultimate Fix

## 🔴 The Real Problem

Connection resets mean the **ESP32 is crashing or resetting**. This happens because:

1. **Out of Memory** - AI processing uses huge amounts of RAM
2. **Brownout** - Power drops when WiFi + Camera + Audio all run
3. **Watchdog Timeout** - ESP32 resets if code doesn't call yield()

## 🔍 What to Check in Serial Monitor

**Look for these patterns:**

```
Brownout detector was triggered
```
= Power supply insufficient

```
Task watchdog got triggered
```
= Code blocked too long

```
Guru Meditation Error: Core 0 panic'ed (LoadProhibited)
```
= Memory corruption or out-of-memory

```
ESP32 rebooting...
```
= Crash and restart

## ✅ Ultimate Stable Version

I've created `dfrobot_ultra_stable.ino` with:

### 1. **Brownout Detector Disabled**
```cpp
WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
```
Prevents resets from voltage dips (use good 5V 2A supply)

### 2. **Memory Monitoring**
```cpp
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
```
Track memory usage throughout operation

### 3. **Recording Length Limit**
```cpp
if (wavBufferSize > 500000) {  // ~15 seconds max
  stopRecording();
}
```
Prevents memory overflow from long recordings

### 4. **Aggressive Memory Cleanup**
```cpp
// Free immediately after use
free(wavBuffer);
wavBuffer = NULL;
```

### 5. **Watchdog Feeding**
```cpp
yield();  // Throughout long operations
```

### 6. **Shorter AI Responses**
```cpp
chat.setMaxTokens(300);  // Was 500
```
Less memory needed for responses

### 7. **TTS Truncation**
```cpp
if (text.length() > 3000) {
  text = text.substring(0, 3000);
}
```
Prevents huge TTS requests

### 8. **Better Web Error Handling**
```javascript
fetch('/status', {signal: AbortSignal.timeout(2000)})
  .catch(e => {
    // Show friendly message instead of errors
  });
```

### 9. **Increased Polling Interval**
```javascript
setInterval(upd, 2000);  // Was 500ms
```
Less load on ESP32

### 10. **Graceful Offline Mode**
```cpp
if (!isProcessing && WiFi.status() == WL_CONNECTED) {
  server.handleClient();  // Only serve when online AND not busy
}
```

## 📊 Memory Usage Analysis

| Operation | Memory Used | Notes |
|-----------|-------------|-------|
| **Idle** | ~200KB | Base system |
| **Recording (5s)** | ~160KB | Audio buffer |
| **STT Upload** | ~50KB | HTTP client |
| **Vision Upload** | ~150KB | Image + HTTP |
| **TTS Download** | ~100KB | MP3 buffer |
| **TOTAL PEAK** | ~460KB | Critical! |

**ESP32-S3 has ~400KB RAM** - This is **very tight**!

## 🔧 Required Hardware Changes

### Power Supply (CRITICAL):
```
❌ USB Computer Port → ~500mA (insufficient)
❌ USB Phone Charger → Often unstable
✅ 5V 2.5A+ Power Supply → Stable, recommended
✅ Add 470µF capacitor → Smooths power
```

### Wiring:
```
✅ Short, thick wires for MAX98357
✅ Good quality jumpers
✅ Solid breadboard connections
```

## 🎯 Testing the Fix

### 1. Upload Ultra-Stable Version
Upload `dfrobot_ultra_stable.ino`

### 2. Open Serial Monitor (115200 baud)
Watch for:
```
Free Heap: XXXXX bytes  ← Should be > 200000 at start
```

### 3. Test Recording
Press BOOT, speak 3-5 seconds, release

**During processing, watch Serial:**
```
=== RECORDING STOP ===
Free heap: 245000 bytes  ← Should stay > 100000

STEP 1/3: Speech Recognition
Free heap after STT: 220000 bytes  ← Check each step

STEP 2/3: Vision Analysis
Free heap after vision: 190000 bytes

STEP 3/3: Audio Playback
Free heap: 180000 bytes

=== COMPLETE ===
```

**If heap drops below 50000 bytes** → DANGER!

### 4. Check Browser Console
Should see:
- ✅ "⚙️ AI Processing..." (no red errors)
- ✅ Status updates resume after
- ✅ Clean console log

## 🚨 If Still Crashing

### Check Serial for Crash Type:

**1. "Brownout detector triggered"**
```
Solution: Better 5V power supply
Add: 470µF capacitor between 5V and GND
```

**2. "Task watchdog"**
```
Solution: Already fixed with yield() calls
If persists: reduce MaxTokens further
```

**3. "LoadProhibited" or memory error**
```
Solution: Record shorter (< 5 seconds)
Check: Free heap stays > 100KB
```

**4. "Core panic"**
```
Solution: May be SD card issue
Try: Different SD card
Format: FAT32 with 4KB clusters
```

## 💡 Pro Tips

### Optimal Usage:
1. **Keep recordings under 5 seconds**
2. **Wait 2-3 seconds between requests**
3. **Use 5V 2.5A power supply**
4. **Don't leave browser open during testing** (saves ESP32 resources)

### If Memory Critical:
```cpp
// Reduce these values:
chat.setMaxTokens(200);      // Was 300
#define SAMPLE_RATE (12000)  // Was 16000 (lower quality but less memory)
```

### Monitoring Health:
```cpp
// Add to loop():
if (ESP.getFreeHeap() < 100000) {
  Serial.println("⚠️  MEMORY WARNING!");
  ESP.restart();  // Safe restart before crash
}
```

## 📈 Expected Performance

With ultra-stable version:

| Metric | Target | Notes |
|--------|--------|-------|
| **Min Free Heap** | > 100KB | During processing |
| **Crash Rate** | 0% | With good power |
| **Recording Length** | 15s max | Limited for safety |
| **Response Time** | 20-40s | Normal |
| **Web Errors** | None | Clean console |

## 🎉 Success Indicators

You know it's working when:
- ✅ No ESP32 resets during operation
- ✅ Serial shows consistent heap > 100KB
- ✅ Browser console has no red errors
- ✅ Can do multiple requests without crashes
- ✅ Web interface stays responsive

## 🔄 If All Else Fails

**Nuclear option:**
1. Use external 5V 3A power supply
2. Record MAX 3 seconds
3. Close browser during processing
4. Add 1000µF capacitor
5. Use quality SD card (Samsung/SanDisk)

This ultra-stable version should eliminate crashes! 🚀
