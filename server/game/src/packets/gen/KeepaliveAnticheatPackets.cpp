#include "packets/gen/KeepaliveAnticheatPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cstdlib>
#include <map>
#include <string>

namespace knc {

namespace {

// no s2c name for 0x4D exists in Protocol h so this stays local
constexpr uint8_t  OP_S_CLIENT_INFO_PROBE = 0x4D;
constexpr uint16_t OP_S_CLIENT_FILE_CHECK = 0x12E;  // CMD S ENTITY DATA 302

using DbRow = std::map<std::string, std::string>;

uint32_t rowU32(const DbRow& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64NoThrow(row, key, 0));
}

std::string rowStr(const DbRow& row, const char* key) {
    return rowStrCore(row, key);
}

} // namespace

Packet KeepaliveAnticheatPackets::clientInfoProbe() {
    // FUN 004790C0 reads nothing before replying receipt of the opcode is the trigger
    return Packet(OP_S_CLIENT_INFO_PROBE);
}

Packet KeepaliveAnticheatPackets::clientFileCheck(const std::vector<FileCheckRecord>& records) {
    Packet pkt = Packet::fromCmdFull(OP_S_CLIENT_FILE_CHECK);
    pkt.writeInt32(static_cast<int32_t>(records.size()));

    for (const auto& r : records) {
        // clamp to 11 chars so the pad byte leaves a terminator for FUN 00403ad0
        const size_t maxChars = FILE_CHECK_PATH_SLOT - 1;
        size_t i = 0;
        for (; i < r.relativePath.size() && i < maxChars; ++i) {
            pkt.writeUInt8(static_cast<uint8_t>(r.relativePath[i]));
        }
        for (; i < FILE_CHECK_PATH_SLOT; ++i) {
            pkt.writeUInt8(0);
        }
        pkt.writeUInt32(r.expectedChecksum);
    }

    const size_t expected = 4 + FILE_CHECK_PATH_SLOT * records.size() + 4 * records.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "clientFileCheck size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

std::vector<KeepaliveAnticheatPackets::FileCheckRecord> KeepaliveAnticheatPackets::loadFileManifest() {
    std::vector<FileCheckRecord> out;

    auto rows = Database::instance().queryPrepared(
        "SELECT relative_path, expected_checksum FROM client_file_manifest ORDER BY id",
        {});

    out.reserve(rows.size());
    for (const auto& row : rows) {
        FileCheckRecord r;
        r.relativePath = rowStr(row, "relative_path");
        r.expectedChecksum = rowU32(row, "expected_checksum");
        out.push_back(std::move(r));
    }

    LOG_INFO("PACKET", "loaded " + std::to_string(out.size()) +
                       " client_file_manifest rows (unused, clientFileCheck has no send site)");
    return out;
}

} // namespace knc
