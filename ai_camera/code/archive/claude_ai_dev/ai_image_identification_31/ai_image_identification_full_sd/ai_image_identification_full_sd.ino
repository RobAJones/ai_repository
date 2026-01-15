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
#define SAMPLE_RATE     (16000)
#define DATA_PIN        (GPIO_NUM_39)  // Microphone data
#define CLOCK_PIN       (GPIO_NUM_38)  // Microphone clock

// I2S Speaker pins (adjust based on your board)
#define I2S_DOUT        (GPIO_NUM_12)  // Speaker data out
#define I2S_BCLK        (GPIO_NUM_10)  // Speaker bit clock
#define I2S_LRC         (GPIO_NUM_11)  // Speaker word select

// SD Card SPI pins (adjust based on your DFRobot board)
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
String lastAudioFile = "";  // Store last TTS file for replay

// Button debouncing
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 500;

// Camera frame management
unsigned long lastFrameRequest = 0;
const unsigned long minFrameInterval = 150;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32-S3 Voice-to-Vision System ===");
  Serial.println("Version: Full SD Card Audio Buffering");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize SD Card first (needed for audio playback)
  Serial.println("\n[1/6] Initializing SD Card...");
  initSDCard();
  
  // Initialize camera
  Serial.println("\n[2/6] Initializing camera...");
  initCamera();
  Serial.println("✓ Camera initialized!");
  
  // Initialize audio playback (speaker)
  Serial.println("\n[3/6] Initializing audio playback...");
  initAudioPlayback();
  
  // Initialize microphone
  Serial.println("\n[4/6] Initializing microphone...");
  audioInit();
  Serial.println("✓ Microphone initialized!");
  
  // Test I2S microphone
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
  
  Serial.println("\n=== System Ready ===");
  Serial.printf("View camera: http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.println("SD Card Audio Buffering: " + String(sdCardAvailable ? "ENABLED" : "DISABLED"));
  Serial.println("\n>>> Press and HOLD BOOT button to record");
  Serial.println(">>> Release button to process\n");
}

void loop() {
  server.handleClient();
  
  // Audio player loop (needed for playback)
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

void initSDCard() {
  Serial.print("Checking for SD card... ");
  
  SPIClass spi(HSPI);
  spi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  
  if (!SD.begin(SD_CS_PIN, spi)) {
    Serial.println("✗ SD Card not found");
    Serial.println("⚠️  Audio buffering DISABLED - will use fallback mode");
    sdCardAvailable = false;
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("✗ No SD card attached");
    sdCardAvailable = false;
    return;
  }
  
  Serial.println("✓ SD Card mounted!");
  Serial.printf("  Type: %s\n", 
    cardType == CARD_MMC ? "MMC" :
    cardType == CARD_SD ? "SD" :
    cardType == CARD_SDHC ? "SDHC" : "Unknown");
  Serial.printf("  Size: %.2f GB\n", SD.cardSize() / (1024.0 * 1024.0 * 1024.0));
  
  sdCardAvailable = true;
  
  // Create audio directory
  if (!SD.exists("/audio")) {
    SD.mkdir("/audio");
    Serial.println("  Created /audio directory");
  }
  
  // Clean up old audio files
  File root = SD.open("/audio");
  File file = root.openNextFile();
  int fileCount = 0;
  while (file) {
    if (!file.isDirectory()) {
      fileCount++;
    }
    file = root.openNextFile();
  }
  Serial.printf("  Existing audio files: %d\n", fileCount);
}

void initAudioPlayback() {
  if (!sdCardAvailable) {
    Serial.println("✗ Audio playback disabled (no SD card)");
    audioPlayerReady = false;
    return;
  }
  
  Serial.println("Initializing I2S audio player...");
  
  // Initialize audio player with I2S pins
  audioPlayer.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audioPlayer.setVolume(21); // 0-21, adjust as needed
  
  audioPlayerReady = true;
  Serial.println("✓ Audio playback ready!");
  Serial.printf("  Speaker pins: BCLK=%d, LRC=%d, DOUT=%d\n", I2S_BCLK, I2S_LRC, I2S_DOUT);
}

void testI2S() {
  Serial.println("Testing microphone for 2 seconds...");
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
  
  Serial.printf("Microphone test: %u bytes captured\n", totalBytes);
  if (totalBytes > 1000) {
    Serial.println("✓ Microphone working!");
  } else {
    Serial.println("✗ WARNING: Microphone may not be working!");
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><title>ESP32-S3 Camera</title>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{font-family:Arial;text-align:center;margin:0;background:#000;color:#fff}"
                  "img{max-width:100%;height:auto;border:2px solid #4CAF50;display:block;margin:10px auto}"
                  ".container{padding:20px}"
                  ".status{background:#333;padding:15px;margin:10px;border-radius:5px;font-size:18px;font-weight:bold}"
                  "h1{color:#4CAF50;margin:10px}"
                  ".recording{background:#ff0000;animation:blink 1s infinite}"
                  ".processing{background:#ff9800}"
                  ".btn{background:#4CAF50;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;margin:5px}"
                  ".btn:hover{background:#45a049}"
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
                  "    document.getElementById('replay').style.display=d.hasAudio?'inline-block':'none';"
                  "    if(!img && !d.recording && !d.processing){"
                  "      img = document.getElementById('camera');"
                  "      img.src='/stream?'+Date.now();"
                  "    }"
                  "  });"
                  "}"
                  "function replayAudio(){"
                  "  fetch('/replay').then(()=>alert('Replaying last response'));"
                  "}"
                  "setInterval(updateStatus, 500);"
                  "updateStatus();"
                  "</script>"
                  "</head><body>"
                  "<div class='container'>"
                  "<h1>🎥 ESP32-S3 Voice-to-Vision</h1>"
                  "<div id='status' class='status'>Loading...</div>"
                  "<button id='replay' class='btn' onclick='replayAudio()' style='display:none'>🔊 Replay Last Response</button>"
                  "<img id='camera' alt='Camera feed' />"
                  "<p style='color:#999'>Press BOOT button on device to record voice command</p>"
                  "</div></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/status", HTTP_GET, []() {
    String status = "Ready - Streaming";
    if (isRecording) status = "🔴 RECORDING";
    else if (isProcessing) status = "⚙️ Processing...";
    
    String json = "{\"recording\":" + String(isRecording) + 
                  ",\"processing\":" + String(isProcessing) +
                  ",\"hasAudio\":" + String(lastAudioFile.length() > 0 ? "true" : "false") +
                  ",\"status\":\"" + status + "\"}";
    server.send(200, "application/json", json);
  });
  
  server.on("/replay", HTTP_GET, []() {
    if (lastAudioFile.length() > 0 && audioPlayerReady) {
      Serial.println("[REPLAY] Playing last audio response...");
      playAudioFromSD(lastAudioFile);
      server.send(200, "text/plain", "Replaying audio");
    } else {
      server.send(404, "text/plain", "No audio to replay");
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
  Serial.printf("I2S Microphone: Clock=GPIO%d, Data=GPIO%d\n", CLOCK_PIN, DATA_PIN);
  I2S_MIC.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  
  if (!I2S_MIC.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("✗ Microphone init failed");
  } else {
    Serial.println("✓ Microphone initialized");
  }
}

void chatInit() {
  chat.setModel("gpt-4o-mini");
  chat.setSystem("You are a helpful assistant that analyzes images and provides concise, informative responses.");
  chat.setMaxTokens(1000);
  chat.setTemperature(0.2);
  chat.setStop("\r");
  chat.setPresencePenalty(0);
  chat.setFrequencyPenalty(0);
  chat.setUser("OpenAI-ESP32-cam");
  
  Serial.println("OpenAI Chat initialized");
}

String SpeechToText() {
  Serial.println("\n[STT] Converting speech to text...");
  Serial.printf("[STT] Buffer: %u bytes\n", wavBufferSize);
  
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
  Serial.println("[VISION] Sending to OpenAI Vision API...");
  
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
  Serial.println("\n[TTS] Downloading MP3 from OpenAI...");
  
  HTTPClient http;
  
  // OpenAI TTS API endpoint
  http.begin("https://api.openai.com/v1/audio/speech");
  http.addHeader("Authorization", "Bearer " + String(api_key));
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  String payload = "{";
  payload += "\"model\":\"tts-1\",";
  payload += "\"input\":\"" + text + "\",";
  payload += "\"voice\":\"alloy\"";  // Options: alloy, echo, fable, onyx, nova, shimmer
  payload += "}";
  
  Serial.println("[TTS] Requesting audio...");
  int httpCode = http.POST(payload);
  
  if (httpCode != 200) {
    Serial.printf("[TTS] ✗ HTTP Error: %d\n", httpCode);
    http.end();
    return false;
  }
  
  Serial.println("[TTS] ✓ Audio received, saving to SD card...");
  
  // Delete old file if exists
  if (SD.exists(filename.c_str())) {
    SD.remove(filename.c_str());
  }
  
  // Open file for writing
  File audioFile = SD.open(filename.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("[TTS] ✗ Failed to create file on SD");
    http.end();
    return false;
  }
  
  // Get the stream
  WiFiClient *stream = http.getStreamPtr();
  
  // Write data to SD card in chunks
  uint8_t buffer[512];
  size_t totalSize = 0;
  unsigned long startTime = millis();
  
  while (http.connected() && (stream->available() > 0 || http.connected())) {
    size_t available = stream->available();
    if (available > 0) {
      int c = stream->readBytes(buffer, min(available, sizeof(buffer)));
      audioFile.write(buffer, c);
      totalSize += c;
      
      // Progress indicator every 10KB
      if (totalSize % 10240 == 0) {
        Serial.printf("[TTS] Downloaded: %u bytes\r", totalSize);
      }
    } else {
      delay(1);
    }
    
    // Timeout after 30 seconds
    if (millis() - startTime > 30000) {
      Serial.println("\n[TTS] ✗ Download timeout");
      audioFile.close();
      http.end();
      return false;
    }
  }
  
  audioFile.close();
  http.end();
  
  Serial.printf("\n[TTS] ✓ Saved %u bytes to %s\n", totalSize, filename.c_str());
  
  return totalSize > 0;
}

bool playAudioFromSD(String filename) {
  if (!audioPlayerReady || !sdCardAvailable) {
    Serial.println("[PLAY] ✗ Audio player not ready");
    return false;
  }
  
  if (!SD.exists(filename.c_str())) {
    Serial.printf("[PLAY] ✗ File not found: %s\n", filename.c_str());
    return false;
  }
  
  Serial.printf("[PLAY] Playing: %s\n", filename.c_str());
  
  // Connect audio file to player
  audioPlayer.connecttoFS(SD, filename.c_str());
  
  // Wait for playback to complete
  while (audioPlayer.isRunning()) {
    audioPlayer.loop();
    delay(10);
  }
  
  Serial.println("[PLAY] ✓ Playback complete");
  
  return true;
}

int TextToSpeechEnhanced(String txt) {
  Serial.println("\n[TTS] === Text-to-Speech with SD Buffering ===");
  Serial.printf("[TTS] Text: \"%s\"\n", txt.c_str());
  
  if (!sdCardAvailable || !audioPlayerReady) {
    Serial.println("[TTS] ⚠️  SD buffering not available - using fallback");
    Serial.println("[TTS] Fallback mode not implemented in this version");
    return -1;
  }
  
  // Generate filename with timestamp
  String audioFile = "/audio/response_" + String(millis()) + ".mp3";
  
  // Step 1: Download MP3 from OpenAI to SD card
  if (!downloadTTSToSD(txt, audioFile)) {
    Serial.println("[TTS] ✗ Failed to download audio");
    return -1;
  }
  
  // Step 2: Play MP3 from SD card
  if (!playAudioFromSD(audioFile)) {
    Serial.println("[TTS] ✗ Failed to play audio");
    return -1;
  }
  
  // Save filename for replay feature
  lastAudioFile = audioFile;
  
  Serial.println("[TTS] ✓ Complete - audio ready for replay!");
  
  return 0;
}

void startRecording() {
  Serial.println("\n========== RECORDING START ==========");
  
  size_t sampleRate = I2S_MIC.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S_MIC.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S_MIC.rxSlotMode();
  
  Serial.printf("[AUDIO] %uHz, %u-bit, %uch\n", sampleRate, sampleWidth, numChannels);
  
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    Serial.println("[ERROR] Malloc failed!");
    return;
  }
  
  const pcm_wav_header_t wavHeader = PCM_WAV_HEADER_DEFAULT(0, sampleWidth, sampleRate, numChannels);
  memcpy(wavBuffer, &wavHeader, PCM_WAV_HEADER_SIZE);
  wavBufferSize = PCM_WAV_HEADER_SIZE;
  
  Serial.println("🎤 RECORDING - Speak now...\n");
  
  digitalWrite(LED_PIN, LOW);
  isRecording = true;
}

void stopRecording() {
  if (!isRecording) return;
  
  isRecording = false;
  isProcessing = true;
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n========== RECORDING STOP ==========");
  Serial.printf("Recorded: %u bytes (%.1f sec)\n", 
                wavBufferSize - PCM_WAV_HEADER_SIZE,
                (float)(wavBufferSize - PCM_WAV_HEADER_SIZE) / (SAMPLE_RATE * 2));
  
  if (wavBufferSize <= PCM_WAV_HEADER_SIZE + 2000) {
    Serial.println("✗ Recording too short!");
    free(wavBuffer);
    wavBuffer = NULL;
    wavBufferSize = 0;
    isProcessing = false;
    streamingEnabled = true;
    Serial.println("[CAMERA] Streaming resumed\n");
    return;
  }
  
  // Update WAV header
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = (wavBufferSize) + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n========== AI PIPELINE ==========\n");
  
  // Step 1: Speech to Text
  Serial.println("STEP 1/3: Speech Recognition");
  String txt = SpeechToText();
  
  if (txt != NULL && txt.length() > 0) {
    Serial.printf("✓ Transcription: \"%s\"\n", txt.c_str());
    
    // Step 2: Image Analysis
    Serial.println("\nSTEP 2/3: Vision Analysis");
    String response = imageAnswering(txt);
    
    if (response != NULL && response.length() > 0) {
      Serial.printf("✓ AI Response:\n%s\n", response.c_str());
      
      // Step 3: Text to Speech with SD buffering
      Serial.println("\nSTEP 3/3: Text-to-Speech with SD Buffering");
      if (TextToSpeechEnhanced(response) == -1) {
        Serial.println("✗ TTS failed");
      } else {
        Serial.println("✓ TTS complete");
      }
    } else {
      Serial.println("✗ Vision analysis failed");
    }
  } else {
    Serial.println("✗ Speech recognition failed");
  }
  
  Serial.println("\n========== COMPLETE ==========\n");
  
  isProcessing = false;
  streamingEnabled = true;
  Serial.println("[CAMERA] Streaming resumed\n");
}

// Audio library callbacks (optional)
void audio_info(const char *info) {
  Serial.printf("[AUDIO INFO] %s\n", info);
}

void audio_eof_mp3(const char *info) {
  Serial.printf("[AUDIO EOF] %s\n", info);
}
