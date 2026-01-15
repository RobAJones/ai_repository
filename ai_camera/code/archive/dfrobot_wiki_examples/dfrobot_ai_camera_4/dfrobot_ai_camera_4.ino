/*
 * ESP32-S3 AI CAM - Camera Test Page
 * Hardware: DFRobot SKU DFR1154 ESP32-S3 AI CAM
 * 
 * Features:
 * - Web-based camera interface
 * - Capture images and save to SD card
 * - Review captured images
 * - Image gallery of saved photos
 */

#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// WiFi credentials - UPDATE THESE
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Web server on port 80
WebServer server(80);

// Camera pins for ESP32-S3 AI CAM (DFRobot DFR1154)
// Based on official pin designation chart
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

// SD Card pins (SD_MMC interface)
#define SD_MMC_CMD        11  // MO (MOSI)
#define SD_MMC_CLK        12  // SCLK
#define SD_MMC_D0         13  // MI (MISO)

// Global variables
int imageCount = 0;
String latestImage = "";

// HTML page with camera controls
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-S3 Camera Test</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 {
            font-size: 28px;
            margin-bottom: 10px;
        }
        .header p {
            opacity: 0.9;
            font-size: 14px;
        }
        .content {
            padding: 30px;
        }
        .camera-view {
            background: #f0f0f0;
            border-radius: 10px;
            overflow: hidden;
            margin-bottom: 20px;
            position: relative;
            padding-bottom: 75%;
        }
        .camera-view img {
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            object-fit: cover;
        }
        .controls {
            display: flex;
            gap: 15px;
            margin-bottom: 30px;
            flex-wrap: wrap;
        }
        button {
            flex: 1;
            min-width: 150px;
            padding: 15px 30px;
            font-size: 16px;
            font-weight: bold;
            border: none;
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .btn-capture {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .btn-capture:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(102, 126, 234, 0.4);
        }
        .btn-review {
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
            color: white;
        }
        .btn-review:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(245, 87, 108, 0.4);
        }
        .btn-refresh {
            background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
            color: white;
        }
        .btn-refresh:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(79, 172, 254, 0.4);
        }
        button:active {
            transform: translateY(0);
        }
        .status {
            padding: 15px;
            background: #e8f5e9;
            border-left: 4px solid #4caf50;
            border-radius: 5px;
            margin-bottom: 20px;
            display: none;
        }
        .status.show {
            display: block;
        }
        .status.error {
            background: #ffebee;
            border-left-color: #f44336;
        }
        .gallery {
            margin-top: 30px;
        }
        .gallery h2 {
            margin-bottom: 20px;
            color: #333;
        }
        .gallery-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(150px, 1fr));
            gap: 15px;
        }
        .gallery-item {
            position: relative;
            padding-bottom: 100%;
            background: #f0f0f0;
            border-radius: 10px;
            overflow: hidden;
            cursor: pointer;
            transition: transform 0.3s;
        }
        .gallery-item:hover {
            transform: scale(1.05);
        }
        .gallery-item img {
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            object-fit: cover;
        }
        .loading {
            text-align: center;
            padding: 20px;
            color: #666;
        }
        .spinner {
            border: 3px solid #f3f3f3;
            border-top: 3px solid #667eea;
            border-radius: 50%;
            width: 40px;
            height: 40px;
            animation: spin 1s linear infinite;
            margin: 20px auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .modal {
            display: none;
            position: fixed;
            z-index: 1000;
            left: 0;
            top: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0,0,0,0.9);
        }
        .modal-content {
            margin: auto;
            display: block;
            max-width: 90%;
            max-height: 90%;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
        }
        .close {
            position: absolute;
            top: 30px;
            right: 45px;
            color: #f1f1f1;
            font-size: 40px;
            font-weight: bold;
            cursor: pointer;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📷 ESP32-S3 AI Camera</h1>
            <p>Capture & Review Images</p>
        </div>
        
        <div class="content">
            <div class="camera-view">
                <img id="stream" src="/stream" alt="Camera Stream">
            </div>
            
            <div class="status" id="status"></div>
            
            <div class="controls">
                <button class="btn-capture" onclick="captureImage()">📸 Capture Photo</button>
                <button class="btn-review" onclick="reviewLatest()">👁️ Review Latest</button>
                <button class="btn-refresh" onclick="refreshGallery()">🔄 Refresh Gallery</button>
            </div>
            
            <div class="gallery">
                <h2>📂 Saved Images (<span id="imageCount">0</span>)</h2>
                <div class="gallery-grid" id="galleryGrid">
                    <div class="loading">
                        <div class="spinner"></div>
                        Loading gallery...
                    </div>
                </div>
            </div>
        </div>
    </div>
    
    <div id="imageModal" class="modal" onclick="closeModal()">
        <span class="close">&times;</span>
        <img class="modal-content" id="modalImage">
    </div>

    <script>
        let streamActive = true;
        
        function showStatus(message, isError = false) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status show' + (isError ? ' error' : '');
            setTimeout(() => {
                status.className = 'status';
            }, 5000);
        }
        
        function captureImage() {
            showStatus('📸 Capturing image...');
            fetch('/capture')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        showStatus('✅ Image saved: ' + data.filename);
                        refreshGallery();
                    } else {
                        showStatus('❌ Error: ' + data.message, true);
                    }
                })
                .catch(error => {
                    showStatus('❌ Capture failed: ' + error, true);
                });
        }
        
        function reviewLatest() {
            fetch('/latest')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        openModal('/image?file=' + data.filename);
                    } else {
                        showStatus('❌ No images found', true);
                    }
                })
                .catch(error => {
                    showStatus('❌ Error loading image: ' + error, true);
                });
        }
        
        function refreshGallery() {
            const grid = document.getElementById('galleryGrid');
            grid.innerHTML = '<div class="loading"><div class="spinner"></div>Loading gallery...</div>';
            
            fetch('/list')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('imageCount').textContent = data.files.length;
                    
                    if (data.files.length === 0) {
                        grid.innerHTML = '<div class="loading">No images yet. Capture your first photo!</div>';
                        return;
                    }
                    
                    grid.innerHTML = '';
                    data.files.forEach(file => {
                        const item = document.createElement('div');
                        item.className = 'gallery-item';
                        item.innerHTML = '<img src="/image?file=' + file + '" alt="' + file + '">';
                        item.onclick = () => openModal('/image?file=' + file);
                        grid.appendChild(item);
                    });
                })
                .catch(error => {
                    grid.innerHTML = '<div class="loading">Error loading gallery</div>';
                    showStatus('❌ Gallery error: ' + error, true);
                });
        }
        
        function openModal(imageSrc) {
            const modal = document.getElementById('imageModal');
            const modalImg = document.getElementById('modalImage');
            modal.style.display = 'block';
            modalImg.src = imageSrc;
        }
        
        function closeModal() {
            document.getElementById('imageModal').style.display = 'none';
        }
        
        // Keep stream alive
        setInterval(() => {
            if (streamActive) {
                const img = document.getElementById('stream');
                img.src = '/stream?t=' + new Date().getTime();
            }
        }, 100);
        
        // Load gallery on page load
        refreshGallery();
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP32-S3 AI CAM Starting...");
  
  // Initialize SD card (1-bit mode)
  // SD_MMC uses CMD=11, CLK=12, D0=13 based on pin chart
  if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
    Serial.println("SD Card Mount Failed!");
  } else {
    Serial.println("SD Card Mounted Successfully");
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
    // Create images directory
    if (!SD_MMC.exists("/images")) {
      SD_MMC.mkdir("/images");
      Serial.println("Created /images directory");
    }
  }
  
  // Camera configuration
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
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 2;
  
  // Optimize settings based on available memory
  if (psramFound()) {
    Serial.println("PSRAM detected - Using high quality settings");
    config.jpeg_quality = 8;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.frame_size = FRAMESIZE_SVGA;  // 800x600
  } else {
    Serial.println("No PSRAM - Using standard quality settings");
    config.frame_size = FRAMESIZE_VGA;   // 640x480
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 12;
  }
  
  // Initialize camera
  Serial.print("Initializing camera... ");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("FAILED!\nCamera init error: 0x%x\n", err);
    Serial.println("Please check camera connections and try again.");
    return;
  }
  Serial.println("SUCCESS!");
  
  // Configure camera sensor
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    // Check for OV3660 sensor specific settings
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);
      s->set_brightness(s, 1);
      s->set_saturation(s, -2);
    }
    
    // General sensor optimizations
    s->set_vflip(s, 1);          // Vertical flip (adjust if needed: 0=no, 1=yes)
    s->set_hmirror(s, 0);        // Horizontal mirror (adjust if needed)
    
    // Set frame size based on PSRAM availability
    if (psramFound()) {
      s->set_framesize(s, FRAMESIZE_SVGA);  // 800x600 for better quality
    } else {
      s->set_framesize(s, FRAMESIZE_VGA);   // 640x480 standard
    }
  }
  
  Serial.println("Camera configuration complete!");
  Serial.printf("Frame size: ");
  if (psramFound()) {
    Serial.println("800x600 (SVGA)");
  } else {
    Serial.println("640x480 (VGA)");
  }
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("Camera URL: http://");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/image", HTTP_GET, handleImage);
  server.on("/list", HTTP_GET, handleList);
  server.on("/latest", HTTP_GET, handleLatest);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleStream() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  
  WiFiClient client = server.client();
  client.write(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
}

void handleCapture() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Camera capture failed\"}");
    return;
  }
  
  // Generate filename
  imageCount++;
  String filename = "/images/IMG_" + String(imageCount) + ".jpg";
  
  // Save to SD card
  File file = SD_MMC.open(filename.c_str(), FILE_WRITE);
  if (!file) {
    esp_camera_fb_return(fb);
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Failed to open file\"}");
    return;
  }
  
  file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);
  
  latestImage = filename;
  
  Serial.printf("Image saved: %s\n", filename.c_str());
  
  String response = "{\"success\":true,\"filename\":\"" + filename + "\"}";
  server.send(200, "application/json", response);
}

void handleImage() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file parameter");
    return;
  }
  
  String filename = server.arg("file");
  File file = SD_MMC.open(filename.c_str(), FILE_READ);
  
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  
  server.streamFile(file, "image/jpeg");
  file.close();
}

void handleList() {
  File root = SD_MMC.open("/images");
  if (!root) {
    server.send(200, "application/json", "{\"files\":[]}");
    return;
  }
  
  String fileList = "{\"files\":[";
  File file = root.openNextFile();
  bool first = true;
  
  while (file) {
    if (!file.isDirectory()) {
      if (!first) fileList += ",";
      fileList += "\"" + String(file.name()) + "\"";
      first = false;
    }
    file = root.openNextFile();
  }
  
  fileList += "]}";
  server.send(200, "application/json", fileList);
}

void handleLatest() {
  if (latestImage.length() > 0) {
    String response = "{\"success\":true,\"filename\":\"" + latestImage + "\"}";
    server.send(200, "application/json", response);
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"No images captured yet\"}");
  }
}
