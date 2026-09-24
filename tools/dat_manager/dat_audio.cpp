#include "dat_audio.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_FLAC
#include "miniaudio/miniaudio.h"

#include <cstdio>

namespace KnC::Tools {

namespace {

// Every clip plays as stereo the engine resamples it to the device rate
constexpr ma_uint32 kChannels = 2;

}

struct AudioPlayer::Impl {
    ma_engine            engine{};
    std::vector<int16_t> frames;
    ma_uint64            frame_count = 0;
    ma_audio_buffer_ref  ref{};
    ma_sound             sound{};
    bool                 voice = false;

    void drop_voice() {
        if (!voice) return;
        ma_sound_uninit(&sound);
        ma_audio_buffer_ref_uninit(&ref);
        voice = false;
    }
};

AudioPlayer::AudioPlayer() : impl_(std::make_unique<Impl>()) {}

AudioPlayer::~AudioPlayer() { shutdown(); }

bool AudioPlayer::init() {
    if (ready_) return true;
    ma_engine_config config = ma_engine_config_init();
    config.channels = kChannels;
    if (ma_engine_init(&config, &impl_->engine) != MA_SUCCESS) {
        std::printf("[audio] no audio device the preview stays silent\n");
        return false;
    }
    ready_ = true;
    std::printf("[audio] miniaudio engine at %u Hz\n", ma_engine_get_sample_rate(&impl_->engine));
    return true;
}

void AudioPlayer::shutdown() {
    unload();
    if (!ready_) return;
    ma_engine_uninit(&impl_->engine);
    ready_ = false;
}

bool AudioPlayer::load(const std::vector<uint8_t>& bytes, std::string& error) {
    unload();
    if (bytes.empty()) {
        error = "empty clip";
        return false;
    }
    ma_decoder_config config = ma_decoder_config_init(ma_format_s16, kChannels, 0);
    ma_decoder decoder;
    if (ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder) != MA_SUCCESS) {
        error = "miniaudio cannot decode this clip";
        return false;
    }
    ma_uint64 length = 0;
    ma_uint64 got = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) == MA_SUCCESS && length > 0) {
        impl_->frames.resize(static_cast<size_t>(length) * kChannels);
        ma_decoder_read_pcm_frames(&decoder, impl_->frames.data(), length, &got);
    }
    sample_rate_ = decoder.outputSampleRate;
    channels_ = kChannels;
    ma_format native_format = ma_format_unknown;
    ma_uint32 native_channels = 0;
    ma_uint32 native_rate = 0;
    if (decoder.pBackend != nullptr &&
        ma_data_source_get_data_format(decoder.pBackend, &native_format, &native_channels, &native_rate,
                                       nullptr, 0) == MA_SUCCESS && native_channels != 0)
        channels_ = native_channels;
    ma_decoder_uninit(&decoder);
    if (got == 0 || sample_rate_ == 0) {
        error = "the clip holds no frames";
        impl_->frames.clear();
        return false;
    }
    impl_->frame_count = got;
    duration_ = static_cast<float>(got) / static_cast<float>(sample_rate_);
    loaded_ = true;
    return true;
}

void AudioPlayer::unload() {
    impl_->drop_voice();
    impl_->frames.clear();
    impl_->frame_count = 0;
    duration_ = 0.f;
    loaded_ = false;
}

void AudioPlayer::play() {
    if (!ready_ || !loaded_) return;
    impl_->drop_voice();
    if (ma_audio_buffer_ref_init(ma_format_s16, kChannels, impl_->frames.data(), impl_->frame_count,
                                 &impl_->ref) != MA_SUCCESS)
        return;
    impl_->ref.sampleRate = sample_rate_;
    if (ma_sound_init_from_data_source(&impl_->engine, &impl_->ref, 0, nullptr, &impl_->sound) != MA_SUCCESS) {
        ma_audio_buffer_ref_uninit(&impl_->ref);
        return;
    }
    impl_->voice = true;
    ma_sound_start(&impl_->sound);
}

void AudioPlayer::stop() { impl_->drop_voice(); }

bool AudioPlayer::playing() const {
    return impl_->voice && ma_sound_is_playing(&impl_->sound) && !ma_sound_at_end(&impl_->sound);
}

float AudioPlayer::position() const {
    if (!impl_->voice) return 0.f;
    float seconds = 0.f;
    ma_sound_get_cursor_in_seconds(&impl_->sound, &seconds);
    return seconds;
}

}
