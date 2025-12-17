#pragma once
#include "net/Packet.h"
#include "net/Protocol.h"
#include <string>
#include <vector>
#include <array>

namespace knc {

// player info for packets (different from Protocol.h structs)
struct PlayerData {
    // Base info
    int32_t id = 0;
    int32_t accountId = 0;
    std::string name;
    
    // Progression
    int32_t level = 1;
    int32_t xp = 0;
    int32_t gold = 0;
    int32_t cash = 0;
    
    // Stats
    int32_t wins = 0;
    int32_t losses = 0;
    int32_t totalRaces = 0;
    int32_t playtimeMinutes = 0;
    
    // License/Rank
    int32_t licenseClass = 0;    // 0=Rookie, 1=Amateur, 2=Pro, 3=Master
    int32_t rankPoints = 0;
    
    // Equipped
    int32_t vehicleId = 0;       // Equipped vehicle unique ID
    int32_t vehicleTemplateId = 0;  // Equipped vehicle template
    int32_t driverId = 1;        // Skin/driver ID
    
    // Room state
    uint8_t team = 0;
    uint8_t slot = 0;
    bool ready = false;
    
    // Flags
    bool tutorialCompleted = false;
    bool isGM = false;
};

struct RoomData {
    uint32_t id = 0;
    std::string name;
    std::string password;
    uint8_t mode = 0;
    uint8_t maxPlayers = 8;
    uint8_t currentPlayers = 0;
    uint8_t mapId = 0;
    uint8_t laps = 3;
    uint8_t state = 0;
    bool isPrivate = false;
};

struct VehicleInfo {
    int32_t id = 0;           // Unique instance ID (DB id)
    int32_t templateId = 0;   // Vehicle type (from Define/*.txt)
    int32_t ownerId = 0;      // Character ID
    int32_t durability = 100;
    int32_t maxDurability = 100;
    int32_t stats[7] = {50, 50, 50, 40, 30, 50, 0};  // speed, accel, handling, drift, boost, weight, special
    bool equipped = false;
};

struct ItemInfo {
    int32_t id = 0;           // Unique instance ID (DB id)
    int32_t templateId = 0;   // Item type (from Define/*.txt)
    int32_t ownerId = 0;      // Character ID
    int32_t quantity = 1;
    int32_t slot = 0;
    bool equipped = false;
};

struct AccessoryInfo {
    int32_t id = 0;           // Unique instance ID (DB id)
    int32_t templateId = 0;   // Accessory type (from Define/*.txt)
    int32_t slot = 0;
    int32_t bonus1 = 0;
    int32_t bonus2 = 0;
    int32_t bonus3 = 0;
    bool equipped = false;
};

class PacketBuilder {
public:
    // auth packets
    static Packet connectionOk();
    static Packet connectionOkWithPlayer(const PlayerData& player);  // 0x0A with real player data
    static Packet displayMessage(const std::u16string& msg, int32_t code);
    static Packet loginResponse(bool success, const std::string& msg);
    static Packet sessionConfirm(int32_t accountId, const PlayerData& player);
    static Packet heartbeatResponse();
    static Packet ack();
    static Packet initResponse();
    static Packet trigger();                    // 0x03 - Character creation trigger
    static Packet registrationResponse(int32_t result, const std::u16string& nickname,
                                       const VehicleInfo* vehicle = nullptr,
                                       const ItemInfo* item = nullptr);  // 0x04
    static Packet dataPairs(const std::vector<std::pair<int32_t, int32_t>>& pairs);  // 0x0C
    static Packet flagSet();                    // 0x0D
    static Packet uiState24();                  // 0x8F - After login
    static Packet uiState25(int32_t val1, int32_t val2);  // 0x90
    
    // ui packets
    static Packet showMenu();
    static Packet showLobby(const std::vector<RoomData>& rooms);
    static Packet showRoom(const RoomData& room, const std::vector<PlayerData>& players);
    static Packet showGarage();
    static Packet showShop();
    static Packet channelList(const std::vector<std::pair<int, std::string>>& channels);
    
    // room packets
    static Packet createRoomResponse(uint32_t roomId, bool success);
    static Packet roomInfo(const RoomData& room);
    static Packet broadcastPlayerData(const PlayerData& player);  // 0x21 - Broadcasts player data to room (NOT "room full")
    static Packet playerDisconnect(int32_t playerId);  // 0x22 - Player left room
    static Packet playerJoin(const PlayerData& player);
    static Packet playerLeft(int32_t playerId);
    static Packet playerUpdate(const PlayerData& player);
    static Packet roomState(uint8_t state);
    
    // game packets
    static Packet countdown(int32_t seconds);
    static Packet raceStart();
    static Packet position(int32_t playerId, float x, float y, float z, float rot);
    static Packet position32(int32_t val1, int32_t val2);  // 0x32 (8 bytes)
    static Packet flag34();                                 // 0x34 (0 bytes)
    static Packet finish(int32_t playerId, int32_t rank, int32_t time);
    static Packet results(const std::vector<std::pair<int32_t, int32_t>>& rankings);
    static Packet raceEnd();
    
    // inventory packets
    static Packet inventoryVehicles(const std::vector<VehicleInfo>& vehicles);
    static Packet inventoryItems(const std::vector<ItemInfo>& items);
    static Packet inventoryAccessories(const std::vector<AccessoryInfo>& accessories);
    
    // chat packets  
    static Packet chatMessage(const std::string& sender, const std::u16string& message);
    static Packet whisperEnable();
    static Packet whisperDisable();
    static Packet systemMessage(const std::u16string& message, int32_t type = 0);
    static Packet displayText(const std::u16string& text, int32_t param);  // 0xB6
    
    // redirect
    static Packet serverRedirect(const std::string& ip, int32_t port);
    
    // race packets
    static Packet gameState40(uint8_t state);         // 0x40 (1 byte!)
    static Packet gameMode(uint8_t mode);
    static Packet gameMode14(int32_t mode);     // 0x14 - Race/Game mode setup (auto-expands for race modes)
    static Packet gameMode14Full(int32_t mode, int32_t mapId, int32_t laps);  // 0x14 with all params
    static Packet lapInfo(int32_t playerId, uint8_t lap, int32_t lapTime);
    static Packet score(int32_t playerId, int32_t score);
    static Packet itemUse(int32_t playerId, int32_t itemId, int32_t targetId);
    static Packet itemHit(int32_t targetId, int32_t itemId, int32_t damage);
    static Packet playerStatus(int32_t playerId, uint8_t status);
    static Packet raceStatus(int32_t playerId, uint8_t position, int32_t time);
    static Packet largeGameState(const std::vector<PlayerData>& players);
    static Packet gameUpdate(int32_t value);    // 0x44
    static Packet playerData(int32_t playerId, int32_t val1, int32_t val2);  // 0x49
    static Packet gameData4B(int32_t playerId, int32_t type, int32_t param1, int32_t param2);  // 0x4B
    static Packet timestamp();                   // 0x4E
    static Packet roomState3D(int32_t val1, int32_t val2);  // 0x3D
    static Packet roomStatus(int32_t roomId, uint8_t status);  // 0x64
    static Packet speedUpdate(int32_t playerId, float speed);  // 0x65 (8 bytes)
    static Packet roomData5C(int32_t playerId, int32_t val1, int32_t val2);  // 0x5C (12 bytes)
    static Packet game5F(int32_t playerId, int32_t value);  // 0x5F (8 bytes!)
    
    // shop packets
    static Packet shopLookup(int32_t categoryId);
    static Packet shopItem(int32_t itemId, int32_t price, int32_t currency);
    static Packet shopItemList(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& items);
    // 0x6F - Shop Purchase Response - CERTIFIED from IDA sub_47B3C0
    // result=1: purchased vehicle (+44 bytes VehicleData)
    // result=12: purchased small item (+36 bytes)
    // other: just result code (error)
    static Packet shopPurchaseVehicle(const VehicleInfo& vehicle);  // result=1
    static Packet shopPurchaseSmallItem(const uint8_t* itemData);   // result=12, 36 bytes
    static Packet shopPurchaseError(int32_t errorCode);             // other results
    static Packet shopUpdate(int32_t gold, int32_t cash);
    static Packet shopChat(int32_t senderId, const std::u16string& senderName,
                           const std::string& message, int32_t param1, int32_t param2,
                           const std::u16string& extra, uint8_t flag);  // 0x6C
    static Packet shopCall();                               // 0x6E (0 bytes)
    static Packet shopEvent(int32_t eventId, int32_t param);  // 0x70 (8 bytes)
    
    // inventory extended
    static Packet addVehicle(const VehicleInfo& vehicle);
    static Packet addItem(const ItemInfo& item);
    static Packet addAccessory(const AccessoryInfo& accessory);
    static Packet dataBlock(const uint8_t* data, size_t len);  // 0x72 (104 bytes)
    static Packet entityData(const uint8_t* data, size_t len);  // 0x28 (104 bytes - different dest)
    static Packet tutorialFail();                              // 0x62 (0 bytes)
    static Packet slotUpdate(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& slots);  // 0x73
    static Packet shopPlayerUpdate(int32_t playerId, int16_t value, uint8_t flag);  // 0x69 (7 bytes)
    static Packet shopPlayerValue(int32_t playerId, int16_t value);  // 0x6A (6 bytes)
    static Packet missionComplete(int32_t missionId);        // 0xA1 (4 bytes)
    static Packet missionList(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& missions);  // 0xA2
    static Packet rewardClaim(int32_t flag1, int32_t flag2, int32_t rewardId,
                              int32_t rewardAmount, int32_t rewardType);  // 0xA3
    static Packet removeItem(int32_t type, int32_t itemId);  // 0xB8 - type first!
    static Packet equipItem(int32_t itemId, int32_t slot, bool equipped);
    static Packet equipVehicle(int32_t slotType, int32_t vehicleUniqueId,
                               int32_t goldChange, int32_t cashChange,
                               const VehicleInfo& vehicle);  // 0x8C type=0
    static Packet equipItemFull(int32_t slotType, int32_t itemUniqueId,
                                int32_t goldChange, int32_t cashChange,
                                const ItemInfo& item);  // 0x8C type=1
    static Packet equipAccessoryFull(int32_t slotType, int32_t accUniqueId,
                                     int32_t goldChange, int32_t cashChange,
                                     const AccessoryInfo& acc);  // 0x8C type=2/3/4
    static Packet itemUpdate(const ItemInfo& item);
    static Packet notification(const std::u16string& msg, int32_t type);
    
    // 0xB5 - Player Comparison - CERTIFIED from IDA sub_47CF80
    // Format: PlayerInfo1(1224) + PlayerInfo2(1224) + wstring nickname
    static Packet playerComparison(const uint8_t* player1Data, const uint8_t* player2Data,
                                   const std::u16string& nickname);  // ~2450+ bytes
    
    // =========================================================================
    // INVENTORY UPDATES (CMDs 183-186) - IDA VERIFIED
    // =========================================================================
    
    // 0xB7 (183) - Update item in inventory (type determines data structure)
    // type 0: VehicleData (44 bytes), type 1: ItemData (56 bytes)
    // type 2,3: AccessoryData (28 bytes)
    static Packet inventoryUpdateVehicle(int32_t goldChange, int32_t cashChange,
                                         const VehicleInfo& vehicle);
    static Packet inventoryUpdateItem(int32_t goldChange, int32_t cashChange,
                                      const ItemInfo& item);
    static Packet inventoryUpdateAccessory(int32_t type, int32_t goldChange, int32_t cashChange,
                                           const AccessoryInfo& accessory);
    
    // 0xB8 (184) - Remove item from inventory
    // type: 0=vehicle, 1=item, 2=accessory type1, 3=accessory type2, 5=pet
    static Packet inventoryRemove(int32_t type, int32_t uniqueId);
    
    // 0xB9 (185) - Slot/stats update (complex, varies by type)
    static Packet inventorySlotUpdate(int32_t type, const uint8_t* data, size_t len);
    
    // 0xBA (186) - Complex inventory operation
    static Packet inventoryOperation(int32_t operationType, const uint8_t* data, size_t len);
    
    // =========================================================================
    // GAME MESSAGES (CMD 206) - IDA VERIFIED sub_47D250
    // =========================================================================
    
    // 0xCE (206) - Game system messages (MSG_LEVEL_UP, MSG_COME_IN_FIRST, etc)
    // Format: ASCII string key + wstring name + int32 param
    static Packet gameMessage(const std::string& msgKey, const std::u16string& name, int32_t param);
    
    // Common message helpers
    static Packet msgLevelUp(const std::u16string& playerName);
    static Packet msgComeInFirst(const std::u16string& playerName, int32_t rank);
    static Packet msgComeInSecond(const std::u16string& playerName, int32_t rank);
    static Packet msgComeInThird(const std::u16string& playerName, int32_t rank);
    static Packet msgComeInOther(const std::u16string& playerName, int32_t rank);
    static Packet msgBecomeOwner(const std::u16string& playerName);
    static Packet msgLeftRoom(const std::u16string& playerName);
    static Packet msgDurabilityScroll(const std::u16string& playerName);
    
    // =========================================================================
    // ADDITIONAL PACKETS - IDA VERIFIED
    // =========================================================================
    
    // 0x33 (51) - Game State - sub_4799B0
    // Format: int32 playerId + int32 state (state=2 sets internal flag)
    static Packet gameState(int32_t playerId, int32_t state);
    
    // 0x47 (71) - Player Action - sub_47A110
    // Format: playerId(4) + type(4) + x(4) + y(4) + z(4) + extra(4) = 24 bytes
    // Types: 2=explosion?, 3=effect?, 4=boost?, 5=hit?
    static Packet playerAction(int32_t playerId, int32_t type, 
                               float x, float y, float z, float extra);
    
    // 0x98 (152) - Gift/Present - sub_47BEF0
    // Format: senderId(4) + receiverId(4) + giftData(212) = 220 bytes
    static Packet gift(int32_t senderId, int32_t receiverId, const uint8_t* giftData);
    
    // 0x9A (154) - Item Switch - sub_47BF80
    // Format: slotId(4) + type(4) + data(variable: 44/56/28 bytes)
    static Packet itemSwitchVehicle(int32_t slotId, const VehicleInfo& vehicle);
    static Packet itemSwitchItem(int32_t slotId, const ItemInfo& item);
    static Packet itemSwitchAccessory(int32_t slotId, const AccessoryInfo& accessory);
    
    // 0xAA (170) - Player Preview - sub_47CBA0
    // Shows player preview UI (state 15)
    // Format: int32 + wstring + 2×int32 + VehicleData(44) + ItemData(56)
    static Packet playerPreview(int32_t playerId, const std::u16string& name,
                                int32_t level, int32_t rank,
                                const VehicleInfo& vehicle, const ItemInfo& item);
    
    // 0xCF (207) - Player Stats Update - sub_47D4A0
    // Format: 4×int32 (playerId + 3 stats)
    static Packet playerStatsUpdate(int32_t playerId, int32_t stat1, int32_t stat2, int32_t stat3);
    
    // 0xD9 (217) - Player Full Update - sub_47D540
    // Format: int32 + VehicleData(44) + ItemData(56) + extraData(60)
    static Packet playerFullUpdate(int32_t playerId, const VehicleInfo& vehicle,
                                   const ItemInfo& item, const uint8_t* extraData);
    
    // =========================================================================
    // RACE PACKETS (CMDs 190-198) - IDA VERIFIED
    // =========================================================================
    
    // 0xBE (190) - Race Init - sub_478B50
    // No payload - clears all race data structures
    static Packet raceInit();
    
    // 0xBF (191) - Race Player Data 1 - sub_47F390
    // Complex: 5×int32 + wstring + 0x14 bytes + wstring + wstring + count + N×0x10
    static Packet racePlayer1(int32_t id1, int32_t id2, int32_t id3, int32_t id4, int32_t id5,
                              const std::u16string& name, const uint8_t* data20,
                              const std::u16string& str2, const std::u16string& str3,
                              const std::vector<std::array<uint8_t, 16>>& items);
    
    // 0xC0 (192) - Race Player Data 2 - sub_47F4F0
    // Complex: 8×int32 (with 1 byte) + 3×wstring + 0x20 + 0x44 + 2×8 bytes + count×0x10
    static Packet racePlayer2(const uint8_t* playerData, size_t len);  // ~164+ bytes
    
    // 0xC4 (196) - Race Data - sub_478C40
    // Format: 2×int32 + 2×wstring
    static Packet raceData(int32_t val1, int32_t val2, 
                           const std::u16string& str1, const std::u16string& str2);
    
    // =========================================================================
    // ENTITY PACKETS (CMDs 237-309) - IDA VERIFIED - USE fromCmdFull FOR CMD>255
    // =========================================================================
    
    // 0xED (237) - Entity Update - sub_47D5E0
    // Format: 0x14 bytes + 0x1C bytes + type switch (0=0x2C, 1=0x38, 2=0x1C, 3=0x1C, 4=0x30)
    static Packet entityUpdate(const uint8_t* header20, const uint8_t* data28, 
                               int32_t type, const uint8_t* typeData, size_t typeLen);
    
    // 0xEE (238) - Entity Position - sub_47D880
    // Format: int32 entityId + 16 bytes position data
    static Packet entityPosition(int32_t entityId, const uint8_t* posData16);
    
    // 0xF0 (240) - Entity Remove - sub_47D930
    static Packet entityRemove(int32_t entityId);
    
    // 0xF1 (241) - Entity Clear - sub_47D970
    static Packet entityClear();
    
    // Generic entity data packet for CMDs 243-309
    // Uses Packet::fromCmdFull() for CMD > 255
    static Packet entityDataRaw(uint16_t cmdFull, const uint8_t* data, size_t len);
    
    // 0x119 (281) - Entity with strings - sub_47E800
    // Format: 2×int32 + 3×wstring
    static Packet entityStrings(int32_t val1, int32_t val2,
                                const std::u16string& str1,
                                const std::u16string& str2,
                                const std::u16string& str3);
    
    // 0x11A/0x11B (282/283) - Entity simple - sub_47E880
    // Format: 8 bytes
    static Packet entitySimple(bool cmd283, int32_t val1, int32_t val2);
    
    // =========================================================================
    // GAME MODE & EXTENDED DATA - IDA VERIFIED
    // =========================================================================
    
    // 0x14 (20) - Game Mode Setup - sub_479CC0
    // Format: int32 mode, if mode==3||8: 5×int32 extra, sets UI state 11
    static Packet gameModeSetup(int32_t mode);  // mode != 3 && mode != 8
    static Packet gameModeSetupFull(int32_t mode, int32_t val1, int32_t val2, 
                                    int32_t val3, int32_t val4, int32_t val5);  // mode == 3 || 8
    
    // 0x82 (130) - Extended Data 396 bytes - sub_47B710
    static Packet extendedData130(const uint8_t* data396);
    
    // 0x83 (131) - Extended Data 396 bytes - sub_47B7D0
    static Packet extendedData131(const uint8_t* data396);
    
private:
    static void writeWString(Packet& pkt, const std::u16string& str);
    static void writePlayerInfo(Packet& pkt, const PlayerData& player);
};

} // namespace knc
