#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

bool InitializeAudio(void *native_window_handle);
void ShutdownAudio();

class LoopingWavSound {
public:
  explicit LoopingWavSound(const char *filename);
  ~LoopingWavSound();

  void SetPlaying(bool should_play);
  void SetGain(float gain);
  void SetPlaybackRate(float playback_rate);
  void SetVolume(float volume);
  void Stop();

private:
  std::string path_;
  std::vector<unsigned char> wav_data_;
  size_t sample_data_offset_;
  uint32_t sample_data_size_;
  uint16_t format_tag_;
  uint16_t channels_;
  uint32_t sample_rate_;
  uint32_t average_bytes_per_second_;
  uint16_t block_align_;
  uint16_t bits_per_sample_;
  void *sound_buffer_;
  float gain_;
  float playback_rate_;
  float volume_;
  bool playing_;
  bool error_reported_;
};

#endif
