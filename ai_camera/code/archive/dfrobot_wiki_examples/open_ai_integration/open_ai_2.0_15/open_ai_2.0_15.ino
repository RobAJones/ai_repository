#include "time.h"
/*
 * ESP32-S3 AI CAM - OpenAI Interface v2.0.15
 * 
 * Reorganized interface for better image/audio pairing workflow
 * - Audio playback bar at top right
 * - Save/Delete for new captures (before adding to resource lists)
 * - Return Test/Audio quadrants moved up for better visibility
 * - Image/Audio file lists at bottom as resource pools
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
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n" \
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n" \
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n" \
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n" \
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n" \
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n" \
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n" \
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n" \
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n" \
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n" \
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n" \
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n" \
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n" \
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n" \
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n" \
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n" \
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n" \
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n" \
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n" \
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n" \
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n" \
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n" \
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n" \
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n" \
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n" \
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n" \
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n" \
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

String base64EncodeFileOptimized(String filePath) {
  File file = SD_MMC.open(filePath, FILE_READ);
  if (!file) return "";
  size_t fileSize = file.size();
  size_t outputLen = 0;
  mbedtls_base64_encode(nullptr, 0, &outputLen, nullptr, fileSize);
  uint8_t* output = (uint8_t*)ps_malloc(outputLen + 1);
  if (!output) output = (uint8_t*)malloc(outputLen + 1);
  if (!output) { file.close(); return ""; }
  String result = "";
  result.reserve(outputLen + 100);
  size_t totalRead = 0;
  uint8_t buffer[3000];
  uint8_t temp[4096];
  while (file.available()) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    if (bytesRead > 0) {
      size_t outLen = 0;
      if (mbedtls_base64_encode(temp, sizeof(temp), &outLen, buffer, bytesRead) == 0) {
        result += String((char*)temp).substring(0, outLen);
      }
      totalRead += bytesRead;
      if (totalRead % 30000 == 0) yield();
    }
  }
  file.close();
  free(output);
  return result;
}

String getHttpErrorString(int code) {
  switch(code) {
    case HTTPC_ERROR_CONNECTION_REFUSED: return "Connection refused";
    case HTTPC_ERROR_SEND_HEADER_FAILED: return "Send header failed";
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED: return "Send payload failed";
    case HTTPC_ERROR_NOT_CONNECTED: return "Not connected";
    case HTTPC_ERROR_CONNECTION_LOST: return "Connection lost";
    case HTTPC_ERROR_NO_STREAM: return "No stream";
    case HTTPC_ERROR_NO_HTTP_SERVER: return "No HTTP server";
    case HTTPC_ERROR_TOO_LESS_RAM: return "Not enough RAM";
    case HTTPC_ERROR_ENCODING: return "Encoding error";
    case HTTPC_ERROR_STREAM_WRITE: return "Stream write error";
    case HTTPC_ERROR_READ_TIMEOUT: return "Read timeout";
    default: return "Unknown error (" + String(code) + ")";
  }
}

String transcribeAudioWithWhisper(String audioFilePath) {
  Serial.println("\n=== OpenAI Whisper API ===");
  
  File audioFile = SD_MMC.open(audioFilePath, FILE_READ);
  if (!audioFile) {
    Serial.println("ERROR: Failed to open audio file");
    return "Error: File not found";
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
  Serial.println("Memory allocated for audio data");
  
  audioFile.read(audioData, fileSize);
  audioFile.close();
  Serial.println("Audio file loaded into memory");
  
  WiFiClientSecure client;
  Serial.println("Setting up secure connection (INSECURE MODE)...");
  client.setInsecure(); // Skip certificate validation
  client.setTimeout(30);
  
  HTTPClient https;
  if (!https.begin(client, OPENAI_WHISPER_URL)) {
    free(audioData);
    Serial.println("ERROR: HTTPS begin failed");
    return "Error: Connection setup failed";
  }
  Serial.println("HTTPS connection initialized");
  
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
    String errorResponse = https.getString();
    Serial.println("Server error response: " + errorResponse);
    transcription = "Error: Whisper API returned " + String(httpCode);
  } else {
    transcription = "Error: " + getHttpErrorString(httpCode);
  }
  
  free(audioData);
  free(requestBody);
  https.end();
  
  Serial.printf("Free heap after: %d bytes\n", ESP.getFreeHeap());
  
  return transcription;
}

String analyzeImageWithGPT4Vision(String imageFilePath, String audioTranscription) {
  Serial.println("\n=== GPT-4 Vision ===");
  String imageBase64 = base64EncodeFileOptimized(imageFilePath);
  if (imageBase64.length() == 0) return "Error: Base64 encoding failed";
  WiFiClientSecure client;
  client.setInsecure();
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
  client.setInsecure();
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
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    delay(1000);
    ESP.restart();
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>ESP32-S3 OpenAI + TTS</title><meta name="viewport" content="width=device-width, initial-scale=1">
<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: #f5f5f5;
  padding: 20px;
}

.main-grid {
  display: grid;
  grid-template-columns: 1.5fr 1fr;
  grid-template-rows: auto auto auto auto;
  gap: 20px;
  max-width: 1400px;
  margin: 0 auto;
}

/* Left column - Video section spans rows 1-4 */
.video-section {
  grid-column: 1;
  grid-row: 1 / 5;
  background: white;
  border-radius: 12px;
  border: 2px solid #ddd;
  padding: 20px;
}

/* Right column top - Audio playback */
.audio-section {
  grid-column: 2;
  grid-row: 1;
  background: white;
  border-radius: 12px;
  border: 2px solid #ddd;
  padding: 20px;
}

/* Right column second - Capture controls */
.capture-controls {
  grid-column: 2;
  grid-row: 2;
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 15px;
}

/* Right column third - Return sections */
.return-section {
  grid-column: 2;
  grid-row: 3;
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 15px;
}

/* Right column fourth - File lists */
.files-section {
  grid-column: 2;
  grid-row: 4;
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 15px;
}

/* Bottom - Message section */
.message-section {
  grid-column: 1 / 3;
  grid-row: 5;
  background: white;
  border-radius: 12px;
  border: 2px solid #ddd;
  padding: 20px;
}

.video-controls {
  display: flex;
  gap: 10px;
  margin-bottom: 15px;
  flex-wrap: wrap;
}

.video-display {
  background: #000;
  border-radius: 8px;
  position: relative;
  padding-bottom: 75%;
  overflow: hidden;
}

.video-display img {
  position: absolute;
  width: 100%;
  height: 100%;
  object-fit: contain;
}

.audio-bar {
  background: #f0f0f0;
  border-radius: 8px;
  padding: 15px;
  margin-bottom: 15px;
  text-align: center;
  font-weight: bold;
  color: #333;
}

.recording-timer {
  font-size: 32px;
  color: #e74c3c;
  font-family: monospace;
  margin: 15px 0;
  text-align: center;
}

.processing-msg {
  text-align: center;
  color: #667eea;
  font-size: 16px;
  margin: 15px 0;
  font-weight: bold;
}

.capture-column, .file-column, .return-column {
  background: white;
  border-radius: 12px;
  border: 2px solid #ddd;
  padding: 15px;
}

.capture-column h3, .file-column h3, .return-column h3 {
  margin-bottom: 10px;
  color: #333;
  font-size: 14px;
  border-bottom: 2px solid #667eea;
  padding-bottom: 8px;
}

.capture-status {
  background: #f8f8f8;
  padding: 12px;
  margin-bottom: 10px;
  border-radius: 6px;
  font-size: 13px;
  min-height: 60px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #666;
}

.capture-status.active {
  background: #e8f5e9;
  color: #2e7d32;
  font-weight: bold;
}

.file-list, .return-list {
  max-height: 150px;
  overflow-y: auto;
  margin-bottom: 10px;
}

.file-item {
  background: #f8f8f8;
  padding: 8px;
  margin-bottom: 5px;
  border-radius: 6px;
  cursor: pointer;
  border: 2px solid transparent;
  font-size: 12px;
}

.file-item:hover {
  background: #e8e8e8;
}

.file-item.selected {
  border-color: #667eea;
  background: #e8eeff;
}

.message-display {
  background: #f9f9f9;
  border-radius: 8px;
  padding: 15px;
  min-height: 150px;
  max-height: 300px;
  overflow-y: auto;
  margin-bottom: 15px;
  font-size: 14px;
  line-height: 1.6;
  border: 1px solid #ddd;
  white-space: pre-wrap;
}

.message-controls {
  display: flex;
  gap: 10px;
}

button {
  padding: 10px 20px;
  font-size: 14px;
  font-weight: 600;
  border: none;
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.2s;
  color: white;
}

button:hover:not(:disabled) {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(0,0,0,0.15);
}

button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-stream { background: #3498db; }
.btn-capture { background: #667eea; }
.btn-review { background: #9b59b6; }
.btn-record { background: #e74c3c; }
.btn-playback { background: #2ecc71; }
.btn-delete { background: #e67e22; }
.btn-save { background: #16a085; }
.btn-send { background: #2c3e50; flex: 1; }
.btn-replay { background: #34495e; flex: 1; }

.btn-record.recording {
  animation: pulse 1.5s infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.6; }
}

.status-bar {
  position: fixed;
  top: 20px;
  right: 20px;
  background: #2ecc71;
  color: white;
  padding: 12px 20px;
  border-radius: 8px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.2);
  display: none;
  z-index: 1000;
  max-width: 300px;
}

.status-bar.show {
  display: block;
}

.status-bar.error {
  background: #e74c3c;
}

.info-banner {
  background: #fff3cd;
  padding: 12px;
  border-radius: 8px;
  margin-bottom: 15px;
  border-left: 4px solid #ffc107;
  font-size: 13px;
}

@media (max-width: 1024px) {
  .main-grid {
    grid-template-columns: 1fr;
  }
  
  .video-section, .audio-section, .capture-controls, 
  .return-section, .files-section, .message-section {
    grid-column: 1;
  }
  
  .video-section {
    grid-row: auto;
  }
}
</style>
</head>
<body>
<div id="statusBar" class="status-bar"></div>

<div class="main-grid">
  <!-- Left: Video Section -->
  <div class="video-section">
    <div class="info-banner">
      <strong>Pairing Workflow:</strong> Capture image/audio → Save to export → Send to OpenAI agent
    </div>
    <div class="video-controls">
      <button class="btn-stream" onclick="toggleStream()">Video Stream</button>
      <button class="btn-capture" onclick="captureImage()">Capture</button>
      <button class="btn-review" onclick="reviewLastImage()">Review</button>
    </div>
    <div class="video-display">
      <img id="videoStream" src="">
    </div>
  </div>

  <!-- Right Row 1: Audio Playback -->
  <div class="audio-section">
    <div class="audio-bar">Audio Playback Bar</div>
    <audio id="audioPlayer" controls style="width:100%; margin-bottom:15px; display:none"></audio>
    <div id="recordingTimer" class="recording-timer" style="display:none">00:00</div>
    <div id="processingMsg" class="processing-msg" style="display:none">Recording audio, please wait...</div>
    <div class="video-controls">
      <button id="recordBtn" class="btn-record" onclick="toggleRecording()">Record</button>
      <button id="playbackBtn" class="btn-playback" onclick="playbackAudio()" disabled>Playback</button>
      <button class="btn-delete" onclick="deleteLastCapture()">Delete</button>
    </div>
  </div>

  <!-- Right Row 2: Capture Save/Delete -->
  <div class="capture-controls">
    <div class="capture-column">
      <h3>Latest Image</h3>
      <div class="capture-status" id="imageStatus">No image captured</div>
      <div class="video-controls" style="margin-bottom:0">
        <button class="btn-save" onclick="saveImageToExport()" style="flex:1" id="imageSaveBtn" disabled>Save</button>
        <button class="btn-delete" onclick="deleteLatestImage()" style="flex:1" id="imageDeleteBtn" disabled>Delete</button>
      </div>
    </div>
    <div class="capture-column">
      <h3>Latest Audio</h3>
      <div class="capture-status" id="audioStatus">No audio recorded</div>
      <div class="video-controls" style="margin-bottom:0">
        <button class="btn-save" onclick="saveAudioToExport()" style="flex:1" id="audioSaveBtn" disabled>Save</button>
        <button class="btn-delete" onclick="deleteLatestAudio()" style="flex:1" id="audioDeleteBtn" disabled>Delete</button>
      </div>
    </div>
  </div>

  <!-- Right Row 3: Return sections for export pairing -->
  <div class="return-section">
    <div class="return-column">
      <h3>Image for Export</h3>
      <div class="file-item" id="exportedImageDisplay" style="min-height:40px; display:flex; align-items:center; justify-content:center; color:#999">None selected</div>
    </div>
    <div class="return-column">
      <h3>Audio for Export</h3>
      <div class="file-item" id="exportedAudioDisplay" style="min-height:40px; display:flex; align-items:center; justify-content:center; color:#999">None selected</div>
    </div>
  </div>

  <!-- Right Row 4: Resource file lists -->
  <div class="files-section">
    <div class="file-column">
      <h3>Image Files (Resources)</h3>
      <div class="file-list" id="imageFileList"></div>
    </div>
    <div class="file-column">
      <h3>Audio Files (Resources)</h3>
      <div class="file-list" id="audioFileList"></div>
    </div>
  </div>

  <!-- Bottom: Message Section -->
  <div class="message-section">
    <h3 style="margin-bottom:15px; color:#333">OpenAI Analysis Results</h3>
    <div class="message-display" id="messageDisplay">OpenAI analysis with TTS will appear here...</div>
    <div class="message-controls">
      <button class="btn-send" onclick="sendToOpenAI()">Send to OpenAI</button>
      <button class="btn-replay" onclick="replayTTSAudio()">Replay TTS</button>
      <button class="btn-save" onclick="saveCompleteAnalysis()">Save Analysis</button>
    </div>
  </div>
</div>

<script>
let recordingStartTime = 0;
let timerInterval = null;
let streamActive = false;
let streamInterval = null;
let latestImageFile = "";
let latestAudioFile = "";
let selectedImageForExport = "";
let selectedAudioForExport = "";
let lastAnalysisResponse = "";
let lastTTSAudioFile = "";
let lastAnalysisID = "";
let isRecording = false;

function showStatus(message, isError = false) {
  const statusBar = document.getElementById('statusBar');
  statusBar.textContent = message;
  statusBar.className = 'status-bar show' + (isError ? ' error' : '');
  setTimeout(() => statusBar.classList.remove('show'), 3000);
}

function toggleStream() {
  const videoStream = document.getElementById('videoStream');
  streamActive = !streamActive;
  
  if (streamActive) {
    videoStream.src = '/stream?' + Date.now();
    startStreamRefresh();
    showStatus('Stream started');
  } else {
    stopStreamRefresh();
    videoStream.src = '';
    showStatus('Stream stopped');
  }
}

function startStreamRefresh() {
  if (streamInterval) clearInterval(streamInterval);
  streamInterval = setInterval(() => {
    if (streamActive && !isRecording) {
      document.getElementById('videoStream').src = '/stream?' + Date.now();
    }
  }, 150);
}

function stopStreamRefresh() {
  if (streamInterval) {
    clearInterval(streamInterval);
    streamInterval = null;
  }
}

function captureImage() {
  fetch('/capture')
    .then(res => res.json())
    .then(data => {
      if (data.success) {
        latestImageFile = data.filename;
        document.getElementById('imageStatus').textContent = data.filename;
        document.getElementById('imageStatus').classList.add('active');
        document.getElementById('imageSaveBtn').disabled = false;
        document.getElementById('imageDeleteBtn').disabled = false;
        showStatus('Image captured: ' + data.filename);
        loadImageList();
      } else {
        showStatus('Capture failed', true);
      }
    })
    .catch(err => showStatus('Capture error', true));
}

function reviewLastImage() {
  fetch('/image/latest')
    .then(res => res.json())
    .then(data => {
      if (data.success && data.filename) {
        stopStreamRefresh();
        streamActive = false;
        document.getElementById('videoStream').src = '/image?file=' + encodeURIComponent(data.filename);
        showStatus('Reviewing: ' + data.filename);
      } else {
        showStatus('No images to review', true);
      }
    })
    .catch(err => showStatus('Review failed', true));
}

function saveImageToExport() {
  if (!latestImageFile) {
    showStatus('No image to export', true);
    return;
  }
  selectedImageForExport = latestImageFile;
  document.getElementById('exportedImageDisplay').textContent = latestImageFile;
  document.getElementById('exportedImageDisplay').style.color = '#333';
  showStatus('Image ready for export: ' + latestImageFile);
}

function saveAudioToExport() {
  if (!latestAudioFile) {
    showStatus('No audio to export', true);
    return;
  }
  selectedAudioForExport = latestAudioFile;
  document.getElementById('exportedAudioDisplay').textContent = latestAudioFile;
  document.getElementById('exportedAudioDisplay').style.color = '#333';
  showStatus('Audio ready for export: ' + latestAudioFile);
}

function deleteLatestImage() {
  if (!latestImageFile) return;
  
  if (confirm('Delete ' + latestImageFile + '?')) {
    fetch('/delete/image?file=' + encodeURIComponent(latestImageFile), { method: 'DELETE' })
      .then(() => {
        showStatus('Deleted: ' + latestImageFile);
        latestImageFile = "";
        document.getElementById('imageStatus').textContent = 'No image captured';
        document.getElementById('imageStatus').classList.remove('active');
        document.getElementById('imageSaveBtn').disabled = true;
        document.getElementById('imageDeleteBtn').disabled = true;
        loadImageList();
      });
  }
}

function deleteLatestAudio() {
  if (!latestAudioFile) return;
  
  if (confirm('Delete ' + latestAudioFile + '?')) {
    fetch('/delete/audio?file=' + encodeURIComponent(latestAudioFile), { method: 'DELETE' })
      .then(() => {
        showStatus('Deleted: ' + latestAudioFile);
        latestAudioFile = "";
        document.getElementById('audioStatus').textContent = 'No audio recorded';
        document.getElementById('audioStatus').classList.remove('active');
        document.getElementById('audioSaveBtn').disabled = true;
        document.getElementById('audioDeleteBtn').disabled = true;
        loadAudioList();
      });
  }
}

function loadImageList() {
  fetch('/list/images')
    .then(res => res.json())
    .then(data => {
      const list = document.getElementById('imageFileList');
      list.innerHTML = '';
      
      if (data.files.length === 0) {
        list.innerHTML = '<div style="text-align:center; color:#999; padding:10px">No images</div>';
        return;
      }
      
      data.files.forEach(file => {
        const item = document.createElement('div');
        item.className = 'file-item';
        item.textContent = file;
        item.onclick = () => selectImageFromList(file);
        list.appendChild(item);
      });
    });
}

function loadAudioList() {
  fetch('/list/audio')
    .then(res => res.json())
    .then(data => {
      const list = document.getElementById('audioFileList');
      list.innerHTML = '';
      
      if (data.files.length === 0) {
        list.innerHTML = '<div style="text-align:center; color:#999; padding:10px">No audio</div>';
        return;
      }
      
      data.files.forEach(file => {
        const item = document.createElement('div');
        item.className = 'file-item';
        item.textContent = file;
        item.onclick = () => selectAudioFromList(file);
        list.appendChild(item);
      });
    });
}

function selectImageFromList(filename) {
  selectedImageForExport = filename;
  document.getElementById('exportedImageDisplay').textContent = filename;
  document.getElementById('exportedImageDisplay').style.color = '#333';
  
  // Highlight in list
  document.querySelectorAll('#imageFileList .file-item').forEach(item => {
    item.classList.toggle('selected', item.textContent === filename);
  });
  
  showStatus('Selected for export: ' + filename);
}

function selectAudioFromList(filename) {
  selectedAudioForExport = filename;
  document.getElementById('exportedAudioDisplay').textContent = filename;
  document.getElementById('exportedAudioDisplay').style.color = '#333';
  
  // Highlight in list
  document.querySelectorAll('#audioFileList .file-item').forEach(item => {
    item.classList.toggle('selected', item.textContent === filename);
  });
  
  const audioPlayer = document.getElementById('audioPlayer');
  audioPlayer.src = '/audio?file=' + encodeURIComponent(filename);
  audioPlayer.style.display = 'block';
  document.getElementById('playbackBtn').disabled = false;
  
  showStatus('Selected for export: ' + filename);
}

function updateTimer() {
  const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
  const minutes = Math.floor(elapsed / 60);
  const seconds = elapsed % 60;
  document.getElementById('recordingTimer').textContent = 
    String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
}

function toggleRecording() {
  const recordBtn = document.getElementById('recordBtn');
  
  if (isRecording) {
    clearInterval(timerInterval);
    const duration = Math.floor((Date.now() - recordingStartTime) / 1000);
    
    stopStreamRefresh();
    recordBtn.textContent = 'Processing...';
    recordBtn.disabled = true;
    recordBtn.classList.remove('recording');
    document.getElementById('recordingTimer').style.display = 'none';
    document.getElementById('processingMsg').style.display = 'block';
    
    showStatus('Recording ' + duration + ' seconds...');
    
    const timeout = (duration + 10) * 1000;
    Promise.race([
      fetch('/audio/record?duration=' + duration, { signal: AbortSignal.timeout(timeout) }),
      new Promise((_, reject) => setTimeout(() => reject(new Error('timeout')), timeout))
    ])
      .then(res => res.json())
      .then(data => {
        if (data.success) {
          latestAudioFile = data.filename;
          document.getElementById('audioStatus').textContent = data.filename;
          document.getElementById('audioStatus').classList.add('active');
          document.getElementById('audioSaveBtn').disabled = false;
          document.getElementById('audioDeleteBtn').disabled = false;
          showStatus('Audio saved: ' + data.filename);
          loadAudioList();
        } else {
          showStatus('Recording failed', true);
        }
      })
      .catch(err => {
        showStatus('Recording timeout - audio may still be saving', true);
        setTimeout(loadAudioList, 2000);
      })
      .finally(() => {
        recordBtn.textContent = 'Record';
        recordBtn.disabled = false;
        document.getElementById('processingMsg').style.display = 'none';
        isRecording = false;
        if (streamActive) startStreamRefresh();
      });
  } else {
    isRecording = true;
    recordingStartTime = Date.now();
    recordBtn.classList.add('recording');
    recordBtn.textContent = 'Stop';
    document.getElementById('recordingTimer').style.display = 'block';
    timerInterval = setInterval(updateTimer, 100);
    showStatus('Recording started');
  }
}

function playbackAudio() {
  if (!selectedAudioForExport) {
    showStatus('No audio selected', true);
    return;
  }
  
  showStatus('Playing on speaker...');
  fetch('/audio/speaker?file=' + encodeURIComponent(selectedAudioForExport), { signal: AbortSignal.timeout(60000) })
    .then(res => res.json())
    .then(data => {
      if (data.success) {
        showStatus('Playback complete');
      } else {
        showStatus('Playback failed', true);
      }
    })
    .catch(err => showStatus('Playback timeout', true));
}

function deleteLastCapture() {
  if (latestAudioFile) {
    deleteLatestAudio();
  } else if (latestImageFile) {
    deleteLatestImage();
  } else {
    showStatus('Nothing to delete', true);
  }
}

function sendToOpenAI() {
  if (!selectedImageForExport || !selectedAudioForExport) {
    showStatus('Select image and audio for export first', true);
    return;
  }
  
  lastAnalysisID = Date.now().toString();
  stopStreamRefresh();
  
  showStatus('Sending to OpenAI (60-90 sec) - Check serial monitor');
  document.getElementById('messageDisplay').textContent = 
    'Processing with OpenAI...\n\n' +
    'Step 1: Transcribing audio...\n' +
    'Step 2: Analyzing image...\n' +
    'Step 3: Generating TTS...\n\n' +
    'Check serial monitor for detailed progress.';
  
  fetch('/openai/analyze', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      image: selectedImageForExport,
      audio: selectedAudioForExport,
      id: lastAnalysisID
    }),
    signal: AbortSignal.timeout(150000)
  })
    .then(res => res.json())
    .then(data => {
      if (data.success) {
        lastAnalysisResponse = data.response;
        lastTTSAudioFile = data.ttsFile;
        
        const displayText = data.response.replace(/\\n/g, '\n');
        document.getElementById('messageDisplay').textContent = displayText;
        showStatus('Analysis complete!');
        
        if (streamActive) startStreamRefresh();
      } else {
        showStatus('OpenAI request failed', true);
        document.getElementById('messageDisplay').textContent = 'Error: ' + (data.error || 'Unknown error');
        if (streamActive) startStreamRefresh();
      }
    })
    .catch(err => {
      showStatus('Timeout - check serial monitor', true);
      document.getElementById('messageDisplay').textContent = 'Timeout - check serial monitor for details';
      if (streamActive) startStreamRefresh();
    });
}

function replayTTSAudio() {
  if (!lastTTSAudioFile || lastTTSAudioFile === 'none') {
    showStatus('No TTS audio available', true);
    return;
  }
  
  const audioPlayer = document.getElementById('audioPlayer');
  audioPlayer.src = '/tts?file=' + encodeURIComponent(lastTTSAudioFile);
  audioPlayer.style.display = 'block';
  audioPlayer.currentTime = 0;
  audioPlayer.play();
  showStatus('Playing TTS response');
}

function saveCompleteAnalysis() {
  if (!lastAnalysisResponse || !lastTTSAudioFile || !selectedAudioForExport) {
    showStatus('Complete analysis first', true);
    return;
  }
  
  showStatus('Preparing download...');
  fetch('/analysis/download', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      text: lastAnalysisResponse,
      image: selectedImageForExport,
      audioQuestion: selectedAudioForExport,
      audioTTS: lastTTSAudioFile,
      id: lastAnalysisID
    })
  })
    .then(res => res.json())
    .then(data => {
      if (data.success) {
        window.location.href = '/analysis/package?id=' + lastAnalysisID;
        showStatus('Download started');
      } else {
        showStatus('Save failed', true);
      }
    })
    .catch(err => showStatus('Save error', true));
}

// Initialize
loadImageList();
loadAudioList();
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  
  SD_MMC.mkdir("/images");
  SD_MMC.mkdir("/audio");
  SD_MMC.mkdir("/tts");
  
  setupCamera();
  
  i2s_mic.setPinsPdmRx(PDM_CLOCK_PIN, PDM_DATA_PIN);
  if (!i2s_mic.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("PDM init failed!");
  }
  
  i2s_spk.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if (!i2s_spk.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("Speaker init failed!");
  }
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP: " + WiFi.localIP().toString());
  
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", index_html); });
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/image", HTTP_GET, handleImageView);
  server.on("/image/latest", HTTP_GET, handleLatestImage);
  server.on("/audio/record", HTTP_GET, handleAudioRecord);
  server.on("/audio", HTTP_GET, handleAudioStream);
  server.on("/audio/speaker", HTTP_GET, handleSpeakerPlayback);
  server.on("/tts", HTTP_GET, handleTTSStream);
  server.on("/list/images", HTTP_GET, handleListImages);
  server.on("/list/audio", HTTP_GET, handleListAudio);
  server.on("/delete/image", HTTP_DELETE, handleDeleteImage);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  server.on("/openai/analyze", HTTP_POST, handleOpenAIAnalyze);
  server.on("/analysis/download", HTTP_POST, handleAnalysisDownload);
  server.on("/analysis/package", HTTP_GET, handleAnalysisPackage);
  
  server.begin();
  Serial.println("Interface v2.0.15 ready - Optimized pairing workflow");
  String apiKeyStatus = (String(OPENAI_API_KEY) == "sk-YOUR-OPENAI-API-KEY-HERE") ? "NOT SET!" : "Configured";
  Serial.println("API Key status: " + apiKeyStatus);
}

void handleStream() {
  if (recordingInProgress || ttsInProgress) { server.send(503); return; }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { server.send(503); return; }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { server.send(500, "application/json", "{\"success\":false}"); return; }
  imageCount++;
  String filename = "IMG_" + String(imageCount) + ".jpg";
  File file = SD_MMC.open("/images/" + filename, FILE_WRITE);
  if (file) {
    file.write(fb->buf, fb->len);
    file.close();
    server.send(200, "application/json", "{\"success\":true,\"filename\":\"" + filename + "\"}");
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
  esp_camera_fb_return(fb);
}

void handleImageView() {
  String filename = server.arg("file");
  File file = SD_MMC.open("/images/" + filename, FILE_READ);
  if (file) {
    server.streamFile(file, "image/jpeg");
    file.close();
  } else {
    server.send(404);
  }
}

void handleLatestImage() {
  File root = SD_MMC.open("/images");
  String latest = "";
  while (File file = root.openNextFile()) {
    if (!file.isDirectory()) latest = String(file.name());
  }
  if (latest.length() > 0) {
    server.send(200, "application/json", "{\"success\":true,\"filename\":\"" + latest + "\"}");
  } else {
    server.send(200, "application/json", "{\"success\":false}");
  }
}

void handleAudioRecord() {
  if (recordingInProgress || ttsInProgress) {
    server.send(503, "application/json", "{\"success\":false,\"error\":\"busy\"}");
    return;
  }
  
  recordingInProgress = true;
  int duration = server.arg("duration").toInt();
  if (duration <= 0) duration = 5;
  
  int totalSamples = SAMPLE_RATE * duration;
  int16_t* audioBuffer = (int16_t*)ps_malloc(totalSamples * sizeof(int16_t));
  if (!audioBuffer) audioBuffer = (int16_t*)malloc(totalSamples * sizeof(int16_t));
  if (!audioBuffer) {
    recordingInProgress = false;
    server.send(500, "application/json", "{\"success\":false,\"error\":\"memory\"}");
    return;
  }
  
  size_t samplesRead = 0;
  while (samplesRead < totalSamples) {
    size_t bytesRead = i2s_mic.readBytes((char*)(audioBuffer + samplesRead), 
                                         (totalSamples - samplesRead) * sizeof(int16_t));
    samplesRead += bytesRead / sizeof(int16_t);
    if (samplesRead % 16000 == 0) yield();
  }
  
  for (int i = 0; i < totalSamples; i++) {
    int32_t amplified = (int32_t)audioBuffer[i] * AMPLIFICATION;
    audioBuffer[i] = constrain(amplified, -32768, 32767);
  }
  
  audioCount++;
  String filename = "REC_" + String(audioCount) + ".wav";
  File file = SD_MMC.open("/audio/" + filename, FILE_WRITE);
  
  if (file) {
    uint32_t fileSize = 44 + (totalSamples * 2);
    file.write((uint8_t*)"RIFF", 4);
    uint32_t chunkSize = fileSize - 8;
    file.write((uint8_t*)&chunkSize, 4);
    file.write((uint8_t*)"WAVE", 4);
    file.write((uint8_t*)"fmt ", 4);
    uint32_t subchunk1Size = 16;
    file.write((uint8_t*)&subchunk1Size, 4);
    uint16_t audioFormat = 1;
    file.write((uint8_t*)&audioFormat, 2);
    uint16_t numChannels = 1;
    file.write((uint8_t*)&numChannels, 2);
    uint32_t sampleRate = SAMPLE_RATE;
    file.write((uint8_t*)&sampleRate, 4);
    uint32_t byteRate = SAMPLE_RATE * 2;
    file.write((uint8_t*)&byteRate, 4);
    uint16_t blockAlign = 2;
    file.write((uint8_t*)&blockAlign, 2);
    uint16_t bitsPerSample = 16;
    file.write((uint8_t*)&bitsPerSample, 2);
    file.write((uint8_t*)"data", 4);
    uint32_t dataSize = totalSamples * 2;
    file.write((uint8_t*)&dataSize, 4);
    file.write((uint8_t*)audioBuffer, totalSamples * 2);
    file.close();
    
    free(audioBuffer);
    recordingInProgress = false;
    server.send(200, "application/json", "{\"success\":true,\"filename\":\"" + filename + "\"}");
  } else {
    free(audioBuffer);
    recordingInProgress = false;
    server.send(500, "application/json", "{\"success\":false}");
  }
}

void handleAudioStream() {
  String filename = server.arg("file");
  File file = SD_MMC.open("/audio/" + filename, FILE_READ);
  if (file) {
    server.streamFile(file, "audio/wav");
    file.close();
  } else {
    server.send(404);
  }
}

void handleSpeakerPlayback() {
  String filename = server.arg("file");
  File file = SD_MMC.open("/audio/" + filename, FILE_READ);
  if (!file) {
    server.send(404, "application/json", "{\"success\":false}");
    return;
  }
  
  file.seek(44);
  uint8_t buffer[512];
  while (file.available()) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    if (bytesRead > 0) {
      i2s_spk.write(buffer, bytesRead);
    }
    yield();
  }
  file.close();
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleTTSStream() {
  String filename = server.arg("file");
  File file = SD_MMC.open("/tts/" + filename, FILE_READ);
  if (file) {
    server.streamFile(file, "audio/mpeg");
    file.close();
  } else {
    server.send(404);
  }
}

void handleListImages() {
  File root = SD_MMC.open("/images");
  String json = "{\"files\":[";
  bool first = true;
  while (File file = root.openNextFile()) {
    if (!file.isDirectory()) {
      if (!first) json += ",";
      json += "\"" + String(file.name()) + "\"";
      first = false;
    }
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleListAudio() {
  File root = SD_MMC.open("/audio");
  String json = "{\"files\":[";
  bool first = true;
  while (File file = root.openNextFile()) {
    if (!file.isDirectory()) {
      if (!first) json += ",";
      json += "\"" + String(file.name()) + "\"";
      first = false;
    }
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleDeleteImage() {
  String filename = server.arg("file");
  if (SD_MMC.remove("/images/" + filename)) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
}

void handleDeleteAudio() {
  String filename = server.arg("file");
  if (SD_MMC.remove("/audio/" + filename)) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
}

void handleOpenAIAnalyze() {
  String body = server.arg("plain");
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, body);
  
  String imageFile = doc["image"].as<String>();
  String audioFile = doc["audio"].as<String>();
  String analysisID = doc["id"].as<String>();
  
  String transcription = transcribeAudioWithWhisper("/audio/" + audioFile);
  String analysis = analyzeImageWithGPT4Vision("/images/" + imageFile, transcription);
  String ttsFile = generateTTSAudio(analysis, analysisID);
  
  DynamicJsonDocument response(4096);
  response["success"] = true;
  response["response"] = analysis;
  response["ttsFile"] = ttsFile.length() > 0 ? ttsFile : "none";
  
  String responseStr;
  serializeJson(response, responseStr);
  server.send(200, "application/json", responseStr);
}

void handleAnalysisDownload() {
  String body = server.arg("plain");
  DynamicJsonDocument doc(4096);
  deserializeJson(doc, body);
  
  String id = doc["id"].as<String>();
  String analysisText = doc["text"].as<String>();
  
  String summaryFilename = "ANALYSIS_" + id + ".txt";
  File summaryFile = SD_MMC.open("/" + summaryFilename, FILE_WRITE);
  if (summaryFile) {
    summaryFile.println("OpenAI Analysis Results");
    summaryFile.println("======================");
    summaryFile.println(analysisText);
    summaryFile.close();
  }
  
  server.send(200, "application/json", "{\"success\":true,\"id\":\"" + id + "\"}");
}

void handleAnalysisPackage() {
  String id = server.arg("id");
  String filename = "ANALYSIS_" + id + ".txt";
  File file = SD_MMC.open("/" + filename, FILE_READ);
  if (file) {
    server.streamFile(file, "text/plain");
    file.close();
  } else {
    server.send(404);
  }
}

void loop() {
  server.handleClient();
  yield();
}
