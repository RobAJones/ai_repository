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
uint8_t *wavBuffer = NULL;
size_t wavBufferSize = 0; 

// Button debouncing
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 500;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32-S3 Voice-to-Vision System (DIAGNOSTIC MODE) ===");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Test button reading
  Serial.printf("Button initial state: %d (LOW=0 when pressed)\n", digitalRead(BUTTON_PIN));
  
  // Initialize camera first with optimized settings
  Serial.println("\n[1/5] Initializing camera...");
  initCamera();
  Serial.println("✓ Camera initialized!");
  
  // Initialize audio with diagnostics
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
  Serial.println("Monitoring button state...");
}

void loop() {
  // Handle web server requests
  server.handleClient();
  
  // Read button state
  int buttonState = digitalRead(BUTTON_PIN);
  static int lastButtonState = HIGH;
  static unsigned long buttonPressTime = 0;
  
  // Detect button state changes
  if (buttonState != lastButtonState) {
    if (buttonState == LOW) {
      // Button pressed
      Serial.println("\n[BUTTON] Pressed detected!");
      buttonPressTime = millis();
      
      if (!isRecording && !isProcessing) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress > debounceDelay) {
          lastButtonPress = currentTime;
          Serial.println("[BUTTON] Starting recording...");
          startRecording();
        } else {
          Serial.println("[BUTTON] Debounce - ignoring press");
        }
      } else {
        Serial.printf("[BUTTON] Ignoring - isRecording=%d, isProcessing=%d\n", isRecording, isProcessing);
      }
    } else {
      // Button released
      if (buttonPressTime > 0) {
        unsigned long pressDuration = millis() - buttonPressTime;
        Serial.printf("\n[BUTTON] Released after %lu ms\n", pressDuration);
        buttonPressTime = 0;
      }
      
      if (isRecording) {
        Serial.println("[BUTTON] Stopping recording...");
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
        Serial.printf("[ERROR] Failed to reallocate WAV buffer! Current size: %u, Requested: %u\n", 
                      wavBufferSize, bytesAvailable);
        stopRecording();
        return;
      }
      wavBuffer = newBuffer;
      
      size_t bytesRead = I2S.readBytes((char *)(wavBuffer + wavBufferSize), bytesAvailable);
      wavBufferSize += bytesRead;
      
      // Log progress every second
      if (currentTime - lastLogTime > 1000) {
        Serial.printf("[RECORDING] Buffer size: %u bytes, Last read: %u bytes\n", wavBufferSize, bytesRead);
        lastLogTime = currentTime;
      }
    }
  }
  
  delay(1);
}

void testI2S() {
  Serial.println("Testing I2S for 2 seconds...");
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
  
  Serial.printf("I2S Test Results:\n");
  Serial.printf("  Total bytes read: %u\n", totalBytes);
  Serial.printf("  Read operations: %d\n", readCount);
  Serial.printf("  Average per read: %u bytes\n", readCount > 0 ? totalBytes / readCount : 0);
  
  if (totalBytes > 0) {
    Serial.println("✓ I2S microphone is working!");
  } else {
    Serial.println("✗ WARNING: No data from I2S microphone!");
    Serial.println("  Check microphone connections:");
    Serial.printf("    Clock pin: GPIO_%d\n", CLOCK_PIN);
    Serial.printf("    Data pin: GPIO_%d\n", DATA_PIN);
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><title>ESP32-S3 Camera</title>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{font-family:Arial;text-align:center;margin:0;background:#000;color:#fff}"
                  "img{max-width:100%;height:auto;border:2px solid #4CAF50}"
                  ".container{padding:20px}"
                  ".status{background:#333;padding:10px;margin:10px;border-radius:5px}"
                  "h1{color:#4CAF50}"
                  ".recording{background:#ff0000;animation:blink 1s infinite}"
                  "@keyframes blink{0%{opacity:1}50%{opacity:0.5}100%{opacity:1}}"
                  "</style>"
                  "<script>"
                  "setInterval(function(){"
                  "  fetch('/status').then(r=>r.json()).then(d=>{"
                  "    document.getElementById('status').className='status'+(d.recording?' recording':'');"
                  "    document.getElementById('status').innerHTML=d.status;"
                  "  });"
                  "},500);"
                  "</script>"
                  "</head><body>"
                  "<div class='container'>"
                  "<h1>ESP32-S3 Camera Preview</h1>"
                  "<div id='status' class='status'>Press BOOT button to start</div>"
                  "<img src='/stream' />"
                  "</div></body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/status", HTTP_GET, []() {
    String status = "Ready";
    if (isRecording) status = "🔴 RECORDING";
    else if (isProcessing) status = "⚙️ Processing...";
    
    String json = "{\"recording\":" + String(isRecording) + 
                  ",\"processing\":" + String(isProcessing) + 
                  ",\"status\":\"" + status + "\"}";
    server.send(200, "application/json", json);
  });
  
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
}

void handleStream() {
  WiFiClient client = server.client();
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();
  
  while (client.connected() && !isRecording && !isProcessing) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[STREAM] Camera capture failed");
      delay(100);
      continue;
    }
    
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    
    esp_camera_fb_return(fb);
    
    delay(100);
    
    if (!client.connected()) break;
  }
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
  Serial.printf("Configuring I2S with Clock=%d, Data=%d\n", CLOCK_PIN, DATA_PIN);
  I2S.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("✗ Failed to initialize I2S PDM RX");
  } else {
    Serial.println("✓ I2S initialized successfully");
    Serial.printf("  Sample rate: %d Hz\n", I2S.rxSampleRate());
    Serial.printf("  Data width: %d bits\n", I2S.rxDataWidth());
    Serial.printf("  Slot mode: %d (1=mono, 2=stereo)\n", I2S.rxSlotMode());
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
  Serial.printf("[STT] Audio buffer size: %u bytes\n", wavBufferSize);
  
  String txt = audio.file(wavBuffer, wavBufferSize, (OpenAI_Audio_Input_Format)5);
  
  Serial.printf("[STT] Result length: %d\n", txt.length());
  
  free(wavBuffer);
  wavBuffer = NULL;
  wavBufferSize = 0;
  
  return txt;
}

String imageAnswering(String txt) {
  camera_fb_t *fb = NULL; 
  String response;
  
  Serial.println("\n[VISION] Capturing image for analysis...");
  
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("[VISION] ✗ Failed to capture image!");
    return "";
  }
  
  Serial.printf("[VISION] ✓ Image captured - Size: %d bytes\n", fb->len);
  delay(100);
  
  Serial.println("[VISION] Sending to OpenAI Vision API...");
  Serial.printf("[VISION] Prompt: \"%s\"\n", txt.c_str());
  
  OpenAI_StringResponse result = chat.message(txt, fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  
  Serial.printf("[VISION] Response parts: %d\n", result.length());
  
  if (result.length() >= 1) {
    for (unsigned int i = 0; i < result.length(); ++i) {
      response = result.getAt(i);
      response.trim();
    }
  }
  
  return response;
}

int TextToSpeech(String txt) {
  Serial.println("\n[TTS] Converting text to speech...");
  Serial.printf("[TTS] Text length: %d\n", txt.length());
  int result = tts.message(txt);
  Serial.printf("[TTS] Result: %d\n", result);
  return result;
}

void startRecording() {
  Serial.println("\n================================");
  Serial.println("=== STARTING RECORDING ===");
  Serial.println("================================");
  
  size_t sampleRate = I2S.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S.rxSlotMode();
  
  Serial.printf("[RECORD] Sample rate: %u Hz\n", sampleRate);
  Serial.printf("[RECORD] Sample width: %u bits\n", sampleWidth);
  Serial.printf("[RECORD] Channels: %u\n", numChannels);
  
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    Serial.println("[RECORD] ✗ Failed to allocate WAV buffer!");
    Serial.printf("[RECORD] Requested size: %d bytes\n", PCM_WAV_HEADER_SIZE);
    Serial.printf("[RECORD] Free heap: %u bytes\n", ESP.getFreeHeap());
    return;
  }
  
  Serial.printf("[RECORD] ✓ Allocated %d bytes for WAV header\n", PCM_WAV_HEADER_SIZE);
  
  const pcm_wav_header_t wavHeader = PCM_WAV_HEADER_DEFAULT(0, sampleWidth, sampleRate, numChannels);
  memcpy(wavBuffer, &wavHeader, PCM_WAV_HEADER_SIZE);
  wavBufferSize = PCM_WAV_HEADER_SIZE;
  
  Serial.println("[RECORD] ✓ WAV header initialized");
  Serial.println("[RECORD] 🎤 RECORDING NOW - Speak your question...");
  Serial.println("[RECORD] (Release button to stop)\n");
  
  digitalWrite(LED_PIN, LOW);
  isRecording = true;
}

void stopRecording() {
  if (!isRecording) {
    Serial.println("[STOP] Not recording, ignoring stop request");
    return;
  }
  
  isRecording = false;
  isProcessing = true;
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n================================");
  Serial.println("=== RECORDING STOPPED ===");
  Serial.println("================================");
  Serial.printf("[STOP] Total recorded: %u bytes\n", wavBufferSize);
  Serial.printf("[STOP] Recording duration: ~%.1f seconds\n", 
                (float)(wavBufferSize - PCM_WAV_HEADER_SIZE) / (SAMPLE_RATE * 2));
  
  if (wavBufferSize <= PCM_WAV_HEADER_SIZE) {
    Serial.println("[STOP] ✗ No audio data recorded!");
    Serial.println("[STOP] Possible issues:");
    Serial.println("  - Microphone not connected");
    Serial.println("  - I2S configuration incorrect");
    Serial.println("  - Recording time too short");
    free(wavBuffer);
    wavBuffer = NULL;
    wavBufferSize = 0;
    isProcessing = false;
    return;
  }
  
  // Update WAV header
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = (wavBufferSize) + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n=== AI PROCESSING PIPELINE ===\n");
  
  // Step 1: Speech to Text
  Serial.println("STEP 1/3: Speech Recognition");
  String txt = SpeechToText();
  
  if (txt != NULL && txt.length() > 0) {
    Serial.printf("✓ Transcription: \"%s\"\n", txt.c_str());
    
    // Step 2: Image Analysis
    Serial.println("\nSTEP 2/3: Image Analysis");
    String response = imageAnswering(txt);
    
    if (response != NULL && response.length() > 0) {
      Serial.printf("✓ AI Response:\n%s\n", response.c_str());
      
      // Step 3: Text to Speech
      Serial.println("\nSTEP 3/3: Text-to-Speech");
      if (TextToSpeech(response) == -1) {
        Serial.println("✗ Audio generation failed!");
      } else {
        Serial.println("✓ Audio playback complete");
      }
    } else {
      Serial.println("✗ Image analysis failed!");
    }
  } else {
    Serial.println("✗ Speech recognition failed!");
    Serial.println("Possible issues:");
    Serial.println("  - Audio too short or quiet");
    Serial.println("  - OpenAI API error");
    Serial.println("  - Network connection issue");
  }
  
  Serial.println("\n=== PIPELINE COMPLETE ===");
  Serial.println("Ready for next query.\n");
  
  isProcessing = false;
}
