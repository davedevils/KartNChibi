#include "packets/gen/GhostPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace knc {

namespace {

// yaw scale 255 over 360 the inverse is 360 over 255 steer step is 90 degrees over 15 buckets
constexpr float kYawToWire   = 0.70833331f;
constexpr float kYawFromWire = 1.4117647f;
constexpr float kSteerStep   = 6.0f;

// db layer caps a column read at 4096 bytes 128 frames is 3584 bytes so one chunk always fits
constexpr size_t kFramesPerDbChunk = 128;

uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

void putU32LE(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

float readF32LE(const uint8_t* p) {
    float v = 0.0f;
    std::memcpy(&v, p, 4);
    return v;
}

void putF32LE(uint8_t* p, float v) {
    std::memcpy(p, &v, 4);
}

int64_t colI64(const std::map<std::string, std::string>& row, const char* key, int64_t def) {
    return rowInt64NoThrow(row, key, def);
}

// returns a reference so it keeps its own body the shared core returns by value
const std::string& colStr(const std::map<std::string, std::string>& row, const char* key) {
    static const std::string kEmpty;
    const auto it = row.find(key);
    return it == row.end() ? kEmpty : it->second;
}

// db is utf 8 so encode properly instead of dropping the high bytes
std::string toUtf8(const std::u16string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t cp = static_cast<uint32_t>(s[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size()) {
            const uint32_t lo = static_cast<uint32_t>(s[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                ++i;
            }
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::u16string fromUtf8(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const uint8_t b0 = static_cast<uint8_t>(s[i]);
        uint32_t cp = 0;
        size_t extra = 0;
        if (b0 < 0x80)            { cp = b0;         extra = 0; }
        else if ((b0 & 0xE0) == 0xC0) { cp = b0 & 0x1F; extra = 1; }
        else if ((b0 & 0xF0) == 0xE0) { cp = b0 & 0x0F; extra = 2; }
        else if ((b0 & 0xF8) == 0xF0) { cp = b0 & 0x07; extra = 3; }
        else { ++i; continue; }

        if (i + extra >= s.size()) break;
        for (size_t k = 1; k <= extra; ++k) {
            cp = (cp << 6) | (static_cast<uint8_t>(s[i + k]) & 0x3F);
        }
        i += extra + 1;

        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<char16_t>(cp));
        }
    }
    return out;
}

std::string toBlob(const uint8_t* data, size_t len) {
    return std::string(reinterpret_cast<const char*>(data), len);
}

template <size_t N>
void blobToArray(const std::string& blob, std::array<uint8_t, N>& out) {
    out.fill(0);
    const size_t n = blob.size() < N ? blob.size() : N;
    std::memcpy(out.data(), blob.data(), n);
    if (blob.size() != N && !blob.empty()) {
        LOG_WARN("GHOST", "block size " + std::to_string(blob.size()) +
                          " expected " + std::to_string(N));
    }
}

// walk one payload without ever reading past the end
struct Reader {
    const uint8_t* p = nullptr;
    size_t n = 0;
    size_t i = 0;
    bool bad = false;

    Reader(const std::vector<uint8_t>& body) : p(body.data()), n(body.size()) {}

    uint32_t u32() {
        if (i + 4 > n) { bad = true; return 0; }
        const uint32_t v = readU32LE(p + i);
        i += 4;
        return v;
    }
};

// shared body of the two frame count opcodes
Packet buildFrameCount(uint16_t opcode, uint32_t frameCount, const char* tag) {
    Packet pkt(opcode);

    uint32_t count = frameCount;
    if (count > GhostPackets::kMaxFramesArray) {
        LOG_ERROR("PACKET", std::string(tag) + " frame count " + std::to_string(count) +
                            " past array capacity " +
                            std::to_string(GhostPackets::kMaxFramesArray) + " clamped");
        count = GhostPackets::kMaxFramesArray;
    }
    pkt.writeUInt32(count);

    const size_t expected = 4;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(tag) + " size " +
                            std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

// shared body of the two frame append opcodes
Packet buildFrameChunk(uint16_t opcode, const ReplayFrame* frames, size_t count,
                       const char* tag) {
    Packet pkt(opcode);

    size_t n = frames ? count : 0;
    if (n > GhostPackets::kMaxFramesPerChunk) {
        LOG_ERROR("PACKET", std::string(tag) + " chunk " + std::to_string(n) +
                            " frames past the 8192 byte container clamped to " +
                            std::to_string(GhostPackets::kMaxFramesPerChunk));
        n = GhostPackets::kMaxFramesPerChunk;
    }

    pkt.writeUInt32(static_cast<uint32_t>(n));
    for (size_t k = 0; k < n; ++k) {
        const auto raw = GhostPackets::encodeFrame(frames[k]);
        pkt.writeBytes(raw.data(), raw.size());
    }

    const size_t expected = 4 + GhostPackets::kFrameSize * n;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(tag) + " size " +
                            std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

} // namespace

uint8_t GhostPackets::yawToByte(float degrees) {
    float d = std::fmod(degrees, 360.0f);
    if (d < 0.0f) d += 360.0f;
    if (!(d >= 0.0f && d <= 360.0f)) {
        LOG_WARN("PACKET", "ghost yaw not finite forced to zero");
        d = 0.0f;
    }
    const float v = d * kYawToWire;
    return static_cast<uint8_t>(static_cast<uint32_t>(v) & 0xFFu);
}

float GhostPackets::yawFromByte(uint8_t value) {
    return static_cast<float>(value) * kYawFromWire;
}

uint8_t GhostPackets::steerBucket(float steerDegrees) {
    float s = steerDegrees;
    if (!(s >= -kMaxSteerDegrees)) s = -kMaxSteerDegrees;
    if (s > kMaxSteerDegrees) s = kMaxSteerDegrees;
    // negative would make the u64 cast undefined so clamp happens first
    const double bucket = (static_cast<double>(kMaxSteerDegrees) + s) / kSteerStep;
    return static_cast<uint8_t>(static_cast<uint64_t>(bucket) & 0xFull);
}

float GhostPackets::steerDegreesFromBucket(uint8_t bucket) {
    // client decode lands one bucket high and is reproduced not corrected
    return static_cast<float>(bucket & 0x0F) * kSteerStep - kMaxSteerDegrees + kSteerStep;
}

uint8_t GhostPackets::speedBucket(float rawSpeed) {
    float v = rawSpeed - kSpeedFloor;
    if (!(v >= 0.0f)) v = 0.0f;
    if (v > kSpeedSpan) v = kSpeedSpan;
    const double bucket = static_cast<double>(v) / static_cast<double>(kSpeedStep);
    return static_cast<uint8_t>(static_cast<uint64_t>(bucket) & 0xFull);
}

float GhostPackets::speedFromBucket(uint8_t bucket) {
    // client drops the floor on decode so this round trips -1000
    return static_cast<float>(bucket & 0x0F) * kSpeedStep;
}

uint32_t GhostPackets::makeSteerSpeed(uint8_t steer, uint8_t speed) {
    return static_cast<uint32_t>(steer & 0x0F) |
           (static_cast<uint32_t>(speed & 0x0F) << 4);
}

uint8_t GhostPackets::steerOf(uint32_t steerSpeed) {
    return static_cast<uint8_t>(steerSpeed & 0x0F);
}

uint8_t GhostPackets::speedOf(uint32_t steerSpeed) {
    return static_cast<uint8_t>((steerSpeed >> 4) & 0x0F);
}

std::array<uint8_t, GhostPackets::kFrameSize> GhostPackets::encodeFrame(const ReplayFrame& f) {
    std::array<uint8_t, kFrameSize> r{};
    putF32LE(r.data() + 0x00, f.posX);
    putF32LE(r.data() + 0x04, f.posY);
    putF32LE(r.data() + 0x08, f.posZ);
    r[0x0C] = f.yaw;                    // 0x0D to 0x0F stay zero
    putU32LE(r.data() + 0x10, f.flags);
    putU32LE(r.data() + 0x14, f.steerSpeed);
    r[0x18] = f.inputMask;              // 0x19 to 0x1B stay zero
    return r;
}

bool GhostPackets::decodeFrame(const uint8_t* data, size_t len, ReplayFrame& out) {
    if (!data || len < kFrameSize) {
        LOG_WARN("PACKET", "ghost frame short " + std::to_string(len) +
                           " expected " + std::to_string(kFrameSize));
        return false;
    }

    out.posX       = readF32LE(data + 0x00);
    out.posY       = readF32LE(data + 0x04);
    out.posZ       = readF32LE(data + 0x08);
    out.yaw        = data[0x0C];
    out.flags      = readU32LE(data + 0x10);
    out.steerSpeed = readU32LE(data + 0x14);
    out.inputMask  = data[0x18];
    return true;
}

bool GhostPackets::decodeFrames(const uint8_t* data, size_t len, size_t count,
                                std::vector<ReplayFrame>& out) {
    if (!data || len < count * kFrameSize) {
        LOG_WARN("PACKET", "ghost frame run short " + std::to_string(len) +
                           " expected " + std::to_string(count * kFrameSize));
        return false;
    }

    out.clear();
    out.reserve(count);
    for (size_t k = 0; k < count; ++k) {
        ReplayFrame f;
        if (!decodeFrame(data + k * kFrameSize, kFrameSize, f)) return false;
        out.push_back(f);
    }
    return true;
}

std::vector<uint8_t> GhostPackets::encodeFrames(const std::vector<ReplayFrame>& frames) {
    std::vector<uint8_t> out;
    out.reserve(frames.size() * kFrameSize);
    for (const auto& f : frames) {
        const auto raw = encodeFrame(f);
        out.insert(out.end(), raw.begin(), raw.end());
    }
    return out;
}

bool GhostPackets::decodeRep(const uint8_t* data, size_t len, std::vector<ReplayFrame>& out) {
    out.clear();

    if (!data || len < 4) {
        LOG_ERROR("GHOST", "rep image shorter than the count header");
        return false;
    }

    // loader reads the count as signed so a negative one succeeds with no frames
    const int32_t signedCount = static_cast<int32_t>(readU32LE(data));
    if (signedCount <= 0) {
        LOG_WARN("GHOST", "rep count " + std::to_string(signedCount) + " loads empty");
        return true;
    }

    const uint32_t count = static_cast<uint32_t>(signedCount);
    if (count > kMaxFramesArray) {
        LOG_ERROR("GHOST", "rep count " + std::to_string(count) +
                           " past the client array capacity " +
                           std::to_string(kMaxFramesArray) + " refused");
        return false;
    }

    const size_t need = 4 + static_cast<size_t>(count) * kFrameSize;
    if (len < need) {
        LOG_ERROR("GHOST", "rep truncated have " + std::to_string(len) +
                           " need " + std::to_string(need));
        return false;
    }
    if (len != need) {
        LOG_WARN("GHOST", "rep has " + std::to_string(len - need) + " trailing bytes");
    }

    return decodeFrames(data + 4, len - 4, count, out);
}

bool GhostPackets::readRepFile(const std::string& path, std::vector<ReplayFrame>& out) {
    out.clear();

    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        LOG_ERROR("GHOST", "rep open failed " + path);
        return false;
    }

    std::vector<uint8_t> image;
    uint8_t buf[8192];
    while (true) {
        const size_t got = std::fread(buf, 1, sizeof(buf), fp);
        if (got == 0) break;
        image.insert(image.end(), buf, buf + got);
    }
    const bool readErr = std::ferror(fp) != 0;
    std::fclose(fp);

    if (readErr) {
        LOG_ERROR("GHOST", "rep read error " + path);
        return false;
    }
    if (!decodeRep(image.data(), image.size(), out)) {
        LOG_ERROR("GHOST", "rep decode failed " + path);
        return false;
    }

    LOG_INFO("GHOST", "rep loaded " + path + " frames " + std::to_string(out.size()));
    return true;
}

std::vector<uint8_t> GhostPackets::encodeRep(const std::vector<ReplayFrame>& frames) {
    std::vector<uint8_t> out;

    size_t n = frames.size();
    if (n > kMaxFramesArray) {
        LOG_ERROR("GHOST", "rep write " + std::to_string(n) +
                           " frames past capacity truncated to " +
                           std::to_string(kMaxFramesArray));
        n = kMaxFramesArray;
    }

    out.resize(4);
    putU32LE(out.data(), static_cast<uint32_t>(n));
    out.reserve(4 + n * kFrameSize);
    for (size_t k = 0; k < n; ++k) {
        const auto raw = encodeFrame(frames[k]);
        out.insert(out.end(), raw.begin(), raw.end());
    }
    return out;
}

bool GhostPackets::writeRepFile(const std::string& path, const std::vector<ReplayFrame>& frames) {
    // client never wrote a rep so this layout is proven against the loader only
    const std::vector<uint8_t> image = encodeRep(frames);

    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        LOG_ERROR("GHOST", "rep create failed " + path);
        return false;
    }

    const size_t put   = std::fwrite(image.data(), 1, image.size(), fp);
    const bool  closed = (std::fclose(fp) == 0);   // never short circuit past the close
    if (put != image.size() || !closed) {
        LOG_ERROR("GHOST", "rep write failed " + path);
        return false;
    }

    LOG_INFO("GHOST", "rep written " + path + " bytes " + std::to_string(image.size()));
    return true;
}

std::array<uint8_t, GhostPackets::kRecordEntrySize>
GhostPackets::recordEntryBytes(const GhostRecordEntry& e) {
    std::array<uint8_t, kRecordEntrySize> r{};

    std::u16string name = e.name;
    if (name.size() > kRecordNameMaxUnits) {
        LOG_WARN("PACKET", "record name truncated to " +
                           std::to_string(kRecordNameMaxUnits) + " units");
        name.resize(kRecordNameMaxUnits);
    }

    // name buffer starts at 0x04 and the terminator must not reach the time at 0xAC
    for (size_t k = 0; k < name.size(); ++k) {
        const uint16_t c = static_cast<uint16_t>(name[k]);
        r[0x04 + k * 2 + 0] = static_cast<uint8_t>(c & 0xFF);
        r[0x04 + k * 2 + 1] = static_cast<uint8_t>((c >> 8) & 0xFF);
    }

    putU32LE(r.data() + 0xAC, static_cast<uint32_t>(e.timeMs));
    return r;  // 0x00 and 0x08 through 0xAB have no reader so they stay zero
}

Packet GhostPackets::boardHeader(uint32_t trackCount) {
    Packet pkt(kOpBoardHeader);

    uint32_t count = trackCount;
    if (count > kMaxTracks) {
        LOG_ERROR("PACKET", "board header " + std::to_string(count) +
                            " tracks past the client array clamped to " +
                            std::to_string(kMaxTracks));
        count = kMaxTracks;
    }
    pkt.writeUInt32(count);

    const size_t expected = 4;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "boardHeader size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet GhostPackets::boardTrack(uint32_t trackIdEcho, const GhostRecordEntry& best,
                                const std::vector<GhostRecordEntry>& entries) {
    Packet pkt(kOpBoardTrack);

    std::vector<GhostRecordEntry> keep = entries;
    if (keep.size() > kMaxBoardEntries) {
        LOG_WARN("PACKET", "board track drops " +
                           std::to_string(keep.size() - kMaxBoardEntries) +
                           " entries the client list refuses past " +
                           std::to_string(kMaxBoardEntries));
        keep.resize(kMaxBoardEntries);
    }

    pkt.writeUInt32(trackIdEcho);   // client reads it then throws it away

    const auto bestRow = recordEntryBytes(best);
    pkt.writeBytes(bestRow.data(), bestRow.size());

    pkt.writeUInt32(static_cast<uint32_t>(keep.size()));
    for (const auto& e : keep) {
        const auto row = recordEntryBytes(e);
        pkt.writeBytes(row.data(), row.size());
    }

    const size_t expected = 184 + kRecordEntrySize * keep.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "boardTrack size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet GhostPackets::boardOpen() {
    // only opener of the board in the whole binary
    Packet pkt(kOpBoardOpen);

    if (!pkt.payload().empty()) {
        LOG_ERROR("PACKET", "boardOpen size " + std::to_string(pkt.payload().size()) +
                            " expected 0");
    }
    return pkt;
}

Packet GhostPackets::ghostFrameCount(uint32_t frameCount) {
    if (frameCount > kMaxFramesGhost) {
        LOG_WARN("PACKET", "ghost frame count " + std::to_string(frameCount) +
                           " past the recorder cap " + std::to_string(kMaxFramesGhost));
    }
    return buildFrameCount(kOpReplayCount, frameCount, "ghostFrameCount");
}

Packet GhostPackets::ghostFrameChunk(const ReplayFrame* frames, size_t count) {
    return buildFrameChunk(kOpReplayChunk, frames, count, "ghostFrameChunk");
}

std::vector<Packet> GhostPackets::ghostFrameChunks(const std::vector<ReplayFrame>& frames) {
    std::vector<Packet> out;

    // cursor is not bounds checked so an overlong stream lands on the mode field
    if (frames.size() > kMaxFramesArray) {
        LOG_ERROR("PACKET", "ghost replay " + std::to_string(frames.size()) +
                            " frames past capacity " + std::to_string(kMaxFramesArray) +
                            " refused");
        return out;
    }
    if (frames.size() > kMaxFramesGhost) {
        LOG_WARN("PACKET", "ghost replay " + std::to_string(frames.size()) +
                           " frames past the recorder cap " +
                           std::to_string(kMaxFramesGhost));
    }

    for (size_t off = 0; off < frames.size(); off += kChunkFrames) {
        const size_t n = (frames.size() - off) < kChunkFrames
                       ? (frames.size() - off) : kChunkFrames;
        out.push_back(ghostFrameChunk(frames.data() + off, n));
    }
    return out;
}

Packet GhostPackets::ghostSession(const GhostSessionInfo& info) {
    Packet pkt(kOpGhostEnter);

    std::u16string name = info.name;
    if (name.size() > kGhostNameMaxUnits) {
        // dest is 28 bytes then the char block starts so a long name loses its NUL
        LOG_WARN("PACKET", "ghost name truncated to " +
                           std::to_string(kGhostNameMaxUnits) + " units");
        name.resize(kGhostNameMaxUnits);
    }

    uint32_t carKind = info.carKind;
    if (carKind != kCarKindA && carKind != kCarKindB) {
        LOG_WARN("PACKET", "ghost car kind " + std::to_string(carKind) +
                           " forced to " + std::to_string(kCarKindA));
        carKind = kCarKindA;
    }

    pkt.writeUInt32(info.trackId);
    pkt.writeWString(name);
    pkt.writeUInt32(carKind);
    pkt.writeInt32(info.recordTimeMs);   // single reader picks an unreachable string
    pkt.writeBytes(info.charBlock.data(), info.charBlock.size());
    pkt.writeBytes(info.kartBlock.data(), info.kartBlock.size());

    const size_t expected = 112 + 2 * (name.size() + 1);
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "ghostSession size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet GhostPackets::altFrameCount(uint32_t frameCount) {
    return buildFrameCount(kOpReplayCountAlt, frameCount, "altFrameCount");
}

Packet GhostPackets::altFrameChunk(const ReplayFrame* frames, size_t count) {
    return buildFrameChunk(kOpReplayChunkAlt, frames, count, "altFrameChunk");
}

Packet GhostPackets::submitResult(uint32_t rank, const GhostRecordEntry& mine,
                                  const std::vector<GhostRecordEntry>& top) {
    Packet pkt(kOpSubmit);

    std::vector<GhostRecordEntry> keep = top;
    if (keep.size() > kMaxTopRecords) {
        LOG_WARN("PACKET", "submit result drops " +
                           std::to_string(keep.size() - kMaxTopRecords) +
                           " top rows the client reads only " +
                           std::to_string(kMaxTopRecords));
        keep.resize(kMaxTopRecords);
    }
    if (keep.size() < kMaxTopRecords) {
        // unread slots stay zero and a zero time counts as faster than any run
        LOG_WARN("PACKET", "submit result has only " + std::to_string(keep.size()) +
                           " top rows so the popup placement will read too high");
    }

    pkt.writeUInt32(rank);

    const auto mineRow = recordEntryBytes(mine);
    pkt.writeBytes(mineRow.data(), mineRow.size());

    pkt.writeUInt32(static_cast<uint32_t>(keep.size()));
    for (const auto& e : keep) {
        const auto row = recordEntryBytes(e);
        pkt.writeBytes(row.data(), row.size());
    }

    const size_t expected = 184 + kRecordEntrySize * keep.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "submitResult size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

bool GhostPackets::parseMenuSelect(const Packet& pkt, uint32_t& outMenuKind,
                                   uint32_t& outSubKind) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() < 8) {
        LOG_WARN("PACKET", "menuSelect size " + std::to_string(body.size()) +
                           " expected 8");
        return false;
    }

    outMenuKind = readU32LE(body.data() + 0);
    outSubKind  = readU32LE(body.data() + 4);
    return true;
}

bool GhostPackets::parseGhostEnter(const Packet& pkt, uint32_t& outTrackId) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() < 4) {
        LOG_WARN("PACKET", "ghostEnter size " + std::to_string(body.size()) +
                           " expected 4");
        return false;
    }

    outTrackId = readU32LE(body.data());
    return true;
}

bool GhostPackets::parseStageBegin(const Packet& pkt) {
    if (!pkt.payload().empty()) {
        LOG_WARN("PACKET", "stageBegin size " + std::to_string(pkt.payload().size()) +
                           " expected 0");
    }
    return true;
}

bool GhostPackets::parseFinalLap(const Packet& pkt) {
    if (!pkt.payload().empty()) {
        LOG_WARN("PACKET", "finalLap size " + std::to_string(pkt.payload().size()) +
                           " expected 0");
    }
    return true;
}

bool GhostPackets::parseUploadCount(const Packet& pkt, uint32_t& outFrameCount) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() < 4) {
        LOG_WARN("PACKET", "uploadCount size " + std::to_string(body.size()) +
                           " expected 4");
        return false;
    }

    const uint32_t count = readU32LE(body.data());
    if (count > kUploadClamp) {
        // client clamps itself so anything above this was forged
        LOG_WARN("PACKET", "uploadCount " + std::to_string(count) +
                           " past the client clamp " + std::to_string(kUploadClamp) +
                           " refused");
        return false;
    }

    outFrameCount = count;
    return true;
}

bool GhostPackets::parseUploadChunk(const Packet& pkt, std::vector<ReplayFrame>& outFrames) {
    outFrames.clear();

    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() < 4) {
        LOG_WARN("PACKET", "uploadChunk size " + std::to_string(body.size()) +
                           " expected at least 4");
        return false;
    }

    const int32_t signedCount = static_cast<int32_t>(readU32LE(body.data()));
    if (signedCount < 0) {
        LOG_WARN("PACKET", "uploadChunk negative count refused");
        return false;
    }

    const size_t count = static_cast<size_t>(signedCount);
    if (body.size() < 4 + count * kFrameSize) {
        LOG_WARN("PACKET", "uploadChunk short have " + std::to_string(body.size()) +
                           " need " + std::to_string(4 + count * kFrameSize));
        return false;
    }

    if (!decodeFrames(body.data() + 4, body.size() - 4, count, outFrames)) return false;

    // client encoder never sets these so a hit means a forged or corrupt upload
    size_t oddFlags = 0;
    size_t oddInput = 0;
    size_t oddSteer = 0;
    for (const auto& f : outFrames) {
        if (f.flags & ~kFlagKnownMask)      ++oddFlags;
        if (f.inputMask & ~kInputKnownMask) ++oddInput;
        if (f.steerSpeed & ~0xFFu)          ++oddSteer;
    }
    if (oddFlags || oddInput || oddSteer) {
        LOG_WARN("PACKET", "uploadChunk unencodable bits flags " + std::to_string(oddFlags) +
                           " input " + std::to_string(oddInput) +
                           " steer " + std::to_string(oddSteer));
    }
    return true;
}

bool GhostPackets::parseSubmit(const Packet& pkt, uint32_t& outTrackId,
                               uint32_t& outTotalTimeMs, uint32_t& outCarKind) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() < 12) {
        LOG_WARN("PACKET", "ghost submit size " + std::to_string(body.size()) +
                           " expected 12");
        return false;
    }

    Reader r(body);
    outTrackId     = r.u32();
    outTotalTimeMs = r.u32();
    outCarKind     = r.u32();
    return !r.bad;
}

bool GhostPackets::beginUpload(GhostUploadSession& session, uint32_t frameCount) {
    if (session.trackId < 0) {
        LOG_WARN("GHOST", "upload begin with no ghost race running");
        return false;
    }
    if (frameCount > kUploadClamp) {
        LOG_WARN("GHOST", "upload begin " + std::to_string(frameCount) +
                          " past the client clamp refused");
        return false;
    }

    session.announcedCount = frameCount;
    session.frames.clear();
    session.frames.reserve(frameCount);
    return true;
}

bool GhostPackets::appendUpload(GhostUploadSession& session,
                                const std::vector<ReplayFrame>& frames) {
    if (session.trackId < 0) {
        LOG_WARN("GHOST", "upload append with no ghost race running");
        return false;
    }
    if (session.frames.size() + frames.size() > session.announcedCount) {
        LOG_WARN("GHOST", "upload append would reach " +
                          std::to_string(session.frames.size() + frames.size()) +
                          " past the announced " + std::to_string(session.announcedCount));
        return false;
    }

    session.frames.insert(session.frames.end(), frames.begin(), frames.end());
    return true;
}

bool GhostPackets::uploadComplete(const GhostUploadSession& session) {
    if (session.trackId < 0)  return false;
    if (!session.started)     return false;
    if (!session.finishedLap) return false;
    return session.frames.size() == session.announcedCount;
}

void GhostPackets::resetUpload(GhostUploadSession& session) {
    session.trackId        = -1;
    session.started        = false;
    session.finishedLap    = false;
    session.announcedCount = 0;
    session.frames.clear();
}

std::array<uint8_t, 0x2C> GhostPackets::defaultCharBlock() {
    std::array<uint8_t, 0x2C> r{};
    // dword 1 is the 0xBF key FUN 00450060 resolves it zero matches none and FUN 00425CB0 bails silently
    const int32_t dwords[7] = { 0, 10, 12000, 12100, 12200, -1, -1 };
    for (size_t k = 0; k < 7; ++k) {
        putU32LE(r.data() + k * 4, static_cast<uint32_t>(dwords[k]));
    }
    return r;   // trailing 16 bytes stay zero
}

std::array<uint8_t, 0x38> GhostPackets::defaultKartBlock() {
    std::array<uint8_t, 0x38> r{};
    // dword 1 is the 0xC0 catalogue key same trap as the character block
    const int32_t dwords[5] = { 0, 10010, 1000, 1100, -1 };
    for (size_t k = 0; k < 5; ++k) {
        putU32LE(r.data() + k * 4, static_cast<uint32_t>(dwords[k]));
    }
    return r;   // the 32 bytes at 0x08 are the part list and stay zero
}

bool GhostPackets::saveRecord(const GhostRecord& record,
                              const std::vector<ReplayFrame>& frames) {
    if (record.timeMs <= 0) {
        LOG_WARN("GHOST", "record time " + std::to_string(record.timeMs) +
                          " renders as an empty row so it is not stored");
        return false;
    }
    if (frames.size() > kMaxFramesArray) {
        LOG_ERROR("GHOST", "record replay " + std::to_string(frames.size()) +
                           " frames past capacity refused");
        return false;
    }

    auto existing = Database::instance().queryPrepared(
        "SELECT time_ms FROM ghost_record WHERE track_id = ? AND char_id = ? LIMIT 1",
        { record.trackId, record.charId });

    if (!existing.empty()) {
        const int64_t old = colI64(existing[0], "time_ms", 0);
        if (old > 0 && old <= record.timeMs) {
            LOG_INFO("GHOST", "keep faster stored time " + std::to_string(old) +
                              " over " + std::to_string(record.timeMs));
            return false;
        }
    }

    Transaction tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR("GHOST", "record save could not open a transaction");
        return false;
    }

    // record and its chunks must land together else a ghost plays a stale replay
    const bool head = tx.execute(
        "REPLACE INTO ghost_record "
        "(track_id, char_id, name, time_ms, car_kind, frame_count, char_block, kart_block) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        { record.trackId, record.charId, toUtf8(record.name), record.timeMs,
          record.carKind, static_cast<uint32_t>(frames.size()),
          toBlob(record.charBlock.data(), record.charBlock.size()),
          toBlob(record.kartBlock.data(), record.kartBlock.size()) });

    if (!head) {
        LOG_ERROR("GHOST", "record save head failed");
        return false;
    }

    if (!tx.execute("DELETE FROM ghost_replay_chunk WHERE track_id = ? AND char_id = ?",
                    { record.trackId, record.charId })) {
        LOG_ERROR("GHOST", "record save could not clear old chunks");
        return false;
    }

    uint32_t chunkIndex = 0;
    for (size_t off = 0; off < frames.size(); off += kFramesPerDbChunk) {
        const size_t n = (frames.size() - off) < kFramesPerDbChunk
                       ? (frames.size() - off) : kFramesPerDbChunk;

        std::vector<uint8_t> blob;
        blob.reserve(n * kFrameSize);
        for (size_t k = 0; k < n; ++k) {
            const auto raw = encodeFrame(frames[off + k]);
            blob.insert(blob.end(), raw.begin(), raw.end());
        }

        if (!tx.execute(
                "INSERT INTO ghost_replay_chunk "
                "(track_id, char_id, chunk_index, frame_count, data) VALUES (?, ?, ?, ?, ?)",
                { record.trackId, record.charId, chunkIndex,
                  static_cast<uint32_t>(n), toBlob(blob.data(), blob.size()) })) {
            LOG_ERROR("GHOST", "record save chunk " + std::to_string(chunkIndex) + " failed");
            return false;
        }
        ++chunkIndex;
    }

    if (!tx.commit()) {
        LOG_ERROR("GHOST", "record save commit failed");
        return false;
    }

    LOG_INFO("GHOST", "record saved track " + std::to_string(record.trackId) +
                      " char " + std::to_string(record.charId) +
                      " time " + std::to_string(record.timeMs) +
                      " frames " + std::to_string(frames.size()));
    return true;
}

std::vector<GhostRecord> GhostPackets::trackLeaderboard(int32_t trackId, size_t limit) {
    std::vector<GhostRecord> out;

    size_t cap = limit;
    if (cap == 0 || cap > kMaxBoardEntries) cap = kMaxBoardEntries;

    // LIMIT takes no bound parameter since MariaDB rejects a quoted LIMIT the literal is safe here
    auto rows = Database::instance().queryPrepared(
        "SELECT track_id, char_id, name, time_ms, car_kind, frame_count, "
        "char_block, kart_block FROM ghost_record "
        "WHERE track_id = ? AND time_ms > 0 ORDER BY time_ms ASC LIMIT " +
            std::to_string(cap),
        { trackId });

    out.reserve(rows.size());
    for (const auto& row : rows) {
        GhostRecord r;
        r.trackId    = static_cast<int32_t>(colI64(row, "track_id", trackId));
        r.charId     = static_cast<uint32_t>(colI64(row, "char_id", 0));
        r.name       = fromUtf8(colStr(row, "name"));
        r.timeMs     = static_cast<int32_t>(colI64(row, "time_ms", 0));
        r.carKind    = static_cast<uint32_t>(colI64(row, "car_kind", kCarKindA));
        r.frameCount = static_cast<uint32_t>(colI64(row, "frame_count", 0));
        blobToArray(colStr(row, "char_block"), r.charBlock);
        blobToArray(colStr(row, "kart_block"), r.kartBlock);
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<GhostRecordEntry> GhostPackets::trackEntries(int32_t trackId, size_t limit) {
    std::vector<GhostRecordEntry> out;
    for (const auto& r : trackLeaderboard(trackId, limit)) {
        GhostRecordEntry e;
        e.name   = r.name;
        e.timeMs = r.timeMs;
        out.push_back(std::move(e));
    }
    return out;
}

bool GhostPackets::trackBest(int32_t trackId, GhostRecord& out) {
    auto rows = trackLeaderboard(trackId, 1);
    if (rows.empty()) return false;
    out = rows.front();
    return true;
}

namespace {

const char* const kRecordColumns =
    "SELECT track_id, char_id, name, time_ms, car_kind, frame_count, char_block, kart_block "
    "FROM ghost_record ";

bool oneRecord(const std::string& sql, const DbParams& params, int32_t trackId, GhostRecord& out) {
    auto rows = Database::instance().queryPrepared(sql, params);
    if (rows.empty()) return false;
    const auto& row = rows.front();
    out.trackId    = static_cast<int32_t>(colI64(row, "track_id", trackId));
    out.charId     = static_cast<uint32_t>(colI64(row, "char_id", 0));
    out.name       = fromUtf8(colStr(row, "name"));
    out.timeMs     = static_cast<int32_t>(colI64(row, "time_ms", 0));
    out.carKind    = static_cast<uint32_t>(colI64(row, "car_kind", GhostPackets::kCarKindA));
    out.frameCount = static_cast<uint32_t>(colI64(row, "frame_count", 0));
    blobToArray(colStr(row, "char_block"), out.charBlock);
    blobToArray(colStr(row, "kart_block"), out.kartBlock);
    return true;
}

} // namespace

bool GhostPackets::playerBest(int32_t trackId, uint32_t charId, GhostRecord& out) {
    return oneRecord(std::string(kRecordColumns) +
                     "WHERE track_id = ? AND char_id = ? AND time_ms > 0 LIMIT 1",
                     { trackId, static_cast<int32_t>(charId) }, trackId, out);
}

bool GhostPackets::ghostAhead(int32_t trackId, uint32_t charId, GhostRecord& out) {
    GhostRecord mine;
    if (!playerBest(trackId, charId, mine)) {
        // no record yet the slowest row on the board is the first target
        return oneRecord(std::string(kRecordColumns) +
                         "WHERE track_id = ? AND time_ms > 0 ORDER BY time_ms DESC LIMIT 1",
                         { trackId }, trackId, out);
    }
    // the closest faster row the one place ahead
    if (oneRecord(std::string(kRecordColumns) +
                  "WHERE track_id = ? AND time_ms > 0 AND time_ms < ? ORDER BY time_ms DESC LIMIT 1",
                  { trackId, mine.timeMs }, trackId, out)) {
        return true;
    }
    // nobody ahead the holder races its own record
    out = mine;
    return true;
}

bool GhostPackets::loadReplay(int32_t trackId, uint32_t charId,
                              std::vector<ReplayFrame>& out) {
    out.clear();

    auto rows = Database::instance().queryPrepared(
        "SELECT chunk_index, frame_count, data FROM ghost_replay_chunk "
        "WHERE track_id = ? AND char_id = ? ORDER BY chunk_index ASC",
        { trackId, charId });

    if (rows.empty()) return false;

    uint32_t wantIndex = 0;
    for (const auto& row : rows) {
        const uint32_t idx   = static_cast<uint32_t>(colI64(row, "chunk_index", 0));
        const uint32_t count = static_cast<uint32_t>(colI64(row, "frame_count", 0));
        const std::string& blob = colStr(row, "data");

        if (idx != wantIndex) {
            LOG_ERROR("GHOST", "replay chunk gap at " + std::to_string(wantIndex) +
                               " got " + std::to_string(idx));
            out.clear();
            return false;
        }
        // db reader caps a column at 4096 bytes so a size mismatch means silent truncation
        if (blob.size() != static_cast<size_t>(count) * kFrameSize) {
            LOG_ERROR("GHOST", "replay chunk " + std::to_string(idx) + " has " +
                               std::to_string(blob.size()) + " bytes expected " +
                               std::to_string(static_cast<size_t>(count) * kFrameSize));
            out.clear();
            return false;
        }

        std::vector<ReplayFrame> part;
        if (!decodeFrames(reinterpret_cast<const uint8_t*>(blob.data()),
                          blob.size(), count, part)) {
            out.clear();
            return false;
        }
        out.insert(out.end(), part.begin(), part.end());
        ++wantIndex;
    }

    if (out.size() > kMaxFramesArray) {
        LOG_ERROR("GHOST", "replay " + std::to_string(out.size()) +
                           " frames past capacity refused");
        out.clear();
        return false;
    }
    return true;
}

uint32_t GhostPackets::rankOf(int32_t trackId, int32_t timeMs) {
    if (timeMs <= 0) return 0;

    auto rows = Database::instance().queryPrepared(
        "SELECT COUNT(*) AS faster FROM ghost_record "
        "WHERE track_id = ? AND time_ms > 0 AND time_ms < ?",
        { trackId, timeMs });

    if (rows.empty()) return 1;
    return static_cast<uint32_t>(colI64(rows[0], "faster", 0)) + 1;
}

} // namespace knc
