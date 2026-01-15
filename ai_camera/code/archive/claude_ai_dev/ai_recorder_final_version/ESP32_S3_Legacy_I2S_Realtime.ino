/*
 * ESP32-S3 AI CAM - Real-time PDM Recording
 * 
 * Uses legacy I2S driver for continuous real-time recording
 * Based on your working diagnostic (PDM mode on GPIO 38/39)
 */

#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <driver/i2s.h>

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

// PDM Microphone
#define PDM_CLK_PIN       38
#define PDM_DATA_PIN      39

// Audio settings
#define SAMPLE_RATE       16000
#define I2S_PORT          I2S_NUM_0
#define AMPLIFICATION     10

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
  uint32_t byteRate = SAMPLE_RATE * 2;
  uint16_t blockAlign = 2;
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize;
};

int imageCount = 0;
int audioCount = 0;
String latestAudio = "";
bool isRecording = false;
File audioFile;
uint32_t recordedBytes = 0;
int16_t peakLevel = 0;

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
            width: 100%; height: 100%;
            object-fit: cover;
        }
        .controls {
            display: flex;
            gap: 15px;
            flex-wrap: wrap;
        }
        button {
            flex: 1;
            min-width: 120px;
            padding: 12px 20px;
            font-size: 14px;
            font-weight: bold;
            border: none;
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s;
            text-transform: uppercase;
            color: white;
        }
        .btn-capture { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
        .btn-record { background: linear-gradient(135deg, #fa709a 0%, #fee140 100%); }
        .btn-record.recording {
            background: linear-gradient(135deg, #eb3349 0%, #f45c43 100%);
            animation: pulse 1.5s ease-in-out infinite;
        }
        .btn-stop { background: linear-gradient(135deg, #ff6b6b 0%, #ee5a6f 100%); }
        .btn-delete { background: linear-gradient(135deg, #ff6b6b 0%, #ee5a6f 100%); }
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
            margin-bottom: 10px;
        }
        .audio-name { font-weight: bold; margin-bottom: 10px; }
        audio { width: 100%; }
        .recording-timer {
            text-align: center;
            font-size: 32px;
            color: #eb3349;
            margin: 15px 0;
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
        .info { background: #d1ecf1; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📷 ESP32-S3 Media Recorder</h1>
            <p>Real-time PDM Audio • 10x Amplified</p>
        </div>
        
        <div class="content">
            <div id="status" class="status"></div>
            
            <div class="info">
                ⏺️ <strong>Real-time recording active!</strong> Audio captures while timer runs. Click stop anytime.
            </div>
            
            <div class="section">
                <h2>📸 Camera</h2>
                <div class="camera-view">
                    <img id="stream" src="/stream">
                </div>
                <div class="controls">
                    <button class="btn-capture" onclick="captureImage()">📷 Capture</button>
                </div>
            </div>
            
            <div class="section">
                <h2>🎤 Audio Recorder</h2>
                <div id="recordingTimer" class="recording-timer" style="display:none;">00:00</div>
                <div class="controls">
                    <button id="recordBtn" class="btn-record" onclick="startRecording()">⏺️ Record</button>
                    <button id="stopBtn" class="btn-stop" onclick="stopRecording()" disabled>⏹️ Stop</button>
                </div>
                <div id="audioList"></div>
            </div>
        </div>
    </div>
    
    <script>
        let recordingStartTime = 0;
        let timerInterval = null;
        
        function showStatus(msg, err = false) {
            const status = document.getElementById('status');
            status.textContent = msg;
            status.className = 'status show' + (err ? ' error' : '');
            setTimeout(() => status.classList.remove('show'), 3000);
        }
        
        function updateTimer() {
            const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
            const minutes = Math.floor(elapsed / 60);
            const seconds = elapsed % 60;
            document.getElementById('recordingTimer').textContent = 
                String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
        }
        
        function captureImage() {
            fetch('/capture')
                .then(r => r.json())
                .then(d => {
                    if (d.success) showStatus('✓ Image captured!');
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
                        document.getElementById('recordingTimer').style.display = 'block';
                        recordingStartTime = Date.now();
                        timerInterval = setInterval(updateTimer, 100);
                        showStatus('🔴 Recording...');
                    }
                });
        }
        
        function stopRecording() {
            clearInterval(timerInterval);
            document.getElementById('stopBtn').disabled = true;
            showStatus('⏳ Finalizing...');
            
            fetch('/audio/stop')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('recordBtn').disabled = false;
                    document.getElementById('stopBtn').disabled = false;
                    document.getElementById('recordBtn').classList.remove('recording');
                    document.getElementById('recordingTimer').style.display = 'none';
                    
                    if (d.success) {
                        const duration = (d.bytes / (16000 * 2)).toFixed(1);
                        showStatus('✓ Saved! ' + duration + 's, Peak: ' + d.peak);
                        loadAudioFiles();
                    } else {
                        showStatus('✗ Save failed', true);
                    }
                });
        }
        
        function loadAudioFiles() {
            fetch('/list/audio')
                .then(r => r.json())
                .then(d => {
                    const list = document.getElementById('audioList');
                    list.innerHTML = '';
                    
                    d.files.forEach(f => {
                        const item = document.createElement('div');
                        item.className = 'audio-item';
                        item.innerHTML = `
                            <div class="audio-name">${f.split('/').pop()}</div>
                            <audio controls>
                                <source src="/audio?file=${encodeURIComponent(f)}" type="audio/wav">
                            </audio>
                            <button class="btn-delete" onclick="deleteAudio('${f}')">🗑️</button>
                        `;
                        list.appendChild(item);
                    });
                });
        }
        
        function deleteAudio(f) {
            if (confirm('Delete?')) {
                fetch('/delete/audio?file=' + encodeURIComponent(f), {method: 'DELETE'})
                    .then(() => loadAudioFiles());
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

void initI2S_PDM() {
  Serial.println("Initializing I2S for PDM microphone...");
  
  // PDM configuration - matches your working diagnostic
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,  // Based on your test
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_PIN_NO_CHANGE,
    .ws_io_num = PDM_CLK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PDM_DATA_PIN
  };
  
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S: 0x%x\n", err);
    return;
  }
  
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: 0x%x\n", err);
    return;
  }
  
  i2s_zero_dma_buffer(I2S_PORT);
  Serial.printf("✓ PDM OK (GPIO %d/%d, %dHz, %dx amp)\n", PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE, AMPLIFICATION);
}

void amplifyAudioSample(int16_t* sample) {
  int32_t amplified = (int32_t)(*sample) * AMPLIFICATION;
  if (amplified > 32767) amplified = 32767;
  if (amplified < -32768) amplified = -32768;
  *sample = (int16_t)amplified;
}

void countExistingFiles() {
  File root = SD_MMC.open("/images");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String name = String(file.name());
        if (name.startsWith("IMG_")) {
          int num = name.substring(4, name.indexOf(".")).toInt();
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
        if (name.startsWith("REC_")) {
          int num = name.substring(4, name.indexOf(".")).toInt();
          if (num > audioCount) audioCount = num;
        }
      }
      file = root.openNextFile();
    }
  }
  
  Serial.printf("Found %d images, %d audio\n", imageCount, audioCount);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-S3 Real-time Recorder ===\n");
  
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);
  
  // SD Card
  Serial.println("→ SD Card...");
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (SD_MMC.begin("/sdcard", true)) {
    Serial.printf("  ✓ %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
    if (!SD_MMC.exists("/images")) SD_MMC.mkdir("/images");
    if (!SD_MMC.exists("/audio")) SD_MMC.mkdir("/audio");
    countExistingFiles();
  }
  
  // Camera
  Serial.println("\n→ Camera...");
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
  config.frame_size = FRAMESIZE_SVGA;
  
  if (esp_camera_init(&config) == ESP_OK) {
    Serial.println("  ✓ OK");
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
      s->set_vflip(s, 1);
      s->set_hmirror(s, 0);
    }
  }
  
  // PDM Microphone
  Serial.println("\n→ PDM Microphone...");
  initI2S_PDM();
  
  // WiFi
  Serial.println("\n→ WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n  ✓ http://%s\n", WiFi.localIP().toString().c_str());
  
  // Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/audio", HTTP_GET, handleAudio);
  server.on("/list/audio", HTTP_GET, handleListAudio);
  server.on("/audio/start", HTTP_GET, handleAudioStart);
  server.on("/audio/stop", HTTP_GET, handleAudioStop);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  
  server.begin();
  Serial.println("\n✓ Ready!\n");
}

void loop() {
  server.handleClient();
  
  // Real-time recording
  if (isRecording && audioFile) {
    const int bufferSize = 512;
    int16_t buffer[bufferSize];
    size_t bytesRead = 0;
    
    esp_err_t result = i2s_read(I2S_PORT, buffer, bufferSize * sizeof(int16_t), 
                                 &bytesRead, 10 / portTICK_PERIOD_MS);
    
    if (result == ESP_OK && bytesRead > 0) {
      int numSamples = bytesRead / sizeof(int16_t);
      
      // Amplify and track peak
      for (int i = 0; i < numSamples; i++) {
        int16_t absVal = abs(buffer[i]);
        if (absVal > peakLevel) peakLevel = absVal;
        amplifyAudioSample(&buffer[i]);
      }
      
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
    server.send(500, "text/plain", "Failed");
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
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  imageCount++;
  String filename = "/images/IMG_" + String(imageCount) + ".jpg";
  File file = SD_MMC.open(filename.c_str(), FILE_WRITE);
  
  if (file) {
    file.write(fb->buf, fb->len);
    file.close();
  }
  
  esp_camera_fb_return(fb);
  server.send(200, "application/json", "{\"success\":true}");
}

void handleAudio() {
  if (!server.hasArg("file")) return;
  File file = SD_MMC.open(server.arg("file").c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.streamFile(file, "audio/wav");
  file.close();
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
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  audioCount++;
  latestAudio = "/audio/REC_" + String(audioCount) + ".wav";
  
  audioFile = SD_MMC.open(latestAudio.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("Failed to open file");
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  // Write placeholder WAV header
  WAVHeader header;
  header.fileSize = 0;
  header.dataSize = 0;
  audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
  
  recordedBytes = 0;
  peakLevel = 0;
  isRecording = true;
  digitalWrite(3, HIGH);
  
  Serial.printf("REC START: %s\n", latestAudio.c_str());
  server.send(200, "application/json", "{\"success\":true}");
}

void handleAudioStop() {
  if (!isRecording) {
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  isRecording = false;
  digitalWrite(3, LOW);
  
  // Update WAV header
  WAVHeader header;
  header.fileSize = recordedBytes + sizeof(WAVHeader) - 8;
  header.dataSize = recordedBytes;
  
  audioFile.seek(0);
  audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
  audioFile.close();
  
  float duration = (float)recordedBytes / (SAMPLE_RATE * 2);
  int ampPeak = min(peakLevel * AMPLIFICATION, 32767);
  
  Serial.printf("REC STOP: %s\n", latestAudio.c_str());
  Serial.printf("  Duration: %.1fs (%d bytes)\n", duration, recordedBytes);
  Serial.printf("  Original peak: %d (%.1f%%)\n", peakLevel, (peakLevel * 100.0) / 32767);
  Serial.printf("  Amplified peak: %d (%.1f%%)\n", ampPeak, (ampPeak * 100.0) / 32767);
  
  String response = "{\"success\":true,\"bytes\":" + String(recordedBytes) + 
                    ",\"peak\":" + String(peakLevel) + "}";
  server.send(200, "application/json", response);
}

void handleDeleteAudio() {
  if (!server.hasArg("file")) return;
  String filename = server.arg("file");
  if (SD_MMC.remove(filename.c_str())) {
    Serial.printf("Deleted: %s\n", filename.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(200, "application/json", "{\"success\":false}");
  }
}
