#include "engine/render/nif_character_model.h"

#include "engine/formats/nif_scene_graph.h"
#include "engine/formats/nif_skeleton.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <map>
#include <tuple>
#include <utility>

namespace KnC::Render {

namespace {

constexpr uint16_t kHiddenObjectFlag = 0x0001u;

// A bone a vertex follows skeleton node and the bind transform into that node
struct Influence {
    int          node = -1;
    NifTransform bind;
};

// Vertex in geometry space with up to four influences weights sum one
struct WeightedVertex {
    SceneVertex surface;
    int   influence[kWeightsPerVertex] = {-1, -1, -1, -1};
    float weight[kWeightsPerVertex]    = {0.f, 0.f, 0.f, 0.f};
};

// One geometry ready to split influences vertices and corner indices
struct SkinnedGeometry {
    std::vector<Influence>      influences;
    std::vector<WeightedVertex> vertices;
    std::vector<uint32_t>       corners;
};

// One part is one texture and one device state like the prop builder
struct PartKey {
    std::string texture;
    // The sphere map over the geometry a mesh without one draws apart
    std::string environment;
    uint32_t    alpha_property    = kNoLink;
    uint32_t    depth_property    = kNoLink;
    uint32_t    material_property = kNoLink;
    uint32_t    texturing         = kNoLink;
    uint8_t     shading           = 0;
    bool        vertex_colours    = false;

    bool operator<(const PartKey& other) const {
        return std::tie(texture, environment, alpha_property, depth_property, material_property, texturing,
                        shading, vertex_colours) <
               std::tie(other.texture, other.environment, other.alpha_property, other.depth_property,
                        other.material_property, other.texturing, other.shading,
                        other.vertex_colours);
    }
};

// Part still open for a key with the slot each influence of the geometry landed in
struct OpenPart {
    std::size_t      part = 0;
    std::vector<int> slot_of_influence;
    std::vector<int> vertex_of;
};

// NiSkinInstance and its NiSkinData or null for a rigid piece
struct ResolvedSkin {
    const NifSkin* instance = nullptr;
    const NifSkin* data     = nullptr;
};

ResolvedSkin resolve_skin(const NifScene& scene, const NifBlock& shape) {
    ResolvedSkin skin;
    if (shape.skin_instance_link >= scene.blocks.size()) return skin;
    const NifSkin* instance = scene.blocks[shape.skin_instance_link].skin.get();
    if (instance == nullptr || instance->data_link >= scene.blocks.size()) return skin;
    const NifSkin* data = scene.blocks[instance->data_link].skin.get();
    if (data == nullptr || data->bone_binds.size() != instance->bones.size()) return skin;
    skin.instance = instance;
    skin.data     = data;
    return skin;
}

bool carries_vertex_colours(const NifBlock& mesh) {
    return mesh.vertex_colours.size() * 3 == mesh.vertices.size() * 4;
}

bool triangle_in_range(const uint16_t* corners, std::size_t vertex_count) {
    return corners[0] < vertex_count && corners[1] < vertex_count && corners[2] < vertex_count;
}

// The four heaviest influences kept and renormalised to sum one
void keep_heaviest(std::vector<std::pair<int, float>>& claims, WeightedVertex& out) {
    std::sort(claims.begin(), claims.end(),
              [](const std::pair<int, float>& left, const std::pair<int, float>& right) {
                  return left.second > right.second;
              });
    if (claims.size() > kWeightsPerVertex) claims.resize(kWeightsPerVertex);
    float total = 0.f;
    for (const auto& claim : claims) total += claim.second;
    if (total <= 0.f) return;
    for (std::size_t slot = 0; slot < claims.size(); ++slot) {
        out.influence[slot] = claims[slot].first;
        out.weight[slot]    = claims[slot].second / total;
    }
}

// Surface vertex left in geometry space the palette moves it
SceneVertex surface_vertex(const NifBlock& mesh, std::size_t vertex) {
    SceneVertex out;
    out.x = mesh.vertices[vertex * 3];
    out.y = mesh.vertices[vertex * 3 + 1];
    out.z = mesh.vertices[vertex * 3 + 2];
    if (mesh.normals.size() == mesh.vertices.size()) {
        out.normal_x = mesh.normals[vertex * 3];
        out.normal_y = mesh.normals[vertex * 3 + 1];
        out.normal_z = mesh.normals[vertex * 3 + 2];
    }
    if (carries_vertex_colours(mesh)) out.abgr = nif_colour_abgr(&mesh.vertex_colours[vertex * 4]);
    if (mesh.uvs.size() * 3 != mesh.vertices.size() * 2) return out;
    out.u = mesh.uvs[vertex * 2];
    out.v = mesh.uvs[vertex * 2 + 1];
    return out;
}

// Walks the stream geometry by geometry and fills parts palettes and the rig
class CharacterBuilder {
public:
    CharacterBuilder(const NifScene& scene, const CharacterModelRequest& request,
                     CharacterModel& out)
        : scene_(scene), request_(request), out_(out), environments_(scene) {}

    void build_geometry();
    bool bind_clips(std::string& error);

private:
    void gather(uint32_t block, SkinnedGeometry& geometry) const;
    void gather_skinned(uint32_t block, const NifBlock& mesh, const ResolvedSkin& skin,
                        SkinnedGeometry& geometry) const;
    void gather_rigid(uint32_t block, const NifBlock& mesh, SkinnedGeometry& geometry) const;
    int  rigid_influence(uint32_t block, SkinnedGeometry& geometry) const;
    void emit(uint32_t block, const SkinnedGeometry& geometry);
    OpenPart& open_part(const PartKey& key, const NifSurfaceState& surface, bool coloured,
                        const EnvironmentMap& environment);
    OpenPart& fresh_part(const PartKey& key, const NifSurfaceState& surface, bool coloured,
                         const EnvironmentMap& environment);
    void copy_vertex(const SkinnedGeometry& geometry, uint32_t vertex, OpenPart& open);
    void drop_empty_parts();
    void measure_bounds();

    const NifScene&              scene_;
    const CharacterModelRequest& request_;
    CharacterModel&              out_;
    std::vector<NifSurfaceState> surfaces_;
    std::map<PartKey, OpenPart>  open_;
    EnvironmentMaps              environments_;
};

void CharacterBuilder::build_geometry() {
    out_.rig.skeleton = build_nif_skeleton(scene_);
    surfaces_ = resolve_surface_states(scene_);
    for (uint32_t block = 0; block < scene_.blocks.size(); ++block) {
        const NifBlock& shape = scene_.blocks[block];
        // Only a block the skeleton reached is drawn like the client tree walk
        if (out_.rig.skeleton.node_of(block) < 0 || shape.data_link >= scene_.blocks.size())
            continue;
        // flags bit 0 is app culled the red helper box of a part is one
        if ((shape.object_flags & kHiddenObjectFlag) != 0) continue;
        const bool textured = !find_base_texture_file_name(scene_, shape).empty();
        if (!textured && !request_.draw_untextured) continue;
        const NifBlock& mesh = scene_.blocks[shape.data_link];
        const bool has_uvs = mesh.uvs.size() * 3 == mesh.vertices.size() * 2;
        if (mesh.vertices.empty() || (textured && !has_uvs)) continue;
        SkinnedGeometry geometry;
        gather(block, geometry);
        emit(block, geometry);
    }
    drop_empty_parts();
    measure_bounds();
}

void CharacterBuilder::gather(uint32_t block, SkinnedGeometry& geometry) const {
    const NifBlock& shape = scene_.blocks[block];
    const NifBlock& mesh  = scene_.blocks[shape.data_link];
    const std::size_t count = mesh.vertices.size() / 3;
    geometry.vertices.resize(count);
    for (std::size_t vertex = 0; vertex < count; ++vertex)
        geometry.vertices[vertex].surface = surface_vertex(mesh, vertex);
    const ResolvedSkin skin = resolve_skin(scene_, shape);
    if (skin.instance != nullptr) gather_skinned(block, mesh, skin, geometry);
    else gather_rigid(block, mesh, geometry);
    geometry.corners.reserve(mesh.triangles.size());
    for (std::size_t corner = 0; corner + 2 < mesh.triangles.size(); corner += 3) {
        if (!triangle_in_range(&mesh.triangles[corner], count)) continue;
        for (int step = 0; step < 3; ++step) geometry.corners.push_back(mesh.triangles[corner + step]);
    }
}

// The geometry own node moves a vertex no bone claims and every rigid piece
int CharacterBuilder::rigid_influence(uint32_t block, SkinnedGeometry& geometry) const {
    Influence own;
    own.node = out_.rig.skeleton.node_of(block);
    geometry.influences.push_back(own);
    return static_cast<int>(geometry.influences.size() - 1);
}

// NiSkinData claims per bone turned into per vertex weights the bind is skin to bone
void CharacterBuilder::gather_skinned(uint32_t block, const NifBlock& mesh,
                                      const ResolvedSkin& skin, SkinnedGeometry& geometry) const {
    const std::size_t count = mesh.vertices.size() / 3;
    for (std::size_t bone = 0; bone < skin.instance->bones.size(); ++bone) {
        Influence influence;
        influence.node = out_.rig.skeleton.node_of(skin.instance->bones[bone]);
        influence.bind = skin.data->bone_binds[bone].bind;
        geometry.influences.push_back(influence);
    }
    std::vector<std::vector<std::pair<int, float>>> claims(count);
    for (std::size_t bone = 0; bone < skin.data->bone_binds.size(); ++bone) {
        const NifSkinBone& bind = skin.data->bone_binds[bone];
        for (std::size_t entry = 0; entry < bind.vertex_indices.size(); ++entry) {
            const std::size_t vertex = bind.vertex_indices[entry];
            if (vertex >= count || entry >= bind.weights.size() || bind.weights[entry] <= 0.f)
                continue;
            claims[vertex].emplace_back(static_cast<int>(bone), bind.weights[entry]);
        }
    }
    int unclaimed = -1;
    for (std::size_t vertex = 0; vertex < count; ++vertex) {
        if (claims[vertex].empty()) {
            if (unclaimed < 0) unclaimed = rigid_influence(block, geometry);
            geometry.vertices[vertex].influence[0] = unclaimed;
            geometry.vertices[vertex].weight[0]    = 1.f;
            continue;
        }
        keep_heaviest(claims[vertex], geometry.vertices[vertex]);
    }
}

void CharacterBuilder::gather_rigid(uint32_t block, const NifBlock& mesh,
                                    SkinnedGeometry& geometry) const {
    const int own = rigid_influence(block, geometry);
    for (std::size_t vertex = 0; vertex * 3 < mesh.vertices.size(); ++vertex) {
        geometry.vertices[vertex].influence[0] = own;
        geometry.vertices[vertex].weight[0]    = 1.f;
    }
}

OpenPart& CharacterBuilder::fresh_part(const PartKey& key, const NifSurfaceState& surface,
                                       bool coloured, const EnvironmentMap& environment) {
    SkinnedPart part;
    if (!key.texture.empty())
        part.texture_path = (std::filesystem::path(request_.texture_dir) / key.texture).string();
    if (!environment.texture.empty()) {
        part.environment = environment;
        part.environment.texture = (std::filesystem::path(request_.texture_dir) / environment.texture).string();
    }
    part.surface = surface;
    part.has_vertex_colours = coloured;
    out_.parts.push_back(std::move(part));
    out_.rig.palettes.emplace_back();
    OpenPart& open = open_[key];
    open.part = out_.parts.size() - 1;
    open.slot_of_influence.clear();
    open.vertex_of.clear();
    return open;
}

OpenPart& CharacterBuilder::open_part(const PartKey& key, const NifSurfaceState& surface,
                                      bool coloured, const EnvironmentMap& environment) {
    const auto known = open_.find(key);
    if (known != open_.end()) return known->second;
    return fresh_part(key, surface, coloured, environment);
}

void CharacterBuilder::copy_vertex(const SkinnedGeometry& geometry, uint32_t vertex,
                                   OpenPart& open) {
    if (open.vertex_of[vertex] >= 0) {
        out_.parts[open.part].indices.push_back(static_cast<uint32_t>(open.vertex_of[vertex]));
        return;
    }
    const WeightedVertex& source = geometry.vertices[vertex];
    SkinnedVertex placed;
    placed.surface = source.surface;
    for (int slot = 0; slot < kWeightsPerVertex; ++slot) {
        placed.bone_weight[slot] = source.weight[slot];
        placed.bone_slot[slot] = source.influence[slot] < 0
            ? 0
            : static_cast<uint8_t>(open.slot_of_influence[static_cast<std::size_t>(source.influence[slot])]);
    }
    SkinnedPart& part = out_.parts[open.part];
    open.vertex_of[vertex] = static_cast<int>(part.vertices.size());
    part.indices.push_back(static_cast<uint32_t>(part.vertices.size()));
    part.vertices.push_back(placed);
}

// Triangles fill the open part a palette that would pass kBonePaletteSize opens a new one
void CharacterBuilder::emit(uint32_t block, const SkinnedGeometry& geometry) {
    const NifSurfaceState& surface = surfaces_[block];
    const bool coloured = carries_vertex_colours(scene_.blocks[scene_.blocks[block].data_link]);
    const EnvironmentMap& environment = environments_.of(block);
    const std::string texture = find_base_texture_file_name(scene_, scene_.blocks[block]);
    const PartKey key{texture, environment.texture,
                      surface.alpha_link, surface.depth_link, surface.material_link,
                      surface.texturing_link,
                      static_cast<uint8_t>(surface_shading(surface, !texture.empty())), coloured};
    OpenPart* open = &open_part(key, surface, coloured, environment);
    open->slot_of_influence.assign(geometry.influences.size(), -1);
    open->vertex_of.assign(geometry.vertices.size(), -1);
    std::vector<int> needed;
    for (std::size_t corner = 0; corner + 2 < geometry.corners.size(); corner += 3) {
        needed.clear();
        for (int step = 0; step < 3; ++step) {
            const WeightedVertex& vertex = geometry.vertices[geometry.corners[corner + step]];
            for (int slot = 0; slot < kWeightsPerVertex; ++slot) {
                const int influence = vertex.influence[slot];
                if (influence < 0 || vertex.weight[slot] <= 0.f) continue;
                if (open->slot_of_influence[static_cast<std::size_t>(influence)] >= 0) continue;
                if (std::find(needed.begin(), needed.end(), influence) == needed.end())
                    needed.push_back(influence);
            }
        }
        BonePalette* palette = &out_.rig.palettes[open->part];
        if (palette->nodes.size() + needed.size() > static_cast<std::size_t>(kBonePaletteSize)) {
            open = &fresh_part(key, surface, coloured, environment);
            open->slot_of_influence.assign(geometry.influences.size(), -1);
            open->vertex_of.assign(geometry.vertices.size(), -1);
            palette = &out_.rig.palettes[open->part];
        }
        for (const int influence : needed) {
            open->slot_of_influence[static_cast<std::size_t>(influence)] =
                static_cast<int>(palette->nodes.size());
            palette->nodes.push_back(geometry.influences[static_cast<std::size_t>(influence)].node);
            palette->binds.push_back(geometry.influences[static_cast<std::size_t>(influence)].bind);
        }
        for (int step = 0; step < 3; ++step) copy_vertex(geometry, geometry.corners[corner + step], *open);
    }
}

// Parts and palettes stay parallel a part without a triangle cannot be uploaded
void CharacterBuilder::drop_empty_parts() {
    std::vector<SkinnedPart> parts;
    std::vector<BonePalette> palettes;
    for (std::size_t index = 0; index < out_.parts.size(); ++index) {
        if (out_.parts[index].indices.empty()) continue;
        parts.push_back(std::move(out_.parts[index]));
        palettes.push_back(std::move(out_.rig.palettes[index]));
    }
    out_.parts = std::move(parts);
    out_.rig.palettes = std::move(palettes);
}

// Rest pose box every vertex moved by its bones the renderer fits a sphere on it
void CharacterBuilder::measure_bounds() {
    std::vector<NifPlacement> rest;
    rest_nif_skeleton(out_.rig.skeleton, rest);
    for (int axis = 0; axis < 3; ++axis) {
        out_.bounds_min[axis] = std::numeric_limits<float>::max();
        out_.bounds_max[axis] = std::numeric_limits<float>::lowest();
    }
    bool any = false;
    for (std::size_t index = 0; index < out_.parts.size(); ++index) {
        const BonePalette& palette = out_.rig.palettes[index];
        for (const SkinnedVertex& vertex : out_.parts[index].vertices) {
            const float local[3] = {vertex.surface.x, vertex.surface.y, vertex.surface.z};
            float moved[3] = {0.f, 0.f, 0.f};
            for (int slot = 0; slot < kWeightsPerVertex; ++slot) {
                const std::size_t entry = vertex.bone_slot[slot];
                if (vertex.bone_weight[slot] <= 0.f || entry >= palette.nodes.size()) continue;
                const int node = palette.nodes[entry];
                const NifPlacement placed = compose_placement(
                    node < 0 ? NifPlacement() : rest[static_cast<std::size_t>(node)],
                    palette.binds[entry]);
                float placed_point[3];
                place_point(placed, local, placed_point);
                for (int axis = 0; axis < 3; ++axis)
                    moved[axis] += vertex.bone_weight[slot] * placed_point[axis];
            }
            for (int axis = 0; axis < 3; ++axis) {
                out_.bounds_min[axis] = std::min(out_.bounds_min[axis], moved[axis]);
                out_.bounds_max[axis] = std::max(out_.bounds_max[axis], moved[axis]);
            }
            any = true;
        }
    }
    if (any) return;
    for (int axis = 0; axis < 3; ++axis) {
        out_.bounds_min[axis] = 0.f;
        out_.bounds_max[axis] = 0.f;
    }
}

// Every sequence of every KF bound by node name a KF without one is an error
bool CharacterBuilder::bind_clips(std::string& error) {
    for (const CharacterClipRequest& wanted : request_.clips) {
        NifScene stream;
        if (!read_nif_scene(wanted.kf_path, stream, error)) return false;
        const std::vector<uint32_t> sequences = find_nif_sequences(stream);
        if (sequences.empty()) {
            error = wanted.kf_path + " holds no NiControllerSequence";
            return false;
        }
        for (std::size_t position = 0; position < sequences.size(); ++position) {
            if (wanted.sequence_index >= 0 &&
                static_cast<std::size_t>(wanted.sequence_index) != position) continue;
            const uint32_t block = sequences[position];
            CharacterClip clip;
            std::size_t unbound = 0;
            if (!bind_nif_sequence(stream, block, out_.rig.skeleton, clip.motion, unbound)) continue;
            // The name given replaces the stream one when the request points at one sequence
            if (!wanted.name.empty() && (sequences.size() == 1 || wanted.sequence_index >= 0))
                clip.motion.name = wanted.name;
            clip.sequence_id = wanted.sequence_id >= 0 ? wanted.sequence_id
                                                       : static_cast<int32_t>(out_.rig.clips.size());
            out_.rig.clips.push_back(std::move(clip));
        }
    }
    return true;
}

} // namespace

bool nif_has_skin(const NifScene& scene) {
    for (const NifBlock& block : scene.blocks)
        if (block.skin_instance_link < scene.blocks.size()) return true;
    return false;
}

bool build_character_model(const NifScene& scene, const CharacterModelRequest& request,
                           CharacterModel& out, std::string& error) {
    out = CharacterModel();
    out.name = std::filesystem::path(request.nif_path).stem().string();
    CharacterBuilder builder(scene, request, out);
    builder.build_geometry();
    out.lights = collect_model_lights(scene);
    if (out.parts.empty() && !request.allow_no_geometry) {
        error = request.nif_path + " has no textured geometry to skin";
        return false;
    }
    return builder.bind_clips(error);
}

bool load_character_model(const CharacterModelRequest& request, CharacterModel& out,
                          std::string& error) {
    NifScene scene;
    if (!read_nif_scene(request.nif_path, scene, error)) return false;
    return build_character_model(scene, request, out, error);
}

}
