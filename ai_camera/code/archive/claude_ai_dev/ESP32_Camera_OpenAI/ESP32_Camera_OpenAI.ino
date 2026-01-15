/*
 * ESP32-S3 AI Camera with OpenAI Vision Integration
 * 
 * This sketch captures images using the DFRobot ESP32-S3 AI Camera and sends them
 * to OpenAI's GPT-4 Vision API along with verbal instructions from the user.
 * 
 * Features:
 * - Voice command recording via I2S microphone
 * - Image capture from ESP32-CAM
 * - Speech-to-text using OpenAI Whisper
 * - Image analysis using GPT-4 Vision
 * - Text-to-speech response playback
 * 
 * Hardware Requirements:
 * - DFRobot ESP32-S3 AI Camera (or compatible ESP32-S3 with OV2640 camera)
 * - I2S PDM Microphone (connected to GPIO38/39)
 * - Push button on GPIO0
 * - Status LED on GPIO3
 * 
 * Library Requirements:
 * - OpenAI library (from your uploaded files)
 * - ESP32 Camera library
 * - ESP_I2S library
 * - WiFi library
 */

#include <WiFi.h>
#include <OpenAI.h>
#include "camera.h"
#include "wav_header.h"
#include <Arduino.h>
#include "ESP_I2S.h"

// ========== PIN DEFINITIONS ==========
#define BUTTON_PIN 0        // Button for recording (GPIO0)
#define LED_PIN 3           // Status LED (GPIO3)
#define DATA_PIN (GPIO_NUM_39)    // I2S microphone data pin
#define CLOCK_PIN (GPIO_NUM_38)   // I2S microphone clock pin

// ========== AUDIO SETTINGS ==========
#define SAMPLE_RATE (16000) // 16kHz sample rate for audio recording

// ========== WIFI CREDENTIALS ==========
// Replace with your WiFi credentials
const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";

// ========== OPENAI API KEY ==========
// Replace with your OpenAI API key
const char* api_key = "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";

// ========== GLOBAL OBJECTS ==========
OpenAI openai(api_key);
OpenAI_ChatCompletion chat(openai);
OpenAI_AudioTranscription audio(openai);
OpenAI_TTS tts(openai);
I2SClass I2S;

// ========== RECORDING STATE ==========
bool isRecording = false; 
uint8_t *wavBuffer = NULL;
size_t wavBufferSize = 0; 

// ========== FUNCTION PROTOTYPES ==========
void audioInit();
void chatInit();
void startRecording();
void stopRecording();
String speechToText();
String imageAnalysis(String instruction);
int textToSpeech(String txt);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("ESP32-S3 AI Camera with OpenAI Vision");
  Serial.println("========================================\n");
  
  // Initialize GPIO pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize audio (I2S microphone)
  Serial.println("Initializing I2S microphone...");
  audioInit();
  
  // Initialize camera
  Serial.println("Initializing camera...");
  initCamera();
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Blink LED during connection
  }
  Serial.println("\nWiFi connected successfully!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize OpenAI chat settings
  Serial.println("Initializing OpenAI chat...");
  chatInit();
  
  // Turn on LED to indicate ready state
  digitalWrite(LED_PIN, HIGH);
  Serial.println("\n========================================");
  Serial.println("System Ready!");
  Serial.println("Press and hold button to record voice");
  Serial.println("Release button to process");
  Serial.println("========================================\n");
}

void loop() {
  // Check button state for recording
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!isRecording) {
      startRecording();
    }
  } else {
    if (isRecording) {
      stopRecording();
    }
  }
  
  // Continuously capture audio data while recording
  if (isRecording) {
    size_t bytesAvailable = I2S.available();
    if (bytesAvailable > 0) {
      // Dynamically expand buffer to store audio data
      uint8_t *newBuffer = (uint8_t *)realloc(wavBuffer, wavBufferSize + bytesAvailable);
      if (newBuffer == NULL) {
        Serial.println("ERROR: Failed to allocate memory for audio buffer!");
        stopRecording();
        return;
      }
      wavBuffer = newBuffer;
      
      // Read audio data into buffer
      size_t bytesRead = I2S.readBytes((char *)(wavBuffer + wavBufferSize), bytesAvailable);
      wavBufferSize += bytesRead;
    }
  }
  
  delay(10); // Small delay to prevent CPU hogging
}

/**
 * Initialize I2S PDM microphone
 */
void audioInit() {
  I2S.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("ERROR: Failed to initialize I2S PDM RX!");
    while(1) { 
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200); 
    }
  }
  Serial.println("I2S microphone initialized successfully");
}

/**
 * Initialize OpenAI chat configuration
 * Uses GPT-4 Vision model for image analysis
 */
void chatInit() {
  // Use GPT-4 Vision model (or gpt-4o for better performance)
  chat.setModel("gpt-4o");
  
  // Set system prompt to define assistant behavior
  chat.setSystem("You are a helpful AI assistant that analyzes images and provides detailed, practical responses based on what you see. Be concise but informative.");
  
  // Set maximum response length
  chat.setMaxTokens(500);
  
  // Lower temperature for more consistent responses
  chat.setTemperature(0.3);
  
  // Set stop sequence
  chat.setStop("\r");
  
  // Set penalty parameters
  chat.setPresencePenalty(0);
  chat.setFrequencyPenalty(0);
  
  // Set user identifier
  chat.setUser("ESP32-Camera-User");
  
  Serial.println("OpenAI chat configured successfully");
}

/**
 * Convert recorded audio to text using OpenAI Whisper
 */
String speechToText() {
  Serial.println("Converting speech to text...");
  
  // Send audio buffer to OpenAI Whisper API
  String transcription = audio.file(wavBuffer, wavBufferSize, (OpenAI_Audio_Input_Format)5);
  
  // Free audio buffer memory
  free(wavBuffer);
  wavBuffer = NULL;
  wavBufferSize = 0;
  
  return transcription;
}

/**
 * Capture image and send to OpenAI with instruction
 * 
 * @param instruction The verbal instruction/question about the image
 * @return OpenAI's response describing or analyzing the image
 */
String imageAnalysis(String instruction) {
  camera_fb_t *fb = NULL; 
  String response;
  
  // Capture image from camera
  Serial.println("Capturing image...");
  digitalWrite(LED_PIN, LOW); // Turn off LED during capture
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("ERROR: Camera capture failed!");
    digitalWrite(LED_PIN, HIGH);
    return "Error: Failed to capture image";
  }
  
  Serial.printf("Image captured: %d bytes\n", fb->len);
  delay(100);
  
  // Return the frame buffer (allows camera to be used again)
  esp_camera_fb_return(fb);
  
  // Brief delay before next capture
  delay(500);
  
  // Get a fresh frame for sending to OpenAI
  Serial.println("Sending image to OpenAI Vision API...");
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("ERROR: Second camera capture failed!");
    digitalWrite(LED_PIN, HIGH);
    return "Error: Failed to capture image for analysis";
  }
  
  // Send image and instruction to OpenAI
  OpenAI_StringResponse result = chat.message(instruction, fb->buf, fb->len);
  
  // Release frame buffer
  esp_camera_fb_return(fb);
  digitalWrite(LED_PIN, HIGH); // Turn LED back on
  
  // Process response
  if (result.length() >= 1) {
    for (unsigned int i = 0; i < result.length(); ++i) {
      response = result.getAt(i);
      response.trim();
    }
  } else {
    response = "Error: No response from OpenAI";
    if (result.error()) {
      Serial.printf("OpenAI Error: %s\n", result.error());
    }
  }
  
  return response;
}

/**
 * Convert text response to speech using OpenAI TTS
 * 
 * @param txt Text to convert to speech
 * @return Status code (0 for success, -1 for failure)
 */
int textToSpeech(String txt) {
  Serial.println("Converting text to speech...");
  return tts.message(txt);
}

/**
 * Start recording audio from microphone
 */
void startRecording() {
  size_t sampleRate = I2S.rxSampleRate();
  uint16_t sampleWidth = (uint16_t)I2S.rxDataWidth();
  uint16_t numChannels = (uint16_t)I2S.rxSlotMode();
  
  // Allocate initial buffer for WAV header
  wavBufferSize = 0;
  wavBuffer = (uint8_t *)malloc(PCM_WAV_HEADER_SIZE);
  if (wavBuffer == NULL) {
    Serial.println("ERROR: Failed to allocate WAV buffer!");
    return;
  }
  
  // Write WAV header to buffer
  const pcm_wav_header_t wavHeader = PCM_WAV_HEADER_DEFAULT(0, sampleWidth, sampleRate, numChannels);
  memcpy(wavBuffer, &wavHeader, PCM_WAV_HEADER_SIZE);
  wavBufferSize = PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n>>> Recording started - Speak now!");
  Serial.println(">>> Release button when finished");
  digitalWrite(LED_PIN, LOW); // Turn off LED during recording
  isRecording = true;
}

/**
 * Stop recording and process the audio
 * This triggers the full pipeline:
 * 1. Speech to text (Whisper)
 * 2. Image capture and analysis (GPT-4 Vision)
 * 3. Text to speech response (TTS)
 */
void stopRecording() {
  if (!isRecording) return;
  
  isRecording = false;
  digitalWrite(LED_PIN, HIGH); // Turn LED back on
  Serial.printf("\n>>> Recording stopped. Total size: %u bytes\n", wavBufferSize);
  
  // Update WAV header with actual audio data size
  pcm_wav_header_t *header = (pcm_wav_header_t *)wavBuffer;
  header->descriptor_chunk.chunk_size = (wavBufferSize) + sizeof(pcm_wav_header_t) - 8;
  header->data_chunk.subchunk_size = wavBufferSize - PCM_WAV_HEADER_SIZE;
  
  // ===== STEP 1: Convert speech to text =====
  Serial.println("\n[STEP 1/3] Converting speech to text...");
  String instruction = speechToText();
  
  if (instruction == NULL || instruction.length() == 0) {
    Serial.println("ERROR: Speech to text conversion failed!");
    return;
  }
  
  Serial.println("----------------------------------------");
  Serial.println("Your instruction:");
  Serial.println(instruction);
  Serial.println("----------------------------------------\n");
  
  // ===== STEP 2: Capture image and get AI analysis =====
  Serial.println("[STEP 2/3] Analyzing image with AI...");
  String aiResponse = imageAnalysis(instruction);
  
  if (aiResponse == NULL || aiResponse.length() == 0) {
    Serial.println("ERROR: Image analysis failed!");
    return;
  }
  
  Serial.println("----------------------------------------");
  Serial.println("AI Response:");
  Serial.println(aiResponse);
  Serial.println("----------------------------------------\n");
  
  // ===== STEP 3: Convert AI response to speech =====
  Serial.println("[STEP 3/3] Converting response to speech...");
  if (textToSpeech(aiResponse) == -1) {
    Serial.println("ERROR: Text-to-speech conversion failed!");
  } else {
    Serial.println("✓ Response played successfully!");
  }
  
  Serial.println("\n========================================");
  Serial.println("Ready for next command!");
  Serial.println("========================================\n");
}
