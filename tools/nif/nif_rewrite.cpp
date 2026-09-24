// checks NIF writer reproduces bytes exactly and edits resize rename and encode survive reload
#include "engine/formats/nif_edit.h"
#include "engine/formats/nif_encode.h"
#include "engine/formats/nif_reader.h"
#include "engine/formats/nif_writer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using namespace KnC;

// extension compare ignores case
bool extension_matches(const fs::path& found, const std::string& wanted) {
    const std::string text = found.extension().string();
    if (text.size() != wanted.size()) return false;
    for (size_t at = 0; at < text.size(); ++at)
        if (std::tolower(static_cast<unsigned char>(text[at])) !=
            std::tolower(static_cast<unsigned char>(wanted[at]))) return false;
    return true;
}

// re encode stats for one block type blocks identical refused and named problems
struct EncodeTotals {
    size_t                   blocks = 0;
    size_t                   identical = 0;
    size_t                   refused = 0;
    std::vector<std::string> problems;
};

constexpr int     kEncodedTypeCount = 4;
const char* const kEncodedTypes[kEncodedTypeCount] = {"NiTriShape", "NiTriStrips",
                                                      "NiTriShapeData",
                                                      "NiTriStripsData"};
constexpr size_t kNamedProblems = 4;

struct RewriteTotals {
    size_t                   streams = 0;
    size_t                   parsed = 0;
    size_t                   identical = 0;
    std::vector<std::string> unparsed;
    std::vector<std::string> different;
    // counted per type not summed across the different families
    EncodeTotals             encoded[kEncodedTypeCount];
};

// first disagreement position or common prefix length
size_t first_difference(const std::string& left, const std::string& right) {
    const size_t shared = std::min(left.size(), right.size());
    for (size_t at = 0; at < shared; ++at)
        if (left[at] != right[at]) return at;
    return shared;
}

int encoded_type_index(const std::string& type) {
    for (int at = 0; at < kEncodedTypeCount; ++at)
        if (type == kEncodedTypes[at]) return at;
    return -1;
}

// re encodes each matching block and compares against the bytes read from source
void check_re_encode(const std::string& source, const NifScene& scene, const fs::path& path,
                     RewriteTotals& totals) {
    for (const NifBlock& block : scene.blocks) {
        const int family = encoded_type_index(block.type);
        if (family < 0 || !can_encode_nif_block(block)) continue;
        EncodeTotals& into = totals.encoded[family];
        ++into.blocks;
        std::string written;
        std::string refusal;
        if (!encode_nif_block(scene.header, block, written, refusal)) {
            ++into.refused;
            if (into.problems.size() < kNamedProblems)
                into.problems.push_back(path.filename().string() + ": " + refusal);
            continue;
        }
        const std::string shipped = nif_block_body(source, block);
        if (written == shipped) {
            ++into.identical;
            continue;
        }
        if (into.problems.size() < kNamedProblems)
            into.problems.push_back(path.filename().string() + ": " + block.type + " is " +
                                    std::to_string(written.size()) + " bytes against " +
                                    std::to_string(shipped.size()) + ", first differs at " +
                                    std::to_string(first_difference(shipped, written)));
    }
}

void check_verbatim(const fs::path& path, RewriteTotals& totals) {
    ++totals.streams;
    std::string source;
    NifScene    scene;
    std::string error;
    if (!read_nif_bytes(path.string(), source) || !read_nif_scene(path.string(), scene, error)) {
        totals.unparsed.push_back(path.string() + ": " + error);
        return;
    }
    ++totals.parsed;
    check_re_encode(source, scene, path, totals);
    std::string built;
    if (!build_nif_stream(source, scene, NifStreamEdit{}, built, error)) {
        totals.different.push_back(path.string() + ": " + error);
        return;
    }
    if (built == source) {
        ++totals.identical;
        return;
    }
    totals.different.push_back(path.string() + ": first differs at byte " +
                               std::to_string(first_difference(source, built)) + " of " +
                               std::to_string(source.size()));
}

void walk(const fs::path& root, const std::string& extension, RewriteTotals& totals) {
    std::error_code failure;
    if (fs::is_regular_file(root, failure)) {
        check_verbatim(root, totals);
        return;
    }
    for (const fs::directory_entry& found : fs::recursive_directory_iterator(root, failure))
        if (found.is_regular_file(failure) && extension_matches(found.path(), extension))
            check_verbatim(found.path(), totals);
    if (failure) std::cerr << "nif_rewrite: cannot walk " << root << ": " << failure.message()
                           << "\n";
}

// returns the offset only if the needle appears exactly once else npos
size_t only_occurrence(const std::string& body, const char* needle, size_t length) {
    const size_t first = body.find(std::string(needle, length));
    if (first == std::string::npos) return std::string::npos;
    return body.find(std::string(needle, length), first + 1) == std::string::npos
               ? first
               : std::string::npos;
}

// finds a block whose non zero translation bytes appear exactly once so it is safe to edit
bool find_editable_node(const std::string& source, const NifScene& scene, uint32_t& block_out,
                        size_t& offset_out) {
    for (uint32_t index = 0; index < scene.blocks.size(); ++index) {
        const NifBlock& block = scene.blocks[index];
        if (!block.has_transform) continue;
        const float* at = block.transform.translation;
        if (at[0] == 0.f && at[1] == 0.f && at[2] == 0.f) continue;
        char needle[12];
        std::memcpy(needle, at, sizeof(needle));
        const std::string body = nif_block_body(source, block);
        const size_t found = only_occurrence(body, needle, sizeof(needle));
        if (found == std::string::npos) continue;
        block_out = index;
        offset_out = static_cast<size_t>(block.byte_offset) + found;
        return true;
    }
    return false;
}

// count differing bytes and first position
size_t differing_bytes(const std::string& left, const std::string& right, size_t& first) {
    first = std::string::npos;
    size_t count = 0;
    for (size_t at = 0; at < std::min(left.size(), right.size()); ++at) {
        if (left[at] == right[at]) continue;
        if (first == std::string::npos) first = at;
        ++count;
    }
    return count + (left.size() > right.size() ? left.size() - right.size()
                                               : right.size() - left.size());
}

bool check_targeted(const fs::path& path, const fs::path& out_dir) {
    std::string source;
    NifScene    scene;
    std::string error;
    if (!read_nif_bytes(path.string(), source) || !read_nif_scene(path.string(), scene, error)) {
        std::cerr << "nif_rewrite: cannot open " << path << ": " << error << "\n";
        return false;
    }
    uint32_t block = 0;
    size_t   offset = 0;
    if (!find_editable_node(source, scene, block, offset)) {
        std::cerr << "nif_rewrite: " << path << " has no node with a unique non-zero"
                  << " translation to edit\n";
        return false;
    }
    const float moved[3] = {scene.blocks[block].transform.translation[0] + 12.5f,
                            scene.blocks[block].transform.translation[1] - 3.25f,
                            scene.blocks[block].transform.translation[2] + 0.75f};
    NifStreamEdit edit;
    edit.blocks.push_back({block, nif_block_body(source, scene.blocks[block])});
    std::memcpy(edit.blocks.front().body.data() +
                    (offset - scene.blocks[block].byte_offset),
                moved, sizeof(moved));

    const fs::path written = out_dir / path.filename();
    if (!write_nif_stream(path.string(), scene, edit, written.string(), error)) {
        std::cerr << "nif_rewrite: " << error << "\n";
        return false;
    }
    std::string produced;
    if (!read_nif_bytes(written.string(), produced)) {
        std::cerr << "nif_rewrite: cannot read back " << written << "\n";
        return false;
    }

    size_t       first = 0;
    const size_t changed = differing_bytes(source, produced, first);
    const bool   span_is_right = produced.size() == source.size() && changed <= sizeof(moved) &&
                               first >= offset && first < offset + sizeof(moved);
    NifScene reloaded;
    const bool reloads = read_nif_scene(written.string(), reloaded, error);
    const bool value_landed =
        reloads && block < reloaded.blocks.size() &&
        std::memcmp(reloaded.blocks[block].transform.translation, moved, sizeof(moved)) == 0;

    std::cout << "  edited block " << block << " (" << scene.blocks[block].type << " '"
              << scene.blocks[block].name << "') translation at byte " << offset << "\n"
              << "  bytes changed          " << changed << " (expected at most "
              << sizeof(moved) << "), first at " << first << "\n"
              << "  length unchanged       " << (produced.size() == source.size() ? "yes" : "no")
              << "\n"
              << "  change inside the span " << (span_is_right ? "yes" : "NO") << "\n"
              << "  reloads                " << (reloads ? "yes" : "NO - " + error) << "\n"
              << "  new value reads back   " << (value_landed ? "yes" : "NO") << "\n"
              << "  written to             " << written.string() << "\n";
    return span_is_right && reloads && value_landed;
}

// bytes before the edited block must match except the size table entry
bool head_is_kept(const std::string& source, const std::string& built, const NifScene& scene,
                  uint32_t block, size_t head_length) {
    size_t entry = std::string::npos;
    if (!scene.header.object_sizes.empty())
        entry = static_cast<size_t>(scene.header.object_sizes_offset) +
                size_t(block) * sizeof(uint32_t);
    for (size_t at = 0; at < head_length; ++at) {
        if (built[at] == source[at]) continue;
        if (entry != std::string::npos && at >= entry && at < entry + sizeof(uint32_t)) continue;
        return false;
    }
    return true;
}

// grows one block body and checks the tail shifts forward by the same amount
bool check_resize(const fs::path& path) {
    std::string source;
    NifScene    scene;
    std::string error;
    if (!read_nif_bytes(path.string(), source) || !read_nif_scene(path.string(), scene, error))
        return false;
    if (scene.blocks.size() < 2) return false;
    constexpr size_t kGrowth = 7;
    const uint32_t   block = 0;
    NifStreamEdit    edit;
    edit.blocks.push_back(
        {block, nif_block_body(source, scene.blocks[block]) + std::string(kGrowth, '\x5A')});
    std::string built;
    if (!build_nif_stream(source, scene, edit, built, error)) {
        std::cerr << "nif_rewrite: " << error << "\n";
        return false;
    }
    const size_t after = static_cast<size_t>(scene.blocks[block].byte_offset +
                                             scene.blocks[block].byte_length);
    const bool grew = built.size() == source.size() + kGrowth;
    const bool head_kept = head_is_kept(source, built, scene, block, after);
    const bool tail_kept =
        built.compare(after + kGrowth, source.size() - after, source, after,
                      source.size() - after) == 0;
    std::cout << "  grew by " << kGrowth << " bytes  " << (grew ? "yes" : "NO") << "\n"
              << "  bytes before intact   " << (head_kept ? "yes" : "NO") << "\n"
              << "  bytes after shifted   " << (tail_kept ? "yes" : "NO") << "\n";
    return grew && head_kept && tail_kept;
}

// grows extra data link list in the NiObjectNET body by one no link entry
bool grow_extra_data_list(const std::string& body, std::string& out) {
    constexpr size_t kCountOffset = 4;   // past the name's string-table index
    if (body.size() < kCountOffset + sizeof(uint32_t)) return false;
    uint32_t link_count = 0;
    std::memcpy(&link_count, body.data() + kCountOffset, sizeof(link_count));
    if (link_count > 1024) return false;                       // 1024 is a sanity guard not a real limit
    const size_t insert_at = kCountOffset + sizeof(uint32_t) + link_count * sizeof(uint32_t);
    if (insert_at > body.size()) return false;
    const uint32_t grown = link_count + 1;
    constexpr uint32_t kNoLinkSentinel = 0xFFFFFFFFu;
    out = body;
    std::memcpy(out.data() + kCountOffset, &grown, sizeof(grown));
    out.insert(insert_at, std::string(reinterpret_cast<const char*>(&kNoLinkSentinel),
                                      sizeof(kNoLinkSentinel)));
    return true;
}

// finds the first NiNode whose extra data list can grow
bool find_growable_node(const NifScene& scene, const std::string& source, uint32_t& block_out,
                        std::string& grown_out) {
    if (scene.header.version < 0x14010001u) return false;      // only test streams from version 20 1 0 1 up older layout differs
    for (uint32_t index = 0; index < scene.blocks.size(); ++index) {
        if (scene.blocks[index].type != "NiNode") continue;
        if (!grow_extra_data_list(nif_block_body(source, scene.blocks[index]), grown_out)) continue;
        block_out = index;
        return true;
    }
    return false;
}

// blocks after the edited one shift by the growth blocks before stay put
bool boundaries_shifted(const NifScene& before, const NifScene& after, uint32_t edited,
                        uint64_t growth, std::string& note) {
    if (before.blocks.size() != after.blocks.size()) {
        note = "the reload has a different block count";
        return false;
    }
    for (uint32_t index = 0; index < before.blocks.size(); ++index) {
        const uint64_t expected_offset =
            before.blocks[index].byte_offset + (index > edited ? growth : 0);
        const uint64_t expected_length =
            before.blocks[index].byte_length + (index == edited ? growth : 0);
        if (after.blocks[index].byte_offset == expected_offset &&
            after.blocks[index].byte_length == expected_length)
            continue;
        note = "block " + std::to_string(index) + " reloads at " +
               std::to_string(after.blocks[index].byte_offset) + "+" +
               std::to_string(after.blocks[index].byte_length) + ", expected " +
               std::to_string(expected_offset) + "+" + std::to_string(expected_length);
        return false;
    }
    return true;
}

// grows a block through the writer and checks offsets and the size table stay correct
bool check_table_resize(const fs::path& path, const fs::path& out_dir) {
    std::string source;
    NifScene    scene;
    std::string error;
    if (!read_nif_bytes(path.string(), source) || !read_nif_scene(path.string(), scene, error)) {
        std::cerr << "nif_rewrite: cannot open " << path << ": " << error << "\n";
        return false;
    }
    NifBlockEdit grown;
    if (!find_growable_node(scene, source, grown.block, grown.body)) {
        std::cerr << "nif_rewrite: " << path << " has no node whose extra-data list can grow\n";
        return false;
    }
    NifStreamEdit edit;
    edit.blocks.push_back(grown);
    const uint64_t growth = grown.body.size() - scene.blocks[grown.block].byte_length;
    const fs::path written = out_dir / ("grown-" + path.filename().string());
    if (!write_nif_stream(path.string(), scene, edit, written.string(), error)) {
        std::cerr << "nif_rewrite: " << error << "\n";
        return false;
    }
    NifScene reloaded;
    const bool reloads = read_nif_scene(written.string(), reloaded, error);
    std::string note;
    const bool boundaries =
        reloads && boundaries_shifted(scene, reloaded, grown.block, growth, note);
    const bool table_agrees =
        reloaded.header.object_sizes.empty() ||
        (grown.block < reloaded.header.object_sizes.size() &&
         reloaded.header.object_sizes[grown.block] == grown.body.size());

    std::cout << "  stream version         " << (scene.header.version >> 24) << "."
              << ((scene.header.version >> 16) & 0xFFu) << "."
              << ((scene.header.version >> 8) & 0xFFu) << "."
              << (scene.header.version & 0xFFu)
              << (scene.header.object_sizes.empty() ? " (no size table)" : " (size table)") << "\n"
              << "  grew block " << grown.block << " by " << growth << " bytes, now "
              << grown.body.size() << "\n"
              << "  reloads                " << (reloads ? "yes" : "NO - " + error) << "\n"
              << "  size table agrees      " << (table_agrees ? "yes" : "NO") << "\n"
              << "  every boundary right   " << (boundaries ? "yes" : "NO - " + note) << "\n"
              << "  written to             " << written.string() << "\n";
    // reader checks the decoded block size against the table from version 20 2 0 5
    return reloads && table_agrees && boundaries;
}

// block names become string table indices from version 20 1 0 1
constexpr uint32_t kStringTableFrom = 0x14010001u;

// prints one aligned check line label padded to a fixed column
void print_check(const char* what, bool held, const std::string& why) {
    std::cout << "  " << std::left << std::setw(23) << what
              << (held ? "yes" : why.empty() ? "NO" : "NO - " + why) << "\n";
}

// first block whose name is not already this given name
bool find_named_block(const NifScene& scene, const std::string& name, uint32_t& block_out) {
    for (uint32_t index = 0; index < scene.blocks.size(); ++index) {
        if (!scene.blocks[index].has_name || scene.blocks[index].name == name) continue;
        block_out = index;
        return true;
    }
    return false;
}

// checks every block besides the renamed one keeps its original name
bool only_one_renamed(const NifScene& before, const NifScene& after, uint32_t renamed,
                      std::string& note) {
    if (before.blocks.size() != after.blocks.size()) {
        note = "the reload has a different block count";
        return false;
    }
    for (uint32_t index = 0; index < before.blocks.size(); ++index) {
        if (index == renamed || before.blocks[index].name == after.blocks[index].name) continue;
        note = "block " + std::to_string(index) + " was renamed too, from '" +
               before.blocks[index].name + "' to '" + after.blocks[index].name + "'";
        return false;
    }
    return true;
}

// block index and new name to write and read back
struct Rename {
    uint32_t    block = 0;
    std::string name;
};

// splices the new name into the block body and writes the renamed stream to disk
bool write_renamed(const fs::path& path, const NifScene& scene, const std::string& source,
                   const Rename& rename, const fs::path& written, NifStreamEdit& edit) {
    edit.blocks.push_back({rename.block, nif_block_body(source, scene.blocks[rename.block])});
    if (!splice_nif_name(scene.header, scene.blocks[rename.block], rename.name,
                         edit.blocks.front().body, edit.added_strings)) {
        std::cerr << "nif_rewrite: block " << rename.block << " of " << path
                  << " will not rename\n";
        return false;
    }
    std::string error;
    if (write_nif_stream(path.string(), scene, edit, written.string(), error)) return true;
    std::cerr << "nif_rewrite: " << error << "\n";
    return false;
}

// renames a named block and checks only its name and the string table changed
bool check_rename(const fs::path& path, const fs::path& out_dir) {
    std::string source;
    NifScene    scene;
    std::string error;
    if (!read_nif_bytes(path.string(), source) || !read_nif_scene(path.string(), scene, error)) {
        std::cerr << "nif_rewrite: cannot open " << path << ": " << error << "\n";
        return false;
    }
    const std::string wanted = "xlemu renamed node";
    uint32_t          block = 0;
    if (!find_named_block(scene, wanted, block)) {
        std::cerr << "nif_rewrite: " << path << " carries no block with a name field\n";
        return false;
    }
    NifStreamEdit  edit;
    const fs::path written = out_dir / ("renamed-" + path.filename().string());
    if (!write_renamed(path, scene, source, {block, wanted}, written, edit)) return false;

    NifScene    reloaded;
    const bool  reloads = read_nif_scene(written.string(), reloaded, error);
    std::string note;
    const bool  landed = reloads && reloaded.blocks[block].name == wanted;
    const bool  alone = reloads && only_one_renamed(scene, reloaded, block, note);
    const bool  table_grew =
        reloads && reloaded.header.strings.size() == scene.header.strings.size() +
                                                        edit.added_strings.size();
    std::cout << "  renamed block " << block << " (" << scene.blocks[block].type << " '"
              << scene.blocks[block].name << "')\n"
              << "  name is " << (scene.header.version >= kStringTableFrom
                                      ? "a string table index"
                                      : "bytes of the body")
              << ", added " << edit.added_strings.size() << " table entr"
              << (edit.added_strings.size() == 1 ? "y" : "ies") << "\n";
    print_check("reloads", reloads, error);
    print_check("new name reads back", landed, "");
    print_check("string table grew", table_grew, "");
    print_check("no other block renamed", alone, note);
    std::cout << "  written to             " << written.string() << "\n";
    return reloads && landed && alone && table_grew;
}

// builds a unit cube 8 corners 12 triangles with normals and uvs
NifTriShapeGeometry unit_cube() {
    NifTriShapeGeometry geometry;
    constexpr float kCornerNormal = 0.57735026f;   // one divided by sqrt three
    for (int corner = 0; corner < 8; ++corner) {
        const float x = (corner & 1) != 0 ? 1.f : -1.f;
        const float y = (corner & 2) != 0 ? 1.f : -1.f;
        const float z = (corner & 4) != 0 ? 1.f : -1.f;
        geometry.positions.insert(geometry.positions.end(), {x, y, z});
        geometry.normals.insert(geometry.normals.end(),
                                {x * kCornerNormal, y * kCornerNormal, z * kCornerNormal});
        geometry.uvs.insert(geometry.uvs.end(), {(x + 1.f) * 0.5f, (y + 1.f) * 0.5f});
    }
    static const uint16_t kFaces[] = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 1, 5, 0, 5, 4,
                                      2, 6, 7, 2, 7, 3, 0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5};
    geometry.triangles.assign(std::begin(kFaces), std::end(kFaces));
    return geometry;
}

// stream holds NiTriShapeData block
bool holds_data_block(const fs::path& path, uint32_t& block_out) {
    NifScene    scene;
    std::string error;
    if (!read_nif_scene(path.string(), scene, error)) return false;
    for (uint32_t index = 0; index < scene.blocks.size(); ++index) {
        if (scene.blocks[index].type != "NiTriShapeData") continue;
        block_out = index;
        return true;
    }
    return false;
}

// first stream under root carries data block
bool find_stream_with_data_block(const fs::path& root, const std::string& extension,
                                 fs::path& path_out, uint32_t& block_out) {
    path_out = root;
    std::error_code failure;
    if (fs::is_regular_file(root, failure)) return holds_data_block(root, block_out);
    for (const fs::directory_entry& found : fs::recursive_directory_iterator(root, failure)) {
        if (!found.is_regular_file(failure) || !extension_matches(found.path(), extension))
            continue;
        if (!holds_data_block(found.path(), block_out)) continue;
        path_out = found.path();
        return true;
    }
    return false;
}

// block arrays match float for float
bool synthesised_arrays_match(const NifBlock& reloaded, const NifTriShapeGeometry& authored) {
    return reloaded.vertices == authored.positions && reloaded.normals == authored.normals &&
           reloaded.uvs == authored.uvs && reloaded.triangles == authored.triangles;
}

// encodes a body for geometry that was never read from a file
bool synthesised_body(const NifHeader& header, const NifTriShapeGeometry& authored,
                      std::string& body, std::string& error) {
    NifBlock built;
    return build_nif_tri_shape_data(authored, built, error) &&
           encode_nif_block(header, built, body, error);
}

// results for reload array match size table and rewrite checks
struct SynthesisedChecks {
    bool reloads      = false;
    bool arrays       = false;
    bool table_agrees = false;
    bool rewrites     = false;

    bool all() const { return reloads && arrays && table_agrees && rewrites; }
};

SynthesisedChecks check_written_stream(const fs::path& written, uint32_t block,
                                       const NifTriShapeGeometry& authored, size_t body_length,
                                       std::string& error) {
    SynthesisedChecks held;
    NifScene reloaded;
    held.reloads = read_nif_scene(written.string(), reloaded, error);
    if (!held.reloads) return held;
    held.arrays = block < reloaded.blocks.size() &&
                  synthesised_arrays_match(reloaded.blocks[block], authored);
    held.table_agrees = reloaded.header.object_sizes.empty() ||
                        (block < reloaded.header.object_sizes.size() &&
                         reloaded.header.object_sizes[block] == body_length);
    // rewrite must reproduce the written stream byte for byte
    std::string bytes;
    std::string rewritten;
    held.rewrites = read_nif_bytes(written.string(), bytes) &&
                    build_nif_stream(bytes, reloaded, NifStreamEdit{}, rewritten, error) &&
                    rewritten == bytes;
    return held;
}

// builds a synthetic NiTriShapeData splices it into a real stream and reads it back
bool check_synthesised_data(const fs::path& root, const std::string& extension,
                            const fs::path& out_dir) {
    fs::path path;
    uint32_t block = 0;
    if (!find_stream_with_data_block(root, extension, path, block)) {
        std::cerr << "nif_rewrite: no stream under " << root << " carries an NiTriShapeData\n";
        return false;
    }
    std::cout << "synthesised block, " << path.filename().string() << ":\n";
    NifScene    scene;
    std::string error;
    if (!read_nif_scene(path.string(), scene, error)) {
        print_check("opens", false, error);
        return false;
    }
    const NifTriShapeGeometry authored = unit_cube();
    NifStreamEdit             edit;
    edit.blocks.push_back({block, std::string()});
    if (!synthesised_body(scene.header, authored, edit.blocks.front().body, error)) {
        print_check("builds", false, error);
        return false;
    }
    const fs::path written = out_dir / ("synthesised-" + path.filename().string());
    if (!write_nif_stream(path.string(), scene, edit, written.string(), error)) {
        print_check("writes", false, error);
        return false;
    }

    const SynthesisedChecks held =
        check_written_stream(written, block, authored, edit.blocks.front().body.size(), error);
    std::cout << "  replaced block " << block << ", " << scene.blocks[block].byte_length
              << " bytes of stream become " << edit.blocks.front().body.size()
              << " bytes nothing read\n";
    print_check("reloads", held.reloads, error);
    print_check("arrays read back", held.arrays, "");
    print_check("size table agrees", held.table_agrees, "");
    print_check("rewrites verbatim", held.rewrites, "");
    std::cout << "  written to             " << written.string() << "\n";
    return held.all();
}

void print_totals(const RewriteTotals& totals) {
    std::cout << totals.identical << "/" << totals.streams
              << " streams rewrite byte-identical (" << totals.parsed << " parsed)\n";
    for (const std::string& line : totals.unparsed) std::cout << "  unparsed " << line << "\n";
    for (const std::string& line : totals.different) std::cout << "  DIFFERS  " << line << "\n";
    std::cout << "re-encoded from the parse alone, no source byte consulted:\n";
    for (int family = 0; family < kEncodedTypeCount; ++family) {
        const EncodeTotals& one = totals.encoded[family];
        std::cout << "  " << std::left << std::setw(16) << kEncodedTypes[family]
                  << one.identical << "/" << one.blocks << " byte-identical, " << one.refused
                  << " refused\n";
        for (const std::string& line : one.problems) std::cout << "    DIFFERS " << line << "\n";
    }
}

// true only if every encoded block matched byte for byte
bool re_encode_clean(const RewriteTotals& totals) {
    for (const EncodeTotals& one : totals.encoded)
        if (one.identical != one.blocks) return false;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: nif_rewrite <directory|file> <output dir> [extension]\n"
                     "       the output dir is where the one edited stream is written;\n"
                     "       the verbatim pass compares in memory and writes nothing.\n";
        return 2;
    }
    const fs::path    root(argv[1]);
    const fs::path    out_dir(argv[2]);
    const std::string extension = argc > 3 ? argv[3] : ".nif";

    RewriteTotals totals;
    walk(root, extension, totals);
    print_totals(totals);

    fs::path sample = root;
    std::error_code failure;
    if (fs::is_directory(root, failure))
        for (const fs::directory_entry& found : fs::recursive_directory_iterator(root, failure))
            if (found.is_regular_file(failure) && extension_matches(found.path(), extension)) {
                NifScene    scene;
                std::string error;
                uint32_t    block = 0;
                size_t      offset = 0;
                std::string bytes;
                if (read_nif_bytes(found.path().string(), bytes) &&
                    read_nif_scene(found.path().string(), scene, error) &&
                    find_editable_node(bytes, scene, block, offset)) {
                    sample = found.path();
                    break;
                }
            }

    std::cout << "targeted edit, " << sample.filename().string() << ":\n";
    const bool targeted = check_targeted(sample, out_dir);
    std::cout << "resize splice, " << sample.filename().string() << ":\n";
    const bool resized = check_resize(sample);
    std::cout << "resize through the size table, " << sample.filename().string() << ":\n";
    const bool regrown = check_table_resize(sample, out_dir);
    std::cout << "rename through the string table, " << sample.filename().string() << ":\n";
    const bool renamed = check_rename(sample, out_dir);

    const bool synthesised = check_synthesised_data(root, extension, out_dir);

    const bool verbatim_clean = totals.streams > 0 && totals.identical == totals.streams;
    const bool clean = verbatim_clean && re_encode_clean(totals) && targeted && resized &&
                       regrown && renamed && synthesised;
    std::cout << (clean ? "nif_rewrite: PASS\n" : "nif_rewrite: FAIL\n");
    return clean ? 0 : 1;
}
