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
#define MAX98357_GAIN   (GPIO_NUM_41)  // Gain Control
#define MAX98357_SD     (GPIO_NUM_40)  // Shutdown/Mode Pin

// ========== SD CARD PINS (1-bit SD_MMC mode) ==========
#define SD_CMD_PIN      (GPIO_NUM_35)  // Command
#define SD_CLK_PIN      (GPIO_NUM_36)  // Clock
#define SD_D0_PIN       (GPIO_NUM_37)  // Data 0

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
  Serial.println("║   Voice-to-Vision with SD Audio Buffering        ║");
  Serial.println("╚═══════════════════════════════════════════════════╝\n");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Configure MAX98357 control pins
  initMAX98357();
  
  // Initialize SD Card
  Serial.println("\n[1/6] Initializing SD Card...");
  initSDCard_DFRobot();
  
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
  Serial.println("\nConfiguring MAX98357 control pins...");
  
  // Configure GAIN pin (GPIO 41)
  pinMode(MAX98357_GAIN, OUTPUT);
  digitalWrite(MAX98357_GAIN, HIGH);  // HIGH = 15dB gain, LOW = 9dB gain
  Serial.printf("  GAIN Pin (GPIO%d): HIGH (15dB gain)\n", MAX98357_GAIN);
  
  // Configure SD/Mode pin (GPIO 40)
  pinMode(MAX98357_SD, OUTPUT);
  digitalWrite(MAX98357_SD, HIGH);  // HIGH = Normal operation, LOW = Shutdown
  Serial.printf("  SD Pin (GPIO%d): HIGH (amplifier enabled)\n", MAX98357_SD);
  
  delay(100);  // Let amplifier stabilize
  Serial.println("✓ MAX98357 control pins configured");
}

void initSDCard_DFRobot() {
  Serial.println("Initializing SD card (1-bit SD_MMC mode)...");
  Serial.printf("  CMD Pin: GPIO%d\n", SD_CMD_PIN);
  Serial.printf("  CLK Pin: GPIO%d\n", SD_CLK_PIN);
  Serial.printf("  D0 Pin:  GPIO%d\n", SD_D0_PIN);
  
  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
  
  if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
    Serial.println("✗ SD Card mount failed");
    Serial.println("⚠️  Trying without custom pins...");
    
    if (!SD_MMC.begin("/sdcard", true)) {
      Serial.println("✗ SD Card not available");
      Serial.println("⚠️  Audio buffering DISABLED");
      sdCardAvailable = false;
      return;
    }
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("✗ No SD card detected");
    sdCardAvailable = false;
    return;
  }
  
  Serial.println("✓ SD Card mounted!");
  Serial.printf("  Type: %s\n", 
    cardType == CARD_MMC ? "MMC" :
    cardType == CARD_SD ? "SD" :
    cardType == CARD_SDHC ? "SDHC" : "Unknown");
  Serial.printf("  Size: %.2f GB\n", SD_MMC.cardSize() / (1024.0 * 1024.0 * 1024.0));
  Serial.printf("  Free: %.2f GB\n", (SD_MMC.cardSize() - SD_MMC.usedBytes()) / (1024.0 * 1024.0 * 1024.0));
  
  sdCardAvailable = true;
  
  if (!SD_MMC.exists("/audio")) {
    SD_MMC.mkdir("/audio");
    Serial.println("  ✓ Created /audio directory");
  }
  
  // List existing files
  File root = SD_MMC.open("/audio");
  int fileCount = 0;
  size_t totalSize = 0;
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        fileCount++;
        totalSize += file.size();
      }
      file = root.openNextFile();
    }
    root.close();
  }
  Serial.printf("  Cached audio files: %d (%.2f MB)\n", fileCount, totalSize / (1024.0 * 1024.0));
}

void initAudioPlayback() {
  if (!sdCardAvailable) {
    Serial.println("✗ Audio playback disabled (no SD card)");
    audioPlayerReady = false;
    return;
  }
  
  Serial.println("Initializing MAX98357 I2S interface...");
  Serial.printf("  BCLK (Bit Clock):    GPIO%d\n", MAX98357_BCLK);
  Serial.printf("  LRCLK (Word Select): GPIO%d\n", MAX98357_LRCLK);
  Serial.printf("  DIN (Data In):       GPIO%d\n", MAX98357_DIN);
  
  // Initialize audio player with MAX98357 pins
  audioPlayer.setPinout(MAX98357_BCLK, MAX98357_LRCLK, MAX98357_DIN);
  
  // Set volume (0-21)
  // With 15dB gain from MAX98357, start at moderate volume
  audioPlayer.setVolume(15);  // Adjust between 10-21 as needed
  
  audioPlayerReady = true;
  Serial.println("✓ MAX98357 ready for audio playback!");
  Serial.println("  Gain: 15dB (hardware)");
  Serial.println("  Volume: 15/21 (software)");
}

void testI2S() {
  Serial.println("Testing PDM microphone for 2 seconds...");
  unsigned long startTime = millis();
  size_t totalBytes = 0;
  
  while (millis() - startTime < 2000) {
    size_t available = I2S_MIC.available();
    if (available > 0) {
      uint8_t tempBuffer[512];
      size_t toRead = min(available, sizeof(tempBuffer));
      size_t bytesRead = I2S_MIC.readBytes((char*)tempBuffer, toRead);
      totalBytes += bytesRead;
    }
    delay(10);
  }
  
  Serial.printf("Microphone test: %u bytes in 2 seconds\n", totalBytes);
  Serial.printf("  Data rate: %.2f KB/s\n", totalBytes / 2048.0);
  
  if (totalBytes > 1000) {
    Serial.println("✓ PDM Microphone working!");
  } else {
    Serial.println("✗ WARNING: Microphone not capturing data!");
    Serial.printf("  Check GPIO%d (CLK) and GPIO%d (DATA)\n", MIC_PDM_CLK, MIC_PDM_DATA);
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String sdStatus = sdCardAvailable ? "🟢 ENABLED" : "🔴 DISABLED";
    String ampStatus = audioPlayerReady ? "🟢 READY" : "🔴 NOT READY";
    
    String html = "<!DOCTYPE html><html><head><title>DFRobot AI Camera</title>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{font-family:Arial;text-align:center;margin:0;background:#000;color:#fff}"
                  "img{max-width:100%;height:auto;border:2px solid #4CAF50;display:block;margin:10px auto}"
                  ".container{padding:20px}"
                  ".status{background:#333;padding:15px;margin:10px;border-radius:5px;font-size:18px;font-weight:bold}"
                  ".info{background:#1a1a1a;padding:8px;margin:5px;border-radius:5px;font-size:13px;color:#999}"
                  "h1{color:#4CAF50;margin:10px;font-size:24px}"
                  ".recording{background:#ff0000;animation:blink 1s infinite}"
                  ".processing{background:#ff9800}"
                  ".btn{background:#4CAF50;color:white;padding:12px 24px;border:none;border-radius:5px;"
                  "cursor:pointer;margin:5px;font-size:16px;font-weight:bold}"
                  ".btn:hover{background:#45a049}"
                  ".btn:disabled{background:#555;cursor:not-allowed}"
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
                  "  fetch('/replay').then(r=>r.text()).then(msg=>{"
                  "    document.getElementById('status').innerHTML='🔊 '+msg;"
                  "    setTimeout(()=>{"
                  "      document.getElementById('replay').disabled=false;"
                  "      updateStatus();"
                  "    }, 3000);"
                  "  });"
                  "}"
                  "setInterval(updateStatus, 500);"
                  "updateStatus();"
                  "</script>"
                  "</head><body>"
                  "<div class='container'>"
                  "<h1>🎥 DFRobot AI Camera + MAX98357</h1>"
                  "<div id='status' class='status'>Loading...</div>"
                  "<div class='info'>SD Card: " + sdStatus + " | MAX98357: " + ampStatus + "</div>"
                  "<button id='replay' class='btn' onclick='replayAudio()' disabled>🔊 Replay Last Response</button>"
                  "<img id='camera' alt='Camera feed' />"
                  "<p style='color:#777;font-size:14px'>Press BOOT button on device to record</p>"
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
        server.send(200, "text/plain", "Replayed successfully!");
      } else {
        server.send(500, "text/plain", "Playback failed");
      }
    } else if (!audioPlayerReady) {
      server.send(503, "text/plain", "MAX98357 not ready");
    } else {
      server.send(404, "text/plain", "No audio cached");
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
  Serial.println("PDM Microphone Configuration:");
  Serial.printf("  Clock: GPIO%d\n", MIC_PDM_CLK);
  Serial.printf("  Data:  GPIO%d\n", MIC_PDM_DATA);
  Serial.printf("  Rate:  %d Hz\n", SAMPLE_RATE);
  Serial.printf("  Mode:  PDM (Pulse Density Modulation)\n");
  
  I2S_MIC.setPinsPdmRx(MIC_PDM_CLK, MIC_PDM_DATA);
  
  if (!I2S_MIC.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("✗ PDM Microphone init failed");
  } else {
    Serial.println("✓ PDM Microphone initialized");
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
  Serial.printf("[STT] Audio buffer: %u bytes\n", wavBufferSize);
  
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
  
  Serial.println("\n[VISION] Capturing image...");
  delay(100);
  
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("[VISION] ✗ Capture failed!");
    return "";
  }
  
  Serial.printf("[VISION] ✓ Image: %d bytes\n", fb->len);
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
  
  int httpCode = http.POST(payload);
  
  if (httpCode != 200) {
    Serial.printf("[TTS] ✗ HTTP Error: %d\n", httpCode);
    http.end();
    return false;
  }
  
  Serial.println("[TTS] ✓ Downloading MP3 to SD card...");
  
  if (SD_MMC.exists(filename.c_str())) {
    SD_MMC.remove(filename.c_str());
  }
  
  File audioFile = SD_MMC.open(filename.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("[TTS] ✗ Failed to create file");
    http.end();
    return false;
  }
  
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t totalSize = 0;
  unsigned long startTime = millis();
  
  Serial.print("[TTS] Progress: ");
  while (http.connected()) {
    size_t available = stream->available();
    if (available > 0) {
      int c = stream->readBytes(buffer, min(available, sizeof(buffer)));
      audioFile.write(buffer, c);
      totalSize += c;
      
      if (totalSize % 10240 == 0) {
        Serial.print("▓");
      }
    } else if (!http.connected()) {
      break;
    } else {
      delay(1);
    }
    
    if (millis() - startTime > 30000) {
      Serial.println("\n[TTS] ✗ Timeout");
      audioFile.close();
      http.end();
      return false;
    }
  }
  
  audioFile.close();
  http.end();
  
  Serial.printf("\n[TTS] ✓ Downloaded %u bytes\n", totalSize);
  Serial.printf("[TTS] Saved to: %s\n", filename.c_str());
  
  return totalSize > 0;
}

bool playAudioFromSD(String filename) {
  if (!audioPlayerReady || !sdCardAvailable) {
    Serial.println("[PLAY] ✗ System not ready");
    return false;
  }
  
  if (!SD_MMC.exists(filename.c_str())) {
    Serial.printf("[PLAY] ✗ File not found: %s\n", filename.c_str());
    return false;
  }
  
  Serial.printf("\n[PLAY] 🔊 Playing through MAX98357...\n");
  Serial.printf("[PLAY] File: %s\n", filename.c_str());
  
  // Enable MAX98357 (should already be enabled, but make sure)
  digitalWrite(MAX98357_SD, HIGH);
  delay(10);
  
  audioPlayer.connecttoFS(SD_MMC, filename.c_str());
  
  unsigned long playbackStart = millis();
  Serial.print("[PLAY] ");
  
  while (audioPlayer.isRunning()) {
    audioPlayer.loop();
    
    // Progress indicator
    if ((millis() - playbackStart) % 1000 == 0) {
      Serial.print("♪");
    }
    
    delay(1);
    
    if (millis() - playbackStart > 60000) {
      Serial.println("\n[PLAY] ⚠️  Timeout");
      break;
    }
  }
  
  Serial.println("\n[PLAY] ✓ Complete");
  
  return true;
}

int TextToSpeechEnhanced(String txt) {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║   TEXT-TO-SPEECH (MAX98357)          ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  if (!sdCardAvailable || !audioPlayerReady) {
    Serial.println("[TTS] ✗ System not ready");
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
  Serial.println("\n[TTS] ✓ Audio cached for replay");
  
  return 0;
}

void startRecording() {
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║      🎤 RECORDING STARTED          ║");
  Serial.println("╚════════════════════════════════════╝");
  
  size_t sampleRate = I2S_MIC.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S_MIC.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S_MIC.rxSlotMode();
  
  Serial.printf("\nConfiguration: %uHz, %u-bit, %uch\n", sampleRate, sampleWidth, numChannels);
  
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    Serial.println("[ERROR] Memory allocation failed!");
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
  Serial.printf("\nRecorded: %u bytes (%.1fs)\n", wavBufferSize - PCM_WAV_HEADER_SIZE, duration);
  
  if (wavBufferSize <= PCM_WAV_HEADER_SIZE + 2000) {
    Serial.println("\n✗ Too short (min 1 second)");
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
    Serial.printf("✓ Transcription: \"%s\"\n", txt.c_str());
    
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
        Serial.println("✓ Audio playback complete");
      } else {
        Serial.println("✗ Audio failed");
      }
    } else {
      Serial.println("✗ Vision failed");
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
  Serial.printf("[♪] %s\n", info);
}

void audio_eof_mp3(const char *info) {
  // End of file - playback complete
}
