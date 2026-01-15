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

// Frame rate control for preview
unsigned long lastFrameTime = 0;
const unsigned long frameInterval = 100; // 100ms = 10 FPS

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== ESP32-S3 Voice-to-Vision System ===");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize camera first with optimized settings
  Serial.println("Initializing camera...");
  initCamera();
  Serial.println("Camera initialized!");
  
  // Initialize audio
  Serial.println("Initializing audio...");
  audioInit();
  Serial.println("Audio initialized!");
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(100);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize web server for camera preview
  setupWebServer();
  server.begin();
  Serial.println("Web server started!");
  Serial.printf("View camera at: http://%s/\n", WiFi.localIP().toString().c_str());
  
  // Initialize chat
  Serial.println("Initializing OpenAI chat...");
  chatInit();
  Serial.println("Chat initialized!");
  
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n=== System Ready ===");
  Serial.printf("Camera preview: http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.println("Press BOOT button to start recording and processing");
  Serial.println("================================\n");
}

void loop() {
  // Handle web server requests
  server.handleClient();
  
  // Handle button press with debouncing
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPress > debounceDelay && !isProcessing) {
      lastButtonPress = currentTime;
      
      if (!isRecording) {
        startRecording();
      }
    }
  } else {
    if (isRecording) {
      stopRecording();
    }
  }
  
  // Continue recording audio while button is held
  if (isRecording) {
    size_t bytesAvailable = I2S.available();
    if (bytesAvailable > 0) {
      uint8_t *newBuffer = (uint8_t *)realloc(wavBuffer, wavBufferSize + bytesAvailable);
      if (newBuffer == NULL) {
        log_e("Failed to reallocate WAV buffer");
        stopRecording();
        return;
      }
      wavBuffer = newBuffer;
      
      size_t bytesRead = I2S.readBytes((char *)(wavBuffer + wavBufferSize), bytesAvailable);
      wavBufferSize += bytesRead;
    }
  }
  
  delay(1); // Minimal delay for responsiveness
}

void setupWebServer() {
  // Main page with live preview
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><title>ESP32-S3 Camera</title>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{font-family:Arial;text-align:center;margin:0;background:#000;color:#fff}"
                  "img{max-width:100%;height:auto;border:2px solid #4CAF50}"
                  ".container{padding:20px}"
                  ".status{background:#333;padding:10px;margin:10px;border-radius:5px}"
                  "h1{color:#4CAF50}"
                  "</style></head><body>"
                  "<div class='container'>"
                  "<h1>ESP32-S3 Camera Preview</h1>"
                  "<div class='status'>Press BOOT button to start voice command</div>"
                  "<img src='/stream' />"
                  "<p>Auto-refreshing camera feed</p>"
                  "</div></body></html>";
    server.send(200, "text/html", html);
  });
  
  // Streaming endpoint
  server.on("/stream", HTTP_GET, handleStream);
  
  // Single frame capture
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
      Serial.println("Camera capture failed");
      delay(100);
      continue;
    }
    
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    
    esp_camera_fb_return(fb);
    
    // Control frame rate
    delay(100); // ~10 FPS
    
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
  I2S.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to initialize I2S PDM RX");
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
  Serial.println("Converting speech to text...");
  String txt = audio.file(wavBuffer, wavBufferSize, (OpenAI_Audio_Input_Format)5);
  
  free(wavBuffer);
  wavBuffer = NULL;
  wavBufferSize = 0;
  
  return txt;
}

String imageAnswering(String txt) {
  camera_fb_t *fb = NULL; 
  String response;
  
  Serial.println("Capturing image for analysis...");
  
  // Capture a fresh frame for AI analysis
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("ERROR: Failed to capture image!");
    return "";
  }
  
  Serial.printf("Image captured - Size: %d bytes\n", fb->len);
  delay(100);
  
  Serial.println("Sending to OpenAI Vision API...");
  
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

int TextToSpeech(String txt) {
  Serial.println("Converting text to speech...");
  return tts.message(txt);
}

void startRecording() {
  size_t sampleRate = I2S.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S.rxSlotMode();
  
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    log_e("Failed to allocate WAV buffer");
    return;
  }
  
  const pcm_wav_header_t wavHeader = PCM_WAV_HEADER_DEFAULT(0, sampleWidth, sampleRate, numChannels);
  memcpy(wavBuffer, &wavHeader, PCM_WAV_HEADER_SIZE);
  wavBufferSize = PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n>>> RECORDING STARTED - Speak your question...");
  digitalWrite(LED_PIN, LOW);
  isRecording = true;
}

void stopRecording() {
  if (!isRecording) return;
  
  isRecording = false;
  isProcessing = true;
  digitalWrite(LED_PIN, HIGH);
  
  Serial.printf(">>> RECORDING STOPPED - Total size: %u bytes\n\n", wavBufferSize);
  
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = (wavBufferSize) + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  Serial.println("=== Starting AI Processing Pipeline ===\n");
  
  Serial.println("STEP 1: Speech Recognition");
  String txt = SpeechToText();
  
  if (txt != NULL && txt.length() > 0) {
    Serial.printf("✓ Transcription: \"%s\"\n\n", txt.c_str());
    
    Serial.println("STEP 2: Image Analysis");
    String response = imageAnswering(txt);
    
    if (response != NULL && response.length() > 0) {
      Serial.printf("✓ AI Response:\n%s\n\n", response.c_str());
      
      Serial.println("STEP 3: Text-to-Speech");
      if (TextToSpeech(response) == -1) {
        Serial.println("✗ Audio generation failed!\n");
      } else {
        Serial.println("✓ Audio playback complete\n");
      }
    } else {
      Serial.println("✗ Image analysis failed!\n");
    }
  } else {
    Serial.println("✗ Speech recognition failed!\n");
  }
  
  Serial.println("=== Pipeline Complete ===\n");
  Serial.println("Ready for next query. Press BOOT button to start.\n");
  
  isProcessing = false;
}
