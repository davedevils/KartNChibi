// Ghost replay file and database reader the file layout is in docs tools README
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "remote_car.h"

namespace KnC::Tools {

/// one loaded or saved ghost run name time car kind blocks and samples
struct GhostRecording {
    std::string name;
    int32_t trackId = 0;
    uint32_t charId = 0;
    /// lap time in ms zero if unknown
    int32_t timeMs = 0;
    /// 3 or 4 in live game frameCount may differ from samples size
    uint32_t carKind = 0;
    uint32_t frameCount = 0;
    /// same layout as the ghost record char and kart blocks
    std::array<uint8_t, 0x2C> charBlock{};
    std::array<uint8_t, 0x38> kartBlock{};
    std::vector<KnC::Kart::Client::GhostSample> samples;
};

/// pose at one moment of playback position yaw drift state and boost state
struct GhostPose {
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float yawDeg = 0.0f;
    bool boosting = false;
    int boostKind = 0;       ///< 0 miniturbo 1 item only meaningful when boosting
    bool reversing = false;
    int driftState = 0;      ///< 0 none 1 or 2 while drifting
    int miniTurboStage = 0;
    int turnState = 0;       ///< 0 1 or 2 cosmetic lean side
};

/// loads a ghost file with or without the small header see the layout above
bool load_ghost_file(const std::string& path, GhostRecording& out, std::string& error);

/// saves a ghost file always with the small header
bool save_ghost_file(const std::string& path, const GhostRecording& recording, std::string& error);

/// Loads one ghost record and its chunks from a live database false when this build has no connector
bool load_ghost_db(const std::string& host, uint16_t port, const std::string& user,
                    const std::string& password, const std::string& database,
                    int32_t trackId, uint32_t charId, GhostRecording& out, std::string& error);

/// walks a recording forward in time and blends pose the way the client does
class GhostPlayback {
public:
    explicit GhostPlayback(const GhostRecording& recording);

    void advance(float seconds);
    void seek(float seconds);       ///< clamped at zero
    void restart();

    bool finished() const;
    GhostPose pose() const;         ///< blended pose finished is true past the last sample
    float elapsedSeconds() const;
    float durationSeconds() const;  ///< sample count times the sample interval
    size_t sampleCount() const;

private:
    GhostRecording m_recording;
    float m_elapsed = 0.0f;
};

}  // namespace KnC Tools
