/// one writer for S2C 0x00C0 every builder that emits a kart definition calls this

#pragma once

#include "net/Packet.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace knc {

/// kart ability pairs draw 0x42B910 draws ids 0 to 25 the real server sends this on every kart row
constexpr int32_t KART_ABILITY_NONE = -1;

/// one 0x00C0 ability pair the id then the percent id 0 percent 0 draws an icon with 0%
struct KartAbilityPair {
    int32_t  id      = KART_ABILITY_NONE;  ///< id 0 to 25 draws the icon anything else hides the pair percent prints after the icon
    uint32_t percent = 0;
};

/// every 0x00C0 field up to the price options see docs packets opcodes 0x00C0 for the readers
struct KartDefinitionFields {
    uint32_t visibleFlag = 0;     ///< visibleFlag wire 0x00 zero hides the row badge wire 0x04 tile overlay 1 new 2 hot
    uint32_t badge = 0;
    uint32_t kartKey = 0;         ///< kartKey wire 0x08 the catalog lookup key unk0c wire 0x0C one byte not four no reader yet
    uint8_t  unk0c = 0;
    int32_t  vehicleKind = 0;     ///< vehicleKind wire 0x0D car kind 2 bike kind 5 spinner modelScheme wire 0x11 0 catalogue 1 factory gates part bonuses
    int32_t  modelScheme = 0;
    int32_t  unk18 = 0;           ///< unk18 wire 0x15 no reader yet requiredLevel wire 0x19 player level gate against byte 0x1A20B09
    int32_t  requiredLevel = 0;
    std::string modelName;        ///< modelName wire cstr chassis path and compare key displayNameKey wire cstr title key shop detail panel
    std::string displayNameKey;
    std::string descriptionKey;   ///< descriptionKey wire cstr description key defaultSkinKeys wire 32 bytes eight default skin keys record 0x84
    std::array<uint8_t, 32> defaultSkinKeys{};
    std::array<float, 17>   stats{};             ///< stats wire 68 bytes 17 LE floats abilityPair0 wire 8 bytes record 0x130 hidden unless id is 0 to 25
    KartAbilityPair abilityPair0;
    KartAbilityPair abilityPair1;                ///< abilityPair1 wire 8 bytes record 0x138 same shape as abilityPair0
};

/// fixed bytes before the three NUL terminated strings matches the RE notes 149 figure
constexpr size_t KART_DEFINITION_FIXED_BYTES = 149;

/// writes the 0x00C0 body strings must already be clamped the caller writes price options after
void writeKartDefinitionBody(Packet& pkt, const KartDefinitionFields& f);

}  // namespace knc
