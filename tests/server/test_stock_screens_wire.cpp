/// the last stock screens on the wire car craft room craft the random invite and the gacha roll

#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <vector>

#include "net/Packet.h"
#include "net/Protocol.h"
#include "packets/gen/CustomCarPackets.h"
#include "packets/gen/RoomCraftPackets.h"
#include "packets/gen/GachaPetPackets.h"
#include "packets/gen/SocialPackets.h"
#include "packets/gen/InventoryPackets.h"
#include "packets/PacketBuilder.h"

using namespace knc;

namespace {

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

} // namespace

// C2S 0x010B is the only car craft verb on the wire install and remove stay local
TEST(CarCraftWire, SaveRequestRoundTrips) {
    Packet req = Packet::fromCmdFull(CMD::C_CARCRAFT_SAVE);
    // preset kart then cover tire booster bumper front fender rear fender and wing
    req.writeUInt32(7);
    req.writeUInt32(101);
    req.writeUInt32(11);
    req.writeUInt32(12);
    req.writeUInt32(13);
    req.writeUInt32(14);
    req.writeUInt32(15);
    req.writeUInt32(16);
    req.writeUInt32(17);
    // one part record its key category and refcount echo are never trusted
    req.writeUInt32(1);
    req.writeUInt32(12);
    req.writeUInt32(3000);
    req.writeUInt32(2);
    req.writeInt32(5);
    for (int i = 0; i < static_cast<int>(0x84 - 16); ++i) req.writeUInt8(0);

    CarSaveRequest parsed = CustomCarPackets::parseSaveRequest(req);
    ASSERT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.presetId, 7u);
    EXPECT_EQ(parsed.kartInstanceId, 101u);
    EXPECT_EQ(parsed.partTire, 12u);
    EXPECT_EQ(parsed.partWing, 17u);
    ASSERT_EQ(parsed.clientInstanceIds.size(), 1u);
    EXPECT_EQ(parsed.clientInstanceIds[0], 12u);
    EXPECT_EQ(parsed.clientRefcounts[0], 5);
}

// S2C 0x010B answer is 0x30 header then 0x84 per part
TEST(CarCraftWire, SaveResultHeaderAndSize) {
    CarPreset preset;
    preset.presetId = 7;
    preset.slotState = 1;
    preset.kartInstanceId = 101;
    preset.partTire = 12;

    CarPartInstance part;
    part.instanceId = 12;
    part.partKey = 3000;
    part.category = 2;
    part.equipRefcount = 1;

    Packet ack = CustomCarPackets::saveResult(preset, preset.kartInstanceId, {part});
    const std::vector<uint8_t>& p = ack.payload();
    EXPECT_EQ(ack.opcode(), 0x010Bu);
    ASSERT_EQ(p.size(), 0x30u + 0x84u);
    // preset slot state the selected kart that changes the car the part count and the first part
    EXPECT_EQ(u32At(p, 0x00), 7u);
    EXPECT_EQ(u32At(p, 0x04), 1u);
    EXPECT_EQ(u32At(p, 0x08), 101u);
    EXPECT_EQ(u32At(p, 0x2C), 1u);
    EXPECT_EQ(u32At(p, 0x30), 12u);
}

// S2C 0x0107 never carries count zero an empty set becomes the empty slot of the real server
TEST(CarCraftWire, PresetListNeverEmpty) {
    Packet list = CustomCarPackets::presetList({});
    const std::vector<uint8_t>& p = list.payload();
    EXPECT_EQ(list.opcode(), 0x0107u);
    ASSERT_EQ(p.size(), 4u + 0x34u);
    EXPECT_EQ(u32At(p, 0), 1u);
    EXPECT_EQ(u32At(p, 8), 0u);
    EXPECT_EQ(u32At(p, 0x18), 0u);
}

// S2C 0x0109 is a bare 0x84 record no count prefix
TEST(CarCraftWire, PartInstanceIsFixed) {
    CarPartInstance part;
    part.instanceId = 42;
    part.partKey = 1000;
    Packet inst = CustomCarPackets::partInstance(part);
    EXPECT_EQ(inst.opcode(), 0x0109u);
    ASSERT_EQ(inst.payload().size(), 0x84u);
    EXPECT_EQ(u32At(inst.payload(), 0), 42u);
}

// char 7 owns karts 7 12 13 14 a save on kart 12 keeps 12 not the stored preset kart
TEST(CarCraftWire, ValidateSaveKeepsTheClientKartNotTheStoredOne) {
    CarPreset current;
    current.presetId = 6;
    current.kartInstanceId = 7;     // the old value already on file
    current.partTire = 8;

    CarPartInstance tire;
    tire.instanceId = 20;
    tire.category = static_cast<uint32_t>(CarPartCategory::Tire);
    tire.periodActive = 1;

    CarSaveRequest req;
    req.valid = true;
    req.presetId = 6;
    req.kartInstanceId = 12;        // a kart bought after the starter one
    req.partTire = 20;

    CarPreset saved = CustomCarPackets::validateSave(req, current, {tire});
    EXPECT_EQ(saved.kartInstanceId, 12u);
    EXPECT_EQ(saved.partTire, 20u);
}

// a save naming a part the character does not own must still lose that slot
TEST(CarCraftWire, ValidateSaveDropsAnUnownedPart) {
    CarPreset current;
    current.kartInstanceId = 7;
    current.partTire = 8;

    CarSaveRequest req;
    req.valid = true;
    req.kartInstanceId = 7;
    req.partTire = 999;             // not in the owned list below

    CarPreset saved = CustomCarPackets::validateSave(req, current, {});
    EXPECT_EQ(saved.partTire, current.partTire);   // kept the old tire not the bad one
}

// C2S 0x010F the bottom bar Save the server never answered before now parses to the held ids
TEST(RoomCraftWire, SaveRequestParses) {
    RoomObjectInstance a;
    a.instanceId = 501;
    a.objectKey = 4001;
    a.category = 3;
    a.pos0 = 1.0f;
    a.placedFlag = 1;
    a.activeFlag = 1;
    const auto rec = RoomCraftPackets::decorRecord(a);

    std::vector<uint8_t> body;
    body.resize(4);
    body[0] = 1;                       // count one
    body.insert(body.end(), rec.begin(), rec.end());

    std::vector<std::array<uint8_t, RoomCraftPackets::RECORD_SIZE>> raw;
    std::vector<RoomObjectInstance> recs;
    ASSERT_TRUE(RoomCraftPackets::parseSaveRequest(body, raw, recs));
    ASSERT_EQ(recs.size(), 1u);
    EXPECT_EQ(recs[0].instanceId, 501u);
    EXPECT_EQ(recs[0].objectKey, 4001u);
    EXPECT_EQ(recs[0].placedFlag, 1u);
}

// S2C 0x010F ack is count then 0x30 per record and the empty ack is four bytes
TEST(RoomCraftWire, SaveAckShapes) {
    RoomObjectInstance a;
    a.instanceId = 501;
    const auto rec = RoomCraftPackets::decorRecord(a);
    std::vector<std::array<uint8_t, RoomCraftPackets::RECORD_SIZE>> raw{rec};

    Packet echo = RoomCraftPackets::saveAckEcho(raw);
    EXPECT_EQ(echo.opcode(), 0x010Fu);
    ASSERT_EQ(echo.payload().size(), 4u + 0x30u);
    EXPECT_EQ(u32At(echo.payload(), 0), 1u);
    EXPECT_EQ(u32At(echo.payload(), 4), 501u);

    Packet empty = RoomCraftPackets::saveAckEmpty();
    EXPECT_EQ(empty.opcode(), 0x010Fu);
    ASSERT_EQ(empty.payload().size(), 4u);
    EXPECT_EQ(u32At(empty.payload(), 0), 0u);
}

// S2C 0x010C object def one price slot header fields first
TEST(RoomCraftWire, ObjectDefinitionHeader) {
    RoomObjectDef def;
    def.objectKey = 2003;
    def.enabled = 1;
    def.badge = 0;
    def.category = 1;             // floor
    def.maxPlaceable = 1;
    def.assetFolder = "Floor03";
    def.nameLocKey = "FLOOR_03_TITLE";
    def.descLocKey = "FLOOR_03_INFO";
    RoomObjectPrice price;
    price.currencyKey = 2006;
    def.prices.push_back(price);

    Packet frame = RoomCraftPackets::objectDefinition(def);
    const std::vector<uint8_t>& p = frame.payload();
    EXPECT_EQ(frame.opcode(), 0x010Cu);
    // enabled badge object key category floor and a max placeable that is never zero
    EXPECT_EQ(u32At(p, 0x00), 1u);
    EXPECT_EQ(u32At(p, 0x04), 0u);
    EXPECT_EQ(u32At(p, 0x08), 2003u);
    EXPECT_EQ(u32At(p, 0x0C), 1u);
    EXPECT_EQ(u32At(p, 0x10), 1u);
}

// S2C 0x010D owned instance is a bare 0x30 record
TEST(RoomCraftWire, PlacedObjectIsFixed) {
    RoomObjectInstance a;
    a.instanceId = 777;
    a.objectKey = 1001;
    Packet inst = RoomCraftPackets::placedObject(a);
    EXPECT_EQ(inst.opcode(), 0x010Du);
    ASSERT_EQ(inst.payload().size(), 0x30u);
    EXPECT_EQ(u32At(inst.payload(), 0), 777u);
    EXPECT_EQ(u32At(inst.payload(), 4), 1001u);
}

// S2C 0x012F count zero clears the box any positive count fills name id and message
TEST(InviteWire, PopupShortShapes) {
    Packet clear = PacketBuilder::invitePopupShort(0, u"Isma", 5, u"");
    EXPECT_EQ(clear.opcode(), 0x012Fu);
    ASSERT_EQ(clear.payload().size(), 4u);
    EXPECT_EQ(u32At(clear.payload(), 0), 0u);

    Packet fill = PacketBuilder::invitePopupShort(1, u"Isma", 5, u"come");
    EXPECT_EQ(fill.opcode(), 0x012Fu);
    // 4 gate then wide name plus nul then id then has message then wide message plus nul
    const size_t expected = 4 + 2 * (4 + 1) + 4 + 4 + 2 * (4 + 1);
    ASSERT_EQ(fill.payload().size(), expected);
    EXPECT_EQ(u32At(fill.payload(), 0), 1u);
}

// S2C 0x006C the by name invite reply key is the inviter id the two wide strings frame an ascii slot
TEST(InviteWire, RoomInviteMixedEncoding) {
    SocialPackets::RoomInvite inv;
    inv.replyKey = 4242;
    inv.inviterName = u"Isma";
    inv.roomId = 9;
    inv.roomPassword = u"pw";
    inv.flag = 0;

    Packet frame = SocialPackets::roomInvite(inv);
    const std::vector<uint8_t>& p = frame.payload();
    EXPECT_EQ(frame.opcode(), 0x006Cu);
    EXPECT_EQ(u32At(p, 0), 4242u);       // reply key echoed by the 0x006D answer
    const size_t expected = 4 + 2 * (4 + 1) + 1 + 4 + 4 + 2 * (2 + 1) + 1;
    EXPECT_EQ(p.size(), expected);
}

// C2S 0x006D answer is eight bytes the reply key then the code
TEST(InviteWire, InviteAnswerParses) {
    Packet ans = Packet::fromCmdFull(CMD::C_ROOM_INVITE_ANSWER);
    ans.writeUInt32(4242);
    ans.writeUInt32(SocialPackets::INVITE_ANSWER_SAME_ROOM);

    SocialPackets::RoomInviteAnswer out;
    ASSERT_TRUE(SocialPackets::parseRoomInviteAnswer(ans, out));
    EXPECT_EQ(out.replyKey, 4242u);
    EXPECT_EQ(out.answer, SocialPackets::INVITE_ANSWER_SAME_ROOM);
}

// C2S 0x00ED is the 0x1C ticket row verbatim the play button sends it the init sends nothing
TEST(GachaWire, RollRequestParses) {
    InventoryPackets::ItemRow ticket;
    ticket.instanceId = 900;
    ticket.baseKey = GachaPetPackets::TICKET_BASE_KEY;
    ticket.periodMode = 2;
    ticket.periodValue = 4;
    ticket.activeFlag = 1;
    const auto blob = InventoryPackets::itemBlob(ticket);

    Packet req = Packet::fromCmdFull(CMD::C_GACHA_ROLL);
    req.writeBytes(blob.data(), blob.size());

    GachaPetPackets::GachaRollRequest out;
    ASSERT_TRUE(GachaPetPackets::parseRoll(req, out));
    EXPECT_EQ(out.instanceId, 900u);
    EXPECT_EQ(out.itemBaseKey, GachaPetPackets::TICKET_BASE_KEY);
    EXPECT_EQ(out.remainingRolls, 4);
}

// S2C 0x00ED refusal is header then the echoed ticket category six no tail
TEST(GachaWire, ResultNoPrizeShape) {
    InventoryPackets::ItemRow ticket;
    ticket.instanceId = 900;
    ticket.baseKey = GachaPetPackets::TICKET_BASE_KEY;
    ticket.periodValue = 3;

    Packet res = GachaPetPackets::resultNoPrize(ticket);
    const std::vector<uint8_t>& p = res.payload();
    EXPECT_EQ(res.opcode(), 0x00EDu);
    ASSERT_EQ(p.size(), GachaPetPackets::GACHA_HEADER_SIZE + GachaPetPackets::SIZE_TICKET_REC);
    // category six then the ticket echo id and the ticket base key
    EXPECT_EQ(u32At(p, 0x04), static_cast<uint32_t>(GachaPetPackets::GC_NONE));
    EXPECT_EQ(u32At(p, GachaPetPackets::GACHA_HEADER_SIZE + 0x00), 900u);
    EXPECT_EQ(u32At(p, GachaPetPackets::GACHA_HEADER_SIZE + 0x04), GachaPetPackets::TICKET_BASE_KEY);
}

// the server decrements the echoed ticket the client never does clamped at zero
TEST(GachaWire, TicketDecrementClamps) {
    InventoryPackets::ItemRow full;
    full.periodValue = 2;
    EXPECT_EQ(GachaPetPackets::decrementedTicket(full).periodValue, 1);

    InventoryPackets::ItemRow last;
    last.periodValue = 0;
    EXPECT_EQ(GachaPetPackets::decrementedTicket(last).periodValue, 0);
}

// S2C 0x0104 count then one 0x1C record per owned pet in the order given
TEST(PetListWire, OwnedListHoldsTheGivenRows) {
    InventoryPackets::PetRow a;
    a.instanceId = 501;
    a.baseKey = 10;
    a.equippedFlag = 1;
    InventoryPackets::PetRow b;
    b.instanceId = 502;
    b.baseKey = 20;

    Packet list = InventoryPackets::ownedPetList({a, b});
    const std::vector<uint8_t>& p = list.payload();
    EXPECT_EQ(list.opcode(), 0x0104u);
    ASSERT_EQ(p.size(), 4u + InventoryPackets::SIZE_PET_REC * 2u);
    // row count then row 0 id key and equipped flag then row 1 id and key
    EXPECT_EQ(u32At(p, 0), 2u);
    EXPECT_EQ(u32At(p, 4 + 0x00), 501u);
    EXPECT_EQ(u32At(p, 4 + 0x04), 10u);
    EXPECT_EQ(u32At(p, 4 + 0x08), 1u);
    EXPECT_EQ(u32At(p, 4 + InventoryPackets::SIZE_PET_REC + 0x00), 502u);
    EXPECT_EQ(u32At(p, 4 + InventoryPackets::SIZE_PET_REC + 0x04), 20u);
}

// the login burst must never carry a second empty 0x0104 after the real owned list rode with it
TEST(PetListWire, LoginBurstCarriesExactlyOnePetList) {
    InventoryPackets::PetRow owned;
    owned.instanceId = 7;
    owned.baseKey = 30;

    std::vector<Packet> burst;
    // the real list rode first
    burst.push_back(InventoryPackets::ownedPetList({owned}));
    // the login burst only appends the empty stub when no owned list is already present
    if (!PacketBuilder::burstHasOpcode(burst, CMD::S_ENTITY_DATA_260))
        burst.push_back(PacketBuilder::entityList260());

    size_t petListCount = 0;
    const Packet* found = nullptr;
    for (const Packet& p : burst) {
        if (p.opcode() == 0x0104u) { ++petListCount; found = &p; }
    }
    ASSERT_EQ(petListCount, 1u);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(u32At(found->payload(), 0), 1u);         // the surviving list still holds the real row
    EXPECT_EQ(u32At(found->payload(), 4), 7u);
}

// a brand new character has no owned burst yet the empty stub still fires once
TEST(PetListWire, LoginBurstStubFiresWhenNoOwnedList) {
    std::vector<Packet> burst;
    if (!PacketBuilder::burstHasOpcode(burst, CMD::S_ENTITY_DATA_260))
        burst.push_back(PacketBuilder::entityList260());

    size_t petListCount = 0;
    for (const Packet& p : burst) if (p.opcode() == 0x0104u) ++petListCount;
    EXPECT_EQ(petListCount, 1u);
}
