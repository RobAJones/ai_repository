/*
 * ESP32-S3 PDM Microphone Test
 * 
 * Simple test to verify if the microphone is working
 * This will record for 3 seconds and show if it's capturing real audio
 */

#include "ESP_I2S.h"

#define SAMPLE_RATE     16000
#define DATA_PIN        GPIO_NUM_39
#define CLOCK_PIN       GPIO_NUM_38

I2SClass i2s;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  PDM MICROPHONE DIAGNOSTIC TEST        ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  pinMode(3, OUTPUT);
  
  // Initialize PDM microphone
  Serial.println("→ Initializing PDM microphone...");
  i2s.setPinsPdmRx(CLOCK_PIN, DATA_PIN);
  
  if (!i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("✗ Failed to initialize PDM");
    while(1) delay(1000);
  }
  
  Serial.printf("✓ PDM initialized (CLK=%d, DATA=%d, %dHz)\n\n", CLOCK_PIN, DATA_PIN, SAMPLE_RATE);
  
  // Wait a moment for stabilization
  delay(500);
  
  Serial.println("══════════════════════════════════════════");
  Serial.println("MAKE SOME NOISE! Clap, talk, whistle...");
  Serial.println("Recording will start in 2 seconds");
  Serial.println("══════════════════════════════════════════\n");
  
  delay(2000);
  
  Serial.println("🔴 RECORDING NOW - MAKE LOUD NOISES!");
  digitalWrite(3, HIGH);
  
  // Record for 3 seconds
  size_t wav_size = 0;
  uint8_t *wav_buffer = i2s.recordWAV(3, &wav_size);
  
  digitalWrite(3, LOW);
  Serial.println("⏹ Recording stopped\n");
  
  if (wav_buffer == NULL || wav_size == 0) {
    Serial.println("✗ Recording failed - no data captured!");
    while(1) delay(1000);
  }
  
  Serial.printf("✓ Captured %d bytes\n\n", wav_size);
  
  // Analyze the audio data (skip WAV header - 44 bytes)
  if (wav_size > 44) {
    int16_t* samples = (int16_t*)(wav_buffer + 44);
    size_t numSamples = (wav_size - 44) / 2;
    
    Serial.println("═══════════════════════════════════════════");
    Serial.println("AUDIO ANALYSIS:");
    Serial.println("═══════════════════════════════════════════");
    
    // Find min, max, average
    int16_t minVal = 32767, maxVal = -32768;
    int64_t sum = 0;
    int zeroCount = 0;
    
    for (size_t i = 0; i < numSamples; i++) {
      int16_t sample = samples[i];
      if (sample < minVal) minVal = sample;
      if (sample > maxVal) maxVal = sample;
      sum += abs(sample);
      if (sample == 0) zeroCount++;
    }
    
    int16_t avgLevel = sum / numSamples;
    int16_t peak = max(abs(minVal), abs(maxVal));
    
    Serial.printf("Total samples: %d\n", numSamples);
    Serial.printf("Zero samples: %d (%.1f%%)\n", zeroCount, (zeroCount * 100.0) / numSamples);
    Serial.printf("Range: %d to %d\n", minVal, maxVal);
    Serial.printf("Peak level: %d (%.1f%% of max 32767)\n", peak, (peak * 100.0) / 32767);
    Serial.printf("Average level: %d\n\n", avgLevel);
    
    // Show first 20 samples
    Serial.println("First 20 samples:");
    for (int i = 0; i < 20 && i < numSamples; i++) {
      Serial.printf("%d ", samples[i]);
    }
    Serial.println("\n");
    
    // Diagnosis
    Serial.println("═══════════════════════════════════════════");
    Serial.println("DIAGNOSIS:");
    Serial.println("═══════════════════════════════════════════");
    
    if (zeroCount > numSamples * 0.9) {
      Serial.println("✗ PROBLEM: >90% samples are zero");
      Serial.println("  → Microphone is NOT connected or NOT working");
      Serial.println("  → Check wiring on GPIO 38/39");
    }
    else if (peak < 100) {
      Serial.println("✗ PROBLEM: Peak is extremely low (<100)");
      Serial.println("  → Microphone might be connected but not capturing sound");
      Serial.println("  → Try speaking VERY LOUDLY directly into mic");
      Serial.println("  → Check if microphone needs power enable pin");
    }
    else if (peak < 2000) {
      Serial.println("⚠ WARNING: Peak is very low (<2000)");
      Serial.println("  → Microphone is working but signal is weak");
      Serial.println("  → This is what you're experiencing!");
      Serial.println("  → Possible causes:");
      Serial.println("     1. Microphone gain is too low (hardware issue)");
      Serial.println("     2. Wrong channel (LEFT vs RIGHT)");
      Serial.println("     3. Need to enable microphone power");
    }
    else if (peak < 10000) {
      Serial.println("⚠ NOTICE: Peak is moderate (2000-10000)");
      Serial.println("  → Microphone is working but could be louder");
      Serial.println("  → May need software amplification");
    }
    else {
      Serial.println("✓ GOOD: Peak level is healthy (>10000)");
      Serial.println("  → Microphone is capturing sound properly!");
    }
    
    Serial.println("\n═══════════════════════════════════════════");
    
    // Check for variation (real audio has variation)
    int16_t range = maxVal - minVal;
    if (range < 50) {
      Serial.println("\n⚠ ADDITIONAL ISSUE: Very little variation in signal");
      Serial.println("  → This suggests NO real audio is being captured");
      Serial.println("  → You're only recording background noise/hiss");
      Serial.println("\nPOSSIBLE FIXES:");
      Serial.println("  1. Check if mic needs VDD/ENABLE pin pulled high");
      Serial.println("  2. Try swapping CLOCK and DATA pins");
      Serial.println("  3. Verify this board actually has a PDM microphone");
      Serial.println("  4. Check if mic is I2S not PDM (needs different init)");
    }
  }
  
  free(wav_buffer);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  TEST COMPLETE                         ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.println("To run test again, press RESET button\n");
}

void loop() {
  // Nothing - test runs once in setup()
}
