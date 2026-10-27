#pragma once

#include <Arduino.h>

class AudioManager {
public:
  static AudioManager &instance();

  bool init();
  void playCelebration();
  bool isPlaying() const;
  void setVolume(uint8_t volume); // 0 - 100

private:
  AudioManager();
  ~AudioManager() = default;
  AudioManager(const AudioManager &) = delete;
  AudioManager &operator=(const AudioManager &) = delete;

  bool _initialized;
  volatile bool _isPlaying;
  uint8_t _volume;

  bool initES8311();
  bool initI2S();
  static void audioTask(void *param);
  void generateAndPlayCoinSound();
};
