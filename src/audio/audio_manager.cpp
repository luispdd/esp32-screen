#include "audio_manager.h"
#include "../config.h"
#include <Wire.h>
#include <driver/i2s_std.h>
#include <esp_log.h>
#include <math.h>

static const char *TAG = "AUDIO_MGR";

static i2s_chan_handle_t s_tx_chan = nullptr;
static SemaphoreHandle_t s_playSemaphore = nullptr;

struct Note {
  float freq;
  uint16_t durMs;
};

// Super Mario Bros Coin Chime
static const Note s_coinNotes[] = {
  { 987.77f, 75 },   // B5
  { 1318.51f, 350 }, // E6
};

static const size_t s_coinNoteCount = sizeof(s_coinNotes) / sizeof(s_coinNotes[0]);

AudioManager &AudioManager::instance() {
  static AudioManager s_instance;
  return s_instance;
}

AudioManager::AudioManager()
  : _initialized(false),
    _isPlaying(false),
    _volume(CELEBRATION_VOLUME) {
}

static bool es8311_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  uint8_t err = Wire.endTransmission();
  return (err == 0);
}

bool AudioManager::initES8311() {
  bool ok = true;

  // Enhance I2C noise immunity
  ok &= es8311_write(0x44, 0x08);
  ok &= es8311_write(0x44, 0x08);

  // Clock manager: configure for 16kHz with 256*fs external MCLK
  ok &= es8311_write(0x01, 0x30);
  ok &= es8311_write(0x02, 0x00);
  ok &= es8311_write(0x03, 0x10);
  ok &= es8311_write(0x04, 0x10);
  ok &= es8311_write(0x05, 0x00);
  ok &= es8311_write(0x06, 0x03); // BCLK divider = 4
  ok &= es8311_write(0x07, 0x00);
  ok &= es8311_write(0x08, 0xFF);

  // System power
  ok &= es8311_write(0x0B, 0x00);
  ok &= es8311_write(0x0C, 0x00);
  ok &= es8311_write(0x10, 0x1F);
  ok &= es8311_write(0x11, 0x7F);

  // Power on Clock State Machine (CSM on: bit 7 = 1; slave mode: bit 6 = 0)
  ok &= es8311_write(0x00, 0x80);
  ok &= es8311_write(0x01, 0x3F & ~0x80); // bit 7 = 0 (use external MCLK from GPIO 16)

  // Serial digital port format: standard I2S 16-bit
  ok &= es8311_write(0x09, 0x00);

  // Power on analog circuitry and DAC
  ok &= es8311_write(0x0D, 0x01);
  ok &= es8311_write(0x0E, 0x02);
  ok &= es8311_write(0x12, 0x00);
  ok &= es8311_write(0x13, 0x10);
  ok &= es8311_write(0x14, 0x1A);
  ok &= es8311_write(0x37, 0x08);
  ok &= es8311_write(0x45, 0x00);

  // Internal reference signal (ADCL + DACR)
  ok &= es8311_write(0x44, 0x58);

  // Apply default volume
  setVolume(_volume);

  // Unmute DAC
  ok &= es8311_write(0x31, 0x00);

  Serial.printf("[AUDIO] ES8311 codec init at 0x%02X: %s\n", ES8311_I2C_ADDR, ok ? "SUCCESS" : "FAILED (No ACK)");
  return ok;
}

bool AudioManager::initI2S() {
  if (s_tx_chan != nullptr) {
    return true;
  }

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;

  esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx_chan, nullptr);
  if (err != ESP_OK) {
    Serial.printf("[AUDIO] i2s_new_channel failed: %d\n", err);
    return false;
  }

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = (gpio_num_t)PIN_I2S_MCLK,
      .bclk = (gpio_num_t)PIN_I2S_BCLK,
      .ws = (gpio_num_t)PIN_I2S_WS,
      .dout = (gpio_num_t)PIN_I2S_DOUT,
      .din = GPIO_NUM_NC,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };
  std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

  err = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
  if (err != ESP_OK) {
    Serial.printf("[AUDIO] i2s_channel_init_std_mode failed: %d\n", err);
    return false;
  }

  Serial.printf("[AUDIO] I2S initialized (MCLK=%d, BCLK=%d, WS=%d, DOUT=%d)\n", 
                PIN_I2S_MCLK, PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT);
  return true;
}

bool AudioManager::init() {
  if (_initialized) return true;

  // 1. Configure Power Amplifier enable pin (Active HIGH)
  pinMode(PIN_AUDIO_PA, OUTPUT);
  digitalWrite(PIN_AUDIO_PA, LOW); // Keep PA disabled when idle to eliminate hiss

  // 2. Initialize I2S peripheral
  if (!initI2S()) {
    return false;
  }

  // 3. Enable I2S channel so MCLK starts clocking ES8311 pin 16
  esp_err_t err = i2s_channel_enable(s_tx_chan);
  if (err != ESP_OK) {
    Serial.printf("[AUDIO] i2s_channel_enable error: %d\n", err);
  }

  // 4. Initialize ES8311 over shared Wire I2C bus (MCLK must be running!)
  if (!initES8311()) {
    Serial.println("[AUDIO] Warning: ES8311 initialization check failed");
  }

  // 5. Create playback task synchronization semaphore
  if (s_playSemaphore == nullptr) {
    s_playSemaphore = xSemaphoreCreateBinary();
  }

  // 6. Create dedicated playback FreeRTOS task
  xTaskCreatePinnedToCore(
    audioTask,
    "AudioTask",
    4096,
    this,
    2,
    nullptr,
    1
  );

  _initialized = true;
  Serial.println("[AUDIO] AudioManager initialized successfully");
  return true;
}

void AudioManager::setVolume(uint8_t volume) {
  if (volume > 100) volume = 100;
  _volume = volume;
  // In ES8311: 0x32 is DAC volume register.
  // 0x00 is mute, 0xBF is 0dB unity gain, 0xFF is +32dB.
  // Map [0..100] to [0..0xBF] so 100% volume is 0dB (safe, undistorted maximum).
  // At volume = 50, regVal = 95 (~ -16dB), comfortable for onboard speaker.
  uint8_t regVal = (uint8_t)(((uint32_t)volume * 0xBF) / 100);
  es8311_write(0x32, regVal);
}

bool AudioManager::isPlaying() const {
  return _isPlaying;
}

void AudioManager::playCelebration() {
  if (!_initialized) {
    init();
  }
  if (_isPlaying) {
    return; // Already playing
  }

  _isPlaying = true;
  if (s_playSemaphore != nullptr) {
    xSemaphoreGive(s_playSemaphore);
  }
}

void AudioManager::audioTask(void *param) {
  AudioManager *self = static_cast<AudioManager *>(param);

  for (;;) {
    if (xSemaphoreTake(s_playSemaphore, portMAX_DELAY) == pdTRUE) {
      Serial.println("[AUDIO] Starting celebration coin chime playback");
      self->generateAndPlayCoinSound();
      self->_isPlaying = false;
      Serial.println("[AUDIO] Celebration coin chime playback completed");
    }
  }
}

void AudioManager::generateAndPlayCoinSound() {
  if (s_tx_chan == nullptr) return;

  // 1. Assert Power Amplifier (GPIO 46 HIGH)
  digitalWrite(PIN_AUDIO_PA, HIGH);
  vTaskDelay(pdMS_TO_TICKS(15)); // Stabilization time

  const uint32_t sampleRate = 16000;
  const int16_t maxAmp = 8000; // Controlled amplitude for pleasant volume

  int16_t chunkBuffer[256 * 2]; // 256 stereo frames

  for (size_t n = 0; n < s_coinNoteCount; n++) {
    const Note &note = s_coinNotes[n];
    size_t totalSamples = (note.durMs * sampleRate) / 1000;

    if (note.freq <= 10.0f) {
      // Rest / silence
      memset(chunkBuffer, 0, sizeof(chunkBuffer));
      size_t samplesRemaining = totalSamples;
      while (samplesRemaining > 0) {
        size_t count = (samplesRemaining > 256) ? 256 : samplesRemaining;
        size_t bytesWritten = 0;
        i2s_channel_write(s_tx_chan, chunkBuffer, count * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        samplesRemaining -= count;
      }
    } else {
      // Authentic NES 25% pulse wave tone generation
      float period = (float)sampleRate / note.freq;
      size_t sampleIdx = 0;

      while (sampleIdx < totalSamples) {
        size_t count = (totalSamples - sampleIdx > 256) ? 256 : (totalSamples - sampleIdx);

        for (size_t i = 0; i < count; i++) {
          size_t s = sampleIdx + i;
          float progress = (float)s / (float)totalSamples;
          // Apply gentle decay on the longer note (E6)
          float decay = (n == 0) ? 1.0f : (1.0f - (0.65f * progress));
          int16_t amp = (int16_t)(maxAmp * decay);

          float phase = fmodf((float)s, period) / period;
          int16_t val = (phase < 0.25f) ? amp : -amp;

          // Stereo left/right
          chunkBuffer[2 * i] = val;
          chunkBuffer[2 * i + 1] = val;
        }

        size_t bytesWritten = 0;
        i2s_channel_write(s_tx_chan, chunkBuffer, count * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        sampleIdx += count;
      }
    }
  }

  // 2. Graceful tail delay to finish the note
  vTaskDelay(pdMS_TO_TICKS(30));

  // 3. Deassert Power Amplifier (GPIO 46 LOW) to silence idle hiss
  digitalWrite(PIN_AUDIO_PA, LOW);
}
