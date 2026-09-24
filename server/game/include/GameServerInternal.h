/// small helpers GameServer split files share not part of the public API

#pragma once

#include "net/Protocol.h"
#include "game/Room.h"
#include "packets/PacketBuilder.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>

namespace knc {

/// two byte hex of one byte used in log lines across the split files
std::string toHex(uint8_t v);

/// four digit hex of one opcode used in log lines across the split files
std::string toHex16(uint16_t v);

/// two id spaces track id and map id are not interchangeable mixing them loads the wrong world
struct RoomTrackChoice {
    int32_t trackId = 0;   // what the client resolves in the 0xC3 catalogue
    int32_t mapId   = 0;
};

/// the room track picked for a wanted map id or the first race track with a map link
RoomTrackChoice defaultRoomTrack(int32_t wantedMapId);

/// room stores 0 none 1 red 2 blue the wire speaks 0 red 1 blue see 0x21 and 0x64
inline uint32_t wireTeam(uint8_t stored) { return stored == 2 ? 1u : 0u; }

/// true when the vehicle templates row is flagged as a factory car
bool factoryFlag(const std::map<std::string, std::string>& row);

/// maps a driver name to the DevClient Driver Body High Asset folder the client renders
std::string driverBodyAsset(const std::string& dbName);

/// only BLUE GREEN PURPLE RED YELLOW body colors exist a missing color loads no texture and fails SetBody
extern const int32_t kDefaultPaintKey;
/// NAMEBOX NORMAL is the stock plate the twenty numbered ones are shop items
extern const int32_t kDefaultPlateKey;

/// KNC KART STATS 0 puts the 0xC0 stat block back to all zero for testing
bool kartStatsEnabled();

/// the 17 wire floats the 0xC0 record ships for one template row the anti cheat reads the same
std::array<float, 17> kartWireStats(const std::map<std::string, std::string>& templateRow);

/// the same block for one template id false when the row is missing factory says whether parts apply
bool kartWireStatsForTemplate(int32_t templateId, std::array<float, 17>& out, bool& factory);

/// room row keeps the loadout the grid packet reads a zero kart key drops the client on the first 0x3E
void applyLoadout(RoomPlayer* rp, int32_t charId);

/// C CREATE ROOM REQ 0x2D wire format from sub 480CC0 the 0x21 member must match sub 40D650 lookup
RoomMemberWire buildBotMember(const RoomPlayer& bot);

/// one room member record for a real player built from the owned character and kart rows
RoomMemberWire buildRoomMember(int32_t charId, const std::u16string& name,
                               uint32_t slot, uint8_t team, bool ready);

}  // namespace knc
