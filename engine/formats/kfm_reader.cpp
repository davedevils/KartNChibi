#include "engine/formats/kfm_reader.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace KnC {
namespace {

constexpr uint32_t make_kfm_version(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(c) << 8) | d;
}

// Every gate below is a branch of NiKFMTool ReadBinary cross read against Gamebryo 2 6 3 2 4
constexpr uint32_t kOldestBinary             = make_kfm_version(1, 2, 0, 0);
constexpr uint32_t kSynchronizeIdFrom        = make_kfm_version(1, 2, 1, 0);
constexpr uint32_t kDefaultTransitionsFrom   = make_kfm_version(1, 2, 2, 0);
constexpr uint32_t kDefaultPathsUntil        = make_kfm_version(1, 2, 3, 0);
constexpr uint32_t kChainHoldsSourceUntil    = make_kfm_version(1, 2, 4, 0);
constexpr uint32_t kLegacySequenceNameUntil  = make_kfm_version(1, 2, 5, 0);
constexpr uint32_t kEndianFlagFrom           = make_kfm_version(1, 2, 6, 0);
constexpr uint32_t kSequenceNameFrom         = make_kfm_version(2, 5, 0, 0);
// Additive from 3103 missing in 3200 and 3201 back from 3202
constexpr uint32_t kAdditiveFrom     = make_kfm_version(30, 1, 0, 3);
constexpr uint32_t kAdditiveLostFrom = make_kfm_version(30, 2, 0, 0);
constexpr uint32_t kAdditiveBackFrom = make_kfm_version(30, 2, 0, 2);

// NiKFMTool loads every string through a 1024 byte stack buffer plus a NUL
constexpr int32_t kMaxStringLength = 1023;
// NiKFMTool LoadFile reads the version comment into a 256 byte buffer
constexpr size_t kMaxVersionLineLength = 256;

// Smallest on disk size of each repeated record used to sanity check counts
constexpr size_t kMinSequenceBytes    = 16;
constexpr size_t kMinTransitionBytes  = 8;
constexpr size_t kMinBlendPairBytes   = 8;
constexpr size_t kMinChainStepBytes   = 8;
constexpr size_t kMinGroupBytes       = 12;
constexpr size_t kMinGroupMemberBytes = 20;

bool has_additive_flag(uint32_t version) {
    if (version >= kAdditiveBackFrom) return true;
    return version >= kAdditiveFrom && version < kAdditiveLostFrom;
}

// Bounds checked forward reader over a file already in memory every take refuses to run off end
class Cursor {
public:
    explicit Cursor(const std::vector<char>& bytes) : bytes_(bytes) {}

    bool take(void* destination, size_t count) {
        if (count > remaining()) return false;
        std::memcpy(destination, bytes_.data() + position_, count);
        position_ += count;
        return true;
    }

    bool take_u8(uint8_t& value)    { return take(&value, sizeof(value)); }
    bool take_u32(uint32_t& value)  { return take(&value, sizeof(value)); }
    bool take_i32(int32_t& value)   { return take(&value, sizeof(value)); }
    bool take_f32(float& value)     { return take(&value, sizeof(value)); }

    size_t remaining() const { return bytes_.size() - position_; }
    size_t position() const  { return position_; }
    void seek(size_t position) { position_ = position; }

private:
    const std::vector<char>& bytes_;
    size_t position_ = 0;
};

// Decodes the binary body that follows the version comment line
class KfmParser {
public:
    KfmParser(const std::vector<char>& bytes, size_t body_offset, uint32_t version)
        : cursor_(bytes), version_(version) {
        cursor_.seek(body_offset);
    }

    const std::string& error() const { return error_; }

    bool parse(KfmFile& out) {
        if (!read_header(out)) return false;

        uint32_t sequence_count = 0;
        if (!read_count(sequence_count, kMinSequenceBytes, "sequence count")) return false;
        out.sequences.resize(sequence_count);
        for (uint32_t i = 0; i < sequence_count; ++i)
            if (!read_sequence(out.sequences[i])) return wrap("sequence", i);

        uint32_t group_count = 0;
        if (!read_count(group_count, kMinGroupBytes, "sequence group count")) return false;
        out.sequence_groups.resize(group_count);
        for (uint32_t i = 0; i < group_count; ++i)
            if (!read_group(out.sequence_groups[i])) return wrap("sequence group", i);

        // A KFM ends on its last group so anything left means a mis read gate
        if (cursor_.remaining() != 0)
            return fail(std::to_string(cursor_.remaining()) + " trailing bytes");
        return true;
    }

private:
    bool fail(const std::string& reason) {
        error_ = reason + " at offset " + std::to_string(cursor_.position());
        return false;
    }

    bool wrap(const char* what, uint32_t index) {
        error_ = std::string(what) + " " + std::to_string(index) + ": " + error_;
        return false;
    }

    bool read_u32(uint32_t& value, const char* what) {
        if (cursor_.take_u32(value)) return true;
        return fail(std::string("truncated ") + what);
    }

    bool read_i32(int32_t& value, const char* what) {
        if (cursor_.take_i32(value)) return true;
        return fail(std::string("truncated ") + what);
    }

    bool read_float(float& value, const char* what) {
        if (cursor_.take_f32(value)) return true;
        return fail(std::string("truncated ") + what);
    }

    // A count is credible only if the records it announces could still fit
    bool read_count(uint32_t& value, size_t smallest_record_bytes, const char* what) {
        if (!read_u32(value, what)) return false;
        if (uint64_t(value) * smallest_record_bytes > cursor_.remaining())
            return fail(std::string(what) + " " + std::to_string(value) + " exceeds the file");
        return true;
    }

    // Length prefixed not NUL terminated on disk
    bool read_string(std::string& out) {
        int32_t length = 0;
        if (!cursor_.take_i32(length)) return fail("truncated string length");
        if (length < 0 || length > kMaxStringLength)
            return fail("implausible string length " + std::to_string(length));
        out.resize(size_t(length));
        if (length != 0 && !cursor_.take(out.data(), size_t(length)))
            return fail("truncated string body");
        return true;
    }

    // Enums stream as a 32 bit int
    bool read_transition_type(KfmTransitionType& out) {
        uint32_t value = 0;
        if (!read_u32(value, "transition type")) return false;
        if (value > uint32_t(KfmTransitionType::DefaultInvalid))
            return fail("unknown transition type " + std::to_string(value));
        out = KfmTransitionType(value);
        return true;
    }

    // Before 1 2 3 0 the file kept one shared prefix for NIF and KFs the loader glues back on
    bool read_default_paths() {
        if (version_ >= kDefaultPathsUntil) return true;
        uint8_t has_default_paths = 0;
        if (!cursor_.take_u8(has_default_paths)) return fail("truncated default-path flag");
        if (has_default_paths == 0) return true;
        return read_string(default_nif_prefix_) && read_string(default_kf_prefix_);
    }

    bool read_default_transitions(KfmFile& out) {
        return read_transition_type(out.default_sync_type)
            && read_transition_type(out.default_non_sync_type)
            && read_float(out.default_sync_duration, "default sync duration")
            && read_float(out.default_non_sync_duration, "default non-sync duration");
    }

    bool read_header(KfmFile& out) {
        if (version_ >= kEndianFlagFrom) {
            uint8_t little_endian = 1;
            if (!cursor_.take_u8(little_endian)) return fail("truncated endian flag");
            if (little_endian != 1) return fail("big-endian KFM is not supported");
        }
        if (!read_default_paths()) return false;
        if (!read_string(out.model_path)) return false;
        out.model_path = default_nif_prefix_ + out.model_path;
        // Both sides of 2 1 0 0 model root gate stream the same encoding
        if (!read_string(out.model_root)) return false;
        if (version_ < kDefaultTransitionsFrom) return true;
        return read_default_transitions(out);
    }

    bool read_chain_step(KfmChainStep& out) {
        return read_u32(out.sequence_id, "chain sequence id")
            && read_float(out.duration, "chain duration");
    }

    bool read_chain(std::vector<KfmChainStep>& out) {
        uint32_t step_count = 0;
        if (!read_count(step_count, kMinChainStepBytes, "chain step count")) return false;
        // Before 1 2 4 0 the source sequence led the chain and is now dropped
        if (version_ < kChainHoldsSourceUntil && step_count > 0) {
            KfmChainStep source_step;
            if (!read_chain_step(source_step)) return false;
            --step_count;
        }
        out.resize(step_count);
        for (KfmChainStep& step : out)
            if (!read_chain_step(step)) return false;
        return true;
    }

    bool read_transition(KfmTransition& out) {
        if (!read_u32(out.target_sequence_id, "transition target")) return false;
        if (!read_transition_type(out.type)) return false;
        // The two Default types defer to the file level settings and store nothing
        if (out.type == KfmTransitionType::DefaultSync ||
            out.type == KfmTransitionType::DefaultNonSync)
            return true;

        if (!read_float(out.duration, "transition duration")) return false;
        uint32_t blend_pair_count = 0;
        if (!read_count(blend_pair_count, kMinBlendPairBytes, "blend pair count")) return false;
        out.blend_pairs.resize(blend_pair_count);
        for (KfmBlendPair& pair : out.blend_pairs)
            if (!read_string(pair.start_key) || !read_string(pair.target_key)) return false;
        return read_chain(out.chain);
    }

    bool read_sequence(KfmSequence& out) {
        if (!read_u32(out.sequence_id, "sequence id")) return false;
        if (version_ < kLegacySequenceNameUntil && !read_string(out.name)) return false;
        if (!read_string(out.kf_path)) return false;
        out.kf_path = default_kf_prefix_ + out.kf_path;

        if (version_ >= kSequenceNameFrom) {
            if (!read_string(out.name)) return false;
        } else if (!read_i32(out.animation_index, "animation index")) {
            return false;
        }

        uint32_t transition_count = 0;
        if (!read_count(transition_count, kMinTransitionBytes, "transition count")) return false;
        out.transitions.resize(transition_count);
        for (uint32_t i = 0; i < transition_count; ++i)
            if (!read_transition(out.transitions[i])) return wrap("transition", i);
        return true;
    }

    bool read_group_member(KfmSequenceGroupMember& out) {
        if (!read_u32(out.sequence_id, "group member sequence id")) return false;
        if (!read_i32(out.priority, "group member priority")) return false;
        if (!read_float(out.weight, "group member weight")) return false;
        if (!read_float(out.ease_in_seconds, "group member ease-in")) return false;
        if (!read_float(out.ease_out_seconds, "group member ease-out")) return false;
        if (version_ >= kSynchronizeIdFrom &&
            !read_u32(out.synchronize_sequence_id, "group member sync id"))
            return false;
        if (!has_additive_flag(version_)) return true;

        uint8_t additive = 0;
        if (!cursor_.take_u8(additive)) return fail("truncated group member additive flag");
        out.additive = additive != 0;
        return true;
    }

    bool read_group(KfmSequenceGroup& out) {
        if (!read_u32(out.group_id, "group id")) return false;
        // Both sides of 2 1 0 0 group name gate stream the same encoding
        if (!read_string(out.name)) return false;
        uint32_t member_count = 0;
        if (!read_count(member_count, kMinGroupMemberBytes, "group member count")) return false;
        out.members.resize(member_count);
        for (KfmSequenceGroupMember& member : out.members)
            if (!read_group_member(member)) return false;
        return true;
    }

    Cursor cursor_;
    uint32_t version_;
    std::string default_nif_prefix_;
    std::string default_kf_prefix_;
    std::string error_;
};

bool load_file(const std::string& path, std::vector<char>& bytes) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input.is_open()) return false;
    bytes.resize(static_cast<size_t>(input.tellg()));
    input.seekg(0, std::ios::beg);
    return static_cast<bool>(
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())));
}

struct KfmVersionLine {
    std::string text;
    bool        binary = false;
    size_t      body_offset = 0;
};

// Gamebryo KFM File Version line only b is stripped and only b means body is binary
bool split_version_line(const std::vector<char>& bytes, KfmVersionLine& out) {
    constexpr char kPrefix[] = ";Gamebryo KFM File Version ";
    constexpr size_t kPrefixLength = sizeof(kPrefix) - 1;

    if (bytes.size() <= kPrefixLength) return false;
    const size_t limit = std::min(bytes.size(), kMaxVersionLineLength);
    const void* newline = std::memchr(bytes.data(), '\n', limit);
    if (!newline) return false;

    const size_t line_length = static_cast<size_t>(
        static_cast<const char*>(newline) - bytes.data());
    if (line_length <= kPrefixLength) return false;
    if (std::memcmp(bytes.data(), kPrefix, kPrefixLength) != 0) return false;

    out.binary = bytes[line_length - 1] == 'b';
    out.text.assign(bytes.data() + kPrefixLength,
                    line_length - kPrefixLength - (out.binary ? 1 : 0));
    out.body_offset = line_length + 1;
    return true;
}

// d d d d most significant component first components may be omitted from the right
bool parse_version_text(const std::string& text, uint32_t& version) {
    version = 0;
    size_t index = 0;
    for (int shift = 24; shift >= 0; shift -= 8) {
        uint32_t component = 0;
        size_t digits = 0;
        while (index < text.size() && text[index] >= '0' && text[index] <= '9' && digits < 3) {
            component = component * 10 + uint32_t(text[index] - '0');
            ++index;
            ++digits;
        }
        if (digits == 0 || component > 255) return false;
        version |= component << shift;
        if (index == text.size()) return true;
        if (text[index] != '.') return false;
        ++index;
    }
    return false;
}

} // namespace

bool read_kfm(const std::string& path, KfmFile& out, std::string& error) {
    std::vector<char> bytes;
    if (!load_file(path, bytes)) { error = "cannot read " + path; return false; }

    KfmVersionLine line;
    if (!split_version_line(bytes, line)) {
        error = "not a KFM: no ';Gamebryo KFM File Version ' line";
        return false;
    }

    uint32_t version = 0;
    if (!parse_version_text(line.text, version)) {
        error = "unreadable KFM version '" + line.text + "'";
        return false;
    }
    if (!line.binary) {
        error = "ASCII KFM " + line.text + " is not supported";
        return false;
    }
    if (version < kOldestBinary) {
        error = "KFM " + line.text + " predates 1.2.0.0 and uses the old ASCII layout";
        return false;
    }

    out = KfmFile{};
    out.version = version;
    out.version_text = line.text;

    KfmParser parser(bytes, line.body_offset, version);
    if (!parser.parse(out)) { error = "KFM " + line.text + ": " + parser.error(); return false; }
    return true;
}

std::string kfm_relative_path(const std::string& stored) {
    size_t start = 0;
    while (start < stored.size() && (stored[start] == '/' || stored[start] == '\\')) ++start;
    std::string tail = stored.substr(start);
    for (char& letter : tail)
        if (letter == '\\') letter = '/';
    return tail;
}

} // namespace KnC
