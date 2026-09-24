// dumps NIF geometry to JSON positions normals uvs indices and texture for a browser
#include "engine/formats/nif_reader.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

// affine transform translation rotation scale
struct Xf {
    float t[3] = {0, 0, 0};
    float r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float s = 1.0f;
};

Xf from_nif(const KnC::NifTransform& n) {
    Xf x;
    for (int i = 0; i < 3; ++i) x.t[i] = n.translation[i];
    for (int i = 0; i < 9; ++i) x.r[i] = n.rotation[i];
    x.s = n.scale;
    return x;
}

// composes parent transform a with child transform b
Xf compose(const Xf& a, const Xf& b) {
    Xf o;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float sum = 0;
            for (int k = 0; k < 3; ++k) sum += a.r[i * 3 + k] * b.r[k * 3 + j];
            o.r[i * 3 + j] = sum;
        }
    for (int i = 0; i < 3; ++i) {
        const float bt = a.r[i * 3] * b.t[0] + a.r[i * 3 + 1] * b.t[1] + a.r[i * 3 + 2] * b.t[2];
        o.t[i] = a.t[i] + a.s * bt;
    }
    o.s = a.s * b.s;
    return o;
}

void apply_pos(const Xf& x, const float* in, float* out) {
    for (int i = 0; i < 3; ++i) {
        const float rv = x.r[i * 3] * in[0] + x.r[i * 3 + 1] * in[1] + x.r[i * 3 + 2] * in[2];
        out[i] = x.t[i] + x.s * rv;
    }
}

void apply_dir(const Xf& x, const float* in, float* out) {
    for (int i = 0; i < 3; ++i)
        out[i] = x.r[i * 3] * in[0] + x.r[i * 3 + 1] * in[1] + x.r[i * 3 + 2] * in[2];
}

// walks NiNode tree accumulating each block world transform from its parent
void walk(const KnC::NifScene& scene, uint32_t idx, const Xf& parent,
          std::vector<Xf>& world, std::vector<char>& seen) {
    if (idx >= scene.blocks.size() || seen[idx]) return;
    seen[idx] = 1;
    const Xf local = scene.blocks[idx].has_transform ? from_nif(scene.blocks[idx].transform) : Xf{};
    world[idx] = compose(parent, local);
    for (uint32_t child : scene.blocks[idx].children) walk(scene, child, world[idx], world, seen);
}

bool is_geometry(const std::string& type) { return type == "NiTriShape" || type == "NiTriStrips"; }

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: nif_export <file.nif>\n");
        return 2;
    }
    KnC::NifScene scene;
    std::string error;
    if (!KnC::read_nif_scene(argv[1], scene, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    std::vector<Xf> world(scene.blocks.size());
    std::vector<char> seen(scene.blocks.size(), 0);
    for (uint32_t root : scene.roots) walk(scene, root, Xf{}, world, seen);
    for (size_t i = 0; i < scene.blocks.size(); ++i)
        if (!seen[i] && scene.blocks[i].has_transform) world[i] = from_nif(scene.blocks[i].transform);

    std::printf("{\"meshes\":[");
    bool first_mesh = true;
    for (uint32_t si = 0; si < scene.blocks.size(); ++si) {
        const KnC::NifBlock& shape = scene.blocks[si];
        if (!is_geometry(shape.type) || shape.data_link == KnC::kNoLink) continue;
        if (shape.data_link >= scene.blocks.size()) continue;
        const KnC::NifBlock& data = scene.blocks[shape.data_link];
        if (data.vertices.empty() || data.triangles.empty()) continue;
        const size_t count = data.vertices.size() / 3;

        // vertex positions from skin weights when skinned else from the rigid node transform
        std::vector<float> pos(data.vertices.size(), 0.f);
        std::vector<float> wsum(count, 0.f);
        bool skinned = false;
        if (shape.skin_instance_link != KnC::kNoLink && shape.skin_instance_link < scene.blocks.size()) {
            const KnC::NifBlock& inst = scene.blocks[shape.skin_instance_link];
            if (inst.skin && inst.skin->data_link < scene.blocks.size()) {
                const KnC::NifBlock& sdata = scene.blocks[inst.skin->data_link];
                if (sdata.skin && sdata.skin->bone_binds.size() == inst.skin->bones.size()) {
                    skinned = true;
                    for (size_t b = 0; b < inst.skin->bones.size(); ++b) {
                        const uint32_t bone_link = inst.skin->bones[b];
                        const Xf bone_world = bone_link < world.size() ? world[bone_link] : Xf{};
                        const Xf skin_to_world = compose(bone_world, from_nif(sdata.skin->bone_binds[b].bind));
                        const KnC::NifSkinBone& bind = sdata.skin->bone_binds[b];
                        for (size_t w = 0; w < bind.vertex_indices.size(); ++w) {
                            const uint16_t vi = bind.vertex_indices[w];
                            if (vi >= count) continue;
                            const float weight = bind.weights[w];
                            float out[3];
                            apply_pos(skin_to_world, &data.vertices[vi * 3], out);
                            for (int k = 0; k < 3; ++k) pos[vi * 3 + k] += weight * out[k];
                            wsum[vi] += weight;
                        }
                    }
                }
            }
        }
        if (skinned) {
            for (size_t v = 0; v < count; ++v)
                if (wsum[v] > 1e-4f)
                    for (int k = 0; k < 3; ++k) pos[v * 3 + k] /= wsum[v];
                else
                    for (int k = 0; k < 3; ++k) pos[v * 3 + k] = data.vertices[v * 3 + k];
        } else {
            for (size_t v = 0; v < count; ++v) apply_pos(world[si], &data.vertices[v * 3], &pos[v * 3]);
        }

        const std::string texture = KnC::find_base_texture_file_name(scene, shape);
        std::printf("%s{\"tex\":\"%s\",\"positions\":[", first_mesh ? "" : ",", texture.c_str());
        first_mesh = false;
        for (size_t i = 0; i < pos.size(); i += 3)
            std::printf("%s%.5f,%.5f,%.5f", i ? "," : "", pos[i], pos[i + 1], pos[i + 2]);
        std::printf("],\"normals\":[");
        for (size_t i = 0; i < data.normals.size(); i += 3) {
            float out[3];
            apply_dir(world[si], &data.normals[i], out);
            std::printf("%s%.4f,%.4f,%.4f", i ? "," : "", out[0], out[1], out[2]);
        }
        std::printf("],\"uvs\":[");
        for (size_t i = 0; i < data.uvs.size(); ++i) std::printf("%s%.5f", i ? "," : "", data.uvs[i]);
        std::printf("],\"indices\":[");
        for (size_t i = 0; i < data.triangles.size(); ++i)
            std::printf("%s%u", i ? "," : "", static_cast<unsigned>(data.triangles[i]));
        std::printf("]}");
    }
    std::printf("]}\n");
    return 0;
}
