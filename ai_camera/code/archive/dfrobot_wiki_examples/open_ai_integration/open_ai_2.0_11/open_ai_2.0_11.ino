/*
 * Enhanced OpenAI Connection Test
 * Fixes SSL issues with NTP time sync and tries multiple approaches
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "time.h"

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* OPENAI_API_KEY = "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";  // REPLACE THIS

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;  // EST (adjust for your timezone)
const int daylightOffset_sec = 3600;

// Let's Encrypt Root CA (used by OpenAI)
const char* LETSENCRYPT_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFFjCCAv6gAwIBAgIRAJErCErPDBinU/bWLiWnX1owDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjAwOTA0MDAwMDAw\n" \
"WhcNMjUwOTE1MTYwMDAwWjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n" \
"RW5jcnlwdDELMAkGA1UEAxMCUjMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n" \
"AoIBAQC7AhUozPaglNMPEuyNVZLD+ILxmaZ6QoinXSaqtSu5xUyxr45r+XXIo9cP\n" \
"R5QUVTVXjJ6oojkZ9YI8QqlObvU7wy7bjcCwXPNZOOftz2nwWgsbvsCUJCWH+jdx\n" \
"sxPnHKzhm+/b5DtFUkWWqcFTzjTIUu61ru2P3mBw4qVUq7ZtDpelQDRrK9O8Zutm\n" \
"NHz6a4uPVymZ+DAXXbpyb/uBxa3Shlg9F8fnCbvxK/eG3MHacV3URuPMrSXBiLxg\n" \
"Z3Vms/EY96Jc5lP/Ooi2R6X/ExjqmAl3P51T+c8B5fWmcBcUr2Ok/5mzk53cU6cG\n" \
"/kiFHaFpriV1uxPMUgP17VGhi9sVAgMBAAGjggEIMIIBBDAOBgNVHQ8BAf8EBAMC\n" \
"AYYwHQYDVR0lBBYwFAYIKwYBBQUHAwIGCCsGAQUFBwMBMBIGA1UdEwEB/wQIMAYB\n" \
"Af8CAQAwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYfr52LFMLGMB8GA1UdIwQYMBaA\n" \
"FHm0WeZ7tuXkAHOACIjIGlj26ZtuMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcw\n" \
"AoYWaHR0cDovL3gxLmkubGVuY3Iub3JnLzAnBgNVHR8EIDAeMBygGqAYhhZodHRw\n" \
"Oi8veDEuYy5sZW5jci5vcmcvMCIGA1UdIAQbMBkwCAYGZ4EMAQIBMA0GCysGAQQB\n" \
"gt8TAQEBMA0GCSqGSIb3DQEBCwUAA4ICAQCFyk5HPqP3hUSFvNVneLKYY611TR6W\n" \
"PTNlclQtgaDqw+34IL9fzLdwALduO/ZelN7kIJ+m74uyA+eitRY8kc607TkC53wl\n" \
"ikfmZW4/RvTZ8M6UK+5UzhK8jCdLuMGYL6KvzXGRSgi3yLgjewQtCPkIVz6D2QQz\n" \
"CkcheAmCJ8MqyJu5zlzyZMjAvnnAT45tRAxekrsu94sQ4egdRCnbWSDtY7kh+BIm\n" \
"lJNXoB1lBMEKIq4QDUOXoRgffuDghje1WrG9ML+Hbisq/yFOGwXD9RiX8F6sw6W4\n" \
"avAuvDszue5L3sz85K+EC4Y/wFVDNvZo4TYXao6Z0f+lQKc0t8DQYzk1OXVu8rp2\n" \
"yJMC6alLbBfODALZvYH7n7do1AZls4I9d1P4jnkDrQoxB3UqQ9hVl3LEKQ73xF1O\n" \
"yK5GhDDX8oVfGKF5u+decIsH4YaTw7mP3GFxJSqv3+0lUFJoi5Lc5da149p90Ids\n" \
"hCExroL1+7mryIkXPeFM5TgO9r0rvZaBFOvV2z0gp35Z0+L4WPlbuEjN/lxPFin+\n" \
"HlUjr8gRsI3qfJOQFy/9rKIJR0Y/8Omwt/8oTWgy1mdeHmmjk7j1nYsvC9JSQ6Zv\n" \
"MldlTTKB3zhThV1+XWYp6rjd5JW1zbVWEkLNxE7GJThEUG3szgBVGP7pSWTUTsqX\n" \
"nLRbwHOoq7hHwg==\n" \
"-----END CERTIFICATE-----\n";

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "Current time: %A, %B %d %Y %H:%M:%S");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n===========================================");
  Serial.println("ENHANCED OPENAI CONNECTION TEST");
  Serial.println("===========================================\n");
  
  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n✗ WiFi connection failed!");
    while(1) delay(1000);
  }
  
  Serial.println("\n✓ WiFi Connected");
  Serial.println("✓ IP: " + WiFi.localIP().toString());
  Serial.printf("✓ Free heap: %d bytes\n\n", ESP.getFreeHeap());
  
  // CRITICAL: Sync time with NTP (required for SSL)
  Serial.println("STEP 1: Syncing time with NTP server...");
  Serial.println("(SSL certificates require correct date/time)");
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  Serial.print("Waiting for time sync");
  int timeAttempts = 0;
  while (time(nullptr) < 100000 && timeAttempts < 20) {
    delay(500);
    Serial.print(".");
    timeAttempts++;
  }
  Serial.println();
  
  if (time(nullptr) < 100000) {
    Serial.println("✗ Time sync FAILED!");
    Serial.println("Will try to continue anyway...");
  } else {
    Serial.println("✓ Time synchronized!");
    printLocalTime();
  }
  
  Serial.println();
  
  // Test DNS resolution
  Serial.println("STEP 2: Testing DNS resolution...");
  IPAddress serverIP;
  if (WiFi.hostByName("api.openai.com", serverIP)) {
    Serial.print("✓ DNS resolved: api.openai.com = ");
    Serial.println(serverIP);
  } else {
    Serial.println("✗ DNS resolution FAILED!");
    Serial.println("ERROR: Cannot resolve api.openai.com");
    while(1) delay(1000);
  }
  
  Serial.println();
  
  // Test 1: Try connection with Let's Encrypt cert
  Serial.println("STEP 3: Testing HTTPS with Let's Encrypt certificate...");
  WiFiClientSecure client;
  client.setCACert(LETSENCRYPT_ROOT_CA);
  client.setTimeout(10);
  
  Serial.println("Attempting connection to api.openai.com:443...");
  if (client.connect("api.openai.com", 443)) {
    Serial.println("✓✓✓ SSL connection SUCCESSFUL with Let's Encrypt cert!");
    client.stop();
  } else {
    Serial.println("✗ Connection failed with Let's Encrypt cert");
    Serial.println("Last SSL error: " + String(client.lastError()));
    
    // Try without certificate verification (insecure but diagnostic)
    Serial.println("\nTrying insecure mode (no cert verification)...");
    client.setInsecure();
    
    if (client.connect("api.openai.com", 443)) {
      Serial.println("✓ Connection works in INSECURE mode");
      Serial.println("→ This means: Network OK, but certificate validation failing");
      Serial.println("→ Likely cause: Time not synced properly or cert issue");
      client.stop();
    } else {
      Serial.println("✗ Connection failed even in insecure mode");
      Serial.println("→ This means: Network/firewall blocking HTTPS");
      while(1) delay(1000);
    }
  }
  
  Serial.println();
  
  // Test 2: Try actual API call
  Serial.println("STEP 4: Testing Whisper API call...");
  
  // Create minimal test WAV
  uint8_t testWav[204];
  memcpy(testWav, "RIFF", 4);
  *(uint32_t*)(testWav + 4) = 196;
  memcpy(testWav + 8, "WAVE", 4);
  memcpy(testWav + 12, "fmt ", 4);
  *(uint32_t*)(testWav + 16) = 16;
  *(uint16_t*)(testWav + 20) = 1;
  *(uint16_t*)(testWav + 22) = 1;
  *(uint32_t*)(testWav + 24) = 16000;
  *(uint32_t*)(testWav + 28) = 32000;
  *(uint16_t*)(testWav + 32) = 2;
  *(uint16_t*)(testWav + 34) = 16;
  memcpy(testWav + 36, "data", 4);
  *(uint32_t*)(testWav + 40) = 160;
  memset(testWav + 44, 0, 160);
  
  String boundary = "----ESP32Test" + String(millis());
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"test.wav\"\r\n";
  bodyStart += "Content-Type: audio/wav\r\n\r\n";
  
  String bodyEnd = "\r\n--" + boundary + "\r\n";
  bodyEnd += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
  bodyEnd += "whisper-1\r\n";
  bodyEnd += "--" + boundary + "--\r\n";
  
  size_t totalSize = bodyStart.length() + 204 + bodyEnd.length();
  uint8_t* requestBody = (uint8_t*)malloc(totalSize);
  
  if (!requestBody) {
    Serial.println("✗ Memory allocation failed");
    while(1) delay(1000);
  }
  
  size_t offset = 0;
  memcpy(requestBody + offset, bodyStart.c_str(), bodyStart.length());
  offset += bodyStart.length();
  memcpy(requestBody + offset, testWav, 204);
  offset += 204;
  memcpy(requestBody + offset, bodyEnd.c_str(), bodyEnd.length());
  
  Serial.println("Sending API request...");
  
  HTTPClient https;
  WiFiClientSecure secureClient;
  secureClient.setCACert(LETSENCRYPT_ROOT_CA);
  
  if (!https.begin(secureClient, "https://api.openai.com/v1/audio/transcriptions")) {
    Serial.println("✗ HTTPS begin failed");
    free(requestBody);
    while(1) delay(1000);
  }
  
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  https.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  https.setTimeout(30000);
  
  int httpCode = https.POST(requestBody, totalSize);
  
  Serial.println("\n===========================================");
  Serial.printf("HTTP Response Code: %d\n", httpCode);
  
  if (httpCode > 0) {
    String response = https.getString();
    Serial.println("\nResponse:");
    Serial.println(response);
    
    if (httpCode == 200) {
      Serial.println("\n✓✓✓ SUCCESS! OpenAI API is working! ✓✓✓");
    } else if (httpCode == 401) {
      Serial.println("\n✗ Authentication failed - check API key");
    } else if (httpCode == 400) {
      Serial.println("\n✗ Bad request format");
    } else {
      Serial.println("\n✗ API error");
    }
  } else {
    Serial.println("\n✗ Connection error:");
    Serial.printf("  Error code: %d\n", httpCode);
  }
  
  Serial.println("===========================================\n");
  
  free(requestBody);
  https.end();
  
  Serial.println("Test complete.");
}

void loop() {
  delay(1000);
}
