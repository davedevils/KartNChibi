/// blob relative offsets of the 1224 byte profile 0x07 0xA7 and 0x0A share see docs packets PROFILE BLOB md

#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {
namespace ProfileBlob {

constexpr size_t kSize       = 1224;
constexpr size_t kTicketId   = 0x000;   ///< account id the client echoes on C2S 0xA7
constexpr size_t kTicketText = 0x004;   ///< session token as wchar the same echo
constexpr size_t kTicketMax  = 32;
constexpr size_t kNickname   = 0x486;
constexpr size_t kNickMax    = 12;
constexpr size_t kBand       = 0x4A0;
constexpr size_t kLevel      = 0x4A1;
constexpr size_t kExpCurrent = 0x4A4;
constexpr size_t kAstro      = 0x4A8;
constexpr size_t kGold       = 0x4AC;
constexpr size_t kSelChar    = 0x4B0;
constexpr size_t kSelKart    = 0x4B4;
constexpr size_t kPendant    = 0x4C4;

inline void putI32(std::vector<uint8_t>& blob, size_t at, int32_t v) {
    if (at + 4 > blob.size()) return;
    const uint32_t u = static_cast<uint32_t>(v);
    blob[at + 0] = static_cast<uint8_t>(u & 0xFF);
    blob[at + 1] = static_cast<uint8_t>((u >> 8) & 0xFF);
    blob[at + 2] = static_cast<uint8_t>((u >> 16) & 0xFF);
    blob[at + 3] = static_cast<uint8_t>((u >> 24) & 0xFF);
}

/// low byte wchar copy capped at maxChars the rest of the field stays zero
inline void putNarrowAsWide(std::vector<uint8_t>& blob, size_t at, const std::string& text, size_t maxChars) {
    for (size_t i = 0; i < text.size() && i < maxChars; ++i) {
        if (at + i * 2 + 1 >= blob.size()) return;
        blob[at + i * 2]     = static_cast<uint8_t>(text[i]);
        blob[at + i * 2 + 1] = 0;
    }
}

/// the head the login server reads back on a channel return account id then token
inline void writeTicket(std::vector<uint8_t>& blob, uint32_t accountId, const std::string& token) {
    putI32(blob, kTicketId, static_cast<int32_t>(accountId));
    putNarrowAsWide(blob, kTicketText, token, kTicketMax);
}

/// level exp astro and gold at the offsets the S2C 0x3C 0x8C and 0xB7 writers also use
inline void writeWallet(std::vector<uint8_t>& blob, int32_t level, int32_t exp, int32_t astro, int32_t gold) {
    if (kLevel < blob.size()) blob[kLevel] = static_cast<uint8_t>(level < 0 ? 0 : (level > 255 ? 255 : level));
    putI32(blob, kExpCurrent, exp);
    putI32(blob, kAstro, astro);
    putI32(blob, kGold, gold);
}

}  // namespace ProfileBlob
}  // namespace knc
