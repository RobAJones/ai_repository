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
#include "Audio.h"

// Disable brownout detector for stability
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BUTTON_PIN 0
#define LED_PIN 3

// Microphone
#define SAMPLE_RATE     (16000)
#define MIC_PDM_CLK     (GPIO_NUM_38)
#define MIC_PDM_DATA    (GPIO_NUM_39)

// MAX98357
#define MAX98357_BCLK   (GPIO_NUM_45)
#define MAX98357_LRCLK  (GPIO_NUM_46)
#define MAX98357_DIN    (GPIO_NUM_42)
#define MAX98357_GAIN   (GPIO_NUM_41)
#define MAX98357_SD     (GPIO_NUM_40)

// SD Card (SPI)
#define SD_CS_PIN       (GPIO_NUM_10)
#define SD_MOSI_PIN     (GPIO_NUM_11)
#define SD_MISO_PIN     (GPIO_NUM_13)
#define SD_SCK_PIN      (GPIO_NUM_12)

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* api_key = "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";

OpenAI openai(api_key);
OpenAI_ChatCompletion chat(openai);
OpenAI_AudioTranscription audio(openai);
I2SClass I2S_MIC;
Audio audioPlayer;
WebServer server(80);

// State
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
  // CRITICAL: Disable brownout detector first
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n╔═══════════════════════════════════════════════════╗");
  Serial.println("║   DFRobot ESP32-S3 AI Camera - ULTRA STABLE     ║");
  Serial.println("║   Enhanced Memory Management & Error Handling    ║");
  Serial.println("╚═══════════════════════════════════════════════════╝\n");
  
  // Print memory info
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("PSRAM: %d bytes\n\n", ESP.getFreePsram());
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  initMAX98357();
  
  Serial.println("\n[1/6] Initializing SD Card...");
  initSDCard_SPI();
  
  Serial.println("\n[2/6] Initializing camera...");
  initCamera();
  Serial.println("✓ Camera ready");
  
  Serial.println("\n[3/6] Initializing audio playback...");
  initAudioPlayback();
  
  Serial.println("\n[4/6] Initializing microphone...");
  audioInit();
  
  Serial.println("\n[5/6] Testing microphone...");
  testI2S();
  
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
    Serial.printf("IP: http://%s/\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n✗ WiFi failed - system will work offline");
  }
  
  setupWebServer();
  server.begin();
  
  chatInit();
  
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n╔═══════════════════════════════════════════════════╗");
  Serial.println("║              SYSTEM READY                         ║");
  Serial.println("╚═══════════════════════════════════════════════════╝");
  Serial.printf("Camera:   http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.printf("SD Card:  %s\n", sdCardAvailable ? "✓ ENABLED" : "✗ DISABLED");
  Serial.printf("MAX98357: %s\n", audioPlayerReady ? "✓ READY" : "✗ NOT READY");
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("\n>>> Press BOOT to record\n");
}

void loop() {
  // Only handle web when not processing
  if (!isProcessing && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
  
  // Audio player
  if (audioPlayerReady && !isRecording) {
    audioPlayer.loop();
  }
  
  // Memory monitoring
  if (millis() - lastHeapCheck > 10000) {
    lastHeapCheck = millis();
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 50000) {
      Serial.printf("⚠️  Low memory: %d bytes\n", freeHeap);
    }
  }
  
  // Button handling
  handleButton();
  
  // Recording
  if (isRecording) {
    handleRecording();
  }
  
  yield();  // Feed watchdog
  delay(1);
}

void handleButton() {
  static int lastButtonState = HIGH;
  static unsigned long buttonPressTime = 0;
  
  int buttonState = digitalRead(BUTTON_PIN);
  
  if (buttonState != lastButtonState) {
    if (buttonState == LOW) {
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
      if (buttonPressTime > 0) {
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
      Serial.println("[ERROR] ✗ Out of memory!");
      Serial.printf("Tried to allocate: %d bytes\n", wavBufferSize + chunkSize);
      Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
      stopRecording();
      return;
    }
    wavBuffer = newBuffer;
    
    size_t bytesRead = I2S_MIC.readBytes((char *)(wavBuffer + wavBufferSize), chunkSize);
    wavBufferSize += bytesRead;
    
    // Limit recording to prevent memory overflow
    if (wavBufferSize > 500000) {  // ~15 seconds max
      Serial.println("\n⚠️  Maximum recording length reached");
      stopRecording();
      return;
    }
    
    if (currentTime - lastLogTime > 1000) {
      Serial.printf("[REC] %u bytes (%.1fs) | Heap: %d\n", 
                    wavBufferSize - PCM_WAV_HEADER_SIZE,
                    (float)(wavBufferSize - PCM_WAV_HEADER_SIZE) / (SAMPLE_RATE * 2),
                    ESP.getFreeHeap());
      lastLogTime = currentTime;
    }
  }
}

void initMAX98357() {
  Serial.println("Configuring MAX98357...");
  pinMode(MAX98357_GAIN, OUTPUT);
  digitalWrite(MAX98357_GAIN, HIGH);
  pinMode(MAX98357_SD, OUTPUT);
  digitalWrite(MAX98357_SD, HIGH);
  delay(100);
  Serial.println("✓ MAX98357 configured");
}

void initSDCard_SPI() {
  Serial.printf("SD Card pins: CS=%d MOSI=%d MISO=%d SCK=%d\n", 
                SD_CS_PIN, SD_MOSI_PIN, SD_MISO_PIN, SD_SCK_PIN);
  
  SPIClass spi(HSPI);
  spi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  
  if (!SD.begin(SD_CS_PIN, spi, 40000000)) {
    Serial.println("✗ SD Card failed");
    sdCardAvailable = false;
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("✗ No SD card");
    sdCardAvailable = false;
    return;
  }
  
  Serial.println("✓ SD Card mounted");
  Serial.printf("  Size: %.2f GB\n", SD.cardSize() / (1024.0 * 1024.0 * 1024.0));
  
  sdCardAvailable = true;
  
  if (!SD.exists("/audio")) {
    SD.mkdir("/audio");
  }
}

void initAudioPlayback() {
  if (!sdCardAvailable) {
    Serial.println("✗ Audio disabled (no SD)");
    audioPlayerReady = false;
    return;
  }
  
  audioPlayer.setPinout(MAX98357_BCLK, MAX98357_LRCLK, MAX98357_DIN);
  audioPlayer.setVolume(15);
  
  audioPlayerReady = true;
  Serial.println("✓ Audio ready");
}

void testI2S() {
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
  
  Serial.printf("Mic test: %u bytes\n", totalBytes);
  if (totalBytes > 1000) {
    Serial.println("✓ Microphone working");
  } else {
    Serial.println("✗ Microphone issue");
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><title>DFRobot Camera</title>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{font-family:Arial;text-align:center;margin:0;background:#000;color:#fff}"
                  "img{max-width:100%;border:3px solid #4CAF50;margin:15px auto;border-radius:8px;display:block}"
                  ".status{background:#1a1a1a;padding:18px;margin:15px;border-radius:8px;font-size:20px;font-weight:bold}"
                  ".recording{background:#ff0000;animation:blink 1s infinite}"
                  ".processing{background:#ff9800}"
                  ".btn{background:#4CAF50;color:white;padding:14px 28px;border:none;border-radius:6px;"
                  "cursor:pointer;margin:8px;font-size:16px;font-weight:bold}"
                  ".btn:disabled{background:#444}"
                  "@keyframes blink{50%{opacity:0.5}}"
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
                  "      document.getElementById('st').innerHTML='⚙️ AI Processing...';"
                  "    }else{"
                  "      document.getElementById('st').innerHTML='⚠️ Connection lost';"
                  "    }"
                  "  });"
                  "}"
                  "function replay(){"
                  "  document.getElementById('rep').disabled=true;"
                  "  fetch('/replay').then(()=>setTimeout(()=>{document.getElementById('rep').disabled=false;upd();},2000));"
                  "}"
                  "setInterval(upd,2000);"
                  "upd();"
                  "</script>"
                  "</head><body>"
                  "<h1 style='color:#4CAF50'>🎥 DFRobot AI Camera</h1>"
                  "<div id='st' class='status'>Loading...</div>"
                  "<button id='rep' class='btn' onclick='replay()' disabled>🔊 Replay</button>"
                  "<img id='cam' alt='Camera'/>"
                  "<p style='color:#888'>Press BOOT to record</p>"
                  "</body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/status", HTTP_GET, []() {
    String status = streamingEnabled ? "Ready" : "";
    if (isRecording) status = "🔴 RECORDING";
    else if (isProcessing) status = "⚙️ Processing";
    
    String json = "{\"recording\":" + String(isRecording ? "true" : "false") + 
                  ",\"processing\":" + String(isProcessing ? "true" : "false") +
                  ",\"hasAudio\":" + String(lastAudioFile.length() > 0 ? "true" : "false") +
                  ",\"status\":\"" + status + "\"}";
    server.send(200, "application/json", json);
  });
  
  server.on("/replay", HTTP_GET, []() {
    if (lastAudioFile.length() > 0 && audioPlayerReady) {
      playAudioFromSD(lastAudioFile);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(404, "text/plain", "No audio");
    }
  });
  
  server.on("/stream", HTTP_GET, handleStream);
}

void handleStream() {
  if (!streamingEnabled) {
    server.send(503, "text/plain", "Paused");
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

void audioInit() {
  I2S_MIC.setPinsPdmRx(MIC_PDM_CLK, MIC_PDM_DATA);
  
  if (!I2S_MIC.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("✗ Mic init failed");
  } else {
    Serial.println("✓ Mic initialized");
  }
}

void chatInit() {
  chat.setModel("gpt-4o-mini");
  chat.setSystem("Analyze images concisely in 1-2 sentences.");
  chat.setMaxTokens(300);  // Shorter to reduce memory
  chat.setTemperature(0.2);
}

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
  Serial.println("\n[STT] Converting...");
  Serial.printf("Buffer: %u bytes\n", wavBufferSize);
  
  delay(500);
  yield();
  
  String txt = audio.file(wavBuffer, wavBufferSize, (OpenAI_Audio_Input_Format)5);
  
  // Free memory immediately
  free(wavBuffer);
  wavBuffer = NULL;
  wavBufferSize = 0;
  
  Serial.printf("Free heap after STT: %d\n", ESP.getFreeHeap());
  yield();
  
  return txt;
}

String imageAnswering(String txt) {
  Serial.println("\n[VISION] Capturing...");
  delay(100);
  yield();
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("✗ Capture failed");
    return "";
  }
  
  Serial.printf("Image: %d bytes\n", fb->len);
  yield();
  
  OpenAI_StringResponse result = chat.message(txt, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  
  String response = "";
  if (result.length() >= 1) {
    response = result.getAt(0);
    response.trim();
  }
  
  Serial.printf("Free heap after vision: %d\n", ESP.getFreeHeap());
  yield();
  
  return response;
}

bool downloadTTSToSD(String text, String filename) {
  Serial.println("\n[TTS] Requesting MP3...");
  
  // Truncate if too long
  if (text.length() > 3000) {
    text = text.substring(0, 3000);
    Serial.println("⚠️  Truncated to 3000 chars");
  }
  
  HTTPClient http;
  http.begin("https://api.openai.com/v1/audio/speech");
  http.addHeader("Authorization", "Bearer " + String(api_key));
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);  // 30 second timeout
  
  String escapedText = escapeJSON(text);
  String payload = "{\"model\":\"tts-1\",\"input\":\"" + escapedText + "\",\"voice\":\"alloy\"}";
  
  int httpCode = http.POST(payload);
  
  if (httpCode != 200) {
    Serial.printf("✗ HTTP %d\n", httpCode);
    http.end();
    return false;
  }
  
  Serial.println("Downloading...");
  
  if (SD.exists(filename.c_str())) SD.remove(filename.c_str());
  
  File audioFile = SD.open(filename.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("✗ File create failed");
    http.end();
    return false;
  }
  
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t totalSize = 0;
  unsigned long startTime = millis();
  
  while (http.connected()) {
    size_t available = stream->available();
    if (available > 0) {
      int c = stream->readBytes(buffer, min(available, sizeof(buffer)));
      audioFile.write(buffer, c);
      totalSize += c;
    } else if (!http.connected()) {
      break;
    } else {
      delay(1);
      yield();
    }
    
    if (millis() - startTime > 30000) {
      Serial.println("✗ Timeout");
      audioFile.close();
      http.end();
      return false;
    }
  }
  
  audioFile.close();
  http.end();
  
  Serial.printf("✓ Downloaded %u bytes\n", totalSize);
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  
  return totalSize > 0;
}

bool playAudioFromSD(String filename) {
  if (!audioPlayerReady || !sdCardAvailable) return false;
  if (!SD.exists(filename.c_str())) return false;
  
  Serial.println("\n[PLAY] Playing...");
  digitalWrite(MAX98357_SD, HIGH);
  delay(10);
  
  audioPlayer.connecttoFS(SD, filename.c_str());
  
  unsigned long startTime = millis();
  while (audioPlayer.isRunning()) {
    audioPlayer.loop();
    delay(1);
    if (millis() - startTime > 60000) break;
  }
  
  Serial.println("✓ Playback done");
  return true;
}

int TextToSpeechEnhanced(String txt) {
  if (!sdCardAvailable || !audioPlayerReady) {
    Serial.println("✗ TTS unavailable");
    return -1;
  }
  
  String audioFile = "/audio/r" + String(millis()) + ".mp3";
  
  if (!downloadTTSToSD(txt, audioFile)) return -1;
  if (!playAudioFromSD(audioFile)) return -1;
  
  lastAudioFile = audioFile;
  return 0;
}

void startRecording() {
  Serial.println("\n=== RECORDING START ===");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    Serial.println("✗ Malloc failed!");
    return;
  }
  
  size_t sampleRate = I2S_MIC.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S_MIC.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S_MIC.rxSlotMode();
  
  const pcm_wav_header_t wavHeader = PCM_WAV_HEADER_DEFAULT(0, sampleWidth, sampleRate, numChannels);
  memcpy(wavBuffer, &wavHeader, PCM_WAV_HEADER_SIZE);
  wavBufferSize = PCM_WAV_HEADER_SIZE;
  
  Serial.println("🎤 SPEAK NOW\n");
  digitalWrite(LED_PIN, LOW);
  isRecording = true;
}

void stopRecording() {
  if (!isRecording) return;
  
  isRecording = false;
  isProcessing = true;
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n=== RECORDING STOP ===");
  float duration = (float)(wavBufferSize - PCM_WAV_HEADER_SIZE) / (SAMPLE_RATE * 2);
  Serial.printf("Duration: %.1fs\n", duration);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  
  if (duration < 0.5) {
    Serial.println("✗ Too short");
    free(wavBuffer);
    wavBuffer = NULL;
    wavBufferSize = 0;
    isProcessing = false;
    streamingEnabled = true;
    return;
  }
  
  // Update WAV header
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = wavBufferSize + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n=== AI PIPELINE ===\n");
  
  // Step 1: Speech to Text
  Serial.println("STEP 1/3: Speech Recognition");
  String txt = SpeechToText();
  
  if (txt.length() > 0) {
    Serial.printf("✓ \"%s\"\n", txt.c_str());
    
    // Step 2: Vision
    Serial.println("\nSTEP 2/3: Vision Analysis");
    String response = imageAnswering(txt);
    
    if (response.length() > 0) {
      Serial.printf("✓ %s\n", response.c_str());
      
      // Step 3: TTS
      Serial.println("\nSTEP 3/3: Audio Playback");
      if (TextToSpeechEnhanced(response) == 0) {
        Serial.println("✓ TTS complete");
      } else {
        Serial.println("✗ TTS failed");
      }
    } else {
      Serial.println("✗ Vision failed");
    }
  } else {
    Serial.println("✗ STT failed");
  }
  
  Serial.println("\n=== COMPLETE ===\n");
  Serial.printf("Free heap: %d bytes\n\n", ESP.getFreeHeap());
  
  isProcessing = false;
  streamingEnabled = true;
}

void audio_info(const char *info) {}
void audio_eof_mp3(const char *info) {}
