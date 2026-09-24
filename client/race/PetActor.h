// one pet on a car plays the KFM id of its driver and hovers beside the seat
#pragma once

#include <cstddef>
#include <string>

namespace KnC::Tools { struct GhostDriver; }
namespace KnC::Render { struct CharacterInstance; }

namespace KnC::Client {

// driver place on car 0x48B800 in a race or the stand hover sub 4A51B0 of the lobby garage and shop
enum class PetHoverKind { Race, Preview };

// the Pet Body nif and the Pet Facial folder of one 0x0103 model folder empty when missing
struct PetFiles {
    std::string nif;
    std::string facialDir;
};
PetFiles petFiles(const std::string& gameDir, const std::string& modelFolder);

class PetActor {
public:
    void reset(PetHoverKind kind);
    // set sequence 0x48B460 hands the driver KFM id to the pet with the car speed
    void follow(int sequenceId, float speed);
    // steps the hover on the scene clock then builds the pet instance on the seat turned a quarter
    void place(const KnC::Tools::GhostDriver& body, std::size_t modelIndex, const float seat[3], const float carWorld[16],
               float clockSeconds, KnC::Render::CharacterInstance& out);

private:
    void hover(float frames, double clockSeconds);

    PetHoverKind m_kind = PetHoverKind::Race;
    int m_sequence = 0;
    float m_speed = 0.f;
    int m_clip = -2;
    float m_clipStart = 0.f;
    float m_lastClock = -1.f;
    // the three state machines of the pet manager 0xE8A0 0xE8A4 0xE8A8
    int m_trailState = 0;
    float m_trail = 0.f;
    double m_trailAt = 0.0;
    int m_sideState = 0;
    float m_side = 0.f;
    double m_sideAt = 0.0;
    int m_bobState = 0;
    float m_bob = 0.f;
};

}
