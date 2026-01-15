/*
 * ESP32-S3 AI CAM - Camera & Audio Recorder
 * Hardware: DFRobot SKU DFR1154 ESP32-S3 AI CAM
 * 
 * Features:
 * - Web-based camera interface
 * - Capture images and save to SD card
 * - Record audio and save to SD card
 * - Playback audio files
 * - Review captured images
 * - Image and audio gallery with delete functionality
 */

#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <driver/i2s.h>

// WiFi credentials - UPDATE THESE
const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";

// Web server on port 80
WebServer server(80);

// Camera pins for ESP32-S3 AI CAM (DFRobot DFR1154)
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
#define SD_MMC_CMD        11
#define SD_MMC_CLK        12
#define SD_MMC_D0         13
#define SD_MMC_CS         10

// I2S Audio pins (adjust these based on your hardware)
#define I2S_WS            3   // Word Select (LRCLK)
#define I2S_SD            19  // Serial Data (DIN)
#define I2S_SCK           20  // Serial Clock (BCLK)

// Audio configuration
#define SAMPLE_RATE       16000
#define SAMPLE_BITS       16
#define CHANNELS          1
#define I2S_PORT          I2S_NUM_0

// Global variables
int imageCount = 0;
int audioCount = 0;
String latestImage = "";
String latestAudio = "";
bool isRecording = false;

// WAV header structure
struct WAVHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = CHANNELS;
  uint32_t sampleRate = SAMPLE_RATE;
  uint32_t byteRate = SAMPLE_RATE * CHANNELS * (SAMPLE_BITS / 8);
  uint16_t blockAlign = CHANNELS * (SAMPLE_BITS / 8);
  uint16_t bitsPerSample = SAMPLE_BITS;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize;
};

// HTML page with camera and audio controls
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-S3 Media Recorder</title>
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
            max-width: 1000px;
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
        .section {
            margin-bottom: 30px;
        }
        .section h2 {
            color: #333;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 2px solid #667eea;
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
            margin-bottom: 20px;
            flex-wrap: wrap;
        }
        button {
            flex: 1;
            min-width: 140px;
            padding: 12px 20px;
            font-size: 14px;
            font-weight: bold;
            border: none;
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .btn-capture {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .btn-review {
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
            color: white;
        }
        .btn-refresh {
            background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
            color: white;
        }
        .btn-record {
            background: linear-gradient(135deg, #fa709a 0%, #fee140 100%);
            color: white;
        }
        .btn-record.recording {
            background: linear-gradient(135deg, #eb3349 0%, #f45c43 100%);
            animation: pulse 1.5s ease-in-out infinite;
        }
        .btn-stop {
            background: linear-gradient(135deg, #ff6b6b 0%, #ee5a6f 100%);
            color: white;
        }
        .btn-delete {
            background: linear-gradient(135deg, #ff6b6b 0%, #ee5a6f 100%);
            color: white;
            min-width: 80px;
            padding: 8px 15px;
            font-size: 12px;
        }
        button:hover:not(:disabled) {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(0,0,0,0.2);
        }
        button:active {
            transform: translateY(0);
        }
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
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
            margin-top: 20px;
        }
        .gallery h3 {
            margin-bottom: 15px;
            color: #555;
            font-size: 18px;
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
            cursor: pointer;
        }
        .audio-item {
            background: #f0f0f0;
            border-radius: 10px;
            padding: 15px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 10px;
        }
        .audio-info {
            flex: 1;
            min-width: 0;
        }
        .audio-name {
            font-weight: bold;
            color: #333;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .audio-controls {
            display: flex;
            gap: 10px;
            flex-shrink: 0;
        }
        .audio-controls audio {
            height: 32px;
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
        .timer {
            display: inline-block;
            margin-left: 10px;
            font-weight: normal;
            font-size: 14px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>[CAM+MIC] ESP32-S3 Media Recorder</h1>
            <p>Capture Images & Record Audio</p>
        </div>
        
        <div class="content">
            <!-- Camera Section -->
            <div class="section">
                <h2>[CAMERA] Live View</h2>
                <div class="camera-view">
                    <img id="stream" src="/stream" alt="Camera Stream">
                </div>
                
                <div class="controls">
                    <button class="btn-capture" onclick="captureImage()">[+] Capture Photo</button>
                    <button class="btn-review" onclick="reviewLatest()">[>] Review Latest</button>
                    <button class="btn-refresh" onclick="refreshGallery()">[*] Refresh All</button>
                </div>
            </div>
            
            <!-- Audio Section -->
            <div class="section">
                <h2>[AUDIO] Voice Recorder</h2>
                <div class="controls">
                    <button class="btn-record" id="recordBtn" onclick="toggleRecording()">
                        [REC] Start Recording<span id="timer" class="timer"></span>
                    </button>
                </div>
            </div>
            
            <div class="status" id="status"></div>
            
            <!-- Image Gallery -->
            <div class="gallery">
                <h3>[IMAGES] Saved Photos (<span id="imageCount">0</span>)</h3>
                <div class="gallery-grid" id="imageGallery">
                    <div class="loading">Loading images...</div>
                </div>
            </div>
            
            <!-- Audio Gallery -->
            <div class="gallery">
                <h3>[AUDIO] Saved Recordings (<span id="audioCount">0</span>)</h3>
                <div class="gallery-grid" id="audioGallery">
                    <div class="loading">Loading audio...</div>
                </div>
            </div>
        </div>
    </div>
    
    <!-- Modal for viewing images -->
    <div id="imageModal" class="modal" onclick="closeModal()">
        <span class="close">&times;</span>
        <img class="modal-content" id="modalImage">
    </div>
    
    <script>
        let streamActive = true;
        let recordingInterval = null;
        let recordingSeconds = 0;
        
        function showStatus(message, isError = false) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status show' + (isError ? ' error' : '');
            setTimeout(() => {
                status.className = 'status';
            }, 5000);
        }
        
        function captureImage() {
            showStatus('Capturing image...');
            fetch('/capture')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        showStatus('[OK] Image saved: ' + data.filename);
                        refreshImageGallery();
                    } else {
                        showStatus('[ERROR] ' + data.message, true);
                    }
                })
                .catch(error => {
                    showStatus('[ERROR] Capture failed: ' + error, true);
                });
        }
        
        function toggleRecording() {
            fetch('/audio/status')
                .then(response => response.json())
                .then(data => {
                    if (data.recording) {
                        stopRecording();
                    } else {
                        startRecording();
                    }
                });
        }
        
        function startRecording() {
            showStatus('Starting audio recording...');
            fetch('/audio/start')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        const btn = document.getElementById('recordBtn');
                        btn.className = 'btn-record recording';
                        btn.innerHTML = '[STOP] Recording<span id="timer" class="timer">0:00</span>';
                        showStatus('[OK] Recording started');
                        recordingSeconds = 0;
                        recordingInterval = setInterval(updateTimer, 1000);
                    } else {
                        showStatus('[ERROR] ' + data.message, true);
                    }
                })
                .catch(error => {
                    showStatus('[ERROR] Failed to start recording: ' + error, true);
                });
        }
        
        function stopRecording() {
            showStatus('Stopping recording...');
            fetch('/audio/stop')
                .then(response => response.json())
                .then(data => {
                    const btn = document.getElementById('recordBtn');
                    btn.className = 'btn-record';
                    btn.innerHTML = '[REC] Start Recording<span id="timer" class="timer"></span>';
                    clearInterval(recordingInterval);
                    
                    if (data.success) {
                        showStatus('[OK] Recording saved: ' + data.filename);
                        refreshAudioGallery();
                    } else {
                        showStatus('[ERROR] ' + data.message, true);
                    }
                })
                .catch(error => {
                    showStatus('[ERROR] Failed to stop recording: ' + error, true);
                });
        }
        
        function updateTimer() {
            recordingSeconds++;
            const minutes = Math.floor(recordingSeconds / 60);
            const seconds = recordingSeconds % 60;
            document.getElementById('timer').textContent = 
                minutes + ':' + (seconds < 10 ? '0' : '') + seconds;
        }
        
        function reviewLatest() {
            fetch('/latest')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        openModal('/image?file=' + data.filename);
                    } else {
                        showStatus('[INFO] ' + data.message, true);
                    }
                })
                .catch(error => {
                    showStatus('[ERROR] ' + error, true);
                });
        }
        
        function refreshGallery() {
            refreshImageGallery();
            refreshAudioGallery();
        }
        
        function refreshImageGallery() {
            const grid = document.getElementById('imageGallery');
            grid.innerHTML = '<div class="loading"><div class="spinner"></div>Loading images...</div>';
            
            fetch('/list/images')
                .then(response => response.json())
                .then(data => {
                    grid.innerHTML = '';
                    document.getElementById('imageCount').textContent = data.files.length;
                    
                    if (data.files.length === 0) {
                        grid.innerHTML = '<div class="loading">No images yet</div>';
                        return;
                    }
                    
                    data.files.forEach(file => {
                        const item = document.createElement('div');
                        item.className = 'gallery-item';
                        item.innerHTML = `
                            <img src="/image?file=${file}" alt="${file}" onclick="openModal('/image?file=${file}')">
                            <button class="btn-delete" onclick="deleteFile('${file}', 'image')" 
                                    style="position:absolute;bottom:5px;right:5px;">Delete</button>
                        `;
                        grid.appendChild(item);
                    });
                })
                .catch(error => {
                    grid.innerHTML = '<div class="loading">Error loading images</div>';
                    showStatus('[ERROR] Image gallery error: ' + error, true);
                });
        }
        
        function refreshAudioGallery() {
            const grid = document.getElementById('audioGallery');
            grid.innerHTML = '<div class="loading"><div class="spinner"></div>Loading audio...</div>';
            
            fetch('/list/audio')
                .then(response => response.json())
                .then(data => {
                    grid.innerHTML = '';
                    document.getElementById('audioCount').textContent = data.files.length;
                    
                    if (data.files.length === 0) {
                        grid.innerHTML = '<div class="loading">No recordings yet</div>';
                        return;
                    }
                    
                    data.files.forEach(file => {
                        const item = document.createElement('div');
                        item.className = 'audio-item';
                        item.innerHTML = `
                            <div class="audio-info">
                                <div class="audio-name">${file}</div>
                            </div>
                            <div class="audio-controls">
                                <audio controls preload="none">
                                    <source src="/audio?file=${file}" type="audio/wav">
                                </audio>
                                <button class="btn-delete" onclick="deleteFile('${file}', 'audio')">Delete</button>
                            </div>
                        `;
                        grid.appendChild(item);
                    });
                })
                .catch(error => {
                    grid.innerHTML = '<div class="loading">Error loading audio</div>';
                    showStatus('[ERROR] Audio gallery error: ' + error, true);
                });
        }
        
        function deleteFile(filename, type) {
            if (!confirm('Delete ' + filename + '?')) return;
            
            fetch('/delete/' + type + '?file=' + encodeURIComponent(filename), {method: 'DELETE'})
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        showStatus('[OK] Deleted: ' + filename);
                        if (type === 'image') refreshImageGallery();
                        else refreshAudioGallery();
                    } else {
                        showStatus('[ERROR] ' + data.message, true);
                    }
                })
                .catch(error => {
                    showStatus('[ERROR] Delete failed: ' + error, true);
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
        
        // Load galleries on page load
        refreshGallery();
    </script>
</body>
</html>
)rawliteral";

void initI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    return;
  }
  
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
  } else {
    Serial.println("I2S initialized successfully");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP32-S3 Media Recorder Starting...");
  
  // Initialize SD card
  Serial.println("Initializing SD Card...");
  if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
    Serial.println("Failed to set SD_MMC pins!");
  }
  
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed!");
  } else {
    Serial.println("SD Card Mounted Successfully");
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
    // Create directories
    if (!SD_MMC.exists("/images")) {
      SD_MMC.mkdir("/images");
      Serial.println("Created /images directory");
    }
    if (!SD_MMC.exists("/audio")) {
      SD_MMC.mkdir("/audio");
      Serial.println("Created /audio directory");
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
  
  if (psramFound()) {
    Serial.println("PSRAM detected - Using high quality settings");
    config.jpeg_quality = 8;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.frame_size = FRAMESIZE_SVGA;
  } else {
    Serial.println("No PSRAM - Using standard quality settings");
    config.frame_size = FRAMESIZE_VGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 12;
  }
  
  // Initialize camera
  Serial.print("Initializing camera... ");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("FAILED!\nCamera init error: 0x%x\n", err);
    return;
  }
  Serial.println("SUCCESS!");
  
  // Configure camera sensor
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);
      s->set_brightness(s, 1);
      s->set_saturation(s, -2);
    }
    s->set_vflip(s, 1);
    s->set_hmirror(s, 0);
    
    if (psramFound()) {
      s->set_framesize(s, FRAMESIZE_SVGA);
    } else {
      s->set_framesize(s, FRAMESIZE_VGA);
    }
  }
  
  Serial.println("Camera configuration complete!");
  
  // Initialize I2S for audio
  initI2S();
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("Media Recorder URL: http://");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/image", HTTP_GET, handleImage);
  server.on("/audio", HTTP_GET, handleAudio);
  server.on("/list/images", HTTP_GET, handleListImages);
  server.on("/list/audio", HTTP_GET, handleListAudio);
  server.on("/latest", HTTP_GET, handleLatest);
  server.on("/audio/start", HTTP_GET, handleAudioStart);
  server.on("/audio/stop", HTTP_GET, handleAudioStop);
  server.on("/audio/status", HTTP_GET, handleAudioStatus);
  server.on("/delete/image", HTTP_DELETE, handleDeleteImage);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  
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
  
  imageCount++;
  String filename = "/images/IMG_" + String(imageCount) + ".jpg";
  
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

void handleAudio() {
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
  
  server.streamFile(file, "audio/wav");
  file.close();
}

void handleListImages() {
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
      fileList += "\"" + String(file.path()) + "\"";
      first = false;
    }
    file = root.openNextFile();
  }
  
  fileList += "]}";
  server.send(200, "application/json", fileList);
}

void handleListAudio() {
  File root = SD_MMC.open("/audio");
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
      fileList += "\"" + String(file.path()) + "\"";
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

void handleAudioStart() {
  if (isRecording) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Already recording\"}");
    return;
  }
  
  audioCount++;
  latestAudio = "/audio/REC_" + String(audioCount) + ".wav";
  
  isRecording = true;
  Serial.printf("Starting audio recording: %s\n", latestAudio.c_str());
  
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Recording started\"}");
}

void handleAudioStop() {
  if (!isRecording) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Not recording\"}");
    return;
  }
  
  isRecording = false;
  
  // Record audio for 5 seconds (adjust as needed)
  File file = SD_MMC.open(latestAudio.c_str(), FILE_WRITE);
  if (!file) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Failed to create audio file\"}");
    return;
  }
  
  // Write WAV header (will update sizes later)
  WAVHeader header;
  header.fileSize = 0;
  header.dataSize = 0;
  file.write((uint8_t*)&header, sizeof(WAVHeader));
  
  // Record audio
  const int recordTime = 5; // seconds
  const int bufferSize = 1024;
  int16_t buffer[bufferSize];
  size_t bytesRead;
  uint32_t totalBytes = 0;
  
  unsigned long startTime = millis();
  while (millis() - startTime < recordTime * 1000) {
    i2s_read(I2S_PORT, buffer, bufferSize * sizeof(int16_t), &bytesRead, portMAX_DELAY);
    file.write((uint8_t*)buffer, bytesRead);
    totalBytes += bytesRead;
  }
  
  // Update WAV header with correct sizes
  header.fileSize = totalBytes + sizeof(WAVHeader) - 8;
  header.dataSize = totalBytes;
  file.seek(0);
  file.write((uint8_t*)&header, sizeof(WAVHeader));
  
  file.close();
  
  Serial.printf("Audio saved: %s (%d bytes)\n", latestAudio.c_str(), totalBytes);
  
  String response = "{\"success\":true,\"filename\":\"" + latestAudio + "\"}";
  server.send(200, "application/json", response);
}

void handleAudioStatus() {
  String status = isRecording ? "true" : "false";
  server.send(200, "application/json", "{\"recording\":" + status + "}");
}

void handleDeleteImage() {
  if (!server.hasArg("file")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing file parameter\"}");
    return;
  }
  
  String filename = server.arg("file");
  
  if (SD_MMC.remove(filename.c_str())) {
    Serial.printf("Deleted image: %s\n", filename.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Failed to delete file\"}");
  }
}

void handleDeleteAudio() {
  if (!server.hasArg("file")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing file parameter\"}");
    return;
  }
  
  String filename = server.arg("file");
  
  if (SD_MMC.remove(filename.c_str())) {
    Serial.printf("Deleted audio: %s\n", filename.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Failed to delete file\"}");
  }
}
