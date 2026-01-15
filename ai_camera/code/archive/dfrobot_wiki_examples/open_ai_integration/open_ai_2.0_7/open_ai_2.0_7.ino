/*
 * ESP32-S3 AI CAM - OpenAI with Enhanced Diagnostics
 * 
 * Added detailed error reporting to diagnose API connection issues
 */

#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "ESP_I2S.h"
#include "mbedtls/base64.h"

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";

// IMPORTANT: Replace with your actual OpenAI API key
const char* OPENAI_API_KEY = "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";

const char* OPENAI_WHISPER_URL = "https://api.openai.com/v1/audio/transcriptions";
const char* OPENAI_CHAT_URL = "https://api.openai.com/v1/chat/completions";
const char* OPENAI_TTS_URL = "https://api.openai.com/v1/audio/speech";

const char* OPENAI_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

WebServer server(80);

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 5
#define SIOD_GPIO_NUM 8
#define SIOC_GPIO_NUM 9
#define Y9_GPIO_NUM 4
#define Y8_GPIO_NUM 6
#define Y7_GPIO_NUM 7
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 17
#define Y4_GPIO_NUM 21
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 16
#define VSYNC_GPIO_NUM 1
#define HREF_GPIO_NUM 2
#define PCLK_GPIO_NUM 15
#define SD_MMC_CMD 11
#define SD_MMC_CLK 12
#define SD_MMC_D0 13
#define PDM_DATA_PIN GPIO_NUM_39
#define PDM_CLOCK_PIN GPIO_NUM_38
#define I2S_BCLK 45
#define I2S_LRC 46
#define I2S_DOUT 42
#define I2S_SD_PIN 41
#define SAMPLE_RATE 16000
#define AMPLIFICATION 10

int imageCount = 0;
int audioCount = 0;
int ttsCount = 0;
bool streamingEnabled = true;
volatile bool recordingInProgress = false;
volatile bool ttsInProgress = false;

I2SClass i2s_mic;
I2SClass i2s_spk;

// Helper function to get HTTP error description
String getHttpErrorString(int code) {
  switch(code) {
    case -1: return "Connection failed / Network error";
    case -2: return "Invalid response";
    case -3: return "Header send error";
    case -4: return "Not connected";
    case -5: return "Connection lost";
    case -6: return "No stream";
    case -7: return "No HTTP server";
    case -8: return "Too less RAM";
    case -9: return "Encoding error";
    case -10: return "Stream write error";
    case -11: return "Timeout";
    default: return "HTTP error " + String(code);
  }
}

// [Previous helper functions - keeping concise]
void amplifyAudio(uint8_t* buffer, size_t size, int factor) {
  int16_t* samples = (int16_t*)buffer;
  size_t sampleCount = size / 2;
  for (size_t i = 0; i < sampleCount; i++) {
    int32_t amplified = (int32_t)samples[i] * factor;
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;
    samples[i] = (int16_t)amplified;
  }
}

String base64EncodeFileOptimized(String filepath) {
  File file = SD_MMC.open(filepath, FILE_READ);
  if (!file) return "";
  size_t fileSize = file.size();
  uint8_t* inputBuffer = (uint8_t*)ps_malloc(fileSize);
  if (!inputBuffer) inputBuffer = (uint8_t*)malloc(fileSize);
  if (!inputBuffer) { file.close(); return ""; }
  file.read(inputBuffer, fileSize);
  file.close();
  size_t base64Size = 4 * ((fileSize + 2) / 3);
  char* base64Output = (char*)ps_malloc(base64Size + 10);
  if (!base64Output) base64Output = (char*)malloc(base64Size + 10);
  if (!base64Output) { free(inputBuffer); return ""; }
  size_t olen;
  mbedtls_base64_encode((unsigned char*)base64Output, base64Size + 10, &olen, inputBuffer, fileSize);
  free(inputBuffer);
  base64Output[olen] = '\0';
  String result = String(base64Output);
  free(base64Output);
  return result;
}

// ENHANCED: Whisper with detailed diagnostics
String transcribeAudioWithWhisper(String audioFilePath) {
  Serial.println("\n=== Whisper API (Enhanced Diagnostics) ===");
  Serial.printf("Free heap before: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("WiFi status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "DISCONNECTED!");
  
  // Check API key
  if (String(OPENAI_API_KEY) == "sk-YOUR-OPENAI-API-KEY-HERE") {
    Serial.println("ERROR: OpenAI API key not configured!");
    return "Error: API key not set";
  }
  Serial.println("API key configured: " + String(OPENAI_API_KEY).substring(0, 10) + "...");
  
  File audioFile = SD_MMC.open(audioFilePath, FILE_READ);
  if (!audioFile) {
    Serial.println("ERROR: Failed to open audio file");
    return "Error: Could not open audio file";
  }
  
  size_t fileSize = audioFile.size();
  Serial.printf("Audio file size: %d bytes\n", fileSize);
  
  uint8_t* audioData = (uint8_t*)ps_malloc(fileSize);
  if (!audioData) audioData = (uint8_t*)malloc(fileSize);
  if (!audioData) { 
    audioFile.close(); 
    Serial.println("ERROR: Memory allocation failed");
    return "Error: Memory allocation failed"; 
  }
  Serial.println("Memory allocated successfully");
  
  audioFile.read(audioData, fileSize);
  audioFile.close();
  
  Serial.println("Creating HTTPS client...");
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  client.setTimeout(30);
  
  HTTPClient https;
  
  Serial.println("Connecting to OpenAI...");
  if (!https.begin(client, OPENAI_WHISPER_URL)) {
    Serial.println("ERROR: Failed to begin HTTPS connection");
    free(audioData);
    return "Error: HTTPS begin failed";
  }
  
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  https.setTimeout(30000);
  
  String boundary = "----ESP32" + String(millis());
  https.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  
  String bodyStart = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String bodyEnd = "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n--" + boundary + "--\r\n";
  
  size_t totalSize = bodyStart.length() + fileSize + bodyEnd.length();
  Serial.printf("Request body size: %d bytes\n", totalSize);
  
  uint8_t* requestBody = (uint8_t*)ps_malloc(totalSize);
  if (!requestBody) requestBody = (uint8_t*)malloc(totalSize);
  if (!requestBody) { 
    free(audioData); 
    https.end();
    Serial.println("ERROR: Request body allocation failed");
    return "Error: Memory allocation failed"; 
  }
  Serial.println("Request body allocated");
  
  size_t offset = 0;
  memcpy(requestBody + offset, bodyStart.c_str(), bodyStart.length()); 
  offset += bodyStart.length();
  memcpy(requestBody + offset, audioData, fileSize); 
  offset += fileSize;
  memcpy(requestBody + offset, bodyEnd.c_str(), bodyEnd.length());
  
  Serial.println("Sending POST request to Whisper API...");
  Serial.printf("Free heap before POST: %d bytes\n", ESP.getFreeHeap());
  
  int httpCode = https.POST(requestBody, totalSize);
  
  Serial.printf("HTTP Response Code: %d\n", httpCode);
  if (httpCode < 0) {
    Serial.println("ERROR: " + getHttpErrorString(httpCode));
  }
  
  String transcription = "";
  
  if (httpCode == 200) {
    String response = https.getString();
    Serial.println("Response received, length: " + String(response.length()));
    Serial.println("Response preview: " + response.substring(0, min(200, (int)response.length())));
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      transcription = doc["text"].as<String>();
      Serial.println("SUCCESS: " + transcription);
    } else {
      Serial.println("ERROR: JSON parsing failed - " + String(error.c_str()));
      transcription = "Error: JSON parse failed";
    }
  } else if (httpCode > 0) {
    // HTTP error from server
    String errorResponse = https.getString();
    Serial.println("Server error response: " + errorResponse);
    transcription = "Error: Whisper API returned " + String(httpCode);
  } else {
    // Connection error
    transcription = "Error: " + getHttpErrorString(httpCode);
  }
  
  free(audioData);
  free(requestBody);
  https.end();
  
  Serial.printf("Free heap after: %d bytes\n", ESP.getFreeHeap());
  
  return transcription;
}

// [Rest of the code same as before - keeping file concise]
// Including: analyzeImageWithGPT4Vision, generateTTSAudio, setupCamera, all handlers

String analyzeImageWithGPT4Vision(String imageFilePath, String audioTranscription) {
  Serial.println("\n=== GPT-4 Vision ===");
  String imageBase64 = base64EncodeFileOptimized(imageFilePath);
  if (imageBase64.length() == 0) return "Error: Base64 encoding failed";
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  HTTPClient https;
  https.begin(client, OPENAI_CHAT_URL);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  https.setTimeout(60000);
  size_t docSize = imageBase64.length() + 4096;
  DynamicJsonDocument* doc = new DynamicJsonDocument(docSize);
  if (!doc) return "Error: JSON allocation failed";
  (*doc)["model"] = "gpt-4o";
  (*doc)["max_tokens"] = 500;
  JsonArray messages = doc->createNestedArray("messages");
  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  JsonArray content = userMsg.createNestedArray("content");
  JsonObject textPart = content.createNestedObject();
  textPart["type"] = "text";
  String prompt = "Analyze this image concisely. ";
  if (audioTranscription.length() > 0 && !audioTranscription.startsWith("Error")) {
    prompt += "Audio: \"" + audioTranscription + "\". Consider both.";
  }
  textPart["text"] = prompt;
  JsonObject imagePart = content.createNestedObject();
  imagePart["type"] = "image_url";
  JsonObject imageUrl = imagePart.createNestedObject("image_url");
  imageUrl["url"] = "data:image/jpeg;base64," + imageBase64;
  String requestBody;
  serializeJson(*doc, requestBody);
  delete doc;
  int httpCode = https.POST(requestBody);
  requestBody = "";
  String analysis = "";
  if (httpCode == 200) {
    String response = https.getString();
    DynamicJsonDocument responseDoc(4096);
    if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
      analysis = responseDoc["choices"][0]["message"]["content"].as<String>();
      Serial.println("SUCCESS");
    } else analysis = "Error: JSON parse failed";
  } else analysis = "Error: GPT-4 returned " + String(httpCode);
  https.end();
  return analysis;
}

String generateTTSAudio(String text, String analysisID) {
  Serial.println("\n=== OpenAI TTS (Non-blocking) ===");
  ttsInProgress = true;
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  client.setTimeout(60);
  HTTPClient https;
  https.begin(client, OPENAI_TTS_URL);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  https.setTimeout(60000);
  https.setReuse(false);
  DynamicJsonDocument doc(4096);
  doc["model"] = "tts-1";
  doc["input"] = text;
  doc["voice"] = "alloy";
  doc["response_format"] = "mp3";
  String requestBody;
  serializeJson(doc, requestBody);
  int httpCode = https.POST(requestBody);
  String ttsFilename = "";
  if (httpCode == 200) {
    WiFiClient* stream = https.getStreamPtr();
    ttsCount++;
    ttsFilename = "TTS_" + analysisID + ".mp3";
    File ttsFile = SD_MMC.open("/tts/" + ttsFilename, FILE_WRITE);
    if (!ttsFile) { https.end(); ttsInProgress = false; return ""; }
    uint8_t buffer[512];
    size_t totalBytes = 0;
    unsigned long lastYield = millis();
    unsigned long startTime = millis();
    while (https.connected() && (stream->available() > 0 || (millis() - startTime < 30000))) {
      size_t size = stream->available();
      if (size > 0) {
        int len = stream->readBytes(buffer, min(size, sizeof(buffer)));
        if (len > 0) {
          ttsFile.write(buffer, len);
          totalBytes += len;
          if (millis() - lastYield > 100) { yield(); lastYield = millis(); }
          if (totalBytes % 10240 == 0) Serial.printf("TTS: %dKB\n", totalBytes / 1024);
        }
      } else { yield(); delay(10); }
      if (millis() - startTime > 60000) break;
    }
    ttsFile.close();
    Serial.printf("TTS saved: %d bytes\n", totalBytes);
  }
  https.end();
  ttsInProgress = false;
  return ttsFilename;
}

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;
  esp_camera_init(&config);
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
}

// [Minimal HTML for testing - full version same as before]
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>ESP32 OpenAI Diagnostic</title></head>
<body><h1>OpenAI Diagnostic Mode</h1><p>Check serial monitor for detailed diagnostics.</p></body></html>
)rawliteral";

void handleRoot() { server.send_P(200, "text/html", index_html); }
void handleStream() {
  if (recordingInProgress || ttsInProgress) { server.send(503, "text/plain", "Busy"); return; }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { server.send(503, "text/plain", "Unavailable"); return; }
  WiFiClient client = server.client();
  client.printf("HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}
void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { server.send(500, "application/json", "{\"success\":false}"); return; }
  imageCount++;
  File file = SD_MMC.open("/images/IMG_" + String(imageCount) + ".jpg", FILE_WRITE);
  if (file) { file.write(fb->buf, fb->len); file.close();
    server.send(200, "application/json", "{\"success\":true,\"filename\":\"IMG_" + String(imageCount) + ".jpg\"}"); }
  else server.send(500, "application/json", "{\"success\":false}");
  esp_camera_fb_return(fb);
}
void handleImageLatest() {
  imageCount == 0 ? server.send(404, "application/json", "{\"success\":false}") :
    server.send(200, "application/json", "{\"success\":true,\"filename\":\"IMG_" + String(imageCount) + ".jpg\"}");
}
void handleImage() {
  File file = SD_MMC.open("/images/" + server.arg("file"), FILE_READ);
  if (!file) { server.send(404, "text/plain", "Not found"); return; }
  server.streamFile(file, "image/jpeg");
  file.close();
}
void handleListImages() {
  File dir = SD_MMC.open("/images");
  String json = "{\"files\":[";
  bool first = true;
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.startsWith("/images/")) name = name.substring(8);
      if (!first) json += ",";
      json += "\"" + name + "\"";
      first = false;
    }
    file = dir.openNextFile();
  }
  json += "]}";
  server.send(200, "application/json", json);
}
void handleDeleteImage() {
  SD_MMC.remove("/images/" + server.arg("file")) ?
    server.send(200, "application/json", "{\"success\":true}") :
    server.send(500, "application/json", "{\"success\":false}");
}
void handleAudioRecord() {
  int duration = server.arg("duration").toInt();
  if (duration < 1 || duration > 60) { server.send(400, "application/json", "{\"success\":false}"); return; }
  recordingInProgress = true;
  i2s_mic.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  i2s_mic.setPinsPdmRx(PDM_CLOCK_PIN, PDM_DATA_PIN);
  size_t wav_size = 0;
  uint8_t* wav_buffer = i2s_mic.recordWAV(duration, &wav_size);
  i2s_mic.end();
  recordingInProgress = false;
  if (wav_buffer && wav_size > 44) {
    amplifyAudio(wav_buffer + 44, wav_size - 44, AMPLIFICATION);
    audioCount++;
    File file = SD_MMC.open("/audio/REC_" + String(audioCount) + ".wav", FILE_WRITE);
    if (file) { file.write(wav_buffer, wav_size); file.close();
      server.send(200, "application/json", "{\"success\":true,\"filename\":\"REC_" + String(audioCount) + ".wav\"}"); }
    else server.send(500, "application/json", "{\"success\":false}");
  } else server.send(500, "application/json", "{\"success\":false}");
  if (wav_buffer) free(wav_buffer);
}
void handleAudioStream() {
  File file = SD_MMC.open("/audio/" + server.arg("file"), FILE_READ);
  if (!file) { server.send(404, "text/plain", "Not found"); return; }
  server.streamFile(file, "audio/wav");
  file.close();
}
void handleListAudio() {
  File dir = SD_MMC.open("/audio");
  String json = "{\"files\":[";
  bool first = true;
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.startsWith("/audio/")) name = name.substring(7);
      if (!first) json += ",";
      json += "\"" + name + "\"";
      first = false;
    }
    file = dir.openNextFile();
  }
  json += "]}";
  server.send(200, "application/json", json);
}
void handleDeleteAudio() {
  SD_MMC.remove("/audio/" + server.arg("file")) ?
    server.send(200, "application/json", "{\"success\":true}") :
    server.send(500, "application/json", "{\"success\":false}");
}
void handleSpeakerPlayback() {
  File file = SD_MMC.open("/audio/" + server.arg("file"), FILE_READ);
  if (!file) { server.send(404, "application/json", "{\"success\":false}"); return; }
  uint8_t header[44];
  file.read(header, 44);
  size_t dataSize = file.size() - 44;
  uint8_t* audioData = (uint8_t*)malloc(dataSize);
  file.read(audioData, dataSize);
  file.close();
  i2s_spk.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  i2s_spk.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  pinMode(I2S_SD_PIN, OUTPUT);
  digitalWrite(I2S_SD_PIN, HIGH);
  i2s_spk.write(audioData, dataSize);
  digitalWrite(I2S_SD_PIN, LOW);
  i2s_spk.end();
  free(audioData);
  server.send(200, "application/json", "{\"success\":true}");
}
void handleTTSStream() {
  String filename = server.arg("file");
  File file = SD_MMC.open("/tts/" + filename, FILE_READ);
  if (!file) { server.send(404, "text/plain", "Not found"); return; }
  server.streamFile(file, "audio/mpeg");
  file.close();
}

void handleOpenAIAnalyze() {
  Serial.println("\n========================================");
  Serial.println("OpenAI Analysis Request");
  Serial.println("========================================");
  
  DynamicJsonDocument reqDoc(512);
  deserializeJson(reqDoc, server.arg("plain"));
  
  String imageFile = reqDoc["image"].as<String>();
  String audioFile = reqDoc["audio"].as<String>();
  String analysisID = reqDoc["id"].as<String>();
  
  String imagePath = "/images/" + imageFile;
  String audioPath = "/audio/" + audioFile;
  
  if (!SD_MMC.exists(imagePath) || !SD_MMC.exists(audioPath)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"File not found\"}");
    return;
  }
  
  String transcription = transcribeAudioWithWhisper(audioPath);
  if (transcription.startsWith("Error")) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"" + transcription + "\"}");
    return;
  }
  
  String analysis = analyzeImageWithGPT4Vision(imagePath, transcription);
  if (analysis.startsWith("Error")) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"" + analysis + "\"}");
    return;
  }
  
  String combinedText = "Audio Transcription: " + transcription + ". Image Analysis: " + analysis;
  String ttsFile = generateTTSAudio(combinedText, analysisID);
  
  if (ttsFile.length() == 0) ttsFile = "none";
  
  String result = "=== AUDIO TRANSCRIPTION ===\\n" + transcription + "\\n\\n=== IMAGE ANALYSIS ===\\n" + analysis;
  result.replace("\"", "\\\"");
  result.replace("\n", "\\n");
  
  String jsonResponse = "{\"success\":true,\"response\":\"" + result + "\",\"ttsFile\":\"" + ttsFile + "\"}";
  server.send(200, "application/json", jsonResponse);
}

void handleAnalysisDownload() {
  DynamicJsonDocument doc(512);
  deserializeJson(doc, server.arg("plain"));
  String analysisID = doc["id"].as<String>();
  String text = doc["text"].as<String>();
  String packageDir = "/packages/pkg_" + analysisID;
  SD_MMC.mkdir("/packages");
  SD_MMC.mkdir(packageDir.c_str());
  File txtFile = SD_MMC.open(packageDir + "/analysis.txt", FILE_WRITE);
  if (txtFile) { txtFile.println(text); txtFile.close(); }
  server.send(200, "application/json", "{\"success\":true}");
}

void handleAnalysisPackage() {
  String id = server.arg("id");
  File txtFile = SD_MMC.open("/packages/pkg_" + id + "/analysis.txt", FILE_READ);
  if (!txtFile) { server.send(404, "text/plain", "Not found"); return; }
  server.streamFile(txtFile, "text/plain");
  txtFile.close();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("ESP32-S3 - OpenAI DIAGNOSTIC MODE");
  Serial.println("========================================\n");
  
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true)) { Serial.println("ERROR: SD failed"); return; }
  Serial.println("✓ SD Card OK");
  
  SD_MMC.mkdir("/images");
  SD_MMC.mkdir("/audio");
  SD_MMC.mkdir("/tts");
  SD_MMC.mkdir("/packages");
  
  setupCamera();
  Serial.println("✓ Camera OK");
  
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { 
    delay(500); 
    Serial.print("."); 
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi OK");
    Serial.println("✓ IP: " + WiFi.localIP().toString());
    Serial.printf("✓ Signal strength: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\nERROR: WiFi connection failed!");
    return;
  }
  
  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/capture", handleCapture);
  server.on("/image/latest", handleImageLatest);
  server.on("/image", handleImage);
  server.on("/list/images", handleListImages);
  server.on("/delete/image", HTTP_DELETE, handleDeleteImage);
  server.on("/audio/record", handleAudioRecord);
  server.on("/audio", handleAudioStream);
  server.on("/list/audio", handleListAudio);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  server.on("/audio/speaker", handleSpeakerPlayback);
  server.on("/tts", handleTTSStream);
  server.on("/openai/analyze", HTTP_POST, handleOpenAIAnalyze);
  server.on("/analysis/download", HTTP_POST, handleAnalysisDownload);
  server.on("/analysis/package", handleAnalysisPackage);
  server.begin();
  
  Serial.println("✓ Server started");
  Serial.printf("✓ Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("✓ PSRAM: %d bytes\n", ESP.getPsramSize());
  Serial.println("\n========================================");
  Serial.println("DIAGNOSTIC MODE READY");
  Serial.println("API Key status: " + String(OPENAI_API_KEY == String("sk-YOUR-OPENAI-API-KEY-HERE") ? "NOT SET!" : "Configured"));
  Serial.println("========================================\n");
}

void loop() {
  server.handleClient();
  yield();
}
