/*
 * ESP32-S3 AI CAM - Camera & Audio Recorder (FINAL - WORKING!)
 * 
 * Using ESP_I2S library with PDM mode as per working example
 * Hardware: DFRobot SKU DFR1154 ESP32-S3 AI CAM
 */

#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include "ESP_I2S.h"

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";

WebServer server(80);

// Camera pins
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

// SD Card pins
#define SD_MMC_CMD        11
#define SD_MMC_CLK        12
#define SD_MMC_D0         13

// PDM Microphone pins (working configuration!)
#define DATA_PIN          GPIO_NUM_39
#define CLOCK_PIN         GPIO_NUM_38

// Audio configuration
#define SAMPLE_RATE       16000

// Global variables
int imageCount = 0;
int audioCount = 0;
String latestImage = "";
String latestAudio = "";
bool isRecording = false;
unsigned long recordStartTime = 0;

I2SClass i2s;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-S3 Media Recorder</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
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
        .header h1 { font-size: 28px; margin-bottom: 10px; }
        .header p { opacity: 0.9; font-size: 14px; }
        .content { padding: 30px; }
        .section { margin-bottom: 30px; }
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
            top: 0; left: 0;
            width: 100%; height: 100%;
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
        button:active { transform: translateY(0); }
        button:disabled { opacity: 0.5; cursor: not-allowed; }
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
        .status.show { display: block; }
        .status.error {
            background: #ffebee;
            border-left-color: #f44336;
        }
        .audio-item {
            background: #f0f0f0;
            border-radius: 10px;
            padding: 15px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 10px;
            margin-bottom: 10px;
        }
        .audio-info { flex: 1; min-width: 0; }
        .audio-name {
            font-weight: bold;
            color: #333;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        audio { width: 100%; margin-top: 10px; }
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
        .gallery-item:hover { transform: scale(1.05); }
        .gallery-item img {
            position: absolute;
            top: 0; left: 0;
            width: 100%; height: 100%;
            object-fit: cover;
            cursor: pointer;
        }
        .recording-timer {
            text-align: center;
            font-size: 24px;
            color: #eb3349;
            margin: 10px 0;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📷 ESP32-S3 Media Recorder</h1>
            <p>Camera & PDM Audio System</p>
        </div>
        
        <div class="content">
            <div id="status" class="status"></div>
            
            <div class="section">
                <h2>📸 Camera</h2>
                <div class="camera-view">
                    <img id="stream" src="/stream" alt="Camera Stream">
                </div>
                <div class="controls">
                    <button class="btn-capture" onclick="captureImage()">📷 Capture Image</button>
                    <button class="btn-review" onclick="reviewImages()">🖼️ Review Images</button>
                    <button class="btn-refresh" onclick="refreshStream()">🔄 Refresh Stream</button>
                </div>
            </div>
            
            <div class="section">
                <h2>🎤 Audio Recorder</h2>
                <div id="recordingTimer" class="recording-timer" style="display:none;">00:00</div>
                <div class="controls">
                    <button id="recordBtn" class="btn-record" onclick="startRecording()">⏺️ Start Recording</button>
                    <button id="stopBtn" class="btn-stop" onclick="stopRecording()" disabled>⏹️ Stop Recording</button>
                </div>
                <div id="audioList"></div>
            </div>
            
            <div id="imageGallery" class="gallery-grid" style="display:none;"></div>
        </div>
    </div>
    
    <script>
        let recordingStartTime = 0;
        let timerInterval = null;
        
        function showStatus(message, isError = false) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status show' + (isError ? ' error' : '');
            setTimeout(() => status.classList.remove('show'), 3000);
        }
        
        function updateTimer() {
            const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
            const minutes = Math.floor(elapsed / 60);
            const seconds = elapsed % 60;
            document.getElementById('recordingTimer').textContent = 
                String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
        }
        
        function refreshStream() {
            document.getElementById('stream').src = '/stream?' + new Date().getTime();
        }
        
        function captureImage() {
            fetch('/capture')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        showStatus('Image captured successfully!');
                        setTimeout(refreshStream, 500);
                    } else {
                        showStatus('Failed to capture image', true);
                    }
                });
        }
        
        function startRecording() {
            fetch('/audio/start')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        document.getElementById('recordBtn').disabled = true;
                        document.getElementById('stopBtn').disabled = false;
                        document.getElementById('recordBtn').classList.add('recording');
                        document.getElementById('recordingTimer').style.display = 'block';
                        recordingStartTime = Date.now();
                        timerInterval = setInterval(updateTimer, 100);
                        showStatus('Recording started...');
                    } else {
                        showStatus('Failed to start recording', true);
                    }
                });
        }
        
        function stopRecording() {
            clearInterval(timerInterval);
            document.getElementById('recordingTimer').style.display = 'none';
            showStatus('Stopping recording...');
            
            fetch('/audio/stop')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('recordBtn').disabled = false;
                    document.getElementById('stopBtn').disabled = true;
                    document.getElementById('recordBtn').classList.remove('recording');
                    
                    if (data.success) {
                        showStatus('Recording saved: ' + data.bytes + ' bytes');
                        loadAudioFiles();
                    } else {
                        showStatus('Failed to stop recording', true);
                    }
                });
        }
        
        function reviewImages() {
            loadImageFiles();
        }
        
        function loadImageFiles() {
            fetch('/list/images')
                .then(response => response.json())
                .then(data => {
                    const gallery = document.getElementById('imageGallery');
                    gallery.innerHTML = '';
                    
                    if (data.files.length === 0) {
                        showStatus('No images found', true);
                        gallery.style.display = 'none';
                        return;
                    }
                    
                    data.files.forEach(file => {
                        const item = document.createElement('div');
                        item.className = 'gallery-item';
                        item.innerHTML = '<img src="/image?file=' + encodeURIComponent(file) + '" alt="' + file + '">';
                        
                        const deleteBtn = document.createElement('button');
                        deleteBtn.className = 'btn-delete';
                        deleteBtn.textContent = '🗑️';
                        deleteBtn.style.position = 'absolute';
                        deleteBtn.style.top = '5px';
                        deleteBtn.style.right = '5px';
                        deleteBtn.onclick = (e) => {
                            e.stopPropagation();
                            deleteImage(file);
                        };
                        item.appendChild(deleteBtn);
                        gallery.appendChild(item);
                    });
                    
                    gallery.style.display = 'grid';
                });
        }
        
        function loadAudioFiles() {
            fetch('/list/audio')
                .then(response => response.json())
                .then(data => {
                    const list = document.getElementById('audioList');
                    list.innerHTML = '';
                    
                    if (data.files.length === 0) return;
                    
                    data.files.forEach(file => {
                        const item = document.createElement('div');
                        item.className = 'audio-item';
                        item.innerHTML = `
                            <div class="audio-info">
                                <div class="audio-name">${file.split('/').pop()}</div>
                                <audio controls>
                                    <source src="/audio?file=${encodeURIComponent(file)}" type="audio/wav">
                                </audio>
                            </div>
                            <button class="btn-delete" onclick="deleteAudio('${file}')">🗑️</button>
                        `;
                        list.appendChild(item);
                    });
                });
        }
        
        function deleteImage(file) {
            if (!confirm('Delete this image?')) return;
            fetch('/delete/image?file=' + encodeURIComponent(file), {method: 'DELETE'})
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        showStatus('Image deleted');
                        loadImageFiles();
                    }
                });
        }
        
        function deleteAudio(file) {
            if (!confirm('Delete this audio file?')) return;
            fetch('/delete/audio?file=' + encodeURIComponent(file), {method: 'DELETE'})
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        showStatus('Audio deleted');
                        loadAudioFiles();
                    }
                });
        }
        
        setInterval(refreshStream, 100);
        loadAudioFiles();
    </script>
</body>
</html>
)rawliteral";

void countExistingFiles() {
  File root = SD_MMC.open("/images");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String name = String(file.name());
        if (name.startsWith("IMG_") && name.endsWith(".jpg")) {
          int num = name.substring(4, name.length() - 4).toInt();
          if (num > imageCount) imageCount = num;
        }
      }
      file = root.openNextFile();
    }
  }
  
  root = SD_MMC.open("/audio");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String name = String(file.name());
        if (name.startsWith("REC_") && name.endsWith(".wav")) {
          int num = name.substring(4, name.length() - 4).toInt();
          if (num > audioCount) audioCount = num;
        }
      }
      file = root.openNextFile();
    }
  }
  
  Serial.printf("Found %d images, %d audio files\n", imageCount, audioCount);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== ESP32-S3 Media Recorder ===");
  Serial.println("Using ESP_I2S library with PDM mode\n");
  
  // Initialize LED (optional)
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);
  
  // SD Card initialization
  Serial.println("→ Initializing SD Card...");
  if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
    Serial.println("  SD pin config failed!");
  }
  
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("  SD Mount Failed!");
  } else {
    Serial.printf("  ✓ SD OK: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
    
    if (!SD_MMC.exists("/images")) SD_MMC.mkdir("/images");
    if (!SD_MMC.exists("/audio")) SD_MMC.mkdir("/audio");
    
    countExistingFiles();
  }
  
  // Camera initialization
  Serial.println("\n→ Initializing Camera...");
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
    Serial.println("  PSRAM detected");
    config.jpeg_quality = 8;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.frame_size = FRAMESIZE_SVGA;
  } else {
    Serial.println("  No PSRAM");
    config.frame_size = FRAMESIZE_VGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 12;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("  Camera init failed: 0x%x\n", err);
  } else {
    Serial.println("  ✓ Camera OK");
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
      if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
      }
      s->set_vflip(s, 1);
      s->set_hmirror(s, 0);
    }
  }
  
  // PDM Microphone initialization using ESP_I2S library
  Serial.println("\n→ Initializing PDM Microphone...");
  i2s.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("  Failed to initialize I2S PDM RX");
  } else {
    Serial.printf("  ✓ PDM Microphone OK (GPIO %d/%d, %dHz)\n", CLOCK_PIN, DATA_PIN, SAMPLE_RATE);
  }
  
  // WiFi connection
  Serial.println("\n→ Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n  ✓ Connected: http://%s\n", WiFi.localIP().toString().c_str());
  
  // Web server setup
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/image", HTTP_GET, handleImage);
  server.on("/audio", HTTP_GET, handleAudio);
  server.on("/list/images", HTTP_GET, handleListImages);
  server.on("/list/audio", HTTP_GET, handleListAudio);
  server.on("/audio/start", HTTP_GET, handleAudioStart);
  server.on("/audio/stop", HTTP_GET, handleAudioStop);
  server.on("/delete/image", HTTP_DELETE, handleDeleteImage);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  
  server.begin();
  Serial.println("\n✓ System ready!\n");
}

void loop() {
  server.handleClient();
  
  // Check if recording should stop (this runs continuously)
  if (isRecording && (millis() - recordStartTime > 60000)) {
    // Auto-stop after 60 seconds max
    Serial.println("Auto-stopping recording after 60 seconds");
    isRecording = false;
  }
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
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Camera failed\"}");
    return;
  }
  
  imageCount++;
  String filename = "/images/IMG_" + String(imageCount) + ".jpg";
  
  File file = SD_MMC.open(filename.c_str(), FILE_WRITE);
  if (!file) {
    esp_camera_fb_return(fb);
    server.send(200, "application/json", "{\"success\":false,\"message\":\"SD write failed\"}");
    return;
  }
  
  file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);
  
  latestImage = filename;
  Serial.printf("Image saved: %s\n", filename.c_str());
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleImage() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }
  
  File file = SD_MMC.open(server.arg("file").c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  
  server.streamFile(file, "image/jpeg");
  file.close();
}

void handleAudio() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }
  
  File file = SD_MMC.open(server.arg("file").c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  
  server.streamFile(file, "audio/wav");
  file.close();
}

void handleListImages() {
  File root = SD_MMC.open("/images");
  String list = "{\"files\":[";
  if (root) {
    File file = root.openNextFile();
    bool first = true;
    while (file) {
      if (!file.isDirectory()) {
        if (!first) list += ",";
        list += "\"" + String(file.path()) + "\"";
        first = false;
      }
      file = root.openNextFile();
    }
  }
  list += "]}";
  server.send(200, "application/json", list);
}

void handleListAudio() {
  File root = SD_MMC.open("/audio");
  String list = "{\"files\":[";
  if (root) {
    File file = root.openNextFile();
    bool first = true;
    while (file) {
      if (!file.isDirectory()) {
        if (!first) list += ",";
        list += "\"" + String(file.path()) + "\"";
        first = false;
      }
      file = root.openNextFile();
    }
  }
  list += "]}";
  server.send(200, "application/json", list);
}

void handleAudioStart() {
  if (isRecording) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Already recording\"}");
    return;
  }
  
  isRecording = true;
  recordStartTime = millis();
  
  Serial.println("Recording started...");
  digitalWrite(3, HIGH);  // Turn on LED
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleAudioStop() {
  if (!isRecording) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Not recording\"}");
    return;
  }
  
  isRecording = false;
  digitalWrite(3, LOW);  // Turn off LED
  
  // Calculate recording time
  int recordTime = (millis() - recordStartTime) / 1000;
  if (recordTime < 1) recordTime = 1;
  if (recordTime > 60) recordTime = 60;
  
  Serial.printf("Recording for %d seconds...\n", recordTime);
  
  // Record audio using ESP_I2S library
  size_t wav_size = 0;
  uint8_t *wav_buffer = i2s.recordWAV(recordTime, &wav_size);
  
  if (wav_buffer == NULL || wav_size == 0) {
    Serial.println("Recording failed!");
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Recording failed\"}");
    return;
  }
  
  // Save to SD card
  audioCount++;
  latestAudio = "/audio/REC_" + String(audioCount) + ".wav";
  
  File file = SD_MMC.open(latestAudio.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    free(wav_buffer);
    server.send(200, "application/json", "{\"success\":false,\"message\":\"SD write failed\"}");
    return;
  }
  
  file.write(wav_buffer, wav_size);
  file.close();
  free(wav_buffer);
  
  Serial.printf("Audio saved: %s (%d bytes)\n", latestAudio.c_str(), wav_size);
  
  String response = "{\"success\":true,\"bytes\":" + String(wav_size) + "}";
  server.send(200, "application/json", response);
}

void handleDeleteImage() {
  if (!server.hasArg("file")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  
  String filename = server.arg("file");
  if (SD_MMC.remove(filename.c_str())) {
    Serial.printf("Deleted: %s\n", filename.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(200, "application/json", "{\"success\":false}");
  }
}

void handleDeleteAudio() {
  if (!server.hasArg("file")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  
  String filename = server.arg("file");
  if (SD_MMC.remove(filename.c_str())) {
    Serial.printf("Deleted: %s\n", filename.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(200, "application/json", "{\"success\":false}");
  }
}
