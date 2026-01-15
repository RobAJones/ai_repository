/*
 * ESP32-S3 Camera Test Sketch - REVISED
 * 
 * Fixes for common issues:
 * - Added proper delays for serial initialization
 * - Fixed camera frame buffer overflow issues
 * - Better error handling and diagnostics
 * - Clearer output formatting
 */

#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "ESP_I2S.h"
#include <WiFi.h>
#include <Arduino.h>
#include <DFRobot_MAX98357A.h>

// ========== PIN DEFINITIONS ==========
#define BUTTON_PIN 0
#define LED_PIN 3
#define DATA_PIN (GPIO_NUM_39)
#define CLOCK_PIN (GPIO_NUM_38)
#define SAMPLE_RATE (16000)

// ========== WIFI CREDENTIALS ==========
const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";

// ========== CAMERA PIN DEFINITIONS ==========
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     5
#define SIOD_GPIO_NUM     8
#define SIOC_GPIO_NUM     9
#define Y9_GPIO_NUM       4
#define Y8_GPIO_NUM       6
#define Y7_GPIO_NUM       7
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       17
#define Y4_GPIO_NUM       21
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       16
#define VSYNC_GPIO_NUM    1
#define HREF_GPIO_NUM     2
#define PCLK_GPIO_NUM     15

I2SClass I2S;
bool testPassed = true;

void setup() {
  // Initialize serial with longer delay
  Serial.begin(115200);
  
  // CRITICAL: Wait for serial to stabilize
  delay(3000);
  
  // Clear any garbage in serial buffer
  while(Serial.available()) {
    Serial.read();
  }
  
  // Send a few newlines to clear display
  Serial.println("\n\n\n\n");
  
  Serial.println("╔════════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 Camera Hardware Test Suite      ║");
  Serial.println("║  REVISED VERSION                           ║");
  Serial.println("╚════════════════════════════════════════════╝");
  Serial.println();
  delay(1000);
  
  // Test 1: GPIO Setup
  runTest("GPIO Pin Setup", testGPIO);
  
  // Test 2: LED Control
  runTest("LED Control", testLED);
  
  // Test 3: Button Input
  runTest("Button Input", testButton);
  
  // Test 4: Memory Check
  runTest("Memory (PSRAM)", testMemory);
  
  // Test 5: Camera (with fixes)
  runTest("Camera Module", testCameraModule);
  
  // Test 6: I2S Microphone
  runTest("I2S Microphone", testMicrophone);
  
  // Test 7: WiFi
  runTest("WiFi Connection", testWiFi);
  
  // Final Results
  Serial.println();
  Serial.println("╔════════════════════════════════════════════╗");
  if (testPassed) {
    Serial.println("║  ✓ ALL TESTS PASSED                        ║");
    Serial.println("║  Your hardware is ready!                   ║");
  } else {
    Serial.println("║  ✗ SOME TESTS FAILED                       ║");
    Serial.println("║  Check errors above                        ║");
  }
  Serial.println("╚════════════════════════════════════════════╝");
  Serial.println();
  
  if (testPassed) {
    Serial.println("Next steps:");
    Serial.println("1. Note your IP address above");
    Serial.println("2. You can now upload the main OpenAI sketch");
    Serial.println("3. Make sure to update WiFi and API credentials");
  }
}

void loop() {
  // Blink LED slowly to show system is running
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 2000) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    lastBlink = millis();
  }
  
  // Check button state (debounced)
  static unsigned long lastPress = 0;
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastPress > 500) {
    Serial.println(">>> Button pressed!");
    lastPress = millis();
  }
}

// ========== TEST FUNCTIONS ==========

void runTest(const char* testName, bool (*testFunc)()) {
  Serial.print("Testing ");
  Serial.print(testName);
  Serial.print("... ");
  Serial.flush();
  
  delay(500);
  
  bool result = testFunc();
  
  if (result) {
    Serial.println(" ✓ PASS");
  } else {
    Serial.println(" ✗ FAIL");
    testPassed = false;
  }
  
  Serial.flush();
  delay(500);
}

bool testGPIO() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  return true;
}

bool testLED() {
  Serial.println();
  Serial.println("   Blinking LED 3 times...");
  
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  return true;
}

bool testButton() {
  Serial.println();
  Serial.println("   Press the button within 5 seconds...");
  
  unsigned long startTime = millis();
  bool buttonPressed = false;
  
  while (millis() - startTime < 5000) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      buttonPressed = true;
      Serial.println("   ✓ Button detected!");
      delay(500); // Debounce
      break;
    }
    delay(10);
  }
  
  if (!buttonPressed) {
    Serial.println("   ✗ Button not pressed - may need checking");
  }
  
  return buttonPressed;
}

bool testMemory() {
  bool psram = psramFound();
  
  Serial.println();
  if (psram) {
    size_t psramSize = ESP.getPsramSize();
    size_t psramFree = ESP.getFreePsram();
    Serial.printf("   ✓ PSRAM Total: %d bytes (%.2f MB)\n", psramSize, psramSize/1024.0/1024.0);
    Serial.printf("   ✓ PSRAM Free: %d bytes (%.2f MB)\n", psramFree, psramFree/1024.0/1024.0);
  } else {
    Serial.println("   ✗ WARNING: No PSRAM detected!");
    Serial.println("   Enable PSRAM: Tools > PSRAM > Enabled");
  }
  
  size_t heapSize = ESP.getHeapSize();
  size_t heapFree = ESP.getFreeHeap();
  Serial.printf("   Heap Total: %d bytes (%.2f KB)\n", heapSize, heapSize/1024.0);
  Serial.printf("   Heap Free: %d bytes (%.2f KB)\n", heapFree, heapFree/1024.0);
  
  return psram; // PSRAM is important for camera
}

bool testCameraModule() {
  Serial.println();
  Serial.println("   Initializing camera with proper settings...");
  
  // DISABLE BROWNOUT DETECTOR - Important for camera
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  // START WITH CONSERVATIVE SETTINGS TO AVOID FB-OVF
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;  // Start small: 320x240
  config.jpeg_quality = 12;
  config.fb_count = 1;  // Single buffer to start
  
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    Serial.println("   Using PSRAM for frame buffer");
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    Serial.println("   WARNING: Using DRAM (no PSRAM)");
  }
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("   ✗ Camera init FAILED: 0x%x\n", err);
    Serial.println("   Common causes:");
    Serial.println("     - Camera not connected");
    Serial.println("     - Wrong pin configuration");
    Serial.println("     - PSRAM not enabled");
    return false;
  }
  
  Serial.println("   ✓ Camera initialized successfully!");
  
  // Get sensor for additional settings
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    // Optimize sensor settings to prevent FB-OVF
    s->set_framesize(s, FRAMESIZE_QVGA);  // Keep at QVGA
    s->set_quality(s, 12);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_special_effect(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 0);
    s->set_ae_level(s, 0);
    s->set_aec_value(s, 300);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_gainceiling(s, (gainceiling_t)0);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_dcw(s, 1);
    s->set_colorbar(s, 0);
    
    Serial.println("   ✓ Sensor configured");
  }
  
  delay(500);
  
  // Try to capture a test frame
  Serial.println("   Attempting to capture test image...");
  
  // Clear any pending frames first
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    esp_camera_fb_return(fb);
    delay(100);
  }
  
  // Now capture fresh frame
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("   ✗ Camera capture FAILED!");
    Serial.println("   This usually means:");
    Serial.println("     - Frame buffer overflow (FB-OVF)");
    Serial.println("     - Insufficient memory");
    Serial.println("     - Camera sensor issue");
    return false;
  }
  
  Serial.printf("   ✓ Image captured: %d bytes\n", fb->len);
  Serial.printf("   ✓ Resolution: %dx%d\n", fb->width, fb->height);
  Serial.printf("   ✓ Format: %s\n", fb->format == PIXFORMAT_JPEG ? "JPEG" : "RAW");
  
  // Return frame buffer immediately
  esp_camera_fb_return(fb);
  
  return true;
}

bool testMicrophone() {
  Serial.println();
  Serial.println("   Initializing I2S microphone...");
  
  I2S.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("   ✗ Failed to initialize I2S!");
    return false;
  }
  
  Serial.println("   ✓ I2S initialized");
  Serial.println("   Checking for audio data (speak now)...");
  
  // Wait a bit for audio data
  delay(500);
  size_t available = I2S.available();
  
  if (available > 0) {
    Serial.printf("   ✓ Audio data detected: %d bytes\n", available);
    
    // Read some samples and check level
    int16_t samples[100];
    I2S.readBytes((char*)samples, 200);
    
    int maxSample = 0;
    for (int i = 0; i < 100; i++) {
      if (abs(samples[i]) > maxSample) {
        maxSample = abs(samples[i]);
      }
    }
    
    Serial.printf("   Max audio level: %d\n", maxSample);
    if (maxSample < 100) {
      Serial.println("   ⚠ Audio level very low - check microphone");
    }
  } else {
    Serial.println("   ⚠ No audio data (microphone may not be connected)");
  }
  
  return true;
}

bool testWiFi() {
  Serial.println();
  Serial.println("   Connecting to WiFi...");
  Serial.printf("   SSID: '%s'\n", ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Scan for networks first
  Serial.println("   Scanning for networks...");
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("   ✗ No networks found!");
    return false;
  }
  
  Serial.printf("   Found %d networks:\n", n);
  bool foundNetwork = false;
  for (int i = 0; i < n && i < 5; i++) {
    Serial.printf("     %d: %s (%d dBm) %s\n", 
      i + 1, 
      WiFi.SSID(i).c_str(), 
      WiFi.RSSI(i),
      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Secured");
    
    if (WiFi.SSID(i) == ssid) {
      foundNetwork = true;
      Serial.printf("   ✓ Found your network '%s' (signal: %d dBm)\n", 
        ssid, WiFi.RSSI(i));
    }
  }
  
  if (!foundNetwork) {
    Serial.printf("   ✗ Your network '%s' not found!\n", ssid);
    Serial.println("   Check:");
    Serial.println("     - SSID is correct (case-sensitive)");
    Serial.println("     - Router is on and broadcasting");
    Serial.println("     - Using 2.4GHz (not 5GHz)");
    return false;
  }
  
  Serial.println("   Connecting...");
  WiFi.begin(ssid, password);
  
  unsigned long startTime = millis();
  int dots = 0;
  
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
    dots++;
    if (dots % 20 == 0) Serial.println();
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("   ✓ WiFi connected successfully!");
    Serial.print("   ✓ IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("   ✓ Signal Strength: %d dBm ", WiFi.RSSI());
    
    int rssi = WiFi.RSSI();
    if (rssi > -50) {
      Serial.println("(Excellent)");
    } else if (rssi > -70) {
      Serial.println("(Good)");
    } else {
      Serial.println("(Weak - move closer to router)");
    }
    
    return true;
  } else {
    Serial.println("   ✗ WiFi connection FAILED!");
    Serial.printf("   Status code: %d\n", WiFi.status());
    Serial.println("   Common issues:");
    Serial.println("     - Wrong password");
    Serial.println("     - MAC filtering on router");
    Serial.println("     - Router connection limit reached");
    Serial.println("     - 5GHz network (ESP32 only supports 2.4GHz)");
    return false;
  }
}
