#include "packets/PacketBuilder.h"
#include "packets/KartDefinitionWire.h"
#include "net/ProfileBlob.h"
#include "logging/Logger.h"

#include <cstring>
#include <ctime>

namespace knc {

// migration 060 the two keys every owned kart row defaults to a zero key hides the kart
static constexpr int32_t kRecordDefaultPaintKey = 9007;
static constexpr int32_t kRecordDefaultPlateKey = 9100;

void PacketBuilder::writeWString(Packet& pkt, const std::u16string& str) {
    for (char16_t c : str) {
        pkt.writeUInt8(static_cast<uint8_t>(c & 0xFF));
        pkt.writeUInt8(static_cast<uint8_t>((c >> 8) & 0xFF));
    }
    pkt.writeUInt16(0);
}

// catalog handlers sub 44eb30 read raw NUL terminated ASCII not wide
static void writeAsciiW(Packet& pkt, const std::u16string& str) {
    std::string ascii;
    ascii.reserve(str.size());
    for (char16_t c : str) ascii.push_back(static_cast<char>(c & 0xFF));
    pkt.writeString(ascii);
}

static void writeInt32LE(std::vector<uint8_t>& buf, size_t offset, int32_t val) {
    buf[offset + 0] = val & 0xFF;
    buf[offset + 1] = (val >> 8) & 0xFF;
    buf[offset + 2] = (val >> 16) & 0xFF;
    buf[offset + 3] = (val >> 24) & 0xFF;
}

void PacketBuilder::writePlayerInfo(Packet& pkt, const PlayerData& player) {
    // 1224 byte PlayerInfo offsets from IDA read into g NetHandler PlayerInfo via sub 480500
    std::vector<uint8_t> info(ProfileBlob::kSize, 0);

    // sub 480500 echoes the head on the channel return so it keeps the login ticket not the character
    ProfileBlob::writeTicket(info, static_cast<uint32_t>(player.accountId), player.ticketToken);

    // offset 0x486 display name 12 wchars max
    constexpr size_t NAME_OFFSET = 0x486;
    for (size_t i = 0; i < player.name.length() && i < 12; ++i) {
        info[NAME_OFFSET + i * 2] = static_cast<uint8_t>(player.name[i]);
        info[NAME_OFFSET + i * 2 + 1] = 0;
    }

    // stats sub block mirrors opcode 0x0A handler sub 47D3B0 layout
    info[0x4A0] = 1;
    info[0x4A1] = player.isGM ? 1 : 0;
    writeInt32LE(info, ProfileBlob::kExpCurrent, player.xp);
    writeInt32LE(info, ProfileBlob::kAstro, player.cash);
    writeInt32LE(info, ProfileBlob::kGold, player.gold);
    writeInt32LE(info, 0x4B0, player.vehicleId > 0 ? player.vehicleId : -1);
    writeInt32LE(info, 0x4B4, player.driverId > 0 ? player.driverId : -1);
    info[0x4B8] = static_cast<uint8_t>(player.licenseClass);
    info[0x4B9] = static_cast<uint8_t>(player.level);
    info[0x4BA] = player.tutorialCompleted ? 1 : 0;
    info[0x4BB] = 0;
    writeInt32LE(info, 0x4BC, player.wins);
    writeInt32LE(info, 0x4C0, player.losses);
    // sub 479080 copies the blob and 0x01A20B2C at 0x4C4 is the worn pendant the char panel draws
    writeInt32LE(info, 0x4C4, player.pendantKey);

    LOG_DEBUG("PACKET", "PlayerInfo build: ID=" + std::to_string(player.id) +
              " Name=" + player.name +
              " Gold=" + std::to_string(player.gold) +
              " Cash=" + std::to_string(player.cash) +
              " VehicleID=" + std::to_string(player.vehicleId) +
              " DriverID=" + std::to_string(player.driverId));

    for (uint8_t b : info) {
        pkt.writeUInt8(b);
    }
}

// auth packets

Packet PacketBuilder::connectionOk() {
    // cmd 0x0A last 38 bytes of PlayerInfo sub 47D3B0
    Packet pkt(CMD::S_CONNECTION_OK);

    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(-1);
    pkt.writeInt32(-1);
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);

    return pkt;
}

Packet PacketBuilder::connectionOkWithPlayer(const PlayerData& player) {
    // 0x0A handler sub 47D3B0 stores fields from offset 0x4A0 exp and gold were swapped before
    Packet pkt(CMD::S_CONNECTION_OK);

    pkt.writeUInt8(1);
    pkt.writeUInt8(player.isGM ? 1 : 0);
    // EXP was gold and GOLD was cash in the buggy version cash is astros
    pkt.writeInt32(player.xp);
    pkt.writeInt32(player.cash);
    pkt.writeInt32(player.gold);
    pkt.writeInt32(player.vehicleTemplateId > 0 ? player.vehicleTemplateId : -1);
    pkt.writeInt32(player.driverId > 0 ? player.driverId : -1);
    pkt.writeUInt8(static_cast<uint8_t>(player.licenseClass));
    pkt.writeUInt8(static_cast<uint8_t>(player.level));
    pkt.writeUInt8(player.tutorialCompleted ? 1 : 0);
    pkt.writeUInt8(0);
    pkt.writeInt32(player.wins);
    pkt.writeInt32(player.losses);
    // plus 0x22 lands on the worn pendant at 0x47D48A
    pkt.writeInt32(player.pendantKey);

    return pkt;
}

Packet PacketBuilder::displayMessage(const std::u16string& msg, int32_t code) {
    // code one gates UI one else disconnects code two
    Packet pkt(CMD::S_DISPLAY_MESSAGE);
    writeWString(pkt, msg);
    pkt.writeInt32(code);
    return pkt;
}

Packet PacketBuilder::messageKey(const std::string& key, int32_t code) {
    Packet pkt(CMD::S_LOGIN_RESPONSE);
    pkt.writeString(key);
    pkt.writeInt32(code);
    return pkt;
}

Packet PacketBuilder::loginResponse(bool success, const std::string& msg) {
    Packet pkt(CMD::S_LOGIN_RESPONSE);
    pkt.writeString(msg);
    pkt.writeInt32(success ? 0 : 1);
    return pkt;
}

Packet PacketBuilder::sessionConfirm(int32_t driverId, const PlayerData& player) {
    // modern client also sends 0xA7 both 0x07 and 0xA7 read 4 plus 1224 bytes 0xA7 skips the launcher signal
    Packet pkt(CMD::S_SESSION_CONFIRM);
    pkt.writeInt32(driverId);
    writePlayerInfo(pkt, player);
    return pkt;
}

Packet PacketBuilder::heartbeatResponse() {
    // flag zero else cmdFull shift hits wrong handler crash
    Packet pkt(CMD::S_HEARTBEAT_RESP);
    return pkt;
}

Packet PacketBuilder::ack() {
    Packet pkt(CMD::S_ACK);
    return pkt;
}

Packet PacketBuilder::initResponse() {
    // cmd 0x0B bidirectional ACK 0x8E has no client handler so reuse this
    Packet pkt(CMD::S_ACK);
    return pkt;
}

Packet PacketBuilder::trigger() {
    // cmd 0x03 S2C SetGameVar sub 479220 opens nickname screen
    Packet pkt(CMD::S_SET_GAME_VAR);
    return pkt;
}

Packet PacketBuilder::registrationResponse(int32_t result, const std::u16string& nickname,
                                           const VehicleInfo* vehicle, const ItemInfo* item) {
    // cmd 0x04 S2C RegisterNickResult sub 479230
    Packet pkt(CMD::S_REGISTER_NICK_RES);
    pkt.writeInt32(result);
    
    if (result == 0 && vehicle && item) {
        writeWString(pkt, nickname);

        pkt.writeInt32(vehicle->id);
        pkt.writeInt32(vehicle->templateId);
        pkt.writeInt32(vehicle->durability);
        pkt.writeInt32(vehicle->maxDurability);
        for (int i = 0; i < 7; ++i) pkt.writeInt32(vehicle->stats[i]);

        pkt.writeInt32(item->id);
        pkt.writeInt32(item->templateId);
        pkt.writeInt32(item->quantity);
        for (int i = 0; i < 11; ++i) pkt.writeInt32(0);
    }
    
    return pkt;
}

Packet PacketBuilder::dataPairs(const std::vector<std::pair<int32_t, int32_t>>& pairs) {
    // cmd 0x0C
    Packet pkt(CMD::S_DATA_PAIRS);
    pkt.writeInt32(static_cast<int32_t>(pairs.size()));
    for (const auto& [key, value] : pairs) {
        pkt.writeInt32(key);
        pkt.writeInt32(value);
    }
    return pkt;
}

Packet PacketBuilder::flagSet() {
    // cmd 0x0D
    Packet pkt(CMD::S_FLAG_SET);
    return pkt;
}

Packet PacketBuilder::uiState24() {
    // cmd 0x8F sub 47E950 tutorial menu
    Packet pkt(CMD::S_UI_STATE_24);
    return pkt;
}

Packet PacketBuilder::uiState25(int32_t val1, int32_t val2) {
    // cmd 0x90
    Packet pkt(CMD::S_UI_STATE_25);
    pkt.writeInt32(val1);
    pkt.writeInt32(val2);
    return pkt;
}

// ui packets

Packet PacketBuilder::showMenu() {
    // cmd 0x11 flag zero else wrong handler
    Packet pkt(CMD::S_SHOW_MENU);
    return pkt;
}

Packet PacketBuilder::showLobby(const std::vector<RoomData>& rooms) {
    // cmd 0x12 zero bytes rooms arg kept for api
    (void)rooms;
    Packet pkt(CMD::S_SHOW_LOBBY);
    return pkt;
}

Packet PacketBuilder::showRoom(const RoomData& room, const std::vector<PlayerData>& players) {
    // cmd 0x13 sub 47FC20 sets ui state 9

    Packet pkt(CMD::S_PLAYER_ROOM_DATA);

    pkt.writeInt32(static_cast<int32_t>(room.id));

    for (char c : room.name) {
        pkt.writeUInt8(static_cast<uint8_t>(c));
        pkt.writeUInt8(0);
    }
    pkt.writeUInt16(0);

    pkt.writeInt32(room.mode);
    pkt.writeInt32(room.mapId);
    pkt.writeInt32(room.laps);
    pkt.writeInt32(room.maxPlayers);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(room.state);
    pkt.writeInt32(room.isPrivate ? 1 : 0);

    pkt.writeInt32(static_cast<int32_t>(players.size()));

    // each player 48 bytes
    for (const auto& p : players) {
        std::vector<uint8_t> playerData(48, 0);

        playerData[0] = static_cast<uint8_t>(p.id & 0xFF);
        playerData[1] = static_cast<uint8_t>((p.id >> 8) & 0xFF);
        playerData[2] = static_cast<uint8_t>((p.id >> 16) & 0xFF);
        playerData[3] = static_cast<uint8_t>((p.id >> 24) & 0xFF);

        playerData[4] = p.slot;
        playerData[5] = p.team;
        playerData[6] = p.ready ? 1 : 0;

        playerData[8] = static_cast<uint8_t>(p.vehicleId & 0xFF);
        playerData[9] = static_cast<uint8_t>((p.vehicleId >> 8) & 0xFF);
        playerData[10] = static_cast<uint8_t>((p.vehicleId >> 16) & 0xFF);
        playerData[11] = static_cast<uint8_t>((p.vehicleId >> 24) & 0xFF);
        
        for (uint8_t b : playerData) {
            pkt.writeUInt8(b);
        }
    }
    
    return pkt;
}

Packet PacketBuilder::showGarage() {
    Packet pkt(CMD::S_SHOW_GARAGE);
    return pkt;
}

Packet PacketBuilder::showShop() {
    Packet pkt(CMD::S_SHOW_SHOP);
    return pkt;
}

Packet PacketBuilder::channelList(const std::vector<std::pair<int, std::string>>& channels) {
    // cmd 0x0E sub 4793F0 sets ui state 4

    Packet pkt(CMD::S_CHANNEL_LIST);
    pkt.writeInt32(static_cast<int32_t>(channels.size()));

    for (const auto& [id, name] : channels) {
        pkt.writeInt32(id);
        for (char c : name) {
            pkt.writeUInt8(static_cast<uint8_t>(c));
            pkt.writeUInt8(0);
        }
        pkt.writeUInt16(0);
        pkt.writeInt32(0);
        pkt.writeInt32(100);
        pkt.writeInt32(0);
    }

    pkt.writeUInt16(0);
    return pkt;
}

// room packets

// no 0x63 and no 0x3F builder sub 47AC30 opens the MakeRoom popup and sub 47A050 despawns a racer

Packet PacketBuilder::playerRoomData(const RoomData& room,
                                     const std::vector<PlayerData>& players) {
    // 0x13 handler sub 47FC20 the 0x30 byte records build the room world through sub 4523A0 not the player list
    (void)players;

    Packet pkt(CMD::S_PLAYER_ROOM_DATA);

    // used to send room id where the master goes seat count where the track goes BCE1B0 is room id
    pkt.writeInt32(static_cast<int32_t>(room.id));

    std::u16string wname(room.name.begin(), room.name.end());
    writeWString(pkt, wname);

    // BCE22C seats BCE210 mode BCE220 host id BCE214 is track not map id BCE218 BCE21C BCE224 BCE244 unread here
    pkt.writeInt32(room.maxPlayers);
    pkt.writeInt32(room.mode);
    pkt.writeInt32(0);
    pkt.writeInt32(room.trackId);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(room.hostId);
    pkt.writeInt32(0);

    // the count and the 0x30 records are appended by appendRoomDecor
    return pkt;
}

Packet PacketBuilder::roomSlotEnabled(uint32_t slot, bool enabled) {
    // all 30 slots start disabled online send this before every 0x21
    Packet pkt(CMD::S_ROOM_SLOT_ENABLED);
    pkt.writeUInt32(slot);
    pkt.writeUInt32(enabled ? 1u : 0u);
    return pkt;
}

Packet PacketBuilder::roomMember(const RoomMemberWire& m) {
    // packed no alignment padding anywhere see sub 40D650
    Packet pkt(CMD::S_ROOM_MEMBER);

    pkt.writeUInt32(m.slot);
    pkt.writeUInt32(m.team);
    pkt.writeUInt32(m.playerId);
    writeWString(pkt, m.displayName);

    pkt.writeUInt8(m.levelIndex);
    pkt.writeUInt8(m.gmBadge);
    pkt.writeUInt8(m.pcCafe);
    pkt.writeUInt32(m.titleKey);   // no pad before this u32

    pkt.writeBytes(m.character.data(), m.character.size());
    pkt.writeBytes(m.kart.data(), m.kart.size());

    pkt.writeUInt32(m.readyState);
    pkt.writeUInt32(m.petBaseKey);
    pkt.writeBytes(m.customCar.data(), m.customCar.size());

    // wrong size here silently desyncs the whole stream loud is better
    const size_t expected = 187 + 2 * (m.displayName.size() + 1);
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "roomMember size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet PacketBuilder::broadcastPlayerData(const PlayerData& player) {
    // cmd 0x21 variable room full sub 4797C0

    Packet pkt(CMD::S_ROOM_FULL);

    pkt.writeInt32(player.id);
    pkt.writeInt32(player.level);
    pkt.writeInt32(static_cast<int32_t>(player.team));

    std::u16string wname(player.name.begin(), player.name.end());
    writeWString(pkt, wname);

    pkt.writeUInt8(player.ready ? 1 : 0);
    pkt.writeUInt8(player.isGM ? 1 : 0);
    pkt.writeUInt8(player.tutorialCompleted ? 1 : 0);
    pkt.writeInt32(0);

    VehicleData vd = {};
    vd.vehicleId  = player.vehicleTemplateId;
    vd.uniqueId   = player.vehicleId;
    vd.durability = 100;
    vd.maxDurability = 100;
    vd.statSpeed    = 50;
    vd.statAccel    = 50;
    vd.statHandling = 50;
    vd.statDrift    = 40;
    vd.statBoost    = 30;
    vd.statWeight   = 50;
    vd.statSpecial  = 0;
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&vd), sizeof(VehicleData));

    ItemData itm = {};
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&itm), sizeof(ItemData));

    pkt.writeInt32(static_cast<int32_t>(player.slot));
    pkt.writeInt32(0);

    for (int i = 0; i < 60; ++i) pkt.writeUInt8(0);

    return pkt;
}

Packet PacketBuilder::playerDisconnect(int32_t playerId) {
    // cmd 0x22 single int32 sub 479920
    Packet pkt(CMD::S_LEAVE_ROOM);
    pkt.writeInt32(playerId);
    return pkt;
}

Packet PacketBuilder::playerJoin(const PlayerData& player) {
    // cmd 0x3E variable size null terminated name sub 479D60

    Packet pkt(CMD::S_PLAYER_JOIN);

    pkt.writeInt32(player.id);

    for (char c : player.name) {
        pkt.writeUInt8(static_cast<uint8_t>(c));
        pkt.writeUInt8(0);
    }
    pkt.writeUInt16(0);

    pkt.writeInt32(player.level);

    pkt.writeInt32(static_cast<int32_t>(player.team));

    VehicleData vd = {};
    vd.vehicleId = player.vehicleTemplateId;
    vd.uniqueId = player.vehicleId;
    vd.durability = 100;
    vd.maxDurability = 100;
    vd.statSpeed = 50;
    vd.statAccel = 50;
    vd.statHandling = 50;
    vd.statDrift = 40;
    vd.statBoost = 30;
    vd.statWeight = 50;
    vd.statSpecial = 0;
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&vd), sizeof(VehicleData));

    ItemData itm = {};
    itm.itemId = 0;
    itm.uniqueId = 0;
    itm.quantity = 0;
    itm.slot = 0;
    itm.equipped = 0;
    itm.expiration = 0;
    itm.enhancement = 0;
    itm.bound = 0;
    memset(itm.reserved, 0, sizeof(itm.reserved));
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&itm), sizeof(ItemData));

    pkt.writeInt32(static_cast<int32_t>(player.slot));

    for (int i = 0; i < 60; ++i) {
        pkt.writeUInt8(0);
    }

    return pkt;
}

Packet PacketBuilder::playerLeft(int32_t playerId) {
    Packet pkt(CMD::S_PLAYER_LEFT);
    pkt.writeInt32(playerId);
    return pkt;
}

Packet PacketBuilder::playerUpdate(const PlayerData& player) {
    // cmd 0x23 two int32 sub 479BE0

    Packet pkt(CMD::S_PLAYER_UPDATE);
    pkt.writeInt32(player.id);
    int32_t flags = (player.slot & 0xFF) |
                    ((player.team & 0xFF) << 8) |
                    ((player.ready ? 1 : 0) << 16);
    pkt.writeInt32(flags);
    return pkt;
}

Packet PacketBuilder::roomState(int32_t masterPlayerId) {
    // cmd 0x30 single int32 sub 479760 not a state carries the room master id
    Packet pkt(CMD::S_ROOM_STATE);
    pkt.writeInt32(masterPlayerId);
    return pkt;
}

// game packets

Packet PacketBuilder::countdown(int32_t seconds) {
    // cmd 0x3B no client handler countdown is driven locally by 0x0D a bare trigger
    Packet pkt(CMD::S_COUNTDOWN);
    pkt.writeInt32(seconds);
    return pkt;
}

// pack vec3 into 8 bytes for 0x40 grid inverse of client sub 44E7F0
static void writePackedVec3(Packet& pkt, float x, float y, float z) {
    static const uint32_t SEL[8] = {1,8,7,2,6,3,4,5};
    auto part = [](float v, uint32_t& iv, uint32_t& fv){
        float a = v < 0.0f ? -v : v;
        iv = static_cast<uint32_t>(a);
        fv = static_cast<uint32_t>((a - static_cast<float>(iv)) * 100.0f + 0.5f);
        if (fv >= 100) { fv = 0; ++iv; }
        if (iv > 4095) iv = 4095;
    };
    uint32_t xi,xf,yi,yf,zi,zf;
    part(x,xi,xf); part(y,yi,yf); part(z,zi,zf);
    uint32_t key = (x < 0.0f ? 4u : 0u) | (y < 0.0f ? 2u : 0u) | (z < 0.0f ? 1u : 0u);
    uint32_t sel = SEL[key];
    uint32_t lo = (sel & 0xF) | ((zf & 0xFF) << 4) | ((zi & 0xFFF) << 12) | ((yf & 0xFF) << 24);
    uint32_t hi = (yi & 0xFFF) | ((xf & 0xFF) << 12) | ((xi & 0xFFF) << 20);
    pkt.writeInt32(static_cast<int32_t>(lo));
    pkt.writeInt32(static_cast<int32_t>(hi));
}

Packet PacketBuilder::startRaceGrid(const std::vector<RaceSpawn>& grid) {
    // cmd 0x40 int8 count then 25 bytes per player sub 47FD30 normal path
    Packet pkt(CMD::S_GAME_STATE_40);
    pkt.writeUInt8(static_cast<uint8_t>(grid.size()));
    for (const auto& g : grid) {
        pkt.writeInt32(g.playerId);
        pkt.writeInt16(0);
        writePackedVec3(pkt, g.x, g.y, g.z);
        writePackedVec3(pkt, g.rx, g.ry, g.rz);
        pkt.writeUInt8(0);
        pkt.writeInt16(0);
    }
    return pkt;
}

Packet PacketBuilder::motionRelay(int32_t senderId, const uint8_t packedPos[8],
                                  const uint8_t packedRot[8], uint8_t flag, int16_t field) {
    // cmd 0x40 per player 25 bytes remote 3D motion forward raw packed bytes prefixed by senderId
    Packet pkt(CMD::S_GAME_STATE_40);
    pkt.writeInt32(senderId);
    pkt.writeInt16(0);
    pkt.writeBytes(packedPos, 8);
    pkt.writeBytes(packedRot, 8);
    pkt.writeUInt8(flag);
    pkt.writeInt16(field);
    return pkt;
}

Packet PacketBuilder::raceStart() {
    // cmd 0x33 state 2 go all racers sub 40A040 first int32 ignored
    Packet pkt(CMD::S_GAME_STATE);
    pkt.writeInt32(0);
    pkt.writeInt32(2);
    return pkt;
}

Packet PacketBuilder::position(int32_t playerId, float x, float y, float z, float rot) {
    // cmd 0x31 three int32 client sends 2D only sub 479950
    (void)z; (void)rot;

    Packet pkt(CMD::S_POSITION);
    pkt.writeInt32(playerId);
    pkt.writeInt32(static_cast<int32_t>(x));
    pkt.writeInt32(static_cast<int32_t>(y));
    return pkt;
}

Packet PacketBuilder::position32(int32_t val1, int32_t val2) {
    // cmd 0x32 two int32 sub 479C70
    Packet pkt(CMD::S_POSITION_32);
    pkt.writeInt32(val1);
    pkt.writeInt32(val2);
    return pkt;
}

Packet PacketBuilder::flag34() {
    // cmd 0x34 sub 479A10
    Packet pkt(CMD::S_FLAG_34);
    return pkt;
}

Packet PacketBuilder::finish(int32_t playerId, int32_t rank, int32_t time) {
    // cmd 0x39 no payload use S RESULTS or S RACE STATUS for ranks sub 479CB0
    (void)playerId; (void)rank; (void)time;
    return Packet(CMD::S_FINISH);
}

Packet PacketBuilder::results(const std::vector<std::pair<int32_t, int32_t>>& rankings) {
    // cmd 0x3A single int32 sub 47AE00
    (void)rankings;

    Packet pkt(CMD::S_RESULTS);
    pkt.writeInt32(0);
    return pkt;
}

Packet PacketBuilder::raceEnd(int32_t playerId, int32_t goldTotal, int32_t cashTotal, bool won) {
    // cmd 0x3C 17 bytes sub 47A5C0 per recipient absolute totals
    Packet pkt(CMD::S_RACE_END);
    pkt.writeInt32(playerId);
    pkt.writeInt32(goldTotal);
    pkt.writeUInt8(0);
    pkt.writeInt32(cashTotal);
    pkt.writeInt32(won ? 1 : 0);
    return pkt;
}

// inventory packets

// one place builds the record bytes so 0x1B 0x1C and 0x21 can never disagree
static void putI32(uint8_t* p, size_t off, int32_t v) {
    p[off + 0] = static_cast<uint8_t>(v & 0xFF);
    p[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

std::array<uint8_t, 0x2C> PacketBuilder::characterRecord(int32_t instanceId, int32_t baseKey) {
    // baseKey is driver key 0xBF catalog lookup offset 0x08 equipped accessories zero means none
    std::array<uint8_t, 0x2C> r{};
    putI32(r.data(), 0x00, instanceId);
    putI32(r.data(), 0x04, baseKey);
    return r;
}

std::array<uint8_t, 0x38> PacketBuilder::kartRecord(const VehicleInfo& v) {
    // 0x08 paint 0x0C plate not durability 0x2C period mode 0x30 value zero paint or plate nulls sub 4510C0 stock build
    std::array<uint8_t, 0x38> r{};
    putI32(r.data(), 0x00, v.id != 0 ? v.id : v.templateId);
    putI32(r.data(), 0x04, v.templateId);                     // kart base key 0xC0 catalog lookup
    putI32(r.data(), 0x08, kRecordDefaultPaintKey);
    putI32(r.data(), 0x0C, kRecordDefaultPlateKey);
    // 0x10 kart item slot 8 key none 0x28 price key of the grant
    putI32(r.data(), 0x10, 0);
    putI32(r.data(), 0x28, 0);
    putI32(r.data(), 0x2C, 3);
    putI32(r.data(), 0x30, v.durability);                     // durability 0 to 500
    putI32(r.data(), 0x34, v.equipped ? 1 : 0);
    return r;
}

Packet PacketBuilder::driverInventory(const std::vector<int32_t>& driverIds) {
    // 0x1B routes to the driver inventory handler sub 478ec0 not a shifted vehicle opcode
    Packet pkt(CMD::S_INVENTORY_VEHICLES);
    pkt.writeInt32(static_cast<int32_t>(driverIds.size()));
    for (int32_t id : driverIds) {
        auto rec = characterRecord(id, id);
        pkt.writeBytes(rec.data(), rec.size());
    }
    return pkt;
}

Packet PacketBuilder::inventoryItems(const std::vector<ItemInfo>& items) {
    // cmd 0x1C ItemData 56 bytes each

    Packet pkt(CMD::S_INVENTORY_ITEMS);
    pkt.writeInt32(static_cast<int32_t>(items.size()));

    for (const auto& item : items) {
        pkt.writeInt32(item.id);
        pkt.writeInt32(item.templateId);
        pkt.writeInt32(item.quantity);
        pkt.writeInt32(item.slot);
        pkt.writeInt32(item.equipped ? 1 : 0);
        pkt.writeInt32(0);
        pkt.writeInt32(0);
        pkt.writeInt32(0);
        for (int i = 0; i < 6; ++i) {
            pkt.writeInt32(0);
        }
    }

    return pkt;
}

Packet PacketBuilder::inventoryAccessories(const std::vector<AccessoryInfo>& accessories) {
    // cmd 0x1D AccessoryData 28 bytes each

    Packet pkt(CMD::S_INVENTORY_ACCESSORY);
    pkt.writeInt32(static_cast<int32_t>(accessories.size()));

    for (const auto& acc : accessories) {
        pkt.writeInt32(acc.id);
        pkt.writeInt32(acc.templateId);
        pkt.writeInt32(acc.slot);
        pkt.writeInt32(acc.bonus1);
        pkt.writeInt32(acc.bonus2);
        pkt.writeInt32(acc.bonus3);
        pkt.writeInt32(acc.equipped ? 1 : 0);
    }

    return pkt;
}

// chat packets

Packet PacketBuilder::chatMessage(uint32_t senderId, const std::u16string& message) {
    // cmd 0x2D fixed 42 wchar message padded 116 bytes total

    Packet pkt(CMD::S_CHAT_MESSAGE);

    pkt.writeInt32(static_cast<int32_t>(senderId));

    for (size_t i = 0; i < 42; ++i) {
        pkt.writeUInt16(i < message.size() ? static_cast<uint16_t>(message[i]) : 0u);
    }

    for (int i = 0; i < 7; ++i) {
        pkt.writeInt32(0);
    }

    return pkt;
}

Packet PacketBuilder::lobbyRoomAdd(uint32_t roomId, const std::u16string& name,
                                   int32_t playerCount, int32_t maxPlayers,
                                   int32_t gameMode, int32_t hasPassword,
                                   int32_t playingFlag, int32_t timeLeftMs) {
    // 0x2D sub 479630 reads id wstring then seven int32 handed to sub 408360
    Packet pkt(CMD::S_LOBBY_ROOM_ADD);
    pkt.writeInt32(static_cast<int32_t>(roomId));
    // name is 42 wchars max 0x5C playerCount left of the %2d over %2d
    writeWString(pkt, name.substr(0, 41));
    pkt.writeInt32(playerCount);
    // 0x60 maxPlayers right of it 0x68 gameMode picks icon row skipped unless 0 to 4
    pkt.writeInt32(maxPlayers);
    pkt.writeInt32(gameMode);
    // 0x6C hasPassword one draws Lobby Room Lock 0x70 channel sub 408EB0 always 0
    pkt.writeInt32(hasPassword);
    pkt.writeInt32(0);
    // 0x74 playingFlag nonzero draws Lobby Room Play grey icon 0x78 timeLeftMs read only on channel 3
    pkt.writeInt32(playingFlag);
    pkt.writeInt32(timeLeftMs);
    return pkt;
}

Packet PacketBuilder::lobbyRoomRemove(uint32_t roomId) {
    Packet pkt(CMD::S_LOBBY_ROOM_REMOVE);
    pkt.writeInt32(static_cast<int32_t>(roomId));
    return pkt;
}

Packet PacketBuilder::lobbyRoomState(uint32_t roomId, int32_t state) {
    Packet pkt(CMD::S_LOBBY_ROOM_STATE);
    pkt.writeInt32(static_cast<int32_t>(roomId));
    pkt.writeInt32(state);
    return pkt;
}

Packet PacketBuilder::whisperEnable() {
    Packet pkt(CMD::S_WHISPER_ENABLE);
    return pkt;
}

Packet PacketBuilder::whisperDisable() {
    Packet pkt(CMD::S_WHISPER_DISABLE);
    return pkt;
}

Packet PacketBuilder::systemMessage(const std::u16string& message, int32_t type) {
    // cmd 0xB4 S SYSTEM MESSAGE handler sub 47CD60 wire is senderId senderName message chatType
    Packet pkt(CMD::S_SYSTEM_MESSAGE);
    pkt.writeInt32(0);
    writeWString(pkt, u"");
    writeWString(pkt, message);
    pkt.writeInt32(type);
    return pkt;
}

Packet PacketBuilder::lobbyChatBroadcast(int32_t senderId,
                                        const std::u16string& senderName,
                                        const std::u16string& message,
                                        int32_t chatType) {
    Packet pkt(CMD::S_SYSTEM_MESSAGE);
    pkt.writeInt32(senderId);
    // sub 47CD60 lands the sender name in wchar t 14 unbounded copy 13 chars max
    writeWString(pkt, senderName.substr(0, 13));
    writeWString(pkt, message);
    pkt.writeInt32(chatType);
    return pkt;
}

Packet PacketBuilder::displayText(const std::u16string& text, int32_t param) {
    // cmd 0xB6 sub 47AC60
    Packet pkt(CMD::S_DISPLAY_TEXT);
    writeWString(pkt, text);
    pkt.writeInt32(param);
    return pkt;
}

// redirect

Packet PacketBuilder::serverRedirect(const std::string& ip, int32_t port) {
    Packet pkt(CMD::S_SERVER_REDIRECT);
    pkt.writeInt32(0);
    pkt.writeString(ip);
    pkt.writeInt32(port);
    return pkt;
}

// race packets

Packet PacketBuilder::gameState40(uint8_t state) {
    // cmd 0x40 single byte sub 47FD30
    Packet pkt(CMD::S_GAME_STATE_40);
    pkt.writeUInt8(state);
    return pkt;
}

Packet PacketBuilder::gameMode(uint8_t mode) {
    // cmd 0x42
    Packet pkt(CMD::S_GAME_MODE);
    pkt.writeInt32(mode);
    return pkt;
}

Packet PacketBuilder::gameMode14(int32_t mode) {
    // cmd 0x14 mode 3 or 8 adds 5 int32 ui state 11 sub 479CC0

    Packet pkt(CMD::S_GAME_MODE_14);
    pkt.writeInt32(mode);

    if (mode == 3 || mode == 8) {
        pkt.writeInt32(0);
        pkt.writeInt32(0);
        pkt.writeInt32(0);
        pkt.writeInt32(0);
        pkt.writeInt32(3);
    }

    return pkt;
}

Packet PacketBuilder::lapInfo(int32_t playerId, uint8_t lap, int32_t lapTime) {
    // cmd 0x35 only 8 bytes lapTime ignored sub 479C20
    Packet pkt(CMD::S_SCORE);
    pkt.writeInt32(playerId);
    pkt.writeInt32(lap);
    (void)lapTime;
    return pkt;
}

Packet PacketBuilder::score(int32_t playerId, int32_t score) {
    Packet pkt(CMD::S_SCORE);
    pkt.writeInt32(playerId);
    pkt.writeInt32(score);
    return pkt;
}

Packet PacketBuilder::playerStatus(int32_t playerId, uint8_t status) {
    Packet pkt(CMD::S_PLAYER_STATUS);
    pkt.writeInt32(playerId);
    pkt.writeUInt8(status);
    return pkt;
}

Packet PacketBuilder::raceStatus(int32_t playerId, uint8_t position, int32_t time) {
    // cmd 0x57 three int32 12 bytes sub 47AB40
    Packet pkt(CMD::S_RACE_STATUS);
    pkt.writeInt32(playerId);
    pkt.writeInt32(static_cast<int32_t>(position));
    pkt.writeInt32(time);
    return pkt;
}

Packet PacketBuilder::resultsScoreboard(const std::vector<ResultRow>& rows) {
    // cmd 0x46 scoreboard proven rows sub 47A760

    Packet pkt(CMD::S_LARGE_GAME_STATE);

    // header hdr then count
    pkt.writeInt32(0);
    pkt.writeInt32(static_cast<int32_t>(rows.size()));

    for (const auto& r : rows) {
        // proven rank playerId statA unused
        pkt.writeInt32(r.rank);
        pkt.writeInt32(0);
        pkt.writeInt32(r.playerId);

        // proven name cap twelve wchar
        std::u16string nm = r.name;
        if (nm.size() > 12) nm.resize(12);
        writeWString(pkt, nm);

        pkt.writeUInt8(r.team);          // flagA team

        // TODO confirm exact result column map unproven
        pkt.writeInt32(r.finishTime);
        pkt.writeInt32(r.rewardGold);
        pkt.writeInt32(r.rewardXp);
        pkt.writeInt32(r.rewardGold + r.rewardXp);
        pkt.writeInt32(0);

        pkt.writeUInt8(0);               // flagB

        // statG statH statI
        pkt.writeInt32(0);
        pkt.writeInt32(0);
        pkt.writeInt32(0);
    }

    return pkt;
}

Packet PacketBuilder::gameUpdate(int32_t value) {
    // cmd 0x44
    Packet pkt(CMD::S_GAME_UPDATE);
    pkt.writeInt32(value);
    return pkt;
}

Packet PacketBuilder::playerData(int32_t playerId, int32_t val1, int32_t val2) {
    // cmd 0x49
    Packet pkt(CMD::S_PLAYER_DATA);
    pkt.writeInt32(playerId);
    pkt.writeInt32(val1);
    pkt.writeInt32(val2);
    return pkt;
}

Packet PacketBuilder::gameData4B(int32_t playerId, int32_t type, int32_t param1, int32_t param2) {
    // cmd 0x4B four int32 sub 47A460
    Packet pkt(CMD::S_GAME_DATA_4B);
    pkt.writeInt32(playerId);
    pkt.writeInt32(type);
    pkt.writeInt32(param1);
    pkt.writeInt32(param2);
    return pkt;
}

Packet PacketBuilder::timestamp() {
    // cmd 0x4E
    Packet pkt(CMD::S_TIMESTAMP);
    return pkt;
}

Packet PacketBuilder::roomStatus(int32_t roomId, uint8_t status) {
    // cmd 0x64 two int32 sub 47ACD0
    Packet pkt(CMD::S_ROOM_STATUS);
    pkt.writeInt32(roomId);
    pkt.writeInt32(static_cast<int32_t>(status));
    return pkt;
}

Packet PacketBuilder::speedUpdate(int32_t playerId, float speed) {
    // cmd 0x65 int32 plus float sub 47AD60
    Packet pkt(CMD::S_SPEED_UPDATE);
    pkt.writeInt32(playerId);
    pkt.writeFloat(speed);
    return pkt;
}

Packet PacketBuilder::roomData5C(int32_t playerId, int32_t val1, int32_t val2) {
    // cmd 0x5C three int32 sub 47A500
    Packet pkt(CMD::S_ROOM_DATA_5C);
    pkt.writeInt32(playerId);
    pkt.writeInt32(val1);
    pkt.writeInt32(val2);
    return pkt;
}

Packet PacketBuilder::game5F(int32_t playerId, int32_t value) {
    // cmd 0x5F two int32 sub 47EE70
    Packet pkt(CMD::S_GAME_5F);
    pkt.writeInt32(playerId);
    pkt.writeInt32(value);
    return pkt;
}

// shop packets

Packet PacketBuilder::shopLookup(int32_t playerId) {
    // cmd 0x68 single int32 sub 47AE30

    Packet pkt(CMD::S_SHOP_LOOKUP);
    pkt.writeInt32(playerId);
    return pkt;
}

Packet PacketBuilder::shopItem(int32_t itemId, int32_t price, int32_t currency) {
    Packet pkt(CMD::S_SHOP_ITEM);
    pkt.writeInt32(itemId);
    pkt.writeInt16(static_cast<int16_t>(price));
    pkt.writeUInt8(static_cast<uint8_t>(currency));
    return pkt;
}

Packet PacketBuilder::shopItemList(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& items) {
    // legacy helper prefer shopLookup and shopItem separately
    Packet pkt(CMD::S_SHOP_LOOKUP);
    pkt.writeInt32(static_cast<int32_t>(items.size()));
    return pkt;
}

Packet PacketBuilder::shopPurchaseVehicle(const VehicleInfo& vehicle) {
    // cmd 0x6F result one plus VehicleData sub 47B3C0
    Packet pkt(CMD::S_SHOP_RESPONSE);
    pkt.writeInt32(1);

    pkt.writeInt32(vehicle.id);
    pkt.writeInt32(vehicle.templateId);
    pkt.writeInt32(vehicle.durability);
    pkt.writeInt32(vehicle.maxDurability);
    for (int i = 0; i < 7; ++i) {
        pkt.writeInt32(vehicle.stats[i]);
    }
    return pkt;
}

Packet PacketBuilder::shopPurchaseSmallItem(const uint8_t* itemData) {
    // cmd 0x6F result twelve plus 36 bytes sub 47B3C0
    Packet pkt(CMD::S_SHOP_RESPONSE);
    pkt.writeInt32(12);

    constexpr size_t ITEM_SIZE = 36;
    for (size_t i = 0; i < ITEM_SIZE; ++i) {
        if (itemData) {
            pkt.writeUInt8(itemData[i]);
        } else {
            pkt.writeUInt8(0);
        }
    }
    return pkt;
}

Packet PacketBuilder::shopPurchaseError(int32_t errorCode) {
    // cmd 0x6F error code sub 47B3C0
    Packet pkt(CMD::S_SHOP_RESPONSE);
    pkt.writeInt32(errorCode);
    return pkt;
}

Packet PacketBuilder::shopUpdate(int32_t gold, int32_t cash) {
    Packet pkt(CMD::S_SHOP_UPDATE);
    pkt.writeInt32(gold);
    pkt.writeInt16(static_cast<int16_t>(cash));
    return pkt;
}

Packet PacketBuilder::invitePopupShort(int32_t count, const std::u16string& name,
                                       int32_t id, const std::u16string& message) {
    // sub 47ED90 reads count first and stops when not positive zero clears the popup
    Packet pkt = Packet::fromCmdFull(CMD::S_INVITE_POPUP_SHORT);
    pkt.writeInt32(count);
    if (count > 0) {
        writeWString(pkt, name);
        pkt.writeInt32(id);
        pkt.writeInt32(message.empty() ? 0 : 1);
        if (!message.empty()) writeWString(pkt, message);
    }
    return pkt;
}

Packet PacketBuilder::invitePopup(int32_t senderId, const std::u16string& senderName,
                               const std::string& message, int32_t param1, int32_t param2,
                               const std::u16string& extra, uint8_t flag) {
    // cmd 0x6C sub 47B030 the invite popup not shop chat

    Packet pkt(CMD::S_INVITE_POPUP);
    pkt.writeInt32(senderId);
    writeWString(pkt, senderName);
    pkt.writeString(message);
    pkt.writeInt32(param1);
    pkt.writeInt32(param2);
    writeWString(pkt, extra);
    pkt.writeUInt8(flag);
    return pkt;
}

Packet PacketBuilder::shopCall() {
    // cmd 0x6E sub 47B190
    Packet pkt(CMD::S_SHOP_CALL);
    return pkt;
}

Packet PacketBuilder::shopEvent(int32_t eventId, int32_t param) {
    // cmd 0x70 two int32 sub 47B300
    Packet pkt(CMD::S_SHOP_EVENT);
    pkt.writeInt32(eventId);
    pkt.writeInt32(param);
    return pkt;
}

// shop catalog server pushed on enter

static void writeFixedWName(Packet& pkt, const std::u16string& name, size_t byteSize) {
    // fixed width utf16le name zero pad keep last cell null
    const size_t cells = byteSize / 2;
    size_t written = 0;
    for (size_t i = 0; i < name.size() && written + 1 < cells; ++i) {
        pkt.writeUInt16(static_cast<uint16_t>(name[i]));
        ++written;
    }
    while (written < cells) {
        pkt.writeUInt16(0);
        ++written;
    }
}

Packet PacketBuilder::shopKartCatalog(const std::vector<ShopCatalogRow>& rows) {
    // cmd 0x76 sub 47B1B0 kart tab1 44 bytes id name32 price stock
    Packet pkt(CMD::S_INVENTORY_LIST);
    pkt.writeInt32(static_cast<int32_t>(rows.size()));
    for (const auto& r : rows) {
        // 0x00 id 0x04 name 32 bytes both proven
        pkt.writeInt32(r.itemId);
        writeFixedWName(pkt, r.name, 32);
        // 0x24 price 0x28 stock both inferred
        pkt.writeInt32(r.price);
        pkt.writeInt32(r.stock);
    }
    return pkt;
}

Packet PacketBuilder::shopItemCatalog(const std::vector<ShopCatalogRow>& rows) {
    // cmd 0x78 sub 47B220 item tab5 36 bytes id name rest zero
    Packet pkt(CMD::S_ITEM_LIST);
    pkt.writeInt32(static_cast<int32_t>(rows.size()));
    for (const auto& r : rows) {
        // 0x00 id proven 0x04 name inferred 32 bytes
        pkt.writeInt32(r.itemId);
        writeFixedWName(pkt, r.name, 32);
    }
    return pkt;
}

Packet PacketBuilder::shopPremiumCatalog(const std::vector<ShopCatalogRow>& rows) {
    // cmd 0x79 sub 47B290 premium tab4 32 bytes small item id name28
    Packet pkt(CMD::S_ITEM_LIST_B);
    pkt.writeInt32(static_cast<int32_t>(rows.size()));
    for (const auto& r : rows) {
        // 0x00 itemId 0x04 name 28 bytes both proven
        pkt.writeInt32(r.itemId);
        writeFixedWName(pkt, r.name, 28);
    }
    return pkt;
}

Packet PacketBuilder::shopPriceStock(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& rows) {
    // cmd 0x73 sub 47B570 reset all kart rows to negative 2 then mark these ids
    Packet pkt(CMD::S_SLOT_UPDATE);
    pkt.writeInt32(static_cast<int32_t>(rows.size()));
    for (const auto& [id, price, stock] : rows) {
        // 0x00 itemId proven 0x04 price 0x08 stock both inferred
        pkt.writeInt32(id);
        pkt.writeInt32(price);
        pkt.writeInt32(stock);
    }
    return pkt;
}

// inventory extended

Packet PacketBuilder::addVehicle(const VehicleInfo& v) {
    // cmd 0x9D uniqueId first then templateId
    Packet pkt(CMD::S_ADD_VEHICLE);
    pkt.writeInt32(v.id);
    pkt.writeInt32(v.templateId);
    pkt.writeInt32(v.durability);
    pkt.writeInt32(v.maxDurability);
    for (int i = 0; i < 7; ++i) {
        pkt.writeInt32(v.stats[i]);
    }
    return pkt;
}

Packet PacketBuilder::addItem(const ItemInfo& item) {
    // cmd 0x9E uniqueId first then templateId
    Packet pkt(CMD::S_ADD_ITEM);
    pkt.writeInt32(item.id);
    pkt.writeInt32(item.templateId);
    pkt.writeInt32(item.quantity);
    pkt.writeInt32(item.slot);
    pkt.writeInt32(item.equipped ? 1 : 0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    for (int i = 0; i < 6; ++i) pkt.writeInt32(0);
    return pkt;
}

Packet PacketBuilder::addAccessory(const AccessoryInfo& acc) {
    // cmd 0x9F AccessoryData 28 bytes
    Packet pkt(CMD::S_ADD_ACCESSORY);
    pkt.writeInt32(acc.id);
    pkt.writeInt32(acc.templateId);
    pkt.writeInt32(acc.slot);
    pkt.writeInt32(acc.bonus1);
    pkt.writeInt32(acc.bonus2);
    pkt.writeInt32(acc.bonus3);
    pkt.writeInt32(acc.equipped ? 1 : 0);
    return pkt;
}

Packet PacketBuilder::dataBlock(const uint8_t* data, size_t len) {
    // cmd 0x72 fixed 104 bytes sub 47B550
    Packet pkt(CMD::S_DATA_BLOCK);

    constexpr size_t BLOCK_SIZE = 104;
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        if (data && i < len) {
            pkt.writeUInt8(data[i]);
        } else {
            pkt.writeUInt8(0);
        }
    }
    return pkt;
}

Packet PacketBuilder::entityData(const uint8_t* data, size_t len) {
    // cmd 0x28 fixed 104 bytes sub 479B20
    Packet pkt(CMD::S_ENTITY_DATA);

    constexpr size_t BLOCK_SIZE = 104;
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        if (data && i < len) {
            pkt.writeUInt8(data[i]);
        } else {
            pkt.writeUInt8(0);
        }
    }
    return pkt;
}

Packet PacketBuilder::tutorialFail() {
    // cmd 0x62 sub 479580
    Packet pkt(CMD::S_TUTORIAL_FAIL);
    return pkt;
}

Packet PacketBuilder::slotUpdate(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& slots) {
    // cmd 0x73 count plus 12 bytes per slot sub 47B570
    Packet pkt(CMD::S_SLOT_UPDATE);
    pkt.writeInt32(static_cast<int32_t>(slots.size()));

    for (const auto& [slotId, val1, val2] : slots) {
        pkt.writeInt32(slotId);
        pkt.writeInt32(val1);
        pkt.writeInt32(val2);
    }
    return pkt;
}

Packet PacketBuilder::shopPlayerUpdate(int32_t playerId, int16_t value, uint8_t flag) {
    // cmd 0x69 int32 int16 int8 sub 47AF00
    Packet pkt(CMD::S_SHOP_ITEM);
    pkt.writeInt32(playerId);
    pkt.writeInt16(value);
    pkt.writeUInt8(flag);
    return pkt;
}

Packet PacketBuilder::shopPlayerValue(int32_t playerId, int16_t value) {
    // cmd 0x6A int32 int16 sub 47AFE0
    Packet pkt(CMD::S_SHOP_UPDATE);
    pkt.writeInt32(playerId);
    pkt.writeInt16(value);
    return pkt;
}

Packet PacketBuilder::missionComplete(int32_t missionId) {
    // cmd 0xA1 single int32 sub 47C930
    Packet pkt(CMD::S_MISSION_COMPLETE);
    pkt.writeInt32(missionId);
    return pkt;
}

Packet PacketBuilder::missionList(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& missions) {
    // cmd 0xA2 count plus 12 bytes per mission sub 47C960
    Packet pkt(CMD::S_MISSION_LIST);
    pkt.writeInt32(static_cast<int32_t>(missions.size()));

    for (const auto& [id, progress, target] : missions) {
        pkt.writeInt32(id);
        pkt.writeInt32(progress);
        pkt.writeInt32(target);
    }
    return pkt;
}

Packet PacketBuilder::rewardClaim(int32_t flag1, int32_t flag2, int32_t rewardId,
                                  int32_t rewardAmount, int32_t rewardType) {
    // cmd 0xA3 simplified base form sub 47C3C0
    Packet pkt(CMD::S_REWARD_CLAIM);
    pkt.writeInt32(flag1);
    pkt.writeInt32(flag2);
    pkt.writeInt32(rewardId);
    pkt.writeInt32(rewardAmount);
    pkt.writeInt32(rewardType);
    return pkt;
}

Packet PacketBuilder::removeItem(int32_t type, int32_t itemId) {
    // cmd 0xB8 two int32 sub 484690
    Packet pkt(CMD::S_REMOVE_ITEM);
    pkt.writeInt32(type);
    pkt.writeInt32(itemId);
    return pkt;
}

Packet PacketBuilder::equipItem(int32_t itemId, int32_t slot, bool equipped) {
    // cmd 0x8C unequip only use equipVehicle equipItemFull equipAccessory for equip sub 47B9E0

    Packet pkt(CMD::S_EQUIP_ITEM);
    pkt.writeInt32(equipped ? 1 : 0);
    pkt.writeInt32(slot);
    pkt.writeInt32(itemId);
    return pkt;
}

Packet PacketBuilder::equipVehicle(int32_t slotType, int32_t vehicleUniqueId,
                                   int32_t goldChange, int32_t cashChange,
                                   const VehicleInfo& vehicle) {
    // cmd 0x8C equip vehicle type 0
    Packet pkt(CMD::S_EQUIP_ITEM);
    pkt.writeInt32(1);
    pkt.writeInt32(slotType);
    pkt.writeInt32(vehicleUniqueId);
    pkt.writeInt32(goldChange);
    pkt.writeInt32(cashChange);

    pkt.writeInt32(vehicle.id);
    pkt.writeInt32(vehicle.templateId);
    pkt.writeInt32(vehicle.durability);
    pkt.writeInt32(vehicle.maxDurability);
    for (int i = 0; i < 7; ++i) pkt.writeInt32(vehicle.stats[i]);
    return pkt;
}

Packet PacketBuilder::equipItemFull(int32_t slotType, int32_t itemUniqueId,
                                    int32_t goldChange, int32_t cashChange,
                                    const ItemInfo& item) {
    // cmd 0x8C equip item type 1
    Packet pkt(CMD::S_EQUIP_ITEM);
    pkt.writeInt32(1);
    pkt.writeInt32(slotType);
    pkt.writeInt32(itemUniqueId);
    pkt.writeInt32(goldChange);
    pkt.writeInt32(cashChange);

    pkt.writeInt32(item.id);
    pkt.writeInt32(item.templateId);
    pkt.writeInt32(item.quantity);
    pkt.writeInt32(item.slot);
    pkt.writeInt32(item.equipped ? 1 : 0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    for (int i = 0; i < 6; ++i) pkt.writeInt32(0);
    return pkt;
}

Packet PacketBuilder::equipAccessoryFull(int32_t slotType, int32_t accUniqueId,
                                         int32_t goldChange, int32_t cashChange,
                                         const AccessoryInfo& acc) {
    // cmd 0x8C equip accessory type 2 3 4
    Packet pkt(CMD::S_EQUIP_ITEM);
    pkt.writeInt32(1);
    pkt.writeInt32(slotType);
    pkt.writeInt32(accUniqueId);
    pkt.writeInt32(goldChange);
    pkt.writeInt32(cashChange);

    pkt.writeInt32(acc.id);
    pkt.writeInt32(acc.templateId);
    pkt.writeInt32(acc.slot);
    pkt.writeInt32(acc.bonus1);
    pkt.writeInt32(acc.bonus2);
    pkt.writeInt32(acc.bonus3);
    pkt.writeInt32(acc.equipped ? 1 : 0);
    return pkt;
}

Packet PacketBuilder::itemUpdate(const ItemInfo& item) {
    Packet pkt(CMD::S_ITEM_UPDATE);
    pkt.writeInt32(item.id);
    pkt.writeInt32(item.templateId);
    pkt.writeInt32(item.quantity);
    pkt.writeInt32(item.slot);
    for (int i = 0; i < 3; ++i) pkt.writeInt32(0);
    return pkt;
}

Packet PacketBuilder::notification(const std::u16string& msg, int32_t type) {
    // cmd 0x7D type then wstring sub 47B6B0
    Packet pkt(CMD::S_NOTIFICATION);
    pkt.writeInt32(type);
    writeWString(pkt, msg);
    return pkt;
}

Packet PacketBuilder::playerComparison(const uint8_t* player1Data, const uint8_t* player2Data,
                                       const std::u16string& nickname) {
    // cmd 0xB5 garage visit sub 47CF80 legacy name kept
    Packet pkt(CMD::S_PLAYER_COMPARISON);

    constexpr size_t PLAYER_INFO_SIZE = 0x4C8;
    for (size_t i = 0; i < PLAYER_INFO_SIZE; ++i) {
        if (player1Data) {
            pkt.writeUInt8(player1Data[i]);
        } else {
            pkt.writeUInt8(0);
        }
    }

    for (size_t i = 0; i < PLAYER_INFO_SIZE; ++i) {
        if (player2Data) {
            pkt.writeUInt8(player2Data[i]);
        } else {
            pkt.writeUInt8(0);
        }
    }

    writeWString(pkt, nickname);

    return pkt;
}

// inventory updates

Packet PacketBuilder::inventoryUpdateVehicle(int32_t goldChange, int32_t cashChange,
                                             const VehicleInfo& vehicle) {
    // cmd 0xB7 type 0 vehicle sub 484F50
    Packet pkt(CMD::S_INVENTORY_UPDATE);
    pkt.writeInt32(0);
    pkt.writeInt32(goldChange);
    pkt.writeInt32(cashChange);

    pkt.writeInt32(vehicle.id);
    pkt.writeInt32(vehicle.templateId);
    pkt.writeInt32(vehicle.durability);
    pkt.writeInt32(vehicle.maxDurability);
    pkt.writeInt32(vehicle.stats[0]);
    pkt.writeInt32(vehicle.stats[1]);
    pkt.writeInt32(vehicle.stats[2]);
    pkt.writeInt32(vehicle.stats[3]);
    pkt.writeInt32(vehicle.stats[4]);
    pkt.writeInt32(vehicle.stats[5]);
    pkt.writeInt32(vehicle.stats[6]);

    return pkt;
}

Packet PacketBuilder::inventoryUpdateItem(int32_t goldChange, int32_t cashChange,
                                          const ItemInfo& item) {
    // cmd 0xB7 type 1 item sub 484F50
    Packet pkt(CMD::S_INVENTORY_UPDATE);
    pkt.writeInt32(1);
    pkt.writeInt32(goldChange);
    pkt.writeInt32(cashChange);

    pkt.writeInt32(item.id);
    pkt.writeInt32(item.templateId);
    pkt.writeInt32(item.quantity);
    pkt.writeInt32(item.slot);
    pkt.writeInt32(item.equipped ? 1 : 0);
    for (int i = 0; i < 9; ++i) pkt.writeInt32(0);

    return pkt;
}

Packet PacketBuilder::inventoryUpdateAccessory(int32_t type, int32_t goldChange, int32_t cashChange,
                                               const AccessoryInfo& accessory) {
    // cmd 0xB7 type 2 or 3 accessory sub 484F50
    Packet pkt(CMD::S_INVENTORY_UPDATE);
    pkt.writeInt32(type);
    pkt.writeInt32(goldChange);
    pkt.writeInt32(cashChange);

    pkt.writeInt32(accessory.id);
    pkt.writeInt32(accessory.templateId);
    pkt.writeInt32(accessory.slot);
    pkt.writeInt32(accessory.bonus1);
    pkt.writeInt32(accessory.bonus2);
    pkt.writeInt32(accessory.bonus3);
    pkt.writeInt32(accessory.equipped ? 1 : 0);

    return pkt;
}

Packet PacketBuilder::inventoryRemove(int32_t type, int32_t uniqueId) {
    // cmd 0xB8 two int32 sub 484690
    Packet pkt(CMD::S_INVENTORY_REMOVE);
    pkt.writeInt32(type);
    pkt.writeInt32(uniqueId);
    return pkt;
}

Packet PacketBuilder::inventorySlotUpdate(int32_t type, const uint8_t* data, size_t len) {
    // cmd 0xB9 sub 484770
    Packet pkt(CMD::S_INVENTORY_SLOT);
    pkt.writeInt32(type);
    if (data && len > 0) {
        pkt.writeBytes(data, len);
    }
    return pkt;
}

Packet PacketBuilder::inventoryOperation(int32_t operationType, const uint8_t* data, size_t len) {
    // cmd 0xBA sub 484B10
    Packet pkt(CMD::S_INVENTORY_OP);
    pkt.writeInt32(operationType);
    if (data && len > 0) {
        pkt.writeBytes(data, len);
    }
    return pkt;
}

// game messages

Packet PacketBuilder::gameMessage(const std::string& msgKey, const std::u16string& name, int32_t param) {
    // cmd 0xCE ascii key plus wstring plus int32 sub 47D250
    Packet pkt(CMD::S_GAME_MESSAGE);

    for (char c : msgKey) {
        pkt.writeUInt8(static_cast<uint8_t>(c));
    }
    pkt.writeUInt8(0);

    // sub 47D250 reads this wstr into wchar t 14 with no bound past 13 chars overwrites the frame
    writeWString(pkt, name.substr(0, 13));

    pkt.writeInt32(param);

    return pkt;
}

Packet PacketBuilder::msgLevelUp(const std::u16string& playerName) {
    return gameMessage("MSG_LEVEL_UP", playerName, 0);
}

Packet PacketBuilder::msgComeInFirst(const std::u16string& playerName, int32_t rank) {
    return gameMessage("MSG_COME_IN_FIRST", playerName, rank);
}

Packet PacketBuilder::msgComeInSecond(const std::u16string& playerName, int32_t rank) {
    return gameMessage("MSG_COME_IN_SECOND", playerName, rank);
}

Packet PacketBuilder::msgComeInThird(const std::u16string& playerName, int32_t rank) {
    return gameMessage("MSG_COME_IN_THIRD", playerName, rank);
}

Packet PacketBuilder::msgComeInOther(const std::u16string& playerName, int32_t rank) {
    return gameMessage("MSG_COME_IN_OTHER", playerName, rank);
}

Packet PacketBuilder::msgBecomeOwner(const std::u16string& playerName) {
    return gameMessage("MSG_BECOME_OWNER", playerName, 0);
}

Packet PacketBuilder::msgLeftRoom(const std::u16string& playerName) {
    return gameMessage("MSG_LEFT_ROOM", playerName, 0);
}

Packet PacketBuilder::msgDurabilityScroll(const std::u16string& playerName) {
    return gameMessage("MSG_DURABILITY_SCROLL", playerName, 0);
}

// additional packets

Packet PacketBuilder::gameState(int32_t playerId, int32_t state) {
    // cmd 0x33 two int32 state 2 sets flag sub 4799B0
    Packet pkt(CMD::S_GAME_STATE);
    pkt.writeInt32(playerId);
    pkt.writeInt32(state);
    return pkt;
}

Packet PacketBuilder::playerAction(int32_t playerId, int32_t type,
                                   float x, float y, float z, float extra) {
    // cmd 0x47 six values 24 bytes sub 47A110
    Packet pkt(CMD::S_PLAYER_ACTION);
    pkt.writeInt32(playerId);
    pkt.writeInt32(type);
    pkt.writeFloat(x);
    pkt.writeFloat(y);
    pkt.writeFloat(z);
    pkt.writeFloat(extra);
    return pkt;
}

Packet PacketBuilder::gift(int32_t senderId, int32_t receiverId, const uint8_t* giftData) {
    // cmd 0x98 sender plus receiver plus 212 bytes sub 47BEF0
    Packet pkt(CMD::S_GIFT);
    pkt.writeInt32(senderId);
    pkt.writeInt32(receiverId);

    if (giftData) {
        pkt.writeBytes(giftData, 212);
    } else {
        for (int i = 0; i < 212; ++i) pkt.writeUInt8(0);
    }

    return pkt;
}

Packet PacketBuilder::itemSwitchVehicle(int32_t slotId, const VehicleInfo& vehicle) {
    // cmd 0x9A type 0 vehicle sub 47BF80
    Packet pkt(CMD::S_ITEM_SWITCH);
    pkt.writeInt32(slotId);
    pkt.writeInt32(0);

    pkt.writeInt32(vehicle.id);
    pkt.writeInt32(vehicle.templateId);
    pkt.writeInt32(vehicle.durability);
    pkt.writeInt32(vehicle.maxDurability);
    pkt.writeInt32(vehicle.stats[0]);
    pkt.writeInt32(vehicle.stats[1]);
    pkt.writeInt32(vehicle.stats[2]);
    pkt.writeInt32(vehicle.stats[3]);
    pkt.writeInt32(vehicle.stats[4]);
    pkt.writeInt32(vehicle.stats[5]);
    pkt.writeInt32(vehicle.stats[6]);

    return pkt;
}

Packet PacketBuilder::itemSwitchItem(int32_t slotId, const ItemInfo& item) {
    // cmd 0x9A type 1 item sub 47BF80
    Packet pkt(CMD::S_ITEM_SWITCH);
    pkt.writeInt32(slotId);
    pkt.writeInt32(1);

    pkt.writeInt32(item.id);
    pkt.writeInt32(item.templateId);
    pkt.writeInt32(item.quantity);
    pkt.writeInt32(item.slot);
    pkt.writeInt32(item.equipped ? 1 : 0);
    for (int i = 0; i < 9; ++i) pkt.writeInt32(0);

    return pkt;
}

Packet PacketBuilder::itemSwitchAccessory(int32_t slotId, const AccessoryInfo& accessory) {
    // cmd 0x9A type 2 accessory sub 47BF80
    Packet pkt(CMD::S_ITEM_SWITCH);
    pkt.writeInt32(slotId);
    pkt.writeInt32(2);

    pkt.writeInt32(accessory.id);
    pkt.writeInt32(accessory.templateId);
    pkt.writeInt32(accessory.slot);
    pkt.writeInt32(accessory.bonus1);
    pkt.writeInt32(accessory.bonus2);
    pkt.writeInt32(accessory.bonus3);
    pkt.writeInt32(accessory.equipped ? 1 : 0);

    return pkt;
}

Packet PacketBuilder::playerPreview(int32_t playerId, const std::u16string& name,
                                    int32_t level, int32_t rank,
                                    const VehicleInfo& vehicle, const ItemInfo& item) {
    // cmd 0xAA gacha roll result popup legacy name sub 47CBA0
    Packet pkt(CMD::S_PLAYER_PREVIEW);

    pkt.writeInt32(playerId);
    writeWString(pkt, name);
    pkt.writeInt32(level);
    pkt.writeInt32(rank);

    pkt.writeInt32(vehicle.id);
    pkt.writeInt32(vehicle.templateId);
    pkt.writeInt32(vehicle.durability);
    pkt.writeInt32(vehicle.maxDurability);
    pkt.writeInt32(vehicle.stats[0]);
    pkt.writeInt32(vehicle.stats[1]);
    pkt.writeInt32(vehicle.stats[2]);
    pkt.writeInt32(vehicle.stats[3]);
    pkt.writeInt32(vehicle.stats[4]);
    pkt.writeInt32(vehicle.stats[5]);
    pkt.writeInt32(vehicle.stats[6]);

    pkt.writeInt32(item.id);
    pkt.writeInt32(item.templateId);
    pkt.writeInt32(item.quantity);
    pkt.writeInt32(item.slot);
    pkt.writeInt32(item.equipped ? 1 : 0);
    for (int i = 0; i < 9; ++i) pkt.writeInt32(0);

    return pkt;
}

Packet PacketBuilder::playerStatsUpdate(int32_t playerId, int32_t stat1, int32_t stat2, int32_t stat3) {
    // cmd 0xCF four int32 sub 47D4A0
    Packet pkt(CMD::S_PLAYER_UPDATE_CF);
    pkt.writeInt32(playerId);
    pkt.writeInt32(stat1);
    pkt.writeInt32(stat2);
    pkt.writeInt32(stat3);
    return pkt;
}

Packet PacketBuilder::playerFullUpdate(int32_t playerId, const VehicleInfo& vehicle,
                                       const ItemInfo& item, const uint8_t* extraData) {
    // cmd 0xD9 playerId plus VehicleData plus ItemData plus 60 bytes sub 47D540
    Packet pkt(CMD::S_PLAYER_FULL_UPDATE);

    pkt.writeInt32(playerId);

    pkt.writeInt32(vehicle.id);
    pkt.writeInt32(vehicle.templateId);
    pkt.writeInt32(vehicle.durability);
    pkt.writeInt32(vehicle.maxDurability);
    pkt.writeInt32(vehicle.stats[0]);
    pkt.writeInt32(vehicle.stats[1]);
    pkt.writeInt32(vehicle.stats[2]);
    pkt.writeInt32(vehicle.stats[3]);
    pkt.writeInt32(vehicle.stats[4]);
    pkt.writeInt32(vehicle.stats[5]);
    pkt.writeInt32(vehicle.stats[6]);

    pkt.writeInt32(item.id);
    pkt.writeInt32(item.templateId);
    pkt.writeInt32(item.quantity);
    pkt.writeInt32(item.slot);
    pkt.writeInt32(item.equipped ? 1 : 0);
    for (int i = 0; i < 9; ++i) pkt.writeInt32(0);

    if (extraData) {
        pkt.writeBytes(extraData, 60);
    } else {
        for (int i = 0; i < 60; ++i) pkt.writeUInt8(0);
    }

    return pkt;
}

// race packets

Packet PacketBuilder::raceInit() {
    // cmd 0xBE clears race arrays sub 478B50
    Packet pkt(CMD::S_RACE_INIT);
    return pkt;
}

Packet PacketBuilder::racePlayer1(int32_t id1, int32_t id2, int32_t id3, int32_t id4, int32_t id5,
                                  const std::u16string& name, const uint8_t* data20,
                                  const std::u16string& str2, const std::u16string& str3,
                                  const std::vector<std::array<uint8_t, 16>>& items) {
    // cmd 0xBF sub 47F390 client reads ascii cstrings not wide
    Packet pkt(CMD::S_RACE_PLAYER_1);

    pkt.writeInt32(id1);
    pkt.writeInt32(id2);
    pkt.writeInt32(id3);
    pkt.writeInt32(id4);
    pkt.writeInt32(id5);
    writeAsciiW(pkt, name);

    if (data20) {
        pkt.writeBytes(data20, 20);
    } else {
        for (int i = 0; i < 20; ++i) pkt.writeUInt8(0);
    }

    writeAsciiW(pkt, str2);
    writeAsciiW(pkt, str3);

    pkt.writeInt32(static_cast<int32_t>(items.size()));
    for (const auto& item : items) {
        pkt.writeBytes(item.data(), 16);
    }

    return pkt;
}

// the tile draw dereferences the option list via sub 451D30 with no null check one record minimum
static void writeCatalogOptions(Packet& pkt, const std::vector<uint32_t>& priceKeys, int32_t durability = 0) {
    // captured live from chibikart every purchasable item carries three option rows one day seven days permanent
    auto row = [&pkt](uint32_t key, int32_t unit, int32_t period) {
        pkt.writeUInt32(key);
        pkt.writeInt32(unit);
        pkt.writeInt32(period);
        pkt.writeInt32(0);
    };
    if (priceKeys.empty()) {
        pkt.writeInt32(1);
        row(0, 0, 0);
        return;
    }
    // 0x42AD20 divides a mode 3 kart by option zero plus 8 so that amount is the full bar
    if (durability > 0) {
        pkt.writeInt32(static_cast<int32_t>(priceKeys.size()));
        for (uint32_t key : priceKeys) row(key, 3, durability);
        return;
    }
    static const int32_t kUnit[3]   = {1, 1, 0};
    static const int32_t kPeriod[3] = {1, 7, 0};  // one day seven days permanent
    pkt.writeInt32(static_cast<int32_t>(priceKeys.size()));
    for (size_t i = 0; i < priceKeys.size(); ++i) {
        const size_t slot = i < 3 ? i : 2;
        row(priceKeys[i], kUnit[slot], kPeriod[slot]);
    }
}

Packet PacketBuilder::driverCatalog(int32_t driverId, const std::string& bodyAsset,
                                    const std::string& nameKey,
                                    const std::array<int32_t, 5>& slotKeys,
                                    const std::vector<uint32_t>& priceKeys,
                                    const std::string& infoKey,
                                    bool creationPick) {
    // 0xBF login driver catalog sub 47F390 char create reads id1 id2 must be non zero key is id4
    Packet pkt(CMD::S_RACE_PLAYER_1);

    // FUN 00473950 and 00473730 need both flags nonzero sub 00450060 rejects zero 0x00 flagA 0x04 flagB creation pick
    pkt.writeInt32(1);
    pkt.writeInt32(creationPick ? 1 : 0);
    // 0x08 id3 0x0c id4 lookup key matches equipped driver id 0x10 id5 both unused
    pkt.writeInt32(0);
    pkt.writeInt32(driverId);
    pkt.writeInt32(0);
    // 0x14 name ascii body asset 0x38 five slot keys sub 4A5ED0 to sub 48C680 hangs model on bone
    pkt.writeString(bodyAsset);
    for (int32_t k : slotKeys) pkt.writeInt32(k);
    // str2 is the label the garage draws finished name not key 0x4c name 0x6d description buy popup draws
    pkt.writeString(nameKey.empty() ? bodyAsset : nameKey);
    pkt.writeString(infoKey);
    writeCatalogOptions(pkt, priceKeys);

    return pkt;
}

Packet PacketBuilder::vehicleCatalog(int32_t templateId, const std::string& modelName,
                                     const std::array<int32_t, 8>& defaultParts,
                                     const std::vector<uint32_t>& priceKeys,
                                     bool isFactoryCar, const std::string& titleKey,
                                     const std::string& infoKey,
                                     const std::array<float, 17>& stats,
                                     const std::array<KartAbilityPair, 2>& abilities,
                                     int32_t durabilityOption) {
    // 0xC0 vehicle catalog sub 47F4F0 key is templateId modelName feeds the car chassis path
    Packet pkt(CMD::S_RACE_PLAYER_2);

    // chibikart sends zero at offset 0x00 so FUN 00418E00 and FUN 00419020 skip every kart row
    KartDefinitionFields f;
    // 0x00 shop visible 0x04 badge overlay 1 new 2 hot 0x08 kartKey must match owned vehicle type
    f.visibleFlag = 1;
    f.badge = 0;
    f.kartKey = static_cast<uint32_t>(templateId);
    // 0x0c unk no reader 0x10 vehicleKind 2 on every row car 0x33B4 0x14 factory car flag
    f.unk0c = 0;
    f.vehicleKind = 2;
    f.modelScheme = isFactoryCar ? 1 : 0;
    // 0x18 unk no reader 0x1c level gate 0 opens every kart 0x20 str1 model asset name
    f.unk18 = 0;
    f.requiredLevel = 0;
    f.modelName = modelName;
    // 0x41 str2 displayNameKey drawn on every kart screen 0x62 str3 descriptionKey the description block
    f.displayNameKey = titleKey;
    f.descriptionKey = infoKey;
    static_assert(sizeof(defaultParts) == f.defaultSkinKeys.size(), "defaultParts must stay 32 bytes");
    std::memcpy(f.defaultSkinKeys.data(), defaultParts.data(), f.defaultSkinKeys.size());
    // 0xa4 seventeen float stat block 0x130 0x138 id -1 hides pair id 0 draws icon 0% every tile
    f.stats = stats;
    f.abilityPair0 = abilities[0];
    f.abilityPair1 = abilities[1];

    writeKartDefinitionBody(pkt, f);
    writeCatalogOptions(pkt, priceKeys, durabilityOption);

    return pkt;
}

Packet PacketBuilder::racePlayer2(const uint8_t* playerData, size_t len) {
    // cmd 0xC0 raw passthrough sub 47F4F0
    Packet pkt(CMD::S_RACE_PLAYER_2);
    if (playerData && len > 0) {
        pkt.writeBytes(playerData, len);
    }
    return pkt;
}

Packet PacketBuilder::raceData(int32_t val1, int32_t val2,
                               const std::u16string& str1, const std::u16string& str2) {
    // cmd 0xC4 sub 478C40
    Packet pkt(CMD::S_RACE_DATA);
    pkt.writeInt32(val1);
    pkt.writeInt32(val2);
    writeWString(pkt, str1);
    writeWString(pkt, str2);
    return pkt;
}

// entity packets

Packet PacketBuilder::entityUpdate(const uint8_t* header20, const uint8_t* data28,
                                   int32_t type, const uint8_t* typeData, size_t typeLen) {
    // cmd 0xED sub 47D5E0
    Packet pkt(CMD::S_ENTITY_UPDATE);
    
    if (header20) {
        pkt.writeBytes(header20, 20);
    } else {
        for (int i = 0; i < 20; ++i) pkt.writeUInt8(0);
    }
    
    if (data28) {
        pkt.writeBytes(data28, 28);
    } else {
        for (int i = 0; i < 28; ++i) pkt.writeUInt8(0);
    }
    
    if (typeData && typeLen > 0) {
        pkt.writeBytes(typeData, typeLen);
    }
    
    return pkt;
}

Packet PacketBuilder::entityPosition(int32_t entityId, const uint8_t* posData16) {
    // cmd 0xEE int32 plus 16 bytes sub 47D880
    Packet pkt(CMD::S_ENTITY_POSITION);
    pkt.writeInt32(entityId);
    if (posData16) {
        pkt.writeBytes(posData16, 16);
    } else {
        for (int i = 0; i < 16; ++i) pkt.writeUInt8(0);
    }
    return pkt;
}

Packet PacketBuilder::entityRemove(int32_t entityId) {
    // cmd 0xF0 sub 47D930
    Packet pkt(CMD::S_ENTITY_REMOVE);
    pkt.writeInt32(entityId);
    return pkt;
}

Packet PacketBuilder::entityClear() {
    // cmd 0xF1 sub 47D970
    Packet pkt(CMD::S_ENTITY_CLEAR);
    return pkt;
}

Packet PacketBuilder::clockSync() {
    // cmd 0xF1 sub 47D970 is a wall clock struct nine int32 not an entity clear
    Packet pkt(CMD::S_ENTITY_CLEAR);
    std::time_t now = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &now);   // msvc argument order is the mirror of the posix one
#else
    localtime_r(&now, &lt);
#endif
    pkt.writeInt32(lt.tm_sec);
    pkt.writeInt32(lt.tm_min);
    pkt.writeInt32(lt.tm_hour);
    pkt.writeInt32(lt.tm_mday);
    pkt.writeInt32(lt.tm_mon);
    pkt.writeInt32(lt.tm_year);
    pkt.writeInt32(lt.tm_wday);
    pkt.writeInt32(lt.tm_yday);
    pkt.writeInt32(lt.tm_isdst);
    return pkt;
}

Packet PacketBuilder::entityDataRaw(uint16_t cmdFull, const uint8_t* data, size_t len) {
    // generic for cmd above 0xFF
    Packet pkt = Packet::fromCmdFull(cmdFull);
    if (data && len > 0) {
        pkt.writeBytes(data, len);
    }
    return pkt;
}

Packet PacketBuilder::pendantDefinition(int32_t visible, int32_t pendantKey,
                                        const std::string& iconBase,
                                        const std::string& titleKey,
                                        const std::string& infoKey) {
    // cmd 0x119 sub 47E800 a pendant definition icon path uses percent s and a two digit index
    Packet pkt = Packet::fromCmdFull(CMD::S_ENTITY_DATA_281);
    pkt.writeInt32(visible);
    pkt.writeInt32(pendantKey);
    pkt.writeString(iconBase.substr(0, 32));
    pkt.writeString(titleKey.substr(0, 32));
    pkt.writeString(infoKey.substr(0, 33));
    return pkt;
}

Packet PacketBuilder::entitySimple(bool cmd283, int32_t val1, int32_t val2) {
    // cmd 0x11A or 0x11B two int32 sub 47E880
    uint16_t cmd = cmd283 ? CMD::S_ENTITY_DATA_283 : CMD::S_ENTITY_DATA_282;
    Packet pkt = Packet::fromCmdFull(cmd);
    pkt.writeInt32(val1);
    pkt.writeInt32(val2);
    return pkt;
}

// game mode and extended data

Packet PacketBuilder::gameModeSetup(int32_t mode) {
    // cmd 0x14 short form sub 479CC0
    Packet pkt(CMD::S_GAME_MODE_SETUP);
    pkt.writeInt32(mode);
    return pkt;
}

Packet PacketBuilder::gameModeSetupFull(int32_t mode, int32_t val1, int32_t val2,
                                        int32_t val3, int32_t val4, int32_t val5) {
    // cmd 0x14 full form mode 3 or 8 ui state 11 sub 479CC0
    Packet pkt(CMD::S_GAME_MODE_SETUP);
    pkt.writeInt32(mode);
    pkt.writeInt32(val1);
    pkt.writeInt32(val2);
    pkt.writeInt32(val3);
    pkt.writeInt32(val4);
    pkt.writeInt32(val5);
    return pkt;
}

Packet PacketBuilder::extendedData130(const uint8_t* data396) {
    // cmd 0x82 fixed 396 bytes sub 47B710
    Packet pkt(CMD::S_EXT_DATA_130);
    if (data396) {
        pkt.writeBytes(data396, 396);
    } else {
        for (int i = 0; i < 396; ++i) pkt.writeUInt8(0);
    }
    return pkt;
}

Packet PacketBuilder::extendedData131(const uint8_t* data396) {
    // cmd 0x83 fixed 396 bytes sub 47B7D0
    Packet pkt(CMD::S_EXT_DATA_131);
    if (data396) {
        pkt.writeBytes(data396, 396);
    } else {
        for (int i = 0; i < 396; ++i) pkt.writeUInt8(0);
    }
    return pkt;
}

// friend buddy block

Packet PacketBuilder::buddyListEntry(const BuddyEntry& entry) {
    // buddy list entry 112 bytes sub 47DB60
    Packet pkt(CMD::S_BUDDY_LIST_ENTRY);
    pkt.writeInt32(entry.characterId);
    pkt.writeInt32(entry.level);
    pkt.writeInt32(entry.rankPoints);
    pkt.writeInt32(entry.isOnline);

    for (size_t i = 0; i < 24; ++i) {
        char16_t c = (i < entry.name.size()) ? entry.name[i] : 0;
        pkt.writeUInt8(static_cast<uint8_t>(c & 0xFF));
        pkt.writeUInt8(static_cast<uint8_t>((c >> 8) & 0xFF));
    }

    pkt.writeInt32(entry.vehicleTemplateId);
    pkt.writeInt32(entry.wins);
    pkt.writeInt32(entry.losses);
    pkt.writeInt32(entry.status);

    for (int i = 0; i < 32; ++i) pkt.writeUInt8(0);
    return pkt;
}

Packet PacketBuilder::buddyStatusList(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& entries) {
    // count plus 12 bytes per entry sub 47DBB0
    Packet pkt(CMD::S_BUDDY_STATUS_LIST);
    pkt.writeInt32(static_cast<int32_t>(entries.size()));
    for (const auto& [id, state, flags] : entries) {
        pkt.writeInt32(id);
        pkt.writeInt32(state);
        pkt.writeInt32(flags);
    }
    return pkt;
}

Packet PacketBuilder::friendOnlineFlip(int32_t characterId, bool online) {
    // two int32 sub 47D1C0
    Packet pkt(CMD::S_FRIEND_ONLINE_FLIP);
    pkt.writeInt32(characterId);
    pkt.writeInt32(online ? 1 : 0);
    return pkt;
}

Packet PacketBuilder::friendStatsUpdate(int32_t characterId, int32_t stat1, int32_t stat2, int32_t stat3) {
    // alias of cmd 0xCF sub 47D4A0
    Packet pkt(CMD::S_FRIEND_STATS_UPDATE);
    pkt.writeInt32(characterId);
    pkt.writeInt32(stat1);
    pkt.writeInt32(stat2);
    pkt.writeInt32(stat3);
    return pkt;
}

Packet PacketBuilder::friendRemoveNotify(int32_t characterId) {
    // single int32 sub 47D930
    Packet pkt(CMD::S_FRIEND_REMOVE);
    pkt.writeInt32(characterId);
    return pkt;
}

Packet PacketBuilder::friendRecordUpdate(const uint8_t* data36) {
    // 36 bytes record sub 47D970
    Packet pkt(CMD::S_FRIEND_RECORD);
    if (data36) {
        pkt.writeBytes(data36, 36);
    } else {
        for (int i = 0; i < 36; ++i) pkt.writeUInt8(0);
    }
    return pkt;
}

Packet PacketBuilder::blockListRefresh(const std::vector<std::pair<int32_t, int32_t>>& blocked) {
    // count plus 8 bytes per entry sub 47DB00
    Packet pkt(CMD::S_BLOCK_LIST_REFRESH);
    pkt.writeInt32(static_cast<int32_t>(blocked.size()));
    for (const auto& [id, flag] : blocked) {
        pkt.writeInt32(id);
        pkt.writeInt32(flag);
    }
    return pkt;
}

// gift inbox

Packet PacketBuilder::giftInboxList(const std::vector<std::array<uint8_t, 0xD4>>& gifts) {
    // container A sub 47BD80 fill only this is the archive not the inbox
    Packet pkt(CMD::S_GIFT_INBOX_RECV);
    pkt.writeInt32(static_cast<int32_t>(gifts.size()));
    for (const auto& gift : gifts) {
        pkt.writeBytes(gift.data(), 0xD4);
    }
    return pkt;
}

Packet PacketBuilder::giftInboxListSent(const std::vector<std::array<uint8_t, 0xD4>>& gifts) {
    // container B sub 47BE00 this is the real mailbox fill this one
    Packet pkt(CMD::S_GIFT_INBOX_SENT);
    pkt.writeInt32(static_cast<int32_t>(gifts.size()));
    for (const auto& gift : gifts) {
        pkt.writeBytes(gift.data(), 0xD4);
    }
    return pkt;
}

// catalog stubs chibikart reads count first so zero is a valid empty answer
static Packet emptyList(uint16_t cmd, int32_t count) {
    Packet pkt(cmd);
    pkt.writeInt32(count);
    return pkt;
}

Packet PacketBuilder::emptyAck(uint16_t cmd) { return Packet(cmd); }

Packet PacketBuilder::petCatalog(int32_t c)     { return emptyList(CMD::S_PET_CATALOG, c); }
Packet PacketBuilder::skyCatalog(int32_t c)     { return emptyList(CMD::S_SKY_CATALOG, c); }
Packet PacketBuilder::factoryCatalog(int32_t c) { return emptyList(CMD::S_FACTORY_CATALOG, c); }
Packet PacketBuilder::itemCatalog(int32_t c)    { return emptyList(CMD::S_ITEM_CATALOG, c); }
Packet PacketBuilder::licenseCatalog(int32_t c) { return emptyList(CMD::S_LICENSE_CATALOG, c); }
Packet PacketBuilder::licenseRank(int32_t c)    { return emptyList(CMD::S_LICENSE_RANK, c); }
Packet PacketBuilder::giftInboxAll(int32_t c)   { return emptyList(CMD::S_GIFT_INBOX_LIST, c); }
Packet PacketBuilder::entityList260(int32_t c)  { return emptyList(CMD::S_ENTITY_DATA_260, c); }
Packet PacketBuilder::entityList263(int32_t c)  { return emptyList(CMD::S_ENTITY_DATA_263, c); }
Packet PacketBuilder::entityList269(int32_t c)  { return emptyList(CMD::S_ENTITY_DATA_269, c); }

std::array<uint8_t, 0xD4> PacketBuilder::giftRecord(int32_t id, int32_t category,
                                                    int32_t senderId,
                                                    const std::string& senderName,
                                                    const std::string& date,
                                                    const std::string& time,
                                                    const std::string& message) {
    // layout captured from a chibikart 0x95 the same record 0x97 and 0x99 carry
    std::array<uint8_t, 0xD4> rec{};
    auto wi32 = [&rec](size_t off, int32_t v) {
        rec[off] = v & 0xFF; rec[off + 1] = (v >> 8) & 0xFF;
        rec[off + 2] = (v >> 16) & 0xFF; rec[off + 3] = (v >> 24) & 0xFF;
    };
    auto fixedW = [&rec](size_t off, size_t capChars, const std::string& str) {
        size_t n = 0;
        for (char c : str) {
            if (n >= capChars) break;
            rec[off] = static_cast<uint8_t>(c);
            off += 2;
            ++n;
        }
    };
    wi32(0x00, id);
    wi32(0x04, category);
    wi32(0x08, senderId);
    fixedW(0x10, 24, senderName);
    size_t off = 0x44;
    auto packW = [&rec, &off](const std::string& str) {
        for (char c : str) {
            if (off + 2 > 0xD4) return;
            rec[off] = static_cast<uint8_t>(c);
            off += 2;
        }
        if (off + 2 <= 0xD4) off += 2;   // NUL terminator
    };
    packW(date);
    packW(time);
    packW("");            // empty field between time and subject
    packW(message);
    return rec;
}

Packet PacketBuilder::playerNotice(int32_t playerId, const std::u16string& text,
                                   uint8_t type) {
    // sub 47E6D0 reads id then wide string then one byte in that order
    Packet pkt = Packet::fromCmdFull(CMD::S_PLAYER_NOTICE);
    pkt.writeInt32(playerId);
    writeWString(pkt, text);
    pkt.writeUInt8(type);
    return pkt;
}

Packet PacketBuilder::giftArrived(const std::array<uint8_t, 0xD4>& rec) {
    // 0x99 appends one 212 byte record via sub 47BE80 into the same container 0x95 and 0x97 fill
    Packet pkt(CMD::S_GIFT_ARRIVED);
    pkt.writeBytes(rec.data(), rec.size());
    return pkt;
}

Packet PacketBuilder::giftSendConfirm(int32_t senderId, int32_t recipientId,
                                       const uint8_t* gift212) {
    // send confirm sub 47BEF0
    Packet pkt(CMD::S_GIFT);
    pkt.writeInt32(senderId);
    pkt.writeInt32(recipientId);
    if (gift212) {
        pkt.writeBytes(gift212, 0xD4);
    } else {
        for (int i = 0; i < 0xD4; ++i) pkt.writeUInt8(0);
    }
    return pkt;
}

// gacha

Packet PacketBuilder::gachaBannerListHeader(int32_t totalBanners) {
    // banner list header sub 47C9C0
    Packet pkt(CMD::S_GACHA_BANNER_HEADER);
    pkt.writeInt32(totalBanners);
    return pkt;
}

Packet PacketBuilder::gachaBannerEntry(const uint8_t* banner176,
                                        const std::vector<std::array<uint8_t, 0xB0>>& children) {
    // banner entry 176 bytes plus children sub 47C9F0
    Packet pkt(CMD::S_GACHA_BANNER_ENTRY);
    pkt.writeInt32(0);
    if (banner176) {
        pkt.writeBytes(banner176, 0xB0);
    } else {
        for (int i = 0; i < 0xB0; ++i) pkt.writeUInt8(0);
    }
    pkt.writeInt32(static_cast<int32_t>(children.size()));
    for (const auto& child : children) {
        pkt.writeBytes(child.data(), 0xB0);
    }
    return pkt;
}

Packet PacketBuilder::gachaRollPayout(int32_t headerId,
                                       const uint8_t* mainPrize176,
                                       const std::vector<std::array<uint8_t, 0xB0>>& bonusPrizes) {
    // roll payout main plus bonus max 3 sub 47CC20
    Packet pkt(CMD::S_GACHA_ROLL_PAYOUT);
    pkt.writeInt32(headerId);
    if (mainPrize176) {
        pkt.writeBytes(mainPrize176, 0xB0);
    } else {
        for (int i = 0; i < 0xB0; ++i) pkt.writeUInt8(0);
    }
    const int32_t n = std::min<int32_t>(3, static_cast<int32_t>(bonusPrizes.size()));
    pkt.writeInt32(n);
    for (int32_t i = 0; i < n; ++i) {
        pkt.writeBytes(bonusPrizes[i].data(), 0xB0);
    }
    return pkt;
}

Packet PacketBuilder::gachaTransactionResult(int32_t resultCode,
                                              int32_t header[7],
                                              int32_t type,
                                              const uint8_t* typeData,
                                              size_t typeLen) {
    // dispatch table sub 47EEF0 sends this to 0x135 which reads seven u32 then blocks
    Packet pkt = Packet::fromCmdFull(0x135);
    for (int i = 0; i < 7; ++i) pkt.writeInt32(header ? header[i] : 0);
    pkt.writeInt32(type);
    if (typeData && typeLen > 0) {
        pkt.writeBytes(typeData, typeLen);
    }
    pkt.writeInt32(resultCode);
    return pkt;
}

bool PacketBuilder::burstHasOpcode(const std::vector<Packet>& burst, uint16_t opcode) {
    for (const Packet& p : burst) {
        if (p.opcode() == opcode) return true;
    }
    return false;
}

Packet PacketBuilder::partCatalog(int32_t partKey, int32_t uiCategory,
                                  const std::string& modelName,
                                  const std::string& displayKey,
                                  const std::string& descKey,
                                  int32_t requiredLevel,
                                  const std::vector<uint32_t>& priceKeys,
                                  int32_t driverRestrict, bool kartSide, bool shopVisible) {
    // 0xC2 handler sub 47F800 reads four int32 one ascii string three int32 two ascii strings sixteen bytes then options
    Packet pkt(CMD::S_PART_CATALOG);

    // FUN 00418E00 tests shop visible FUN 004199D0 gates tile FUN 0042B1B0 draws icons 0x00 visible 0x08 partKey sub 4510C0
    pkt.writeInt32(shopVisible ? 1 : 0);
    pkt.writeInt32(0);
    pkt.writeInt32(partKey);
    // 0x0c requiredLevel 0x10 modelName asset feeds car parts path
    pkt.writeInt32(requiredLevel);
    pkt.writeString(modelName);
    // 0x34 kartSide 0x38 uiCategory which tab the tile lands on 0x3c driverRestrict
    pkt.writeInt32(kartSide ? 1 : 0);
    pkt.writeInt32(uiCategory);
    pkt.writeInt32(driverRestrict);
    pkt.writeString(displayKey);          // 0x40 label
    pkt.writeString(descKey);
    for (int i = 0; i < 2; ++i) { pkt.writeInt32(-1); pkt.writeInt32(0); }
    writeCatalogOptions(pkt, priceKeys);

    return pkt;
}

Packet PacketBuilder::roomTrackSelect(int32_t trackId, int32_t subType) {
    // 0x35 the room track panel trackId is the track catalog id
    Packet pkt(CMD::S_ROOM_TRACK_SELECT);
    pkt.writeInt32(trackId);
    pkt.writeInt32(subType);
    return pkt;
}

Packet PacketBuilder::roomLoadoutUpdate(int32_t playerId,
                                        const std::array<uint8_t, 0x2C>& character,
                                        const std::array<uint8_t, 0x38>& kart,
                                        const std::array<uint8_t, 0x3C>& customCar) {
    // 0xD9 sub 47D540 for the local player it also sets the lobby character and kart selection
    Packet pkt(CMD::S_PLAYER_FULL_UPDATE);
    pkt.writeInt32(playerId);
    pkt.writeBytes(character.data(), character.size());
    pkt.writeBytes(kart.data(), kart.size());
    pkt.writeBytes(customCar.data(), customCar.size());
    return pkt;
}

std::array<uint8_t, 0x2C> PacketBuilder::characterRecord(int32_t instanceId, int32_t baseKey,
                                                         const std::array<int32_t, 5>& slots) {
    // record offset 0x00 owned instance id offset 0x04 driver base key offset 0x08 five accessory keys
    std::array<uint8_t, 0x2C> rec{};
    auto put = [&rec](size_t off, int32_t v) {
        rec[off + 0] = static_cast<uint8_t>(v & 0xFF);
        rec[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        rec[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        rec[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    };
    put(0x00, instanceId);
    put(0x04, baseKey);
    for (size_t i = 0; i < slots.size(); ++i) put(0x08 + i * 4, slots[i]);
    put(0x34, 1);  // active flag
    return rec;
}

Packet PacketBuilder::driverCatalog(int32_t driverId, const std::string& bodyAsset) {
    // the char create screen only needs the asset no prices and no slots
    return driverCatalog(driverId, bodyAsset, std::string(), {0, 0, 0, 0, 0}, {}, std::string());
}

Packet PacketBuilder::carPartCatalog(int32_t partKey, int32_t category,
                                     const std::string& modelName,
                                     const std::string& displayKey,
                                     const std::string& descKey,
                                     int32_t tier, int32_t requiredLevel,
                                     const std::vector<uint32_t>& priceKeys) {
    // 0x108 handler sub 00480210 reads five int32 three ascii strings a block then options getting this short drops the socket
    Packet pkt = Packet::fromCmdFull(CMD::S_CAR_PART_CATALOG);

    // partKey repeats at 0x00 0x04 0x08 lookup key 0x0c category craft tab 0x10 level 0x14 modelName asset
    pkt.writeInt32(partKey);
    pkt.writeInt32(partKey);
    pkt.writeInt32(partKey);
    pkt.writeInt32(category);
    pkt.writeInt32(requiredLevel);
    pkt.writeString(modelName);
    pkt.writeString(displayKey);
    pkt.writeString(descKey);

    // 0x44 stat block tier sits eight bytes in
    std::array<uint8_t, 0x44> block{};
    block[8]  = static_cast<uint8_t>(tier & 0xFF);
    block[9]  = static_cast<uint8_t>((tier >> 8) & 0xFF);
    block[10] = static_cast<uint8_t>((tier >> 16) & 0xFF);
    block[11] = static_cast<uint8_t>((tier >> 24) & 0xFF);
    pkt.writeBytes(block.data(), block.size());

    // two ability pairs with id minus one so sub 42B7A0 draws no 0% icon
    for (int i = 0; i < 2; ++i) { pkt.writeInt32(-1); pkt.writeInt32(0); }
    for (int i = 0; i < 12; ++i) pkt.writeUInt8(0);
    writeCatalogOptions(pkt, priceKeys);

    return pkt;
}

} // namespace knc
