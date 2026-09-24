// walks a directory of NIF streams and reports parse totals and decoder failures
#include "engine/formats/nif_animation_eval.h"
#include "tools/nif/nif_effect_census.h"
#include "engine/formats/nif_reader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

const char* interpolation_name(KnC::NifKeyInterpolation interpolation) {
    switch (interpolation) {
        case KnC::NifKeyInterpolation::Linear:                return "linear";
        case KnC::NifKeyInterpolation::Quadratic:             return "quadratic";
        case KnC::NifKeyInterpolation::TensionContinuityBias: return "tcb";
        case KnC::NifKeyInterpolation::XyzRotations:          return "xyz-rotations";
        case KnC::NifKeyInterpolation::Step:                  return "step";
        case KnC::NifKeyInterpolation::None:                  break;
    }
    return "none";
}

const char* transform_name(KnC::NifTextureTransformOperation operation) {
    switch (operation) {
        case KnC::NifTextureTransformOperation::TranslateV: return "translate v";
        case KnC::NifTextureTransformOperation::Rotate:     return "rotate";
        case KnC::NifTextureTransformOperation::ScaleU:     return "scale u";
        case KnC::NifTextureTransformOperation::ScaleV:     return "scale v";
        case KnC::NifTextureTransformOperation::TranslateU: break;
    }
    return "translate u";
}

const char* cycle_name(KnC::NifCycleType cycle) {
    switch (cycle) {
        case KnC::NifCycleType::Reverse: return "reverse";
        case KnC::NifCycleType::Clamp:   return "clamp";
        case KnC::NifCycleType::Loop:    break;
    }
    return "loop";
}

// extension compare ignores case
bool extension_matches(const std::string& found, const std::string& wanted) {
    if (found.size() != wanted.size()) return false;
    for (size_t i = 0; i < found.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(found[i])) !=
            std::tolower(static_cast<unsigned char>(wanted[i]))) return false;
    return true;
}

bool ends_with(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_controller(const std::string& type) {
    return ends_with(type, "Controller") || ends_with(type, "Ctlr");
}

// running totals for animation controllers keys and text keys across all files
struct AnimationTotals {
    size_t files_with_animation = 0;
    size_t interpolators = 0;
    size_t key_data_blocks = 0;
    size_t keys = 0;
    size_t sequences = 0;
    size_t sequence_entries = 0;
    size_t sequences_with_text_keys = 0;
    size_t text_key_blocks = 0;
    size_t text_keys = 0;
    std::map<std::string, size_t> controllers;
    std::map<std::string, size_t> keys_by_interpolation;
    // count per text key name the client callback actually dispatches on
    std::map<std::string, size_t> dispatched_text_keys;
    size_t other_text_keys = 0;
    std::set<std::string> distinct_text_keys;
};

// dispatched key names from HBOnline 0x00758320 to 0x00758344
bool is_dispatched_text_key(const std::string& text) {
    static const char* const kDispatched[] = {"start", "end", "e",  "e1", "e2",
                                              "e3",    "s",   "s1", "s2", "s3"};
    for (const char* name : kDispatched)
        if (text == name) return true;
    return false;
}

// worst deviation of weight sum from 1 and its file
struct WeightSumWorst {
    double      deviation = 0.0;
    std::string path;
};

// running totals for skin instances weights partitions and range errors
struct SkinTotals {
    size_t files_with_skin = 0;
    size_t files_with_partition = 0;
    size_t instances = 0;
    size_t instance_bones = 0;
    size_t most_bones_on_one_skin = 0;
    size_t data_blocks = 0;
    size_t data_blocks_with_weights = 0;
    size_t weights = 0;
    size_t partition_blocks = 0;
    size_t partitions = 0;
    size_t partition_vertices = 0;
    size_t partition_weights = 0;
    size_t skinned_geometry = 0;
    size_t index_errors = 0;
    size_t unbound_partition_vertices = 0;
    std::map<uint32_t, size_t> partitions_by_bones_per_vertex;
    WeightSumWorst data_weight_sum;
    WeightSumWorst partition_weight_sum;
};

struct SmokeTotals {
    AnimationTotals animation;
    SkinTotals skin;
    KnC::census::EffectTotals effect;
    size_t files_with_tiled_spans = 0;
    std::string first_untiled_file;
    size_t files = 0;
    size_t vertices = 0;
    size_t triangles = 0;
    size_t wall_nodes = 0;
    size_t uv_pairs = 0;
    size_t normal_vectors = 0;
    size_t mesh_data_blocks = 0;
    size_t mesh_data_with_uvs = 0;
    size_t mesh_data_with_normals = 0;
    size_t geometry_blocks = 0;
    size_t geometry_blocks_with_texture = 0;
    // NiTriStripsData strips the reader expands into triangles
    size_t strips = 0;
    size_t strip_corners = 0;
    // count of light blocks kept and how many have attenuation
    size_t lights = 0;
    size_t lights_with_attenuation = 0;
    std::set<std::string> texture_names;
    std::map<std::string, size_t> blockers;
    std::map<uint64_t, size_t> trailing;
    std::map<uint32_t, size_t> versions;
    std::map<uint32_t, size_t> failed_versions;
    // block types with no decoder counted per type and per file
    std::map<std::string, size_t> skipped_blocks;
    std::map<std::string, size_t> skipped_files;
};

std::string version_text(uint32_t version) {
    return std::to_string(version >> 24) + "." + std::to_string((version >> 16) & 0xFFu) +
           "." + std::to_string((version >> 8) & 0xFFu) + "." +
           std::to_string(version & 0xFFu);
}

// turns a block error into a short reason mis sized wrong decoder or not written
std::string blocking_reason(const std::string& error) {
    if (error.rfind("block ", 0) != 0) return error;
    const size_t quote = error.find('\'');
    if (quote == std::string::npos) return error;
    const size_t end = error.find('\'', quote + 1);
    const std::string type = error.substr(quote + 1, end - quote - 1);
    if (error.find("MIS-SIZED") != std::string::npos) return type + " (MIS-SIZED)";
    if (error.find("FAILED") != std::string::npos) return type + " (decoder is WRONG)";
    return type + " (not written)";
}

bool is_geometry(const KnC::NifBlock& block) {
    return block.type == "NiTriShape" || block.type == "NiTriStrips";
}

bool carries_animation(const KnC::NifScene& scene) {
    for (const auto& block : scene.blocks)
        if (block.animation) return true;
    return false;
}

void accumulate_key_group(const KnC::NifKeyGroup& group, AnimationTotals& totals) {
    if (group.key_count() == 0) return;
    totals.keys += group.key_count();
    totals.keys_by_interpolation[interpolation_name(group.interpolation)] +=
        group.key_count();
}

void accumulate_key_data(const KnC::NifAnimation& animation, AnimationTotals& totals) {
    const size_t before = totals.keys;
    accumulate_key_group(animation.channel, totals);
    accumulate_key_group(animation.translations, totals);
    accumulate_key_group(animation.rotations, totals);
    accumulate_key_group(animation.scales, totals);
    for (const auto& axis : animation.rotation_axes) accumulate_key_group(axis, totals);
    if (totals.keys != before) ++totals.key_data_blocks;
}

void accumulate_text_keys(const std::vector<KnC::NifTextKey>& keys,
                          AnimationTotals& totals) {
    if (keys.empty()) return;
    ++totals.text_key_blocks;
    totals.text_keys += keys.size();
    for (const KnC::NifTextKey& key : keys) {
        totals.distinct_text_keys.insert(key.text);
        if (is_dispatched_text_key(key.text)) ++totals.dispatched_text_keys[key.text];
        else ++totals.other_text_keys;
    }
}

void accumulate_animation(const KnC::NifBlock& block, AnimationTotals& totals) {
    if (!block.animation) return;
    if (is_controller(block.type)) ++totals.controllers[block.type];
    if (ends_with(block.type, "Interpolator")) ++totals.interpolators;
    if (!block.animation->sequence.entries.empty()) {
        ++totals.sequences;
        totals.sequence_entries += block.animation->sequence.entries.size();
        if (block.animation->sequence.text_keys_link != KnC::kNoLink)
            ++totals.sequences_with_text_keys;
    }
    accumulate_text_keys(block.animation->text_keys, totals);
    accumulate_key_data(*block.animation, totals);
}

// resolved skin instance data and partition for one geometry block
struct ResolvedSkin {
    const KnC::NifSkin* instance = nullptr;
    const KnC::NifSkin* data = nullptr;
    const KnC::NifSkin* partition = nullptr;
    size_t vertex_count = 0;
};

const KnC::NifSkin* skin_at(const KnC::NifScene& scene, uint32_t link) {
    if (link >= scene.blocks.size()) return nullptr;
    return scene.blocks[link].skin.get();
}

// resolves the skin instance data and partition blocks and the vertex count
bool resolve_skin(const KnC::NifScene& scene, const KnC::NifBlock& geometry,
                  ResolvedSkin& out) {
    if (!is_geometry(geometry)) return false;
    out.instance = skin_at(scene, geometry.skin_instance_link);
    if (out.instance == nullptr) return false;
    out.data = skin_at(scene, out.instance->data_link);
    out.partition = skin_at(scene, out.instance->partition_link);
    if (geometry.data_link < scene.blocks.size())
        out.vertex_count = scene.blocks[geometry.data_link].vertices.size() / 3;
    return true;
}

// sums NiSkinData weights per vertex worst deviation from 1 across all
double worst_data_weight_sum(const ResolvedSkin& skin) {
    std::vector<double> sums(skin.vertex_count, 0.0);
    for (const auto& bone : skin.data->bone_binds)
        for (size_t weight = 0; weight < bone.weights.size(); ++weight)
            if (bone.vertex_indices[weight] < sums.size())
                sums[bone.vertex_indices[weight]] += bone.weights[weight];
    double worst = 0.0;
    for (const double sum : sums)
        if (sum != 0.0) worst = std::max(worst, std::abs(sum - 1.0));
    return worst;
}

// sums partition weights per vertex counts unbound vertices whose sum is zero
double worst_partition_weight_sum(const KnC::NifSkin& partition, size_t& unbound) {
    double worst = 0.0;
    for (const auto& record : partition.partitions) {
        const size_t stride = record.bones_per_vertex;
        if (stride == 0) continue;
        for (size_t base = 0; base + stride <= record.weights.size(); base += stride) {
            double sum = 0.0;
            for (size_t slot = 0; slot < stride; ++slot) sum += record.weights[base + slot];
            if (sum == 0.0) ++unbound;
            else worst = std::max(worst, std::abs(sum - 1.0));
        }
    }
    return worst;
}

// counts partition bone and vertex indices that fall outside their range
size_t count_partition_range_errors(const ResolvedSkin& skin) {
    size_t errors = 0;
    for (const auto& record : skin.partition->partitions) {
        for (const uint16_t bone : record.bones)
            if (bone >= skin.instance->bones.size()) ++errors;
        for (const uint8_t index : record.bone_indices)
            if (index >= record.bones.size()) ++errors;
        for (const uint16_t vertex : record.vertex_map)
            if (vertex >= skin.vertex_count) ++errors;
    }
    return errors;
}

// counts NiSkinData vertex indices that fall outside the mesh vertex count
size_t count_data_range_errors(const ResolvedSkin& skin) {
    size_t errors = skin.data->bone_binds.size() == skin.instance->bones.size() ? 0 : 1;
    for (const auto& bone : skin.data->bone_binds)
        for (const uint16_t vertex : bone.vertex_indices)
            if (vertex >= skin.vertex_count) ++errors;
    return errors;
}

void note_weight_sum(double deviation, const std::string& path, WeightSumWorst& worst) {
    if (deviation <= worst.deviation) return;
    worst.deviation = deviation;
    worst.path = path;
}

void accumulate_skin_block(const KnC::NifBlock& block, SkinTotals& totals) {
    const KnC::NifSkin& skin = *block.skin;
    if (block.type == "NiSkinInstance") {
        ++totals.instances;
        totals.instance_bones += skin.bones.size();
        totals.most_bones_on_one_skin =
            std::max(totals.most_bones_on_one_skin, skin.bones.size());
        return;
    }
    if (block.type == "NiSkinData") {
        ++totals.data_blocks;
        size_t weights = 0;
        for (const auto& bone : skin.bone_binds) weights += bone.weights.size();
        if (weights != 0) ++totals.data_blocks_with_weights;
        totals.weights += weights;
        return;
    }
    // third skin block family NiSkinPartition
    if (block.type != "NiSkinPartition") return;
    ++totals.partition_blocks;
    totals.partitions += skin.partitions.size();
    for (const auto& record : skin.partitions) {
        totals.partition_vertices += record.vertex_map.size();
        totals.partition_weights += record.weights.size();
        ++totals.partitions_by_bones_per_vertex[record.bones_per_vertex];
    }
}

// checks every skinned geometry block in one file for range and weight errors
void check_skinned_geometry(const KnC::NifScene& scene, const std::string& path,
                            SkinTotals& totals) {
    for (const auto& block : scene.blocks) {
        ResolvedSkin skin;
        if (!resolve_skin(scene, block, skin)) continue;
        ++totals.skinned_geometry;
        if (skin.data != nullptr) {
            totals.index_errors += count_data_range_errors(skin);
            note_weight_sum(worst_data_weight_sum(skin), path, totals.data_weight_sum);
        }
        if (skin.partition == nullptr) continue;
        totals.index_errors += count_partition_range_errors(skin);
        note_weight_sum(
            worst_partition_weight_sum(*skin.partition, totals.unbound_partition_vertices),
            path, totals.partition_weight_sum);
    }
}

void accumulate_skin(const KnC::NifScene& scene, const std::string& path,
                     SkinTotals& totals) {
    bool carries_skin = false;
    bool carries_partition = false;
    for (const auto& block : scene.blocks) {
        if (!block.skin) continue;
        carries_skin = true;
        carries_partition = carries_partition || block.type == "NiSkinPartition";
        accumulate_skin_block(block, totals);
    }
    if (!carries_skin) return;
    ++totals.files_with_skin;
    if (carries_partition) ++totals.files_with_partition;
    check_skinned_geometry(scene, path, totals);
}

// true if blocks run back to back from the header through to the footer
bool spans_tile_body(const KnC::NifScene& scene, uint64_t file_size) {
    uint64_t expected = scene.header.body_offset;
    for (const auto& block : scene.blocks) {
        if (block.byte_offset != expected) return false;
        expected += block.byte_length;
    }
    return expected == file_size - scene.trailing_bytes;
}

void accumulate_block(const KnC::NifScene& scene, const KnC::NifBlock& block,
                      const KnC::NifSurfaceState& state, SmokeTotals& totals) {
    accumulate_animation(block, totals.animation);
    totals.vertices += block.vertices.size() / 3;
    totals.triangles += block.triangles.size() / 3;
    totals.uv_pairs += block.uvs.size() / 2;
    totals.normal_vectors += block.normals.size() / 3;
    if (block.name.rfind("__true_wall_", 0) == 0) ++totals.wall_nodes;
    if (!block.texture_file_name.empty()) totals.texture_names.insert(block.texture_file_name);
    if (block.geometry_source) {
        totals.strips += block.geometry_source->strip_lengths.size();
        totals.strip_corners += block.geometry_source->strip_indices.size();
    }
    if (block.light) {
        ++totals.lights;
        if (block.light->has_attenuation) ++totals.lights_with_attenuation;
    }
    if (!block.vertices.empty()) {
        ++totals.mesh_data_blocks;
        if (!block.uvs.empty()) ++totals.mesh_data_with_uvs;
        if (!block.normals.empty()) ++totals.mesh_data_with_normals;
    }
    KnC::census::accumulate_effect_block(scene, block, state, totals.effect);
    if (!is_geometry(block)) return;
    ++totals.geometry_blocks;
    if (!KnC::find_base_texture_file_name(scene, block).empty())
        ++totals.geometry_blocks_with_texture;
}

void accumulate_file(const std::string& path, SmokeTotals& totals) {
    ++totals.files;
    KnC::NifScene scene;
    std::string error;
    if (!KnC::read_nif_scene(path, scene, error)) {
        // key groups by stream version and which block type broke the parse
        ++totals.blockers[version_text(scene.header.version) + "  " +
                          blocking_reason(error)];
        ++totals.failed_versions[scene.header.version];
        return;
    }
    ++totals.versions[scene.header.version];
    ++totals.trailing[scene.trailing_bytes];
    std::error_code size_error;
    const uint64_t size = std::filesystem::file_size(path, size_error);
    if (!size_error && spans_tile_body(scene, size)) ++totals.files_with_tiled_spans;
    else if (totals.first_untiled_file.empty()) totals.first_untiled_file = path;
    std::set<std::string> skipped_here;
    for (const auto& block : scene.blocks) {
        if (!block.skipped) continue;
        ++totals.skipped_blocks[version_text(scene.header.version) + "  " + block.type];
        skipped_here.insert(version_text(scene.header.version) + "  " + block.type);
    }
    for (const std::string& kind : skipped_here) ++totals.skipped_files[kind];
    const size_t systems_before = totals.effect.systems;
    const std::vector<KnC::NifSurfaceState> surfaces = KnC::resolve_surface_states(scene);
    for (size_t index = 0; index < scene.blocks.size(); ++index)
        accumulate_block(scene, scene.blocks[index], surfaces[index], totals);
    if (totals.effect.systems != systems_before) ++totals.effect.files_with_particles;
    if (carries_animation(scene)) ++totals.animation.files_with_animation;
    accumulate_skin(scene, path, totals.skin);
}

void print_animation_totals(const AnimationTotals& totals) {
    size_t controllers = 0;
    for (const auto& entry : totals.controllers) controllers += entry.second;
    std::cout << "  animation: " << totals.files_with_animation
              << " file(s) carry any; " << controllers << " controllers, "
              << totals.interpolators << " interpolators, " << totals.keys
              << " keys in " << totals.key_data_blocks << " data blocks; "
              << totals.sequences << " sequences over " << totals.sequence_entries
              << " entries\n";
    std::cout << "    " << totals.sequences_with_text_keys << "/" << totals.sequences
              << " sequence(s) name a text key block; " << totals.text_key_blocks
              << " such block(s) hold " << totals.text_keys << " key(s) under "
              << totals.distinct_text_keys.size() << " distinct name(s)\n";
    for (const auto& entry : totals.dispatched_text_keys)
        std::cout << "    " << entry.second << " '" << entry.first << "' text key(s)\n";
    if (totals.other_text_keys != 0)
        std::cout << "    " << totals.other_text_keys
                  << " text key(s) the client's callback does not dispatch on\n";
    for (const auto& entry : totals.controllers)
        std::cout << "    " << entry.first << " x" << entry.second << "\n";
    for (const auto& entry : totals.keys_by_interpolation)
        std::cout << "    " << entry.second << " " << entry.first
                  << " key(s)\n";
}

void print_skin_totals(const SkinTotals& totals) {
    std::cout << "  skin: " << totals.files_with_skin << " file(s) carry any, "
              << totals.files_with_partition << " of those partitioned; "
              << totals.instances << " NiSkinInstance binding "
              << totals.instance_bones << " bones (most on one skin "
              << totals.most_bones_on_one_skin << "); "
              << totals.data_blocks_with_weights << "/" << totals.data_blocks
              << " NiSkinData carry " << totals.weights << " weights; "
              << totals.partition_blocks << " NiSkinPartition over "
              << totals.partitions << " partitions, " << totals.partition_vertices
              << " mapped vertices and " << totals.partition_weights << " weights\n";
    for (const auto& entry : totals.partitions_by_bones_per_vertex)
        std::cout << "    " << entry.second << " partition(s) at " << entry.first
                  << " bone(s) per vertex\n";
    if (totals.files_with_skin == 0) return;
    std::cout << "    " << totals.skinned_geometry << " skinned geometry block(s), "
              << totals.index_errors << " out-of-range index error(s), "
              << totals.unbound_partition_vertices
              << " partition vertex/vertices bound to no bone\n"
              << "    worst |weight sum - 1|: NiSkinData "
              << totals.data_weight_sum.deviation << " in "
              << totals.data_weight_sum.path << "\n"
              << "    worst |weight sum - 1|: partition "
              << totals.partition_weight_sum.deviation << " in "
              << totals.partition_weight_sum.path << "\n";
}

void print_totals(const SmokeTotals& totals) {
    size_t decoded = 0;
    for (const auto& entry : totals.trailing) decoded += entry.second;
    std::cout << decoded << "/" << totals.files << " files decode fully; "
              << totals.vertices << " vertices, " << totals.triangles
              << " triangles, " << totals.wall_nodes << " __true_wall_ nodes\n"
              << "  " << totals.uv_pairs << " uv pairs in "
              << totals.mesh_data_with_uvs << "/" << totals.mesh_data_blocks
              << " mesh data blocks\n"
              << "  " << totals.normal_vectors << " normals in "
              << totals.mesh_data_with_normals << "/" << totals.mesh_data_blocks
              << " mesh data blocks\n"
              << "  " << totals.strips << " strips of " << totals.strip_corners
              << " corners retained\n"
              << "  " << totals.lights << " light(s) retained, "
              << totals.lights_with_attenuation << " of them attenuated\n"
              << "  " << totals.texture_names.size() << " distinct texture names; "
              << totals.geometry_blocks_with_texture << "/" << totals.geometry_blocks
              << " geometry blocks resolve a base texture\n";
    for (const auto& entry : totals.versions)
        std::cout << "  version " << version_text(entry.first) << " decodes in "
                  << entry.second << " file(s)\n";
    for (const auto& entry : totals.failed_versions)
        std::cout << "  version " << version_text(entry.first) << " FAILS in "
                  << entry.second << " file(s)\n";
    for (const auto& entry : totals.trailing)
        std::cout << "  " << entry.first << " trailing byte(s) in "
                  << entry.second << " file(s)\n";
    for (const auto& entry : totals.blockers)
        std::cout << "  blocked at " << entry.first << " in " << entry.second
                  << " file(s)\n";
    for (const auto& entry : totals.skipped_blocks) {
        const auto files = totals.skipped_files.find(entry.first);
        std::cout << "  SKIPPED (no decoder, header states its size) " << entry.first
                  << ": " << entry.second << " block(s) in "
                  << (files == totals.skipped_files.end() ? 0 : files->second)
                  << " file(s)\n";
    }
    std::cout << "  spans tile the body in " << totals.files_with_tiled_spans << "/"
              << decoded << " decoded file(s)";
    if (!totals.first_untiled_file.empty())
        std::cout << ", first break in " << totals.first_untiled_file;
    std::cout << "\n";
    print_animation_totals(totals.animation);
    print_skin_totals(totals.skin);
    KnC::census::print_effect_totals(totals.effect);
}

// prints 5 evenly spaced samples across the controller time window
void print_samples(const KnC::NifKeyGroup& group, float stop_time) {
    constexpr int kSampleCount = 5;
    std::cout << "        samples";
    for (int step = 0; step < kSampleCount; ++step) {
        const float time = stop_time * step / (kSampleCount - 1);
        KnC::NifKeySample sample;
        if (!KnC::sample_key_group(group, time, sample)) {
            std::cout << ": not sampled, " << interpolation_name(group.interpolation)
                      << " x" << group.components << "\n";
            return;
        }
        std::cout << "  t=" << time << " ->";
        for (uint32_t component = 0; component < sample.count; ++component)
            std::cout << " " << sample.values[component];
    }
    std::cout << "\n";
}

void print_key_group(const KnC::NifKeyGroup& group, const char* label) {
    if (group.key_count() == 0) return;
    std::cout << "      " << label << ": " << group.key_count() << " "
              << interpolation_name(group.interpolation) << " key(s), "
              << group.components << " component(s)\n";
    const size_t parameters_per_key =
        group.interpolation_parameters.size() / group.key_count();
    for (size_t key = 0; key < group.key_count(); ++key) {
        std::cout << "        t=" << group.times[key] << " ->";
        for (uint32_t part = 0; part < group.components; ++part)
            std::cout << " " << group.values[key * group.components + part];
        if (parameters_per_key != 0) std::cout << "  [";
        for (size_t part = 0; part < parameters_per_key; ++part)
            std::cout << (part == 0 ? "" : " ")
                      << group.interpolation_parameters[key * parameters_per_key + part];
        if (parameters_per_key != 0) std::cout << "]";
        std::cout << "\n";
    }
}

void print_key_data(const KnC::NifAnimation& animation, float stop_time) {
    print_key_group(animation.channel, "channel");
    print_key_group(animation.translations, "translations");
    print_key_group(animation.rotations, "rotations");
    print_key_group(animation.scales, "scales");
    for (const auto& axis : animation.rotation_axes)
        print_key_group(axis, "rotation axis");
    if (animation.channel.key_count() != 0) print_samples(animation.channel, stop_time);
}

// prints pose translation rotation and scale or a single value
void print_pose(const KnC::NifBlock& block) {
    const KnC::NifInterpolator& interpolator = block.animation->interpolator;
    if (block.type != "NiTransformInterpolator") {
        std::cout << " pose " << interpolator.pose_value;
        return;
    }
    std::cout << " pose translation (" << interpolator.pose_translation[0] << " "
              << interpolator.pose_translation[1] << " "
              << interpolator.pose_translation[2] << "), rotation wxyz ("
              << interpolator.pose_rotation[0] << " " << interpolator.pose_rotation[1]
              << " " << interpolator.pose_rotation[2] << " "
              << interpolator.pose_rotation[3] << "), scale " << interpolator.pose_scale;
}

void print_interpolator(const KnC::NifScene& scene, uint32_t link, float stop_time) {
    const KnC::NifAnimation* animation = KnC::find_animation(scene, link);
    if (animation == nullptr) return;
    std::cout << "    block " << link << " " << scene.blocks[link].type;
    print_pose(scene.blocks[link]);
    std::cout << ", data block " << animation->interpolator.data_link << "\n";
    const KnC::NifAnimation* keys =
        KnC::find_animation(scene, animation->interpolator.data_link);
    if (keys == nullptr) return;
    std::cout << "      block " << animation->interpolator.data_link << " "
              << scene.blocks[animation->interpolator.data_link].type << "\n";
    print_key_data(*keys, stop_time);
}

void print_controller(const KnC::NifScene& scene, uint32_t index) {
    const KnC::NifController& controller = scene.blocks[index].animation->controller;
    std::cout << "  block " << index << " " << scene.blocks[index].type
              << " drives block " << controller.target_link << ", flags 0x"
              << std::hex << controller.flags << std::dec << " ("
              << cycle_name(controller.cycle_type())
              << (controller.is_active() ? ", active" : ", inactive")
              << "), frequency " << controller.frequency << ", phase "
              << controller.phase << ", " << controller.start_time << " .. "
              << controller.stop_time << "\n";
    if (scene.blocks[index].type == "NiTextureTransformController") {
        const KnC::NifTextureTransform& transform =
            scene.blocks[index].animation->texture_transform;
        std::cout << "    texture map " << transform.map_index << ", "
                  << transform_name(transform.operation) << "\n";
    }
    print_interpolator(scene, controller.interpolator_link, controller.stop_time);
}

// prints per skinned block its bones vertices and weight sum errors
void print_file_skin(const KnC::NifScene& scene) {
    for (const auto& block : scene.blocks) {
        ResolvedSkin skin;
        if (!resolve_skin(scene, block, skin)) continue;
        std::cout << "  " << block.type << " '" << block.name << "': "
                  << skin.instance->bones.size() << " bone(s), " << skin.vertex_count
                  << " vertices";
        if (skin.data != nullptr)
            std::cout << ", NiSkinData worst |sum-1| " << worst_data_weight_sum(skin)
                      << ", " << count_data_range_errors(skin) << " index error(s)";
        size_t unbound = 0;
        if (skin.partition != nullptr)
            std::cout << ", " << skin.partition->partitions.size()
                      << " partition(s) worst |sum-1| "
                      << worst_partition_weight_sum(*skin.partition, unbound) << ", "
                      << count_partition_range_errors(skin) << " index error(s), "
                      << unbound << " unbound vertex/vertices";
        std::cout << "\n";
    }
}

// prints each sequence text keys the kf clip fires while playing
void print_file_text_keys(const KnC::NifScene& scene) {
    for (const auto& block : scene.blocks) {
        if (!block.animation || block.type != "NiControllerSequence") continue;
        const KnC::NifAnimation* text =
            KnC::find_animation(scene, block.animation->sequence.text_keys_link);
        if (text == nullptr || text->text_keys.empty()) continue;
        std::cout << "  sequence '" << block.name << "' "
                  << block.animation->sequence.start_time << ".."
                  << block.animation->sequence.stop_time << "s:";
        for (const KnC::NifTextKey& key : text->text_keys)
            std::cout << " " << key.text << "@" << key.time;
        std::cout << "\n";
    }
}

// prints controllers text keys skin and surface detail for one file
int print_file_animation(const std::string& path) {
    KnC::NifScene scene;
    std::string error;
    if (!KnC::read_nif_scene(path, scene, error)) {
        std::cerr << path << ": " << error << "\n";
        return 1;
    }
    std::cout << std::setprecision(9) << path << ": " << scene.blocks.size()
              << " blocks\n";
    for (uint32_t index = 0; index < scene.blocks.size(); ++index) {
        if (!scene.blocks[index].animation) continue;
        if (!is_controller(scene.blocks[index].type)) continue;
        print_controller(scene, index);
    }
    print_file_text_keys(scene);
    print_file_skin(scene);
    KnC::census::print_file_surfaces(scene);
    KnC::census::print_file_particles(scene);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: nif_smoke <directory|file> [extension]\n";
        return 2;
    }
    const std::string root = argv[1];
    const std::string extension = argc > 2 ? argv[2] : ".nif";
    if (!std::filesystem::is_directory(root)) return print_file_animation(root);

    SmokeTotals totals;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (!extension_matches(entry.path().extension().string(), extension)) continue;
        accumulate_file(entry.path().string(), totals);
    }
    print_totals(totals);
    return totals.blockers.empty() ? 0 : 1;
}
