#include <WiFi.h>
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

// State management
bool isRecording = false; 
bool isProcessing = false;  // New flag to prevent button spam during processing
uint8_t *wavBuffer = NULL;
size_t wavBufferSize = 0; 

// Button debouncing
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 500; // 500ms debounce

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  
  Serial.println("\n\n=== ESP32-S3 Voice-to-Vision System ===");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize camera first
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
  
  // Initialize chat
  Serial.println("Initializing OpenAI chat...");
  chatInit();
  Serial.println("Chat initialized!");
  
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("\n=== System Ready ===");
  Serial.println("Camera preview running...");
  Serial.println("Press BOOT button to start recording and processing");
  Serial.println("================================\n");
}

void loop() {
  // Continuous camera preview (capture and release frames)
  if (!isRecording && !isProcessing) {
    displayCameraPreview();
  }
  
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
  
  delay(10); 
}

void displayCameraPreview() {
  // Capture a frame
  camera_fb_t *fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("Camera preview: Failed to capture frame");
    delay(100);
    return;
  }
  
  // Optionally print frame info periodically (every ~2 seconds)
  static unsigned long lastPrintTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastPrintTime > 2000) {
    Serial.printf("Preview: Frame captured - Size: %d bytes, Width: %d, Height: %d\n", 
                  fb->len, fb->width, fb->height);
    lastPrintTime = currentTime;
  }
  
  // Return the frame buffer immediately (this is the "display" - in a real system
  // you'd send this to a display driver or stream it over HTTP)
  esp_camera_fb_return(fb);
  
  // Small delay to prevent overwhelming the system
  delay(100); // ~10 FPS preview
}

void audioInit() {
  I2S.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to initialize I2S PDM RX");
  }
}

void chatInit() {
  chat.setModel("gpt-4o-mini");   // Model to use for completion
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
  
  // Clean up audio buffer
  free(wavBuffer);
  wavBuffer = NULL;
  wavBufferSize = 0;
  
  return txt;
}

String imageAnswering(String txt) {
  camera_fb_t *fb = NULL; 
  String response;
  
  Serial.println("Capturing image for analysis...");
  
  // Take Picture with Camera
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("ERROR: Failed to capture image!");
    return "";
  }
  
  Serial.printf("Image captured - Size: %d bytes\n", fb->len);
  delay(100);
  
  Serial.println("Sending to OpenAI Vision API...");
  
  // Send to OpenAI with the prompt and image
  OpenAI_StringResponse result = chat.message(txt, fb->buf, fb->len);
  
  // Return frame buffer
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
  digitalWrite(LED_PIN, LOW); // LED off during recording
  isRecording = true;
}

void stopRecording() {
  if (!isRecording) return;
  
  isRecording = false;
  isProcessing = true; // Set processing flag to prevent new recordings
  digitalWrite(LED_PIN, HIGH); // LED on when processing
  
  Serial.printf(">>> RECORDING STOPPED - Total size: %u bytes\n\n", wavBufferSize);
  
  // Update WAV header with correct sizes
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = (wavBufferSize) + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  // Process the recording
  Serial.println("=== Starting AI Processing Pipeline ===\n");
  
  // Step 1: Speech to Text
  Serial.println("STEP 1: Speech Recognition");
  String txt = SpeechToText();
  
  if (txt != NULL && txt.length() > 0) {
    Serial.printf("✓ Transcription: \"%s\"\n\n", txt.c_str());
    
    // Step 2: Image Analysis
    Serial.println("STEP 2: Image Analysis");
    String response = imageAnswering(txt);
    
    if (response != NULL && response.length() > 0) {
      Serial.printf("✓ AI Response:\n%s\n\n", response.c_str());
      
      // Step 3: Text to Speech
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
  
  isProcessing = false; // Clear processing flag
}
