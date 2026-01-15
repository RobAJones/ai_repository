#include <WiFi.h>
#include <WebServer.h>
#include <OpenAI.h>
#include "camera.h"
#include "wav_header.h"
#include <Arduino.h>
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "ESP_I2S.h"

#define BUTTON_PIN 0
#define LED_PIN 3
#define SAMPLE_RATE     (16000)
#define DATA_PIN        (GPIO_NUM_39)
#define CLOCK_PIN       (GPIO_NUM_38)

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* api_key = "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";

OpenAI openai(api_key);
OpenAI_ChatCompletion chat(openai);
OpenAI_AudioTranscription audio(openai);
OpenAI_TTS tts(openai);
I2SClass I2S;
WebServer server(80);

// State management
bool isRecording = false; 
bool isProcessing = false;
bool streamingEnabled = true; // Control streaming to prevent FB-OVF
uint8_t *wavBuffer = NULL;
size_t wavBufferSize = 0; 

// Button debouncing
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 500;

// Camera frame management
unsigned long lastFrameRequest = 0;
const unsigned long minFrameInterval = 150; // Slower = more stable (150ms = ~6.7 FPS)

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32-S3 Voice-to-Vision System ===");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Test button
  Serial.printf("Button initial state: %d\n", digitalRead(BUTTON_PIN));
  
  // Initialize camera
  Serial.println("\n[1/5] Initializing camera...");
  initCamera();
  Serial.println("✓ Camera initialized!");
  
  // Initialize audio
  Serial.println("\n[2/5] Initializing audio...");
  audioInit();
  Serial.println("✓ Audio initialized!");
  
  // Test I2S
  Serial.println("\n[3/5] Testing I2S microphone...");
  testI2S();
  
  // Connect to WiFi
  Serial.println("\n[4/5] Connecting to WiFi...");
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
  Serial.println("✓ Web server started!");
  Serial.printf("View camera at: http://%s/\n", WiFi.localIP().toString().c_str());
  
  // Initialize chat
  Serial.println("\n[5/5] Initializing OpenAI...");
  chatInit();
  Serial.println("✓ OpenAI initialized!");
  
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n=== System Ready ===");
  Serial.printf("Camera preview: http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.println("\n>>> Press and HOLD BOOT button to record");
  Serial.println(">>> Release button to process\n");
}

void loop() {
  // Handle web server requests (only when not busy)
  server.handleClient();
  
  // Read button state
  int buttonState = digitalRead(BUTTON_PIN);
  static int lastButtonState = HIGH;
  static unsigned long buttonPressTime = 0;
  
  // Detect button state changes
  if (buttonState != lastButtonState) {
    if (buttonState == LOW) {
      // Button pressed
      Serial.println("\n[BUTTON] Pressed!");
      buttonPressTime = millis();
      
      if (!isRecording && !isProcessing) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress > debounceDelay) {
          lastButtonPress = currentTime;
          streamingEnabled = false; // STOP streaming during recording
          Serial.println("[CAMERA] Streaming disabled for recording");
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
  
  // Continue recording audio while button is held
  if (isRecording) {
    static unsigned long lastLogTime = 0;
    unsigned long currentTime = millis();
    
    size_t bytesAvailable = I2S.available();
    if (bytesAvailable > 0) {
      uint8_t *newBuffer = (uint8_t *)realloc(wavBuffer, wavBufferSize + bytesAvailable);
      if (newBuffer == NULL) {
        Serial.printf("[ERROR] Realloc failed! Size: %u + %u\n", wavBufferSize, bytesAvailable);
        stopRecording();
        return;
      }
      wavBuffer = newBuffer;
      
      size_t bytesRead = I2S.readBytes((char *)(wavBuffer + wavBufferSize), bytesAvailable);
      wavBufferSize += bytesRead;
      
      // Log progress every second
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

void testI2S() {
  Serial.println("Testing microphone for 2 seconds...");
  unsigned long startTime = millis();
  size_t totalBytes = 0;
  int readCount = 0;
  
  while (millis() - startTime < 2000) {
    size_t available = I2S.available();
    if (available > 0) {
      uint8_t tempBuffer[512];
      size_t toRead = min(available, sizeof(tempBuffer));
      size_t bytesRead = I2S.readBytes((char*)tempBuffer, toRead);
      totalBytes += bytesRead;
      readCount++;
    }
    delay(10);
  }
  
  Serial.printf("I2S Test:\n");
  Serial.printf("  Total: %u bytes in %d reads\n", totalBytes, readCount);
  
  if (totalBytes > 1000) {
    Serial.println("✓ Microphone working!");
  } else {
    Serial.println("✗ WARNING: Little/no data from microphone!");
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
                  ".status{background:#333;padding:10px;margin:10px;border-radius:5px;font-size:18px}"
                  "h1{color:#4CAF50}"
                  ".recording{background:#ff0000;animation:blink 1s infinite}"
                  ".processing{background:#ff9800}"
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
                  "    if(!img && !d.recording && !d.processing){"
                  "      img = document.getElementById('camera');"
                  "      img.src='/stream?'+Date.now();"
                  "    }"
                  "  });"
                  "}"
                  "setInterval(updateStatus, 500);"
                  "updateStatus();"
                  "</script>"
                  "</head><body>"
                  "<div class='container'>"
                  "<h1>ESP32-S3 Camera</h1>"
                  "<div id='status' class='status'>Loading...</div>"
                  "<img id='camera' alt='Camera feed' />"
                  "<p>Press BOOT button on device to record</p>"
                  "</div></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/status", HTTP_GET, []() {
    String status = "Ready - Streaming";
    if (isRecording) status = "🔴 RECORDING";
    else if (isProcessing) status = "⚙️ Processing AI...";
    
    String json = "{\"recording\":" + String(isRecording) + 
                  ",\"processing\":" + String(isProcessing) + 
                  ",\"status\":\"" + status + "\"}";
    server.send(200, "application/json", json);
  });
  
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
}

void handleStream() {
  if (!streamingEnabled) {
    server.send(503, "text/plain", "Streaming paused during recording/processing");
    return;
  }
  
  WiFiClient client = server.client();
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();
  
  int frameCount = 0;
  unsigned long streamStart = millis();
  
  while (client.connected() && streamingEnabled && !isRecording && !isProcessing) {
    // Rate limiting to prevent FB-OVF
    unsigned long now = millis();
    if (now - lastFrameRequest < minFrameInterval) {
      delay(10);
      continue;
    }
    lastFrameRequest = now;
    
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[STREAM] Failed to get frame");
      delay(200);
      continue;
    }
    
    // Send frame
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    
    // CRITICAL: Return frame buffer immediately
    esp_camera_fb_return(fb);
    
    frameCount++;
    
    // Log streaming stats every 10 seconds
    if (frameCount % 60 == 0) {
      float fps = frameCount / ((millis() - streamStart) / 1000.0);
      Serial.printf("[STREAM] %d frames, %.1f FPS\n", frameCount, fps);
    }
    
    if (!client.connected()) break;
  }
  
  Serial.println("[STREAM] Client disconnected");
}

void handleCapture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  
  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
}

void audioInit() {
  Serial.printf("I2S Config: Clock=%d, Data=%d, Rate=%d\n", CLOCK_PIN, DATA_PIN, SAMPLE_RATE);
  I2S.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("✗ I2S init failed");
  } else {
    Serial.println("✓ I2S initialized");
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
}

String SpeechToText() {
  Serial.println("\n[STT] Converting speech to text...");
  Serial.printf("[STT] Buffer: %u bytes\n", wavBufferSize);
  
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
  
  // Get a fresh frame for AI
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("[VISION] ✗ Capture failed!");
    return "";
  }
  
  Serial.printf("[VISION] ✓ Got image: %d bytes\n", fb->len);
  
  Serial.println("[VISION] Sending to OpenAI...");
  
  OpenAI_StringResponse result = chat.message(txt, fb->buf, fb->len);
  
  // Return frame immediately
  esp_camera_fb_return(fb);
  
  if (result.length() >= 1) {
    for (unsigned int i = 0; i < result.length(); ++i) {
      response = result.getAt(i);
      response.trim();
    }
  }
  
  return response;
}

int TextToSpeech(String txt) {
  Serial.println("\n[TTS] Converting to speech...");
  return tts.message(txt);
}

void startRecording() {
  Serial.println("\n========== RECORDING START ==========");
  
  size_t sampleRate = I2S.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S.rxSlotMode();
  
  Serial.printf("[AUDIO] Rate=%u, Width=%u, Channels=%u\n", sampleRate, sampleWidth, numChannels);
  
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    Serial.println("[ERROR] Malloc failed!");
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
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
  
  if (wavBufferSize <= PCM_WAV_HEADER_SIZE + 1000) {
    Serial.println("✗ Recording too short!");
    free(wavBuffer);
    wavBuffer = NULL;
    wavBufferSize = 0;
    isProcessing = false;
    streamingEnabled = true;
    Serial.println("[CAMERA] Streaming re-enabled\n");
    return;
  }
  
  // Update WAV header
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = (wavBufferSize) + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n========== AI PIPELINE ==========\n");
  
  // Step 1: Speech to Text
  Serial.println("STEP 1: Speech Recognition");
  String txt = SpeechToText();
  
  if (txt != NULL && txt.length() > 0) {
    Serial.printf("✓ Text: \"%s\"\n", txt.c_str());
    
    // Step 2: Image Analysis
    Serial.println("\nSTEP 2: Vision Analysis");
    String response = imageAnswering(txt);
    
    if (response != NULL && response.length() > 0) {
      Serial.printf("✓ Response:\n%s\n", response.c_str());
      
      // Step 3: Text to Speech
      Serial.println("\nSTEP 3: Text-to-Speech");
      if (TextToSpeech(response) == -1) {
        Serial.println("✗ TTS failed");
      } else {
        Serial.println("✓ TTS complete");
      }
    } else {
      Serial.println("✗ Vision failed");
    }
  } else {
    Serial.println("✗ Speech recognition failed");
  }
  
  Serial.println("\n========== COMPLETE ==========\n");
  
  isProcessing = false;
  streamingEnabled = true; // Re-enable streaming
  Serial.println("[CAMERA] Streaming re-enabled\n");
}
