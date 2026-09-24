/// keepalive delayed-ack and anticheat probe wire builders

#pragma once
#include "net/Packet.h"
#include "net/Protocol.h"
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// S2C 0x4D triggers the client 276 byte fingerprint reply S2C 0x12E arms file integrity checks
struct KeepaliveAnticheatPackets {

    /// char 12 NUL padded ascii path FUN 00403AD0 reads it as a cstr so it needs a terminator
    static constexpr size_t FILE_CHECK_PATH_SLOT = 12;

    /// one 16 byte record 12 byte path plus u32 expected checksum
    struct FileCheckRecord {
        std::string relativePath;       ///< relativePath clamped 11 chars plus NUL example pak001 dat checksum vs client FUN 00403A10
        uint32_t expectedChecksum = 0;
    };

    /// S2C 0x4D zero payload send once per session after the login burst
    static Packet clientInfoProbe();

    /// S2C 0x12E file integrity manifest unwired since FUN 00403A10 checksum is unconfirmed
    static Packet clientFileCheck(const std::vector<FileCheckRecord>& records);

    /// loads client file manifest unused until clientFileCheck is wired up
    static std::vector<FileCheckRecord> loadFileManifest();
};

} // namespace knc
