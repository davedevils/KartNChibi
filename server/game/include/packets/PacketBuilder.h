#pragma once
#include "net/Packet.h"
#include "net/Protocol.h"
#include "packets/KartDefinitionWire.h"
#include <string>
#include <vector>
#include <array>
#include <tuple>

namespace knc {

// spawn slot for 0x40 start race grid
struct RaceSpawn {
    int32_t playerId = 0;
    float x = 0, y = 0, z = 0;
    float rx = 0, ry = 0, rz = 0;
};

// one scoreboard row for 0x46 results sub 47A760
struct ResultRow {
    int32_t rank = 0;          // zero based placement
    int32_t playerId = 0;
    std::u16string name;       // up to 12 wchar finishTime is ms
    int32_t finishTime = 0;
    int32_t rewardGold = 0;
    int32_t rewardXp = 0;
    uint8_t team = 0;
};

// 0x21 room member wire packed 187 bytes plus names same record bytes as 0x1B and 0x1C
struct RoomMemberWire {
    uint32_t slot = 0;
    uint32_t team = 0;
    uint32_t playerId = 0;
    std::u16string displayName;
    uint8_t levelIndex = 0;
    uint8_t gmBadge = 0;
    uint8_t pcCafe = 0;
    uint32_t titleKey = 0;
    std::array<uint8_t, 0x2C> character{};
    std::array<uint8_t, 0x38> kart{};
    uint32_t readyState = 0;
    uint32_t petBaseKey = 0;
    std::array<uint8_t, 0x3C> customCar{};
};

// player info for packets differs from Protocol h structs
struct PlayerData {
    int32_t id = 0;
    int32_t accountId = 0;
    std::string name;
    /// the redirect token the profile blob head carries next to the account id
    std::string ticketToken;

    int32_t level = 1;
    int32_t xp = 0;
    int32_t gold = 0;
    int32_t cash = 0;

    int32_t wins = 0;
    int32_t losses = 0;
    int32_t totalRaces = 0;
    int32_t playtimeMinutes = 0;

    int32_t licenseClass = 0;    // 0 is Rookie 1 is Amateur 2 is Pro 3 is Master
    int32_t rankPoints = 0;
    // the worn pendant key profile blob 0x4C4 and 0x000A plus 0x22 zero for none
    int32_t pendantKey = 0;

    int32_t vehicleId = 0;
    int32_t vehicleTemplateId = 0;
    int32_t driverId = 1;        // skin or driver id

    uint8_t team = 0;
    uint8_t slot = 0;
    bool ready = false;

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
    // leading u32 of the 0x07 id space start button stays grey when it mismatches
    int32_t hostId = 0;
    // 0xC3 catalogue id of the room track the same value the 0x35 carries
    int32_t trackId = 0;
};

struct VehicleInfo {
    int32_t id = 0;
    int32_t templateId = 0;   // vehicle type from a Define txt file
    int32_t ownerId = 0;
    int32_t durability = 100;
    int32_t maxDurability = 100;
    int32_t stats[7] = {50, 50, 50, 40, 30, 50, 0};  // speed accel handling drift boost weight special
    bool equipped = false;
};

struct ItemInfo {
    int32_t id = 0;
    int32_t templateId = 0;   // item type from a Define txt file
    int32_t ownerId = 0;
    int32_t quantity = 1;
    int32_t slot = 0;
    bool equipped = false;
};

struct AccessoryInfo {
    int32_t id = 0;
    int32_t templateId = 0;   // accessory type from a Define txt file
    int32_t slot = 0;
    int32_t bonus1 = 0;
    int32_t bonus2 = 0;
    int32_t bonus3 = 0;
    bool equipped = false;
};

class PacketBuilder {
public:
    static Packet connectionOk();
    static Packet connectionOkWithPlayer(const PlayerData& player);  // 0x0A with real player data
    static Packet displayMessage(const std::u16string& msg, int32_t code);
    // 0x0001 sub 478DA0 shows the def trans line of the key 0x0002 would show the key itself
    static Packet messageKey(const std::string& key, int32_t code);
    static Packet loginResponse(bool success, const std::string& msg);
    static Packet sessionConfirm(int32_t driverId, const PlayerData& player);  // driverId equals characterId
    static Packet heartbeatResponse();
    static Packet ack();
    static Packet initResponse();
    static Packet trigger();                    // 0x03 character creation trigger
    static Packet registrationResponse(int32_t result, const std::u16string& nickname,
                                       const VehicleInfo* vehicle = nullptr,
                                       const ItemInfo* item = nullptr);  // registrationResponse is 0x04 dataPairs is 0x0C
    static Packet dataPairs(const std::vector<std::pair<int32_t, int32_t>>& pairs);
    static Packet flagSet();                    // flagSet is 0x0D uiState24 is 0x8F after login
    static Packet uiState24();
    static Packet uiState25(int32_t val1, int32_t val2);  // uiState25 is 0x90

    static Packet showMenu();
    static Packet showLobby(const std::vector<RoomData>& rooms);
    static Packet showRoom(const RoomData& room, const std::vector<PlayerData>& players);
    static Packet showGarage();
    static Packet showShop();
    static Packet channelList(const std::vector<std::pair<int, std::string>>& channels);
    
    // no 0x63 or 0x3F room answer is 0x13 chain sub 479C70 0x32 flips slot enabled before 0x21 or it bails
    static Packet roomSlotEnabled(uint32_t slot, bool enabled);
    // 0x21 room member commit creates the driver and car sub 40D650
    static Packet roomMember(const RoomMemberWire& member);
    static Packet broadcastPlayerData(const PlayerData& player);
    // 0x13 triggers client SetUIState 9 sub 47FC20 reads an int32 a name eight int32s a count then per player records
    static Packet playerRoomData(const RoomData& room,
                                 const std::vector<PlayerData>& players);
    static Packet playerDisconnect(int32_t playerId);  // 0x22 player left room
    static Packet playerJoin(const PlayerData& player);
    static Packet playerLeft(int32_t playerId);
    static Packet playerUpdate(const PlayerData& player);
    static Packet roomState(int32_t masterPlayerId);  // 0x30 one int32 the room masters player id

    // sub 479630 to sub 408360 0x2D adds a lobby room sub 407B50 draws it channel must be zero
    static Packet lobbyRoomAdd(uint32_t roomId, const std::u16string& name,
                               int32_t playerCount, int32_t maxPlayers,
                               int32_t gameMode, int32_t hasPassword,
                               int32_t playingFlag, int32_t timeLeftMs);
    // sub 479710 to sub 4084B0 0x2E drops one lobby room by id
    static Packet lobbyRoomRemove(uint32_t roomId);
    // sub 479BE0 to sub 407660 0x23 rewrites the playing flag that swaps the row art
    static Packet lobbyRoomState(uint32_t roomId, int32_t state);
    
    static Packet countdown(int32_t seconds);
    static Packet raceStart();
    static Packet position(int32_t playerId, float x, float y, float z, float rot);
    static Packet position32(int32_t val1, int32_t val2);  // 0x32 is 8 bytes flag34 0x34 has no body
    static Packet flag34();
    static Packet finish(int32_t playerId, int32_t rank, int32_t time);
    static Packet results(const std::vector<std::pair<int32_t, int32_t>>& rankings);
    // 0x46 scoreboard with proven rows sub 47A760
    static Packet resultsScoreboard(const std::vector<ResultRow>& rows);
    // sub 47A5C0 0x3C per recipient wallet totals plus a win flag
    static Packet raceEnd(int32_t playerId, int32_t goldTotal, int32_t cashTotal, bool won);
    
    static Packet inventoryItems(const std::vector<ItemInfo>& items);
    static Packet inventoryAccessories(const std::vector<AccessoryInfo>& accessories);
    // sub 478ec0 0x1B driver inventory 44 byte entries id must match selected driver plus catalog key and accessories
    static Packet driverInventory(const std::vector<int32_t>& driverIds);
    // sub 478f20 0x1C is the vehicle inventory 56 byte entries key must match the selected vehicle plus catalog fields
    
    static Packet chatMessage(uint32_t senderId, const std::u16string& message);
    static Packet whisperEnable();
    static Packet whisperDisable();
    static Packet systemMessage(const std::u16string& message, int32_t type = 0);
    // 0xB4 used as lobby chat broadcast sender id name message and chat type
    static Packet lobbyChatBroadcast(int32_t senderId,
                                     const std::u16string& senderName,
                                     const std::u16string& message,
                                     int32_t chatType = 0);
    static Packet displayText(const std::u16string& text, int32_t param);  // 0xB6
    
    static Packet serverRedirect(const std::string& ip, int32_t port);

    static Packet gameState40(uint8_t state);         // 0x40 is only 1 byte startRaceGrid 0x40 real grid 25B per player
    static Packet startRaceGrid(const std::vector<RaceSpawn>& grid);
    // 0x40 relay one remote 3D motion 25B forward raw packed bytes prefixed senderId
    static Packet motionRelay(int32_t senderId, const uint8_t packedPos[8],
                              const uint8_t packedRot[8], uint8_t flag, int16_t field);
    static Packet gameMode(uint8_t mode);
    static Packet gameMode14(int32_t mode);     // 0x14 is 4 bytes and 24 only for kind 3 or 8
    static Packet lapInfo(int32_t playerId, uint8_t lap, int32_t lapTime);
    static Packet score(int32_t playerId, int32_t score);
    // 0x45 must not carry item use sub 47A560 and sub 4B4B00 read it as a standings row wrong rank
    static Packet playerStatus(int32_t playerId, uint8_t status);
    static Packet raceStatus(int32_t playerId, uint8_t position, int32_t time);
    static Packet gameUpdate(int32_t value);    // gameUpdate is 0x44 playerData is 0x49
    static Packet playerData(int32_t playerId, int32_t val1, int32_t val2);
    static Packet gameData4B(int32_t playerId, int32_t type, int32_t param1, int32_t param2);  // gameData4B is 0x4B timestamp is 0x4E
    static Packet timestamp();
    static Packet roomStatus(int32_t roomId, uint8_t status);  // roomStatus is 0x64 speedUpdate is 0x65 8 bytes
    static Packet speedUpdate(int32_t playerId, float speed);
    static Packet roomData5C(int32_t playerId, int32_t val1, int32_t val2);  // roomData5C is 0x5C 12 bytes game5F is 0x5F 8 bytes
    static Packet game5F(int32_t playerId, int32_t value);

    static Packet shopLookup(int32_t categoryId);
    static Packet shopItem(int32_t itemId, int32_t price, int32_t currency);
    static Packet shopItemList(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& items);
    // sub 47B3C0 0x6F result is one vehicle and twelve small items or an error
    static Packet shopPurchaseVehicle(const VehicleInfo& vehicle);
    static Packet shopPurchaseSmallItem(const uint8_t* itemData);
    static Packet shopPurchaseError(int32_t errorCode);
    static Packet shopUpdate(int32_t gold, int32_t cash);
    /// s2c 0x12F short invite popup count zero clears it shares globals with 0x6C
    static Packet invitePopupShort(int32_t count, const std::u16string& name,
                                   int32_t id, const std::u16string& message);

    static Packet invitePopup(int32_t senderId, const std::u16string& senderName,
                           const std::string& message, int32_t param1, int32_t param2,
                           const std::u16string& extra, uint8_t flag);  // invitePopup is 0x6C shopCall 0x6E has no body
    static Packet shopCall();
    static Packet shopEvent(int32_t eventId, int32_t param);  // 0x70 is 8 bytes

    // shop catalog pushed on shop enter one row is id name price and stock
    struct ShopCatalogRow {
        int32_t itemId = 0;
        std::u16string name;
        int32_t price = -2;   // -2 means not for sale
        int32_t stock = -2;
    };
    // sub 47B1B0 0x76 kart catalog tab is count then 44 byte rows of id name price stock
    static Packet shopKartCatalog(const std::vector<ShopCatalogRow>& rows);
    // sub 47B220 0x78 item catalog tab is count then 36 byte rows of id and name
    static Packet shopItemCatalog(const std::vector<ShopCatalogRow>& rows);
    // sub 47B290 0x79 premium catalog tab is count then 32 byte rows of id and a 28 byte name
    static Packet shopPremiumCatalog(const std::vector<ShopCatalogRow>& rows);
    // sub 47B570 0x73 price and stock by id count then 12 byte rows of id price stock
    static Packet shopPriceStock(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& rows);
    
    static Packet addVehicle(const VehicleInfo& vehicle);
    static Packet addItem(const ItemInfo& item);
    static Packet addAccessory(const AccessoryInfo& accessory);
    static Packet dataBlock(const uint8_t* data, size_t len);  // dataBlock is 0x72 104 bytes entityData is 0x28 104 bytes to a different destination
    static Packet entityData(const uint8_t* data, size_t len);
    static Packet tutorialFail();                              // tutorialFail is 0x62 has no body slotUpdate is 0x73
    static Packet slotUpdate(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& slots);
    static Packet shopPlayerUpdate(int32_t playerId, int16_t value, uint8_t flag);  // shopPlayerUpdate is 0x69 7 bytes shopPlayerValue is 0x6A 6 bytes
    static Packet shopPlayerValue(int32_t playerId, int16_t value);
    static Packet missionComplete(int32_t missionId);        // missionComplete is 0xA1 4 bytes missionList is 0xA2
    static Packet missionList(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& missions);
    static Packet rewardClaim(int32_t flag1, int32_t flag2, int32_t rewardId,
                              int32_t rewardAmount, int32_t rewardType);  // rewardClaim is 0xA3 removeItem is 0xB8 sends type before the item id
    static Packet removeItem(int32_t type, int32_t itemId);
    static Packet equipItem(int32_t itemId, int32_t slot, bool equipped);
    static Packet equipVehicle(int32_t slotType, int32_t vehicleUniqueId,
                               int32_t goldChange, int32_t cashChange,
                               const VehicleInfo& vehicle);  // 0x8C type 0
    static Packet equipItemFull(int32_t slotType, int32_t itemUniqueId,
                                int32_t goldChange, int32_t cashChange,
                                const ItemInfo& item);  // 0x8C type 1
    static Packet equipAccessoryFull(int32_t slotType, int32_t accUniqueId,
                                     int32_t goldChange, int32_t cashChange,
                                     const AccessoryInfo& acc);  // 0x8C type 2 3 or 4
    static Packet itemUpdate(const ItemInfo& item);
    static Packet notification(const std::u16string& msg, int32_t type);
    
    // sub 47CF80 0xB5 garage visit shows two player infos plus the requester name
    static Packet playerComparison(const uint8_t* player1Data, const uint8_t* player2Data,
                                   const std::u16string& nickname);

    // inventory updates cmds 183 to 186

    // cmd 0xB7 type zero vehicle one item two three accessory
    static Packet inventoryUpdateVehicle(int32_t goldChange, int32_t cashChange,
                                         const VehicleInfo& vehicle);
    static Packet inventoryUpdateItem(int32_t goldChange, int32_t cashChange,
                                      const ItemInfo& item);
    static Packet inventoryUpdateAccessory(int32_t type, int32_t goldChange, int32_t cashChange,
                                           const AccessoryInfo& accessory);
    
    // 0xB8 removes an item from inventory type 0 vehicle 1 item 2 or 3 accessory 5 pet
    static Packet inventoryRemove(int32_t type, int32_t uniqueId);
    
    // 0xB9 slot or stats update varies by type
    static Packet inventorySlotUpdate(int32_t type, const uint8_t* data, size_t len);
    
    // 0xBA complex inventory operation
    static Packet inventoryOperation(int32_t operationType, const uint8_t* data, size_t len);
    
    // 0xCE game system messages ascii key plus wide name plus an int32 param
    static Packet gameMessage(const std::string& msgKey, const std::u16string& name, int32_t param);
    
    static Packet msgLevelUp(const std::u16string& playerName);
    static Packet msgComeInFirst(const std::u16string& playerName, int32_t rank);
    static Packet msgComeInSecond(const std::u16string& playerName, int32_t rank);
    static Packet msgComeInThird(const std::u16string& playerName, int32_t rank);
    static Packet msgComeInOther(const std::u16string& playerName, int32_t rank);
    static Packet msgBecomeOwner(const std::u16string& playerName);
    static Packet msgLeftRoom(const std::u16string& playerName);
    static Packet msgDurabilityScroll(const std::u16string& playerName);
    
    // sub 4799B0 0x33 game state state value 2 sets an internal flag
    static Packet gameState(int32_t playerId, int32_t state);
    
    // sub 47A110 0x47 player action type 2 explosion 3 effect 4 boost 5 hit guesses
    static Packet playerAction(int32_t playerId, int32_t type, 
                               float x, float y, float z, float extra);
    
    // sub 47BEF0 0x98 gift sender id receiver id and 212 bytes of gift data
    static Packet gift(int32_t senderId, int32_t receiverId, const uint8_t* giftData);
    
    // sub 47BF80 0x9A item switch slot id type then 44 56 or 28 bytes of data
    static Packet itemSwitchVehicle(int32_t slotId, const VehicleInfo& vehicle);
    static Packet itemSwitchItem(int32_t slotId, const ItemInfo& item);
    static Packet itemSwitchAccessory(int32_t slotId, const AccessoryInfo& accessory);
    
    // sub 47CBA0 0xAA player preview sets ui state 15
    static Packet playerPreview(int32_t playerId, const std::u16string& name,
                                int32_t level, int32_t rank,
                                const VehicleInfo& vehicle, const ItemInfo& item);
    
    // sub 47D4A0 0xCF player stats update is four int32s player id plus three stats
    static Packet playerStatsUpdate(int32_t playerId, int32_t stat1, int32_t stat2, int32_t stat3);
    
    // sub 47D540 0xD9 player full update is an int32 plus vehicle data plus item data plus 60 extra bytes
    static Packet playerFullUpdate(int32_t playerId, const VehicleInfo& vehicle,
                                   const ItemInfo& item, const uint8_t* extraData);
    
    // sub 478B50 0xBE race init has no payload and clears all race data structures
    static Packet raceInit();
    
    // sub 47F390 unk 80E680 0xBF driver catalog race roster ascii names key is id4 at entry offset 0x0c
    static Packet racePlayer1(int32_t id1, int32_t id2, int32_t id3, int32_t id4, int32_t id5,
                              const std::u16string& name, const uint8_t* data20,
                              const std::u16string& str2, const std::u16string& str3,
                              const std::vector<std::array<uint8_t, 16>>& items);

    // 0xBF login driver catalog entry char creation needs id1 and id2 non zero body asset key is driverId
    static Packet driverCatalog(int32_t driverId, const std::string& bodyAsset);
    static Packet driverCatalog(int32_t driverId, const std::string& bodyAsset,
                                const std::string& nameKey,
                                const std::array<int32_t, 5>& slotKeys,
                                const std::vector<uint32_t>& priceKeys,
                                const std::string& infoKey,
                                bool creationPick = false);

    // sub 47F4F0 0xC0 key is templateId at 0x08 model feeds chassis car paths ability pairs default hidden on every row
    static Packet vehicleCatalog(int32_t templateId, const std::string& modelName,
                                 const std::array<int32_t, 8>& defaultParts,
                                 const std::vector<uint32_t>& priceKeys,
                                 bool isFactoryCar, const std::string& titleKey,
                                 const std::string& infoKey,
                                 const std::array<float, 17>& stats,
                                 const std::array<KartAbilityPair, 2>& abilities = {},
                                 int32_t durabilityOption = 0);

    /// 0xC2 part record stride 0xDC key at offset 0x08 driver restrict negative one for any kart side flags paints
    static Packet partCatalog(int32_t partKey, int32_t uiCategory,
                              const std::string& modelName,
                              const std::string& displayKey,
                              const std::string& descKey,
                              int32_t requiredLevel,
                              const std::vector<uint32_t>& priceKeys = {},
                              int32_t driverRestrict = -1,
                              bool kartSide = false,
                              bool shopVisible = true);

    /// 0x2C character block without accessories
    static std::array<uint8_t, 0x2C> characterRecord(int32_t instanceId, int32_t baseKey);

    /// 0x38 kart block built from an equipped vehicle row
    static std::array<uint8_t, 0x38> kartRecord(const VehicleInfo& vehicle);

    /// 0xBF character record 0x2C bytes with five accessory slots
    static std::array<uint8_t, 0x2C> characterRecord(int32_t instanceId, int32_t baseKey,
                                                     const std::array<int32_t, 5>& slots);

    /// 0x108 car part catalog stride 0x120 category at offset 0x0c model at offset 0x14
    static Packet carPartCatalog(int32_t partKey, int32_t category,
                                 const std::string& modelName,
                                 const std::string& displayKey,
                                 const std::string& descKey,
                                 int32_t tier, int32_t requiredLevel,
                                 const std::vector<uint32_t>& priceKeys);

    /// 0x35 room track select panel
    static Packet roomTrackSelect(int32_t trackId, int32_t subType = 0);

    /// 0xD9 sets the client lobby selection from block instance ids
    static Packet roomLoadoutUpdate(int32_t playerId,
                                    const std::array<uint8_t, 0x2C>& character,
                                    const std::array<uint8_t, 0x38>& kart,
                                    const std::array<uint8_t, 0x3C>& customCar);

    static Packet racePlayer2(const uint8_t* playerData, size_t len);  // 0xC0 raw passthrough kept for race path roughly 164 or more bytes
    
    // sub 478C40 0xC4 race data two int32s plus two wide strings
    static Packet raceData(int32_t val1, int32_t val2, 
                           const std::u16string& str1, const std::u16string& str2);
    
    // sub 47D5E0 0xED entity update header plus data then a type switch sized 0x2C 0x38 0x1C 0x1C or 0x30
    static Packet entityUpdate(const uint8_t* header20, const uint8_t* data28, 
                               int32_t type, const uint8_t* typeData, size_t typeLen);
    
    // sub 47D880 0xEE entity position an int32 id plus 16 bytes of position data
    static Packet entityPosition(int32_t entityId, const uint8_t* posData16);
    
    // sub 47D930 0xF0 entity remove
    static Packet entityRemove(int32_t entityId);
    
    // sub 47D970 0xF1 entity clear
    static Packet entityClear();

    /// 0xF1 struct tm wall clock nine int32s keeps client time synced
    static Packet clockSync();
    
    // generic entity data for cmds above 255 uses Packet fromCmdFull
    static Packet entityDataRaw(uint16_t cmdFull, const uint8_t* data, size_t len);
    
    /// 0x119 pendant definition visible key icon base title key and info key ascii
    static Packet pendantDefinition(int32_t visible, int32_t pendantKey,
                                    const std::string& iconBase,
                                    const std::string& titleKey,
                                    const std::string& infoKey);
    
    // sub 47E880 0x11A or 0x11B entity simple is 8 bytes
    static Packet entitySimple(bool cmd283, int32_t val1, int32_t val2);
    
    // sub 479CC0 0x14 mode 3 or 8 adds five int32s sets ui state 11 else not 3 or 8
    static Packet gameModeSetup(int32_t mode);
    static Packet gameModeSetupFull(int32_t mode, int32_t val1, int32_t val2,
                                    int32_t val3, int32_t val4, int32_t val5);  // mode is 3 or 8
    
    // sub 47B710 0x82 extended data is 396 bytes
    static Packet extendedData130(const uint8_t* data396);

    // sub 47B7D0 0x83 extended data is 396 bytes
    static Packet extendedData131(const uint8_t* data396);

    /// friend or buddy entry summary is 0x70 bytes matching sub 47DB60
    struct BuddyEntry {
        int32_t  characterId  = 0;
        int32_t  level        = 0;
        int32_t  rankPoints   = 0;
        int32_t  isOnline     = 0;
        std::u16string name;        // Will be padded to fixed 24 wchars
        int32_t  vehicleTemplateId = 0;
        int32_t  wins         = 0;
        int32_t  losses       = 0;
        int32_t  status       = 0;  // status icon or mood
    };

    /// sub 47DB60 to sub 452180 0xFB appends one entry to the client buddy list
    static Packet buddyListEntry(const BuddyEntry& entry);

    /// 0xFC bulk refresh of buddy status 12 bytes per entry
    static Packet buddyStatusList(const std::vector<std::tuple<int32_t, int32_t, int32_t>>& entries);

    /// 0xCD friend online or offline flip toast
    static Packet friendOnlineFlip(int32_t characterId, bool online);

    /// 0xCF friend stats refresh four int32s
    static Packet friendStatsUpdate(int32_t characterId, int32_t stat1, int32_t stat2, int32_t stat3);

    /// 0xEF friend remove notify
    static Packet friendRemoveNotify(int32_t characterId);

    /// 0xF0 friend record update is a 36 byte blob
    static Packet friendRecordUpdate(const uint8_t* data36);

    /// 0xF3 refreshes the block list 8 bytes per entry sent after block player
    static Packet blockListRefresh(const std::vector<std::pair<int32_t, int32_t>>& blocked);

    /// 0x96 gift inbox refresh for received count then that many 0xD4 byte records
    static Packet giftInboxList(const std::vector<std::array<uint8_t, 0xD4>>& gifts);

    /// 0x97 gift inbox refresh for the sent archive
    static Packet giftInboxListSent(const std::vector<std::array<uint8_t, 0xD4>>& gifts);

    // chibikart catalogs never sent kept our screens dead layouts documented in docs packets CHIBIKART GAP empty count is valid
    static Packet emptyAck(uint16_t cmd);
    static Packet petCatalog(int32_t count = 0);        ///< emptyAck used for 0x10E and 0x11D petCatalog is 0x103 pet 01 pet 10 title
    static Packet skyCatalog(int32_t count = 0);
    static Packet factoryCatalog(int32_t count = 0);    ///< skyCatalog is 0x10C sky01 sky 01 title factoryCatalog is 0x107 Factory Car
    static Packet itemCatalog(int32_t count = 0);
    static Packet licenseCatalog(int32_t count = 0);    ///< itemCatalog is 0xC1 gacha coin licenseCatalog is 0xC3 license 01
    static Packet licenseRank(int32_t count = 0);
    static Packet giftInboxAll(int32_t count = 0);      ///< licenseRank is 0xC5 license r 00 giftInboxAll is 0x95 the 212 byte list
    static Packet entityList260(int32_t count = 0);
    static Packet entityList263(int32_t count = 0);     ///< entityList260 is 0x104 entityList263 is 0x107 sibling entityList269 is 0x10D
    static Packet entityList269(int32_t count = 0);

    /// 0x98 gift confirm toast sender id recipient id and a 0xD4 byte payload matching the shared mailbox record layout
    static std::array<uint8_t, 0xD4> giftRecord(int32_t id, int32_t category,
                                                int32_t senderId,
                                                const std::string& senderName,
                                                const std::string& date,
                                                const std::string& time,
                                                const std::string& message);

    /// s2c 0x116 one line shown against a player only eight free slots exist a ninth is dropped until one clears
    static Packet playerNotice(int32_t playerId, const std::u16string& text, uint8_t type);

    /// s2c 0x99 appends one record to the open mailbox no count no clear
    static Packet giftArrived(const std::array<uint8_t, 0xD4>& rec);

    static Packet giftSendConfirm(int32_t senderId, int32_t recipientId,
                                  const uint8_t* gift212);

    /// 0xAB gacha banner list header resets the client cursor
    static Packet gachaBannerListHeader(int32_t totalBanners);

    /// 0xAC one banner entry plus its child prize records
    static Packet gachaBannerEntry(const uint8_t* banner176,
                                   const std::vector<std::array<uint8_t, 0xB0>>& children);

    /// 0xB1 gacha roll payout is the main prize plus up to three bonus prizes
    static Packet gachaRollPayout(int32_t headerId,
                                  const uint8_t* mainPrize176,
                                  const std::vector<std::array<uint8_t, 0xB0>>& bonusPrizes);

    /// 0x134 final gacha result positive is a prize id negative 700 800 or 900 are miss classes
    static Packet gachaTransactionResult(int32_t resultCode,
                                         int32_t header[7],
                                         int32_t type,
                                         const uint8_t* typeData,
                                         size_t typeLen);

    /// true when the burst already carries this opcode so a stub never rides after the real one
    static bool burstHasOpcode(const std::vector<Packet>& burst, uint16_t opcode);

private:
    static void writeWString(Packet& pkt, const std::u16string& str);
    static void writePlayerInfo(Packet& pkt, const PlayerData& player);
};

} // namespace knc
