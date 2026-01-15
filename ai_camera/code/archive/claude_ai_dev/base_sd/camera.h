/*
 * Camera Configuration for DFRobot ESP32-S3 AI Camera
 *
 * This file contains the pin definitions and initialization code
 * for the OV2640 camera module on the ESP32-S3 board.
 *
 * Pin configuration is specific to DFRobot ESP32-S3 AI Camera
 */

#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"
#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#include <Arduino.h>

// ========== CAMERA PIN DEFINITIONS ==========
// These pins are specific to DFRobot ESP32-S3 AI Camera
// Adjust if using a different ESP32-CAM board

#define PWDN_GPIO_NUM     -1    // Power down pin (not used)
#define RESET_GPIO_NUM    -1    // Reset pin (not used)
#define XCLK_GPIO_NUM     5     // External clock output
#define SIOD_GPIO_NUM     8     // I2C data (SDA)
#define SIOC_GPIO_NUM     9     // I2C clock (SCL)

// Camera data pins (Y9-Y2)
#define Y9_GPIO_NUM       4     // Data bit 9 (MSB)
#define Y8_GPIO_NUM       6     // Data bit 8
#define Y7_GPIO_NUM       7     // Data bit 7
#define Y6_GPIO_NUM       14    // Data bit 6
#define Y5_GPIO_NUM       17    // Data bit 5
#define Y4_GPIO_NUM       21    // Data bit 4
#define Y3_GPIO_NUM       18    // Data bit 3
#define Y2_GPIO_NUM       16    // Data bit 2 (LSB)

// Camera control pins
#define VSYNC_GPIO_NUM    1     // Vertical sync
#define HREF_GPIO_NUM     2     // Horizontal reference
#define PCLK_GPIO_NUM     15    // Pixel clock

/**
 * Initialize the ESP32 camera module
 *
 * This function configures the camera with appropriate settings for
 * image capture and OpenAI Vision API submission.
 *
 * Key settings:
 * - Frame size: Starts at QVGA (320x240) for balance of quality and speed
 * - Format: JPEG for efficient transmission
 * - Quality: 10 (lower number = higher quality)
 * - PSRAM: Used if available for better performance
 */
void initCamera() {
  camera_config_t config;

  // ===== Basic Configuration =====
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  // ===== Pin Configuration =====
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

  // ===== Clock Configuration =====
  config.xclk_freq_hz = 20000000;  // 20MHz clock frequency

  // ===== Image Configuration =====
  config.frame_size = FRAMESIZE_VGA;      // Initial frame size (640x480)
  config.pixel_format = PIXFORMAT_JPEG;    // JPEG format for streaming/API
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM; // Use PSRAM if available
  config.jpeg_quality = 10;                // JPEG quality (0-63, lower is better)
  config.fb_count = 2;                     // Number of frame buffers

  // ===== Optimize Settings Based on Available Memory =====
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      // PSRAM available - use higher quality settings
      Serial.println("PSRAM detected - Using high quality settings");
      config.jpeg_quality = 8;   // Higher quality
      config.fb_count = 2;       // Double buffering
      config.grab_mode = CAMERA_GRAB_LATEST;
      config.frame_size = FRAMESIZE_SVGA;  // 800x600 for better detail
    } else {
      // No PSRAM - use conservative settings
      Serial.println("No PSRAM - Using standard quality settings");
      config.frame_size = FRAMESIZE_VGA;   // 640x480
      config.fb_location = CAMERA_FB_IN_DRAM;
      config.jpeg_quality = 12;  // Lower quality to save memory
    }
  } else {
    // For RGB565 format (not typically used with OpenAI)
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

  // ===== Initialize Camera =====
  Serial.print("Initializing camera... ");
  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("FAILED!\nCamera init error: 0x%x\n", err);
    Serial.println("Please check camera connections and try again.");
    while(1) {
      delay(1000);
    }
  }

  Serial.println("SUCCESS!");

  // ===== Apply Sensor-Specific Settings =====
  sensor_t *s = esp_camera_sensor_get();

  if (s->id.PID == OV3660_PID) {
    // Settings for OV3660 sensor
    s->set_vflip(s, 1);        // Flip vertically
    s->set_brightness(s, 1);   // Increase brightness
    s->set_saturation(s, -2);  // Reduce saturation
  }



  // Additional sensor optimizations
  s->set_vflip(s, 1);          // No vertical flip (adjust if needed 0-no,1-yes)
  s->set_hmirror(s, 0);        // No horizontal mirror (adjust if needed)

  // Set to appropriate frame size for OpenAI Vision API
  // QVGA (320x240) provides good balance of quality and transmission speed
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_VGA);  // Can be changed to FRAMESIZE_SVGA or FRAMESIZE_UXGA for higher resolution
  }

  Serial.println("Camera configuration complete!");
  Serial.printf("Frame size: %dx%d\n",
    config.frame_size == FRAMESIZE_VGA ? 640 :
    config.frame_size == FRAMESIZE_SVGA ? 800 :
    config.frame_size == FRAMESIZE_QVGA ? 320 : 240,
    config.frame_size == FRAMESIZE_VGA ? 480 :
    config.frame_size == FRAMESIZE_SVGA ? 600 :
    config.frame_size == FRAMESIZE_QVGA ? 240 : 240);
}

/**
 * Capture a test image to verify camera is working
 * This can be called in setup() for debugging
 */
bool testCamera() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera test failed!");
    return false;
  }
  Serial.printf("Camera test passed! Image size: %d bytes\n", fb->len);
  esp_camera_fb_return(fb);
  return true;
}

#endif // CAMERA_H
