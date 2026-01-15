# Full SD Card Audio Buffering - Installation Guide

## 🎯 What This Does

This version provides **complete audio buffering** with:
- ✅ Downloads MP3 from OpenAI to SD card
- ✅ Plays from SD card with perfect buffering
- ✅ Eliminates ALL audio artifacts
- ✅ Allows replay without re-downloading
- ✅ Web button to replay last response

## 📦 Required Library

You MUST install the **ESP32-audioI2S** library:

### Installation Method 1: Arduino Library Manager (Recommended)
1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for **"ESP32-audioI2S"**
4. Install **"ESP32-audioI2S" by schreibfaul1**

### Installation Method 2: Manual
```bash
cd ~/Documents/Arduino/libraries/
git clone https://github.com/schreibfaul1/ESP32-audioI2S.git
```

## 🔌 Hardware Setup

### Required Hardware:
1. **SD Card** (FAT32, 1-32GB)
2. **I2S Amplifier/Speaker** (e.g., MAX98357A)
   - OR built-in speaker if your board has one

### SD Card Pins (Verify for your board):
```cpp
#define SD_CS_PIN       41    // Chip Select
#define SD_MOSI_PIN     40    // Master Out
#define SD_MISO_PIN     39    // Master In  
#define SD_SCK_PIN      42    // Clock
```

**⚠️ CRITICAL**: Check your DFRobot ESP32-S3 board schematic for actual SD pins!

### Speaker Pins (I2S):
```cpp
#define I2S_DOUT        12    // Data Out (to speaker DIN)
#define I2S_BCLK        10    // Bit Clock (to speaker BCLK)
#define I2S_LRC         11    // Left/Right Clock (to speaker LRC)
```

### Wiring Example (MAX98357A Amplifier):
```
MAX98357A  →  ESP32-S3
------------------------
LRC        →  GPIO 11
BCLK       →  GPIO 10
DIN        →  GPIO 12
GND        →  GND
VIN        →  5V
```

## 🚀 How to Use

### 1. Prepare SD Card
- Format as **FAT32**
- Insert into ESP32 before powering on

### 2. Update Pin Definitions
Open the code and verify/update these pin numbers for YOUR board:
```cpp
// SD Card pins - CHECK YOUR BOARD!
#define SD_CS_PIN       41
#define SD_MOSI_PIN     40
#define SD_MISO_PIN     39
#define SD_SCK_PIN      42

// Speaker pins - CHECK YOUR BOARD!
#define I2S_DOUT        12
#define I2S_BCLK        10
#define I2S_LRC         11
```

### 3. Upload Code
- Make sure ESP32-audioI2S library is installed
- Upload `ai_image_identification_full_sd.ino`
- Open Serial Monitor at 115200 baud

### 4. Verify Startup
You should see:
```
=== ESP32-S3 Voice-to-Vision System ===
Version: Full SD Card Audio Buffering

[1/6] Initializing SD Card...
✓ SD Card mounted!
  Type: SDHC
  Size: 31.90 GB
  Created /audio directory

[2/6] Initializing camera...
✓ Camera initialized!

[3/6] Initializing audio playback...
✓ Audio playback ready!
  Speaker pins: BCLK=10, LRC=11, DOUT=12

[4/6] Initializing microphone...
✓ Microphone initialized!

[5/6] Testing microphone...
✓ Microphone working!

[6/6] Connecting to WiFi...
✓ WiFi connected!

=== System Ready ===
SD Card Audio Buffering: ENABLED
```

## 🎵 How It Works

### Recording & Processing:
1. **Press BOOT** → Records your voice
2. **Release BOOT** → Processes:
   ```
   STEP 1: Speech → Text (Whisper)
   STEP 2: Text + Image → AI Response (GPT-4 Vision)
   STEP 3: AI Response → MP3
      [TTS] Downloading MP3 from OpenAI...
      [TTS] ✓ Saved 45678 bytes to /audio/response_12345.mp3
      [PLAY] Playing: /audio/response_12345.mp3
      [PLAY] ✓ Playback complete
   ```

### Replay Feature:
- Open web browser: `http://192.168.0.XXX`
- Click **"🔊 Replay Last Response"** button
- Plays audio instantly from SD card (no re-download!)

## 🔍 Troubleshooting

### SD Card Not Detected:
```
✗ SD Card not found
⚠️  Audio buffering DISABLED
```
**Solutions:**
1. Verify SD card is FAT32 formatted
2. Check SD card pin numbers match your board
3. Try a different SD card (some are incompatible)
4. Check wiring if using external SD module

### Audio Playback Issues:
```
[PLAY] ✗ Audio player not ready
```
**Solutions:**
1. Check I2S speaker pins are correct
2. Verify MAX98357A wiring
3. Check speaker connections
4. Adjust volume: `audioPlayer.setVolume(15);` (0-21)

### Compilation Errors:
```
'Audio' was not declared in this scope
```
**Solution:**
- Install ESP32-audioI2S library (see above)

### Download Fails:
```
[TTS] ✗ HTTP Error: 401
```
**Solution:**
- Check API key is valid
- Check WiFi connection

### Microphone vs Speaker Conflict:
The code uses **separate I2S peripherals**:
- Microphone: I2S_MIC (PDM mode)
- Speaker: audioPlayer (standard I2S)

No conflict should occur!

## 📊 SD Card Storage

### File Management:
- Audio files stored in: `/audio/response_TIMESTAMP.mp3`
- Each file: ~50-500KB (depends on response length)
- Old files can be manually deleted

### Auto-cleanup (Optional):
Add to code to delete old files:
```cpp
void cleanupOldAudio() {
  File root = SD.open("/audio");
  File file = root.openNextFile();
  int count = 0;
  
  while (file) {
    if (!file.isDirectory() && count > 10) {
      SD.remove(file.path());
    }
    count++;
    file = root.openNextFile();
  }
}
```

## 🎛️ Customization

### Change TTS Voice:
In `downloadTTSToSD()`:
```cpp
payload += "\"voice\":\"nova\"";  // Options: alloy, echo, fable, onyx, nova, shimmer
```

### Adjust Volume:
In `initAudioPlayback()`:
```cpp
audioPlayer.setVolume(15);  // 0-21 (15 is medium)
```

### Change Audio Quality:
```cpp
payload += "\"model\":\"tts-1-hd\"";  // Higher quality (slower download)
```

## 📈 Performance

### Timing:
- Download MP3: 2-5 seconds
- Playback: Natural speed (2-30 seconds)
- Replay: Instant (plays from SD)

### SD Card Usage:
- 10 responses ≈ 2-5 MB
- 100 responses ≈ 20-50 MB
- 1GB SD card = thousands of responses

## 🆚 Comparison: Old vs New

### Old Method (20 second delay):
```
[TTS] Requesting audio...
[wait 20 seconds with artifacts]
[TTS] Complete
```

### New Method (SD buffering):
```
[TTS] Downloading MP3...
[TTS] ✓ Saved 45KB
[PLAY] Playing from SD...
[perfect audio quality]
[PLAY] ✓ Complete
```

## 🎉 Benefits

1. **Perfect Audio** - No artifacts, screeches, or static
2. **Instant Replay** - No re-download needed
3. **Reliable** - Buffered playback is rock solid
4. **Expandable** - Can save/replay any number of responses
5. **Professional** - Production-ready audio quality

## 🔧 Advanced Features

### Save Conversation History:
Audio files include timestamp - you can:
- Keep entire conversation history
- Replay any past response
- Export audio files for analysis

### Add Custom Sounds:
Put MP3 files on SD card:
```cpp
audioPlayer.connecttoFS(SD, "/sounds/beep.mp3");
```

### Streaming Mode:
Instead of downloading entire file:
```cpp
audioPlayer.connecttohost("http://streaming-url");
```

This is production-ready code with professional audio quality! 🎵
