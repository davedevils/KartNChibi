// Audio preview on miniaudio one clip decoded in memory play stop and the cursor
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace KnC::Tools {

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Opens the device false leaves the decode working and the play silent
    bool init();
    void shutdown();
    bool ready() const { return ready_; }

    // Decodes the whole clip false when the bytes are not a known format
    bool load(const std::vector<uint8_t>& bytes, std::string& error);
    void unload();
    bool loaded() const { return loaded_; }

    void play();
    void stop();
    bool playing() const;
    float position() const;
    float duration() const { return duration_; }
    uint32_t sample_rate() const { return sample_rate_; }
    uint32_t channels() const { return channels_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool     ready_ = false;
    bool     loaded_ = false;
    float    duration_ = 0.f;
    uint32_t sample_rate_ = 0;
    uint32_t channels_ = 0;
};

}
