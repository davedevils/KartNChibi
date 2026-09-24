// bake skinned animation to per frame vertex positions JSON stdout
#include "engine/formats/nif_reader.h"
#include "engine/formats/nif_scene_graph.h"
#include "engine/formats/nif_skeleton.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

bool is_geometry(const std::string& type) {
    return type == "NiTriShape" || type == "NiTriStrips";
}

// shape block index paired with its data block index
struct Geometry {
    uint32_t shape = 0;
    uint32_t data  = 0;
};

std::vector<Geometry> collect_geometry(const KnC::NifScene& model) {
    std::vector<Geometry> found;
    for (uint32_t index = 0; index < model.blocks.size(); ++index) {
        const KnC::NifBlock& shape = model.blocks[index];
        if (!is_geometry(shape.type) || shape.data_link >= model.blocks.size()) continue;
        const KnC::NifBlock& data = model.blocks[shape.data_link];
        if (data.vertices.empty() || data.uvs.empty()) continue;
        if (KnC::find_base_texture_file_name(model, shape).empty()) continue;
        found.push_back({index, shape.data_link});
    }
    return found;
}

// resolved skin instance and data or null when the shape is rigid
struct ResolvedSkin {
    const KnC::NifSkin* instance = nullptr;
    const KnC::NifSkin* data     = nullptr;
};

ResolvedSkin resolve_skin(const KnC::NifScene& model, const KnC::NifBlock& shape) {
    ResolvedSkin skin;
    if (shape.skin_instance_link >= model.blocks.size()) return skin;
    const KnC::NifSkin* instance = model.blocks[shape.skin_instance_link].skin.get();
    if (instance == nullptr || instance->data_link >= model.blocks.size()) return skin;
    const KnC::NifSkin* data = model.blocks[instance->data_link].skin.get();
    if (data == nullptr || data->bone_binds.size() != instance->bones.size()) return skin;
    skin.instance = instance;
    skin.data     = data;
    return skin;
}

// vertex position is bone weighted sum of posed placement times bind pose
void skin_positions(const KnC::NifBlock& mesh, const ResolvedSkin& skin,
                    const KnC::NifSkeleton& skeleton,
                    const std::vector<KnC::NifPlacement>& posed, std::vector<float>& out) {
    const std::size_t count = mesh.vertices.size() / 3;
    out.assign(mesh.vertices.size(), 0.f);
    std::vector<float> reached(count, 0.f);
    for (std::size_t bone = 0; bone < skin.instance->bones.size(); ++bone) {
        const int node = skeleton.node_of(skin.instance->bones[bone]);
        const KnC::NifPlacement place = KnC::compose_placement(
            node < 0 ? KnC::NifPlacement() : posed[static_cast<std::size_t>(node)],
            skin.data->bone_binds[bone].bind);
        const KnC::NifSkinBone& bind = skin.data->bone_binds[bone];
        for (std::size_t entry = 0; entry < bind.vertex_indices.size(); ++entry) {
            const std::size_t vertex = bind.vertex_indices[entry];
            if (vertex >= count) continue;
            float moved[3];
            KnC::place_point(place, &mesh.vertices[vertex * 3], moved);
            for (int axis = 0; axis < 3; ++axis)
                out[vertex * 3 + axis] += bind.weights[entry] * moved[axis];
            reached[vertex] += bind.weights[entry];
        }
    }
    for (std::size_t vertex = 0; vertex < count; ++vertex) {
        if (reached[vertex] > 1e-4f) {
            for (int axis = 0; axis < 3; ++axis) out[vertex * 3 + axis] /= reached[vertex];
            continue;
        }
        // vertex with no bone weight keeps its original mesh position
        for (int axis = 0; axis < 3; ++axis)
            out[vertex * 3 + axis] = mesh.vertices[vertex * 3 + axis];
    }
}

void rigid_positions(const KnC::NifBlock& mesh, const KnC::NifPlacement& place,
                     std::vector<float>& out) {
    out.assign(mesh.vertices.size(), 0.f);
    for (std::size_t vertex = 0; vertex * 3 < mesh.vertices.size(); ++vertex)
        KnC::place_point(place, &mesh.vertices[vertex * 3], &out[vertex * 3]);
}

void print_topology(const KnC::NifScene& model, const std::vector<Geometry>& geometry) {
    std::printf("{\"meshes\":[");
    for (std::size_t index = 0; index < geometry.size(); ++index) {
        const KnC::NifBlock& shape = model.blocks[geometry[index].shape];
        const KnC::NifBlock& data  = model.blocks[geometry[index].data];
        const std::string texture = KnC::find_base_texture_file_name(model, shape);
        std::printf("%s{\"tex\":\"%s\",\"uvs\":[", index ? "," : "", texture.c_str());
        for (std::size_t uv = 0; uv < data.uvs.size(); ++uv)
            std::printf("%s%.5f", uv ? "," : "", data.uvs[uv]);
        std::printf("],\"indices\":[");
        for (std::size_t corner = 0; corner < data.triangles.size(); ++corner)
            std::printf("%s%u", corner ? "," : "",
                        static_cast<unsigned>(data.triangles[corner]));
        std::printf("]}");
    }
    std::printf("],\"frames\":[");
}

// first sequence in a kf stream is always idle for every character
bool find_first_sequence(const KnC::NifScene& clip_stream, uint32_t& out) {
    for (uint32_t block : KnC::find_nif_sequences(clip_stream))
        if (clip_stream.blocks[block].animation != nullptr &&
            !clip_stream.blocks[block].animation->sequence.entries.empty()) {
            out = block;
            return true;
        }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: nif_animate <model.nif> <anim.kf> [frames]\n");
        return 2;
    }
    const int frames = argc > 3 ? std::atoi(argv[3]) : 24;
    KnC::NifScene model, clip_stream;
    std::string error;
    if (!KnC::read_nif_scene(argv[1], model, error)) {
        std::fprintf(stderr, "model: %s\n", error.c_str());
        return 1;
    }
    if (!KnC::read_nif_scene(argv[2], clip_stream, error)) {
        std::fprintf(stderr, "kf: %s\n", error.c_str());
        return 1;
    }
    uint32_t sequence_block = 0;
    if (!find_first_sequence(clip_stream, sequence_block)) {
        std::fprintf(stderr, "no sequence in kf\n");
        return 1;
    }

    const KnC::NifSkeleton skeleton = KnC::build_nif_skeleton(model);
    KnC::NifSkeletonClip clip;
    std::size_t unbound = 0;
    KnC::bind_nif_sequence(clip_stream, sequence_block, skeleton, clip, unbound);
    std::fprintf(stderr, "clip '%s' [%.3f..%.3f] binds %zu channel(s), %zu unbound\n",
                 clip.name.c_str(), clip.start_time, clip.stop_time, clip.channels.size(),
                 unbound);

    const std::vector<Geometry> geometry = collect_geometry(model);
    print_topology(model, geometry);
    std::vector<KnC::NifPlacement> posed;
    std::vector<float> positions;
    for (int frame = 0; frame < frames; ++frame) {
        const float seconds = clip.start_time + clip.duration() * frame / frames;
        KnC::pose_nif_skeleton(skeleton, clip, seconds, posed);
        std::printf("%s[", frame ? "," : "");
        for (std::size_t index = 0; index < geometry.size(); ++index) {
            const KnC::NifBlock& shape = model.blocks[geometry[index].shape];
            const KnC::NifBlock& mesh  = model.blocks[geometry[index].data];
            const ResolvedSkin skin = resolve_skin(model, shape);
            if (skin.instance != nullptr) {
                skin_positions(mesh, skin, skeleton, posed, positions);
            } else {
                const int node = skeleton.node_of(geometry[index].shape);
                rigid_positions(mesh,
                                node < 0 ? KnC::NifPlacement()
                                         : posed[static_cast<std::size_t>(node)],
                                positions);
            }
            std::printf("%s[", index ? "," : "");
            for (std::size_t value = 0; value < positions.size(); ++value)
                std::printf("%s%.4f", value ? "," : "", positions[value]);
            std::printf("]");
        }
        std::printf("]");
    }
    std::printf("]}\n");
    return 0;
}
