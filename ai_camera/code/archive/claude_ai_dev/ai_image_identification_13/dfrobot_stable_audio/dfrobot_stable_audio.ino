#include <WiFi.h>
#include <WebServer.h>
#include <OpenAI.h>
#include "camera.h"
#include "wav_header.h"
#include "ESP_I2S.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"


// ========== PINS ==========
#define BUTTON_PIN      0
#define LED_PIN         3
#define MIC_CLK         38
#define MIC_DATA        39
#define SAMPLE_RATE     16000
#define MAX_REC_SIZE    480000  // 15 seconds

// ========== CREDENTIALS ==========
const char* ssid = "OSxDesign_2.4GH";
const char* password = "ixnaywifi";
const char* api_key = "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";

// ========== OBJECTS ==========
OpenAI openai(api_key);
OpenAI_ChatCompletion chat(openai);
OpenAI_AudioTranscription audio(openai);
OpenAI_TTS tts(openai);  // Use built-in TTS - NO SCREECHING
I2SClass I2S;
WebServer server(80);

// ========== STATE ==========
bool isRecording = false;
bool isProcessing = false;
bool streamEnabled = true;
uint8_t *wavBuf = NULL;
size_t wavSize = 0;
unsigned long lastBtn = 0;
unsigned long lastFrame = 0;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brownout
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== DFRobot ESP32-S3 AI Camera ===");
  Serial.println("    Optimized - Stable Audio");
  Serial.printf("Heap: %d | PSRAM: %d\n\n", ESP.getFreeHeap(), ESP.getFreePsram());
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, HIGH);
  
  // Init camera
  Serial.print("Camera... ");
  initCamera();
  Serial.println("OK");
  
  // Init microphone
  Serial.print("Microphone... ");
  I2S.setPinsPdmRx(MIC_CLK, MIC_DATA);
  I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  
  // Test mic
  unsigned long start = millis();
  size_t total = 0;
  while (millis() - start < 1000) {
    size_t avail = I2S.available();
    if (avail > 0) {
      uint8_t buf[256];
      total += I2S.readBytes((char*)buf, min(avail, sizeof(buf)));
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
  size_t avail = I2S.available();
  if (avail > 0) {
    size_t chunk = min(avail, (size_t)4096);
    uint8_t *newBuf = (uint8_t*)realloc(wavBuf, wavSize + chunk);
    
    if (!newBuf || wavSize + chunk > MAX_REC_SIZE) {
      if (!newBuf) Serial.printf("OOM! Size: %d\n", wavSize);
      else Serial.println("Max length");
      stopRec();
      return;
    }
    
    wavBuf = newBuf;
    wavSize += I2S.readBytes((char*)(wavBuf + wavSize), chunk);
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
      ".proc{background:#f80}</style>"
      "<script>let p=0,f=0;function u(){fetch('/status').then(r=>r.json()).then(d=>{"
      "p=d.processing;f=0;document.getElementById('s').className='st'+(d.recording?' rec':"
      "d.processing?' proc':'');document.getElementById('s').innerHTML=d.status;"
      "if(!p&&!document.getElementById('c').src.includes('stream'))document.getElementById('c')"
      ".src='/stream?'+Date.now();}).catch(()=>{f++;if(p||f<3)document.getElementById('s')"
      ".innerHTML='⚙️ Processing...';});}setInterval(u,2000);u();</script></head><body>"
      "<h1 style='color:#4CAF50'>🎥 DFRobot AI Camera</h1>"
      "<div id='s' class='st'>Loading...</div>"
      "<img id='c' alt='Camera'/>"
      "<p style='color:#888'>Press BOOT to record<br>Stable audio (20-30s delay)</p>"
      "</body></html>"
    );
  });
  
  server.on("/status", HTTP_GET, []() {
    String st = streamEnabled ? "Ready" : "";
    if (isRecording) st = "🔴 REC";
    else if (isProcessing) st = "⚙️ Processing";
    
    server.send(200, "application/json",
      "{\"recording\":" + String(isRecording) + 
      ",\"processing\":" + String(isProcessing) +
      ",\"status\":\"" + st + "\"}"
    );
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

String STT() {
  Serial.print("STT... ");
  delay(500);  // Stable delay
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
  delay(100);  // Camera stabilization
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

int TTS(String txt) {
  Serial.print("TTS... ");
  delay(100);  // Prevent I2S conflicts
  
  // Use built-in OpenAI TTS - STABLE, NO SCREECHING
  int result = tts.message(txt);
  
  if (result == -1) {
    Serial.println("FAIL");
  } else {
    Serial.println("OK");
    // Built-in library handles playback
    delay(20000);  // Wait for audio completion
  }
  
  return result;
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
  
  size_t sr = I2S.rxSampleRate();
  uint16_t sw = (uint16_t)I2S.rxDataWidth();
  uint16_t ch = (uint16_t)I2S.rxSlotMode();
  
  const pcm_wav_header_t hdr = PCM_WAV_HEADER_DEFAULT(0, sw, sr, ch);
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
      
      // Text to speech (stable - no screeching!)
      if (TTS(ans) == 0) {
        Serial.println("Audio complete\n");
      }
    }
  }
  
  Serial.println("=== COMPLETE ===");
  Serial.printf("Heap: %d\n\n", ESP.getFreeHeap());
  
  isProcessing = false;
  streamEnabled = true;
}
