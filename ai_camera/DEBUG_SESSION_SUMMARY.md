# AI Camera Debug Session Summary
**Date:** February 3, 2026

## What We Were Trying To Do

Fix the WiFi setup captive portal page so users could:
1. Scan for WiFi networks
2. Manually type a network name (for hidden SSIDs)
3. Enter OpenAI API key
4. Connect the camera to their network

## Issues Encountered

### Issue 1: JavaScript Functions Not Working
**Symptom:** Clicking "Scan for Networks" button showed error:
```
Uncaught ReferenceError: scanNetworks is not defined
```

**Root Cause:** The browser couldn't parse the JavaScript because of two problems:
1. Google Fonts `@import` failed (no internet during AP mode)
2. C++ compiler injecting `#line` preprocessor directives into the HTML

### Issue 2: `#line` Preprocessor Directives in PROGMEM
**Symptom:** Browser showed error:
```
Uncaught SyntaxError: Private field '#line' must be declared in an enclosing class
```

**Root Cause:** ESP32 Arduino Core 3.3.5 was inserting debug line markers like:
```
#line 430 "/path/to/ai_camera_unified.ino"
```
directly into the raw string literals stored in PROGMEM. These appeared in the middle of the JavaScript code, breaking parsing.

**Found in binary:**
```bash
strings firmware.bin | grep "#line"
# Output showed multiple #line directives embedded in the HTML
```

### Issue 3: SSID Field Was Read-Only
**Symptom:** Users couldn't manually type a network name if scan didn't find it.

**Root Cause:** HTML had `readonly` attribute:
```html
<input type="text" id="ssid" ... readonly>
```

## Attempted Fixes

### Fix 1: Remove Google Fonts (SAFE - WORKED)
Changed line 227:
```css
/* Before */
@import url('https://fonts.googleapis.com/css2?family=Outfit...');

/* After */
/* Using system fonts - no internet during setup */
```

### Fix 2: Make SSID Editable (SAFE - WORKED)
Changed line 371:
```html
<!-- Before -->
<input type="text" id="ssid" placeholder="Select a network above" readonly>

<!-- After -->
<input type="text" id="ssid" placeholder="Select or type network name">
```

### Fix 3: Use System Fonts (SAFE - WORKED)
```css
/* Before */
font-family: 'Outfit', sans-serif;
font-family: 'Fira Code', monospace;

/* After */
font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
font-family: 'SF Mono', Monaco, 'Courier New', monospace;
```

### Fix 4: Remove #line with -P Flag (BROKE THE DEVICE)
```bash
arduino-cli compile --build-property "compiler.cpp.extra_flags=-P" ...
```

**Result:** The `-P` flag removed the #line directives BUT also affected other preprocessor behavior, causing the firmware to not boot (no WiFi network created, no LED activity).

### Fix 5: Change Raw Literal Delimiter (DID NOT HELP)
Changed from `R"rawliteral(..."` to `R"HTMLPAGE(..."` - didn't fix the #line issue.

## Compilation Command Reference

**Working compilation (but has #line bug):**
```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi" \
  --output-dir ./build \
  .
```

**Board options explained:**
- `CDCOnBoot=cdc` - USB serial works on boot
- `FlashSize=16M` - 16MB flash chip
- `PartitionScheme=app3M_fat9M_16MB` - 3MB app, 9.9MB storage
- `PSRAM=opi` - Enable 8MB external RAM (needed for camera)

## Files Modified (NOT COMMITTED)

1. `ai_camera/code/ai_camera_unified/ai_camera_unified.ino`
   - Line 227: Removed Google Fonts import
   - Line 371: Removed readonly from SSID input
   - Lines 242, 287, 299: Changed to system fonts
   - Line 219, 578: Changed raw literal delimiter

2. `ai_camera/tools/serial_monitor.sh` (NEW)
3. `ai_camera/tools/monitor.py` (NEW)

## What Still Needs To Be Fixed

### The #line Problem
The ESP32 Arduino Core 3.3.5 injects `#line` directives into raw string literals. Possible solutions:

1. **Store HTML in LittleFS/SPIFFS** instead of PROGMEM
2. **Minify JavaScript** to single lines (no function blocks)
3. **Use escaped strings** instead of raw literals
4. **Downgrade ESP32 Arduino Core** to older version without this bug
5. **Post-process the binary** to remove #line directives
6. **Report bug** to ESP32 Arduino Core maintainers

### Resources for Future Fix
- ESP32 Arduino Core GitHub: https://github.com/espressif/arduino-esp32
- ESP Web Tools: https://esphome.github.io/esp-web-tools/
- Web Serial API: https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API

## Serial Debugging Setup

Created tools for future debugging:
```bash
# Monitor serial output
python3 ai_camera/tools/monitor.py --log debug.log

# Or simple version
./ai_camera/tools/serial_monitor.sh
```

## Conclusion

The code changes (removing Google Fonts, making SSID editable, using system fonts) are correct and safe. The blocker is the `#line` preprocessor directive bug in ESP32 Arduino Core 3.3.5 that corrupts JavaScript in PROGMEM raw string literals. The `-P` compiler flag removes this but breaks other functionality.

Next steps should focus on alternative HTML storage methods or finding a proper fix for the #line injection issue.
