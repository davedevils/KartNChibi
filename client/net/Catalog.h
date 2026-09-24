// the catalogue rows of the login burst drivers karts parts items prices licence tests and the owned lists
#pragma once

#include "net/Packet.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

// one price option of a catalogue row the inline list every def carries
struct PriceOption {
    uint32_t priceKey = 0;
    uint32_t periodMode = 0;
    uint32_t periodValue = 0;
    uint32_t active = 0;
};

// one 0x00C6 price row the shop prints the triple from it
struct PriceRow {
    uint32_t key = 0;
    uint32_t unitType = 0;
    uint32_t unitAmount = 0;
    uint32_t priceBase = 0;
    uint32_t priceSale = 0;
};

// one 0x00BF driver row the asset names the Driver Body High folder
struct DriverRow {
    uint32_t key = 0;
    uint32_t badge = 0;
    uint32_t requiredLevel = 0;
    std::string asset;
    std::string nameKey;
    std::string descKey;
    std::array<uint32_t, 5> costume{};
    std::vector<PriceOption> prices;
    bool visible = false;
    bool creationPick = false;
};

// one 0x00C0 kart row the model names the Car folder and the Car Body High folder
struct KartRow {
    uint32_t key = 0;
    uint32_t badge = 0;
    uint32_t vehicleKind = 0;
    uint32_t modelScheme = 0;
    uint32_t requiredLevel = 0;
    std::string model;
    std::string nameKey;
    std::string descKey;
    std::array<uint32_t, 8> skins{};
    std::array<float, 17> stats{};
    // the two ability pairs of record 0x130 and 0x138 an id off 0 to 25 hides the pair
    std::array<int32_t, 2> abilityId{-1, -1};
    std::array<uint32_t, 2> abilityPercent{};
    std::vector<PriceOption> prices;
    bool visible = false;
};

// one 0x00C1 item row the icon names the Parts image
struct ItemRow {
    uint32_t key = 0;
    uint32_t badge = 0;
    uint32_t useType = 0;
    uint32_t requiredLevel = 0;
    std::string icon;
    std::string nameKey;
    std::string descKey;
    std::vector<PriceOption> prices;
    bool visible = false;
};

// one 0x00C2 kart part row the equip slot picks the tab and the owned slot
struct PartRow {
    uint32_t key = 0;
    uint32_t badge = 0;
    uint32_t requiredLevel = 0;
    std::string model;
    uint32_t restrictTarget = 0;
    uint32_t equipSlot = 0;
    uint32_t restrictKey = 0xFFFFFFFFu;
    std::string nameKey;
    std::string descKey;
    std::vector<PriceOption> prices;
    bool visible = false;
};

// one 0x0103 pet row the shop pet tab draws
struct PetRow {
    uint32_t key = 0;
    uint32_t badge = 0;
    uint32_t requiredPendant = 0;
    std::string model;
    std::string nameKey;
    std::string descKey;
    std::vector<PriceOption> prices;
    bool visible = false;
};

// one 0x00C3 track row the folder is the second World folder the theme row gives the first
struct TrackRow {
    uint32_t trackId = 0;
    uint32_t themeId = 0;
    std::string folder;
    float tuning[3] = {0.f, 0.f, 0.f};
    uint32_t fallOffTimeoutMs = 500;
    uint32_t difficulty = 0;
    uint32_t requiredLicense = 0;
    uint32_t specialModeOnly = 0;
    uint32_t lapCount = 3;
    float fogFar = 0.f;
    std::string nameKey;
    bool visible = false;
};

// one 0x00C5 licence test definition the key is tier times ten plus test
struct LicenceTestDef {
    uint32_t key = 0;
    // record 0x2C echoed as the second dword of C2S 0x00A3
    uint32_t testParam = 0;
    uint32_t rewardExp = 0;
    uint32_t rewardGold = 0;
    // 0 driver 1 kart 2 item 3 part 4 pet 5 room object 6 carcraft part
    uint32_t rewardItemType = 0;
    uint32_t rewardItemKey = 0;
    float fogNear = 0.f;
    float fogFar = 0.f;
    float lensFlare[3] = {0.f, 0.f, 0.f};
};

// one 0x00C4 theme row
struct ThemeRow {
    uint32_t themeId = 0;
    std::string folder;
    std::string nameKey;
};

// one 0x010C room craft object row the folder names the World Room subfolder of its category
struct RoomObjectRow {
    uint32_t key = 0;
    uint32_t badge = 0;
    // 0 sky 1 floor 2 back object 3 prop 4 effect
    uint32_t category = 0;
    uint32_t maxPlaceable = 0;
    std::string folder;
    std::string nameKey;
    std::string descKey;
    std::vector<PriceOption> prices;
    bool visible = false;
};

// one 0x001B owned character record the five accessory slots are part keys
struct OwnedCharacter {
    uint32_t instance = 0;
    uint32_t driverKey = 0;
    // body face head glass back the 0x00C2 equip slots 2 to 6
    std::array<uint32_t, 5> accessory{};
    uint32_t priceKey = 0;
    uint32_t limitType = 0;
    uint32_t limitValue = 0;
    uint32_t active = 0;
};

// one 0x001C owned kart record the eight dwords overlay the kart definition
struct OwnedKart {
    uint32_t instance = 0;
    uint32_t kartKey = 0;
    // slot 0 paint slot 1 name plate slot 2 the kart item slot then three spares
    std::array<uint32_t, 6> part{};
    std::array<uint32_t, 2> item{};
    uint32_t priceKey = 0;
    uint32_t expiryKind = 0;
    int32_t durability = 0;
    uint32_t active = 0;
};

// one 0x001D owned consumable record
struct OwnedItem {
    uint32_t instance = 0;
    uint32_t itemKey = 0;
    uint32_t priceKey = 0;
    uint32_t periodMode = 0;
    uint32_t periodValue = 0;
    uint32_t active = 0;
    uint32_t inUse = 0;
};

// one 0x0104 owned pet record the equipped flag is tested against one
struct OwnedPet {
    uint32_t instance = 0;
    uint32_t petKey = 0;
    uint32_t equipped = 0;
    uint32_t priceKey = 0;
    uint32_t periodMode = 0;
    uint32_t periodValue = 0;
    uint32_t active = 0;
};

// one 0x0107 car craft preset the seven slots hold car craft part instance ids
struct CarCraftPreset {
    uint32_t presetId = 0;
    uint32_t slotState = 0;
    std::string name;
    uint32_t kartInstance = 0;
    // cover tires booster bumper front fender rear fender wing
    std::array<uint32_t, 7> slot{};
};

// one 0x0108 car craft part definition the model dir names the Parts icon
struct CarCraftPartDef {
    uint32_t enabled = 0;
    uint32_t key = 0;
    // 0 cover 1 booster 2 tires 3 front fender 4 rear fender 5 bumper 6 wing
    uint32_t category = 0;
    std::string model;
    std::string nameKey;
    std::string descKey;
    std::vector<PriceOption> prices;
};

// one 0x0109 owned car craft part the whole 0x84 blob is kept the save echoes it back
struct CarCraftPartInstance {
    uint32_t instance = 0;
    uint32_t partKey = 0;
    uint32_t category = 0;
    int32_t refCount = 0;
    int32_t grade = 0;
    std::array<uint8_t, 0x84> raw{};
};

// one 0x010D owned room craft object the same 0x30 layout the 0x010F save carries back
struct RoomCraftInstance {
    uint32_t instance = 0;
    uint32_t objectKey = 0;
    uint32_t category = 0;
    float x = 0.f, y = 0.f, z = 0.f;
    float yaw = 0.f;
    uint32_t placed = 0;
    uint32_t priceKey = 0;
    uint32_t periodMode = 0;
    uint32_t periodValue = 0;
    uint32_t active = 0;
};

// one 0x001E owned part record one packet per row
struct OwnedPart {
    uint32_t instance = 0;
    uint32_t partKey = 0;
    uint32_t priceKey = 0;
    uint32_t periodMode = 0;
    uint32_t periodValue = 0;
    uint32_t active = 0;
};

class Catalog {
public:
    // stores a row when the opcode is one of the catalogue opcodes and returns true
    bool handle(uint16_t opcode, const ::knc::Packet& pkt);
    void clear();

    const DriverRow* driver(uint32_t key) const;
    const KartRow* kart(uint32_t key) const;
    const ItemRow* item(uint32_t key) const;
    const PartRow* part(uint32_t key) const;
    const PetRow* pet(uint32_t key) const;
    const PriceRow* price(uint32_t key) const;
    const TrackRow* track(uint32_t trackId) const;
    const ThemeRow* theme(uint32_t themeId) const;
    const LicenceTestDef* licenceTest(uint32_t key) const;
    const RoomObjectRow* roomObject(uint32_t key) const;
    const OwnedCharacter* ownedCharacter(uint32_t instance) const;
    const OwnedKart* ownedKart(uint32_t instance) const;
    const OwnedPet* ownedPet(uint32_t instance) const;
    const OwnedPet* equippedPet() const;
    const CarCraftPartDef* carCraftPart(uint32_t key) const;
    const CarCraftPartInstance* carCraftInstance(uint32_t instance) const;
    const RoomCraftInstance* roomCraftInstance(uint32_t instance) const;

    const std::vector<DriverRow>& drivers() const { return m_drivers; }
    const std::vector<KartRow>& karts() const { return m_karts; }
    const std::vector<ItemRow>& items() const { return m_items; }
    const std::vector<PartRow>& parts() const { return m_parts; }
    const std::vector<PetRow>& pets() const { return m_pets; }
    const std::vector<PriceRow>& prices() const { return m_prices; }
    const std::vector<TrackRow>& tracks() const { return m_tracks; }
    const std::vector<ThemeRow>& themes() const { return m_themes; }
    const std::vector<LicenceTestDef>& licenceTests() const { return m_licenceTests; }
    const std::vector<RoomObjectRow>& roomObjects() const { return m_roomObjects; }
    const std::vector<OwnedCharacter>& ownedCharacters() const { return m_ownedCharacters; }
    const std::vector<OwnedKart>& ownedKarts() const { return m_ownedKarts; }
    const std::vector<OwnedItem>& ownedItems() const { return m_ownedItems; }
    const std::vector<OwnedPart>& ownedParts() const { return m_ownedParts; }
    const std::vector<OwnedPet>& ownedPets() const { return m_ownedPets; }
    const std::vector<CarCraftPreset>& carCraftPresets() const { return m_carCraftPresets; }
    const std::vector<CarCraftPartDef>& carCraftParts() const { return m_carCraftParts; }
    const std::vector<CarCraftPartInstance>& carCraftInstances() const { return m_carCraftInstances; }
    std::vector<CarCraftPartInstance>& carCraftInstances() { return m_carCraftInstances; }
    const std::vector<RoomCraftInstance>& roomCraftInstances() const { return m_roomCraftInstances; }
    // the master copy the 0x010F dirty diff compares against it is taken on every 0x010D and every ack
    const std::vector<RoomCraftInstance>& roomCraftMaster() const { return m_roomCraftMaster; }
    void takeRoomCraftMaster() { m_roomCraftMaster = m_roomCraftInstances; }
    void putRoomCraftInstance(const RoomCraftInstance& row);
    void putCarCraftPreset(const CarCraftPreset& row);
    void putCarCraftInstance(const CarCraftPartInstance& row);

    // the owned rows the buy ack and the equip ack carry come back in as one record
    void putOwnedCharacter(const OwnedCharacter& row);
    void putOwnedKart(const OwnedKart& row);
    void putOwnedItem(const OwnedItem& row);
    void putOwnedPart(const OwnedPart& row);
    void putOwnedPet(const OwnedPet& row);
    void appendOwnedPart(const OwnedPart& row);
    // the 0x00B8 ack drops every owned row of that base key in the category true when one went
    bool removeOwned(uint32_t category, uint32_t baseKey);
    // reads one owned record out of a packet the caller already placed the read head on
    static OwnedCharacter readOwnedCharacter(::knc::Packet& p);
    static OwnedKart readOwnedKart(::knc::Packet& p);
    static OwnedItem readOwnedItem(::knc::Packet& p);
    static OwnedPart readOwnedPart(::knc::Packet& p);
    static OwnedPet readOwnedPet(::knc::Packet& p);
    static RoomCraftInstance readRoomCraftInstance(::knc::Packet& p);
    static CarCraftPreset readCarCraftPreset(::knc::Packet& p);
    static CarCraftPartInstance readCarCraftInstance(::knc::Packet& p);

    // theme folder then track folder as World names them empty when a row is missing
    std::string trackFolder(uint32_t trackId) const;
    // the tracks a room can pick visible rows with a folder in id order
    std::vector<const TrackRow*> pickableTracks() const;

    // the four garage bars of garage stat bars compute 0x428AB0 as a percent 40 to 100
    std::array<float, 4> statBars(const KartRow& row) const;

private:
    void parseDriver(::knc::Packet& p);
    void parseKart(::knc::Packet& p);
    void parseItem(::knc::Packet& p);
    void parsePart(::knc::Packet& p);
    void parsePet(::knc::Packet& p);
    void parsePrice(::knc::Packet& p);
    void parseTrack(::knc::Packet& p);
    void parseTheme(::knc::Packet& p);
    void parseLicenceTest(::knc::Packet& p);
    void parseRoomObject(::knc::Packet& p);
    void parseOwnedCharacters(::knc::Packet& p);
    void parseOwnedKarts(::knc::Packet& p);
    void parseOwnedItems(::knc::Packet& p);
    void parseOwnedPart(::knc::Packet& p, bool replaceByKey);
    void parseOwnedPets(::knc::Packet& p);
    void parseCarCraftPresets(::knc::Packet& p);
    void parseCarCraftPartDef(::knc::Packet& p);
    void parseCarCraftInstance(::knc::Packet& p);
    void parseRoomCraftInstance(::knc::Packet& p);

    std::vector<DriverRow> m_drivers;
    std::vector<KartRow> m_karts;
    std::vector<ItemRow> m_items;
    std::vector<PartRow> m_parts;
    std::vector<PetRow> m_pets;
    std::vector<PriceRow> m_prices;
    std::vector<TrackRow> m_tracks;
    std::vector<ThemeRow> m_themes;
    std::vector<LicenceTestDef> m_licenceTests;
    std::vector<RoomObjectRow> m_roomObjects;
    std::vector<OwnedCharacter> m_ownedCharacters;
    std::vector<OwnedKart> m_ownedKarts;
    std::vector<OwnedItem> m_ownedItems;
    std::vector<OwnedPart> m_ownedParts;
    std::vector<OwnedPet> m_ownedPets;
    std::vector<CarCraftPreset> m_carCraftPresets;
    std::vector<CarCraftPartDef> m_carCraftParts;
    std::vector<CarCraftPartInstance> m_carCraftInstances;
    std::vector<RoomCraftInstance> m_roomCraftInstances;
    std::vector<RoomCraftInstance> m_roomCraftMaster;
};

}
