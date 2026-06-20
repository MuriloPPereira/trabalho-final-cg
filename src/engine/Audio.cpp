#include "engine/Audio.h"

#include "utils/FileUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace {
ma_engine g_audio_engine;
bool g_audio_engine_initialized = false;

struct MiniaudioSoundBackend {
  ma_audio_buffer audio_buffer;
  ma_sound sound;
  bool audio_buffer_initialized;
  bool sound_initialized;

  MiniaudioSoundBackend()
      : audio_buffer_initialized(false), sound_initialized(false) {}
};

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

ma_format MiniaudioFormat(uint16_t bits_per_sample) {
  switch (bits_per_sample) {
  case 8:
    return ma_format_u8;
  case 16:
    return ma_format_s16;
  case 24:
    return ma_format_s24;
  case 32:
    return ma_format_s32;
  default:
    return ma_format_unknown;
  }
}

void ApplySoundControls(MiniaudioSoundBackend *backend, float gain,
                        float volume, float playback_rate) {
  if (backend == NULL || !backend->sound_initialized)
    return;
  ma_sound_set_volume(&backend->sound, gain * volume);
  ma_sound_set_pitch(&backend->sound, playback_rate);
}
} // namespace

bool InitializeAudio(void *native_window_handle) {
  (void)native_window_handle;
  if (g_audio_engine_initialized)
    return true;

  const ma_engine_config config = ma_engine_config_init();
  const ma_result result = ma_engine_init(&config, &g_audio_engine);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "ERROR: miniaudio initialization failed (%d).\n",
            static_cast<int>(result));
    return false;
  }

  g_audio_engine_initialized = true;
  return true;
}

void ShutdownAudio() {
  if (!g_audio_engine_initialized)
    return;
  ma_engine_uninit(&g_audio_engine);
  g_audio_engine_initialized = false;
}

LoopingWavSound::LoopingWavSound(const char *filename)
    : path_(ResolveExistingPath(filename)), sample_data_offset_(0),
      sample_data_size_(0), format_tag_(0), channels_(0), sample_rate_(0),
      average_bytes_per_second_(0), block_align_(0), bits_per_sample_(0),
      backend_(NULL), gain_(1.0f), playback_rate_(1.0f), volume_(1.0f),
      playing_(false), error_reported_(false) {
  if (!ReadWholeFile(path_, wav_data_) ||
      !ParseWav(wav_data_, sample_data_offset_, sample_data_size_, format_tag_,
                channels_, sample_rate_, average_bytes_per_second_,
                block_align_, bits_per_sample_)) {
    fprintf(stderr, "ERROR: Cannot load WAV file \"%s\".\n", path_.c_str());
    error_reported_ = true;
    return;
  }

  const ma_format format = MiniaudioFormat(bits_per_sample_);
  if (format == ma_format_unknown) {
    fprintf(stderr, "ERROR: Unsupported WAV bit depth in \"%s\".\n",
            path_.c_str());
    error_reported_ = true;
    return;
  }

  MiniaudioSoundBackend *backend = new MiniaudioSoundBackend();
  const ma_uint64 frame_count = sample_data_size_ / block_align_;
  const ma_audio_buffer_config config = ma_audio_buffer_config_init(
      format, channels_, frame_count, wav_data_.data() + sample_data_offset_,
      NULL);
  const ma_result result =
      ma_audio_buffer_init(&config, &backend->audio_buffer);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "ERROR: Cannot create audio buffer for \"%s\" (%d).\n",
            path_.c_str(), static_cast<int>(result));
    delete backend;
    error_reported_ = true;
    return;
  }

  backend->audio_buffer_initialized = true;
  backend_ = backend;
}

LoopingWavSound::~LoopingWavSound() {
  Stop();
  MiniaudioSoundBackend *backend =
      static_cast<MiniaudioSoundBackend *>(backend_);
  if (backend != NULL) {
    if (backend->audio_buffer_initialized)
      ma_audio_buffer_uninit(&backend->audio_buffer);
    delete backend;
    backend_ = NULL;
  }
}

void LoopingWavSound::SetPlaying(bool should_play) {
  if (!should_play) {
    Stop();
    return;
  }

  if (playing_ || error_reported_)
    return;
  if (!g_audio_engine_initialized) {
    fprintf(stderr, "ERROR: Audio system is not initialized for \"%s\".\n",
            path_.c_str());
    error_reported_ = true;
    return;
  }

  MiniaudioSoundBackend *backend =
      static_cast<MiniaudioSoundBackend *>(backend_);
  if (backend == NULL || !backend->audio_buffer_initialized) {
    error_reported_ = true;
    return;
  }

  ma_result result = ma_sound_init_from_data_source(
      &g_audio_engine, &backend->audio_buffer, MA_SOUND_FLAG_NO_SPATIALIZATION,
      NULL, &backend->sound);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "ERROR: Cannot initialize sound \"%s\" (%d).\n",
            path_.c_str(), static_cast<int>(result));
    error_reported_ = true;
    return;
  }

  backend->sound_initialized = true;
  ma_sound_set_looping(&backend->sound, MA_TRUE);
  ma_sound_seek_to_pcm_frame(&backend->sound, 0);
  ApplySoundControls(backend, gain_, volume_, playback_rate_);
  result = ma_sound_start(&backend->sound);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "ERROR: Cannot play sound \"%s\" (%d).\n",
            path_.c_str(), static_cast<int>(result));
    ma_sound_uninit(&backend->sound);
    backend->sound_initialized = false;
    error_reported_ = true;
    return;
  }

  playing_ = true;
}

void LoopingWavSound::SetGain(float gain) {
  gain_ = std::max(0.0f, std::min(gain, 4.0f));
  ApplySoundControls(static_cast<MiniaudioSoundBackend *>(backend_), gain_,
                     volume_, playback_rate_);
}

void LoopingWavSound::SetPlaybackRate(float playback_rate) {
  playback_rate_ = std::max(0.25f, std::min(playback_rate, 4.0f));
  ApplySoundControls(static_cast<MiniaudioSoundBackend *>(backend_), gain_,
                     volume_, playback_rate_);
}

void LoopingWavSound::SetVolume(float volume) {
  volume_ = std::max(0.0f, std::min(volume, 1.0f));
  ApplySoundControls(static_cast<MiniaudioSoundBackend *>(backend_), gain_,
                     volume_, playback_rate_);
}

void LoopingWavSound::Stop() {
  MiniaudioSoundBackend *backend =
      static_cast<MiniaudioSoundBackend *>(backend_);
  if (backend != NULL && backend->sound_initialized) {
    ma_sound_stop(&backend->sound);
    ma_sound_seek_to_pcm_frame(&backend->sound, 0);
    ma_sound_uninit(&backend->sound);
    backend->sound_initialized = false;
  }
  playing_ = false;
}
