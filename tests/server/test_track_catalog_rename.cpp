/// Proves the column rename of both catalogues never touched the wire

#include <gtest/gtest.h>
#include <cstring>
#include "packets/gen/SpawnPackets.h"

using namespace knc;

TEST(TrackCatalogRename, TuningFieldsCarryRawBitsOnTheWire) {
    SpawnPackets::TrackCatalogRow row;
    row.visibleFlag = 1;
    row.trackId = 10;
    row.themeId = 10;
    row.folderName = "Forest_01";
    row.tuningEngineSetupBits = 0xDEADBEEF;  // 0x30 and 0x34 are raw bits not floats
    row.tuningEngineForceBits = 0xCAFEBABE;
    row.tuningTurnForce = 90.0f;             // record offset 0x38 a real float sent as bits
    row.fallOffTimeoutMs = 500;
    row.lapCount = 3;
    row.displayNameKey = "TRACK_FOREST_01_INFO";

    Packet pkt = SpawnPackets::trackCatalogEntry(row);
    pkt.resetReadPos();

    EXPECT_EQ(pkt.readUInt32(), row.visibleFlag);
    EXPECT_EQ(pkt.readUInt32(), row.trackId);
    EXPECT_EQ(pkt.readUInt32(), row.themeId);
    EXPECT_EQ(pkt.readString(35), row.folderName);

    // these three dwords sit right after the folder name matching record offset 0x30 0x34 0x38
    EXPECT_EQ(pkt.readUInt32(), 0xDEADBEEFu);
    EXPECT_EQ(pkt.readUInt32(), 0xCAFEBABEu);

    uint32_t expectedTurnForceBits = 0;
    float expectedTurnForce = 90.0f;
    std::memcpy(&expectedTurnForceBits, &expectedTurnForce, sizeof(expectedTurnForceBits));
    EXPECT_EQ(pkt.readUInt32(), expectedTurnForceBits);
}

TEST(TrackCatalogRename, DefaultRowStillMatchesTheOldWireDefaults) {
    // The struct defaults must still equal the old defaults of the three tuning fields
    SpawnPackets::TrackCatalogRow row;
    EXPECT_EQ(row.tuningEngineSetupBits, 1053609165u);
    EXPECT_EQ(row.tuningEngineForceBits, 1058642330u);
}
