/// the pendant rules of the stock client the owned row the equip answer and the grants the text table names

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace knc {

/// sub 451250 erases by instance found by key each owned row needs its id since one key per character
inline uint32_t pendantInstanceId(uint32_t pendantKey) { return pendantKey; }

/// the 8 byte row of 0x011A 0x011B and of a type 7 reward instance then key
inline std::array<uint8_t, 8> pendantRow(uint32_t pendantKey) {
    const uint32_t instance = pendantInstanceId(pendantKey);
    std::array<uint8_t, 8> out{};
    for (int i = 0; i < 4; ++i) {
        out[static_cast<size_t>(i)] = static_cast<uint8_t>((instance >> (i * 8)) & 0xFF);
        out[static_cast<size_t>(4 + i)] = static_cast<uint8_t>((pendantKey >> (i * 8)) & 0xFF);
    }
    return out;
}

/// C2S 0x0123 minus one takes it off any key the character owns goes on anything else keeps the worn one
inline int32_t pendantEquipApplied(int32_t wanted, bool owned, int32_t current) {
    if (wanted <= 0) return 0;
    return owned ? wanted : current;
}

/// PENDANT 08 to 12 of the text table level 10 20 30 40 50 zero below 10
inline uint32_t levelPendantFor(uint32_t level) {
    if (level >= 50) return 12;
    if (level >= 40) return 11;
    if (level >= 30) return 10;
    if (level >= 20) return 9;
    if (level >= 10) return 8;
    return 0;
}

/// every level pendant a character of this level holds so a jump of two tiers grants both
inline std::vector<uint32_t> levelPendantsUpTo(uint32_t level) {
    std::vector<uint32_t> out;
    for (uint32_t tier = 10; tier <= 50 && tier <= level; tier += 10) out.push_back(levelPendantFor(tier));
    return out;
}

/// PENDANT 06 race 100 times and PENDANT 07 race 1000 times
inline std::vector<uint32_t> racePendantsFor(uint32_t totalRaces) {
    std::vector<uint32_t> out;
    if (totalRaces >= 100) out.push_back(6);
    if (totalRaces >= 1000) out.push_back(7);
    return out;
}

/// PENDANT 01 you passed the tutorial the rookie licence grade
constexpr uint32_t kTutorialPendant = 1;

/// PENDANT 02 to 05 finish quest level 5 10 15 20 scenario keys of Rosie Chai Porki and Dim Dim
inline uint32_t questPendantFor(uint32_t scenarioKey) {
    switch (scenarioKey) {
        case 5:  return 2;
        case 10: return 3;
        case 15: return 4;
        case 20: return 5;
        default: return 0;
    }
}

/// PENDANT 13 the hidden row the mission menu chapter 1 missions 0 to 4 all cleared
constexpr uint32_t kMissionChapterOnePendant = 13;

/// sub 43BAD0 groups five missions a chapter and chapter 1 is mission 0 to 4
inline bool missionChapterOneCleared(const std::vector<uint32_t>& clearedMissionIds) {
    for (uint32_t id = 0; id < 5; ++id) {
        bool found = false;
        for (uint32_t c : clearedMissionIds) {
            if (c == id) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

}  // namespace knc
