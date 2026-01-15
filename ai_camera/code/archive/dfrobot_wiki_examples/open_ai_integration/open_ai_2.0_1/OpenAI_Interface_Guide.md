# ESP32-S3 OpenAI Interface - Implementation Guide

## Overview

This interface recreates the design shown in your screenshot with six main functional areas:

### Layout Structure

```
┌─────────────────────────┬─────────────────────┐
│                         │  Audio Controls     │
│   Video Stream /        │  + File List        │
│   Image Review          │                     │
│   (Upper Left)          │  (Upper Right)      │
│                         │                     │
├─────────────────────────┼─────────────────────┤
│                         │  Image Files        │
│                         │  + Export Label     │
│   Message Display       ├─────────────────────┤
│   (OpenAI Response)     │  Audio Files        │
│                         │  + Export Label     │
│   (Lower Left)          │  (Middle Right)     │
│                         │                     │
├─────────────────────────┼─────────────────────┤
│   Send / Replay / Save  │  Return Text List   │
│   (Bottom Left)         │  Return Audio List  │
│                         │  (Lower Right)      │
└─────────────────────────┴─────────────────────┘
```

## Interface Components

### 1. Upper Left - Video/Image Display Area
- **Video Stream Button**: Toggle live camera feed on/off
- **Capture Button**: Take a photo and save to SD card
- **Review Button**: Display the most recently captured image
- **Display Area**: Shows live stream or review image

### 2. Upper Right - Audio Controls
- **Audio Playback Bar**: Shows selected audio file player
- **Record Button**: Click to start timer, click again to stop and capture audio
- **Playback Button**: Play selected audio through MAX98357A speaker
- **Delete Button**: Remove selected audio file
- **Recording Timer**: Shows elapsed time during recording

### 3. Middle Right - File Management (Split into two columns)

#### Left Column - Image Files
- **File List**: Clickable list of all captured images
- **Save Button**: Add selected image to "Image for Export" 
- **Delete Button**: Remove selected image from SD card
- **Export Label**: Shows which image is queued for OpenAI

#### Right Column - Audio Files  
- **File List**: Clickable list of all recorded audio files
- **Save Button**: Add selected audio to "Audio for Export"
- **Delete Button**: Remove selected audio from SD card
- **Export Label**: Shows which audio is queued for OpenAI

### 4. Lower Left - Message Display
- **Message Display Box**: Large text area showing OpenAI analysis results
- **Send Button**: Send selected image + audio to OpenAI for analysis
- **Replay Button**: Replay the audio in the browser player
- **Save Button**: Save the analysis result to SD card

### 5. Lower Right - Return Lists (Split into two columns)

#### Left Column - Return Text
- List of previous OpenAI text responses
- Click any item to view in message display

#### Right Column - Return Audio
- List of previous OpenAI audio responses (if TTS enabled)
- Click any item to play audio

## Workflow

### Standard Operation Flow:

1. **Capture Media**
   - Use "Video Stream" to preview
   - Click "Capture" to take photo → appears in Image Files list
   - Click image filename to select it
   - Click "Save" to move to "Image for Export"

2. **Record Audio**
   - Click "Record" → timer starts
   - Speak your message
   - Click "Record" again → device captures that duration of audio
   - New file appears in Audio Files list
   - Click filename to select and preview
   - Click "Save" to move to "Audio for Export"

3. **Send to OpenAI**
   - Verify "Image for Export" and "Audio for Export" show your selections
   - Click "Send" button
   - Wait for processing
   - Response appears in "Returned Test Message Display"
   - Response is automatically added to "Return Test" list

4. **Review & Save**
   - Click items in Return Test list to view previous responses
   - Use "Replay" to hear audio again
   - Use "Save" to store analysis on SD card

## Implementation Details

### Current Status

The provided sketch includes:
- ✅ Complete web interface matching the design
- ✅ Camera capture and streaming
- ✅ Audio recording with 10x amplification
- ✅ File management (CRUD operations)
- ✅ Speaker playback through MAX98357A
- ✅ Export queue system
- ⚠️ OpenAI integration (placeholder - needs implementation)

### OpenAI Integration (To Be Implemented)

The sketch includes a placeholder `handleOpenAIAnalyze()` function. To complete the integration:

#### Requirements:
1. OpenAI API key (add to `openai_api_key` variable)
2. HTTPS client library for secure connections
3. JSON parsing library (ArduinoJson recommended)

#### Implementation Steps:

```cpp
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

void handleOpenAIAnalyze() {
  // Parse request
  StaticJsonDocument<200> reqDoc;
  deserializeJson(reqDoc, server.arg("plain"));
  
  String imageFile = reqDoc["image"];
  String audioFile = reqDoc["audio"];
  
  // 1. Read image file and convert to base64
  File imgFile = SD_MMC.open("/images/" + imageFile, FILE_READ);
  size_t imgSize = imgFile.size();
  uint8_t* imgData = (uint8_t*)malloc(imgSize);
  imgFile.read(imgData, imgSize);
  imgFile.close();
  
  String imageBase64 = base64Encode(imgData, imgSize);
  free(imgData);
  
  // 2. Read audio file and convert to base64
  File audFile = SD_MMC.open("/audio/" + audioFile, FILE_READ);
  size_t audSize = audFile.size();
  uint8_t* audData = (uint8_t*)malloc(audSize);
  audFile.read(audData, audSize);
  audFile.close();
  
  String audioBase64 = base64Encode(audData, audSize);
  free(audData);
  
  // 3. Build OpenAI API request
  WiFiClientSecure client;
  client.setInsecure(); // For testing - use proper cert in production
  
  HTTPClient https;
  https.begin(client, "https://api.openai.com/v1/chat/completions");
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", "Bearer " + String(openai_api_key));
  
  // Build JSON request body
  StaticJsonDocument<4096> apiDoc;
  apiDoc["model"] = "gpt-4o"; // or "gpt-4-turbo" for vision
  
  JsonArray messages = apiDoc.createNestedArray("messages");
  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  
  JsonArray content = userMsg.createNestedArray("content");
  
  // Add image
  JsonObject imgPart = content.createNestedObject();
  imgPart["type"] = "image_url";
  JsonObject imgUrl = imgPart.createNestedObject("image_url");
  imgUrl["url"] = "data:image/jpeg;base64," + imageBase64;
  
  // Add text prompt
  JsonObject textPart = content.createNestedObject();
  textPart["type"] = "text";
  textPart["text"] = "Analyze this image and describe what you see. Also, I have an audio recording - please provide insights based on both.";
  
  // Note: OpenAI doesn't directly support audio in vision API
  // You would need to:
  // Option A: Use Whisper API separately to transcribe audio first
  // Option B: Send audio description in text
  // Option C: Use a multimodal model that supports both
  
  String requestBody;
  serializeJson(apiDoc, requestBody);
  
  // 4. Send request
  int httpResponseCode = https.POST(requestBody);
  
  if (httpResponseCode == 200) {
    String response = https.getString();
    
    // Parse response
    StaticJsonDocument<4096> respDoc;
    deserializeJson(respDoc, response);
    
    String aiResponse = respDoc["choices"][0]["message"]["content"];
    
    // Send back to client
    String jsonResp = "{\"success\":true,\"response\":\"" + aiResponse + "\"}";
    server.send(200, "application/json", jsonResp);
  } else {
    String error = "HTTP Error: " + String(httpResponseCode);
    server.send(500, "application/json", 
      "{\"success\":false,\"error\":\"" + error + "\"}");
  }
  
  https.end();
}
```

#### Audio Processing Options:

**Option 1: Whisper API (Recommended)**
```cpp
// Transcribe audio first
HTTPClient https;
https.begin(client, "https://api.openai.com/v1/audio/transcriptions");
https.addHeader("Authorization", "Bearer " + String(openai_api_key));

// Create multipart form data
String boundary = "----WebKitFormBoundary" + String(random(1000000));
https.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

// Build multipart body with audio file
String body = "--" + boundary + "\r\n";
body += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
body += "Content-Type: audio/wav\r\n\r\n";
// Append audio data
body += "\r\n--" + boundary + "--\r\n";

// Send and get transcription
```

**Option 2: Two-Step Process**
1. Call Whisper API to transcribe audio
2. Include transcription text in vision API call
3. Combine both analyses in response

### Memory Considerations

The ESP32-S3 has limited RAM. For large files:

```cpp
// Process in chunks
#define CHUNK_SIZE 8192

String readFileInChunks(String filepath) {
  File file = SD_MMC.open(filepath, FILE_READ);
  String base64 = "";
  
  while (file.available()) {
    size_t toRead = min(CHUNK_SIZE, file.available());
    uint8_t* buffer = (uint8_t*)malloc(toRead);
    file.read(buffer, toRead);
    
    String chunk = base64Encode(buffer, toRead);
    base64 += chunk;
    
    free(buffer);
  }
  
  file.close();
  return base64;
}
```

## Hardware Notes

### Audio Recording Behavior
- Recording uses blocking operation (ESP_I2S library)
- Device processes audio AFTER you click stop
- Cannot stream or preview during capture
- Audio amplified 10x due to low microphone gain

### Camera & Audio Conflict
- Both use I2S_NUM_0 internally
- Cannot capture video and audio simultaneously
- Sequential operation only

### Speaker Playback
- Uses MAX98357A on separate I2S port
- Blocking operation (processes entire file)
- SD pin (GPIO 41) enables/disables speaker

## API Endpoints

### Camera
- `GET /stream` - MJPEG video stream
- `GET /capture` - Capture photo
- `GET /image/latest` - Get most recent image info
- `GET /image?file=<name>` - View specific image
- `GET /list/images` - List all images
- `DELETE /delete/image?file=<name>` - Delete image

### Audio
- `GET /audio/record?duration=<sec>` - Record audio
- `GET /audio?file=<name>` - Stream audio file
- `GET /audio/speaker?file=<name>` - Play through speaker
- `GET /list/audio` - List all audio files
- `DELETE /delete/audio?file=<name>` - Delete audio

### OpenAI Integration
- `POST /openai/analyze` - Send to OpenAI
  - Body: `{"image": "IMG_1.jpg", "audio": "REC_1.wav"}`
- `POST /analysis/save` - Save analysis result
  - Body: `{"text": "...", "image": "...", "audio": "..."}`

## Configuration

### WiFi Settings
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### OpenAI API Key
```cpp
const char* openai_api_key = "sk-...";
```

### Audio Settings
```cpp
#define SAMPLE_RATE 16000
#define AMPLIFICATION 10  // Adjust based on mic sensitivity
```

### Camera Settings
```cpp
config.frame_size = FRAMESIZE_SVGA;  // 800x600
config.jpeg_quality = 12;             // Lower = better quality
```

## File Structure on SD Card

```
/sdcard/
  ├── /images/
  │     ├── IMG_1.jpg
  │     ├── IMG_2.jpg
  │     └── ...
  ├── /audio/
  │     ├── REC_1.wav
  │     ├── REC_2.wav
  │     └── ...
  └── /analysis/
        ├── result_12345.txt
        └── ...
```

## Troubleshooting

### "No image selected" error
- Click an image filename in the Image Files list first
- The selected item will highlight
- Then click Save to move to export queue

### "Processing..." never completes
- Check serial monitor for errors
- Verify SD card is working
- Check WiFi connection
- Ensure audio duration is reasonable (1-60 seconds)

### OpenAI request fails
- Verify API key is correct
- Check internet connectivity
- Ensure HTTPS client is configured properly
- Monitor API rate limits and quotas

### Audio too quiet/loud
- Adjust `AMPLIFICATION` constant
- Values: 5 (quieter) to 20 (louder)
- Recompile and upload

## Next Steps

1. **Complete OpenAI Integration**
   - Implement full HTTPS request handling
   - Add proper error handling
   - Implement audio transcription
   - Add response parsing

2. **Enhanced Features**
   - Add TTS for audio responses
   - Implement conversation history
   - Add batch processing
   - Enable image/audio preview before sending

3. **Security Improvements**
   - Store API key securely
   - Use proper SSL certificates
   - Implement authentication
   - Add rate limiting

4. **UI Enhancements**
   - Add progress indicators
   - Improve error messages
   - Add confirmation dialogs
   - Implement drag-and-drop selection

## License & Credits

Based on DFRobot ESP32-S3 AI CAM (SKU: DFR1154)
Developed by Robert using ESP32-S3, ESP_I2S library, and OpenAI API
