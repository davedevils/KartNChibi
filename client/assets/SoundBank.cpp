#include "SoundBank.h"

#include "AssetStore.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_FLAC
#define MA_NO_MP3
#include "miniaudio.h"

#include <cstdio>

namespace KnC::Client {

namespace {

// the one shots share this many voices the oldest is reused when they are all busy
constexpr size_t kVoiceCount = 12;
constexpr ma_uint32 kChannels = 2;

}

// one decoded clip stereo 16 bit at the rate of the file the engine resamples it
struct SoundBank::Clip {
    std::vector<int16_t> frames;
    ma_uint64 frameCount = 0;
    ma_uint32 sampleRate = 0;
    bool ok = false;
};

// one playing instance a ref on the clip memory and the sound on it
struct SoundBank::Voice {
    ma_audio_buffer_ref ref{};
    ma_sound sound{};
    bool used = false;
    uint64_t serial = 0;
};

struct SoundBank::Impl {
    ma_engine engine{};
    std::map<std::string, std::unique_ptr<Clip>> clips;
    std::vector<std::unique_ptr<Voice>> voices;
    Voice music;
    Voice engineLoop;
    uint64_t serial = 0;

    void stopVoice(Voice& v) {
        if (!v.used) return;
        ma_sound_uninit(&v.sound);
        ma_audio_buffer_ref_uninit(&v.ref);
        v.used = false;
    }

    bool startVoice(Voice& v, Clip& clip, bool loop, float volume, float pitch) {
        stopVoice(v);
        if (ma_audio_buffer_ref_init(ma_format_s16, kChannels, clip.frames.data(), clip.frameCount, &v.ref) != MA_SUCCESS)
            return false;
        v.ref.sampleRate = clip.sampleRate;
        if (ma_sound_init_from_data_source(&engine, &v.ref, 0, nullptr, &v.sound) != MA_SUCCESS) {
            ma_audio_buffer_ref_uninit(&v.ref);
            return false;
        }
        ma_sound_set_looping(&v.sound, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&v.sound, volume);
        ma_sound_set_pitch(&v.sound, pitch);
        v.used = true;
        v.serial = ++serial;
        return ma_sound_start(&v.sound) == MA_SUCCESS;
    }
};

SoundBank::SoundBank() : m_impl(std::make_unique<Impl>()) {}

SoundBank::~SoundBank() { shutdown(); }

bool SoundBank::init(AssetStore& assets, bool mute) {
    m_assets = &assets;
    if (mute) {
        std::printf("[sound] muted\n");
        return false;
    }
    ma_engine_config config = ma_engine_config_init();
    config.channels = kChannels;
    if (ma_engine_init(&config, &m_impl->engine) != MA_SUCCESS) {
        std::printf("[sound] no audio device the client stays silent\n");
        return false;
    }
    for (size_t i = 0; i < kVoiceCount; ++i) m_impl->voices.push_back(std::make_unique<Voice>());
    m_ready = true;
    std::printf("[sound] miniaudio engine at %u Hz\n", ma_engine_get_sample_rate(&m_impl->engine));
    return true;
}

void SoundBank::shutdown() {
    if (!m_ready) return;
    stopAll();
    m_impl->voices.clear();
    m_impl->clips.clear();
    ma_engine_uninit(&m_impl->engine);
    m_ready = false;
}

// shared by play music and engine caches by name in the clip map
SoundBank::Clip* SoundBank::clip(const std::string& name) {
    auto it = m_impl->clips.find(name);
    if (it != m_impl->clips.end()) return it->second->ok ? it->second.get() : nullptr;
    auto c = std::make_unique<Clip>();
    std::vector<uint8_t> bytes;
    const std::string path = "Data/Public/Sound/High/" + name + ".wav";
    if (!m_assets || !m_assets->readBytes(path, bytes)) {
        std::printf("[sound] not found %s\n", path.c_str());
    } else {
        ma_decoder_config dc = ma_decoder_config_init(ma_format_s16, kChannels, 0);
        ma_decoder decoder;
        if (ma_decoder_init_memory(bytes.data(), bytes.size(), &dc, &decoder) == MA_SUCCESS) {
            ma_uint64 length = 0;
            if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) == MA_SUCCESS && length > 0) {
                c->frames.resize(static_cast<size_t>(length) * kChannels);
                ma_uint64 got = 0;
                ma_decoder_read_pcm_frames(&decoder, c->frames.data(), length, &got);
                c->frameCount = got;
                c->sampleRate = decoder.outputSampleRate;
                c->ok = got > 0 && c->sampleRate > 0;
            }
            ma_decoder_uninit(&decoder);
        }
        if (c->ok) {
            std::printf("[sound] loaded %s %llu frames at %u Hz\n", path.c_str(), static_cast<unsigned long long>(c->frameCount), c->sampleRate);
            m_played.push_back(name);
        } else {
            std::printf("[sound] cannot decode %s\n", path.c_str());
        }
    }
    Clip* raw = c.get();
    const bool ok = c->ok;
    m_impl->clips[name] = std::move(c);
    return ok ? raw : nullptr;
}

void SoundBank::play(const std::string& name, float volume) {
    if (!m_ready) return;
    Clip* c = clip(name);
    if (!c) return;
    Voice* pick = nullptr;
    for (auto& v : m_impl->voices) {
        if (!v->used || ma_sound_at_end(&v->sound) || !ma_sound_is_playing(&v->sound)) { pick = v.get(); break; }
    }
    if (!pick) {
        for (auto& v : m_impl->voices) if (!pick || v->serial < pick->serial) pick = v.get();
    }
    m_impl->startVoice(*pick, *c, false, volume * m_effectGain, 1.f);
}

void SoundBank::music(const std::string& name, float volume) {
    if (!m_ready) return;
    if (name == m_musicName) return;
    m_impl->stopVoice(m_impl->music);
    // a loop is minutes of samples the old one goes when the next starts
    if (!m_musicName.empty()) m_impl->clips.erase(m_musicName);
    m_musicName = name;
    if (name.empty()) return;
    Clip* c = clip(name);
    if (!c) return;
    m_impl->startVoice(m_impl->music, *c, true, volume, 1.f);
}

void SoundBank::engine(const std::string& name, float pitch, float volume) {
    if (!m_ready) return;
    if (pitch <= 0.f) { stopEngine(); return; }
    m_engineVolume = volume;
    if (name != m_engineName || !m_impl->engineLoop.used) {
        m_impl->stopVoice(m_impl->engineLoop);
        m_engineName = name;
        Clip* c = clip(name);
        if (!c) return;
        m_impl->startVoice(m_impl->engineLoop, *c, true, volume * m_kartGain, pitch);
        return;
    }
    ma_sound_set_pitch(&m_impl->engineLoop.sound, pitch);
    ma_sound_set_volume(&m_impl->engineLoop.sound, volume * m_kartGain);
}

// a running engine loop takes the new gain at once the next one shots take theirs when they start
void SoundBank::setGroupGain(float effect, float kart) {
    m_effectGain = effect < 0.f ? 0.f : effect > 1.f ? 1.f : effect;
    m_kartGain = kart < 0.f ? 0.f : kart > 1.f ? 1.f : kart;
    if (m_ready && m_impl->engineLoop.used) ma_sound_set_volume(&m_impl->engineLoop.sound, m_engineVolume * m_kartGain);
    std::printf("[sound] group gain effect %.2f kart %.2f\n", m_effectGain, m_kartGain);
}

void SoundBank::stopEngine() {
    if (!m_ready) return;
    m_impl->stopVoice(m_impl->engineLoop);
    m_engineName.clear();
}

void SoundBank::stopAll() {
    if (!m_ready) return;
    for (auto& v : m_impl->voices) m_impl->stopVoice(*v);
    m_impl->stopVoice(m_impl->music);
    m_impl->stopVoice(m_impl->engineLoop);
    m_musicName.clear();
    m_engineName.clear();
}

}
