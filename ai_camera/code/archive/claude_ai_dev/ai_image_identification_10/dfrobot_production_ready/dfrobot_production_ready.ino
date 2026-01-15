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
#include "ESP_I2S.h"
#include "Audio.h"

/*
 * DFRobot ESP32-S3 AI Camera - Production Ready
 * 
 * Hardware Pin Configuration (from DFRobot documentation):
 * 
 * MAX98357 Audio Amplifier:
 *   BCLK  → GPIO 45
 *   LRCLK → GPIO 46
 *   DIN   → GPIO 42
 *   GAIN  → GPIO 41 (HIGH=15dB, LOW=9dB)
 *   MODE  → GPIO 40 (HIGH=ON, LOW=OFF)
 * 
 * PDM Microphone:
 *   CLK   → GPIO 38
 *   DATA  → GPIO 39
 * 
 * SD Card (SPI):
 *   CS    → GPIO 10
 *   MOSI  → GPIO 11 (M0)
 *   MISO  → GPIO 13 (M1)
 *   SCLK  → GPIO 12
 * 
 * Other:
 *   LED   → GPIO 3
 *   BOOT  → GPIO 0
 */

// Disable brownout detector (handled in camera.h too)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BUTTON_PIN 0
#define LED_PIN 3

// ========== MICROPHONE (PDM) ==========
#define SAMPLE_RATE     (16000)
#define MIC_PDM_CLK     (GPIO_NUM_38)
#define MIC_PDM_DATA    (GPIO_NUM_39)

// ========== MAX98357 AUDIO AMPLIFIER ==========
#define MAX98357_BCLK   (GPIO_NUM_45)  // Bit Clock
#define MAX98357_LRCLK  (GPIO_NUM_46)  // Left/Right Clock
#define MAX98357_DIN    (GPIO_NUM_42)  // Data In
#define MAX98357_GAIN   (GPIO_NUM_41)  // Gain: HIGH=15dB, LOW=9dB
#define MAX98357_MODE   (GPIO_NUM_40)  // Mode: HIGH=ON, LOW=OFF

// ========== SD CARD (SPI) ==========
#define SD_CS_PIN       (GPIO_NUM_10)  // Chip Select
#define SD_MOSI_PIN     (GPIO_NUM_11)  // Master Out Slave In (M0)
#define SD_MISO_PIN     (GPIO_NUM_13)  // Master In Slave Out (M1)
#define SD_SCK_PIN      (GPIO_NUM_12)  // Serial Clock

// WiFi credentials
const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* api_key = "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";

// Objects
OpenAI openai(api_key);
OpenAI_ChatCompletion chat(openai);
OpenAI_AudioTranscription audio(openai);
I2SClass I2S_MIC;
Audio audioPlayer;
WebServer server(80);

// State variables
bool isRecording = false; 
bool isProcessing = false;
bool streamingEnabled = true;
bool sdCardAvailable = false;
bool audioPlayerReady = false;
uint8_t *wavBuffer = NULL;
size_t wavBufferSize = 0; 
String lastAudioFile = "";

// Timing
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 500;
unsigned long lastFrameRequest = 0;
const unsigned long minFrameInterval = 150;
unsigned long lastHeapCheck = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n╔═══════════════════════════════════════════════════╗");
  Serial.println("║   DFRobot ESP32-S3 AI Camera                     ║");
  Serial.println("║   Production Ready - Hardware Verified           ║");
  Serial.println("╚═══════════════════════════════════════════════════╝\n");
  
  // Print memory info
  Serial.println("Memory Status:");
  Serial.printf("  Free Heap: %d bytes (%.2f KB)\n", ESP.getFreeHeap(), ESP.getFreeHeap()/1024.0);
  Serial.printf("  PSRAM: %d bytes (%.2f MB)\n\n", ESP.getFreePsram(), ESP.getFreePsram()/(1024.0*1024.0));
  
  // Pin setup
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, HIGH);
  
  // Initialize MAX98357
  Serial.println("[1/6] Initializing MAX98357 Audio Amplifier...");
  initMAX98357();
  
  // Initialize SD Card
  Serial.println("\n[2/6] Initializing SD Card (SPI Mode)...");
  initSDCard();
  
  // Initialize camera
  Serial.println("\n[3/6] Initializing Camera...");
  initCamera();
  Serial.println("✓ Camera initialized successfully\n");
  
  // Initialize audio playback
  Serial.println("[4/6] Initializing Audio Playback...");
  initAudioPlayback();
  
  // Initialize microphone
  Serial.println("\n[5/6] Initializing PDM Microphone...");
  initMicrophone();
  
  // Test microphone
  Serial.println("\n[6/6] Testing Microphone (2 seconds)...");
  testMicrophone();
  
  // Connect to WiFi
  Serial.println("\n[7/6] Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { 
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected successfully!");
    Serial.printf("   IP Address: http://%s/\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n⚠️  WiFi connection failed - system will work without web interface");
  }
  
  // Initialize web server
  setupWebServer();
  server.begin();
  
  // Initialize OpenAI chat
  initOpenAI();
  
  Serial.println("\n╔═══════════════════════════════════════════════════╗");
  Serial.println("║              SYSTEM READY                         ║");
  Serial.println("╚═══════════════════════════════════════════════════╝");
  Serial.printf("Camera Stream:  http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.printf("SD Card:        %s\n", sdCardAvailable ? "✓ ENABLED" : "✗ DISABLED");
  Serial.printf("MAX98357:       %s\n", audioPlayerReady ? "✓ READY" : "✗ NOT READY");
  Serial.printf("Free Heap:      %d bytes\n", ESP.getFreeHeap());
  Serial.println("\n>>> Press and HOLD BOOT button to record");
  Serial.println(">>> Speak your question");
  Serial.println(">>> Release button to process\n");
}

void loop() {
  // Only handle web requests when not processing and WiFi is connected
  if (!isProcessing && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
  
  // Audio player loop (must run continuously for playback)
  if (audioPlayerReady && !isRecording) {
    audioPlayer.loop();
  }
  
  // Memory monitoring every 10 seconds
  if (millis() - lastHeapCheck > 10000) {
    lastHeapCheck = millis();
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 100000) {
      Serial.printf("⚠️  LOW MEMORY WARNING: %d bytes remaining\n", freeHeap);
    }
  }
  
  // Handle button input
  handleButton();
  
  // Handle recording
  if (isRecording) {
    handleRecording();
  }
  
  yield();  // Feed watchdog
  delay(1);
}

void initMAX98357() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   MAX98357 AUDIO AMPLIFIER SETUP      ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("  BCLK (Bit Clock):    GPIO %d\n", MAX98357_BCLK);
  Serial.printf("  LRCLK (Word Select): GPIO %d\n", MAX98357_LRCLK);
  Serial.printf("  DIN (Data In):       GPIO %d\n", MAX98357_DIN);
  Serial.printf("  GAIN (Control):      GPIO %d\n", MAX98357_GAIN);
  Serial.printf("  MODE (Enable):       GPIO %d\n", MAX98357_MODE);
  
  // Configure GAIN pin - HIGH = 15dB, LOW = 9dB
  pinMode(MAX98357_GAIN, OUTPUT);
  digitalWrite(MAX98357_GAIN, HIGH);
  Serial.println("\n  ✓ GAIN set to HIGH → 15dB amplification");
  
  // Configure MODE pin - HIGH = enabled, LOW = shutdown
  pinMode(MAX98357_MODE, OUTPUT);
  digitalWrite(MAX98357_MODE, HIGH);
  Serial.println("  ✓ MODE set to HIGH → Amplifier ENABLED");
  
  delay(100);  // Let amplifier stabilize
  Serial.println("\n✓ MAX98357 configured successfully\n");
}

void initSDCard() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   SD CARD INITIALIZATION (SPI)         ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("  CS Pin (Chip Select): GPIO %d\n", SD_CS_PIN);
  Serial.printf("  MOSI Pin (M0):        GPIO %d\n", SD_MOSI_PIN);
  Serial.printf("  MISO Pin (M1):        GPIO %d\n", SD_MISO_PIN);
  Serial.printf("  SCK Pin (Clock):      GPIO %d\n", SD_SCK_PIN);
  
  // Initialize SPI bus
  SPIClass spi(HSPI);
  spi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  
  // Initialize SD card
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
  
  // Check card type
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("\n✗ No SD card detected in slot\n");
    sdCardAvailable = false;
    return;
  }
  
  // Card successfully mounted
  Serial.println("\n✓ SD Card successfully mounted!");
  Serial.println("──────────────────────────────────────");
  Serial.printf("  Card Type:    %s\n", 
    cardType == CARD_MMC ? "MMC" :
    cardType == CARD_SD ? "SD" :
    cardType == CARD_SDHC ? "SDHC" : "Unknown");
  Serial.printf("  Total Size:   %.2f GB\n", SD.cardSize() / (1024.0 * 1024.0 * 1024.0));
  Serial.printf("  Used Space:   %.2f MB\n", SD.usedBytes() / (1024.0 * 1024.0));
  Serial.printf("  Free Space:   %.2f GB\n", (SD.cardSize() - SD.usedBytes()) / (1024.0 * 1024.0 * 1024.0));
  
  sdCardAvailable = true;
  
  // Create audio directory if it doesn't exist
  if (!SD.exists("/audio")) {
    if (SD.mkdir("/audio")) {
      Serial.println("  ✓ Created /audio directory");
    } else {
      Serial.println("  ⚠️  Failed to create /audio directory");
    }
  } else {
    Serial.println("  ✓ /audio directory exists");
  }
  
  // Count existing audio files
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
    
    Serial.printf("  Cached Files: %d (%.2f MB)\n", fileCount, totalSize / (1024.0 * 1024.0));
  }
  Serial.println("──────────────────────────────────────\n");
}

void initAudioPlayback() {
  if (!sdCardAvailable) {
    Serial.println("✗ Audio playback disabled - SD card required\n");
    audioPlayerReady = false;
    return;
  }
  
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   AUDIO PLAYBACK INITIALIZATION        ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("  I2S BCLK:  GPIO %d\n", MAX98357_BCLK);
  Serial.printf("  I2S LRCLK: GPIO %d\n", MAX98357_LRCLK);
  Serial.printf("  I2S DIN:   GPIO %d\n", MAX98357_DIN);
  
  // Configure audio player with MAX98357 pins
  audioPlayer.setPinout(MAX98357_BCLK, MAX98357_LRCLK, MAX98357_DIN);
  
  // Set volume (0-21, with 15dB hardware gain)
  audioPlayer.setVolume(15);  // 71% volume
  
  audioPlayerReady = true;
  Serial.println("\n✓ Audio player ready!");
  Serial.println("──────────────────────────────────────");
  Serial.println("  Hardware Gain:   15dB");
  Serial.println("  Software Volume: 15/21 (71%)");
  Serial.println("  Max Power:       ~3W @ 4Ω");
  Serial.println("──────────────────────────────────────\n");
}

void initMicrophone() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   PDM MICROPHONE CONFIGURATION         ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("  Clock Pin:    GPIO %d\n", MIC_PDM_CLK);
  Serial.printf("  Data Pin:     GPIO %d\n", MIC_PDM_DATA);
  Serial.printf("  Sample Rate:  %d Hz\n", SAMPLE_RATE);
  Serial.printf("  Mode:         PDM (Pulse Density Modulation)\n");
  Serial.printf("  Bit Width:    16-bit\n");
  Serial.printf("  Channels:     Mono\n");
  
  // Configure PDM microphone pins
  I2S_MIC.setPinsPdmRx(MIC_PDM_CLK, MIC_PDM_DATA);
  
  // Initialize I2S in PDM RX mode
  if (!I2S_MIC.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("\n✗ PDM Microphone initialization FAILED\n");
  } else {
    Serial.println("\n✓ PDM Microphone initialized successfully\n");
  }
}

void testMicrophone() {
  Serial.println("Testing microphone data capture...");
  
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
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   MICROPHONE TEST RESULTS              ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.printf("  Total Bytes:  %u\n", totalBytes);
  Serial.printf("  Data Rate:    %.2f KB/s\n", totalBytes / 2048.0);
  Serial.printf("  Max Chunk:    %u bytes\n", maxChunk);
  Serial.printf("  Chunk Count:  %d\n", chunkCount);
  
  if (totalBytes > 1000) {
    Serial.println("\n✓ Microphone is WORKING correctly!\n");
  } else {
    Serial.println("\n✗ WARNING: Microphone not capturing data!");
    Serial.printf("  Check connections on GPIO %d (CLK) and GPIO %d (DATA)\n\n", MIC_PDM_CLK, MIC_PDM_DATA);
  }
}

void initOpenAI() {
  chat.setModel("gpt-4o-mini");
  chat.setSystem("You are a helpful assistant that analyzes images and provides concise responses in 1-2 sentences.");
  chat.setMaxTokens(300);  // Shorter responses to save memory
  chat.setTemperature(0.2);
  chat.setStop("\r");
  chat.setPresencePenalty(0);
  chat.setFrequencyPenalty(0);
  chat.setUser("DFRobot-ESP32-S3");
  
  Serial.println("✓ OpenAI chat configured\n");
}

void handleButton() {
  static int lastButtonState = HIGH;
  static unsigned long buttonPressTime = 0;
  
  int buttonState = digitalRead(BUTTON_PIN);
  
  if (buttonState != lastButtonState) {
    if (buttonState == LOW) {
      // Button pressed
      buttonPressTime = millis();
      if (!isRecording && !isProcessing) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress > debounceDelay) {
          lastButtonPress = currentTime;
          streamingEnabled = false;
          startRecording();
        }
      }
    } else {
      // Button released
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
}

void handleRecording() {
  static unsigned long lastLogTime = 0;
  unsigned long currentTime = millis();
  
  size_t bytesAvailable = I2S_MIC.available();
  if (bytesAvailable > 0) {
    // Limit chunk size to prevent memory issues
    size_t chunkSize = min(bytesAvailable, (size_t)4096);
    
    uint8_t *newBuffer = (uint8_t *)realloc(wavBuffer, wavBufferSize + chunkSize);
    if (newBuffer == NULL) {
      Serial.println("\n[ERROR] ✗ Out of memory during recording!");
      Serial.printf("  Attempted allocation: %d bytes\n", wavBufferSize + chunkSize);
      Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
      stopRecording();
      return;
    }
    wavBuffer = newBuffer;
    
    size_t bytesRead = I2S_MIC.readBytes((char *)(wavBuffer + wavBufferSize), chunkSize);
    wavBufferSize += bytesRead;
    
    // Safety limit: max 15 seconds
    if (wavBufferSize > 500000) {
      Serial.println("\n⚠️  Maximum recording length reached (15 seconds)");
      stopRecording();
      return;
    }
    
    // Log progress every second
    if (currentTime - lastLogTime > 1000) {
      Serial.printf("[REC] %u bytes (%.1fs) | Free Heap: %d bytes\n", 
                    wavBufferSize - PCM_WAV_HEADER_SIZE,
                    (float)(wavBufferSize - PCM_WAV_HEADER_SIZE) / (SAMPLE_RATE * 2),
                    ESP.getFreeHeap());
      lastLogTime = currentTime;
    }
  }
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
                  "@keyframes blink{0%{opacity:1}50%{opacity:0.5}100%{opacity:1}}"
                  "</style>"
                  "<script>"
                  "let procMode=false,failCount=0;"
                  "function upd(){"
                  "  fetch('/status',{signal:AbortSignal.timeout(2000)})"
                  "  .then(r=>r.json()).then(d=>{"
                  "    failCount=0;procMode=d.processing;"
                  "    document.getElementById('st').className='status'+(d.recording?' recording':d.processing?' processing':'');"
                  "    document.getElementById('st').innerHTML=d.status;"
                  "    document.getElementById('rep').disabled=!d.hasAudio;"
                  "    if(!procMode&&!document.getElementById('cam').src.includes('stream')){"
                  "      document.getElementById('cam').src='/stream?'+Date.now();"
                  "    }"
                  "  }).catch(e=>{"
                  "    failCount++;"
                  "    if(procMode||failCount<3){"
                  "      document.getElementById('st').innerHTML='⚙️ Processing AI...';"
                  "    }else{"
                  "      document.getElementById('st').innerHTML='⚠️ Connection lost';"
                  "    }"
                  "  });"
                  "}"
                  "function replay(){"
                  "  document.getElementById('rep').disabled=true;"
                  "  document.getElementById('st').innerHTML='🔊 Replaying...';"
                  "  fetch('/replay').then(()=>setTimeout(()=>{document.getElementById('rep').disabled=false;upd();},2000));"
                  "}"
                  "setInterval(upd,2000);"
                  "upd();"
                  "</script>"
                  "</head><body>"
                  "<div class='container'>"
                  "<h1>🎥 DFRobot AI Camera + MAX98357</h1>"
                  "<div id='st' class='status'>Initializing...</div>"
                  "<div class='info'>SD Card: " + sdStatus + " | MAX98357: " + ampStatus + "</div>"
                  "<button id='rep' class='btn' onclick='replay()' disabled>🔊 Replay Last Response</button>"
                  "<img id='cam' alt='Camera feed'/>"
                  "<p style='color:#888;margin-top:20px'>Press BOOT button on device to record voice command</p>"
                  "</div></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/status", HTTP_GET, []() {
    String status = streamingEnabled ? "Ready - Streaming" : "";
    if (isRecording) status = "🔴 RECORDING";
    else if (isProcessing) status = "⚙️ Processing AI";
    
    String json = "{\"recording\":" + String(isRecording ? "true" : "false") + 
                  ",\"processing\":" + String(isProcessing ? "true" : "false") +
                  ",\"hasAudio\":" + String(lastAudioFile.length() > 0 ? "true" : "false") +
                  ",\"status\":\"" + status + "\"}";
    server.send(200, "application/json", json);
  });
  
  server.on("/replay", HTTP_GET, []() {
    if (lastAudioFile.length() > 0 && audioPlayerReady) {
      Serial.println("\n[REPLAY] Playing cached audio from SD card...");
      playAudioFromSD(lastAudioFile);
      server.send(200, "text/plain", "Replayed");
    } else if (!audioPlayerReady) {
      server.send(503, "text/plain", "Audio not ready");
    } else {
      server.send(404, "text/plain", "No audio cached");
    }
  });
  
  server.on("/stream", HTTP_GET, handleStream);
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
    if (millis() - lastFrameRequest < minFrameInterval) {
      delay(10);
      continue;
    }
    lastFrameRequest = millis();
    
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      delay(200);
      continue;
    }
    
    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);
    
    if (!client.connected()) break;
  }
}

// Helper function to escape JSON strings
String escapeJSON(String str) {
  String escaped = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    switch (c) {
      case '"':  escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[7];
          sprintf(buf, "\\u%04x", (int)c);
          escaped += buf;
        } else {
          escaped += c;
        }
        break;
    }
  }
  return escaped;
}

String SpeechToText() {
  Serial.println("\n[STT] Converting speech to text...");
  Serial.printf("[STT] Audio buffer: %u bytes (%.1f seconds)\n", 
                wavBufferSize, 
                (float)wavBufferSize / (SAMPLE_RATE * 2));
  
  delay(500);
  yield();
  
  String txt = audio.file(wavBuffer, wavBufferSize, (OpenAI_Audio_Input_Format)5);
  
  // Free memory immediately
  free(wavBuffer);
  wavBuffer = NULL;
  wavBufferSize = 0;
  
  Serial.printf("[STT] Free heap after transcription: %d bytes\n", ESP.getFreeHeap());
  yield();
  
  return txt;
}

String imageAnswering(String txt) {
  Serial.println("\n[VISION] Capturing image for analysis...");
  delay(100);
  yield();
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[VISION] ✗ Camera capture failed!");
    return "";
  }
  
  Serial.printf("[VISION] ✓ Captured image: %d bytes\n", fb->len);
  Serial.println("[VISION] Analyzing with GPT-4 Vision API...");
  yield();
  
  OpenAI_StringResponse result = chat.message(txt, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  
  String response = "";
  if (result.length() >= 1) {
    response = result.getAt(0);
    response.trim();
  }
  
  Serial.printf("[VISION] Free heap after analysis: %d bytes\n", ESP.getFreeHeap());
  yield();
  
  return response;
}

bool downloadTTSToSD(String text, String filename) {
  Serial.println("\n[TTS] Requesting MP3 from OpenAI TTS...");
  
  // Truncate if too long (OpenAI limit is 4096 chars)
  if (text.length() > 3000) {
    text = text.substring(0, 3000);
    Serial.println("[TTS] ⚠️  Text truncated to 3000 characters");
  }
  
  HTTPClient http;
  http.begin("https://api.openai.com/v1/audio/speech");
  http.addHeader("Authorization", "Bearer " + String(api_key));
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);  // 30 second timeout
  
  String escapedText = escapeJSON(text);
  String payload = "{\"model\":\"tts-1\",\"input\":\"" + escapedText + "\",\"voice\":\"alloy\"}";
  
  Serial.printf("[TTS] Text length: %d characters\n", text.length());
  
  int httpCode = http.POST(payload);
  
  if (httpCode != 200) {
    Serial.printf("[TTS] ✗ HTTP Error: %d\n", httpCode);
    String response = http.getString();
    if (response.length() > 0) {
      Serial.printf("[TTS] Error response: %s\n", response.c_str());
    }
    http.end();
    return false;
  }
  
  Serial.println("[TTS] ✓ Downloading MP3 to SD card...");
  
  // Delete old file if exists
  if (SD.exists(filename.c_str())) {
    SD.remove(filename.c_str());
  }
  
  File audioFile = SD.open(filename.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("[TTS] ✗ Failed to create file on SD card");
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
      
      // Progress indicator
      if (totalSize / 5120 > dots) {
        Serial.print("▓");
        dots = totalSize / 5120;
      }
    } else if (!http.connected()) {
      break;
    } else {
      delay(1);
      yield();
    }
    
    // Timeout check
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
  Serial.printf("[TTS] Free heap: %d bytes\n", ESP.getFreeHeap());
  
  return totalSize > 0;
}

bool playAudioFromSD(String filename) {
  if (!audioPlayerReady || !sdCardAvailable) {
    Serial.println("[PLAY] ✗ Audio system not ready");
    return false;
  }
  
  if (!SD.exists(filename.c_str())) {
    Serial.printf("[PLAY] ✗ File not found: %s\n", filename.c_str());
    return false;
  }
  
  Serial.printf("\n[PLAY] 🔊 Playing audio through MAX98357...\n");
  Serial.printf("[PLAY] File: %s\n", filename.c_str());
  
  // Ensure MAX98357 is enabled
  digitalWrite(MAX98357_MODE, HIGH);
  delay(10);
  
  // Connect to file and start playback
  audioPlayer.connecttoFS(SD, filename.c_str());
  
  unsigned long startTime = millis();
  int lastSecond = 0;
  
  Serial.print("[PLAY] ");
  
  // Playback loop
  while (audioPlayer.isRunning()) {
    audioPlayer.loop();
    
    // Progress indicator
    int currentSecond = (millis() - startTime) / 1000;
    if (currentSecond > lastSecond) {
      Serial.print("♪");
      lastSecond = currentSecond;
    }
    
    delay(1);
    yield();
    
    // Timeout safety
    if (millis() - startTime > 60000) {
      Serial.println("\n[PLAY] ⚠️  Playback timeout (60s)");
      break;
    }
  }
  
  Serial.printf("\n[PLAY] ✓ Playback complete (%.1f seconds)\n", 
                (millis() - startTime) / 1000.0);
  
  return true;
}

int TextToSpeechEnhanced(String txt) {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║   TEXT-TO-SPEECH (SD BUFFERED)        ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  if (!sdCardAvailable || !audioPlayerReady) {
    Serial.println("[TTS] ✗ System not ready");
    Serial.println("       SD Card: " + String(sdCardAvailable ? "OK" : "MISSING"));
    Serial.println("       Audio:   " + String(audioPlayerReady ? "OK" : "NOT READY"));
    return -1;
  }
  
  String audioFile = "/audio/resp_" + String(millis()) + ".mp3";
  
  // Download MP3 from OpenAI
  if (!downloadTTSToSD(txt, audioFile)) {
    return -1;
  }
  
  // Play from SD card
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
  Serial.printf("Free heap before recording: %d bytes\n\n", ESP.getFreeHeap());
  
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    Serial.println("[ERROR] ✗ Memory allocation failed!");
    Serial.printf("  Requested: %d bytes\n", PCM_WAV_HEADER_SIZE);
    Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    return;
  }
  
  size_t sampleRate = I2S_MIC.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S_MIC.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S_MIC.rxSlotMode();
  
  const pcm_wav_header_t wavHeader = PCM_WAV_HEADER_DEFAULT(0, sampleWidth, sampleRate, numChannels);
  memcpy(wavBuffer, &wavHeader, PCM_WAV_HEADER_SIZE);
  wavBufferSize = PCM_WAV_HEADER_SIZE;
  
  Serial.println(">>> 🎤 SPEAK YOUR QUESTION NOW...\n");
  
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
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  
  // Check minimum length
  if (duration < 0.5) {
    Serial.println("\n✗ Recording too short (minimum 0.5 seconds)\n");
    free(wavBuffer);
    wavBuffer = NULL;
    wavBufferSize = 0;
    isProcessing = false;
    streamingEnabled = true;
    return;
  }
  
  // Update WAV header with actual sizes
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = wavBufferSize + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║       AI PROCESSING PIPELINE       ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // ===== STEP 1: Speech Recognition =====
  Serial.println("━━━ STEP 1/3: SPEECH RECOGNITION ━━━");
  String txt = SpeechToText();
  
  if (txt.length() > 0) {
    Serial.printf("✓ Transcription: \"%s\"\n", txt.c_str());
    
    // ===== STEP 2: Vision Analysis =====
    Serial.println("\n━━━ STEP 2/3: VISION ANALYSIS ━━━");
    String response = imageAnswering(txt);
    
    if (response.length() > 0) {
      Serial.println("\n✓ AI Response:");
      Serial.println("┌───────────────────────────────────┐");
      Serial.printf("│ %s\n", response.c_str());
      Serial.println("└───────────────────────────────────┘");
      
      // ===== STEP 3: Text-to-Speech =====
      Serial.println("\n━━━ STEP 3/3: AUDIO PLAYBACK ━━━");
      if (TextToSpeechEnhanced(response) == 0) {
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
  Serial.println("╚════════════════════════════════════╝");
  Serial.printf("Free heap after processing: %d bytes\n\n", ESP.getFreeHeap());
  
  isProcessing = false;
  streamingEnabled = true;
}

// Audio library callback functions
void audio_info(const char *info) {
  // Optional: Uncomment for debugging audio info
  // Serial.printf("[AUDIO] %s\n", info);
}

void audio_eof_mp3(const char *info) {
  // End of MP3 file - playback complete
}
