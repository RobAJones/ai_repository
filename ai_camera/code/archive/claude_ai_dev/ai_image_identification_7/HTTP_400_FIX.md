# HTTP 400 Error Fix - TTS Issue

## 🐛 Problem

```
[TTS] ✗ HTTP Error: 400
```

HTTP 400 = "Bad Request" - means the JSON payload sent to OpenAI is malformed.

## 🔍 Cause

The AI response text contained special characters (quotes, newlines, etc.) that weren't properly escaped in the JSON payload. For example:

```json
{
  "model": "tts-1",
  "input": "The image shows a "red" cup.",  ← Unescaped quotes broke JSON
  "voice": "alloy"
}
```

## ✅ Solution Applied

Added `escapeJSON()` function that properly escapes:
- Double quotes: `"` → `\"`
- Backslashes: `\` → `\\`
- Newlines: `\n` → `\\n`
- Tabs: `\t` → `\\t`
- Control characters: Converted to Unicode escape sequences

## 📝 Changes Made

**Before (broken):**
```cpp
String payload = "{";
payload += "\"model\":\"tts-1\",";
payload += "\"input\":\"" + text + "\",";  // ← Direct concatenation
payload += "\"voice\":\"alloy\"";
payload += "}";
```

**After (fixed):**
```cpp
String escapedText = escapeJSON(text);  // ← Escape special chars

String payload = "{";
payload += "\"model\":\"tts-1\",";
payload += "\"input\":\"" + escapedText + "\",";  // ← Use escaped text
payload += "\"voice\":\"alloy\"";
payload += "}";
```

## 🚀 How to Apply

Upload the new file: `dfrobot_hardware_verified_fixed.ino`

The fix is backward compatible - it works with all text, whether it has special characters or not.

## 🧪 Test Cases

The escapeJSON function now handles:

✅ `The image shows "text" in quotes`  
✅ `Line 1\nLine 2` (newlines)  
✅ `C:\path\to\file` (backslashes)  
✅ `Tab\tseparated\tvalues`  
✅ Control characters (ASCII < 0x20)  

## 📊 Expected Output After Fix

```
━━━ STEP 3/3: AUDIO PLAYBACK ━━━

╔═══════════════════════════════════════╗
║   TEXT-TO-SPEECH (SD BUFFERED)        ║
╚═══════════════════════════════════════╝

[TTS] Requesting MP3 from OpenAI...
[TTS] Text length: 253 characters
[TTS] ✓ Downloading MP3 to SD card...
[TTS] Progress: ▓▓▓▓▓▓▓▓▓
[TTS] ✓ Downloaded 45678 bytes (44.61 KB)
[TTS] Saved to: /audio/resp_12345.mp3

[PLAY] 🔊 Playing through MAX98357...
[PLAY] File: /audio/resp_12345.mp3
[PLAY] ♪♪♪♪♪
[PLAY] ✓ Playback complete (4.2 seconds)

[TTS] ✓ Audio cached - available for replay

✓ Audio playback successful
```

## 🔧 Alternative Solution

If you still get HTTP 400 errors, it might be the API key. Check:

1. **API Key Format**: Should start with `sk-proj-...`
2. **API Key Valid**: Not expired or revoked
3. **API Credits**: Account has credits available
4. **Character Limit**: OpenAI TTS has 4096 character limit

To test if it's the API key or JSON:
```cpp
Serial.printf("[DEBUG] Escaped text: %s\n", escapedText.c_str());
```

This will show you exactly what's being sent to OpenAI.

## 📚 Additional Notes

### OpenAI TTS API Limits:
- **Max input**: 4,096 characters
- **Rate limit**: 50 requests/minute
- **File size**: Typically 20-100KB per response

### If Text is Too Long:
The code could be enhanced to truncate:
```cpp
if (text.length() > 4000) {
  text = text.substring(0, 4000) + "...";
  Serial.println("[TTS] ⚠️  Text truncated to 4000 chars");
}
```

This fix should resolve the HTTP 400 error! 🎉
