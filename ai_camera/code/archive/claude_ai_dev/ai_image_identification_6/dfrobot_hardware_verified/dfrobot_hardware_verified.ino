#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <OpenAI.h>
#include "camera.h"
#include "wav_header.h"
#include <Arduino.h>
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "ESP_I2S.h"
#include "Audio.h"  // ESP32-audioI2S library

#define BUTTON_PIN 0
#define LED_PIN 3

// ========== MICROPHONE PINS (PDM) ==========
#define SAMPLE_RATE     (16000)
#define MIC_PDM_CLK     (GPIO_NUM_38)  // PDM Clock
#define MIC_PDM_DATA    (GPIO_NUM_39)  // PDM Data

// ========== MAX98357 AUDIO AMPLIFIER PINS ==========
#define MAX98357_BCLK   (GPIO_NUM_45)  // Bit Clock
#define MAX98357_LRCLK  (GPIO_NUM_46)  // Left/Right Clock (Word Select)
#define MAX98357_DIN    (GPIO_NUM_42)  // Data In
#define MAX98357_GAIN   (GPIO_NUM_41)  // Gain Control (15dB when HIGH, 9dB when LOW)
#define MAX98357_SD     (GPIO_NUM_40)  // Shutdown/Mode Pin (HIGH=enabled, LOW=shutdown)

// ========== SD CARD PINS (SPI MODE) ==========
// From DFRobot hardware: SD section
#define SD_CS_PIN       (GPIO_NUM_10)  // CS (Chip Select)
#define SD_MOSI_PIN     (GPIO_NUM_11)  // M0 (Master Out Slave In)
#define SD_MISO_PIN     (GPIO_NUM_13)  // M1 (Master In Slave Out)
#define SD_SCK_PIN      (GPIO_NUM_12)  // SCLK (Serial Clock)

// ========== ALS (Ambient Light Sensor) PINS ==========
// Available if needed in future
#define ALS_SDA_PIN     (GPIO_NUM_8)   // I2C Data
#define ALS_SCL_PIN     (GPIO_NUM_9)   // I2C Clock
#define ALS_INT_PIN     (GPIO_NUM_48)  // Interrupt

// ========== OTHER AVAILABLE PINS ==========
#define IR_PIN          (GPIO_NUM_47)  // Infrared
#define UART_TX_PIN     (GPIO_NUM_43)  // UART TX
#define UART_RX_PIN     (GPIO_NUM_44)  // UART RX

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* api_key = "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";

OpenAI openai(api_key);
OpenAI_ChatCompletion chat(openai);
OpenAI_AudioTranscription audio(openai);
I2SClass I2S_MIC;  // Microphone I2S
Audio audioPlayer;  // Speaker audio player
WebServer server(80);

// State management
bool isRecording = false; 
bool isProcessing = false;
bool streamingEnabled = true;
bool sdCardAvailable = false;
bool audioPlayerReady = false;
uint8_t *wavBuffer = NULL;
size_t wavBufferSize = 0; 
String lastAudioFile = "";

// Button debouncing
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 500;

// Camera frame management
unsigned long lastFrameRequest = 0;
const unsigned long minFrameInterval = 150;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔═══════════════════════════════════════════════════╗");
  Serial.println("║   DFRobot ESP32-S3 AI Camera + MAX98357          ║");
  Serial.println("║   Hardware-Verified Pin Configuration            ║");
  Serial.println("║   Voice-to-Vision with SD Audio Buffering        ║");
  Serial.println("╚═══════════════════════════════════════════════════╝\n");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Configure MAX98357 control pins
  initMAX98357();
  
  // Initialize SD Card (SPI mode)
  Serial.println("\n[1/6] Initializing SD Card (SPI mode)...");
  initSDCard_SPI();
  
  // Initialize camera
  Serial.println("\n[2/6] Initializing camera...");
  initCamera();
  Serial.println("✓ Camera initialized!");
  
  // Initialize audio playback (MAX98357)
  Serial.println("\n[3/6] Initializing MAX98357 audio amplifier...");
  initAudioPlayback();
  
  // Initialize microphone
  Serial.println("\n[4/6] Initializing PDM microphone...");
  audioInit();
  Serial.println("✓ Microphone initialized!");
  
  // Test microphone
  Serial.println("\n[5/6] Testing microphone...");
  testI2S();
  
  // Connect to WiFi
  Serial.println("\n[6/6] Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { 
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n✗ WiFi connection failed!");
  }
  
  // Initialize web server
  setupWebServer();
  server.begin();
  Serial.printf("✓ Web server: http://%s/\n", WiFi.localIP().toString().c_str());
  
  // Initialize OpenAI chat
  chatInit();
  Serial.println("✓ OpenAI initialized!");
  
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n╔═══════════════════════════════════════════════════╗");
  Serial.println("║              SYSTEM READY                         ║");
  Serial.println("╚═══════════════════════════════════════════════════╝");
  Serial.printf("Camera:       http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.printf("SD Buffering: %s\n", sdCardAvailable ? "ENABLED ✓" : "DISABLED ✗");
  Serial.printf("MAX98357:     %s\n", audioPlayerReady ? "READY ✓" : "NOT READY ✗");
  Serial.println("\n>>> Press and HOLD BOOT button to record");
  Serial.println(">>> Release button to process\n");
}

void loop() {
  server.handleClient();
  
  // Audio player loop
  if (audioPlayerReady) {
    audioPlayer.loop();
  }
  
  int buttonState = digitalRead(BUTTON_PIN);
  static int lastButtonState = HIGH;
  static unsigned long buttonPressTime = 0;
  
  if (buttonState != lastButtonState) {
    if (buttonState == LOW) {
      Serial.println("\n[BUTTON] Pressed!");
      buttonPressTime = millis();
      
      if (!isRecording && !isProcessing) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress > debounceDelay) {
          lastButtonPress = currentTime;
          streamingEnabled = false;
          Serial.println("[CAMERA] Streaming paused");
          startRecording();
        }
      }
    } else {
      if (buttonPressTime > 0) {
        unsigned long pressDuration = millis() - buttonPressTime;
        Serial.printf("\n[BUTTON] Released after %lu ms\n", pressDuration);
        buttonPressTime = 0;
      }
      
      if (isRecording) {
        stopRecording();
      }
    }
    lastButtonState = buttonState;
  }
  
  if (isRecording) {
    static unsigned long lastLogTime = 0;
    unsigned long currentTime = millis();
    
    size_t bytesAvailable = I2S_MIC.available();
    if (bytesAvailable > 0) {
      uint8_t *newBuffer = (uint8_t *)realloc(wavBuffer, wavBufferSize + bytesAvailable);
      if (newBuffer == NULL) {
        Serial.printf("[ERROR] Realloc failed!\n");
        stopRecording();
        return;
      }
      wavBuffer = newBuffer;
      
      size_t bytesRead = I2S_MIC.readBytes((char *)(wavBuffer + wavBufferSize), bytesAvailable);
      wavBufferSize += bytesRead;
      
      if (currentTime - lastLogTime > 1000) {
        Serial.printf("[RECORDING] %u bytes (%.1f sec)\n", 
                      wavBufferSize - PCM_WAV_HEADER_SIZE,
                      (float)(wavBufferSize - PCM_WAV_HEADER_SIZE) / (SAMPLE_RATE * 2));
        lastLogTime = currentTime;
      }
    }
  }
  
  delay(1);
}

void initMAX98357() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   MAX98357 CONTROL PIN CONFIGURATION   ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  // Configure GAIN pin (GPIO 41)
  pinMode(MAX98357_GAIN, OUTPUT);
  digitalWrite(MAX98357_GAIN, HIGH);  // HIGH = 15dB gain, LOW = 9dB gain
  Serial.printf("  ✓ GAIN Pin (GPIO%d): HIGH → 15dB gain\n", MAX98357_GAIN);
  
  // Configure SD/Mode pin (GPIO 40)
  pinMode(MAX98357_SD, OUTPUT);
  digitalWrite(MAX98357_SD, HIGH);  // HIGH = Normal operation, LOW = Shutdown
  Serial.printf("  ✓ SD/MODE Pin (GPIO%d): HIGH → Amplifier ENABLED\n", MAX98357_SD);
  
  delay(100);  // Let amplifier stabilize
  Serial.println("  ✓ MAX98357 control pins configured\n");
}

void initSDCard_SPI() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   SD CARD INITIALIZATION (SPI MODE)    ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("  CS Pin:   GPIO%d\n", SD_CS_PIN);
  Serial.printf("  MOSI Pin: GPIO%d\n", SD_MOSI_PIN);
  Serial.printf("  MISO Pin: GPIO%d\n", SD_MISO_PIN);
  Serial.printf("  SCK Pin:  GPIO%d\n", SD_SCK_PIN);
  
  // Initialize SPI for SD card
  SPIClass spi(HSPI);
  spi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  
  if (!SD.begin(SD_CS_PIN, spi, 40000000)) {  // 40MHz SPI clock
    Serial.println("\n✗ SD Card initialization failed");
    Serial.println("  Possible causes:");
    Serial.println("  - No SD card inserted");
    Serial.println("  - Card not formatted as FAT32");
    Serial.println("  - Poor contact with card slot");
    Serial.println("\n⚠️  Audio buffering DISABLED");
    Serial.println("  System will continue without SD card features\n");
    sdCardAvailable = false;
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("\n✗ No SD card detected in slot");
    sdCardAvailable = false;
    return;
  }
  
  Serial.println("\n✓ SD Card successfully mounted!");
  Serial.println("──────────────────────────────────────");
  Serial.printf("  Type:     %s\n", 
    cardType == CARD_MMC ? "MMC" :
    cardType == CARD_SD ? "SD" :
    cardType == CARD_SDHC ? "SDHC" : "Unknown");
  Serial.printf("  Size:     %.2f GB\n", SD.cardSize() / (1024.0 * 1024.0 * 1024.0));
  Serial.printf("  Used:     %.2f MB\n", SD.usedBytes() / (1024.0 * 1024.0));
  Serial.printf("  Free:     %.2f GB\n", (SD.cardSize() - SD.usedBytes()) / (1024.0 * 1024.0 * 1024.0));
  
  sdCardAvailable = true;
  
  // Create audio directory
  if (!SD.exists("/audio")) {
    if (SD.mkdir("/audio")) {
      Serial.println("  ✓ Created /audio directory");
    } else {
      Serial.println("  ✗ Failed to create /audio directory");
    }
  } else {
    Serial.println("  ✓ /audio directory exists");
  }
  
  // List existing audio files
  File root = SD.open("/audio");
  if (root) {
    int fileCount = 0;
    size_t totalSize = 0;
    
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        fileCount++;
        totalSize += file.size();
      }
      file = root.openNextFile();
    }
    root.close();
    
    Serial.printf("  Cached files: %d (%.2f MB)\n", fileCount, totalSize / (1024.0 * 1024.0));
  }
  Serial.println("──────────────────────────────────────\n");
}

void initAudioPlayback() {
  if (!sdCardAvailable) {
    Serial.println("✗ Audio playback disabled (no SD card)");
    audioPlayerReady = false;
    return;
  }
  
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   MAX98357 I2S AUDIO INITIALIZATION    ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("  BCLK  (Bit Clock):    GPIO%d\n", MAX98357_BCLK);
  Serial.printf("  LRCLK (Word Select):  GPIO%d\n", MAX98357_LRCLK);
  Serial.printf("  DIN   (Data In):      GPIO%d\n", MAX98357_DIN);
  
  // Initialize audio player with MAX98357 pins
  audioPlayer.setPinout(MAX98357_BCLK, MAX98357_LRCLK, MAX98357_DIN);
  
  // Set volume (0-21)
  // With 15dB hardware gain, start at moderate volume
  audioPlayer.setVolume(15);  // Adjust 8-21 as needed
  
  audioPlayerReady = true;
  Serial.println("\n✓ MAX98357 I2S interface ready!");
  Serial.println("──────────────────────────────────────");
  Serial.println("  Hardware Gain: 15dB");
  Serial.println("  Software Volume: 15/21 (71%)");
  Serial.println("  Output Power: ~3W @ 4Ω speaker");
  Serial.println("──────────────────────────────────────\n");
}

void testI2S() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   PDM MICROPHONE TEST (2 seconds)      ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  unsigned long startTime = millis();
  size_t totalBytes = 0;
  size_t maxChunk = 0;
  int chunkCount = 0;
  
  while (millis() - startTime < 2000) {
    size_t available = I2S_MIC.available();
    if (available > 0) {
      uint8_t tempBuffer[512];
      size_t toRead = min(available, sizeof(tempBuffer));
      size_t bytesRead = I2S_MIC.readBytes((char*)tempBuffer, toRead);
      totalBytes += bytesRead;
      chunkCount++;
      if (bytesRead > maxChunk) maxChunk = bytesRead;
    }
    delay(10);
  }
  
  Serial.printf("\nTest Results:\n");
  Serial.printf("  Total Bytes:  %u\n", totalBytes);
  Serial.printf("  Data Rate:    %.2f KB/s\n", totalBytes / 2048.0);
  Serial.printf("  Max Chunk:    %u bytes\n", maxChunk);
  Serial.printf("  Chunk Count:  %d\n", chunkCount);
  
  if (totalBytes > 1000) {
    Serial.println("\n✓ PDM Microphone WORKING!");
  } else {
    Serial.println("\n✗ WARNING: Microphone NOT capturing data!");
    Serial.printf("  Check connections on GPIO%d (CLK) and GPIO%d (DATA)\n", MIC_PDM_CLK, MIC_PDM_DATA);
  }
  Serial.println();
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String sdStatus = sdCardAvailable ? "🟢 ENABLED" : "🔴 DISABLED";
    String ampStatus = audioPlayerReady ? "🟢 READY" : "🔴 NOT READY";
    
    String html = "<!DOCTYPE html><html><head><title>DFRobot AI Camera</title>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{font-family:'Segoe UI',Arial;text-align:center;margin:0;background:#0a0a0a;color:#fff}"
                  "img{max-width:100%;height:auto;border:3px solid #4CAF50;display:block;margin:15px auto;border-radius:8px;box-shadow:0 4px 12px rgba(76,175,80,0.3)}"
                  ".container{padding:20px;max-width:800px;margin:0 auto}"
                  ".status{background:#1a1a1a;padding:18px;margin:15px;border-radius:8px;font-size:20px;font-weight:bold;border:2px solid #333}"
                  ".info{background:#0d0d0d;padding:12px;margin:10px;border-radius:6px;font-size:14px;color:#aaa;border:1px solid #222}"
                  "h1{color:#4CAF50;margin:15px;font-size:28px;text-shadow:0 2px 4px rgba(0,0,0,0.5)}"
                  ".recording{background:#ff0000;animation:blink 1s infinite;border-color:#ff0000}"
                  ".processing{background:#ff9800;border-color:#ff9800}"
                  ".btn{background:linear-gradient(135deg,#4CAF50,#45a049);color:white;padding:14px 28px;border:none;border-radius:6px;"
                  "cursor:pointer;margin:8px;font-size:16px;font-weight:bold;box-shadow:0 4px 8px rgba(0,0,0,0.3);transition:all 0.3s}"
                  ".btn:hover{background:linear-gradient(135deg,#45a049,#3d8b40);transform:translateY(-2px);box-shadow:0 6px 12px rgba(0,0,0,0.4)}"
                  ".btn:active{transform:translateY(0)}"
                  ".btn:disabled{background:#444;cursor:not-allowed;box-shadow:none}"
                  ".hardware-info{font-size:11px;color:#666;margin-top:20px}"
                  "@keyframes blink{0%{opacity:1}50%{opacity:0.5}100%{opacity:1}}"
                  "</style>"
                  "<script>"
                  "let img = null;"
                  "function updateStatus(){"
                  "  fetch('/status').then(r=>r.json()).then(d=>{"
                  "    let cls='status';"
                  "    if(d.recording) cls+=' recording';"
                  "    else if(d.processing) cls+=' processing';"
                  "    document.getElementById('status').className=cls;"
                  "    document.getElementById('status').innerHTML=d.status;"
                  "    document.getElementById('replay').disabled=!d.hasAudio;"
                  "    if(!img && !d.recording && !d.processing){"
                  "      img = document.getElementById('camera');"
                  "      img.src='/stream?'+Date.now();"
                  "    }"
                  "  });"
                  "}"
                  "function replayAudio(){"
                  "  document.getElementById('replay').disabled=true;"
                  "  document.getElementById('status').innerHTML='🔊 Replaying audio...';"
                  "  fetch('/replay').then(r=>r.text()).then(msg=>{"
                  "    setTimeout(()=>{"
                  "      document.getElementById('replay').disabled=false;"
                  "      updateStatus();"
                  "    }, 2000);"
                  "  });"
                  "}"
                  "setInterval(updateStatus, 500);"
                  "updateStatus();"
                  "</script>"
                  "</head><body>"
                  "<div class='container'>"
                  "<h1>🎥 DFRobot AI Camera + MAX98357</h1>"
                  "<div id='status' class='status'>Initializing...</div>"
                  "<div class='info'>SD Card: " + sdStatus + " | MAX98357: " + ampStatus + "</div>"
                  "<button id='replay' class='btn' onclick='replayAudio()' disabled>🔊 Replay Last Response</button>"
                  "<img id='camera' alt='Camera feed loading...' />"
                  "<p style='color:#888;font-size:15px;margin-top:20px'>Press BOOT button on device to record voice command</p>"
                  "<div class='hardware-info'>Hardware-Verified Pin Configuration | SPI SD Card</div>"
                  "</div></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/status", HTTP_GET, []() {
    String status = "Ready - Streaming";
    if (isRecording) status = "🔴 RECORDING";
    else if (isProcessing) status = "⚙️ Processing AI...";
    
    String json = "{\"recording\":" + String(isRecording) + 
                  ",\"processing\":" + String(isProcessing) +
                  ",\"hasAudio\":" + String(lastAudioFile.length() > 0 ? "true" : "false") +
                  ",\"status\":\"" + status + "\"}";
    server.send(200, "application/json", json);
  });
  
  server.on("/replay", HTTP_GET, []() {
    if (lastAudioFile.length() > 0 && audioPlayerReady) {
      Serial.println("\n[REPLAY] Playing cached audio...");
      if (playAudioFromSD(lastAudioFile)) {
        server.send(200, "text/plain", "✓ Replayed!");
      } else {
        server.send(500, "text/plain", "✗ Playback failed");
      }
    } else if (!audioPlayerReady) {
      server.send(503, "text/plain", "✗ MAX98357 not ready");
    } else {
      server.send(404, "text/plain", "✗ No audio cached yet");
    }
  });
  
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
}

void handleStream() {
  if (!streamingEnabled) {
    server.send(503, "text/plain", "Streaming paused");
    return;
  }
  
  WiFiClient client = server.client();
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();
  
  while (client.connected() && streamingEnabled && !isRecording && !isProcessing) {
    unsigned long now = millis();
    if (now - lastFrameRequest < minFrameInterval) {
      delay(10);
      continue;
    }
    lastFrameRequest = now;
    
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      delay(200);
      continue;
    }
    
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    
    esp_camera_fb_return(fb);
    
    if (!client.connected()) break;
  }
}

void handleCapture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Capture failed");
    return;
  }
  
  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
}

void audioInit() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   PDM MICROPHONE CONFIGURATION         ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("  Clock Pin: GPIO%d\n", MIC_PDM_CLK);
  Serial.printf("  Data Pin:  GPIO%d\n", MIC_PDM_DATA);
  Serial.printf("  Rate:      %d Hz\n", SAMPLE_RATE);
  Serial.printf("  Mode:      PDM (Pulse Density Modulation)\n");
  Serial.printf("  Bit Width: 16-bit\n");
  Serial.printf("  Channels:  Mono\n");
  
  I2S_MIC.setPinsPdmRx(MIC_PDM_CLK, MIC_PDM_DATA);
  
  if (!I2S_MIC.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("\n✗ PDM Microphone initialization FAILED");
  } else {
    Serial.println("\n✓ PDM Microphone initialized successfully\n");
  }
}

void chatInit() {
  chat.setModel("gpt-4o-mini");
  chat.setSystem("You are a helpful assistant that analyzes images and provides concise responses in 1-2 sentences.");
  chat.setMaxTokens(500);
  chat.setTemperature(0.2);
  chat.setStop("\r");
  chat.setPresencePenalty(0);
  chat.setFrequencyPenalty(0);
  chat.setUser("DFRobot-ESP32-S3-MAX98357");
  
  Serial.println("OpenAI Chat initialized");
}

String SpeechToText() {
  Serial.println("\n[STT] Converting speech to text...");
  Serial.printf("[STT] Audio buffer: %u bytes (%.1f sec)\n", 
                wavBufferSize, 
                (float)wavBufferSize / (SAMPLE_RATE * 2));
  
  delay(500);
  
  String txt = audio.file(wavBuffer, wavBufferSize, (OpenAI_Audio_Input_Format)5);
  
  free(wavBuffer);
  wavBuffer = NULL;
  wavBufferSize = 0;
  
  return txt;
}

String imageAnswering(String txt) {
  camera_fb_t *fb = NULL; 
  String response;
  
  Serial.println("\n[VISION] Capturing image for analysis...");
  delay(100);
  
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("[VISION] ✗ Capture failed!");
    return "";
  }
  
  Serial.printf("[VISION] ✓ Captured %d bytes\n", fb->len);
  Serial.println("[VISION] Analyzing with GPT-4 Vision...");
  
  OpenAI_StringResponse result = chat.message(txt, fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  
  if (result.length() >= 1) {
    for (unsigned int i = 0; i < result.length(); ++i) {
      response = result.getAt(i);
      response.trim();
    }
  }
  
  return response;
}

bool downloadTTSToSD(String text, String filename) {
  Serial.println("\n[TTS] Requesting MP3 from OpenAI...");
  
  HTTPClient http;
  http.begin("https://api.openai.com/v1/audio/speech");
  http.addHeader("Authorization", "Bearer " + String(api_key));
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{";
  payload += "\"model\":\"tts-1\",";
  payload += "\"input\":\"" + text + "\",";
  payload += "\"voice\":\"alloy\"";
  payload += "}";
  
  Serial.printf("[TTS] Text length: %d characters\n", text.length());
  
  int httpCode = http.POST(payload);
  
  if (httpCode != 200) {
    Serial.printf("[TTS] ✗ HTTP Error: %d\n", httpCode);
    http.end();
    return false;
  }
  
  Serial.println("[TTS] ✓ Downloading MP3 to SD card...");
  
  if (SD.exists(filename.c_str())) {
    SD.remove(filename.c_str());
  }
  
  File audioFile = SD.open(filename.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("[TTS] ✗ Failed to create file on SD");
    http.end();
    return false;
  }
  
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t totalSize = 0;
  unsigned long startTime = millis();
  int dots = 0;
  
  Serial.print("[TTS] Progress: ");
  while (http.connected()) {
    size_t available = stream->available();
    if (available > 0) {
      int c = stream->readBytes(buffer, min(available, sizeof(buffer)));
      audioFile.write(buffer, c);
      totalSize += c;
      
      if (totalSize / 5120 > dots) {
        Serial.print("▓");
        dots = totalSize / 5120;
      }
    } else if (!http.connected()) {
      break;
    } else {
      delay(1);
    }
    
    if (millis() - startTime > 30000) {
      Serial.println("\n[TTS] ✗ Download timeout");
      audioFile.close();
      http.end();
      return false;
    }
  }
  
  audioFile.close();
  http.end();
  
  Serial.printf("\n[TTS] ✓ Downloaded %u bytes (%.2f KB)\n", totalSize, totalSize / 1024.0);
  Serial.printf("[TTS] Saved to: %s\n", filename.c_str());
  
  return totalSize > 0;
}

bool playAudioFromSD(String filename) {
  if (!audioPlayerReady || !sdCardAvailable) {
    Serial.println("[PLAY] ✗ System not ready");
    return false;
  }
  
  if (!SD.exists(filename.c_str())) {
    Serial.printf("[PLAY] ✗ File not found: %s\n", filename.c_str());
    return false;
  }
  
  Serial.printf("\n[PLAY] 🔊 Playing through MAX98357...\n");
  Serial.printf("[PLAY] File: %s\n", filename.c_str());
  
  // Ensure MAX98357 is enabled
  digitalWrite(MAX98357_SD, HIGH);
  delay(10);
  
  audioPlayer.connecttoFS(SD, filename.c_str());
  
  unsigned long playbackStart = millis();
  int lastSecond = 0;
  
  Serial.print("[PLAY] ");
  
  while (audioPlayer.isRunning()) {
    audioPlayer.loop();
    
    int currentSecond = (millis() - playbackStart) / 1000;
    if (currentSecond > lastSecond) {
      Serial.print("♪");
      lastSecond = currentSecond;
    }
    
    delay(1);
    
    if (millis() - playbackStart > 60000) {
      Serial.println("\n[PLAY] ⚠️  Timeout (60s exceeded)");
      break;
    }
  }
  
  Serial.printf("\n[PLAY] ✓ Playback complete (%.1f seconds)\n", 
                (millis() - playbackStart) / 1000.0);
  
  return true;
}

int TextToSpeechEnhanced(String txt) {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║   TEXT-TO-SPEECH (SD BUFFERED)        ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  if (!sdCardAvailable || !audioPlayerReady) {
    Serial.println("[TTS] ✗ System not ready (check SD card & MAX98357)");
    return -1;
  }
  
  String audioFile = "/audio/resp_" + String(millis()) + ".mp3";
  
  // Download MP3
  if (!downloadTTSToSD(txt, audioFile)) {
    return -1;
  }
  
  // Play from SD
  if (!playAudioFromSD(audioFile)) {
    return -1;
  }
  
  lastAudioFile = audioFile;
  Serial.println("\n[TTS] ✓ Audio cached - available for replay\n");
  
  return 0;
}

void startRecording() {
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║      🎤 RECORDING STARTED          ║");
  Serial.println("╚════════════════════════════════════╝");
  
  size_t sampleRate = I2S_MIC.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S_MIC.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S_MIC.rxSlotMode();
  
  Serial.printf("\nAudio Config: %uHz, %u-bit, %uch\n", sampleRate, sampleWidth, numChannels);
  
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    Serial.println("[ERROR] ✗ Memory allocation failed!");
    return;
  }
  
  const pcm_wav_header_t wavHeader = PCM_WAV_HEADER_DEFAULT(0, sampleWidth, sampleRate, numChannels);
  memcpy(wavBuffer, &wavHeader, PCM_WAV_HEADER_SIZE);
  wavBufferSize = PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n>>> SPEAK YOUR QUESTION NOW...\n");
  
  digitalWrite(LED_PIN, LOW);
  isRecording = true;
}

void stopRecording() {
  if (!isRecording) return;
  
  isRecording = false;
  isProcessing = true;
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║      ⏹️  RECORDING STOPPED         ║");
  Serial.println("╚════════════════════════════════════╝");
  
  float duration = (float)(wavBufferSize - PCM_WAV_HEADER_SIZE) / (SAMPLE_RATE * 2);
  Serial.printf("\nRecorded: %u bytes (%.1f seconds)\n", 
                wavBufferSize - PCM_WAV_HEADER_SIZE, duration);
  
  if (wavBufferSize <= PCM_WAV_HEADER_SIZE + 2000) {
    Serial.println("\n✗ Recording too short (minimum 1 second required)\n");
    free(wavBuffer);
    wavBuffer = NULL;
    wavBufferSize = 0;
    isProcessing = false;
    streamingEnabled = true;
    return;
  }
  
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = (wavBufferSize) + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║       AI PROCESSING PIPELINE       ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // Step 1
  Serial.println("━━━ STEP 1/3: SPEECH RECOGNITION ━━━");
  String txt = SpeechToText();
  
  if (txt != NULL && txt.length() > 0) {
    Serial.printf("✓ You said: \"%s\"\n", txt.c_str());
    
    // Step 2
    Serial.println("\n━━━ STEP 2/3: VISION ANALYSIS ━━━");
    String response = imageAnswering(txt);
    
    if (response != NULL && response.length() > 0) {
      Serial.println("\n✓ AI Response:");
      Serial.println("┌───────────────────────────────────┐");
      Serial.printf("│ %s\n", response.c_str());
      Serial.println("└───────────────────────────────────┘");
      
      // Step 3
      Serial.println("\n━━━ STEP 3/3: AUDIO PLAYBACK ━━━");
      if (TextToSpeechEnhanced(response) != -1) {
        Serial.println("✓ Audio playback successful");
      } else {
        Serial.println("✗ Audio playback failed");
      }
    } else {
      Serial.println("✗ Vision analysis failed");
    }
  } else {
    Serial.println("✗ Speech recognition failed");
  }
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║      ✓ PIPELINE COMPLETE          ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  isProcessing = false;
  streamingEnabled = true;
}

// Audio library callbacks
void audio_info(const char *info) {
  // Optional: Print audio info for debugging
  // Serial.printf("[AUDIO] %s\n", info);
}

void audio_eof_mp3(const char *info) {
  // End of MP3 file - playback complete
}
