#include "Catalog.h"

#include <cstring>

namespace KnC::Client {

using ::knc::Packet;

namespace {

// every read is guarded so a short or custom frame never throws
struct Reader {
    Packet p;
    explicit Reader(const Packet& src) : p(src) { p.resetReadPos(); }
    uint32_t u32() { return p.remaining() >= 4 ? p.readUInt32() : 0; }
    int32_t i32() { return p.remaining() >= 4 ? p.readInt32() : 0; }
    uint8_t u8() { return p.remaining() >= 1 ? p.readUInt8() : 0; }
    float f32() { return p.remaining() >= 4 ? p.readFloat() : 0.f; }
    std::string str(size_t cap) { return p.remaining() ? p.readString(cap) : std::string(); }
    void skip(size_t n) { if (p.remaining() >= n) p.readBytes(n); else p.readBytes(p.remaining()); }
    size_t left() const { return p.remaining(); }
    // the inline price option list cap four rows extras are consumed then dropped
    std::vector<PriceOption> priceList() {
        std::vector<PriceOption> out;
        const uint32_t count = u32();
        for (uint32_t i = 0; i < count && left() >= 16; ++i) {
            PriceOption o;
            o.priceKey = u32();
            o.periodMode = u32();
            o.periodValue = u32();
            o.active = u32();
            if (out.size() < 4) out.push_back(o);
        }
        return out;
    }
};

float bitsToFloat(uint32_t bits) {
    float f = 0.f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

template <class Row, class Key>
void upsert(std::vector<Row>& rows, const Row& row, Key Row::*key) {
    for (Row& r : rows) {
        if (r.*key == row.*key) { r = row; return; }
    }
    rows.push_back(row);
}

// the bar constants of garage stat bars compute 0x428AB0
constexpr float kBarSpan = 0.2f;
constexpr float kBarCeil = 0.3f;
constexpr float kBarFloor = 0.6f;
constexpr float kBarLow = 40.f;
constexpr float kBarHigh = 100.f;

}

bool Catalog::handle(uint16_t opcode, const Packet& pkt) {
    Packet copy = pkt;
    switch (opcode) {
    case 0x00BF: parseDriver(copy); return true;
    case 0x00C0: parseKart(copy); return true;
    case 0x00C1: parseItem(copy); return true;
    case 0x00C2: parsePart(copy); return true;
    case 0x00C3: parseTrack(copy); return true;
    case 0x00C4: parseTheme(copy); return true;
    case 0x00C5: parseLicenceTest(copy); return true;
    case 0x00C6: parsePrice(copy); return true;
    case 0x0103: parsePet(copy); return true;
    case 0x010C: parseRoomObject(copy); return true;
    case 0x0104: parseOwnedPets(copy); return true;
    case 0x0107: parseCarCraftPresets(copy); return true;
    case 0x0108: parseCarCraftPartDef(copy); return true;
    case 0x0109: parseCarCraftInstance(copy); return true;
    case 0x010D: parseRoomCraftInstance(copy); return true;
    case 0x001B: parseOwnedCharacters(copy); return true;
    case 0x001C: parseOwnedKarts(copy); return true;
    case 0x001D: parseOwnedItems(copy); return true;
    case 0x001E: parseOwnedPart(copy, true); return true;
    case 0x009F: parseOwnedPart(copy, false); return true;
    default: return false;
    }
}

void Catalog::clear() {
    m_drivers.clear();
    m_karts.clear();
    m_items.clear();
    m_parts.clear();
    m_pets.clear();
    m_prices.clear();
    m_tracks.clear();
    m_themes.clear();
    m_licenceTests.clear();
    m_roomObjects.clear();
    m_ownedCharacters.clear();
    m_ownedKarts.clear();
    m_ownedItems.clear();
    m_ownedParts.clear();
    m_ownedPets.clear();
    m_carCraftPresets.clear();
    m_carCraftParts.clear();
    m_carCraftInstances.clear();
    m_roomCraftInstances.clear();
    m_roomCraftMaster.clear();
}

// 0x00BF wire order five u32 cstr five u32 cstr cstr count rows
void Catalog::parseDriver(Packet& p) {
    Reader r(p);
    DriverRow row;
    row.visible = r.u32() != 0;
    row.creationPick = r.u32() != 0;
    row.badge = r.u32();
    row.key = r.u32();
    row.requiredLevel = r.u32();
    row.asset = r.str(36);
    for (uint32_t& c : row.costume) c = r.u32();
    row.nameKey = r.str(33);
    row.descKey = r.str(35);
    row.prices = r.priceList();
    if (row.key == 0) return;
    upsert(m_drivers, row, &DriverRow::key);
}

// 0x00C0 wire order u32 u32 u32 u8 four u32 three cstr eight u32 17 f32 two ability pairs count rows
void Catalog::parseKart(Packet& p) {
    Reader r(p);
    KartRow row;
    row.visible = r.u32() != 0;
    row.badge = r.u32();
    row.key = r.u32();
    r.u8();
    row.vehicleKind = r.u32();
    row.modelScheme = r.u32();
    r.u32();
    row.requiredLevel = r.u32();
    row.model = r.str(33);
    row.nameKey = r.str(33);
    row.descKey = r.str(34);
    for (uint32_t& s : row.skins) s = r.u32();
    for (float& s : row.stats) s = r.f32();
    for (size_t i = 0; i < 2; ++i) {
        row.abilityId[i] = r.i32();
        row.abilityPercent[i] = r.u32();
    }
    row.prices = r.priceList();
    if (row.key == 0) return;
    upsert(m_karts, row, &KartRow::key);
}

// 0x00C1 wire order five u32 three cstr count rows the icon comes before the name
void Catalog::parseItem(Packet& p) {
    Reader r(p);
    ItemRow row;
    row.visible = r.u32() != 0;
    row.badge = r.u32();
    row.key = r.u32();
    row.useType = r.u32();
    row.requiredLevel = r.u32();
    row.icon = r.str(33);
    row.nameKey = r.str(33);
    row.descKey = r.str(34);
    row.prices = r.priceList();
    if (row.key == 0) return;
    upsert(m_items, row, &ItemRow::key);
}

// 0x00C2 wire order four u32 cstr three u32 cstr cstr two pairs count rows
void Catalog::parsePart(Packet& p) {
    Reader r(p);
    PartRow row;
    row.visible = r.u32() != 0;
    row.badge = r.u32();
    row.key = r.u32();
    row.requiredLevel = r.u32();
    row.model = r.str(36);
    row.restrictTarget = r.u32();
    row.equipSlot = r.u32();
    row.restrictKey = r.u32();
    row.nameKey = r.str(33);
    row.descKey = r.str(35);
    r.u32(); r.u32();
    r.u32(); r.u32();
    row.prices = r.priceList();
    if (row.key == 0) return;
    upsert(m_parts, row, &PartRow::key);
}

// 0x0103 wire order four u32 three cstr count rows
void Catalog::parsePet(Packet& p) {
    Reader r(p);
    PetRow row;
    row.visible = r.u32() != 0;
    row.badge = r.u32();
    row.key = r.u32();
    row.requiredPendant = r.u32();
    row.model = r.str(33);
    row.nameKey = r.str(33);
    row.descKey = r.str(34);
    row.prices = r.priceList();
    if (row.key == 0) return;
    upsert(m_pets, row, &PetRow::key);
}

// 0x00C6 is one 28 byte row per packet the two unknown dwords have no reader
void Catalog::parsePrice(Packet& p) {
    Reader r(p);
    PriceRow row;
    row.key = r.u32();
    r.u32();
    r.u32();
    row.unitType = r.u32();
    row.unitAmount = r.u32();
    row.priceBase = r.u32();
    row.priceSale = r.u32();
    if (row.key == 0) return;
    upsert(m_prices, row, &PriceRow::key);
}

// 0x00C3 wire order u32 u32 u32 cstr then 14 u32 then cstr
void Catalog::parseTrack(Packet& p) {
    Reader r(p);
    TrackRow row;
    row.visible = r.u32() != 0;
    row.trackId = r.u32();
    row.themeId = r.u32();
    row.folder = r.str(36);
    row.tuning[0] = bitsToFloat(r.u32());
    row.tuning[1] = bitsToFloat(r.u32());
    row.tuning[2] = bitsToFloat(r.u32());
    r.u32();
    row.fallOffTimeoutMs = r.u32();
    row.difficulty = r.u32();
    row.requiredLicense = r.u32();
    row.specialModeOnly = r.u32();
    row.lapCount = r.u32();
    r.u32();
    row.fogFar = bitsToFloat(r.u32());
    for (float& f : row.lensFlare) f = bitsToFloat(r.u32());
    row.nameKey = r.str(36);
    if (row.lapCount < 1) row.lapCount = 1;
    if (row.lapCount > 9) row.lapCount = 9;
    if (row.trackId == 0) return;
    upsert(m_tracks, row, &TrackRow::trackId);
}

// 0x00C4 wire order u32 u32 cstr cstr
void Catalog::parseTheme(Packet& p) {
    Reader r(p);
    ThemeRow row;
    r.u32();
    row.themeId = r.u32();
    row.folder = r.str(33);
    row.nameKey = r.str(35);
    if (row.themeId == 0) return;
    upsert(m_themes, row, &ThemeRow::themeId);
}

// 0x00C5 wire order u32 u32 cstr thirteen u32 cstr cstr the page names the readers of each dword
void Catalog::parseLicenceTest(Packet& p) {
    Reader r(p);
    LicenceTestDef row;
    r.u32();
    row.key = r.u32();
    r.str(36);
    row.testParam = r.u32();
    row.rewardExp = r.u32();
    row.rewardGold = r.u32();
    r.u32();
    row.rewardItemType = r.u32();
    row.rewardItemKey = r.u32();
    r.u32();
    r.u32();
    row.fogNear = bitsToFloat(r.u32());
    row.fogFar = bitsToFloat(r.u32());
    for (float& f : row.lensFlare) f = bitsToFloat(r.u32());
    r.str(33);
    r.str(35);
    upsert(m_licenceTests, row, &LicenceTestDef::key);
}

// 0x010C wire order six u32 three cstr then the price list the folder is the asset name
void Catalog::parseRoomObject(Packet& p) {
    Reader r(p);
    RoomObjectRow row;
    row.visible = r.u32() != 0;
    row.badge = r.u32();
    row.key = r.u32();
    row.category = r.u32();
    row.maxPlaceable = r.u32();
    r.u32();
    row.folder = r.str(33);
    row.nameKey = r.str(33);
    row.descKey = r.str(34);
    row.prices = r.priceList();
    if (row.key == 0) return;
    upsert(m_roomObjects, row, &RoomObjectRow::key);
}

namespace {

// these read at the current position of the packet the caller owns the head
uint32_t take32(Packet& p) { return p.remaining() >= 4 ? p.readUInt32() : 0u; }

}

OwnedCharacter Catalog::readOwnedCharacter(Packet& p) {
    OwnedCharacter rec;
    rec.instance = take32(p);
    rec.driverKey = take32(p);
    for (uint32_t& a : rec.accessory) a = take32(p);
    rec.priceKey = take32(p);
    rec.limitType = take32(p);
    rec.limitValue = take32(p);
    rec.active = take32(p);
    return rec;
}

OwnedKart Catalog::readOwnedKart(Packet& p) {
    OwnedKart rec;
    rec.instance = take32(p);
    rec.kartKey = take32(p);
    for (uint32_t& s : rec.part) s = take32(p);
    for (uint32_t& s : rec.item) s = take32(p);
    rec.priceKey = take32(p);
    rec.expiryKind = take32(p);
    rec.durability = static_cast<int32_t>(take32(p));
    rec.active = take32(p);
    return rec;
}

OwnedItem Catalog::readOwnedItem(Packet& p) {
    OwnedItem rec;
    rec.instance = take32(p);
    rec.itemKey = take32(p);
    rec.priceKey = take32(p);
    rec.periodMode = take32(p);
    rec.periodValue = take32(p);
    rec.active = take32(p);
    rec.inUse = take32(p);
    return rec;
}

OwnedPart Catalog::readOwnedPart(Packet& p) {
    OwnedPart rec;
    rec.instance = take32(p);
    rec.partKey = take32(p);
    take32(p);
    rec.priceKey = take32(p);
    rec.periodMode = take32(p);
    rec.periodValue = take32(p);
    rec.active = take32(p);
    return rec;
}

OwnedPet Catalog::readOwnedPet(Packet& p) {
    OwnedPet rec;
    rec.instance = take32(p);
    rec.petKey = take32(p);
    rec.equipped = take32(p);
    rec.priceKey = take32(p);
    rec.periodMode = take32(p);
    rec.periodValue = take32(p);
    rec.active = take32(p);
    return rec;
}

// the 0x30 record of 0x010D and of both legs of 0x010F
RoomCraftInstance Catalog::readRoomCraftInstance(Packet& p) {
    RoomCraftInstance rec;
    rec.instance = take32(p);
    rec.objectKey = take32(p);
    rec.category = take32(p);
    rec.x = bitsToFloat(take32(p));
    rec.y = bitsToFloat(take32(p));
    rec.z = bitsToFloat(take32(p));
    rec.yaw = bitsToFloat(take32(p));
    rec.placed = take32(p);
    rec.priceKey = take32(p);
    rec.periodMode = take32(p);
    rec.periodValue = take32(p);
    rec.active = take32(p);
    return rec;
}

// the 0x34 preset record of 0x0107 the name is a fixed twelve byte ascii slot
CarCraftPreset Catalog::readCarCraftPreset(Packet& p) {
    CarCraftPreset rec;
    rec.presetId = take32(p);
    rec.slotState = take32(p);
    const std::vector<uint8_t> name = p.remaining() >= 12 ? p.readBytes(12) : std::vector<uint8_t>(12, 0);
    for (uint8_t c : name) {
        if (c == 0) break;
        rec.name.push_back(static_cast<char>(c));
    }
    rec.kartInstance = take32(p);
    for (uint32_t& s : rec.slot) s = take32(p);
    return rec;
}

// the 0x84 part instance of 0x0109 kept whole so the 0x010B save ships it back
CarCraftPartInstance Catalog::readCarCraftInstance(Packet& p) {
    CarCraftPartInstance rec;
    if (p.remaining() < 0x84) return rec;
    const std::vector<uint8_t> blob = p.readBytes(0x84);
    for (size_t i = 0; i < rec.raw.size(); ++i) rec.raw[i] = blob[i];
    auto at = [&](size_t off) {
        return static_cast<uint32_t>(blob[off]) | (static_cast<uint32_t>(blob[off + 1]) << 8) |
               (static_cast<uint32_t>(blob[off + 2]) << 16) | (static_cast<uint32_t>(blob[off + 3]) << 24);
    };
    rec.instance = at(0x00);
    rec.partKey = at(0x04);
    rec.category = at(0x08);
    rec.refCount = static_cast<int32_t>(at(0x0C));
    rec.grade = static_cast<int32_t>(at(0x80));
    return rec;
}

// 0x0104 is a full replace of count records of 0x1C bytes cap 64
void Catalog::parseOwnedPets(Packet& p) {
    p.resetReadPos();
    const int32_t count = p.remaining() >= 4 ? p.readInt32() : 0;
    m_ownedPets.clear();
    for (int32_t i = 0; i < count && p.remaining() >= 0x1C; ++i) m_ownedPets.push_back(readOwnedPet(p));
}

// 0x0107 is a full replace of count preset records of 0x34 bytes cap 5
void Catalog::parseCarCraftPresets(Packet& p) {
    p.resetReadPos();
    const int32_t count = p.remaining() >= 4 ? p.readInt32() : 0;
    m_carCraftPresets.clear();
    for (int32_t i = 0; i < count && p.remaining() >= 0x34; ++i) m_carCraftPresets.push_back(readCarCraftPreset(p));
}

// 0x0108 five u32 three cstr a 0x60 tail then the price list the stock appends blind
void Catalog::parseCarCraftPartDef(Packet& p) {
    Reader r(p);
    CarCraftPartDef row;
    row.enabled = r.u32();
    r.u32();
    row.key = r.u32();
    row.category = r.u32();
    r.u32();
    row.model = r.str(33);
    row.nameKey = r.str(33);
    row.descKey = r.str(34);
    r.skip(0x60);
    row.prices = r.priceList();
    if (row.key == 0) return;
    upsert(m_carCraftParts, row, &CarCraftPartDef::key);
}

// 0x0109 is one 0x84 owned part instance with no count prefix
void Catalog::parseCarCraftInstance(Packet& p) {
    p.resetReadPos();
    if (p.remaining() < 0x84) return;
    putCarCraftInstance(readCarCraftInstance(p));
}

// 0x010D is one 0x30 owned room object with no count prefix the stock appends blind
void Catalog::parseRoomCraftInstance(Packet& p) {
    p.resetReadPos();
    if (p.remaining() < 0x30) return;
    putRoomCraftInstance(readRoomCraftInstance(p));
    m_roomCraftMaster = m_roomCraftInstances;
}

void Catalog::putRoomCraftInstance(const RoomCraftInstance& row) {
    upsert(m_roomCraftInstances, row, &RoomCraftInstance::instance);
}

void Catalog::putCarCraftPreset(const CarCraftPreset& row) {
    upsert(m_carCraftPresets, row, &CarCraftPreset::presetId);
}

void Catalog::putCarCraftInstance(const CarCraftPartInstance& row) {
    upsert(m_carCraftInstances, row, &CarCraftPartInstance::instance);
}

void Catalog::putOwnedPet(const OwnedPet& row) {
    upsert(m_ownedPets, row, &OwnedPet::instance);
}

const OwnedPet* Catalog::ownedPet(uint32_t instance) const {
    for (const OwnedPet& r : m_ownedPets) if (r.instance == instance) return &r;
    return nullptr;
}

const OwnedPet* Catalog::equippedPet() const {
    for (const OwnedPet& r : m_ownedPets) if (r.equipped == 1) return &r;
    return nullptr;
}

const CarCraftPartDef* Catalog::carCraftPart(uint32_t key) const {
    for (const CarCraftPartDef& r : m_carCraftParts) if (r.key == key) return &r;
    return nullptr;
}

const CarCraftPartInstance* Catalog::carCraftInstance(uint32_t instance) const {
    for (const CarCraftPartInstance& r : m_carCraftInstances) if (r.instance == instance) return &r;
    return nullptr;
}

const RoomCraftInstance* Catalog::roomCraftInstance(uint32_t instance) const {
    for (const RoomCraftInstance& r : m_roomCraftInstances) if (r.instance == instance) return &r;
    return nullptr;
}

// 0x001B is a full replace of count records of 0x2C bytes
void Catalog::parseOwnedCharacters(Packet& p) {
    p.resetReadPos();
    const int32_t count = p.remaining() >= 4 ? p.readInt32() : 0;
    m_ownedCharacters.clear();
    for (int32_t i = 0; i < count && p.remaining() >= 0x2C; ++i)
        m_ownedCharacters.push_back(readOwnedCharacter(p));
}

// 0x001C is a full replace of count records of 0x38 bytes
void Catalog::parseOwnedKarts(Packet& p) {
    p.resetReadPos();
    const int32_t count = p.remaining() >= 4 ? p.readInt32() : 0;
    m_ownedKarts.clear();
    for (int32_t i = 0; i < count && p.remaining() >= 0x38; ++i)
        m_ownedKarts.push_back(readOwnedKart(p));
}

// 0x001D is a full replace of count records of 0x1C bytes
void Catalog::parseOwnedItems(Packet& p) {
    p.resetReadPos();
    const int32_t count = p.remaining() >= 4 ? p.readInt32() : 0;
    m_ownedItems.clear();
    for (int32_t i = 0; i < count && p.remaining() >= 0x1C; ++i)
        m_ownedItems.push_back(readOwnedItem(p));
}

// 0x001E replaces the row with the same base key 0x009F appends blind
void Catalog::parseOwnedPart(Packet& p, bool replaceByKey) {
    if (p.payload().size() < 0x1C) return;
    p.resetReadPos();
    const OwnedPart row = readOwnedPart(p);
    if (replaceByKey) putOwnedPart(row);
    else appendOwnedPart(row);
}

void Catalog::putOwnedCharacter(const OwnedCharacter& row) {
    upsert(m_ownedCharacters, row, &OwnedCharacter::instance);
}

void Catalog::putOwnedKart(const OwnedKart& row) {
    upsert(m_ownedKarts, row, &OwnedKart::instance);
}

void Catalog::putOwnedItem(const OwnedItem& row) {
    upsert(m_ownedItems, row, &OwnedItem::instance);
}

void Catalog::appendOwnedPart(const OwnedPart& row) {
    m_ownedParts.push_back(row);
}

// sub 484690 erases by the base key for the categories 0 to 3 the others remove nothing here
bool Catalog::removeOwned(uint32_t category, uint32_t baseKey) {
    bool removed = false;
    auto drop = [&](auto& rows, auto key) {
        for (size_t i = rows.size(); i > 0; --i) {
            if (rows[i - 1].*key != baseKey) continue;
            rows.erase(rows.begin() + static_cast<long long>(i - 1));
            removed = true;
        }
    };
    switch (category) {
    case 0: drop(m_ownedCharacters, &OwnedCharacter::driverKey); break;
    case 1: drop(m_ownedKarts, &OwnedKart::kartKey); break;
    case 2: drop(m_ownedItems, &OwnedItem::itemKey); break;
    case 3: drop(m_ownedParts, &OwnedPart::partKey); break;
    // sub 484690 removes nothing for the pet and the car craft categories
    default: break;
    }
    return removed;
}

void Catalog::putOwnedPart(const OwnedPart& row) {
    for (OwnedPart& r : m_ownedParts) {
        if (r.partKey != row.partKey) continue;
        r = row;
        return;
    }
    m_ownedParts.push_back(row);
}

const DriverRow* Catalog::driver(uint32_t key) const {
    for (const DriverRow& r : m_drivers) if (r.key == key) return &r;
    return nullptr;
}

const KartRow* Catalog::kart(uint32_t key) const {
    for (const KartRow& r : m_karts) if (r.key == key) return &r;
    return nullptr;
}

const ItemRow* Catalog::item(uint32_t key) const {
    for (const ItemRow& r : m_items) if (r.key == key) return &r;
    return nullptr;
}

const PartRow* Catalog::part(uint32_t key) const {
    for (const PartRow& r : m_parts) if (r.key == key) return &r;
    return nullptr;
}

const PetRow* Catalog::pet(uint32_t key) const {
    for (const PetRow& r : m_pets) if (r.key == key) return &r;
    return nullptr;
}

const PriceRow* Catalog::price(uint32_t key) const {
    for (const PriceRow& r : m_prices) if (r.key == key) return &r;
    return nullptr;
}

const TrackRow* Catalog::track(uint32_t trackId) const {
    for (const TrackRow& r : m_tracks) if (r.trackId == trackId) return &r;
    return nullptr;
}

const ThemeRow* Catalog::theme(uint32_t themeId) const {
    for (const ThemeRow& r : m_themes) if (r.themeId == themeId) return &r;
    return nullptr;
}

const LicenceTestDef* Catalog::licenceTest(uint32_t key) const {
    for (const LicenceTestDef& r : m_licenceTests) if (r.key == key) return &r;
    return nullptr;
}

const RoomObjectRow* Catalog::roomObject(uint32_t key) const {
    for (const RoomObjectRow& r : m_roomObjects) if (r.key == key) return &r;
    return nullptr;
}

const OwnedCharacter* Catalog::ownedCharacter(uint32_t instance) const {
    for (const OwnedCharacter& r : m_ownedCharacters) if (r.instance == instance) return &r;
    return nullptr;
}

const OwnedKart* Catalog::ownedKart(uint32_t instance) const {
    for (const OwnedKart& r : m_ownedKarts) if (r.instance == instance) return &r;
    return nullptr;
}

std::string Catalog::trackFolder(uint32_t trackId) const {
    const TrackRow* t = track(trackId);
    if (!t || t->folder.empty()) return std::string();
    const ThemeRow* th = theme(t->themeId);
    if (!th || th->folder.empty()) return std::string();
    return th->folder + "/" + t->folder;
}

std::vector<const TrackRow*> Catalog::pickableTracks() const {
    std::vector<const TrackRow*> out;
    for (const TrackRow& r : m_tracks) {
        if (!r.visible || r.folder.empty()) continue;
        if (!theme(r.themeId)) continue;
        out.push_back(&r);
    }
    for (size_t i = 1; i < out.size(); ++i) {
        for (size_t j = i; j > 0 && out[j - 1]->trackId > out[j]->trackId; --j) {
            const TrackRow* t = out[j];
            out[j] = out[j - 1];
            out[j - 1] = t;
        }
    }
    return out;
}

// bar one stats 0 and 1 bar two stat 2 bar three stats 8 10 11 bar four stat 3
std::array<float, 4> Catalog::statBars(const KartRow& row) const {
    static const int kIndex[4][3] = {{0, 1, -1}, {2, -1, -1}, {8, 10, 11}, {3, -1, -1}};
    std::array<float, 4> bars{};
    for (int b = 0; b < 4; ++b) {
        auto value = [&](const KartRow& k) {
            float v = 0.f;
            for (int s = 0; s < 3; ++s) if (kIndex[b][s] >= 0) v += k.stats[static_cast<size_t>(kIndex[b][s])];
            return v;
        };
        float low = 0.f;
        float high = 0.f;
        bool first = true;
        for (const KartRow& k : m_karts) {
            const float v = value(k);
            if (first || v > high) high = v;
            if (first || v < low) low = v;
            first = false;
        }
        float scaled = 0.f;
        if (!first && high - low > 0.f) {
            scaled = ((value(row) - low) / (high - low)) * kBarSpan;
            if (scaled > kBarCeil) scaled = kBarCeil;
        }
        float bar = (scaled + kBarFloor) * kBarHigh;
        if (bar > kBarHigh) bar = kBarHigh;
        else if (bar < kBarLow) bar = kBarLow;
        bars[static_cast<size_t>(b)] = bar;
    }
    return bars;
}

}
