/*
 * ESP32-S3 AI CAM - Audio Recorder (MULTIPLE CONFIG ATTEMPTS)
 * 
 * This version will try different I2S configurations to find what works
 * with your PDM microphone on GPIO 38/39
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

// Audio config
#define SAMPLE_RATE       16000
#define I2S_PORT          I2S_NUM_0

int imageCount = 0;
int audioCount = 0;
String latestAudio = "";
bool isRecording = false;
File audioFile;
uint32_t recordedBytes = 0;

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
        .btn-test { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); }
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
        audio { width: 100%; margin-top: 10px; }
        .debug {
            background: #f5f5f5;
            padding: 15px;
            border-radius: 5px;
            font-family: monospace;
            font-size: 12px;
            margin-top: 20px;
            white-space: pre-wrap;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📷 ESP32-S3 Recorder</h1>
            <p>Audio Debug Mode</p>
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
                </div>
            </div>
            
            <div class="section">
                <h2>🎤 Audio Recorder</h2>
                <div class="controls">
                    <button class="btn-test" onclick="testMic()">🔧 Test Microphone</button>
                    <button id="recordBtn" class="btn-record" onclick="startRecording()">⏺️ Record</button>
                    <button id="stopBtn" class="btn-stop" onclick="stopRecording()" disabled>⏹️ Stop</button>
                </div>
                <div id="debugOutput" class="debug"></div>
                <div id="audioList"></div>
            </div>
        </div>
    </div>
    
    <script>
        function showStatus(msg, err = false) {
            const status = document.getElementById('status');
            status.textContent = msg;
            status.className = 'status show' + (err ? ' error' : '');
            setTimeout(() => status.classList.remove('show'), 3000);
        }
        
        function testMic() {
            showStatus('Testing microphone...');
            fetch('/mic/test')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('debugOutput').textContent = d.debug;
                    if (d.working) {
                        showStatus('✓ Microphone working!');
                    } else {
                        showStatus('✗ Microphone not working', true);
                    }
                });
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
                        if (d.bytes > 0) loadAudioFiles();
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
                        const div = document.createElement('div');
                        div.className = 'audio-item';
                        div.innerHTML = `
                            <div>${f.split('/').pop()}</div>
                            <audio controls>
                                <source src="/audio?file=${encodeURIComponent(f)}" type="audio/wav">
                            </audio>
                        `;
                        list.appendChild(div);
                    });
                });
        }
        
        setInterval(() => {
            document.getElementById('stream').src = '/stream?' + Date.now();
        }, 100);
        
        // Auto-test on load
        setTimeout(testMic, 1000);
    </script>
</body>
</html>
)rawliteral";

// Try different I2S configurations
bool initI2S_Config1() {
  Serial.println("\n=== Trying Config 1: PDM RIGHT channel ===");
  i2s_driver_uninstall(I2S_PORT);
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
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
  
  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_PORT);
  return true;
}

bool initI2S_Config2() {
  Serial.println("\n=== Trying Config 2: PDM LEFT channel ===");
  i2s_driver_uninstall(I2S_PORT);
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
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
  
  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_PORT);
  return true;
}

bool initI2S_Config3() {
  Serial.println("\n=== Trying Config 3: Standard I2S (not PDM) ===");
  i2s_driver_uninstall(I2S_PORT);
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = PDM_CLK_PIN,
    .ws_io_num = PDM_CLK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PDM_DATA_PIN
  };
  
  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_PORT);
  return true;
}

String testConfig() {
  String result = "";
  const int samples = 200;
  int16_t buffer[samples];
  size_t bytesRead;
  
  delay(100);  // Let it settle
  
  esp_err_t err = i2s_read(I2S_PORT, buffer, samples * sizeof(int16_t), &bytesRead, 1000);
  
  result += "Read result: " + String(err) + "\n";
  result += "Bytes read: " + String(bytesRead) + "\n";
  
  if (bytesRead > 0) {
    int nonZero = 0;
    int16_t minVal = 32767, maxVal = -32768;
    
    for (int i = 0; i < bytesRead / 2; i++) {
      if (buffer[i] != 0) nonZero++;
      if (buffer[i] < minVal) minVal = buffer[i];
      if (buffer[i] > maxVal) maxVal = buffer[i];
    }
    
    result += "Non-zero samples: " + String(nonZero) + "/" + String(bytesRead/2) + "\n";
    result += "Range: " + String(minVal) + " to " + String(maxVal) + "\n";
    result += "First 20 samples: ";
    for (int i = 0; i < 20 && i < bytesRead/2; i++) {
      result += String(buffer[i]) + " ";
    }
    result += "\n";
    
    if (nonZero > 10 && (maxVal - minVal) > 100) {
      result += "✓ WORKING!\n";
      return result;
    } else {
      result += "✗ Not working (all zeros or no variation)\n";
    }
  } else {
    result += "✗ No data\n";
  }
  
  return result;
}

void findWorkingConfig() {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║  SEARCHING FOR WORKING I2S CONFIG     ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  // Try Config 1
  if (initI2S_Config1()) {
    String result = testConfig();
    Serial.println(result);
    if (result.indexOf("WORKING") > 0) {
      Serial.println("✓ Config 1 works!");
      return;
    }
  }
  
  // Try Config 2
  if (initI2S_Config2()) {
    String result = testConfig();
    Serial.println(result);
    if (result.indexOf("WORKING") > 0) {
      Serial.println("✓ Config 2 works!");
      return;
    }
  }
  
  // Try Config 3
  if (initI2S_Config3()) {
    String result = testConfig();
    Serial.println(result);
    if (result.indexOf("WORKING") > 0) {
      Serial.println("✓ Config 3 works!");
      return;
    }
  }
  
  Serial.println("\n✗ NO WORKING CONFIG FOUND");
  Serial.println("The PDM microphone may have a hardware issue");
  Serial.println("or require additional initialization.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n╔═══════════════════════════════════════╗");
  Serial.println("║   ESP32-S3 AUDIO RECORDER DEBUG       ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  // SD Card
  Serial.println("→ Initializing SD Card...");
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (SD_MMC.begin("/sdcard", true)) {
    Serial.printf("  ✓ SD OK: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
    if (!SD_MMC.exists("/images")) SD_MMC.mkdir("/images");
    if (!SD_MMC.exists("/audio")) SD_MMC.mkdir("/audio");
  } else {
    Serial.println("  ✗ SD Failed");
  }
  
  // Camera
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
  } else {
    Serial.println("  ✗ Camera Failed");
  }
  
  // Find working I2S config
  Serial.println("\n→ Testing I2S configurations...");
  findWorkingConfig();
  
  // WiFi
  Serial.println("\n→ Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n  ✓ http://%s\n", WiFi.localIP().toString().c_str());
  
  // Web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/audio", HTTP_GET, handleAudio);
  server.on("/list/audio", HTTP_GET, handleListAudio);
  server.on("/audio/start", HTTP_GET, handleAudioStart);
  server.on("/audio/stop", HTTP_GET, handleAudioStop);
  server.on("/mic/test", HTTP_GET, handleMicTest);
  
  server.begin();
  Serial.println("\n✓ System ready!\n");
}

void loop() {
  server.handleClient();
  
  if (isRecording && audioFile) {
    int16_t buffer[512];
    size_t bytesRead = 0;
    
    esp_err_t result = i2s_read(I2S_PORT, buffer, 512 * sizeof(int16_t), 
                                 &bytesRead, 100 / portTICK_PERIOD_MS);
    
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
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Already recording\"}");
    return;
  }
  
  audioCount++;
  latestAudio = "/audio/REC_" + String(audioCount) + ".wav";
  
  audioFile = SD_MMC.open(latestAudio.c_str(), FILE_WRITE);
  if (!audioFile) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"SD failed\"}");
    return;
  }
  
  WAVHeader header;
  header.fileSize = 0;
  header.dataSize = 0;
  audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
  
  recordedBytes = 0;
  isRecording = true;
  
  Serial.printf("Recording: %s\n", latestAudio.c_str());
  server.send(200, "application/json", "{\"success\":true}");
}

void handleAudioStop() {
  if (!isRecording) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Not recording\"}");
    return;
  }
  
  isRecording = false;
  
  WAVHeader header;
  header.fileSize = recordedBytes + sizeof(WAVHeader) - 8;
  header.dataSize = recordedBytes;
  
  audioFile.seek(0);
  audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
  audioFile.close();
  
  Serial.printf("Stopped: %d bytes\n", recordedBytes);
  
  String response = "{\"success\":true,\"bytes\":" + String(recordedBytes) + "}";
  server.send(200, "application/json", response);
}

void handleMicTest() {
  Serial.println("\n=== Web-triggered mic test ===");
  
  String debug = "Testing all configurations...\n\n";
  bool found = false;
  
  // Test Config 1
  if (initI2S_Config1()) {
    debug += "Config 1 (PDM RIGHT):\n";
    String result = testConfig();
    debug += result + "\n";
    if (result.indexOf("WORKING") > 0) found = true;
  }
  
  // Test Config 2
  if (!found && initI2S_Config2()) {
    debug += "Config 2 (PDM LEFT):\n";
    String result = testConfig();
    debug += result + "\n";
    if (result.indexOf("WORKING") > 0) found = true;
  }
  
  // Test Config 3
  if (!found && initI2S_Config3()) {
    debug += "Config 3 (Standard I2S):\n";
    String result = testConfig();
    debug += result + "\n";
    if (result.indexOf("WORKING") > 0) found = true;
  }
  
  if (!found) {
    debug += "\n✗ NO CONFIGURATION WORKED\n";
    debug += "Possible issues:\n";
    debug += "- Microphone not connected to GPIO 38/39\n";
    debug += "- Microphone requires power enable pin\n";
    debug += "- Hardware defect\n";
  }
  
  Serial.println(debug);
  
  String json = "{\"working\":" + String(found ? "true" : "false") + ",\"debug\":\"" + debug + "\"}";
  json.replace("\n", "\\n");
  server.send(200, "application/json", json);
}
