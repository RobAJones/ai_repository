/*
 * Minimal OpenAI Whisper Test
 * Tests ONLY the Whisper API connection
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* OPENAI_API_KEY = "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";  // REPLACE THIS

const char* OPENAI_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n===========================================");
  Serial.println("MINIMAL OPENAI WHISPER CONNECTION TEST");
  Serial.println("===========================================\n");
  
  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✓ WiFi Connected");
  Serial.println("✓ IP: " + WiFi.localIP().toString());
  Serial.printf("✓ Free heap: %d bytes\n\n", ESP.getFreeHeap());
  
  // Test 1: Simple HTTPS GET to verify SSL works
  Serial.println("TEST 1: Testing HTTPS connection to api.openai.com...");
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  
  if (client.connect("api.openai.com", 443)) {
    Serial.println("✓ SSL connection successful!");
    client.stop();
  } else {
    Serial.println("✗ SSL connection FAILED!");
    Serial.println("ERROR: Cannot establish secure connection to OpenAI");
    while(1) delay(1000);
  }
  
  // Test 2: Try a simple API call
  Serial.println("\nTEST 2: Testing Whisper API endpoint...");
  Serial.println("Creating minimal audio data...");
  
  // Create a tiny test WAV (44 bytes header + 160 bytes of silence)
  uint8_t testWav[204];
  
  // WAV header for 16kHz mono 16-bit (100ms of silence)
  memcpy(testWav, "RIFF", 4);
  *(uint32_t*)(testWav + 4) = 196; // file size - 8
  memcpy(testWav + 8, "WAVE", 4);
  memcpy(testWav + 12, "fmt ", 4);
  *(uint32_t*)(testWav + 16) = 16; // fmt chunk size
  *(uint16_t*)(testWav + 20) = 1;  // PCM
  *(uint16_t*)(testWav + 22) = 1;  // mono
  *(uint32_t*)(testWav + 24) = 16000; // sample rate
  *(uint32_t*)(testWav + 28) = 32000; // byte rate
  *(uint16_t*)(testWav + 32) = 2;  // block align
  *(uint16_t*)(testWav + 34) = 16; // bits per sample
  memcpy(testWav + 36, "data", 4);
  *(uint32_t*)(testWav + 40) = 160; // data size
  memset(testWav + 44, 0, 160); // silence
  
  Serial.println("✓ Test WAV created (204 bytes)");
  
  // Build multipart request
  String boundary = "----ESP32Test" + String(millis());
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"test.wav\"\r\n";
  bodyStart += "Content-Type: audio/wav\r\n\r\n";
  
  String bodyEnd = "\r\n--" + boundary + "\r\n";
  bodyEnd += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
  bodyEnd += "whisper-1\r\n";
  bodyEnd += "--" + boundary + "--\r\n";
  
  size_t totalSize = bodyStart.length() + 204 + bodyEnd.length();
  
  Serial.printf("Request size: %d bytes\n", totalSize);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  
  // Allocate request body
  uint8_t* requestBody = (uint8_t*)malloc(totalSize);
  if (!requestBody) {
    Serial.println("✗ Memory allocation FAILED!");
    while(1) delay(1000);
  }
  Serial.println("✓ Memory allocated");
  
  // Build request
  size_t offset = 0;
  memcpy(requestBody + offset, bodyStart.c_str(), bodyStart.length());
  offset += bodyStart.length();
  memcpy(requestBody + offset, testWav, 204);
  offset += 204;
  memcpy(requestBody + offset, bodyEnd.c_str(), bodyEnd.length());
  
  Serial.println("✓ Request body assembled");
  
  // Send request
  Serial.println("\nSending POST request to Whisper API...");
  Serial.println("(This may take 10-30 seconds)\n");
  
  HTTPClient https;
  WiFiClientSecure secureClient;
  secureClient.setCACert(OPENAI_ROOT_CA);
  
  if (!https.begin(secureClient, "https://api.openai.com/v1/audio/transcriptions")) {
    Serial.println("✗ HTTPS begin FAILED!");
    free(requestBody);
    while(1) delay(1000);
  }
  
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  https.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  https.setTimeout(30000);
  
  Serial.println("Headers set, sending POST...");
  int httpCode = https.POST(requestBody, totalSize);
  
  Serial.println("\n===========================================");
  Serial.printf("HTTP Response Code: %d\n", httpCode);
  
  if (httpCode > 0) {
    String response = https.getString();
    Serial.println("\nResponse:");
    Serial.println(response);
    
    if (httpCode == 200) {
      Serial.println("\n✓✓✓ SUCCESS! Whisper API is working! ✓✓✓");
    } else {
      Serial.println("\n✗ API returned error code");
      if (httpCode == 401) Serial.println("Check your API key!");
      if (httpCode == 400) Serial.println("Bad request format");
    }
  } else {
    Serial.println("\n✗ Connection error:");
    if (httpCode == -1) Serial.println("  Connection failed");
    else if (httpCode == -11) Serial.println("  Timeout");
    else Serial.printf("  Error code: %d\n", httpCode);
  }
  
  Serial.println("===========================================\n");
  
  free(requestBody);
  https.end();
  
  Serial.println("Test complete. System will remain idle.");
}

void loop() {
  delay(1000);
}
