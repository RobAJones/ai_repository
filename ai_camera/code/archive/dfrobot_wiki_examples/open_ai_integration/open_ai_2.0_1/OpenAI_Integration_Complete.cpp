/*
 * OpenAI Integration Module for ESP32-S3
 * 
 * This module provides complete OpenAI API integration including:
 * 1. Whisper API for audio transcription
 * 2. GPT-4 Vision for image analysis
 * 3. Combined analysis of image + audio
 * 
 * Add these includes to your main sketch:
 */

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <base64.h>

// Configuration
const char* OPENAI_API_KEY = "sk-YOUR-API-KEY-HERE";
const char* OPENAI_WHISPER_URL = "https://api.openai.com/v1/audio/transcriptions";
const char* OPENAI_CHAT_URL = "https://api.openai.com/v1/chat/completions";

// Root certificate for api.openai.com (valid as of 2025)
// You can get this by visiting https://api.openai.com in a browser
const char* OPENAI_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

// Helper function: Convert file to base64
String fileToBase64(String filepath) {
  File file = SD_MMC.open(filepath, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file: " + filepath);
    return "";
  }
  
  size_t fileSize = file.size();
  uint8_t* buffer = (uint8_t*)malloc(fileSize);
  if (!buffer) {
    Serial.println("Failed to allocate memory for base64 conversion");
    file.close();
    return "";
  }
  
  file.read(buffer, fileSize);
  file.close();
  
  // Calculate base64 size (4/3 of original + padding)
  size_t base64Length = ((fileSize + 2) / 3) * 4;
  char* base64Buffer = (char*)malloc(base64Length + 1);
  
  if (!base64Buffer) {
    free(buffer);
    Serial.println("Failed to allocate memory for base64 string");
    return "";
  }
  
  // Encode to base64
  base64_encode(base64Buffer, (char*)buffer, fileSize);
  base64Buffer[base64Length] = '\0';
  
  String result = String(base64Buffer);
  
  free(buffer);
  free(base64Buffer);
  
  return result;
}

// Function 1: Transcribe audio using Whisper API
String transcribeAudioWithWhisper(String audioFilePath) {
  Serial.println("Transcribing audio with Whisper API...");
  
  // Read WAV file from SD card
  File audioFile = SD_MMC.open(audioFilePath, FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open audio file");
    return "Error: Could not open audio file";
  }
  
  size_t fileSize = audioFile.size();
  uint8_t* audioData = (uint8_t*)malloc(fileSize);
  audioFile.read(audioData, fileSize);
  audioFile.close();
  
  // Setup HTTPS client
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  
  HTTPClient https;
  https.begin(client, OPENAI_WHISPER_URL);
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  
  // Create multipart form data
  String boundary = "----ESP32Boundary" + String(millis());
  https.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  
  // Build multipart body
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  bodyStart += "Content-Type: audio/wav\r\n\r\n";
  
  String bodyEnd = "\r\n--" + boundary + "\r\n";
  bodyEnd += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
  bodyEnd += "whisper-1\r\n";
  bodyEnd += "--" + boundary + "--\r\n";
  
  // Calculate total size
  size_t totalSize = bodyStart.length() + fileSize + bodyEnd.length();
  
  // Allocate buffer for complete request
  uint8_t* requestBody = (uint8_t*)malloc(totalSize);
  if (!requestBody) {
    free(audioData);
    Serial.println("Memory allocation failed");
    return "Error: Memory allocation failed";
  }
  
  // Assemble request body
  size_t offset = 0;
  memcpy(requestBody + offset, bodyStart.c_str(), bodyStart.length());
  offset += bodyStart.length();
  memcpy(requestBody + offset, audioData, fileSize);
  offset += fileSize;
  memcpy(requestBody + offset, bodyEnd.c_str(), bodyEnd.length());
  
  // Send POST request
  https.addHeader("Content-Length", String(totalSize));
  int httpCode = https.POST(requestBody, totalSize);
  
  String transcription = "";
  
  if (httpCode == 200) {
    String response = https.getString();
    Serial.println("Whisper API Response: " + response);
    
    // Parse JSON response
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      transcription = doc["text"].as<String>();
      Serial.println("Transcription: " + transcription);
    } else {
      Serial.println("JSON parsing failed");
      transcription = "Error: Could not parse transcription";
    }
  } else {
    Serial.printf("Whisper API failed: %d\n", httpCode);
    String errorResponse = https.getString();
    Serial.println("Error: " + errorResponse);
    transcription = "Error: Whisper API returned " + String(httpCode);
  }
  
  // Cleanup
  free(audioData);
  free(requestBody);
  https.end();
  
  return transcription;
}

// Function 2: Analyze image with GPT-4 Vision
String analyzeImageWithGPT4Vision(String imageFilePath, String audioTranscription) {
  Serial.println("Analyzing image with GPT-4 Vision...");
  
  // Convert image to base64
  String imageBase64 = fileToBase64(imageFilePath);
  if (imageBase64.length() == 0) {
    return "Error: Could not encode image";
  }
  
  // Setup HTTPS client
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  
  HTTPClient https;
  https.begin(client, OPENAI_CHAT_URL);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  https.setTimeout(30000); // 30 second timeout
  
  // Build JSON request
  DynamicJsonDocument doc(32000); // Large doc for base64 image
  
  doc["model"] = "gpt-4o"; // Use GPT-4 with vision
  doc["max_tokens"] = 1000;
  
  JsonArray messages = doc.createNestedArray("messages");
  JsonObject systemMsg = messages.createNestedObject();
  systemMsg["role"] = "system";
  systemMsg["content"] = "You are an AI assistant analyzing images and audio. Provide concise, insightful analysis.";
  
  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  
  JsonArray content = userMsg.createNestedArray("content");
  
  // Add text with audio transcription
  JsonObject textPart = content.createNestedObject();
  textPart["type"] = "text";
  String promptText = "Please analyze this image. ";
  if (audioTranscription.length() > 0 && !audioTranscription.startsWith("Error")) {
    promptText += "Additionally, here is an audio transcription related to the image: \"" + 
                  audioTranscription + "\". Please provide insights considering both the image and the audio.";
  } else {
    promptText += "Describe what you see and any notable features.";
  }
  textPart["text"] = promptText;
  
  // Add image
  JsonObject imagePart = content.createNestedObject();
  imagePart["type"] = "image_url";
  JsonObject imageUrl = imagePart.createNestedObject("image_url");
  imageUrl["url"] = "data:image/jpeg;base64," + imageBase64;
  
  // Serialize to string
  String requestBody;
  serializeJson(doc, requestBody);
  
  Serial.printf("Request body size: %d bytes\n", requestBody.length());
  
  // Send POST request
  int httpCode = https.POST(requestBody);
  
  String analysis = "";
  
  if (httpCode == 200) {
    String response = https.getString();
    
    // Parse response
    DynamicJsonDocument responseDoc(4096);
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      analysis = responseDoc["choices"][0]["message"]["content"].as<String>();
      Serial.println("Analysis received");
    } else {
      Serial.println("JSON parsing failed");
      analysis = "Error: Could not parse analysis response";
    }
  } else {
    Serial.printf("GPT-4 Vision API failed: %d\n", httpCode);
    String errorResponse = https.getString();
    Serial.println("Error response: " + errorResponse);
    analysis = "Error: GPT-4 Vision returned " + String(httpCode);
  }
  
  https.end();
  
  return analysis;
}

// Main handler for OpenAI analysis endpoint
void handleOpenAIAnalyze() {
  Serial.println("\n=== OpenAI Analysis Request ===");
  
  // Parse incoming JSON
  String requestBody = server.arg("plain");
  DynamicJsonDocument reqDoc(512);
  DeserializationError error = deserializeJson(reqDoc, requestBody);
  
  if (error) {
    server.send(400, "application/json", 
      "{\"success\":false,\"error\":\"Invalid JSON\"}");
    return;
  }
  
  String imageFile = reqDoc["image"].as<String>();
  String audioFile = reqDoc["audio"].as<String>();
  
  Serial.println("Image: " + imageFile);
  Serial.println("Audio: " + audioFile);
  
  // Check if files exist
  String imagePath = "/images/" + imageFile;
  String audioPath = "/audio/" + audioFile;
  
  if (!SD_MMC.exists(imagePath)) {
    server.send(404, "application/json", 
      "{\"success\":false,\"error\":\"Image file not found\"}");
    return;
  }
  
  if (!SD_MMC.exists(audioPath)) {
    server.send(404, "application/json", 
      "{\"success\":false,\"error\":\"Audio file not found\"}");
    return;
  }
  
  // Step 1: Transcribe audio
  Serial.println("\nStep 1: Transcribing audio...");
  String transcription = transcribeAudioWithWhisper(audioPath);
  
  if (transcription.startsWith("Error")) {
    server.send(500, "application/json", 
      "{\"success\":false,\"error\":\"" + transcription + "\"}");
    return;
  }
  
  // Step 2: Analyze image with transcription context
  Serial.println("\nStep 2: Analyzing image...");
  String analysis = analyzeImageWithGPT4Vision(imagePath, transcription);
  
  if (analysis.startsWith("Error")) {
    server.send(500, "application/json", 
      "{\"success\":false,\"error\":\"" + analysis + "\"}");
    return;
  }
  
  // Step 3: Combine results
  String combinedResult = "TRANSCRIPTION:\n" + transcription + "\n\n";
  combinedResult += "ANALYSIS:\n" + analysis;
  
  // Escape quotes for JSON
  combinedResult.replace("\"", "\\\"");
  combinedResult.replace("\n", "\\n");
  
  // Send success response
  String jsonResponse = "{\"success\":true,\"response\":\"" + combinedResult + "\"}";
  server.send(200, "application/json", jsonResponse);
  
  Serial.println("\n=== Analysis Complete ===\n");
}

// Usage in setup():
/*
void setup() {
  // ... existing setup code ...
  
  // Add OpenAI endpoint
  server.on("/openai/analyze", HTTP_POST, handleOpenAIAnalyze);
  
  // ... rest of setup ...
}
*/

// Memory optimization tips:
// 1. Process files in chunks if memory is limited
// 2. Use DynamicJsonDocument with appropriate sizing
// 3. Free memory immediately after use
// 4. Consider using PSRAM for large allocations

// Error handling improvements:
// 1. Check WiFi connection before API calls
// 2. Implement retry logic for failed requests
// 3. Add timeout handling
// 4. Log all errors to SD card for debugging

// Performance considerations:
// 1. Cache base64 encoded images if reusing
// 2. Implement request queuing for multiple analyses
// 3. Use connection pooling if possible
// 4. Monitor heap usage with esp_get_free_heap_size()
