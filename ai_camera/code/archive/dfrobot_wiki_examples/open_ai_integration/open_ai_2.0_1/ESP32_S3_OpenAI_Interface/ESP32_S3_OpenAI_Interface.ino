/*
 * ESP32-S3 AI CAM - OpenAI Integration Interface
 * 
 * Features:
 * - Camera capture and streaming with review
 * - PDM audio recording (10x amplification)
 * - OpenAI API integration for image and audio analysis
 * - Speaker playback through MAX98357A
 * - Comprehensive file management
 * 
 * Layout:
 * - Upper Left: Video Stream / Image Review
 * - Upper Right: Audio Controls & File Lists
 * - Middle Right: Image & Audio File Management
 * - Lower Left: OpenAI Response Display
 * - Lower: Send, Replay, Save Controls
 */

#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "ESP_I2S.h"
#include "mbedtls/base64.h"

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* openai_api_key = "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";

WebServer server(80);

// Camera pins (DFRobot ESP32-S3 AI CAM)
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
#define PDM_DATA_PIN      GPIO_NUM_39
#define PDM_CLOCK_PIN     GPIO_NUM_38

// MAX98357A I2S Amplifier
#define I2S_BCLK          45
#define I2S_LRC           46
#define I2S_DOUT          42
#define I2S_SD_PIN        41

#define SAMPLE_RATE       16000
#define AMPLIFICATION     10

int imageCount = 0;
int audioCount = 0;
String currentReviewImage = "";
bool isRecording = false;

I2SClass i2s_mic;
I2SClass i2s_spk;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-S3 OpenAI Interface</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #f5f5f5;
            padding: 20px;
        }
        .main-grid {
            display: grid;
            grid-template-columns: 1.5fr 1fr;
            grid-template-rows: auto auto auto;
            gap: 20px;
            max-width: 1400px;
            margin: 0 auto;
        }
        
        /* Upper Left - Video/Image Area */
        .video-section {
            grid-column: 1;
            grid-row: 1;
            background: white;
            border-radius: 12px;
            border: 2px solid #ddd;
            padding: 20px;
        }
        .video-controls {
            display: flex;
            gap: 10px;
            margin-bottom: 15px;
            flex-wrap: wrap;
        }
        .video-display {
            background: #000;
            border-radius: 8px;
            position: relative;
            padding-bottom: 75%;
            overflow: hidden;
        }
        .video-display img {
            position: absolute;
            width: 100%;
            height: 100%;
            object-fit: contain;
        }
        
        /* Upper Right - Audio Controls */
        .audio-section {
            grid-column: 2;
            grid-row: 1;
            background: white;
            border-radius: 12px;
            border: 2px solid #ddd;
            padding: 20px;
        }
        .audio-bar {
            background: #f0f0f0;
            border-radius: 8px;
            padding: 15px;
            margin-bottom: 15px;
            text-align: center;
            font-weight: bold;
            color: #333;
        }
        .recording-timer {
            font-size: 32px;
            color: #e74c3c;
            font-family: monospace;
            margin: 15px 0;
        }
        
        /* Middle Right - File Lists */
        .files-section {
            grid-column: 2;
            grid-row: 2;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }
        .file-column {
            background: white;
            border-radius: 12px;
            border: 2px solid #ddd;
            padding: 15px;
        }
        .file-column h3 {
            margin-bottom: 10px;
            color: #333;
            font-size: 14px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 8px;
        }
        .file-list {
            max-height: 200px;
            overflow-y: auto;
            margin-bottom: 10px;
        }
        .file-item {
            background: #f8f8f8;
            padding: 8px;
            margin-bottom: 5px;
            border-radius: 6px;
            cursor: pointer;
            border: 2px solid transparent;
            font-size: 12px;
        }
        .file-item:hover {
            background: #e8e8e8;
        }
        .file-item.selected {
            border-color: #667eea;
            background: #e8eeff;
        }
        .export-label {
            font-size: 11px;
            color: #666;
            margin-top: 10px;
            padding: 8px;
            background: #f0f0f0;
            border-radius: 4px;
            min-height: 30px;
        }
        
        /* Lower Left - Message Display */
        .message-section {
            grid-column: 1;
            grid-row: 2 / 4;
            background: white;
            border-radius: 12px;
            border: 2px solid #ddd;
            padding: 20px;
        }
        .message-display {
            background: #f9f9f9;
            border-radius: 8px;
            padding: 15px;
            min-height: 200px;
            max-height: 400px;
            overflow-y: auto;
            margin-bottom: 15px;
            font-size: 14px;
            line-height: 1.6;
            border: 1px solid #ddd;
        }
        .message-controls {
            display: flex;
            gap: 10px;
        }
        
        /* Lower Right - Return Lists */
        .return-section {
            grid-column: 2;
            grid-row: 3;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }
        .return-column {
            background: white;
            border-radius: 12px;
            border: 2px solid #ddd;
            padding: 15px;
        }
        .return-column h3 {
            margin-bottom: 10px;
            color: #333;
            font-size: 14px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 8px;
        }
        .return-list {
            max-height: 150px;
            overflow-y: auto;
        }
        
        /* Buttons */
        button {
            padding: 10px 20px;
            font-size: 14px;
            font-weight: 600;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s;
            color: white;
        }
        button:hover:not(:disabled) {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        }
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        
        .btn-stream { background: #3498db; }
        .btn-capture { background: #667eea; }
        .btn-review { background: #9b59b6; }
        .btn-record { background: #e74c3c; }
        .btn-playback { background: #2ecc71; }
        .btn-delete { background: #e67e22; }
        .btn-save { background: #16a085; }
        .btn-send { background: #2c3e50; flex: 1; }
        .btn-replay { background: #34495e; flex: 1; }
        
        .btn-record.recording {
            animation: pulse 1.5s infinite;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.6; }
        }
        
        .status-bar {
            position: fixed;
            top: 20px;
            right: 20px;
            background: #2ecc71;
            color: white;
            padding: 12px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.2);
            display: none;
            z-index: 1000;
        }
        .status-bar.show { display: block; }
        .status-bar.error { background: #e74c3c; }
        
        @media (max-width: 1024px) {
            .main-grid {
                grid-template-columns: 1fr;
            }
            .video-section, .audio-section, .files-section, 
            .message-section, .return-section {
                grid-column: 1;
            }
        }
    </style>
</head>
<body>
    <div id="statusBar" class="status-bar"></div>
    
    <div class="main-grid">
        <!-- Upper Left: Video/Image Display -->
        <div class="video-section">
            <div class="video-controls">
                <button class="btn-stream" onclick="toggleStream()">Video Stream</button>
                <button class="btn-capture" onclick="captureImage()">Capture</button>
                <button class="btn-review" onclick="reviewLastImage()">Review</button>
            </div>
            <div class="video-display">
                <img id="videoStream" src="/stream" alt="Video Stream">
            </div>
        </div>
        
        <!-- Upper Right: Audio Controls -->
        <div class="audio-section">
            <div class="audio-bar">Audio Playback Bar</div>
            <audio id="audioPlayer" controls style="width: 100%; margin-bottom: 15px; display: none;"></audio>
            <div id="recordingTimer" class="recording-timer" style="display: none;">00:00</div>
            <div class="video-controls">
                <button id="recordBtn" class="btn-record" onclick="startRecording()">Record</button>
                <button id="playbackBtn" class="btn-playback" onclick="playbackAudio()">Playback</button>
                <button class="btn-delete" onclick="deleteSelectedAudio()">Delete</button>
            </div>
        </div>
        
        <!-- Middle Right: File Lists -->
        <div class="files-section">
            <!-- Image Files -->
            <div class="file-column">
                <h3>Image Files</h3>
                <div class="file-list" id="imageFileList"></div>
                <div class="video-controls" style="margin-bottom: 10px;">
                    <button class="btn-save" onclick="saveImageToExport()" style="flex: 1;">Save</button>
                    <button class="btn-delete" onclick="deleteSelectedImage()" style="flex: 1;">Delete</button>
                </div>
                <div class="export-label" id="imageExportLabel">Image for Export</div>
            </div>
            
            <!-- Audio Files -->
            <div class="file-column">
                <h3>Audio Files</h3>
                <div class="file-list" id="audioFileList"></div>
                <div class="video-controls" style="margin-bottom: 10px;">
                    <button class="btn-save" onclick="saveAudioToExport()" style="flex: 1;">Save</button>
                    <button class="btn-delete" onclick="deleteSelectedAudio()" style="flex: 1;">Delete</button>
                </div>
                <div class="export-label" id="audioExportLabel">Audio for Export</div>
            </div>
        </div>
        
        <!-- Lower Left: Message Display -->
        <div class="message-section">
            <h3 style="margin-bottom: 15px; color: #333;">Returned Test Message Display</h3>
            <div class="message-display" id="messageDisplay">
                Messages from OpenAI will appear here...
            </div>
            <div class="message-controls">
                <button class="btn-send" onclick="sendToOpenAI()">Send</button>
                <button class="btn-replay" onclick="replayAudio()">Replay</button>
                <button class="btn-save" onclick="saveAnalysis()">Save</button>
            </div>
        </div>
        
        <!-- Lower Right: Return Lists -->
        <div class="return-section">
            <!-- Return Text -->
            <div class="return-column">
                <h3>Return Test</h3>
                <div class="return-list" id="returnTextList"></div>
            </div>
            
            <!-- Return Audio -->
            <div class="return-column">
                <h3>Return Audio</h3>
                <div class="return-list" id="returnAudioList"></div>
            </div>
        </div>
    </div>
    
    <script>
        let recordingStartTime = 0;
        let timerInterval = null;
        let streamActive = true;
        let selectedImageFile = '';
        let selectedAudioFile = '';
        let selectedImageForExport = '';
        let selectedAudioForExport = '';
        let lastAnalysisResponse = '';
        
        function showStatus(msg, isError = false) {
            const bar = document.getElementById('statusBar');
            bar.textContent = msg;
            bar.className = 'status-bar show' + (isError ? ' error' : '');
            setTimeout(() => bar.classList.remove('show'), 3000);
        }
        
        // Video Stream Control
        function toggleStream() {
            streamActive = !streamActive;
            const img = document.getElementById('videoStream');
            if (streamActive) {
                img.src = '/stream?' + Date.now();
                showStatus('Stream started');
            } else {
                img.src = '';
                showStatus('Stream stopped');
            }
        }
        
        // Image Capture
        function captureImage() {
            fetch('/capture')
                .then(r => r.json())
                .then(d => {
                    if (d.success) {
                        showStatus('Image captured: ' + d.filename);
                        loadImageList();
                    }
                })
                .catch(e => showStatus('Capture failed', true));
        }
        
        // Review Last Image
        function reviewLastImage() {
            fetch('/image/latest')
                .then(r => r.json())
                .then(d => {
                    if (d.success && d.filename) {
                        document.getElementById('videoStream').src = 
                            '/image?file=' + encodeURIComponent(d.filename);
                        streamActive = false;
                        showStatus('Reviewing: ' + d.filename);
                    } else {
                        showStatus('No images to review', true);
                    }
                })
                .catch(e => showStatus('Review failed', true));
        }
        
        // Load Image File List
        function loadImageList() {
            fetch('/list/images')
                .then(r => r.json())
                .then(d => {
                    const list = document.getElementById('imageFileList');
                    list.innerHTML = '';
                    d.files.forEach(file => {
                        const div = document.createElement('div');
                        div.className = 'file-item';
                        div.textContent = file;
                        div.onclick = () => selectImageFile(file);
                        list.appendChild(div);
                    });
                });
        }
        
        // Load Audio File List
        function loadAudioList() {
            fetch('/list/audio')
                .then(r => r.json())
                .then(d => {
                    const list = document.getElementById('audioFileList');
                    list.innerHTML = '';
                    d.files.forEach(file => {
                        const div = document.createElement('div');
                        div.className = 'file-item';
                        div.textContent = file;
                        div.onclick = () => selectAudioFile(file);
                        list.appendChild(div);
                    });
                });
        }
        
        // Select Image File
        function selectImageFile(filename) {
            selectedImageFile = filename;
            document.querySelectorAll('#imageFileList .file-item').forEach(el => {
                el.classList.toggle('selected', el.textContent === filename);
            });
            showStatus('Selected: ' + filename);
        }
        
        // Select Audio File
        function selectAudioFile(filename) {
            selectedAudioFile = filename;
            document.querySelectorAll('#audioFileList .file-item').forEach(el => {
                el.classList.toggle('selected', el.textContent === filename);
            });
            
            // Load audio for preview
            const player = document.getElementById('audioPlayer');
            player.src = '/audio?file=' + encodeURIComponent(filename);
            player.style.display = 'block';
            showStatus('Selected: ' + filename);
        }
        
        // Save Image to Export
        function saveImageToExport() {
            if (!selectedImageFile) {
                showStatus('No image selected', true);
                return;
            }
            selectedImageForExport = selectedImageFile;
            document.getElementById('imageExportLabel').textContent = 
                'Export: ' + selectedImageFile;
            showStatus('Ready for export: ' + selectedImageFile);
        }
        
        // Save Audio to Export
        function saveAudioToExport() {
            if (!selectedAudioFile) {
                showStatus('No audio selected', true);
                return;
            }
            selectedAudioForExport = selectedAudioFile;
            document.getElementById('audioExportLabel').textContent = 
                'Export: ' + selectedAudioFile;
            showStatus('Ready for export: ' + selectedAudioFile);
        }
        
        // Delete Selected Image
        function deleteSelectedImage() {
            if (!selectedImageFile) {
                showStatus('No image selected', true);
                return;
            }
            if (confirm('Delete ' + selectedImageFile + '?')) {
                fetch('/delete/image?file=' + encodeURIComponent(selectedImageFile), 
                      {method: 'DELETE'})
                    .then(() => {
                        showStatus('Deleted: ' + selectedImageFile);
                        selectedImageFile = '';
                        loadImageList();
                    });
            }
        }
        
        // Delete Selected Audio
        function deleteSelectedAudio() {
            if (!selectedAudioFile) {
                showStatus('No audio selected', true);
                return;
            }
            if (confirm('Delete ' + selectedAudioFile + '?')) {
                fetch('/delete/audio?file=' + encodeURIComponent(selectedAudioFile), 
                      {method: 'DELETE'})
                    .then(() => {
                        showStatus('Deleted: ' + selectedAudioFile);
                        selectedAudioFile = '';
                        document.getElementById('audioPlayer').style.display = 'none';
                        loadAudioList();
                    });
            }
        }
        
        // Recording Controls
        function updateTimer() {
            const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
            const minutes = Math.floor(elapsed / 60);
            const seconds = elapsed % 60;
            document.getElementById('recordingTimer').textContent = 
                String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
        }
        
        function startRecording() {
            const btn = document.getElementById('recordBtn');
            if (btn.classList.contains('recording')) {
                // Stop recording
                clearInterval(timerInterval);
                const duration = Math.floor((Date.now() - recordingStartTime) / 1000);
                
                btn.textContent = 'Processing...';
                btn.disabled = true;
                
                fetch('/audio/record?duration=' + duration)
                    .then(r => r.json())
                    .then(d => {
                        if (d.success) {
                            showStatus('Audio saved: ' + d.filename);
                            loadAudioList();
                        }
                        btn.classList.remove('recording');
                        btn.textContent = 'Record';
                        btn.disabled = false;
                        document.getElementById('recordingTimer').style.display = 'none';
                    })
                    .catch(e => {
                        showStatus('Recording failed', true);
                        btn.classList.remove('recording');
                        btn.textContent = 'Record';
                        btn.disabled = false;
                    });
            } else {
                // Start recording
                recordingStartTime = Date.now();
                btn.classList.add('recording');
                document.getElementById('recordingTimer').style.display = 'block';
                timerInterval = setInterval(updateTimer, 100);
                showStatus('Recording started');
            }
        }
        
        // Playback Audio through Speaker
        function playbackAudio() {
            if (!selectedAudioFile) {
                showStatus('No audio selected', true);
                return;
            }
            showStatus('Playing on speaker...');
            fetch('/audio/speaker?file=' + encodeURIComponent(selectedAudioFile))
                .then(r => r.json())
                .then(d => {
                    if (d.success) {
                        showStatus('Playback complete');
                    }
                })
                .catch(e => showStatus('Playback failed', true));
        }
        
        // Send to OpenAI
        function sendToOpenAI() {
            if (!selectedImageForExport || !selectedAudioForExport) {
                showStatus('Select image and audio for export first', true);
                return;
            }
            
            showStatus('Sending to OpenAI...');
            document.getElementById('messageDisplay').textContent = 'Processing request...';
            
            fetch('/openai/analyze', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    image: selectedImageForExport,
                    audio: selectedAudioForExport
                })
            })
            .then(r => r.json())
            .then(d => {
                if (d.success) {
                    lastAnalysisResponse = d.response;
                    document.getElementById('messageDisplay').textContent = d.response;
                    showStatus('Analysis complete');
                    
                    // Add to return lists
                    addToReturnText(d.response);
                    if (d.audioUrl) {
                        addToReturnAudio(d.audioUrl);
                    }
                } else {
                    showStatus('OpenAI request failed', true);
                    document.getElementById('messageDisplay').textContent = 
                        'Error: ' + (d.error || 'Unknown error');
                }
            })
            .catch(e => {
                showStatus('Network error', true);
                document.getElementById('messageDisplay').textContent = 'Network error occurred';
            });
        }
        
        // Replay Audio
        function replayAudio() {
            const player = document.getElementById('audioPlayer');
            if (player.src) {
                player.play();
                showStatus('Replaying audio');
            } else {
                showStatus('No audio to replay', true);
            }
        }
        
        // Save Analysis
        function saveAnalysis() {
            if (!lastAnalysisResponse) {
                showStatus('No analysis to save', true);
                return;
            }
            
            fetch('/analysis/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    text: lastAnalysisResponse,
                    image: selectedImageForExport,
                    audio: selectedAudioForExport
                })
            })
            .then(r => r.json())
            .then(d => {
                if (d.success) {
                    showStatus('Analysis saved: ' + d.filename);
                }
            });
        }
        
        // Add to Return Text List
        function addToReturnText(text) {
            const list = document.getElementById('returnTextList');
            const div = document.createElement('div');
            div.className = 'file-item';
            div.textContent = text.substring(0, 50) + '...';
            div.onclick = () => {
                document.getElementById('messageDisplay').textContent = text;
            };
            list.insertBefore(div, list.firstChild);
        }
        
        // Add to Return Audio List
        function addToReturnAudio(url) {
            const list = document.getElementById('returnAudioList');
            const div = document.createElement('div');
            div.className = 'file-item';
            div.textContent = 'Audio Response';
            div.onclick = () => {
                document.getElementById('audioPlayer').src = url;
                document.getElementById('audioPlayer').play();
            };
            list.insertBefore(div, list.firstChild);
        }
        
        // Initialize
        loadImageList();
        loadAudioList();
        
        // Refresh stream periodically
        setInterval(() => {
            if (streamActive) {
                const img = document.getElementById('videoStream');
                if (img.src.includes('/stream')) {
                    img.src = '/stream?' + Date.now();
                }
            }
        }, 100);
    </script>
</body>
</html>
)rawliteral";

// Amplify audio samples
void amplifyAudio(uint8_t* buffer, size_t size, int factor) {
  int16_t* samples = (int16_t*)buffer;
  size_t sampleCount = size / 2;
  
  for (size_t i = 0; i < sampleCount; i++) {
    int32_t amplified = (int32_t)samples[i] * factor;
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;
    samples[i] = (int16_t)amplified;
  }
}

// Base64 encode for OpenAI
String base64Encode(uint8_t* data, size_t length) {
  size_t olen;
  mbedtls_base64_encode(NULL, 0, &olen, data, length);
  
  uint8_t* encoded = (uint8_t*)malloc(olen);
  mbedtls_base64_encode(encoded, olen, &olen, data, length);
  
  String result = String((char*)encoded);
  free(encoded);
  return result;
}

// Camera initialization
void setupCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }
  
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
}

// HTTP Handlers
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleStream() {
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();
  
  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) continue;
    
    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.println("\r\n");
    
    esp_camera_fb_return(fb);
    delay(100);
  }
}

void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "application/json", "{\"success\":false}");
    return;
  }
  
  imageCount++;
  String filename = "/images/IMG_" + String(imageCount) + ".jpg";
  
  File file = SD_MMC.open(filename, FILE_WRITE);
  if (file) {
    file.write(fb->buf, fb->len);
    file.close();
    server.send(200, "application/json", 
      "{\"success\":true,\"filename\":\"IMG_" + String(imageCount) + ".jpg\"}");
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
  
  esp_camera_fb_return(fb);
}

void handleImageLatest() {
  if (imageCount == 0) {
    server.send(404, "application/json", "{\"success\":false}");
    return;
  }
  
  String filename = "IMG_" + String(imageCount) + ".jpg";
  server.send(200, "application/json", 
    "{\"success\":true,\"filename\":\"" + filename + "\"}");
}

void handleImage() {
  String filename = server.arg("file");
  if (filename.isEmpty()) {
    server.send(400, "text/plain", "Missing file parameter");
    return;
  }
  
  String filepath = "/images/" + filename;
  File file = SD_MMC.open(filepath, FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  
  server.streamFile(file, "image/jpeg");
  file.close();
}

void handleListImages() {
  File dir = SD_MMC.open("/images");
  if (!dir) {
    server.send(500, "application/json", "{\"files\":[]}");
    return;
  }
  
  String json = "{\"files\":[";
  bool first = true;
  
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      if (!first) json += ",";
      json += "\"" + String(file.name()) + "\"";
      first = false;
    }
    file = dir.openNextFile();
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}

void handleDeleteImage() {
  String filename = server.arg("file");
  String filepath = "/images/" + filename;
  
  if (SD_MMC.remove(filepath)) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
}

void handleAudioRecord() {
  int duration = server.arg("duration").toInt();
  if (duration < 1 || duration > 60) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid duration\"}");
    return;
  }
  
  // Record audio
  i2s_mic.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  i2s_mic.setPinsPdmRx(PDM_CLOCK_PIN, PDM_DATA_PIN);
  
  size_t wav_size = 0;
  uint8_t* wav_buffer = i2s_mic.recordWAV(duration, &wav_size);
  
  if (wav_buffer && wav_size > 44) {
    amplifyAudio(wav_buffer + 44, wav_size - 44, AMPLIFICATION);
    
    audioCount++;
    String filename = "/audio/REC_" + String(audioCount) + ".wav";
    
    File file = SD_MMC.open(filename, FILE_WRITE);
    if (file) {
      file.write(wav_buffer, wav_size);
      file.close();
      server.send(200, "application/json", 
        "{\"success\":true,\"filename\":\"REC_" + String(audioCount) + ".wav\"}");
    } else {
      server.send(500, "application/json", "{\"success\":false}");
    }
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
  
  if (wav_buffer) free(wav_buffer);
}

void handleAudioStream() {
  String filename = server.arg("file");
  String filepath = "/audio/" + filename;
  
  File file = SD_MMC.open(filepath, FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  
  server.streamFile(file, "audio/wav");
  file.close();
}

void handleListAudio() {
  File dir = SD_MMC.open("/audio");
  if (!dir) {
    server.send(500, "application/json", "{\"files\":[]}");
    return;
  }
  
  String json = "{\"files\":[";
  bool first = true;
  
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      if (!first) json += ",";
      json += "\"" + String(file.name()) + "\"";
      first = false;
    }
    file = dir.openNextFile();
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}

void handleDeleteAudio() {
  String filename = server.arg("file");
  String filepath = "/audio/" + filename;
  
  if (SD_MMC.remove(filepath)) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
}

void handleSpeakerPlayback() {
  String filename = server.arg("file");
  String filepath = "/audio/" + filename;
  
  File file = SD_MMC.open(filepath, FILE_READ);
  if (!file) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"File not found\"}");
    return;
  }
  
  // Read WAV file
  uint8_t header[44];
  file.read(header, 44);
  
  size_t dataSize = file.size() - 44;
  uint8_t* audioData = (uint8_t*)malloc(dataSize);
  file.read(audioData, dataSize);
  file.close();
  
  // Initialize I2S for playback
  i2s_spk.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  i2s_spk.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  
  // Enable speaker
  pinMode(I2S_SD_PIN, OUTPUT);
  digitalWrite(I2S_SD_PIN, HIGH);
  
  // Play audio
  i2s_spk.write(audioData, dataSize);
  
  // Disable speaker
  digitalWrite(I2S_SD_PIN, LOW);
  i2s_spk.end();
  
  free(audioData);
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleOpenAIAnalyze() {
  // This is a placeholder - actual OpenAI integration requires:
  // 1. Reading image and audio files
  // 2. Base64 encoding
  // 3. Making HTTPS POST to OpenAI API
  // 4. Parsing response
  
  String body = server.arg("plain");
  
  // For now, return mock response
  String mockResponse = "Image shows: [placeholder description]. Audio transcription: [placeholder text]. Analysis: The content appears to be a test recording with visual and audio components.";
  
  server.send(200, "application/json", 
    "{\"success\":true,\"response\":\"" + mockResponse + "\"}");
}

void handleAnalysisSave() {
  // Save analysis results to SD card
  String body = server.arg("plain");
  
  File file = SD_MMC.open("/analysis/result_" + String(millis()) + ".txt", FILE_WRITE);
  if (file) {
    file.print(body);
    file.close();
    server.send(200, "application/json", 
      "{\"success\":true,\"filename\":\"result_" + String(millis()) + ".txt\"}");
  } else {
    server.send(500, "application/json", "{\"success\":false}");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize SD Card
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  
  // Create directories
  SD_MMC.mkdir("/images");
  SD_MMC.mkdir("/audio");
  SD_MMC.mkdir("/analysis");
  
  // Count existing files
  File dir = SD_MMC.open("/images");
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) imageCount++;
    file = dir.openNextFile();
  }
  
  dir = SD_MMC.open("/audio");
  file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) audioCount++;
    file = dir.openNextFile();
  }
  
  // Setup camera
  setupCamera();
  
  // Setup WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/capture", handleCapture);
  server.on("/image/latest", handleImageLatest);
  server.on("/image", handleImage);
  server.on("/list/images", handleListImages);
  server.on("/delete/image", HTTP_DELETE, handleDeleteImage);
  server.on("/audio/record", handleAudioRecord);
  server.on("/audio", handleAudioStream);
  server.on("/list/audio", handleListAudio);
  server.on("/delete/audio", HTTP_DELETE, handleDeleteAudio);
  server.on("/audio/speaker", handleSpeakerPlayback);
  server.on("/openai/analyze", HTTP_POST, handleOpenAIAnalyze);
  server.on("/analysis/save", HTTP_POST, handleAnalysisSave);
  
  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
}
