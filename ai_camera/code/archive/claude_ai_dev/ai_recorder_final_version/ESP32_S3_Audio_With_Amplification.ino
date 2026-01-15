/*
 * ESP32-S3 AI CAM - Camera & Audio Recorder
 * 
 * With audio level diagnostics and amplification option
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

// PDM Microphone pins
#define DATA_PIN          GPIO_NUM_39
#define CLOCK_PIN         GPIO_NUM_38

// MAX98357 Amplifier pins (for playback testing)
#define MAX98357_BCLK     45
#define MAX98357_LRC      46
#define MAX98357_DIN      42

// Audio configuration
#define SAMPLE_RATE       16000
#define AMPLIFICATION     4  // Increase volume by this factor (2-8 is typical)

// Global variables
int imageCount = 0;
int audioCount = 0;
String latestImage = "";
String latestAudio = "";
bool isRecording = false;
unsigned long recordStartTime = 0;

I2SClass i2s;    // For recording (PDM)
I2SClass i2s1;   // For playback (MAX98357)

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
        .btn-play { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); }
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
        .audio-controls { display: flex; gap: 10px; margin-top: 10px; }
        audio { width: 100%; margin-top: 10px; }
        .recording-timer {
            text-align: center;
            font-size: 24px;
            color: #eb3349;
            margin: 10px 0;
            font-weight: bold;
        }
        .info-box {
            background: #fff3cd;
            border-left: 4px solid #ffc107;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📷 ESP32-S3 Media Recorder</h1>
            <p>PDM Recording with Volume Boost</p>
        </div>
        
        <div class="content">
            <div id="status" class="status"></div>
            
            <div class="info-box">
                <strong>Audio Info:</strong> Volume amplification is ${AMPLIFICATION}x. 
                Test playback through MAX98357 speaker to verify levels.
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
                    if (d.success) showStatus('Image captured!');
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
                        showStatus('Recording...');
                    }
                });
        }
        
        function stopRecording() {
            clearInterval(timerInterval);
            document.getElementById('recordingTimer').style.display = 'none';
            showStatus('Processing...');
            
            fetch('/audio/stop')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('recordBtn').disabled = false;
                    document.getElementById('stopBtn').disabled = true;
                    document.getElementById('recordBtn').classList.remove('recording');
                    
                    if (d.success) {
                        showStatus('Saved: ' + d.bytes + ' bytes (Peak: ' + d.peak + ')');
                        loadAudioFiles();
                    }
                });
        }
        
        function playOnSpeaker(file) {
            showStatus('Playing on MAX98357 speaker...');
            fetch('/audio/play?file=' + encodeURIComponent(file))
                .then(r => r.json())
                .then(d => {
                    if (d.success) {
                        showStatus('Playback complete');
                    } else {
                        showStatus('Playback failed', true);
                    }
                });
        }
        
        function loadAudioFiles() {
            fetch('/list/audio')
                .then(r => r.json())
                .then(d => {
                    const list = document.getElementById('audioList');
                    list.innerHTML = '';
                    
                    if (d.files.length === 0) return;
                    
                    d.files.forEach(f => {
                        const item = document.createElement('div');
                        item.className = 'audio-item';
                        item.innerHTML = `
                            <div class="audio-name">${f.split('/').pop()}</div>
                            <audio controls>
                                <source src="/audio?file=${encodeURIComponent(f)}" type="audio/wav">
                            </audio>
                            <div class="audio-controls">
                                <button class="btn-play" onclick="playOnSpeaker('${f}')">🔊 Play on Speaker</button>
                                <button class="btn-delete" onclick="deleteAudio('${f}')">🗑️ Delete</button>
                            </div>
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

// Amplify audio samples to increase volume
void amplifyAudio(uint8_t* buffer, size_t size, int gain) {
  int16_t* samples = (int16_t*)buffer;
  size_t numSamples = size / 2;
  
  for (size_t i = 0; i < numSamples; i++) {
    int32_t amplified = (int32_t)samples[i] * gain;
    
    // Clamp to prevent distortion
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;
    
    samples[i] = (int16_t)amplified;
  }
}

// Analyze audio levels
void analyzeAudioLevels(uint8_t* buffer, size_t size, int16_t* peak, int16_t* avg) {
  int16_t* samples = (int16_t*)buffer;
  size_t numSamples = size / 2;
  
  int16_t maxVal = 0;
  int64_t sum = 0;
  
  for (size_t i = 0; i < numSamples; i++) {
    int16_t absVal = abs(samples[i]);
    if (absVal > maxVal) maxVal = absVal;
    sum += absVal;
  }
  
  *peak = maxVal;
  *avg = (numSamples > 0) ? (sum / numSamples) : 0;
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
  
  Serial.printf("Found %d images, %d audio files\n", imageCount, audioCount);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-S3 Media Recorder ===");
  Serial.printf("Audio amplification: %dx\n\n", AMPLIFICATION);
  
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
  
  // PDM Microphone (recording)
  Serial.println("\n→ PDM Microphone (recording)...");
  i2s.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("  Failed to initialize");
  } else {
    Serial.printf("  ✓ OK (GPIO %d/%d)\n", CLOCK_PIN, DATA_PIN);
  }
  
  // MAX98357 Amplifier (playback)
  Serial.println("\n→ MAX98357 Speaker (playback)...");
  i2s1.setPins(MAX98357_BCLK, MAX98357_LRC, MAX98357_DIN);
  if (!i2s1.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("  Failed to initialize");
  } else {
    Serial.printf("  ✓ OK (GPIO %d/%d/%d)\n", MAX98357_BCLK, MAX98357_LRC, MAX98357_DIN);
  }
  
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
  server.on("/image", HTTP_GET, handleImage);
  server.on("/audio", HTTP_GET, handleAudio);
  server.on("/list/audio", HTTP_GET, handleListAudio);
  server.on("/audio/start", HTTP_GET, handleAudioStart);
  server.on("/audio/stop", HTTP_GET, handleAudioStop);
  server.on("/audio/play", HTTP_GET, handleAudioPlay);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  
  server.begin();
  Serial.println("\n✓ Ready!\n");
}

void loop() {
  server.handleClient();
  
  if (isRecording && (millis() - recordStartTime > 60000)) {
    isRecording = false;
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

void handleImage() {
  if (!server.hasArg("file")) return;
  File file = SD_MMC.open(server.arg("file").c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.streamFile(file, "image/jpeg");
  file.close();
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
  
  isRecording = true;
  recordStartTime = millis();
  digitalWrite(3, HIGH);
  
  Serial.println("Recording started...");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleAudioStop() {
  if (!isRecording) {
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  isRecording = false;
  digitalWrite(3, LOW);
  
  int recordTime = (millis() - recordStartTime) / 1000;
  if (recordTime < 1) recordTime = 1;
  if (recordTime > 60) recordTime = 60;
  
  Serial.printf("Recording for %d seconds...\n", recordTime);
  
  // Record audio
  size_t wav_size = 0;
  uint8_t *wav_buffer = i2s.recordWAV(recordTime, &wav_size);
  
  if (wav_buffer == NULL || wav_size == 0) {
    Serial.println("Recording failed!");
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  // Analyze audio levels BEFORE amplification
  int16_t peak = 0, avg = 0;
  
  // Skip WAV header (44 bytes) and analyze actual audio data
  if (wav_size > 44) {
    analyzeAudioLevels(wav_buffer + 44, wav_size - 44, &peak, &avg);
    Serial.printf("Audio levels - Peak: %d, Avg: %d (max is 32767)\n", peak, avg);
    
    // Amplify if needed (skip WAV header)
    if (AMPLIFICATION > 1) {
      amplifyAudio(wav_buffer + 44, wav_size - 44, AMPLIFICATION);
      Serial.printf("Audio amplified by %dx\n", AMPLIFICATION);
    }
  }
  
  // Save to SD
  audioCount++;
  latestAudio = "/audio/REC_" + String(audioCount) + ".wav";
  
  File file = SD_MMC.open(latestAudio.c_str(), FILE_WRITE);
  if (!file) {
    free(wav_buffer);
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  file.write(wav_buffer, wav_size);
  file.close();
  free(wav_buffer);
  
  Serial.printf("Saved: %s (%d bytes)\n", latestAudio.c_str(), wav_size);
  
  String response = "{\"success\":true,\"bytes\":" + String(wav_size) + 
                    ",\"peak\":" + String(peak) + "}";
  server.send(200, "application/json", response);
}

void handleAudioPlay() {
  if (!server.hasArg("file")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }
  
  String filename = server.arg("file");
  File file = SD_MMC.open(filename.c_str(), FILE_READ);
  
  if (!file) {
    server.send(404, "application/json", "{\"success\":false,\"message\":\"File not found\"}");
    return;
  }
  
  size_t fileSize = file.size();
  uint8_t* buffer = (uint8_t*)malloc(fileSize);
  
  if (!buffer) {
    file.close();
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Memory allocation failed\"}");
    return;
  }
  
  file.read(buffer, fileSize);
  file.close();
  
  Serial.printf("Playing: %s (%d bytes)\n", filename.c_str(), fileSize);
  
  // Play on MAX98357 speaker
  i2s1.playWAV(buffer, fileSize);
  
  free(buffer);
  
  Serial.println("Playback complete");
  server.send(200, "application/json", "{\"success\":true}");
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
