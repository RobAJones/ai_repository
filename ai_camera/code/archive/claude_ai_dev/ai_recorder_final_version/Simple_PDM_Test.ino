/*
 * ESP32-S3 PDM Microphone Simple Test
 * 
 * Minimal test to check microphone output
 */

#include "ESP_I2S.h"

#define SAMPLE_RATE     16000
#define DATA_PIN        GPIO_NUM_39
#define CLOCK_PIN       GPIO_NUM_38

I2SClass i2s;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== PDM MIC TEST ===\n");
  
  pinMode(3, OUTPUT);
  
  i2s.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("FAILED to init PDM");
    while(1) delay(1000);
  }
  
  Serial.println("PDM OK - GPIO 38/39\n");
  delay(500);
  
  Serial.println("GET READY TO MAKE NOISE!");
  Serial.println("Starting in 2 seconds...\n");
  delay(2000);
  
  Serial.println("RECORDING - CLAP OR TALK LOUDLY!");
  digitalWrite(3, HIGH);
  
  // Record 3 seconds
  size_t wav_size = 0;
  uint8_t *wav_buffer = i2s.recordWAV(3, &wav_size);
  
  digitalWrite(3, LOW);
  Serial.println("STOPPED\n");
  
  if (!wav_buffer || wav_size < 100) {
    Serial.println("ERROR: No data!");
    while(1) delay(1000);
  }
  
  Serial.printf("Got %d bytes\n\n", wav_size);
  
  // Simple analysis - just check first 1000 samples after WAV header
  if (wav_size > 2044) {  // 44 byte header + 2000 bytes of audio
    int16_t* samples = (int16_t*)(wav_buffer + 44);
    
    Serial.println("First 50 samples:");
    for (int i = 0; i < 50; i++) {
      Serial.printf("%d ", samples[i]);
      if ((i + 1) % 10 == 0) Serial.println();
    }
    Serial.println("\n");
    
    // Find min and max in first 1000 samples
    int16_t minVal = 32767, maxVal = -32768;
    int zeroCount = 0;
    
    for (int i = 0; i < 1000; i++) {
      int16_t val = samples[i];
      if (val < minVal) minVal = val;
      if (val > maxVal) maxVal = val;
      if (val == 0) zeroCount++;
    }
    
    Serial.printf("Analysis of first 1000 samples:\n");
    Serial.printf("  Min: %d\n", minVal);
    Serial.printf("  Max: %d\n", maxVal);
    Serial.printf("  Range: %d\n", maxVal - minVal);
    Serial.printf("  Zeros: %d (%.1f%%)\n\n", zeroCount, (zeroCount * 100.0) / 1000);
    
    int16_t peak = max(abs(minVal), abs(maxVal));
    Serial.printf("Peak level: %d / 32767 (%.1f%%)\n\n", peak, (peak * 100.0) / 32767);
    
    // Diagnosis
    Serial.println("DIAGNOSIS:");
    if (zeroCount > 900) {
      Serial.println("  ✗ >90% zeros - Mic NOT working");
      Serial.println("    Check connections!");
    }
    else if (peak < 500) {
      Serial.println("  ✗ Peak < 500 - Almost no signal");
      Serial.println("    Mic may need ENABLE pin");
      Serial.println("    Or wrong init mode");
    }
    else if (peak < 2000) {
      Serial.println("  ⚠ Peak < 2000 - Very weak signal");
      Serial.println("    Mic works but gain is too low");
      Serial.println("    This is your current problem!");
      Serial.println("\n  POSSIBLE FIXES:");
      Serial.println("    1. Check if mic has VDD_EN pin (needs HIGH)");
      Serial.println("    2. Try I2S mode instead of PDM");
      Serial.println("    3. Swap CLK/DATA pins");
    }
    else if (peak < 10000) {
      Serial.println("  ⚠ Peak 2000-10000 - Moderate signal");
      Serial.println("    Mic works, may need amplification");
    }
    else {
      Serial.println("  ✓ Peak > 10000 - Good signal!");
      Serial.println("    Mic is working properly");
    }
    
    Serial.println("\n===================");
    
    // Additional check - look at variation
    if (maxVal - minVal < 100) {
      Serial.println("\n⚠ CRITICAL: Almost no variation!");
      Serial.println("  This is just NOISE, not real audio");
      Serial.println("\n  LIKELY CAUSES:");
      Serial.println("  → Microphone needs power/enable");
      Serial.println("  → Wrong I2S configuration");
      Serial.println("  → Mic is damaged/not present");
    }
  }
  
  free(wav_buffer);
  Serial.println("\n=== TEST DONE ===");
  Serial.println("Press RESET to run again\n");
}

void loop() {
  delay(1000);
}
