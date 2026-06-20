#include "engine/Audio.h"

#include "utils/FileUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <dsound.h>
#endif

namespace {
#ifdef _WIN32
IDirectSound8 *g_direct_sound = NULL;

LONG DirectSoundVolume(float volume) {
  if (volume <= 0.00001f)
    return DSBVOLUME_MIN;
  const float hundredths_of_db = 2000.0f * std::log10(volume);
  return static_cast<LONG>(std::max(
      static_cast<float>(DSBVOLUME_MIN),
      std::min(static_cast<float>(DSBVOLUME_MAX), hundredths_of_db)));
}

DWORD DirectSoundFrequency(uint32_t sample_rate, float playback_rate) {
  const double requested =
      static_cast<double>(sample_rate) * static_cast<double>(playback_rate);
  return static_cast<DWORD>(std::max(
      static_cast<double>(DSBFREQUENCY_MIN),
      std::min(static_cast<double>(DSBFREQUENCY_MAX), requested)));
}
#endif

uint32_t ReadUint32LE(const unsigned char *bytes) {
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

uint16_t ReadUint16LE(const unsigned char *bytes) {
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8);
}

bool ParseWav(const std::vector<unsigned char> &wav_data,
              size_t &sample_data_offset, uint32_t &sample_data_size,
              uint16_t &format_tag, uint16_t &channels,
              uint32_t &sample_rate, uint32_t &average_bytes_per_second,
              uint16_t &block_align, uint16_t &bits_per_sample) {
  if (wav_data.size() < 12 ||
      std::memcmp(wav_data.data(), "RIFF", 4) != 0 ||
      std::memcmp(wav_data.data() + 8, "WAVE", 4) != 0)
    return false;

  bool found_format = false;
  bool found_samples = false;
  size_t chunk_offset = 12;
  while (chunk_offset + 8 <= wav_data.size()) {
    const uint32_t declared_size =
        ReadUint32LE(wav_data.data() + chunk_offset + 4);
    const size_t physical_size = wav_data.size() - chunk_offset - 8;

    if (std::memcmp(wav_data.data() + chunk_offset, "fmt ", 4) == 0) {
      if (declared_size < 16 || physical_size < 16)
        return false;
      const unsigned char *format = wav_data.data() + chunk_offset + 8;
      format_tag = ReadUint16LE(format);
      channels = ReadUint16LE(format + 2);
      sample_rate = ReadUint32LE(format + 4);
      average_bytes_per_second = ReadUint32LE(format + 8);
      block_align = ReadUint16LE(format + 12);
      bits_per_sample = ReadUint16LE(format + 14);
      found_format = true;
    } else if (std::memcmp(wav_data.data() + chunk_offset, "data", 4) ==
               0) {
      const size_t available_size =
          std::min(static_cast<size_t>(declared_size), physical_size);
      if (available_size > UINT32_MAX)
        return false;
      sample_data_offset = chunk_offset + 8;
      sample_data_size = static_cast<uint32_t>(available_size);
      found_samples = true;
      break;
    }

    const size_t padded_size =
        static_cast<size_t>(declared_size) + (declared_size & 1u);
    if (padded_size > physical_size)
      return false;
    chunk_offset += 8 + padded_size;
  }

  if (!found_format || !found_samples || format_tag != 1 || channels == 0 ||
      sample_rate == 0 || block_align == 0 || bits_per_sample == 0)
    return false;

  sample_data_size -= sample_data_size % block_align;
  return sample_data_size > 0;
}
} // namespace

bool InitializeAudio(void *native_window_handle) {
#ifdef _WIN32
  if (g_direct_sound != NULL)
    return true;

  HRESULT result = DirectSoundCreate8(NULL, &g_direct_sound, NULL);
  if (FAILED(result)) {
    fprintf(stderr, "ERROR: DirectSound initialization failed (0x%08lx).\n",
            static_cast<unsigned long>(result));
    g_direct_sound = NULL;
    return false;
  }

  HWND window = static_cast<HWND>(native_window_handle);
  if (window == NULL)
    window = GetDesktopWindow();
  result = g_direct_sound->SetCooperativeLevel(window, DSSCL_NORMAL);
  if (FAILED(result)) {
    fprintf(stderr,
            "ERROR: DirectSound cooperative level failed (0x%08lx).\n",
            static_cast<unsigned long>(result));
    g_direct_sound->Release();
    g_direct_sound = NULL;
    return false;
  }
  return true;
#else
  (void)native_window_handle;
  return false;
#endif
}

void ShutdownAudio() {
#ifdef _WIN32
  if (g_direct_sound != NULL) {
    g_direct_sound->Release();
    g_direct_sound = NULL;
  }
#endif
}

LoopingWavSound::LoopingWavSound(const char *filename)
    : path_(ResolveExistingPath(filename)), sample_data_offset_(0),
      sample_data_size_(0), format_tag_(0), channels_(0), sample_rate_(0),
      average_bytes_per_second_(0), block_align_(0), bits_per_sample_(0),
      sound_buffer_(NULL), gain_(1.0f), playback_rate_(1.0f), volume_(1.0f),
      playing_(false),
      error_reported_(false) {
  if (!ReadWholeFile(path_, wav_data_) ||
      !ParseWav(wav_data_, sample_data_offset_, sample_data_size_, format_tag_,
                channels_, sample_rate_, average_bytes_per_second_,
                block_align_, bits_per_sample_)) {
    fprintf(stderr, "ERROR: Cannot load WAV file \"%s\".\n", path_.c_str());
    error_reported_ = true;
  }
}

LoopingWavSound::~LoopingWavSound() { Stop(); }

void LoopingWavSound::SetPlaying(bool should_play) {
  if (!should_play) {
    Stop();
    return;
  }

  if (playing_)
    return;
  if (error_reported_)
    return;

#ifdef _WIN32
  if (g_direct_sound == NULL) {
    if (!error_reported_) {
      fprintf(stderr, "ERROR: Audio system is not initialized for \"%s\".\n",
              path_.c_str());
      error_reported_ = true;
    }
    return;
  }

  WAVEFORMATEX format = {};
  format.wFormatTag = format_tag_;
  format.nChannels = channels_;
  format.nSamplesPerSec = sample_rate_;
  format.nAvgBytesPerSec = average_bytes_per_second_;
  format.nBlockAlign = block_align_;
  format.wBitsPerSample = bits_per_sample_;

  DSBUFFERDESC description = {};
  description.dwSize = sizeof(DSBUFFERDESC);
  description.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY |
                        DSBCAPS_GETCURRENTPOSITION2;
  description.dwBufferBytes = sample_data_size_;
  description.lpwfxFormat = &format;

  IDirectSoundBuffer *buffer = NULL;
  HRESULT result =
      g_direct_sound->CreateSoundBuffer(&description, &buffer, NULL);
  if (SUCCEEDED(result)) {
    void *first_region = NULL;
    void *second_region = NULL;
    DWORD first_size = 0;
    DWORD second_size = 0;
    result = buffer->Lock(0, sample_data_size_, &first_region, &first_size,
                          &second_region, &second_size, 0);
    if (SUCCEEDED(result)) {
      const unsigned char *samples = wav_data_.data() + sample_data_offset_;
      std::vector<unsigned char> gained_samples;
      if (bits_per_sample_ == 16 && std::abs(gain_ - 1.0f) > 0.0001f) {
        gained_samples.resize(sample_data_size_);
        for (size_t offset = 0; offset + 1 < sample_data_size_; offset += 2) {
          const int16_t sample =
              static_cast<int16_t>(ReadUint16LE(samples + offset));
          const int scaled = static_cast<int>(std::lround(sample * gain_));
          const int16_t clipped = static_cast<int16_t>(
              std::max(-32768, std::min(32767, scaled)));
          const uint16_t encoded = static_cast<uint16_t>(clipped);
          gained_samples[offset] =
              static_cast<unsigned char>(encoded & 0xff);
          gained_samples[offset + 1] =
              static_cast<unsigned char>((encoded >> 8) & 0xff);
        }
        samples = gained_samples.data();
      }
      std::memcpy(first_region, samples, first_size);
      if (second_region != NULL && second_size > 0)
        std::memcpy(second_region, samples + first_size, second_size);
      buffer->Unlock(first_region, first_size, second_region, second_size);
      buffer->SetVolume(DirectSoundVolume(volume_));
      buffer->SetFrequency(
          DirectSoundFrequency(sample_rate_, playback_rate_));
      buffer->SetCurrentPosition(0);
      result = buffer->Play(0, 0, DSBPLAY_LOOPING);
      if (SUCCEEDED(result)) {
        sound_buffer_ = buffer;
        playing_ = true;
        return;
      }
    }
    buffer->Release();
  }

  if (!error_reported_) {
    fprintf(stderr, "ERROR: Cannot play WAV file \"%s\" (0x%08lx).\n",
            path_.c_str(), static_cast<unsigned long>(result));
    error_reported_ = true;
  }
#else
  if (!error_reported_) {
    fprintf(stderr,
            "WARNING: WAV playback is not available on this platform: \"%s\".\n",
            path_.c_str());
    error_reported_ = true;
  }
#endif
}

void LoopingWavSound::SetGain(float gain) {
  const float new_gain = std::max(0.0f, std::min(gain, 4.0f));
  if (std::abs(new_gain - gain_) <= 0.0001f)
    return;

  const bool restart = playing_;
  Stop();
  gain_ = new_gain;
  if (restart)
    SetPlaying(true);
}

void LoopingWavSound::SetPlaybackRate(float playback_rate) {
  playback_rate_ =
      std::max(0.25f, std::min(playback_rate, 4.0f));
#ifdef _WIN32
  if (sound_buffer_ != NULL) {
    IDirectSoundBuffer *buffer =
        static_cast<IDirectSoundBuffer *>(sound_buffer_);
    buffer->SetFrequency(
        DirectSoundFrequency(sample_rate_, playback_rate_));
  }
#endif
}

void LoopingWavSound::SetVolume(float volume) {
  volume_ = std::max(0.0f, std::min(volume, 1.0f));
#ifdef _WIN32
  if (sound_buffer_ != NULL) {
    IDirectSoundBuffer *buffer =
        static_cast<IDirectSoundBuffer *>(sound_buffer_);
    buffer->SetVolume(DirectSoundVolume(volume_));
  }
#endif
}

void LoopingWavSound::Stop() {
  if (!playing_)
    return;

#ifdef _WIN32
  IDirectSoundBuffer *buffer = static_cast<IDirectSoundBuffer *>(sound_buffer_);
  buffer->Stop();
  buffer->SetCurrentPosition(0);
  buffer->Release();
  sound_buffer_ = NULL;
#endif
  playing_ = false;
}
