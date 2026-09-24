/// sub 47D5E0 handles opcode 0x00ED sub 47EEF0 handles 0x0135 and sub 456B90 draws the period sprite

#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/GachaPetPackets.h"

#include <cstdint>
#include <vector>

namespace knc {

class GameServer;

/// gacha and pet handler all static with one forced roll slot per character
class GachaHandler {
public:

    /// C2S 0x00ED is 28 bytes and always answers one S2C 0x00ED with ticket spend and prize grant atomic
    static void handleRoll(Session::Ptr session, Packet& packet, GameServer* server);

    /// pins the next roll of one character to an exact value consumed once and reduced modulo table weight
    static void forceNextRoll(uint32_t characterId, uint64_t rollValue);

    /// S2C 0x00ED with a caller supplied tail for a scripted or GM award must match the owned record
    static void sendRawPrize(Session::Ptr session,
                             const GachaPetPackets::GachaHeader& header,
                             const GachaPetPackets::ItemRow& ticket,
                             const std::vector<uint8_t>& tail);

    /// category is the 0x00B7 enum not the gacha one and category 4 pet is refused
    static void sendRandomReward(Session::Ptr session, uint32_t category,
                                 uint32_t prizeBaseKey, int32_t resultCode);

    /// S2C 0x0103 catalog then S2C 0x0104 owned list sent once per character enter
    static void onCharacterEnter(Session::Ptr session, GameServer* server);

    /// S2C 0x0103 one frame per pet trimmed to the client cap
    static size_t sendPetCatalog(Session::Ptr session);

    /// S2C 0x0103 for one pet for a catalog refresh after a shop edit
    static void sendPetDefinition(Session::Ptr session, uint32_t petBaseKey);

    /// S2C 0x0104 full replace of the owned pet container
    static void sendOwnedPets(Session::Ptr session);

    /// equipped pet base key feeds room member 0x0021 and grid racer 0x003E or 0 when none
    static uint32_t equippedPetBaseKey(uint32_t characterId);

    /// true when a C2S 0x00B7 leads with category 4
    static bool isPetBuy(const Packet& packet);

    /// true when a C2S 0x00B9 is 12 bytes and leads with category 4
    static bool isPetEquip(const Packet& packet);

    /// true when a C2S 0x00BA is 8 bytes and leads with category 4
    static bool isPetUnequip(const Packet& packet);

    /// C2S 0x00B7 category 4 answers S2C 0x00B7 on success only trailing wstring is a launcher session token not a username
    static void handlePetBuy(Session::Ptr session, Packet& packet, GameServer* server);

    /// C2S 0x00B9 category 4 answers sub op 1 or the 64 byte swap form
    static void handlePetEquip(Session::Ptr session, Packet& packet, GameServer* server);

    /// C2S 0x00BA category 4 answers the 32 byte ack with the flag cleared
    static void handlePetUnequip(Session::Ptr session, Packet& packet, GameServer* server);
};

} // namespace knc
