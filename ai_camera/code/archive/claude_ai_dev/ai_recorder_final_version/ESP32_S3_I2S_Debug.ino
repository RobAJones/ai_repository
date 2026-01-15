/*
 * ESP32-S3 I2S Debug Version
 * 
 * This will show exactly what i2s_read is returning
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

#define SAMPLE_RATE       16000
#define I2S_PORT          I2S_NUM_0
#define AMPLIFICATION     10

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
uint32_t loopCounter = 0;
uint32_t readAttempts = 0;
uint32_t successfulReads = 0;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-S3 Debug</title>
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
        .recording-timer {
            text-align: center;
            font-size: 32px;
            color: #eb3349;
            margin: 15px 0;
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
        .debug { background: #fff3cd; padding: 15px; border-radius: 5px; margin: 20px 0; font-family: monospace; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔍 I2S Debug Mode</h1>
        </div>
        
        <div class="content">
            <div id="status" class="status"></div>
            
            <div class="debug">
                <strong>Debug Info:</strong> Check Serial Monitor for detailed I2S read status.
                This version logs every read attempt to diagnose the issue.
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
                <h2>🎤 Audio Recorder (DEBUG)</h2>
                <div id="recordingTimer" class="recording-timer" style="display:none;">00:00</div>
                <div class="controls">
                    <button id="recordBtn" class="btn-record" onclick="startRecording()">⏺️ Record</button>
                    <button id="stopBtn" class="btn-stop" onclick="stopRecording()" disabled>⏹️ Stop</button>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        let recordingStartTime = 0;
        let timerInterval = null;
        
        function showStatus(msg) {
            const status = document.getElementById('status');
            status.textContent = msg;
            status.className = 'status show';
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
            fetch('/capture').then(r => r.json()).then(d => {
                if (d.success) showStatus('Image captured');
            });
        }
        
        function startRecording() {
            fetch('/audio/start').then(r => r.json()).then(d => {
                if (d.success) {
                    document.getElementById('recordBtn').disabled = true;
                    document.getElementById('stopBtn').disabled = false;
                    document.getElementById('recordBtn').classList.add('recording');
                    document.getElementById('recordingTimer').style.display = 'block';
                    recordingStartTime = Date.now();
                    timerInterval = setInterval(updateTimer, 100);
                    showStatus('Recording - CHECK SERIAL MONITOR');
                }
            });
        }
        
        function stopRecording() {
            clearInterval(timerInterval);
            document.getElementById('stopBtn').disabled = true;
            
            fetch('/audio/stop').then(r => r.json()).then(d => {
                document.getElementById('recordBtn').disabled = false;
                document.getElementById('stopBtn').disabled = false;
                document.getElementById('recordBtn').classList.remove('recording');
                document.getElementById('recordingTimer').style.display = 'none';
                
                if (d.success) {
                    showStatus('Stopped - Check Serial Monitor');
                }
            });
        }
        
        setInterval(() => {
            document.getElementById('stream').src = '/stream?' + Date.now();
        }, 100);
    </script>
</body>
</html>
)rawliteral";

void initI2S_PDM() {
  Serial.println("\n=== INITIALIZING I2S PDM ===");
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
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
  
  Serial.printf("Installing I2S driver on port %d...\n", I2S_PORT);
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  Serial.printf("  Install result: 0x%x (%s)\n", err, err == ESP_OK ? "OK" : "FAILED");
  
  if (err != ESP_OK) return;
  
  Serial.printf("Setting I2S pins (CLK=%d, DATA=%d)...\n", PDM_CLK_PIN, PDM_DATA_PIN);
  err = i2s_set_pin(I2S_PORT, &pin_config);
  Serial.printf("  Pin set result: 0x%x (%s)\n", err, err == ESP_OK ? "OK" : "FAILED");
  
  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("✓ I2S PDM initialized\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n╔══════════════════════════════════╗");
  Serial.println("║  I2S DEBUG MODE                  ║");
  Serial.println("╚══════════════════════════════════╝\n");
  
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);
  
  // SD Card
  Serial.println("→ SD Card...");
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (SD_MMC.begin("/sdcard", true)) {
    Serial.printf("  ✓ %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
    if (!SD_MMC.exists("/audio")) SD_MMC.mkdir("/audio");
    if (!SD_MMC.exists("/images")) SD_MMC.mkdir("/images");
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
  Serial.println("→ WiFi...");
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
  server.on("/audio/start", HTTP_GET, handleAudioStart);
  server.on("/audio/stop", HTTP_GET, handleAudioStop);
  
  server.begin();
  Serial.println("\n✓ Ready - Start recording to see debug output\n");
}

void loop() {
  server.handleClient();
  
  if (isRecording && audioFile) {
    loopCounter++;
    
    const int bufferSize = 512;
    int16_t buffer[bufferSize];
    size_t bytesRead = 0;
    
    readAttempts++;
    esp_err_t result = i2s_read(I2S_PORT, buffer, bufferSize * sizeof(int16_t), 
                                 &bytesRead, 100 / portTICK_PERIOD_MS);
    
    // Log every 100th attempt
    if (readAttempts % 100 == 0) {
      Serial.printf("[Loop %lu] i2s_read result: 0x%x, bytesRead: %d\n", 
                    loopCounter, result, bytesRead);
    }
    
    if (result == ESP_OK && bytesRead > 0) {
      successfulReads++;
      
      // Log first successful read
      if (successfulReads == 1) {
        Serial.println("\n✓ FIRST SUCCESSFUL READ!");
        Serial.printf("  Bytes: %d\n", bytesRead);
        Serial.printf("  First 10 samples: ");
        for (int i = 0; i < 10 && i < bytesRead/2; i++) {
          Serial.printf("%d ", buffer[i]);
        }
        Serial.println("\n");
      }
      
      audioFile.write((uint8_t*)buffer, bytesRead);
      recordedBytes += bytesRead;
    } else if (result != ESP_OK) {
      // Log errors
      if (readAttempts % 100 == 1) {
        Serial.printf("[ERROR] i2s_read failed: 0x%x\n", result);
      }
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

void handleAudioStart() {
  if (isRecording) {
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  audioCount++;
  latestAudio = "/audio/REC_" + String(audioCount) + ".wav";
  
  audioFile = SD_MMC.open(latestAudio.c_str(), FILE_WRITE);
  if (!audioFile) {
    Serial.println("✗ Failed to open file");
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  WAVHeader header;
  header.fileSize = 0;
  header.dataSize = 0;
  audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
  
  recordedBytes = 0;
  loopCounter = 0;
  readAttempts = 0;
  successfulReads = 0;
  isRecording = true;
  digitalWrite(3, HIGH);
  
  Serial.println("\n╔══════════════════════════════════╗");
  Serial.println("║  RECORDING STARTED               ║");
  Serial.println("╚══════════════════════════════════╝");
  Serial.printf("File: %s\n", latestAudio.c_str());
  Serial.println("Monitoring i2s_read calls...\n");
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleAudioStop() {
  if (!isRecording) {
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }
  
  isRecording = false;
  digitalWrite(3, LOW);
  
  Serial.println("\n╔══════════════════════════════════╗");
  Serial.println("║  RECORDING STOPPED               ║");
  Serial.println("╚══════════════════════════════════╝");
  Serial.printf("Loop iterations: %lu\n", loopCounter);
  Serial.printf("Read attempts: %lu\n", readAttempts);
  Serial.printf("Successful reads: %lu\n", successfulReads);
  Serial.printf("Bytes captured: %d\n", recordedBytes);
  Serial.printf("Success rate: %.1f%%\n\n", (successfulReads * 100.0) / max(readAttempts, 1ul));
  
  // Update header
  WAVHeader header;
  header.fileSize = recordedBytes + sizeof(WAVHeader) - 8;
  header.dataSize = recordedBytes;
  
  audioFile.seek(0);
  audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
  audioFile.close();
  
  if (recordedBytes == 0) {
    Serial.println("⚠ WARNING: Zero bytes recorded!");
    Serial.println("  Possible causes:");
    Serial.println("  1. Camera is using I2S_NUM_0");
    Serial.println("  2. I2S driver not properly initialized");
    Serial.println("  3. GPIO conflict");
  }
  
  String response = "{\"success\":true,\"bytes\":" + String(recordedBytes) + "}";
  server.send(200, "application/json", response);
}
