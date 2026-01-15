#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <OpenAI.h>
#include "camera.h"
#include "wav_header.h"
#include <SPI.h>
#include "SD.h"
#include "ESP_I2S.h"
#include "Audio.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ========== PINS ==========
#define BUTTON_PIN      0
#define LED_PIN         3
#define MIC_CLK         38
#define MIC_DATA        39
#define MAX_BCLK        45
#define MAX_LRCLK       46
#define MAX_DIN         42
#define MAX_GAIN        41
#define MAX_MODE        40
#define SD_CS           10
#define SD_MOSI         11
#define SD_MISO         13
#define SD_SCK          12
#define SAMPLE_RATE     16000
#define MAX_REC_SIZE    480000  // 15 seconds max

// ========== CREDENTIALS ==========
const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* api_key = "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";

// ========== OBJECTS ==========
OpenAI openai(api_key);
OpenAI_ChatCompletion chat(openai);
OpenAI_AudioTranscription audio(openai);
I2SClass I2S_MIC;
Audio audioPlayer;
WebServer server(80);

// ========== STATE ==========
bool isRecording = false;
bool isProcessing = false;
bool streamEnabled = true;
bool sdReady = false;
bool audioReady = false;
uint8_t *wavBuf = NULL;
size_t wavSize = 0;
String lastAudio = "";
unsigned long lastBtn = 0;
unsigned long lastFrame = 0;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== DFRobot ESP32-S3 AI Camera ===");
  Serial.printf("Heap: %d | PSRAM: %d\n\n", ESP.getFreeHeap(), ESP.getFreePsram());
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MAX_GAIN, OUTPUT);
  pinMode(MAX_MODE, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(MAX_GAIN, HIGH);   // 15dB gain
  digitalWrite(MAX_MODE, HIGH);   // Enable amp
  
  // Init SD
  Serial.print("SD Card... ");
  SPIClass spi(HSPI);
  spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, spi, 40000000)) {
    Serial.printf("OK (%.1fGB)\n", SD.cardSize() / (1024.0 * 1024.0 * 1024.0));
    if (!SD.exists("/audio")) SD.mkdir("/audio");
    sdReady = true;
  } else {
    Serial.println("FAIL");
  }
  
  // Init camera
  Serial.print("Camera... ");
  initCamera();
  Serial.println("OK");
  
  // Init audio player
  if (sdReady) {
    Serial.print("MAX98357... ");
    audioPlayer.setPinout(MAX_BCLK, MAX_LRCLK, MAX_DIN);
    audioPlayer.setVolume(15);
    audioReady = true;
    Serial.println("OK");
  }
  
  // Init microphone
  Serial.print("Microphone... ");
  I2S_MIC.setPinsPdmRx(MIC_CLK, MIC_DATA);
  I2S_MIC.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  
  // Test mic
  unsigned long start = millis();
  size_t total = 0;
  while (millis() - start < 1000) {
    size_t avail = I2S_MIC.available();
    if (avail > 0) {
      uint8_t buf[256];
      total += I2S_MIC.readBytes((char*)buf, min(avail, sizeof(buf)));
    }
    delay(10);
  }
  Serial.printf("OK (%d bytes/s)\n", total);
  
  // WiFi
  Serial.print("WiFi... ");
  WiFi.begin(ssid, password);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("OK\nIP: http://%s/\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("FAIL");
  }
  
  // OpenAI
  chat.setModel("gpt-4o-mini");
  chat.setSystem("Analyze images in 1-2 sentences.");
  chat.setMaxTokens(200);
  chat.setTemperature(0.2);
  
  // Web server
  setupWeb();
  server.begin();
  
  Serial.println("\n=== READY ===");
  Serial.printf("Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("Press BOOT to record\n");
}

void loop() {
  if (!isProcessing && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
  
  if (audioReady && !isRecording) {
    audioPlayer.loop();
  }
  
  handleButton();
  
  if (isRecording) {
    handleRecording();
  }
  
  yield();
}

void handleButton() {
  static int lastState = HIGH;
  int state = digitalRead(BUTTON_PIN);
  
  if (state != lastState) {
    if (state == LOW && !isRecording && !isProcessing) {
      if (millis() - lastBtn > 500) {
        lastBtn = millis();
        streamEnabled = false;
        startRec();
      }
    } else if (isRecording) {
      stopRec();
    }
    lastState = state;
  }
}

void handleRecording() {
  size_t avail = I2S_MIC.available();
  if (avail > 0) {
    size_t chunk = min(avail, (size_t)4096);
    uint8_t *newBuf = (uint8_t*)realloc(wavBuf, wavSize + chunk);
    
    if (!newBuf) {
      Serial.printf("OOM! Size: %d\n", wavSize);
      stopRec();
      return;
    }
    
    wavBuf = newBuf;
    wavSize += I2S_MIC.readBytes((char*)(wavBuf + wavSize), chunk);
    
    if (wavSize > MAX_REC_SIZE) {
      Serial.println("Max length reached");
      stopRec();
    }
  }
}

void setupWeb() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
      "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width'>"
      "<style>body{font-family:Arial;text-align:center;background:#000;color:#fff;margin:0}"
      "img{max-width:100%;border:2px solid #4CAF50;margin:10px}"
      ".st{background:#222;padding:15px;margin:10px;border-radius:5px;font-size:18px}"
      ".rec{background:#f00;animation:b 1s infinite}@keyframes b{50%{opacity:.5}}"
      ".proc{background:#f80}.btn{background:#4CAF50;color:#fff;padding:12px 24px;"
      "border:none;border-radius:5px;margin:5px;font-size:14px;cursor:pointer}"
      ".btn:disabled{background:#444}</style>"
      "<script>let p=0,f=0;function u(){fetch('/status').then(r=>r.json()).then(d=>{"
      "p=d.processing;f=0;document.getElementById('s').className='st'+(d.recording?' rec':"
      "d.processing?' proc':'');document.getElementById('s').innerHTML=d.status;"
      "document.getElementById('r').disabled=!d.hasAudio;if(!p&&!document.getElementById('c')"
      ".src.includes('stream'))document.getElementById('c').src='/stream?'+Date.now();})"
      ".catch(()=>{f++;if(p||f<3)document.getElementById('s').innerHTML='⚙️ Processing...';})}"
      "function play(){document.getElementById('r').disabled=true;fetch('/replay')"
      ".then(()=>setTimeout(()=>{document.getElementById('r').disabled=false;u()},2000))}"
      "setInterval(u,2000);u();</script></head><body><h1 style='color:#4CAF50'>"
      "🎥 DFRobot AI Camera</h1><div id='s' class='st'>Loading...</div>"
      "<button id='r' class='btn' onclick='play()' disabled>🔊 Replay</button>"
      "<img id='c' alt='Camera'/><p style='color:#888'>Press BOOT to record</p></body></html>"
    );
  });
  
  server.on("/status", HTTP_GET, []() {
    String st = streamEnabled ? "Ready" : "";
    if (isRecording) st = "🔴 REC";
    else if (isProcessing) st = "⚙️ Processing";
    
    server.send(200, "application/json",
      "{\"recording\":" + String(isRecording) + 
      ",\"processing\":" + String(isProcessing) +
      ",\"hasAudio\":" + String(lastAudio.length() > 0) +
      ",\"status\":\"" + st + "\"}"
    );
  });
  
  server.on("/replay", HTTP_GET, []() {
    if (lastAudio.length() && audioReady) {
      playAudio(lastAudio);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(404, "text/plain", "No audio");
    }
  });
  
  server.on("/stream", HTTP_GET, []() {
    if (!streamEnabled) {
      server.send(503, "text/plain", "Paused");
      return;
    }
    
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=f\r\n");
    
    while (client.connected() && streamEnabled && !isRecording && !isProcessing) {
      if (millis() - lastFrame < 150) {
        delay(10);
        continue;
      }
      lastFrame = millis();
      
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) {
        delay(200);
        continue;
      }
      
      client.printf("--f\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
      client.write(fb->buf, fb->len);
      client.print("\r\n");
      esp_camera_fb_return(fb);
    }
  });
}

String escapeJSON(String s) {
  String r = "";
  for (int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"') r += "\\\"";
    else if (c == '\\') r += "\\\\";
    else if (c == '\n') r += "\\n";
    else if (c == '\r') r += "\\r";
    else if (c == '\t') r += "\\t";
    else r += c;
  }
  return r;
}

String STT() {
  Serial.print("STT... ");
  yield();
  String txt = audio.file(wavBuf, wavSize, (OpenAI_Audio_Input_Format)5);
  free(wavBuf);
  wavBuf = NULL;
  wavSize = 0;
  yield();
  Serial.printf("OK (%dB heap)\n", ESP.getFreeHeap());
  return txt;
}

String vision(String q) {
  Serial.print("Vision... ");
  yield();
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("FAIL");
    return "";
  }
  
  OpenAI_StringResponse res = chat.message(q, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  
  String ans = "";
  if (res.length() > 0) {
    ans = res.getAt(0);
    ans.trim();
  }
  
  yield();
  Serial.printf("OK (%dB heap)\n", ESP.getFreeHeap());
  return ans;
}

bool downloadMP3(String txt, String file) {
  Serial.print("TTS... ");
  
  if (txt.length() > 2500) {
    txt = txt.substring(0, 2500);
  }
  
  HTTPClient http;
  http.begin("https://api.openai.com/v1/audio/speech");
  http.addHeader("Authorization", "Bearer " + String(api_key));
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);
  
  String payload = "{\"model\":\"tts-1\",\"input\":\"" + escapeJSON(txt) + "\",\"voice\":\"alloy\"}";
  int code = http.POST(payload);
  
  if (code != 200) {
    Serial.printf("HTTP %d\n", code);
    http.end();
    return false;
  }
  
  if (SD.exists(file.c_str())) SD.remove(file.c_str());
  File f = SD.open(file.c_str(), FILE_WRITE);
  if (!f) {
    Serial.println("File FAIL");
    http.end();
    return false;
  }
  
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[512];
  size_t total = 0;
  unsigned long start = millis();
  
  while (http.connected() && millis() - start < 30000) {
    size_t avail = stream->available();
    if (avail > 0) {
      int c = stream->readBytes(buf, min(avail, sizeof(buf)));
      f.write(buf, c);
      total += c;
    } else if (!http.connected()) {
      break;
    } else {
      delay(1);
      yield();
    }
  }
  
  f.close();
  http.end();
  
  Serial.printf("OK (%dKB)\n", total / 1024);
  return total > 0;
}

bool playAudio(String file) {
  if (!audioReady || !SD.exists(file.c_str())) return false;
  
  Serial.print("Play... ");
  digitalWrite(MAX_MODE, HIGH);
  audioPlayer.connecttoFS(SD, file.c_str());
  
  unsigned long start = millis();
  while (audioPlayer.isRunning() && millis() - start < 60000) {
    audioPlayer.loop();
    delay(1);
    yield();
  }
  
  Serial.println("OK");
  return true;
}

int TTS(String txt) {
  if (!sdReady || !audioReady) {
    Serial.println("TTS unavailable");
    return -1;
  }
  
  String file = "/audio/r" + String(millis()) + ".mp3";
  
  if (!downloadMP3(txt, file)) return -1;
  if (!playAudio(file)) return -1;
  
  lastAudio = file;
  return 0;
}

void startRec() {
  Serial.println("\n=== REC START ===");
  Serial.printf("Heap: %d\n", ESP.getFreeHeap());
  
  wavSize = 0;
  wavBuf = (uint8_t*)malloc(PCM_WAV_HEADER_SIZE);
  if (!wavBuf) {
    Serial.println("Malloc FAIL");
    return;
  }
  
  const pcm_wav_header_t hdr = PCM_WAV_HEADER_DEFAULT(0, 16, SAMPLE_RATE, 1);
  memcpy(wavBuf, &hdr, PCM_WAV_HEADER_SIZE);
  wavSize = PCM_WAV_HEADER_SIZE;
  
  Serial.println("🎤 SPEAK NOW\n");
  digitalWrite(LED_PIN, LOW);
  isRecording = true;
}

void stopRec() {
  if (!isRecording) return;
  
  isRecording = false;
  isProcessing = true;
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("=== REC STOP ===");
  float dur = (float)(wavSize - PCM_WAV_HEADER_SIZE) / (SAMPLE_RATE * 2);
  Serial.printf("Duration: %.1fs\n", dur);
  Serial.printf("Heap: %d\n", ESP.getFreeHeap());
  
  if (dur < 0.5) {
    Serial.println("Too short\n");
    free(wavBuf);
    wavBuf = NULL;
    wavSize = 0;
    isProcessing = false;
    streamEnabled = true;
    return;
  }
  
  // Update WAV header
  pcm_wav_header_t *hdr = (pcm_wav_header_t*)wavBuf;
  hdr->descriptor_chunk.chunk_size = wavSize + sizeof(pcm_wav_header_t) - 8;
  hdr->data_chunk.subchunk_size = wavSize - PCM_WAV_HEADER_SIZE;
  
  Serial.println("\n=== AI PIPELINE ===\n");
  
  // Speech to text
  String txt = STT();
  if (txt.length() > 0) {
    Serial.printf("You: \"%s\"\n\n", txt.c_str());
    
    // Vision analysis
    String ans = vision(txt);
    if (ans.length() > 0) {
      Serial.printf("AI: \"%s\"\n\n", ans.c_str());
      
      // Text to speech
      if (TTS(ans) == 0) {
        Serial.println("Audio cached\n");
      }
    }
  }
  
  Serial.println("=== COMPLETE ===");
  Serial.printf("Heap: %d\n\n", ESP.getFreeHeap());
  
  isProcessing = false;
  streamEnabled = true;
}

void audio_info(const char *info) {}
void audio_eof_mp3(const char *info) {}
