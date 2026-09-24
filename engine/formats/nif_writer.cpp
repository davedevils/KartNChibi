#include "engine/formats/nif_writer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace KnC {

namespace {

namespace fs = std::filesystem;

// Longest string bound reader puts on one string
constexpr size_t kLongestString = 65536;

// Blocks tile body exactly splice first at offset each where last
bool spans_match_source(const std::string& source_bytes, const NifScene& scene,
                        std::string& error) {
    const uint64_t size = static_cast<uint64_t>(source_bytes.size());
    if (scene.header.body_offset > size) {
        error = "the header claims a body offset past the end of the stream";
        return false;
    }
    uint64_t expected = scene.header.body_offset;
    for (const NifBlock& block : scene.blocks) {
        if (block.byte_offset != expected) {
            error = "block spans do not tile the body";
            return false;
        }
        expected += block.byte_length;
    }
    if (scene.trailing_bytes > size || expected != size - scene.trailing_bytes) {
        error = "the last block does not end where the footer begins";
        return false;
    }
    return true;
}

// Stream 20 2 0 5 states block length header table
bool resize_is_representable(const NifScene& scene, const std::vector<NifBlockEdit>& edits,
                             std::string& error) {
    if (scene.header.object_sizes.empty()) return true;
    for (const NifBlockEdit& edit : edits) {
        if (edit.body.size() == scene.blocks[edit.block].byte_length) continue;
        if (edit.body.size() > 0xFFFFFFFFull) {
            error = "block " + std::to_string(edit.block) +
                    " is longer than the size table can state";
            return false;
        }
        const uint64_t entry = scene.header.object_sizes_offset +
                               uint64_t(edit.block) * sizeof(uint32_t);
        if (entry + sizeof(uint32_t) > scene.header.body_offset) {
            error = "the block size table does not reach block " +
                    std::to_string(edit.block);
            return false;
        }
    }
    return true;
}

// New block length over header entry object sizes offset
void rewrite_size_table(const NifScene& scene, const std::vector<NifBlockEdit>& edits,
                        std::string& out) {
    if (scene.header.object_sizes.empty()) return;
    for (const NifBlockEdit& edit : edits) {
        const uint32_t length = static_cast<uint32_t>(edit.body.size());
        if (length == scene.blocks[edit.block].byte_length) continue;
        const size_t entry = static_cast<size_t>(scene.header.object_sizes_offset) +
                             size_t(edit.block) * sizeof(uint32_t);
        for (size_t byte = 0; byte < sizeof(uint32_t); ++byte)
            out[entry + byte] = static_cast<char>((length >> (byte * 8)) & 0xFFu);
    }
}

// Stream no fixed table no appended strings longer than bound
bool appended_strings_fit(const NifScene& scene, const NifStreamEdit& edit, std::string& error) {
    if (edit.added_strings.empty()) return true;
    if (scene.header.strings_end_offset == 0 ||
        scene.header.strings_end_offset > scene.header.body_offset) {
        error = "the stream carries no fixed string table to append a name to";
        return false;
    }
    for (const std::string& added : edit.added_strings)
        if (added.size() > kLongestString) {
            error = "a name of " + std::to_string(added.size()) +
                    " bytes is longer than a string table entry may be";
            return false;
        }
    return true;
}

// Longest entry table holds once additions in it
uint32_t longest_string(const NifScene& scene, const NifStreamEdit& edit) {
    uint32_t longest = scene.header.max_string_length;
    for (const std::string& added : edit.added_strings)
        longest = std::max(longest, static_cast<uint32_t>(added.size()));
    return longest;
}

void write_u32(std::string& out, size_t at, uint32_t value) {
    for (size_t byte = 0; byte < sizeof(uint32_t); ++byte)
        out[at + byte] = static_cast<char>((value >> (byte * 8)) & 0xFFu);
}

void append_u32(std::string& out, uint32_t value) {
    for (size_t byte = 0; byte < sizeof(uint32_t); ++byte)
        out.push_back(static_cast<char>((value >> (byte * 8)) & 0xFFu));
}

// Header first block body copied reader drops format line
void append_header(const std::string& source_bytes, const NifScene& scene,
                   const NifStreamEdit& edit, std::string& out) {
    const size_t body_offset = static_cast<size_t>(scene.header.body_offset);
    if (edit.added_strings.empty()) {
        out.append(source_bytes, 0, body_offset);
        return;
    }
    const size_t table_at = static_cast<size_t>(scene.header.strings_offset);
    const size_t table_end = static_cast<size_t>(scene.header.strings_end_offset);
    out.append(source_bytes, 0, table_end);
    write_u32(out, table_at,
              static_cast<uint32_t>(scene.header.strings.size() + edit.added_strings.size()));
    write_u32(out, table_at + sizeof(uint32_t), longest_string(scene, edit));
    for (const std::string& added : edit.added_strings) {
        append_u32(out, static_cast<uint32_t>(added.size()));
        out.append(added);
    }
    out.append(source_bytes, table_end, body_offset - table_end);
}

bool edits_are_sound(const NifScene& scene, const NifStreamEdit& edit, std::string& error) {
    const std::vector<NifBlockEdit>& edits = edit.blocks;
    std::vector<uint32_t> named;
    named.reserve(edits.size());
    for (const NifBlockEdit& one : edits) {
        if (one.block >= scene.blocks.size()) {
            error = "block " + std::to_string(one.block) + " is past the end of the stream";
            return false;
        }
        named.push_back(one.block);
    }
    std::sort(named.begin(), named.end());
    if (std::adjacent_find(named.begin(), named.end()) != named.end()) {
        error = "two edits name the same block";
        return false;
    }
    return resize_is_representable(scene, edits, error) &&
           appended_strings_fit(scene, edit, error);
}

// Replacement for one block or null caller left alone
const std::string* replacement_for(const std::vector<NifBlockEdit>& edits, uint32_t block) {
    for (const NifBlockEdit& one : edits)
        if (one.block == block) return &one.body;
    return nullptr;
}

} // namespace

bool read_nif_bytes(const std::string& path, std::string& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return !stream.bad();
}

bool build_nif_stream(const std::string& source_bytes, const NifScene& scene,
                      const NifStreamEdit& edit, std::string& out, std::string& error) {
    if (!spans_match_source(source_bytes, scene, error)) return false;
    if (!edits_are_sound(scene, edit, error)) return false;

    out.clear();
    out.reserve(source_bytes.size());
    append_header(source_bytes, scene, edit, out);
    rewrite_size_table(scene, edit.blocks, out);
    for (uint32_t index = 0; index < scene.blocks.size(); ++index) {
        const NifBlock& block = scene.blocks[index];
        const std::string* replaced = replacement_for(edit.blocks, index);
        if (replaced != nullptr) out.append(*replaced);
        else out.append(source_bytes, static_cast<size_t>(block.byte_offset),
                        static_cast<size_t>(block.byte_length));
    }
    // Footer root count roots reader keeps copied costs nothing
    out.append(source_bytes, source_bytes.size() - static_cast<size_t>(scene.trailing_bytes),
               static_cast<size_t>(scene.trailing_bytes));
    return true;
}

bool write_nif_stream(const std::string& source_path, const NifScene& scene,
                      const NifStreamEdit& edit, const std::string& out_path,
                      std::string& error) {
    std::error_code failure;
    if (fs::equivalent(source_path, out_path, failure)) {
        error = out_path + " is the stream that was read, and the client tree is read-only";
        return false;
    }
    std::string source_bytes;
    if (!read_nif_bytes(source_path, source_bytes)) {
        error = "cannot read " + source_path;
        return false;
    }
    std::string built;
    if (!build_nif_stream(source_bytes, scene, edit, built, error)) return false;

    fs::create_directories(fs::path(out_path).parent_path(), failure);
    std::ofstream output(out_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error = out_path + " cannot be written";
        return false;
    }
    output.write(built.data(), static_cast<std::streamsize>(built.size()));
    output.flush();
    if (output) return true;
    error = out_path + " was only partly written";
    return false;
}

} // namespace KnC
