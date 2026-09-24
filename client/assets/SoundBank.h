// the wav clips of the pak on miniaudio music one shots and the engine loop
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace KnC::Client {

class AssetStore;

class SoundBank {
public:
    SoundBank();
    ~SoundBank();
    SoundBank(const SoundBank&) = delete;
    SoundBank& operator=(const SoundBank&) = delete;

    // opens the device muted means no device and every call is a no op
    bool init(AssetStore& assets, bool mute);
    void shutdown();
    bool enabled() const { return m_ready; }

    // one shot of Sound High name wav the clip is decoded once and kept
    void play(const std::string& name, float volume = 1.f);
    // the looping track name empty stops it the same name keeps playing
    void music(const std::string& name, float volume = 0.6f);
    // the engine loop pitch one is the file as recorded zero stops it
    void engine(const std::string& name, float pitch, float volume);
    void stopEngine();
    void stopAll();
    // the Option2 ini groups the one shots and the engine loop each scale by their gain 0 to 1
    void setGroupGain(float effect, float kart);
    float effectGain() const { return m_effectGain; }
    float kartGain() const { return m_kartGain; }
    // the names wired so far for the report
    const std::vector<std::string>& played() const { return m_played; }

private:
    struct Clip;
    struct Voice;
    struct Impl;
    Clip* clip(const std::string& name);
    std::unique_ptr<Impl> m_impl;
    AssetStore* m_assets = nullptr;
    bool m_ready = false;
    std::string m_musicName;
    std::string m_engineName;
    std::vector<std::string> m_played;
    float m_effectGain = 1.f;
    float m_kartGain = 1.f;
    float m_engineVolume = 0.f;
};

}
