/*
 * ESP32-S3 AI CAM - OpenAI Integration (UI Fixed)
 * 
 * Fixed:
 * - Replay button now replays the SENT audio (the question)
 * - Return Test list shows previous analysis results (text)
 * - Return Audio list shows sent audio files for reference
 * - Proper display of analysis in message box
 */

// [All includes same as before]
#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "ESP_I2S.h"
#include "mbedtls/base64.h"

const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* OPENAI_API_KEY = "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";
const char* OPENAI_WHISPER_URL = "https://api.openai.com/v1/audio/transcriptions";
const char* OPENAI_CHAT_URL = "https://api.openai.com/v1/chat/completions";

const char* OPENAI_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

WebServer server(80);

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 5
#define SIOD_GPIO_NUM 8
#define SIOC_GPIO_NUM 9
#define Y9_GPIO_NUM 4
#define Y8_GPIO_NUM 6
#define Y7_GPIO_NUM 7
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 17
#define Y4_GPIO_NUM 21
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 16
#define VSYNC_GPIO_NUM 1
#define HREF_GPIO_NUM 2
#define PCLK_GPIO_NUM 15
#define SD_MMC_CMD 11
#define SD_MMC_CLK 12
#define SD_MMC_D0 13
#define PDM_DATA_PIN GPIO_NUM_39
#define PDM_CLOCK_PIN GPIO_NUM_38
#define I2S_BCLK 45
#define I2S_LRC 46
#define I2S_DOUT 42
#define I2S_SD_PIN 41
#define SAMPLE_RATE 16000
#define AMPLIFICATION 10

int imageCount = 0;
int audioCount = 0;
bool streamingEnabled = true;
volatile bool recordingInProgress = false;

I2SClass i2s_mic;
I2SClass i2s_spk;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>ESP32-S3 OpenAI</title><meta name="viewport" content="width=device-width, initial-scale=1">
<style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background:#f5f5f5;padding:20px}.main-grid{display:grid;grid-template-columns:1.5fr 1fr;grid-template-rows:auto auto auto;gap:20px;max-width:1400px;margin:0 auto}.video-section{grid-column:1;grid-row:1;background:white;border-radius:12px;border:2px solid #ddd;padding:20px}.audio-section{grid-column:2;grid-row:1;background:white;border-radius:12px;border:2px solid #ddd;padding:20px}.files-section{grid-column:2;grid-row:2;display:grid;grid-template-columns:1fr 1fr;gap:15px}.message-section{grid-column:1;grid-row:2/4;background:white;border-radius:12px;border:2px solid #ddd;padding:20px}.return-section{grid-column:2;grid-row:3;display:grid;grid-template-columns:1fr 1fr;gap:15px}.video-controls{display:flex;gap:10px;margin-bottom:15px;flex-wrap:wrap}.video-display{background:#000;border-radius:8px;position:relative;padding-bottom:75%;overflow:hidden}.video-display img{position:absolute;width:100%;height:100%;object-fit:contain}.audio-bar{background:#f0f0f0;border-radius:8px;padding:15px;margin-bottom:15px;text-align:center;font-weight:bold;color:#333}.recording-timer{font-size:32px;color:#e74c3c;font-family:monospace;margin:15px 0;text-align:center}.processing-msg{text-align:center;color:#667eea;font-size:16px;margin:15px 0;font-weight:bold}.file-column,.return-column{background:white;border-radius:12px;border:2px solid #ddd;padding:15px}.file-column h3,.return-column h3{margin-bottom:10px;color:#333;font-size:14px;border-bottom:2px solid #667eea;padding-bottom:8px}.file-list,.return-list{max-height:200px;overflow-y:auto;margin-bottom:10px}.file-item{background:#f8f8f8;padding:8px;margin-bottom:5px;border-radius:6px;cursor:pointer;border:2px solid transparent;font-size:12px}.file-item:hover{background:#e8e8e8}.file-item.selected{border-color:#667eea;background:#e8eeff}.export-label{font-size:11px;color:#666;margin-top:10px;padding:8px;background:#f0f0f0;border-radius:4px;min-height:30px;word-wrap:break-word}.message-display{background:#f9f9f9;border-radius:8px;padding:15px;min-height:200px;max-height:400px;overflow-y:auto;margin-bottom:15px;font-size:14px;line-height:1.6;border:1px solid #ddd;white-space:pre-wrap}.message-controls{display:flex;gap:10px}button{padding:10px 20px;font-size:14px;font-weight:600;border:none;border-radius:8px;cursor:pointer;transition:all .2s;color:white}button:hover:not(:disabled){transform:translateY(-2px);box-shadow:0 4px 12px rgba(0,0,0,.15)}button:disabled{opacity:.5;cursor:not-allowed}.btn-stream{background:#3498db}.btn-capture{background:#667eea}.btn-review{background:#9b59b6}.btn-record{background:#e74c3c}.btn-playback{background:#2ecc71}.btn-delete{background:#e67e22}.btn-save{background:#16a085}.btn-send{background:#2c3e50;flex:1}.btn-replay{background:#34495e;flex:1}.btn-record.recording{animation:pulse 1.5s infinite}@keyframes pulse{0%,100%{opacity:1}50%{opacity:.6}}.status-bar{position:fixed;top:20px;right:20px;background:#2ecc71;color:white;padding:12px 20px;border-radius:8px;box-shadow:0 4px 12px rgba(0,0,0,.2);display:none;z-index:1000;max-width:300px}.status-bar.show{display:block}.status-bar.error{background:#e74c3c}.info-banner{background:#fff3cd;padding:12px;border-radius:8px;margin-bottom:15px;border-left:4px solid #ffc107;font-size:13px}@media (max-width:1024px){.main-grid{grid-template-columns:1fr}.video-section,.audio-section,.files-section,.message-section,.return-section{grid-column:1}}</style>
</head>
<body><div id="statusBar" class="status-bar"></div><div class="main-grid"><div class="video-section"><div class="info-banner"><strong>NOTE:</strong> "Replay" plays your original audio question. "Return Test" shows analysis history. "Return Audio" shows sent audio files.</div><div class="video-controls"><button class="btn-stream" onclick="toggleStream()">Video Stream</button><button class="btn-capture" onclick="captureImage()">Capture</button><button class="btn-review" onclick="reviewLastImage()">Review</button></div><div class="video-display"><img id="videoStream" src=""></div></div><div class="audio-section"><div class="audio-bar">Audio Playback Bar</div><audio id="audioPlayer" controls style="width:100%;margin-bottom:15px;display:none"></audio><div id="recordingTimer" class="recording-timer" style="display:none">00:00</div><div id="processingMsg" class="processing-msg" style="display:none">Recording audio, please wait...</div><div class="video-controls"><button id="recordBtn" class="btn-record" onclick="toggleRecording()">Record</button><button id="playbackBtn" class="btn-playback" onclick="playbackAudio()" disabled>Playback</button><button class="btn-delete" onclick="deleteSelectedAudio()">Delete</button></div></div><div class="files-section"><div class="file-column"><h3>Image Files</h3><div class="file-list" id="imageFileList"></div><div class="video-controls" style="margin-bottom:10px"><button class="btn-save" onclick="saveImageToExport()" style="flex:1">Save</button><button class="btn-delete" onclick="deleteSelectedImage()" style="flex:1">Delete</button></div><div class="export-label" id="imageExportLabel">Image for Export</div></div><div class="file-column"><h3>Audio Files</h3><div class="file-list" id="audioFileList"></div><div class="video-controls" style="margin-bottom:10px"><button class="btn-save" onclick="saveAudioToExport()" style="flex:1">Save</button><button class="btn-delete" onclick="deleteSelectedAudio()" style="flex:1">Delete</button></div><div class="export-label" id="audioExportLabel">Audio for Export</div></div></div><div class="message-section"><h3 style="margin-bottom:15px;color:#333">Returned Test Message Display</h3><div class="message-display" id="messageDisplay">OpenAI analysis results will appear here...</div><div class="message-controls"><button class="btn-send" onclick="sendToOpenAI()">Send</button><button class="btn-replay" onclick="replayOriginalAudio()">Replay</button><button class="btn-save" onclick="saveAnalysis()">Save</button></div></div><div class="return-section"><div class="return-column"><h3>Return Test</h3><div class="return-list" id="returnTextList"></div></div><div class="return-column"><h3>Return Audio</h3><div class="return-list" id="returnAudioList"></div></div></div></div><script>let recordingStartTime=0,timerInterval=null,streamActive=!1,streamInterval=null,selectedImageFile="",selectedAudioFile="",selectedImageForExport="",selectedAudioForExport="",lastAnalysisResponse="",lastSentAudioFile="",isRecording=!1;function showStatus(e,t=!1){const a=document.getElementById("statusBar");a.textContent=e,a.className="status-bar show"+(t?" error":""),setTimeout((()=>a.classList.remove("show")),3e3)}function toggleStream(){const e=document.getElementById("videoStream");streamActive=!streamActive,streamActive?(e.src="/stream?"+Date.now(),startStreamRefresh(),showStatus("Stream started")):(stopStreamRefresh(),e.src="",showStatus("Stream stopped"))}function startStreamRefresh(){streamInterval&&clearInterval(streamInterval),streamInterval=setInterval((()=>{streamActive&&!isRecording&&(document.getElementById("videoStream").src="/stream?"+Date.now())}),150)}function stopStreamRefresh(){streamInterval&&(clearInterval(streamInterval),streamInterval=null)}function captureImage(){fetch("/capture").then((e=>e.json())).then((e=>{e.success?(showStatus("Image captured: "+e.filename),loadImageList()):showStatus("Capture failed",!0)})).catch((e=>showStatus("Capture error",!0)))}function reviewLastImage(){fetch("/image/latest").then((e=>e.json())).then((e=>{e.success&&e.filename?(stopStreamRefresh(),streamActive=!1,document.getElementById("videoStream").src="/image?file="+encodeURIComponent(e.filename),showStatus("Reviewing: "+e.filename)):showStatus("No images to review",!0)})).catch((e=>showStatus("Review failed",!0)))}function loadImageList(){fetch("/list/images").then((e=>e.json())).then((e=>{const t=document.getElementById("imageFileList");if(t.innerHTML="",0===e.files.length)return void(t.innerHTML='<div style="text-align:center;color:#999;padding:10px">No images</div>');e.files.forEach((e=>{const a=document.createElement("div");a.className="file-item",a.textContent=e,a.onclick=()=>selectImageFile(e),t.appendChild(a)}))}))}function loadAudioList(){fetch("/list/audio").then((e=>e.json())).then((e=>{const t=document.getElementById("audioFileList");if(t.innerHTML="",0===e.files.length)return void(t.innerHTML='<div style="text-align:center;color:#999;padding:10px">No audio</div>');e.files.forEach((e=>{const a=document.createElement("div");a.className="file-item",a.textContent=e,a.onclick=()=>selectAudioFile(e),t.appendChild(a)}))}))}function selectImageFile(e){selectedImageFile=e,document.querySelectorAll("#imageFileList .file-item").forEach((t=>{t.classList.toggle("selected",t.textContent===e)})),showStatus("Selected: "+e)}function selectAudioFile(e){selectedAudioFile=e,document.querySelectorAll("#audioFileList .file-item").forEach((t=>{t.classList.toggle("selected",t.textContent===e)}));const t=document.getElementById("audioPlayer");t.src="/audio?file="+encodeURIComponent(e),t.style.display="block",document.getElementById("playbackBtn").disabled=!1,showStatus("Selected: "+e)}function saveImageToExport(){if(!selectedImageFile)return void showStatus("No image selected",!0);selectedImageForExport=selectedImageFile,document.getElementById("imageExportLabel").textContent="Export: "+selectedImageFile,showStatus("Ready for export: "+selectedImageFile)}function saveAudioToExport(){if(!selectedAudioFile)return void showStatus("No audio selected",!0);selectedAudioForExport=selectedAudioFile,document.getElementById("audioExportLabel").textContent="Export: "+selectedAudioFile,showStatus("Ready for export: "+selectedAudioFile)}function deleteSelectedImage(){selectedImageFile?confirm("Delete "+selectedImageFile+"?")&&fetch("/delete/image?file="+encodeURIComponent(selectedImageFile),{method:"DELETE"}).then((()=>{showStatus("Deleted: "+selectedImageFile),selectedImageFile="",loadImageList()})):showStatus("No image selected",!0)}function deleteSelectedAudio(){selectedAudioFile?confirm("Delete "+selectedAudioFile+"?")&&fetch("/delete/audio?file="+encodeURIComponent(selectedAudioFile),{method:"DELETE"}).then((()=>{showStatus("Deleted: "+selectedAudioFile),selectedAudioFile="",document.getElementById("audioPlayer").style.display="none",document.getElementById("playbackBtn").disabled=!0,loadAudioList()})):showStatus("No audio selected",!0)}function updateTimer(){const e=Math.floor((Date.now()-recordingStartTime)/1e3),t=Math.floor(e/60),a=e%60;document.getElementById("recordingTimer").textContent=String(t).padStart(2,"0")+":"+String(a).padStart(2,"0")}function toggleRecording(){const e=document.getElementById("recordBtn");if(isRecording){clearInterval(timerInterval);const t=Math.floor((Date.now()-recordingStartTime)/1e3);stopStreamRefresh(),e.textContent="Processing...",e.disabled=!0,e.classList.remove("recording"),document.getElementById("recordingTimer").style.display="none",document.getElementById("processingMsg").style.display="block",showStatus("Recording "+t+" seconds...");const a=1e3*(t+10);Promise.race([fetch("/audio/record?duration="+t,{signal:AbortSignal.timeout(a)}),new Promise(((e,t)=>setTimeout((()=>t(new Error("timeout"))),a)))]).then((e=>e.json())).then((e=>{e.success?(showStatus("Audio saved: "+e.filename),loadAudioList()):showStatus("Recording failed",!0)})).catch((e=>{showStatus("Recording timeout - audio may still be saving",!0),setTimeout(loadAudioList,2e3)})).finally((()=>{e.textContent="Record",e.disabled=!1,document.getElementById("processingMsg").style.display="none",isRecording=!1,streamActive&&startStreamRefresh()}))}else isRecording=!0,recordingStartTime=Date.now(),e.classList.add("recording"),e.textContent="Stop",document.getElementById("recordingTimer").style.display="block",timerInterval=setInterval(updateTimer,100),showStatus("Recording started")}function playbackAudio(){selectedAudioFile?(showStatus("Playing on speaker..."),fetch("/audio/speaker?file="+encodeURIComponent(selectedAudioFile),{signal:AbortSignal.timeout(6e4)}).then((e=>e.json())).then((e=>{e.success?showStatus("Playback complete"):showStatus("Playback failed",!0)})).catch((e=>showStatus("Playback timeout",!0)))):showStatus("No audio selected",!0)}function sendToOpenAI(){if(!selectedImageForExport||!selectedAudioForExport)return void showStatus("Select image and audio for export first",!0);lastSentAudioFile=selectedAudioForExport,showStatus("Sending to OpenAI... (30-60 seconds)"),document.getElementById("messageDisplay").textContent="Processing with OpenAI...\n\nStep 1: Transcribing audio...\nStep 2: Analyzing image...\n\nPlease wait...",fetch("/openai/analyze",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({image:selectedImageForExport,audio:selectedAudioForExport}),signal:AbortSignal.timeout(9e4)}).then((e=>e.json())).then((e=>{if(e.success){lastAnalysisResponse=e.response;const t=e.response.replace(/\\n/g,"\n");document.getElementById("messageDisplay").textContent=t,showStatus("Analysis complete"),addToReturnText(t,selectedImageForExport,selectedAudioForExport)}else showStatus("OpenAI request failed",!0),document.getElementById("messageDisplay").textContent="Error: "+(e.error||"Unknown error")})).catch((e=>{showStatus("Network error or timeout",!0),document.getElementById("messageDisplay").textContent="Network error. Check serial monitor."}))}function replayOriginalAudio(){if(!lastSentAudioFile)return void showStatus("No audio to replay - send an analysis first",!0);const e=document.getElementById("audioPlayer");e.src="/audio?file="+encodeURIComponent(lastSentAudioFile),e.style.display="block",e.currentTime=0,e.play(),showStatus("Replaying your original question")}function saveAnalysis(){lastAnalysisResponse?fetch("/analysis/save",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({text:lastAnalysisResponse,image:selectedImageForExport,audio:selectedAudioForExport})}).then((e=>e.json())).then((e=>{e.success&&showStatus("Analysis saved: "+e.filename)})):showStatus("No analysis to save",!0)}function addToReturnText(e,t,a){const n=document.getElementById("returnTextList"),o=document.createElement("div");o.className="file-item";const s=new Date,i=s.toLocaleTimeString();o.textContent=i+" - "+e.substring(0,30)+"...",o.onclick=()=>{document.getElementById("messageDisplay").textContent=e},n.insertBefore(o,n.firstChild);const l=document.getElementById("returnAudioList"),r=document.createElement("div");r.className="file-item",r.textContent=i+" - "+a,r.onclick=()=>{const e=document.getElementById("audioPlayer");e.src="/audio?file="+encodeURIComponent(a),e.style.display="block",e.play()},l.insertBefore(r,l.firstChild)}loadImageList(),loadAudioList();</script>
</body>
</html>
)rawliteral";

// [All backend code same as previous version - keeping concise]
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

String base64EncodeFileOptimized(String filepath) {
  File file = SD_MMC.open(filepath, FILE_READ);
  if (!file) { Serial.println("ERROR: Failed to open file"); return ""; }
  size_t fileSize = file.size();
  Serial.printf("File size: %d bytes\n", fileSize);
  uint8_t* inputBuffer = (uint8_t*)ps_malloc(fileSize);
  if (!inputBuffer) inputBuffer = (uint8_t*)malloc(fileSize);
  if (!inputBuffer) { Serial.println("ERROR: Cannot allocate input buffer"); file.close(); return ""; }
  file.read(inputBuffer, fileSize);
  file.close();
  size_t base64Size = 4 * ((fileSize + 2) / 3);
  char* base64Output = (char*)ps_malloc(base64Size + 10);
  if (!base64Output) base64Output = (char*)malloc(base64Size + 10);
  if (!base64Output) { Serial.println("ERROR: Cannot allocate output buffer"); free(inputBuffer); return ""; }
  size_t olen;
  int ret = mbedtls_base64_encode((unsigned char*)base64Output, base64Size + 10, &olen, inputBuffer, fileSize);
  free(inputBuffer);
  if (ret != 0) { Serial.printf("ERROR: Base64 encoding failed, code: %d\n", ret); free(base64Output); return ""; }
  base64Output[olen] = '\0';
  Serial.printf("Encoded length: %d\n", olen);
  String result = String(base64Output);
  free(base64Output);
  return result;
}

String transcribeAudioWithWhisper(String audioFilePath) {
  Serial.println("\n=== Whisper API ===");
  File audioFile = SD_MMC.open(audioFilePath, FILE_READ);
  if (!audioFile) return "Error: Could not open audio file";
  size_t fileSize = audioFile.size();
  uint8_t* audioData = (uint8_t*)ps_malloc(fileSize);
  if (!audioData) audioData = (uint8_t*)malloc(fileSize);
  if (!audioData) { audioFile.close(); return "Error: Memory allocation failed"; }
  audioFile.read(audioData, fileSize);
  audioFile.close();
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  HTTPClient https;
  https.begin(client, OPENAI_WHISPER_URL);
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  https.setTimeout(30000);
  String boundary = "----ESP32" + String(millis());
  https.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  String bodyStart = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String bodyEnd = "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n--" + boundary + "--\r\n";
  size_t totalSize = bodyStart.length() + fileSize + bodyEnd.length();
  uint8_t* requestBody = (uint8_t*)ps_malloc(totalSize);
  if (!requestBody) requestBody = (uint8_t*)malloc(totalSize);
  if (!requestBody) { free(audioData); return "Error: Memory allocation failed"; }
  size_t offset = 0;
  memcpy(requestBody + offset, bodyStart.c_str(), bodyStart.length()); offset += bodyStart.length();
  memcpy(requestBody + offset, audioData, fileSize); offset += fileSize;
  memcpy(requestBody + offset, bodyEnd.c_str(), bodyEnd.length());
  int httpCode = https.POST(requestBody, totalSize);
  String transcription = "";
  if (httpCode == 200) {
    String response = https.getString();
    Serial.println("Whisper Response: " + response);
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
      transcription = doc["text"].as<String>();
      Serial.println("SUCCESS: " + transcription);
    } else transcription = "Error: JSON parse failed";
  } else {
    Serial.printf("ERROR: Whisper returned %d\n", httpCode);
    transcription = "Error: Whisper API returned " + String(httpCode);
  }
  free(audioData);
  free(requestBody);
  https.end();
  return transcription;
}

String analyzeImageWithGPT4Vision(String imageFilePath, String audioTranscription) {
  Serial.println("\n=== GPT-4 Vision ===");
  String imageBase64 = base64EncodeFileOptimized(imageFilePath);
  if (imageBase64.length() == 0) return "Error: Base64 encoding failed";
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  HTTPClient https;
  https.begin(client, OPENAI_CHAT_URL);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
  https.setTimeout(60000);
  size_t docSize = imageBase64.length() + 4096;
  DynamicJsonDocument* doc = new DynamicJsonDocument(docSize);
  if (!doc) return "Error: JSON allocation failed";
  (*doc)["model"] = "gpt-4o";
  (*doc)["max_tokens"] = 500;
  JsonArray messages = doc->createNestedArray("messages");
  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  JsonArray content = userMsg.createNestedArray("content");
  JsonObject textPart = content.createNestedObject();
  textPart["type"] = "text";
  String prompt = "Analyze this image concisely. ";
  if (audioTranscription.length() > 0 && !audioTranscription.startsWith("Error")) {
    prompt += "Audio: \"" + audioTranscription + "\". Consider both.";
  }
  textPart["text"] = prompt;
  JsonObject imagePart = content.createNestedObject();
  imagePart["type"] = "image_url";
  JsonObject imageUrl = imagePart.createNestedObject("image_url");
  imageUrl["url"] = "data:image/jpeg;base64," + imageBase64;
  String requestBody;
  serializeJson(*doc, requestBody);
  delete doc;
  Serial.printf("Request size: %d bytes\n", requestBody.length());
  int httpCode = https.POST(requestBody);
  requestBody = "";
  String analysis = "";
  if (httpCode == 200) {
    String response = https.getString();
    Serial.println("GPT-4 response received");
    DynamicJsonDocument responseDoc(4096);
    if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
      analysis = responseDoc["choices"][0]["message"]["content"].as<String>();
      Serial.println("SUCCESS");
    } else analysis = "Error: JSON parse failed";
  } else {
    Serial.printf("ERROR: GPT-4 returned %d\n", httpCode);
    analysis = "Error: GPT-4 returned " + String(httpCode);
  }
  https.end();
  return analysis;
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
  esp_camera_init(&config);
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
}

void handleRoot() { server.send_P(200, "text/html", index_html); }
void handleStream() {
  if (recordingInProgress) { server.send(503, "text/plain", "Recording"); return; }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { server.send(503, "text/plain", "Unavailable"); return; }
  WiFiClient client = server.client();
  client.printf("HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}
void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { server.send(500, "application/json", "{\"success\":false}"); return; }
  imageCount++;
  File file = SD_MMC.open("/images/IMG_" + String(imageCount) + ".jpg", FILE_WRITE);
  if (file) { file.write(fb->buf, fb->len); file.close();
    server.send(200, "application/json", "{\"success\":true,\"filename\":\"IMG_" + String(imageCount) + ".jpg\"}"); }
  else server.send(500, "application/json", "{\"success\":false}");
  esp_camera_fb_return(fb);
}
void handleImageLatest() {
  imageCount == 0 ? server.send(404, "application/json", "{\"success\":false}") :
    server.send(200, "application/json", "{\"success\":true,\"filename\":\"IMG_" + String(imageCount) + ".jpg\"}");
}
void handleImage() {
  File file = SD_MMC.open("/images/" + server.arg("file"), FILE_READ);
  if (!file) { server.send(404, "text/plain", "Not found"); return; }
  server.streamFile(file, "image/jpeg");
  file.close();
}
void handleListImages() {
  File dir = SD_MMC.open("/images");
  String json = "{\"files\":[";
  bool first = true;
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.startsWith("/images/")) name = name.substring(8);
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
  SD_MMC.remove("/images/" + server.arg("file")) ?
    server.send(200, "application/json", "{\"success\":true}") :
    server.send(500, "application/json", "{\"success\":false}");
}
void handleAudioRecord() {
  int duration = server.arg("duration").toInt();
  if (duration < 1 || duration > 60) { server.send(400, "application/json", "{\"success\":false}"); return; }
  recordingInProgress = true;
  i2s_mic.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  i2s_mic.setPinsPdmRx(PDM_CLOCK_PIN, PDM_DATA_PIN);
  size_t wav_size = 0;
  uint8_t* wav_buffer = i2s_mic.recordWAV(duration, &wav_size);
  i2s_mic.end();
  recordingInProgress = false;
  if (wav_buffer && wav_size > 44) {
    amplifyAudio(wav_buffer + 44, wav_size - 44, AMPLIFICATION);
    audioCount++;
    File file = SD_MMC.open("/audio/REC_" + String(audioCount) + ".wav", FILE_WRITE);
    if (file) { file.write(wav_buffer, wav_size); file.close();
      server.send(200, "application/json", "{\"success\":true,\"filename\":\"REC_" + String(audioCount) + ".wav\"}"); }
    else server.send(500, "application/json", "{\"success\":false}");
  } else server.send(500, "application/json", "{\"success\":false}");
  if (wav_buffer) free(wav_buffer);
}
void handleAudioStream() {
  File file = SD_MMC.open("/audio/" + server.arg("file"), FILE_READ);
  if (!file) { server.send(404, "text/plain", "Not found"); return; }
  server.streamFile(file, "audio/wav");
  file.close();
}
void handleListAudio() {
  File dir = SD_MMC.open("/audio");
  String json = "{\"files\":[";
  bool first = true;
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.startsWith("/audio/")) name = name.substring(7);
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
  SD_MMC.remove("/audio/" + server.arg("file")) ?
    server.send(200, "application/json", "{\"success\":true}") :
    server.send(500, "application/json", "{\"success\":false}");
}
void handleSpeakerPlayback() {
  File file = SD_MMC.open("/audio/" + server.arg("file"), FILE_READ);
  if (!file) { server.send(404, "application/json", "{\"success\":false}"); return; }
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
  Serial.println("\n========================================");
  Serial.println("OpenAI Analysis Request");
  Serial.println("========================================");
  
  DynamicJsonDocument reqDoc(512);
  deserializeJson(reqDoc, server.arg("plain"));
  
  String imageFile = reqDoc["image"].as<String>();
  String audioFile = reqDoc["audio"].as<String>();
  String imagePath = "/images/" + imageFile;
  String audioPath = "/audio/" + audioFile;
  
  if (!SD_MMC.exists(imagePath) || !SD_MMC.exists(audioPath)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"File not found\"}");
    return;
  }
  
  String transcription = transcribeAudioWithWhisper(audioPath);
  if (transcription.startsWith("Error")) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"" + transcription + "\"}");
    return;
  }
  
  String analysis = analyzeImageWithGPT4Vision(imagePath, transcription);
  if (analysis.startsWith("Error")) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"" + analysis + "\"}");
    return;
  }
  
  String result = "=== AUDIO TRANSCRIPTION ===\\n" + transcription + "\\n\\n=== IMAGE ANALYSIS ===\\n" + analysis;
  result.replace("\"", "\\\"");
  result.replace("\n", "\\n");
  
  server.send(200, "application/json", "{\"success\":true,\"response\":\"" + result + "\"}");
  
  Serial.println("========================================");
  Serial.println("TRANSCRIPTION: " + transcription);
  Serial.println("ANALYSIS: " + analysis);
  Serial.println("========================================\n");
}

void handleAnalysisSave() {
  File file = SD_MMC.open("/analysis/result_" + String(millis()) + ".txt", FILE_WRITE);
  if (file) { file.print(server.arg("plain")); file.close();
    server.send(200, "application/json", "{\"success\":true,\"filename\":\"result_" + String(millis()) + ".txt\"}"); }
  else server.send(500, "application/json", "{\"success\":false}");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("ESP32-S3 AI CAM - OpenAI (UI Fixed)");
  Serial.println("========================================\n");
  
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true)) { Serial.println("ERROR: SD failed"); return; }
  Serial.println("✓ SD Card OK");
  
  SD_MMC.mkdir("/images");
  SD_MMC.mkdir("/audio");
  SD_MMC.mkdir("/analysis");
  
  File dir = SD_MMC.open("/images");
  File file = dir.openNextFile();
  while (file) { if (!file.isDirectory()) imageCount++; file = dir.openNextFile(); }
  dir = SD_MMC.open("/audio");
  file = dir.openNextFile();
  while (file) { if (!file.isDirectory()) audioCount++; file = dir.openNextFile(); }
  Serial.printf("✓ %d images, %d audio\n", imageCount, audioCount);
  
  setupCamera();
  Serial.println("✓ Camera OK");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✓ WiFi OK");
  Serial.println("✓ IP: " + WiFi.localIP().toString());
  
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
  
  Serial.println("✓ Server started");
  Serial.printf("✓ Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("\n========================================");
  Serial.println("READY! " + WiFi.localIP().toString());
  Serial.println("========================================\n");
}

void loop() {
  server.handleClient();
}
