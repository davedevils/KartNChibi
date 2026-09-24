#include "ghost_replay.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

#if KNC_REPLAY_DB
#include <mysql.h>
#include <cstdlib>
#endif

namespace KnC::Tools {

namespace {

using KnC::Kart::Client::GhostSample;

constexpr size_t kFrameBytes = 28;
constexpr char kMagic[4] = {'K', 'C', 'G', 'R'};
constexpr uint32_t kFormatVersion = 1;

// Header is magic version track char time car frame count char block kart block name length
constexpr size_t kFixedHeaderBytes = 4 + 4 + 4 + 4 + 4 + 4 + 4 + 0x2C + 0x38 + 2;

// ten physics ticks of 20 ms hold one ghost sample matches stage 15 decimation
constexpr float kPhysicsTickSeconds = 0.02f;
constexpr int kTicksPerGhostSample = 10;
constexpr float kSampleIntervalSeconds = kPhysicsTickSeconds * static_cast<float>(kTicksPerGhostSample);
constexpr float kYawByteToDeg = 360.0f / 255.0f;

uint32_t read_u32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void write_u32le(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

uint16_t read_u16le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

void write_u16le(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

float read_f32le(const uint8_t* p) {
    float v = 0.0f;
    std::memcpy(&v, p, 4);
    return v;
}

void write_f32le(uint8_t* p, float v) {
    std::memcpy(p, &v, 4);
}

// wire layout matches ghost replay chunk data and packet registry replay frame
GhostSample decode_sample(const uint8_t* p) {
    GhostSample s;
    s.pos[0] = read_f32le(p + 0x00);
    s.pos[1] = read_f32le(p + 0x04);
    s.pos[2] = read_f32le(p + 0x08);
    s.yawByte = p[0x0C];
    s.flags = static_cast<uint16_t>(read_u32le(p + 0x10) & 0xFFFF);
    s.nibbles = static_cast<uint8_t>(read_u32le(p + 0x14) & 0xFF);
    s.inputMask = p[0x18];
    return s;
}

void encode_sample(const GhostSample& s, uint8_t* p) {
    std::memset(p, 0, kFrameBytes);
    write_f32le(p + 0x00, s.pos[0]);
    write_f32le(p + 0x04, s.pos[1]);
    write_f32le(p + 0x08, s.pos[2]);
    p[0x0C] = s.yawByte;
    write_u32le(p + 0x10, s.flags);
    write_u32le(p + 0x14, s.nibbles);
    p[0x18] = s.inputMask;
}

float wrap_signed_180(float deg) {
    deg = std::fmod(deg, 360.0f);
    if (deg < 0.0f) deg += 360.0f;
    if (deg > 180.0f) deg -= 360.0f;
    return deg;
}

// bit layout mirrors car ghost sample apply in remote car cpp low byte first
void decode_flags(uint16_t flags, GhostPose& pose) {
    const uint8_t low = static_cast<uint8_t>(flags & 0xFF);
    if (low & 0x80) {
        pose.boosting = true;
        pose.boostKind = 0;
    } else if (low & 0x40) {
        pose.boosting = true;
        pose.boostKind = 1;
    } else {
        pose.boosting = false;
    }
    pose.reversing = (low & 0x20) != 0;
    pose.miniTurboStage = (low & 0x08) ? 1 : 0;
    if (low & 0x04) pose.turnState = 1;
    else if (low & 0x02) pose.turnState = 2;
    else pose.turnState = 0;
    if (flags & 0x0010) pose.driftState = 0;
    if (flags & 0x0100) pose.driftState = 1;
    if (flags & 0x0200) pose.driftState = 2;
}

GhostPose pose_from_sample(const GhostSample& s) {
    GhostPose p;
    p.pos[0] = s.pos[0];
    p.pos[1] = s.pos[1];
    p.pos[2] = s.pos[2];
    p.yawDeg = static_cast<float>(s.yawByte) * kYawByteToDeg;
    decode_flags(s.flags, p);
    return p;
}

}  // namespace

bool load_ghost_file(const std::string& path, GhostRecording& out, std::string& error) {
    out = GhostRecording{};
    error.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "could not open the ghost file " + path;
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (data.size() >= 4 && std::memcmp(data.data(), kMagic, 4) == 0) {
        if (data.size() < kFixedHeaderBytes) {
            error = "ghost file header is shorter than expected";
            return false;
        }
        size_t off = 4;
        const uint32_t version = read_u32le(&data[off]); off += 4;
        if (version != kFormatVersion) {
            error = "ghost file version is not one this tool knows";
            return false;
        }
        out.trackId = static_cast<int32_t>(read_u32le(&data[off])); off += 4;
        out.charId = read_u32le(&data[off]); off += 4;
        out.timeMs = static_cast<int32_t>(read_u32le(&data[off])); off += 4;
        out.carKind = read_u32le(&data[off]); off += 4;
        const uint32_t frameCount = read_u32le(&data[off]); off += 4;
        std::memcpy(out.charBlock.data(), &data[off], out.charBlock.size()); off += out.charBlock.size();
        std::memcpy(out.kartBlock.data(), &data[off], out.kartBlock.size()); off += out.kartBlock.size();
        const uint16_t nameLen = read_u16le(&data[off]); off += 2;
        if (data.size() < off + nameLen) {
            error = "ghost file name field runs past the end of the file";
            return false;
        }
        out.name.assign(reinterpret_cast<const char*>(&data[off]), nameLen);
        off += nameLen;

        const size_t need = static_cast<size_t>(frameCount) * kFrameBytes;
        if (data.size() - off != need) {
            error = "ghost file sample bytes do not match its stored frame count";
            return false;
        }
        out.frameCount = frameCount;
        out.samples.reserve(frameCount);
        for (uint32_t i = 0; i < frameCount; ++i) {
            out.samples.push_back(decode_sample(&data[off + static_cast<size_t>(i) * kFrameBytes]));
        }
        return true;
    }

    if (data.size() % kFrameBytes != 0) {
        error = "ghost file has no header and its size is not a multiple of 28";
        return false;
    }
    const size_t count = data.size() / kFrameBytes;
    out.frameCount = static_cast<uint32_t>(count);
    out.samples.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        out.samples.push_back(decode_sample(&data[i * kFrameBytes]));
    }
    return true;
}

bool save_ghost_file(const std::string& path, const GhostRecording& recording, std::string& error) {
    error.clear();
    if (recording.name.size() > 0xFFFF) {
        error = "ghost name is too long to store in the file header";
        return false;
    }

    std::vector<uint8_t> data;
    data.reserve(kFixedHeaderBytes + recording.name.size() + recording.samples.size() * kFrameBytes);

    auto pushU32 = [&](uint32_t v) {
        uint8_t b[4];
        write_u32le(b, v);
        data.insert(data.end(), b, b + 4);
    };

    data.insert(data.end(), kMagic, kMagic + 4);
    pushU32(kFormatVersion);
    pushU32(static_cast<uint32_t>(recording.trackId));
    pushU32(recording.charId);
    pushU32(static_cast<uint32_t>(recording.timeMs));
    pushU32(recording.carKind);
    pushU32(static_cast<uint32_t>(recording.samples.size()));
    data.insert(data.end(), recording.charBlock.begin(), recording.charBlock.end());
    data.insert(data.end(), recording.kartBlock.begin(), recording.kartBlock.end());

    uint8_t nameLenBytes[2];
    write_u16le(nameLenBytes, static_cast<uint16_t>(recording.name.size()));
    data.insert(data.end(), nameLenBytes, nameLenBytes + 2);
    data.insert(data.end(), recording.name.begin(), recording.name.end());

    for (const auto& s : recording.samples) {
        uint8_t frame[kFrameBytes];
        encode_sample(s, frame);
        data.insert(data.end(), frame, frame + kFrameBytes);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "could not create the ghost file " + path;
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) {
        error = "write failed for the ghost file " + path;
        return false;
    }
    return true;
}

#if KNC_REPLAY_DB

namespace {

// closes the connection on every exit path including an early return
struct MysqlConn {
    MYSQL* handle = nullptr;
    ~MysqlConn() {
        if (handle) mysql_close(handle);
    }
};

bool append_chunk_samples(const uint8_t* data, size_t len, uint32_t count,
                          std::vector<GhostSample>& out, std::string& error) {
    if (len != static_cast<size_t>(count) * kFrameBytes) {
        error = "a replay chunk byte count does not match its stored frame count";
        return false;
    }
    out.reserve(out.size() + count);
    for (uint32_t i = 0; i < count; ++i) {
        out.push_back(decode_sample(data + static_cast<size_t>(i) * kFrameBytes));
    }
    return true;
}

}  // namespace

bool load_ghost_db(const std::string& host, uint16_t port, const std::string& user,
                    const std::string& password, const std::string& database,
                    int32_t trackId, uint32_t charId, GhostRecording& out, std::string& error) {
    out = GhostRecording{};
    error.clear();
    out.trackId = trackId;
    out.charId = charId;

    MysqlConn conn;
    conn.handle = mysql_init(nullptr);
    if (!conn.handle) {
        error = "mysql init failed";
        return false;
    }
    if (!mysql_real_connect(conn.handle, host.c_str(), user.c_str(), password.c_str(),
                            database.c_str(), port, nullptr, 0)) {
        error = std::string("could not connect to the database ") + mysql_error(conn.handle);
        return false;
    }
    mysql_set_character_set(conn.handle, "utf8mb4");

    {
        const std::string sql =
            "SELECT name, time_ms, car_kind, frame_count, char_block, kart_block "
            "FROM ghost_record WHERE track_id = " +
            std::to_string(trackId) + " AND char_id = " + std::to_string(charId) + " LIMIT 1";
        if (mysql_query(conn.handle, sql.c_str()) != 0) {
            error = std::string("ghost record query failed ") + mysql_error(conn.handle);
            return false;
        }
        MYSQL_RES* res = mysql_store_result(conn.handle);
        if (!res) {
            error = std::string("ghost record query returned no result set ") + mysql_error(conn.handle);
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row) {
            mysql_free_result(res);
            error = "no ghost record for that track and character";
            return false;
        }
        unsigned long* lens = mysql_fetch_lengths(res);
        out.name.assign(row[0] ? row[0] : "", row[0] ? lens[0] : 0);
        out.timeMs = row[1] ? static_cast<int32_t>(std::strtol(row[1], nullptr, 10)) : 0;
        out.carKind = row[2] ? static_cast<uint32_t>(std::strtoul(row[2], nullptr, 10)) : 0;
        out.frameCount = row[3] ? static_cast<uint32_t>(std::strtoul(row[3], nullptr, 10)) : 0;
        if (row[4]) {
            const size_t n = std::min<size_t>(lens[4], out.charBlock.size());
            std::memcpy(out.charBlock.data(), row[4], n);
        }
        if (row[5]) {
            const size_t n = std::min<size_t>(lens[5], out.kartBlock.size());
            std::memcpy(out.kartBlock.data(), row[5], n);
        }
        mysql_free_result(res);
    }

    {
        const std::string sql =
            "SELECT chunk_index, frame_count, data FROM ghost_replay_chunk "
            "WHERE track_id = " +
            std::to_string(trackId) + " AND char_id = " + std::to_string(charId) +
            " ORDER BY chunk_index ASC";
        if (mysql_query(conn.handle, sql.c_str()) != 0) {
            error = std::string("replay chunk query failed ") + mysql_error(conn.handle);
            return false;
        }
        MYSQL_RES* res = mysql_store_result(conn.handle);
        if (!res) {
            error = std::string("replay chunk query returned no result set ") + mysql_error(conn.handle);
            return false;
        }

        uint32_t wantIndex = 0;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            unsigned long* lens = mysql_fetch_lengths(res);
            const uint32_t idx = row[0] ? static_cast<uint32_t>(std::strtoul(row[0], nullptr, 10)) : 0;
            const uint32_t count = row[1] ? static_cast<uint32_t>(std::strtoul(row[1], nullptr, 10)) : 0;
            if (idx != wantIndex) {
                mysql_free_result(res);
                error = "replay chunks are out of order or one is missing";
                return false;
            }
            if (!row[2]) {
                mysql_free_result(res);
                error = "a replay chunk has no data";
                return false;
            }
            if (!append_chunk_samples(reinterpret_cast<const uint8_t*>(row[2]), lens[2], count,
                                      out.samples, error)) {
                mysql_free_result(res);
                return false;
            }
            ++wantIndex;
        }
        mysql_free_result(res);
    }

    if (out.samples.empty()) {
        error = "the ghost record has no replay chunks";
        return false;
    }
    return true;
}

#else

bool load_ghost_db(const std::string&, uint16_t, const std::string&, const std::string&,
                    const std::string&, int32_t, uint32_t, GhostRecording&, std::string& error) {
    error = "this build has no database connector so ghost dump cannot reach a server";
    return false;
}

#endif

GhostPlayback::GhostPlayback(const GhostRecording& recording) : m_recording(recording) {}

void GhostPlayback::advance(float seconds) {
    m_elapsed += seconds;
    if (m_elapsed < 0.0f) m_elapsed = 0.0f;
}

void GhostPlayback::seek(float seconds) {
    m_elapsed = seconds < 0.0f ? 0.0f : seconds;
}

void GhostPlayback::restart() {
    m_elapsed = 0.0f;
}

float GhostPlayback::elapsedSeconds() const {
    return m_elapsed;
}

float GhostPlayback::durationSeconds() const {
    return static_cast<float>(m_recording.samples.size()) * kSampleIntervalSeconds;
}

size_t GhostPlayback::sampleCount() const {
    return m_recording.samples.size();
}

bool GhostPlayback::finished() const {
    return m_recording.samples.empty() || m_elapsed >= durationSeconds();
}

GhostPose GhostPlayback::pose() const {
    const auto& samples = m_recording.samples;
    if (samples.empty()) return GhostPose{};

    const size_t last = samples.size() - 1;
    float sampleFloat = m_elapsed / kSampleIntervalSeconds;
    if (sampleFloat < 0.0f) sampleFloat = 0.0f;
    size_t idx = static_cast<size_t>(sampleFloat);
    if (idx > last) idx = last;

    if (idx == 0) return pose_from_sample(samples[0]);

    const auto& prev = samples[idx - 1];
    const auto& cur = samples[idx];
    float fraction = sampleFloat - static_cast<float>(idx);
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    GhostPose p;
    for (int a = 0; a < 3; ++a) {
        p.pos[a] = prev.pos[a] + (cur.pos[a] - prev.pos[a]) * fraction;
    }

    const float prevYaw = static_cast<float>(prev.yawByte) * kYawByteToDeg;
    const float curYaw = static_cast<float>(cur.yawByte) * kYawByteToDeg;
    const float yawDelta = wrap_signed_180(curYaw - prevYaw);
    p.yawDeg = prevYaw + yawDelta * fraction;

    decode_flags(cur.flags, p);
    return p;
}

}  // namespace KnC Tools
