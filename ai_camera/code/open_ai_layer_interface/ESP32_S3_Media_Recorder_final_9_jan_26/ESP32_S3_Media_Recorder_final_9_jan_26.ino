/*
 * ESP32-S3 AI CAM - Complete Media Recorder with Playback
 * 
 * Features:
 * - Camera capture and streaming
 * - PDM audio recording (10x amplification)
 * - Three playback options:
 *   1. Browser audio player (streams from SD)
 *   2. Download WAV file
 *   3. Play through MAX98357A speaker
 * 
 * Note: Recording and playback are NOT real-time (blocking operations)
 * Confgure the IDE for the following Parameters
 * Use ESP32s3 Dev Module
 * Set the following the tools tab
 * USB CDC on Boot "enabled"
 * Flash Size "16MB (128Mb)
 * PartitionScheme "16M Flash (3MB APP/9.9MBFATFA)"
 * PSRAM "OPI PSRAM"
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

// PDM Microphone (Recording)
#define PDM_DATA_PIN      GPIO_NUM_39
#define PDM_CLOCK_PIN     GPIO_NUM_38

// MAX98357A I2S Amplifier (Playback)
#define I2S_BCLK          45
#define I2S_LRC           46
#define I2S_DOUT          42
#define I2S_SD_PIN        41  // Shutdown pin (LOW = shutdown, HIGH = enabled)

#define SAMPLE_RATE       16000
#define AMPLIFICATION     10

int imageCount = 0;
int audioCount = 0;
String latestAudio = "";
bool isRecording = false;
bool isPlayingOnSpeaker = false;
unsigned long recordStartTime = 0;

I2SClass i2s_mic;   // For recording (PDM)
I2SClass i2s_spk;   // For playback (standard I2S)

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
            gap: 10px;
            flex-wrap: wrap;
        }
        button {
            padding: 10px 16px;
            font-size: 13px;
            font-weight: bold;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.3s;
            text-transform: uppercase;
            color: white;
        }
        .btn-capture { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
        .btn-record { background: linear-gradient(135deg, #fa709a 0%, #fee140 100%); flex: 1; min-width: 120px; }
        .btn-record.recording {
            background: linear-gradient(135deg, #eb3349 0%, #f45c43 100%);
            animation: pulse 1.5s ease-in-out infinite;
        }
        .btn-stop { background: linear-gradient(135deg, #ff6b6b 0%, #ee5a6f 100%); flex: 1; min-width: 120px; }
        .btn-play { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); }
        .btn-download { background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%); }
        .btn-delete { background: linear-gradient(135deg, #ff6b6b 0%, #ee5a6f 100%); }
        .btn-speaker { 
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); 
            font-size: 16px;
            padding: 12px 20px;
        }
        .btn-speaker.playing {
            background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
            animation: pulse 1.5s ease-in-out infinite;
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
            background: #f5f5f5;
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 15px;
            border: 2px solid #e0e0e0;
        }
        .audio-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }
        .audio-name { 
            font-weight: bold; 
            color: #333;
            font-size: 14px;
        }
        .audio-player { margin: 10px 0; }
        audio { 
            width: 100%; 
            height: 35px;
        }
        .audio-actions {
            display: flex;
            gap: 8px;
            margin-top: 10px;
        }
        .recording-timer {
            text-align: center;
            font-size: 32px;
            color: #eb3349;
            margin: 15px 0;
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
        .processing-message {
            text-align: center;
            font-size: 18px;
            color: #667eea;
            margin: 15px 0;
            font-weight: bold;
        }
        .info { 
            background: #fff3cd; 
            padding: 15px; 
            border-radius: 5px; 
            margin-bottom: 20px;
            border-left: 4px solid #ffc107;
            font-size: 13px;
        }
        .speaker-notice {
            background: #e7f3ff;
            padding: 12px;
            border-radius: 5px;
            margin-top: 10px;
            border-left: 4px solid #2196f3;
            font-size: 12px;
        }
        .image-item {
            background: #f5f5f5;
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 15px;
            border: 2px solid #e0e0e0;
        }
        .image-preview {
            width: 100%;
            border-radius: 8px;
            margin-bottom: 10px;
            cursor: pointer;
        }
        .image-actions {
            display: flex;
            gap: 8px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ESP32-S3 Media Recorder</h1>
            <p>Camera &bull; Audio Recording &bull; Speaker Playback</p>
        </div>
        
        <div class="content">
            <div id="status" class="status"></div>
            
            <div class="info">
                <strong>INFO:</strong> Audio Recording - Click Record to start timer. Click Stop when done - 
                device will then capture that duration of audio (blocking operation, please wait). 
                Audio is amplified 10x for optimal volume.
            </div>
            
            <div class="section">
                <h2>&#9654; Camera</h2>
                <div class="camera-view">
                    <img id="stream" src="/stream">
                </div>
                <div class="controls">
                    <button class="btn-capture" onclick="captureImage()">&#9632; Capture Photo</button>
                </div>
                <div id="imageGallery"></div>
            </div>
            
            <div class="section">
                <h2>&#9834; Audio Recorder</h2>
                <div id="recordingTimer" class="recording-timer" style="display:none;">00:00</div>
                <div id="processingMsg" class="processing-message" style="display:none;">Processing audio...</div>
                <div class="controls">
                    <button id="recordBtn" class="btn-record" onclick="startRecording()">&#9679; Record</button>
                    <button id="stopBtn" class="btn-stop" onclick="stopRecording()" disabled>&#9632; Stop</button>
                </div>
                
                <div class="speaker-notice">
                    <strong>Speaker Playback:</strong> Click the speaker button on any recording to play 
                    through the onboard MAX98357A amplifier. Playback is blocking (device processes the entire 
                    file). Browser playback and download are also available.
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
            setTimeout(() => status.classList.remove('show'), 4000);
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
                    if (d.success) {
                        showStatus('Image captured!');
                        loadImageGallery();
                    }
                });
        }
        
        function loadImageGallery() {
            fetch('/list/images')
                .then(r => r.json())
                .then(d => {
                    const gallery = document.getElementById('imageGallery');
                    gallery.innerHTML = '';
                    
                    if (d.files.length === 0) {
                        return;
                    }
                    
                    gallery.innerHTML = '<h3 style="margin: 20px 0 10px 0; color: #333;">Captured Images</h3>';
                    
                    d.files.forEach((f) => {
                        const item = document.createElement('div');
                        item.className = 'image-item';
                        item.innerHTML = `
                            <img class="image-preview" src="/image?file=${encodeURIComponent(f)}" 
                                 onclick="window.open('/image?file=${encodeURIComponent(f)}', '_blank')">
                            <div class="image-actions">
                                <button class="btn-download" onclick="downloadImage('${f}')">Download</button>
                                <button class="btn-delete" onclick="deleteImage('${f}')">Delete</button>
                            </div>
                        `;
                        gallery.appendChild(item);
                    });
                });
        }
        
        function downloadImage(file) {
            window.location.href = '/image/download?file=' + encodeURIComponent(file);
            showStatus('Downloading image...');
        }
        
        function deleteImage(file) {
            if (confirm('Delete this image?')) {
                fetch('/delete/image?file=' + encodeURIComponent(file), {method: 'DELETE'})
                    .then(() => {
                        showStatus('Deleted');
                        loadImageGallery();
                    });
            }
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
                        showStatus('REC - Timer started');
                    }
                });
        }
        
        function stopRecording() {
            clearInterval(timerInterval);
            const duration = Math.floor((Date.now() - recordingStartTime) / 1000);
            
            document.getElementById('stopBtn').disabled = true;
            document.getElementById('recordingTimer').style.display = 'none';
            document.getElementById('processingMsg').style.display = 'block';
            
            showStatus('Recording ' + duration + ' seconds of audio...');
            
            fetch('/audio/stop')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('recordBtn').disabled = false;
                    document.getElementById('stopBtn').disabled = false;
                    document.getElementById('recordBtn').classList.remove('recording');
                    document.getElementById('processingMsg').style.display = 'none';
                    
                    if (d.success) {
                        showStatus('Saved! Peak: ' + d.peak);
                        loadAudioFiles();
                    } else {
                        showStatus('Recording failed', true);
                    }
                });
        }
        
        function playSpeaker(file, btnId) {
            const btn = document.getElementById(btnId);
            btn.disabled = true;
            btn.classList.add('playing');
            btn.textContent = 'Playing...';
            
            showStatus('Playing through speaker...');
            
            fetch('/audio/play?file=' + encodeURIComponent(file))
                .then(r => r.json())
                .then(d => {
                    btn.disabled = false;
                    btn.classList.remove('playing');
                    btn.textContent = 'Speaker';
                    
                    if (d.success) {
                        showStatus('Playback complete');
                    } else {
                        showStatus('Playback failed', true);
                    }
                });
        }
        
        function downloadAudio(file) {
            window.location.href = '/audio/download?file=' + encodeURIComponent(file);
            showStatus('Downloading...');
        }
        
        function loadAudioFiles() {
            fetch('/list/audio')
                .then(r => r.json())
                .then(d => {
                    const list = document.getElementById('audioList');
                    list.innerHTML = '';
                    
                    if (d.files.length === 0) {
                        list.innerHTML = '<p style="text-align:center;color:#999;padding:20px;">No recordings yet</p>';
                        return;
                    }
                    
                    d.files.forEach((f, idx) => {
                        const btnId = 'speakerBtn' + idx;
                        const item = document.createElement('div');
                        item.className = 'audio-item';
                        item.innerHTML = `
                            <div class="audio-header">
                                <div class="audio-name">&#9834; ${f.split('/').pop()}</div>
                            </div>
                            <div class="audio-player">
                                <audio controls>
                                    <source src="/audio?file=${encodeURIComponent(f)}" type="audio/wav">
                                </audio>
                            </div>
                            <div class="audio-actions">
                                <button id="${btnId}" class="btn-speaker" onclick="playSpeaker('${f}', '${btnId}')">Speaker</button>
                                <button class="btn-download" onclick="downloadAudio('${f}')">Download</button>
                                <button class="btn-delete" onclick="deleteAudio('${f}')">Delete</button>
                            </div>
                        `;
                        list.appendChild(item);
                    });
                });
        }
        
        function deleteAudio(f) {
            if (confirm('Delete this recording?')) {
                fetch('/delete/audio?file=' + encodeURIComponent(f), {method: 'DELETE'})
                    .then(() => {
                        showStatus('Deleted');
                        loadAudioFiles();
                    });
            }
        }
        
        setInterval(() => {
            document.getElementById('stream').src = '/stream?' + Date.now();
        }, 100);
        
        loadImageGallery();
        loadAudioFiles();
    </script>
</body>
</html>
)rawliteral";

void amplifyAudio(uint8_t* buffer, size_t size, int gain) {
  int16_t* samples = (int16_t*)buffer;
  size_t numSamples = size / 2;
  
  for (size_t i = 0; i < numSamples; i++) {
    int32_t amplified = (int32_t)samples[i] * gain;
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;
    samples[i] = (int16_t)amplified;
  }
}

int16_t getPeakLevel(uint8_t* buffer, size_t size) {
  int16_t* samples = (int16_t*)buffer;
  size_t numSamples = size / 2;
  int16_t peak = 0;
  
  for (size_t i = 0; i < numSamples; i++) {
    int16_t absVal = abs(samples[i]);
    if (absVal > peak) peak = absVal;
  }
  
  return peak;
}

void initSpeaker() {
  pinMode(I2S_SD_PIN, OUTPUT);
  digitalWrite(I2S_SD_PIN, LOW);  // Start with speaker disabled
  
  i2s_spk.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if (!i2s_spk.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("  Speaker init failed");
  } else {
    Serial.printf("  ✓ Speaker (GPIO %d/%d/%d)\n", I2S_BCLK, I2S_LRC, I2S_DOUT);
  }
}

void playWAVOnSpeaker(String filename) {
  Serial.printf("Playing on speaker: %s\n", filename.c_str());
  
  File file = SD_MMC.open(filename.c_str(), FILE_READ);
  if (!file) {
    Serial.println("  Failed to open file");
    return;
  }
  
  // Skip WAV header (44 bytes)
  file.seek(44);
  
  // Enable speaker
  digitalWrite(I2S_SD_PIN, HIGH);
  delay(10);
  
  // Stream audio data to speaker
  const int bufferSize = 1024;
  uint8_t buffer[bufferSize];
  size_t totalBytes = 0;
  
  while (file.available()) {
    size_t bytesRead = file.read(buffer, bufferSize);
    if (bytesRead > 0) {
      i2s_spk.write(buffer, bytesRead);
      totalBytes += bytesRead;
    }
  }
  
  delay(100);  // Let last samples finish playing
  
  // Disable speaker
  digitalWrite(I2S_SD_PIN, LOW);
  
  file.close();
  
  float duration = (float)totalBytes / (SAMPLE_RATE * 2);
  Serial.printf("  Played %.1f seconds (%d bytes)\n", duration, totalBytes);
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
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 Media Recorder with Playback ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
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
    Serial.println("  ✓ Camera OK");
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
      s->set_vflip(s, 1);
      s->set_hmirror(s, 0);
    }
  }
  
  // PDM Microphone
  Serial.println("\n→ PDM Microphone...");
  i2s_mic.setPinsPdmRx(PDM_CLOCK_PIN, PDM_DATA_PIN);
  if (!i2s_mic.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("  Failed");
  } else {
    Serial.printf("  ✓ PDM Mic (GPIO %d/%d, %dHz, %dx amp)\n", 
                  PDM_CLOCK_PIN, PDM_DATA_PIN, SAMPLE_RATE, AMPLIFICATION);
  }
  
  // Speaker (MAX98357A)
  Serial.println("\n→ MAX98357A Speaker...");
  initSpeaker();
  
  // WiFi
  Serial.println("\n→ WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n  ✓ http://%s\n", WiFi.localIP().toString().c_str());
  
  // Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/image", HTTP_GET, handleImageView);
  server.on("/image/download", HTTP_GET, handleImageDownload);
  server.on("/list/images", HTTP_GET, handleListImages);
  server.on("/delete/image", HTTP_DELETE, handleDeleteImage);
  server.on("/audio", HTTP_GET, handleAudioStream);
  server.on("/audio/download", HTTP_GET, handleAudioDownload);
  server.on("/audio/play", HTTP_GET, handleAudioPlay);
  server.on("/list/audio", HTTP_GET, handleListAudio);
  server.on("/audio/start", HTTP_GET, handleAudioStart);
  server.on("/audio/stop", HTTP_GET, handleAudioStop);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  
  server.begin();
  Serial.println("\n✓ System Ready!\n");
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
    Serial.printf("Saved: %s\n", filename.c_str());
  }
  
  esp_camera_fb_return(fb);
  server.send(200, "application/json", "{\"success\":true}");
}

void handleImageView() {
  if (!server.hasArg("file")) return;
  File file = SD_MMC.open(server.arg("file").c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.streamFile(file, "image/jpeg");
  file.close();
}

void handleImageDownload() {
  if (!server.hasArg("file")) return;
  String filename = server.arg("file");
  File file = SD_MMC.open(filename.c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  
  String downloadName = filename.substring(filename.lastIndexOf('/') + 1);
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + downloadName + "\"");
  server.streamFile(file, "image/jpeg");
  file.close();
  
  Serial.printf("Downloaded image: %s\n", filename.c_str());
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

void handleDeleteImage() {
  if (!server.hasArg("file")) return;
  String filename = server.arg("file");
  if (SD_MMC.remove(filename.c_str())) {
    Serial.printf("Deleted image: %s\n", filename.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(200, "application/json", "{\"success\":false}");
  }
}

void handleAudioStream() {
  if (!server.hasArg("file")) return;
  File file = SD_MMC.open(server.arg("file").c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.streamFile(file, "audio/wav");
  file.close();
}

void handleAudioDownload() {
  if (!server.hasArg("file")) return;
  String filename = server.arg("file");
  File file = SD_MMC.open(filename.c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  
  String downloadName = filename.substring(filename.lastIndexOf('/') + 1);
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + downloadName + "\"");
  server.streamFile(file, "audio/wav");
  file.close();
  
  Serial.printf("Downloaded: %s\n", filename.c_str());
}

void handleAudioPlay() {
  if (!server.hasArg("file")) {
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  String filename = server.arg("file");
  
  if (isPlayingOnSpeaker) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Already playing\"}");
    return;
  }
  
  isPlayingOnSpeaker = true;
  
  // Send response first
  server.send(200, "application/json", "{\"success\":true}");
  
  // Then play (blocking)
  playWAVOnSpeaker(filename);
  
  isPlayingOnSpeaker = false;
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
  
  Serial.println("▶ Recording timer started");
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
  
  Serial.printf("⏺ Recording %d seconds...\n", recordTime);
  
  // Record audio
  size_t wav_size = 0;
  uint8_t *wav_buffer = i2s_mic.recordWAV(recordTime, &wav_size);
  
  if (!wav_buffer || wav_size < 100) {
    Serial.println("  ✗ Recording failed");
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  // Analyze and amplify
  int16_t originalPeak = 0;
  if (wav_size > 44) {
    originalPeak = getPeakLevel(wav_buffer + 44, wav_size - 44);
    Serial.printf("  Original peak: %d (%.1f%%)\n", originalPeak, (originalPeak * 100.0) / 32767);
    
    amplifyAudio(wav_buffer + 44, wav_size - 44, AMPLIFICATION);
    int16_t newPeak = getPeakLevel(wav_buffer + 44, wav_size - 44);
    Serial.printf("  Amplified: %d (%.1f%%)\n", newPeak, (newPeak * 100.0) / 32767);
  }
  
  // Save to SD
  audioCount++;
  latestAudio = "/audio/REC_" + String(audioCount) + ".wav";
  
  File file = SD_MMC.open(latestAudio.c_str(), FILE_WRITE);
  if (!file) {
    free(wav_buffer);
    Serial.println("  ✗ Failed to save");
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  file.write(wav_buffer, wav_size);
  file.close();
  free(wav_buffer);
  
  Serial.printf("  ✓ Saved: %s (%d bytes)\n\n", latestAudio.c_str(), wav_size);
  
  String response = "{\"success\":true,\"bytes\":" + String(wav_size) + 
                    ",\"peak\":" + String(originalPeak) + "}";
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
