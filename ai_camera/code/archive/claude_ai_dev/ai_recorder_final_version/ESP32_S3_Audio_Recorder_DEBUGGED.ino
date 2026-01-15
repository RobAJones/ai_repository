/*
 * ESP32-S3 AI CAM - Camera & Audio Recorder (DEBUGGED FOR AUDIO)
 * Hardware: DFRobot SKU DFR1154 ESP32-S3 AI CAM
 * 
 * AUDIO FIX:
 * - Improved I2S PDM configuration for proper microphone recording
 * - Added debugging to verify audio data is being captured
 * - Fixed buffer sizes and timing
 * 
 * Note: PDM Microphone on GPIO 38 (CLK) and 39 (DATA)
 *       MAX98357 is for PLAYBACK only (not recording)
 */

#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <driver/i2s.h>

// WiFi credentials
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
#define SD_MMC_CS         10

// PDM Microphone pins (for RECORDING)
#define PDM_CLK_PIN       38
#define PDM_DATA_PIN      39

// Audio configuration - OPTIMIZED FOR PDM
#define SAMPLE_RATE       16000
#define SAMPLE_BITS       I2S_BITS_PER_SAMPLE_16BIT
#define I2S_PORT          I2S_NUM_0
#define DMA_BUF_COUNT     8
#define DMA_BUF_LEN       1024

// Global variables
int imageCount = 0;
int audioCount = 0;
String latestImage = "";
String latestAudio = "";
bool isRecording = false;
File audioFile;
uint32_t recordedBytes = 0;

// WAV header
struct WAVHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = 1;
  uint32_t sampleRate = SAMPLE_RATE;
  uint32_t byteRate = SAMPLE_RATE * 1 * 2;  // 16-bit mono
  uint16_t blockAlign = 2;
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize;
};

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-S3 Recorder</title>
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
        }
        .btn-capture {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .btn-review {
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
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
            min-width: 60px;
            padding: 8px 12px;
            font-size: 12px;
        }
        button:hover:not(:disabled) {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(0,0,0,0.2);
        }
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
            margin-bottom: 10px;
        }
        .audio-name { font-weight: bold; color: #333; }
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
        }
        .gallery-item img {
            position: absolute;
            width: 100%; height: 100%;
            object-fit: cover;
            cursor: pointer;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📷 ESP32-S3 Recorder</h1>
            <p>Camera & Audio System</p>
        </div>
        
        <div class="content">
            <div id="status" class="status"></div>
            
            <div class="section">
                <h2>📸 Camera</h2>
                <div class="camera-view">
                    <img id="stream" src="/stream">
                </div>
                <div class="controls">
                    <button class="btn-capture" onclick="captureImage()">📷 Capture</button>
                    <button class="btn-review" onclick="loadImageFiles()">🖼️ Review</button>
                </div>
            </div>
            
            <div class="section">
                <h2>🎤 Audio Recorder</h2>
                <div class="controls">
                    <button id="recordBtn" class="btn-record" onclick="startRecording()">⏺️ Record</button>
                    <button id="stopBtn" class="btn-stop" onclick="stopRecording()" disabled>⏹️ Stop</button>
                </div>
                <div id="audioList"></div>
            </div>
            
            <div id="imageGallery" class="gallery-grid"></div>
        </div>
    </div>
    
    <script>
        function showStatus(msg, err = false) {
            const status = document.getElementById('status');
            status.textContent = msg;
            status.className = 'status show' + (err ? ' error' : '');
            setTimeout(() => status.classList.remove('show'), 3000);
        }
        
        function captureImage() {
            fetch('/capture')
                .then(r => r.json())
                .then(d => {
                    if (d.success) {
                        showStatus('Image captured!');
                        document.getElementById('stream').src = '/stream?' + Date.now();
                    } else {
                        showStatus('Capture failed: ' + d.message, true);
                    }
                });
        }
        
        function startRecording() {
            fetch('/audio/start')
                .then(r => r.json())
                .then(d => {
                    if (d.success) {
                        document.getElementById('recordBtn').disabled = true;
                        document.getElementById('stopBtn').disabled = false;
                        document.getElementById('recordBtn').classList.add('recording');
                        showStatus('Recording...');
                    } else {
                        showStatus('Start failed: ' + d.message, true);
                    }
                });
        }
        
        function stopRecording() {
            fetch('/audio/stop')
                .then(r => r.json())
                .then(d => {
                    if (d.success) {
                        document.getElementById('recordBtn').disabled = false;
                        document.getElementById('stopBtn').disabled = true;
                        document.getElementById('recordBtn').classList.remove('recording');
                        showStatus('Saved: ' + d.bytes + ' bytes');
                        loadAudioFiles();
                    } else {
                        showStatus('Stop failed: ' + d.message, true);
                    }
                });
        }
        
        function loadImageFiles() {
            fetch('/list/images')
                .then(r => r.json())
                .then(d => {
                    const gallery = document.getElementById('imageGallery');
                    gallery.innerHTML = '';
                    d.files.forEach(f => {
                        const div = document.createElement('div');
                        div.className = 'gallery-item';
                        div.innerHTML = '<img src="/image?file=' + encodeURIComponent(f) + '">';
                        gallery.appendChild(div);
                    });
                });
        }
        
        function loadAudioFiles() {
            fetch('/list/audio')
                .then(r => r.json())
                .then(d => {
                    const list = document.getElementById('audioList');
                    list.innerHTML = '';
                    d.files.forEach(f => {
                        const div = document.createElement('div');
                        div.className = 'audio-item';
                        div.innerHTML = `
                            <div>
                                <div class="audio-name">${f.split('/').pop()}</div>
                                <audio controls>
                                    <source src="/audio?file=${encodeURIComponent(f)}" type="audio/wav">
                                </audio>
                            </div>
                            <button class="btn-delete" onclick="deleteAudio('${f}')">🗑️</button>
                        `;
                        list.appendChild(div);
                    });
                });
        }
        
        function deleteAudio(f) {
            if (confirm('Delete?')) {
                fetch('/delete/audio?file=' + encodeURIComponent(f), {method: 'DELETE'})
                    .then(r => r.json())
                    .then(d => {
                        if (d.success) loadAudioFiles();
                    });
            }
        }
        
        setInterval(() => {
            document.getElementById('stream').src = '/stream?' + Date.now();
        }, 100);
        
        loadAudioFiles();
    </script>
</body>
</html>
)rawliteral";

void initI2S() {
  Serial.println("Initializing I2S for PDM microphone...");
  
  // Uninstall if already installed
  i2s_driver_uninstall(I2S_PORT);
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,  // Try RIGHT channel for PDM
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("I2S driver install FAILED: 0x%x\n", err);
    return;
  }
  Serial.println("I2S driver installed");
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_PIN_NO_CHANGE,
    .ws_io_num = PDM_CLK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PDM_DATA_PIN
  };
  
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("I2S set pins FAILED: 0x%x\n", err);
  } else {
    Serial.println("I2S pins configured: CLK=38, DATA=39");
  }
  
  // Clear DMA buffers
  i2s_zero_dma_buffer(I2S_PORT);
  
  Serial.println("PDM Microphone initialized");
}

void testMicrophone() {
  Serial.println("\n=== TESTING MICROPHONE ===");
  const int testSamples = 100;
  int16_t buffer[testSamples];
  size_t bytesRead;
  
  esp_err_t result = i2s_read(I2S_PORT, buffer, testSamples * sizeof(int16_t), &bytesRead, 1000);
  
  Serial.printf("I2S Read Result: %d\n", result);
  Serial.printf("Bytes Read: %d\n", bytesRead);
  
  if (bytesRead > 0) {
    int16_t minVal = 32767, maxVal = -32768;
    int nonZero = 0;
    
    for (int i = 0; i < bytesRead / 2; i++) {
      if (buffer[i] != 0) nonZero++;
      if (buffer[i] < minVal) minVal = buffer[i];
      if (buffer[i] > maxVal) maxVal = buffer[i];
    }
    
    Serial.printf("Non-zero samples: %d/%d\n", nonZero, bytesRead/2);
    Serial.printf("Range: %d to %d\n", minVal, maxVal);
    Serial.printf("First 10 samples: ");
    for (int i = 0; i < 10 && i < bytesRead/2; i++) {
      Serial.printf("%d ", buffer[i]);
    }
    Serial.println();
    
    if (nonZero > 0) {
      Serial.println("✓ Microphone is working!");
    } else {
      Serial.println("✗ All zeros - mic may not be working");
    }
  } else {
    Serial.println("✗ No data read from microphone");
  }
  Serial.println("=========================\n");
}

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
  Serial.println("\n\n=== ESP32-S3 Media Recorder ===");
  
  // SD Card with custom pins
  Serial.println("Initializing SD Card...");
  if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
    Serial.println("SD pin config failed!");
  }
  
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Mount Failed!");
  } else {
    Serial.printf("SD OK: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
    
    if (!SD_MMC.exists("/images")) SD_MMC.mkdir("/images");
    if (!SD_MMC.exists("/audio")) SD_MMC.mkdir("/audio");
    
    countExistingFiles();
  }
  
  // Camera
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
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.frame_size = psramFound() ? FRAMESIZE_SVGA : FRAMESIZE_VGA;
  
  if (esp_camera_init(&config) == ESP_OK) {
    Serial.println("Camera OK");
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
      s->set_vflip(s, 1);
      s->set_hmirror(s, 0);
    }
  } else {
    Serial.println("Camera FAILED");
  }
  
  // Initialize I2S for PDM mic
  initI2S();
  
  // Test the microphone
  delay(500);  // Let it stabilize
  testMicrophone();
  
  // WiFi
  Serial.print("WiFi connecting");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected: http://%s\n", WiFi.localIP().toString().c_str());
  
  // Web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/image", HTTP_GET, handleImage);
  server.on("/audio", HTTP_GET, handleAudio);
  server.on("/list/images", HTTP_GET, handleListImages);
  server.on("/list/audio", HTTP_GET, handleListAudio);
  server.on("/audio/start", HTTP_GET, handleAudioStart);
  server.on("/audio/stop", HTTP_GET, handleAudioStop);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  
  server.begin();
  Serial.println("Server started\n");
}

void loop() {
  server.handleClient();
  
  // Continuous audio recording
  if (isRecording && audioFile) {
    const int bufferSize = 512;
    int16_t buffer[bufferSize];
    size_t bytesRead = 0;
    
    esp_err_t result = i2s_read(I2S_PORT, buffer, bufferSize * sizeof(int16_t), 
                                 &bytesRead, 10 / portTICK_PERIOD_MS);
    
    if (result == ESP_OK && bytesRead > 0) {
      audioFile.write((uint8_t*)buffer, bytesRead);
      recordedBytes += bytesRead;
    }
  }
}

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleStream() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Capture failed");
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
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Failed\"}");
    return;
  }
  
  imageCount++;
  String filename = "/images/IMG_" + String(imageCount) + ".jpg";
  File file = SD_MMC.open(filename.c_str(), FILE_WRITE);
  
  if (file) {
    file.write(fb->buf, fb->len);
    file.close();
    esp_camera_fb_return(fb);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    esp_camera_fb_return(fb);
    server.send(200, "application/json", "{\"success\":false,\"message\":\"SD write failed\"}");
  }
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
  
  audioCount++;
  latestAudio = "/audio/REC_" + String(audioCount) + ".wav";
  
  audioFile = SD_MMC.open(latestAudio.c_str(), FILE_WRITE);
  if (!audioFile) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"SD file failed\"}");
    return;
  }
  
  // Write WAV header
  WAVHeader header;
  header.fileSize = 0;
  header.dataSize = 0;
  audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
  
  recordedBytes = 0;
  isRecording = true;
  
  Serial.printf("Recording started: %s\n", latestAudio.c_str());
  server.send(200, "application/json", "{\"success\":true}");
}

void handleAudioStop() {
  if (!isRecording) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Not recording\"}");
    return;
  }
  
  isRecording = false;
  
  // Update WAV header
  WAVHeader header;
  header.fileSize = recordedBytes + sizeof(WAVHeader) - 8;
  header.dataSize = recordedBytes;
  
  audioFile.seek(0);
  audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
  audioFile.close();
  
  Serial.printf("Recording stopped: %s (%d bytes)\n", latestAudio.c_str(), recordedBytes);
  
  String response = "{\"success\":true,\"filename\":\"" + latestAudio + "\",\"bytes\":" + String(recordedBytes) + "}";
  server.send(200, "application/json", response);
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
