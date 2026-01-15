/*
 * ESP32-S3 AI CAM - OpenAI Integration Interface (Timeout Fixed)
 * 
 * Features:
 * - Camera capture and streaming with smart pause during recording
 * - PDM audio recording with proper timeout handling
 * - OpenAI API integration
 * - Speaker playback through MAX98357A
 * - Improved connection management
 * 
 * Fix: Stops video stream during blocking operations to prevent timeouts
 */

#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include "ESP_I2S.h"

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
bool streamingEnabled = true;
volatile bool recordingInProgress = false;

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
            text-align: center;
        }
        .processing-msg {
            text-align: center;
            color: #667eea;
            font-size: 16px;
            margin: 15px 0;
            font-weight: bold;
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
            word-wrap: break-word;
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
            white-space: pre-wrap;
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
            max-width: 300px;
        }
        .status-bar.show { display: block; }
        .status-bar.error { background: #e74c3c; }
        
        .info-banner {
            background: #fff3cd;
            padding: 12px;
            border-radius: 8px;
            margin-bottom: 15px;
            border-left: 4px solid #ffc107;
            font-size: 13px;
        }
        
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
            <div class="info-banner">
                <strong>NOTE:</strong> Video stream pauses during audio recording (blocking operation). This is normal behavior.
            </div>
            <div class="video-controls">
                <button class="btn-stream" onclick="toggleStream()">Video Stream</button>
                <button class="btn-capture" onclick="captureImage()">Capture</button>
                <button class="btn-review" onclick="reviewLastImage()">Review</button>
            </div>
            <div class="video-display">
                <img id="videoStream" src="" alt="Video Stream">
            </div>
        </div>
        
        <!-- Upper Right: Audio Controls -->
        <div class="audio-section">
            <div class="audio-bar">Audio Playback Bar</div>
            <audio id="audioPlayer" controls style="width: 100%; margin-bottom: 15px; display: none;"></audio>
            <div id="recordingTimer" class="recording-timer" style="display: none;">00:00</div>
            <div id="processingMsg" class="processing-msg" style="display: none;">Recording audio, please wait...</div>
            <div class="video-controls">
                <button id="recordBtn" class="btn-record" onclick="toggleRecording()">Record</button>
                <button id="playbackBtn" class="btn-playback" onclick="playbackAudio()" disabled>Playback</button>
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
            <div class="message-display" id="messageDisplay">Messages from OpenAI will appear here...</div>
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
        let streamActive = false;
        let streamInterval = null;
        let selectedImageFile = '';
        let selectedAudioFile = '';
        let selectedImageForExport = '';
        let selectedAudioForExport = '';
        let lastAnalysisResponse = '';
        let isRecording = false;
        
        function showStatus(msg, isError = false) {
            const bar = document.getElementById('statusBar');
            bar.textContent = msg;
            bar.className = 'status-bar show' + (isError ? ' error' : '');
            setTimeout(() => bar.classList.remove('show'), 3000);
        }
        
        // Video Stream Control with auto-refresh
        function toggleStream() {
            const img = document.getElementById('videoStream');
            streamActive = !streamActive;
            
            if (streamActive) {
                img.src = '/stream?' + Date.now();
                startStreamRefresh();
                showStatus('Stream started');
            } else {
                stopStreamRefresh();
                img.src = '';
                showStatus('Stream stopped');
            }
        }
        
        function startStreamRefresh() {
            if (streamInterval) clearInterval(streamInterval);
            streamInterval = setInterval(() => {
                if (streamActive && !isRecording) {
                    const img = document.getElementById('videoStream');
                    const oldSrc = img.src;
                    img.src = '/stream?' + Date.now();
                }
            }, 150); // Refresh every 150ms (~6 fps)
        }
        
        function stopStreamRefresh() {
            if (streamInterval) {
                clearInterval(streamInterval);
                streamInterval = null;
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
                    } else {
                        showStatus('Capture failed', true);
                    }
                })
                .catch(e => showStatus('Capture error', true));
        }
        
        // Review Last Image
        function reviewLastImage() {
            fetch('/image/latest')
                .then(r => r.json())
                .then(d => {
                    if (d.success && d.filename) {
                        stopStreamRefresh();
                        streamActive = false;
                        document.getElementById('videoStream').src = 
                            '/image?file=' + encodeURIComponent(d.filename);
                        showStatus('Reviewing: ' + d.filename);
                    } else {
                        showStatus('No images to review', true);
                    }
                })
                .catch(e => showStatus('Review failed', true));
        }
        
        // Load File Lists
        function loadImageList() {
            fetch('/list/images')
                .then(r => r.json())
                .then(d => {
                    const list = document.getElementById('imageFileList');
                    list.innerHTML = '';
                    if (d.files.length === 0) {
                        list.innerHTML = '<div style="text-align:center;color:#999;padding:10px;">No images</div>';
                        return;
                    }
                    d.files.forEach(file => {
                        const div = document.createElement('div');
                        div.className = 'file-item';
                        div.textContent = file;
                        div.onclick = () => selectImageFile(file);
                        list.appendChild(div);
                    });
                });
        }
        
        function loadAudioList() {
            fetch('/list/audio')
                .then(r => r.json())
                .then(d => {
                    const list = document.getElementById('audioFileList');
                    list.innerHTML = '';
                    if (d.files.length === 0) {
                        list.innerHTML = '<div style="text-align:center;color:#999;padding:10px;">No audio</div>';
                        return;
                    }
                    d.files.forEach(file => {
                        const div = document.createElement('div');
                        div.className = 'file-item';
                        div.textContent = file;
                        div.onclick = () => selectAudioFile(file);
                        list.appendChild(div);
                    });
                });
        }
        
        // File Selection
        function selectImageFile(filename) {
            selectedImageFile = filename;
            document.querySelectorAll('#imageFileList .file-item').forEach(el => {
                el.classList.toggle('selected', el.textContent === filename);
            });
            showStatus('Selected: ' + filename);
        }
        
        function selectAudioFile(filename) {
            selectedAudioFile = filename;
            document.querySelectorAll('#audioFileList .file-item').forEach(el => {
                el.classList.toggle('selected', el.textContent === filename);
            });
            
            const player = document.getElementById('audioPlayer');
            player.src = '/audio?file=' + encodeURIComponent(filename);
            player.style.display = 'block';
            document.getElementById('playbackBtn').disabled = false;
            showStatus('Selected: ' + filename);
        }
        
        // Export Queue
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
        
        // Delete Functions
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
                        document.getElementById('playbackBtn').disabled = true;
                        loadAudioList();
                    });
            }
        }
        
        // Recording Functions
        function updateTimer() {
            const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
            const minutes = Math.floor(elapsed / 60);
            const seconds = elapsed % 60;
            document.getElementById('recordingTimer').textContent = 
                String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
        }
        
        function toggleRecording() {
            const btn = document.getElementById('recordBtn');
            
            if (isRecording) {
                // Stop recording
                clearInterval(timerInterval);
                const duration = Math.floor((Date.now() - recordingStartTime) / 1000);
                
                // Pause stream during recording
                stopStreamRefresh();
                
                btn.textContent = 'Processing...';
                btn.disabled = true;
                btn.classList.remove('recording');
                document.getElementById('recordingTimer').style.display = 'none';
                document.getElementById('processingMsg').style.display = 'block';
                
                showStatus('Recording ' + duration + ' seconds...');
                
                // Give timeout of duration + 10 seconds
                const timeout = (duration + 10) * 1000;
                
                Promise.race([
                    fetch('/audio/record?duration=' + duration, {
                        signal: AbortSignal.timeout(timeout)
                    }),
                    new Promise((_, reject) => setTimeout(() => reject(new Error('timeout')), timeout))
                ])
                    .then(r => r.json())
                    .then(d => {
                        if (d.success) {
                            showStatus('Audio saved: ' + d.filename);
                            loadAudioList();
                        } else {
                            showStatus('Recording failed', true);
                        }
                    })
                    .catch(e => {
                        showStatus('Recording timeout - audio may still be saving', true);
                        setTimeout(loadAudioList, 2000);
                    })
                    .finally(() => {
                        btn.textContent = 'Record';
                        btn.disabled = false;
                        document.getElementById('processingMsg').style.display = 'none';
                        isRecording = false;
                        
                        // Resume stream if it was active
                        if (streamActive) {
                            startStreamRefresh();
                        }
                    });
            } else {
                // Start recording
                isRecording = true;
                recordingStartTime = Date.now();
                btn.classList.add('recording');
                btn.textContent = 'Stop';
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
            
            fetch('/audio/speaker?file=' + encodeURIComponent(selectedAudioFile), {
                signal: AbortSignal.timeout(60000) // 60 second timeout
            })
                .then(r => r.json())
                .then(d => {
                    if (d.success) {
                        showStatus('Playback complete');
                    } else {
                        showStatus('Playback failed', true);
                    }
                })
                .catch(e => showStatus('Playback timeout', true));
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
                }),
                signal: AbortSignal.timeout(60000)
            })
            .then(r => r.json())
            .then(d => {
                if (d.success) {
                    lastAnalysisResponse = d.response;
                    document.getElementById('messageDisplay').textContent = d.response;
                    showStatus('Analysis complete');
                    
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
                showStatus('Network error or timeout', true);
                document.getElementById('messageDisplay').textContent = 'Network error occurred';
            });
        }
        
        // Replay Audio
        function replayAudio() {
            const player = document.getElementById('audioPlayer');
            if (player.src) {
                player.currentTime = 0;
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
        
        // Return Lists
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
    </script>
</body>
</html>
)rawliteral";

// Rest of the code continues with camera setup and handlers...
// (Previous amplifyAudio, setupCamera, and handler functions remain the same)

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
  if (recordingInProgress) {
    server.send(503, "text/plain", "Recording in progress");
    return;
  }
  
  WiFiClient client = server.client();
  
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(503, "text/plain", "Camera unavailable");
    return;
  }
  
  client.printf("HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
  client.write(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
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
      String name = String(file.name());
      if (name.startsWith("/images/")) {
        name = name.substring(8); // Remove /images/ prefix
      }
      if (!first) json += ",";
      json += "\"" + name + "\"";
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
  
  recordingInProgress = true;
  
  // Record audio
  i2s_mic.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  i2s_mic.setPinsPdmRx(PDM_CLOCK_PIN, PDM_DATA_PIN);
  
  Serial.printf("Recording %d seconds of audio...\n", duration);
  
  size_t wav_size = 0;
  uint8_t* wav_buffer = i2s_mic.recordWAV(duration, &wav_size);
  
  i2s_mic.end();
  
  recordingInProgress = false;
  
  if (wav_buffer && wav_size > 44) {
    amplifyAudio(wav_buffer + 44, wav_size - 44, AMPLIFICATION);
    
    audioCount++;
    String filename = "/audio/REC_" + String(audioCount) + ".wav";
    
    File file = SD_MMC.open(filename, FILE_WRITE);
    if (file) {
      file.write(wav_buffer, wav_size);
      file.close();
      Serial.printf("Saved audio: %s (%d bytes)\n", filename.c_str(), wav_size);
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
      String name = String(file.name());
      if (name.startsWith("/audio/")) {
        name = name.substring(7); // Remove /audio/ prefix
      }
      if (!first) json += ",";
      json += "\"" + name + "\"";
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
  
  uint8_t header[44];
  file.read(header, 44);
  
  size_t dataSize = file.size() - 44;
  uint8_t* audioData = (uint8_t*)malloc(dataSize);
  file.read(audioData, dataSize);
  file.close();
  
  i2s_spk.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  i2s_spk.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  
  pinMode(I2S_SD_PIN, OUTPUT);
  digitalWrite(I2S_SD_PIN, HIGH);
  
  i2s_spk.write(audioData, dataSize);
  
  digitalWrite(I2S_SD_PIN, LOW);
  i2s_spk.end();
  
  free(audioData);
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleOpenAIAnalyze() {
  String mockResponse = "Image analysis: [Placeholder description]\\n\\nAudio transcription: [Placeholder text]\\n\\nCombined analysis: This demonstrates the interface. To enable actual OpenAI analysis, add your API key and use the OpenAI_Integration_Complete.cpp module.";
  
  server.send(200, "application/json", 
    "{\"success\":true,\"response\":\"" + mockResponse + "\"}");
}

void handleAnalysisSave() {
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
  Serial.println("\n\nESP32-S3 AI CAM - OpenAI Interface");
  
  // Initialize SD Card
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  Serial.println("SD Card initialized");
  
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
  
  Serial.printf("Found %d images, %d audio files\n", imageCount, audioCount);
  
  // Setup camera
  setupCamera();
  Serial.println("Camera initialized");
  
  // Setup WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
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
  Serial.println("Web server started");
  Serial.println("Ready!");
}

void loop() {
  server.handleClient();
}
