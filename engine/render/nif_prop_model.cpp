#include "engine/render/nif_prop_model.h"

#include "engine/formats/nif_reader.h"
#include "engine/formats/nif_scene_graph.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace KnC::Render {

namespace {

void set_normal(const NifBlock& mesh, const NifPlacement& placement, size_t vertex,
                SceneVertex& out) {
    if (mesh.normals.size() != mesh.vertices.size()) return;
    float placed[3];
    place_direction(placement, &mesh.normals[vertex * 3], placed);
    out.normal_x = placed[0];
    out.normal_y = placed[1];
    out.normal_z = placed[2];
}

bool carries_vertex_colours(const NifBlock& mesh) {
    return mesh.vertex_colours.size() * 3 == mesh.vertices.size() * 4;
}

// The authored vertex colour is the vertex diffuse a mesh without one stays white
void set_vertex_colour(const NifBlock& mesh, size_t vertex, SceneVertex& out) {
    if (!carries_vertex_colours(mesh)) return;
    out.abgr = nif_colour_abgr(&mesh.vertex_colours[vertex * 4]);
}

uint32_t append_vertices(const NifBlock& mesh, const NifPlacement& placement, PropPart& out) {
    const uint32_t base = static_cast<uint32_t>(out.vertices.size());
    const size_t count = mesh.vertices.size() / 3;
    out.vertices.reserve(out.vertices.size() + count);
    for (size_t vertex = 0; vertex < count; ++vertex) {
        float world[3];
        place_point(placement, &mesh.vertices[vertex * 3], world);
        SceneVertex placed;
        placed.x = world[0];
        placed.y = world[1];
        placed.z = world[2];
        set_normal(mesh, placement, vertex, placed);
        set_vertex_colour(mesh, vertex, placed);
        placed.u = mesh.uvs[vertex * 2];
        placed.v = mesh.uvs[vertex * 2 + 1];
        out.vertices.push_back(placed);
    }
    return base;
}

bool triangle_in_range(const uint16_t* corners, size_t vertex_count) {
    return corners[0] < vertex_count && corners[1] < vertex_count && corners[2] < vertex_count;
}

void append_indices(const NifBlock& mesh, uint32_t base, PropPart& out) {
    const size_t vertex_count = mesh.vertices.size() / 3;
    out.indices.reserve(out.indices.size() + mesh.triangles.size());
    for (size_t corner = 0; corner + 2 < mesh.triangles.size(); corner += 3) {
        if (!triangle_in_range(&mesh.triangles[corner], vertex_count)) continue;
        for (int step = 0; step < 3; ++step)
            out.indices.push_back(base + mesh.triangles[corner + step]);
    }
}

// The authored sphere carried into the placed space
ModelBound placed_bound(const NifBlock& mesh, const NifPlacement& placement) {
    ModelBound bound;
    if (mesh.bound_radius <= 0.f) return bound;
    place_point(placement, mesh.bound_center, bound.center);
    bound.radius = mesh.bound_radius * placement.scale;
    return bound;
}

// NiBound Update keeps the sphere that holds the other or grows to hold both
void merge_bound(const ModelBound& child, ModelBound& out) {
    if (child.radius < 0.f) return;
    if (out.radius < 0.f) {
        out = child;
        return;
    }
    float offset[3];
    for (int axis = 0; axis < 3; ++axis) offset[axis] = child.center[axis] - out.center[axis];
    const float distance =
        std::sqrt(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);
    if (out.radius >= distance + child.radius) return;
    if (child.radius >= distance + out.radius) {
        out = child;
        return;
    }
    const float grown = (out.radius + child.radius + distance) * 0.5f;
    const float step = (grown - out.radius) / distance;
    for (int axis = 0; axis < 3; ++axis) out.center[axis] += offset[axis] * step;
    out.radius = grown;
}

// A part with no triangle cannot be uploaded
void drop_empty_parts(PropModel& model) {
    const auto has_no_triangle = [](const PropPart& part) { return part.indices.empty(); };
    model.parts.erase(
        std::remove_if(model.parts.begin(), model.parts.end(), has_no_triangle),
        model.parts.end());
}

NifTransform as_transform(const NifPlacement& placement) {
    NifTransform transform;
    for (int cell = 0; cell < 9; ++cell) transform.rotation[cell] = placement.rotation[cell];
    for (int axis = 0; axis < 3; ++axis) transform.translation[axis] = placement.translation[axis];
    transform.scale = placement.scale;
    return transform;
}

// A controller animates when active and its interpolator exists keys may be absent
bool controller_resolves(const NifScene& scene, const NifBlock& block, bool play_stopped) {
    const NifController& controller = block.animation->controller;
    if (!play_stopped && !controller.is_active()) return false;
    if (controller.target_link >= scene.blocks.size()) return false;
    return find_animation(scene, controller.interpolator_link) != nullptr;
}

// A morpher animates when active and its NiMorphData holds frames it carries no interpolator link
bool morpher_resolves(const NifScene& scene, const NifBlock& block, bool play_stopped) {
    const NifController& controller = block.animation->controller;
    if (!play_stopped && !controller.is_active()) return false;
    if (controller.target_link >= scene.blocks.size()) return false;
    const NifAnimation* data = find_animation(scene, block.animation->morpher.data_link);
    return data != nullptr && !data->morph.targets.empty();
}

// Only for a block controller resolves accepted both hops are dereferenced here
AnimatedChannel channel_of(const NifScene& scene, const NifBlock& block, bool play_stopped) {
    AnimatedChannel channel;
    channel.controller = block.animation->controller;
    if (play_stopped) channel.controller.flags |= kControllerActiveBit;
    const NifAnimation* interpolator = find_animation(scene, channel.controller.interpolator_link);
    channel.interpolator = interpolator->interpolator;
    const NifAnimation* keys = find_animation(scene, interpolator->interpolator.data_link);
    if (keys != nullptr) channel.keys = *keys;
    return channel;
}

// A morph weight plays the morpher window on one NiFloatInterpolator of its own list
AnimatedChannel weight_channel_of(const NifScene& scene, const NifBlock& morpher,
                                  uint32_t interpolator_link, bool play_stopped) {
    AnimatedChannel channel;
    channel.controller = morpher.animation->controller;
    if (play_stopped) channel.controller.flags |= kControllerActiveBit;
    const NifAnimation* interpolator = find_animation(scene, interpolator_link);
    if (interpolator == nullptr) return channel;
    channel.interpolator = interpolator->interpolator;
    const NifAnimation* keys = find_animation(scene, interpolator->interpolator.data_link);
    if (keys != nullptr) channel.keys = *keys;
    return channel;
}

// Rotation transposed scale reciprocated translation carried back through both
NifPlacement invert_placement(const NifPlacement& placement) {
    NifPlacement inverted;
    const float scale = placement.scale != 0.f ? 1.f / placement.scale : 1.f;
    inverted.scale = scale;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            inverted.rotation[row * 3 + column] = placement.rotation[column * 3 + row];
    for (int axis = 0; axis < 3; ++axis) {
        float sum = 0.f;
        for (int step = 0; step < 3; ++step)
            sum += inverted.rotation[axis * 3 + step] * placement.translation[step];
        inverted.translation[axis] = -scale * sum;
    }
    return inverted;
}

// The first property of a type on a geometry block or kNoLink
uint32_t property_link_of(const NifScene& scene, const NifBlock& geometry,
                          const std::string& type) {
    for (const uint32_t link : geometry.properties) {
        if (link >= scene.blocks.size()) continue;
        if (scene.blocks[link].type == type) return link;
    }
    return kNoLink;
}

// Only the base map is sampled a controller on another slot is ignored
bool drives_the_map(const NifTextureTransform& transform, uint32_t map) {
    return !transform.is_shader_map && transform.map_index == map;
}

// The map slots of the NiTexturingProperty the renderer builds a matrix for
constexpr uint32_t kBaseMap = 0;
constexpr uint32_t kDetailMap = 2;

// TransformMethod 1 Max is the only composition the renderer builds
constexpr uint32_t kMaxTransformMethod = 1;

void retarget_component(NifTextureTransformOperation operation, int slot, UvAnimation& out) {
    switch (operation) {
        case NifTextureTransformOperation::TranslateU: out.translate_u = slot; break;
        case NifTextureTransformOperation::TranslateV: out.translate_v = slot; break;
        case NifTextureTransformOperation::Rotate:     out.rotate = slot;      break;
        case NifTextureTransformOperation::ScaleU:     out.scale_u = slot;     break;
        case NifTextureTransformOperation::ScaleV:     out.scale_v = slot;     break;
    }
}

bool drives_any_component(const UvAnimation& channel) {
    return channel.translate_u >= 0 || channel.translate_v >= 0 || channel.rotate >= 0 ||
           channel.scale_u >= 0 || channel.scale_v >= 0;
}

// Depth first walk baking the transforms onto the geometry untextured shapes skipped animated nodes kept as channels
class PropPartCollector {
public:
    PropPartCollector(const NifScene& scene, const NifModelRequest& request, PropModel& out)
        : scene_(scene), request_(request), out_(out) {}

    void collect_from_roots();

private:
    // The block the placement since the nearest animated ancestor and that ancestor
    struct Frame {
        uint32_t     block_index = 0;
        NifPlacement placement;
        int          animated_node = -1;
    };

    // One part is one texture one channel set and one device state
    struct PartKey {
        std::string   texture;
        std::string   detail;
        PartAnimation animation;
        uint32_t      alpha_property    = kNoLink;
        uint32_t      depth_property    = kNoLink;
        uint32_t      material_property = kNoLink;
        uint8_t       shading           = 0;
        bool          vertex_colours    = false;

        bool operator<(const PartKey& other) const {
            return std::tie(texture, detail, animation.node, animation.uv, animation.alpha,
                            animation.morph, animation.flip, animation.detail_uv, alpha_property,
                            depth_property, material_property, shading, vertex_colours) <
                   std::tie(other.texture, other.detail, other.animation.node, other.animation.uv,
                            other.animation.alpha, other.animation.morph, other.animation.flip,
                            other.animation.detail_uv, other.alpha_property, other.depth_property,
                            other.material_property, other.shading, other.vertex_colours);
        }
    };

    void index_controllers();
    void collect_particle_systems();
    void build_particle_system(uint32_t system_block);
    void take_texture_flip(uint32_t texturing_link, ParticleSystemDefinition& system) const;
    void take_particle_mesh(uint32_t node_link, bool own_transform, ParticleSystemDefinition& system) const;
    void collect_modifiers(uint32_t system_block, ParticleSystemDefinition& system) const;
    void collect_emitter_triangles(const NifPSysEmitter& emitter, uint32_t system_block,
                                   std::vector<float>& out) const;
    bool begin_animated_node(Frame& frame);
    NifPlacement model_space(const Frame& frame) const;
    PartAnimation part_animation_of(const NifBlock& geometry, uint32_t geometry_link,
                                    const NifSurfaceState& surface, int animated_node);
    int  uv_channel_of(uint32_t property_link, uint32_t map);
    void take_texture_controller(uint32_t controller, UvAnimation& channel, uint32_t map);
    int  alpha_channel_of(uint32_t property_link);
    int  flip_channel_of(uint32_t texturing_link);
    int  morph_channel_of(uint32_t geometry_link);
    const NifMorphData* morph_data_of(uint32_t geometry_link) const;
    void bake_morph_frames(const NifMorphData& morph, const Frame& frame,
                           size_t vertex_count, uint32_t base, PropPart& out) const;
    void emit_geometry(const NifBlock& geometry, const Frame& frame);
    void push_root_children(uint32_t root_index);
    void push_children(const Frame& frame);
    PropPart& part_for(const PartKey& key, const NifSurfaceState& surface);

    const NifScene&        scene_;
    const NifModelRequest& request_;
    PropModel&             out_;
    std::map<PartKey, size_t>                 slots_;
    std::map<uint32_t, uint32_t>              transform_controllers_;
    std::map<uint32_t, uint32_t>              emitter_controllers_;
    std::map<uint32_t, std::vector<uint32_t>> texture_controllers_;
    std::map<uint32_t, uint32_t>              alpha_controllers_;
    std::map<uint32_t, uint32_t>              morph_controllers_;
    std::map<uint32_t, int>                   morph_channels_;
    std::map<uint32_t, int>                   flip_channels_;
    std::map<uint32_t, uint32_t>              flip_controllers_;
    std::map<std::pair<uint32_t, uint32_t>, int> uv_channels_;
    std::map<uint32_t, int>                   alpha_channels_;
    // Parallel to the animation nodes each node rest pose in model space
    std::vector<NifPlacement> rest_world_;
    std::vector<Frame>        pending_;
    std::vector<bool>         visited_;
    std::vector<NifSurfaceState> surfaces_;
    // Where the walk found each block for the particle systems built after it
    std::vector<NifPlacement> block_model_space_;
    std::vector<int>          block_animated_node_;
    std::vector<NifPlacement> block_local_placement_;
};

// One pass over the stream the first controller on a target wins
void PropPartCollector::index_controllers() {
    for (const NifBlock& block : scene_.blocks) {
        if (!block.animation) continue;
        const uint32_t target = block.animation->controller.target_link;
        const uint32_t index = static_cast<uint32_t>(&block - scene_.blocks.data());
        if (block.type == "NiGeomMorpherController") {
            if (morpher_resolves(scene_, block, request_.play_stopped_controllers))
                morph_controllers_.emplace(target, index);
            continue;
        }
        if (!controller_resolves(scene_, block, request_.play_stopped_controllers)) continue;
        if (block.type == "NiTransformController") transform_controllers_.emplace(target, index);
        else if (block.type == "NiAlphaController") alpha_controllers_.emplace(target, index);
        else if (block.type == "NiFlipController") flip_controllers_.emplace(target, index);
        else if (block.type == "NiPSysEmitterCtlr") emitter_controllers_.emplace(target, index);
        else if (block.type == "NiTextureTransformController")
            texture_controllers_[target].push_back(index);
    }
}

PropPart& PropPartCollector::part_for(const PartKey& key, const NifSurfaceState& surface) {
    const auto known = slots_.find(key);
    if (known != slots_.end()) return out_.parts[known->second];
    slots_.emplace(key, out_.parts.size());
    PropPart part;
    part.texture_path = (std::filesystem::path(request_.texture_dir) / key.texture).string();
    if (!key.detail.empty())
        part.detail_texture_path = (std::filesystem::path(request_.texture_dir) / key.detail).string();
    part.animation = key.animation;
    part.surface = surface;
    out_.parts.push_back(std::move(part));
    return out_.parts.back();
}

// controller replaces this node transform each frame billboard becomes its own node so the view can turn it
bool PropPartCollector::begin_animated_node(Frame& frame) {
    const auto driven = transform_controllers_.find(frame.block_index);
    const NifBlock& block = scene_.blocks[frame.block_index];
    if (driven == transform_controllers_.end() && !block.has_billboard) return false;
    AnimatedNode node;
    node.parent = frame.animated_node;
    node.local = block.transform;
    if (driven != transform_controllers_.end()) {
        node.channel = channel_of(scene_, scene_.blocks[driven->second],
                                  request_.play_stopped_controllers);
        node.has_channel = true;
    }
    if (block.has_billboard) node.billboard = billboard_facing(block.billboard_mode);
    transform_matrix(as_transform(frame.placement), node.rest);
    rest_world_.push_back(compose_placement(model_space(frame), block.transform));
    frame.animated_node = static_cast<int>(out_.animation.nodes.size());
    frame.placement = NifPlacement();
    out_.animation.nodes.push_back(std::move(node));
    return true;
}

NifPlacement PropPartCollector::model_space(const Frame& frame) const {
    if (frame.animated_node < 0) return frame.placement;
    return compose_placement(rest_world_[static_cast<size_t>(frame.animated_node)],
                             as_transform(frame.placement));
}

// One channel per NiTexturingProperty however many components it animates
int PropPartCollector::uv_channel_of(uint32_t property_link, uint32_t map) {
    const auto known = uv_channels_.find({property_link, map});
    if (known != uv_channels_.end()) return known->second;
    const auto driven = texture_controllers_.find(property_link);
    if (driven == texture_controllers_.end()) return -1;
    UvAnimation channel;
    channel.rest = map == kDetailMap ? scene_.blocks[property_link].detail_texture_matrix
                                     : scene_.blocks[property_link].base_texture_matrix;
    for (const uint32_t controller : driven->second)
        take_texture_controller(controller, channel, map);
    const int index =
        drives_any_component(channel) ? static_cast<int>(out_.animation.uv_channels.size()) : -1;
    if (index >= 0) out_.animation.uv_channels.push_back(channel);
    uv_channels_.emplace(std::make_pair(property_link, map), index);
    return index;
}

void PropPartCollector::take_texture_controller(uint32_t controller, UvAnimation& channel,
                                                uint32_t map) {
    const NifTextureTransform& retarget =
        scene_.blocks[controller].animation->texture_transform;
    if (!drives_the_map(retarget, map)) return;
    if (channel.rest.is_present && channel.rest.method != kMaxTransformMethod) {
        std::cerr << "[render] " << request_.nif_path << " composes a texture matrix by "
                  << "method " << channel.rest.method << ", which the renderer does not build\n";
        return;
    }
    const int slot = static_cast<int>(out_.animation.texture_floats.size());
    out_.animation.texture_floats.push_back(channel_of(scene_, scene_.blocks[controller], request_.play_stopped_controllers));
    retarget_component(retarget.operation, slot, channel);
}

int PropPartCollector::alpha_channel_of(uint32_t property_link) {
    const auto known = alpha_channels_.find(property_link);
    if (known != alpha_channels_.end()) return known->second;
    const auto driven = alpha_controllers_.find(property_link);
    if (driven == alpha_controllers_.end()) return -1;
    const int index = static_cast<int>(out_.animation.alpha_channels.size());
    out_.animation.alpha_channels.push_back(channel_of(scene_, scene_.blocks[driven->second], request_.play_stopped_controllers));
    alpha_channels_.emplace(property_link, index);
    return index;
}

const NifMorphData* PropPartCollector::morph_data_of(uint32_t geometry_link) const {
    const auto driven = morph_controllers_.find(geometry_link);
    if (driven == morph_controllers_.end()) return nullptr;
    const NifBlock& morpher = scene_.blocks[driven->second];
    const NifAnimation* data = find_animation(scene_, morpher.animation->morpher.data_link);
    return data == nullptr ? nullptr : &data->morph;
}

// One channel per morphed geometry holding one weight channel per frame of its NiMorphData
int PropPartCollector::morph_channel_of(uint32_t geometry_link) {
    const auto known = morph_channels_.find(geometry_link);
    if (known != morph_channels_.end()) return known->second;
    const NifMorphData* morph = morph_data_of(geometry_link);
    if (morph == nullptr) return -1;
    const NifBlock& morpher = scene_.blocks[morph_controllers_.find(geometry_link)->second];
    const std::vector<uint32_t>& links = morpher.animation->morpher.weight_interpolator_links;
    MorphAnimation channel;
    channel.relative_targets = morph->relative_targets;
    channel.weights.reserve(morph->targets.size());
    for (size_t frame = 0; frame < morph->targets.size(); ++frame)
        channel.weights.push_back(weight_channel_of(
            scene_, morpher, frame < links.size() ? links[frame] : kNoLink,
            request_.play_stopped_controllers));
    const int index = static_cast<int>(out_.animation.morph_channels.size());
    out_.animation.morph_channels.push_back(std::move(channel));
    morph_channels_.emplace(geometry_link, index);
    return index;
}

// Frame 0 of a relative NiMorphData holds positions the rest hold offsets so only the first is placed
void PropPartCollector::bake_morph_frames(const NifMorphData& morph, const Frame& frame,
                                          size_t vertex_count, uint32_t base,
                                          PropPart& out) const {
    const size_t count = std::min(static_cast<size_t>(morph.vertex_count), vertex_count);
    out.morph_first_vertex = base;
    out.morph_relative = morph.relative_targets;
    out.morph_frames.assign(morph.targets.size(), std::vector<float>());
    for (size_t index = 0; index < morph.targets.size(); ++index) {
        std::vector<float>& baked = out.morph_frames[index];
        baked.assign(count * 3, 0.f);
        const std::vector<float>& authored = morph.targets[index].vectors;
        if (authored.size() < count * 3) continue;
        const bool absolute = !morph.relative_targets || index == 0;
        for (size_t vertex = 0; vertex < count; ++vertex) {
            if (absolute) {
                place_point(frame.placement, &authored[vertex * 3], &baked[vertex * 3]);
                continue;
            }
            float turned[3];
            place_direction(frame.placement, &authored[vertex * 3], turned);
            for (int axis = 0; axis < 3; ++axis)
                baked[vertex * 3 + axis] = turned[axis] * frame.placement.scale;
        }
    }
}

// The resolved links stand in a property on a parent node reaches the shape the crash sheet fades that way
PartAnimation PropPartCollector::part_animation_of(const NifBlock& geometry,
                                                   uint32_t geometry_link,
                                                   const NifSurfaceState& surface,
                                                   int animated_node) {
    PartAnimation animation;
    animation.node = animated_node;
    animation.morph = morph_channel_of(geometry_link);
    uint32_t texturing = property_link_of(scene_, geometry, "NiTexturingProperty");
    if (texturing == kNoLink) texturing = surface.texturing_link;
    if (texturing != kNoLink) animation.uv = uv_channel_of(texturing, kBaseMap);
    if (texturing != kNoLink) animation.flip = flip_channel_of(texturing);
    if (texturing != kNoLink && scene_.blocks[texturing].detail_texture_link != kNoLink)
        animation.detail_uv = uv_channel_of(texturing, kDetailMap);
    uint32_t material = property_link_of(scene_, geometry, "NiMaterialProperty");
    if (material == kNoLink) material = surface.material_link;
    if (material != kNoLink) animation.alpha = alpha_channel_of(material);
    return animation;
}

// flags bit 0 is app culled hides a shape mushman of Forest 02 has two bodies exe shows one
constexpr uint16_t kHiddenObjectFlag = 0x0001u;

void PropPartCollector::emit_geometry(const NifBlock& geometry, const Frame& frame) {
    // KNC PROP LOG names every shape the build leaves out and why
    static const bool logging = std::getenv("KNC_PROP_LOG") != nullptr;
    const auto dropped = [&](const char* why) {
        if (logging)
            std::cerr << "[prop]   shape " << geometry.name << " left out " << why << " flags "
                      << geometry.object_flags << std::endl;
    };
    if ((geometry.object_flags & kHiddenObjectFlag) != 0) return dropped("app culled");
    const std::string texture_name = find_base_texture_file_name(scene_, geometry);
    if (texture_name.empty()) return dropped("no base map");
    const NifBlock& mesh = scene_.blocks[geometry.data_link];
    if (mesh.vertices.empty() || mesh.uvs.size() * 3 != mesh.vertices.size() * 2) return dropped("no uv set");
    const NifSurfaceState& surface = surfaces_[frame.block_index];
    const bool coloured = carries_vertex_colours(mesh);
    const PartKey key{texture_name, find_detail_texture_file_name(scene_, geometry),
                      part_animation_of(geometry, frame.block_index, surface, frame.animated_node),
                      surface.alpha_link, surface.depth_link, surface.material_link,
                      static_cast<uint8_t>(surface_shading(surface, true)), coloured};
    PropPart& part = part_for(key, surface);
    part.has_vertex_colours = coloured;
    // The vertices land in the frame placement the sphere is measured there too
    merge_bound(placed_bound(mesh, frame.placement), part.bound);
    const uint32_t base = append_vertices(mesh, frame.placement, part);
    if (key.animation.morph >= 0) {
        const NifMorphData* morph = morph_data_of(frame.block_index);
        if (morph != nullptr)
            bake_morph_frames(*morph, frame, mesh.vertices.size() / 3, base, part);
    }
    append_indices(mesh, base, part);
}

// Siblings go on the stack in reverse so they come off in file order the client draw order
void PropPartCollector::push_root_children(uint32_t root_index) {
    const NifBlock& root = scene_.blocks[root_index];
    NifPlacement placement;
    if (root.has_transform) placement = compose_placement(placement, root.transform);
    for (auto child = root.children.rbegin(); child != root.children.rend(); ++child) {
        if (*child >= scene_.blocks.size() || visited_[*child]) continue;
        visited_[*child] = true;
        pending_.push_back({*child, placement, -1});
    }
}

void PropPartCollector::push_children(const Frame& frame) {
    const NifBlock& block = scene_.blocks[frame.block_index];
    for (auto child = block.children.rbegin(); child != block.children.rend(); ++child) {
        if (*child >= scene_.blocks.size() || visited_[*child]) continue;
        visited_[*child] = true;
        pending_.push_back({*child, frame.placement, frame.animated_node});
    }
}

void PropPartCollector::collect_from_roots() {
    visited_.assign(scene_.blocks.size(), false);
    surfaces_ = resolve_surface_states(scene_);
    block_model_space_.assign(scene_.blocks.size(), NifPlacement());
    block_local_placement_.assign(scene_.blocks.size(), NifPlacement());
    block_animated_node_.assign(scene_.blocks.size(), -1);
    index_controllers();
    const std::vector<uint32_t> roots = find_root_block_indices(scene_);
    for (auto root = roots.rbegin(); root != roots.rend(); ++root) push_root_children(*root);

    while (!pending_.empty()) {
        Frame frame = pending_.back();
        pending_.pop_back();
        const NifBlock& block = scene_.blocks[frame.block_index];
        if (!begin_animated_node(frame) && block.has_transform)
            frame.placement = compose_placement(frame.placement, block.transform);
        block_model_space_[frame.block_index] = model_space(frame);
        block_local_placement_[frame.block_index] = frame.placement;
        block_animated_node_[frame.block_index] = frame.animated_node;
        if (block.data_link < scene_.blocks.size()) {
            merge_bound(placed_bound(scene_.blocks[block.data_link], model_space(frame)),
                        out_.bound);
            emit_geometry(block, frame);
        }
        push_children(frame);
    }
    // The named nodes keep their rest below the nearest animated ancestor a placer poses them
    for (uint32_t index = 0; index < scene_.blocks.size(); ++index) {
        const NifBlock& block = scene_.blocks[index];
        if (!visited_[index] || block.type != "NiNode" || block.name.empty()) continue;
        NamedNode named;
        named.name = block.name;
        named.animated_node = block_animated_node_[index];
        transform_matrix(as_transform(block_local_placement_[index]), named.rest);
        out_.named_nodes.push_back(std::move(named));
    }
    drop_empty_parts(out_);
    collect_particle_systems();
}

void PropPartCollector::collect_particle_systems() {
    for (uint32_t index = 0; index < scene_.blocks.size(); ++index) {
        const NifBlock& block = scene_.blocks[index];
        if (block.type != "NiParticleSystem" && block.type != "NiMeshParticleSystem") continue;
        if (!block.particles || !visited_[index]) continue;
        build_particle_system(index);
    }
}

// The emitter mesh triangles carried into the system's own space
void PropPartCollector::collect_emitter_triangles(const NifPSysEmitter& emitter,
                                                  uint32_t system_block,
                                                  std::vector<float>& out) const {
    const NifPlacement into_system = invert_placement(block_model_space_[system_block]);
    for (const uint32_t mesh_link : emitter.emitter_meshes) {
        if (mesh_link >= scene_.blocks.size()) continue;
        const NifBlock& shape = scene_.blocks[mesh_link];
        if (shape.data_link >= scene_.blocks.size()) continue;
        const NifBlock& mesh = scene_.blocks[shape.data_link];
        const NifPlacement placement =
            compose_placement(into_system, as_transform(block_model_space_[mesh_link]));
        const size_t vertex_count = mesh.vertices.size() / 3;
        for (size_t corner = 0; corner + 2 < mesh.triangles.size(); corner += 3) {
            if (!triangle_in_range(&mesh.triangles[corner], vertex_count)) continue;
            for (int step = 0; step < 3; ++step) {
                float placed[3];
                place_point(placement, &mesh.vertices[mesh.triangles[corner + step] * 3], placed);
                out.insert(out.end(), placed, placed + 3);
            }
        }
    }
}

// The active modifiers in run order with the colour ramp and emitter meshes resolved
void PropPartCollector::collect_modifiers(uint32_t system_block,
                                          ParticleSystemDefinition& system) const {
    for (const uint32_t link : scene_.blocks[system_block].particles->modifiers) {
        if (link >= scene_.blocks.size() || !scene_.blocks[link].psys_modifier) continue;
        const NifPSysModifier& modifier = *scene_.blocks[link].psys_modifier;
        if (!modifier.is_active) continue;
        system.modifiers.push_back(modifier);
        if (modifier.kind == NifPSysModifierKind::Colour) {
            const NifAnimation* ramp = find_animation(scene_, modifier.colour_data_link);
            if (ramp != nullptr) system.colour_ramp = ramp->channel;
        }
        if (modifier.kind == NifPSysModifierKind::MeshEmitter)
            collect_emitter_triangles(modifier.emitter, system_block, system.emitter_triangles);
    }
    std::stable_sort(system.modifiers.begin(), system.modifiers.end(),
                     [](const NifPSysModifier& left, const NifPSysModifier& right) {
                         return left.order < right.order;
                     });
}

// NiFlipController on the base map of a mesh part one channel per NiTexturingProperty it drives
int PropPartCollector::flip_channel_of(uint32_t texturing_link) {
    const auto known = flip_channels_.find(texturing_link);
    if (known != flip_channels_.end()) return known->second;
    const auto driven = flip_controllers_.find(texturing_link);
    if (driven == flip_controllers_.end()) return -1;
    const NifBlock& controller = scene_.blocks[driven->second];
    const NifTextureFlip& flip = controller.animation->texture_flip;
    if (flip.affected_map != 0) return -1;
    FlipAnimation channel;
    for (const uint32_t link : flip.textures) {
        if (link >= scene_.blocks.size()) continue;
        const NifBlock& texture = scene_.blocks[link];
        if (texture.type != "NiSourceTexture" || texture.texture_file_name.empty()) continue;
        channel.textures.push_back(
            (std::filesystem::path(request_.texture_dir) / texture.texture_file_name).string());
    }
    if (channel.textures.empty()) return -1;
    channel.channel = channel_of(scene_, controller, request_.play_stopped_controllers);
    const int index = static_cast<int>(out_.animation.flip_channels.size());
    out_.animation.flip_channels.push_back(std::move(channel));
    flip_channels_.emplace(texturing_link, index);
    return index;
}

// NiFlipController on the base map of the system the files it walks through by its float keys
void PropPartCollector::take_texture_flip(uint32_t texturing_link,
                                          ParticleSystemDefinition& system) const {
    const auto driven = flip_controllers_.find(texturing_link);
    if (driven == flip_controllers_.end()) return;
    const NifBlock& controller = scene_.blocks[driven->second];
    const NifTextureFlip& flip = controller.animation->texture_flip;
    if (flip.affected_map != 0) return;
    for (const uint32_t link : flip.textures) {
        if (link >= scene_.blocks.size()) continue;
        const NifBlock& texture = scene_.blocks[link];
        if (texture.type != "NiSourceTexture" || texture.texture_file_name.empty()) continue;
        system.flip_textures.push_back(
            (std::filesystem::path(request_.texture_dir) / texture.texture_file_name).string());
    }
    if (system.flip_textures.empty()) return;
    system.flip = channel_of(scene_, controller, request_.play_stopped_controllers);
    system.has_flip = true;
}

// The mesh templates drawn one per particle the quad takes their extent and uv patch every template adds to both
void PropPartCollector::take_particle_mesh(uint32_t node_link, bool own_transform,
                                          ParticleSystemDefinition& system) const {
    if (node_link >= scene_.blocks.size()) return;
    struct Pending { uint32_t block; NifPlacement placement; };
    std::vector<Pending> pending{{node_link, NifPlacement()}};
    std::vector<uint8_t> seen(scene_.blocks.size(), 0);
    float lo[3] = {1e30f, 1e30f, 1e30f};
    float hi[3] = {-1e30f, -1e30f, -1e30f};
    float uv_lo[2] = {1e30f, 1e30f};
    float uv_hi[2] = {-1e30f, -1e30f};
    if (system.mesh_particles) {
        // A second template widens what the first one set
        for (int axis = 0; axis < 3; ++axis) {
            lo[axis] = -system.mesh_half_extent;
            hi[axis] = system.mesh_half_extent;
        }
        for (int axis = 0; axis < 2; ++axis) {
            uv_lo[axis] = system.mesh_uv_min[axis];
            uv_hi[axis] = system.mesh_uv_max[axis];
        }
    }
    bool found = false;
    while (!pending.empty()) {
        const Pending frame = pending.back();
        pending.pop_back();
        if (frame.block >= scene_.blocks.size() || seen[frame.block] != 0) continue;
        seen[frame.block] = 1;
        const NifBlock& block = scene_.blocks[frame.block];
        // A container node is the particle space its own transform stays out a template keeps its own
        NifPlacement placement = frame.placement;
        if ((frame.block != node_link || own_transform) && block.has_transform)
            placement = compose_placement(placement, block.transform);
        for (const uint32_t child : block.children) pending.push_back({child, placement});
        if (block.data_link >= scene_.blocks.size()) continue;
        const NifBlock& mesh = scene_.blocks[block.data_link];
        for (size_t corner = 0; corner + 2 < mesh.vertices.size(); corner += 3) {
            float placed[3];
            place_point(placement, &mesh.vertices[corner], placed);
            for (int axis = 0; axis < 3; ++axis) {
                lo[axis] = std::min(lo[axis], placed[axis]);
                hi[axis] = std::max(hi[axis], placed[axis]);
            }
            found = true;
        }
        for (size_t pair = 0; pair + 1 < mesh.uvs.size(); pair += 2) {
            uv_lo[0] = std::min(uv_lo[0], mesh.uvs[pair]);
            uv_hi[0] = std::max(uv_hi[0], mesh.uvs[pair]);
            uv_lo[1] = std::min(uv_lo[1], mesh.uvs[pair + 1]);
            uv_hi[1] = std::max(uv_hi[1], mesh.uvs[pair + 1]);
        }
        // Each triangle uv box is a face patch two triangles of one face share it
        const size_t uv_count = mesh.uvs.size() / 2;
        for (size_t corner = 0; corner + 2 < mesh.triangles.size(); corner += 3) {
            std::array<float, 4> patch = {1e30f, 1e30f, -1e30f, -1e30f};
            bool valid = true;
            for (int step = 0; step < 3; ++step) {
                const uint16_t index = mesh.triangles[corner + step];
                if (index >= uv_count) { valid = false; break; }
                patch[0] = std::min(patch[0], mesh.uvs[index * 2]);
                patch[1] = std::min(patch[1], mesh.uvs[index * 2 + 1]);
                patch[2] = std::max(patch[2], mesh.uvs[index * 2]);
                patch[3] = std::max(patch[3], mesh.uvs[index * 2 + 1]);
            }
            if (!valid) continue;
            bool known = false;
            for (const std::array<float, 4>& seen : system.mesh_uv_patches)
                if (std::abs(seen[0] - patch[0]) < 1e-3f && std::abs(seen[1] - patch[1]) < 1e-3f &&
                    std::abs(seen[2] - patch[2]) < 1e-3f && std::abs(seen[3] - patch[3]) < 1e-3f)
                    known = true;
            if (!known) system.mesh_uv_patches.push_back(patch);
        }
    }
    if (!found) return;
    system.mesh_particles = true;
    float half = 0.f;
    for (int axis = 0; axis < 3; ++axis) half = std::max(half, 0.5f * (hi[axis] - lo[axis]));
    system.mesh_half_extent = half > 0.f ? half : 1.f;
    if (uv_hi[0] >= uv_lo[0] && uv_hi[1] >= uv_lo[1]) {
        for (int axis = 0; axis < 2; ++axis) {
            system.mesh_uv_min[axis] = uv_lo[axis];
            system.mesh_uv_max[axis] = uv_hi[axis];
        }
    }
}

// One authored system with its pool modifiers birth rate and surface
void PropPartCollector::build_particle_system(uint32_t system_block) {
    const NifBlock& block = scene_.blocks[system_block];
    ParticleSystemDefinition system;
    system.name = block.name.empty() ? block.type : block.name;
    system.world_space = block.particles->world_space;
    if (system.world_space) {
        // A world space system is born through its placement and does not ride its node
        transform_matrix(as_transform(block_model_space_[system_block]), system.birth);
    } else {
        system.node = block_animated_node_[system_block];
        transform_matrix(as_transform(block_local_placement_[system_block]), system.rest);
    }
    if (block.data_link < scene_.blocks.size() && scene_.blocks[block.data_link].particles) {
        const NifParticleSystem& data = *scene_.blocks[block.data_link].particles;
        system.pool_size = data.pool_size;
        if (block.type == "NiMeshParticleSystem") {
            // Gamebryo clones the templates of the mesh update modifier the data node is a bare container
            for (const uint32_t link : block.particles->modifiers) {
                if (link >= scene_.blocks.size() || !scene_.blocks[link].psys_modifier) continue;
                for (const uint32_t mesh : scene_.blocks[link].psys_modifier->mesh_links)
                    take_particle_mesh(mesh, true, system);
            }
            if (!system.mesh_particles) take_particle_mesh(data.particle_meshes_link, false, system);
            if (!system.mesh_particles)
                std::cerr << "[render] " << system.name << " mesh particles without a mesh template\n";
        }
    }
    const std::string texture = find_base_texture_file_name(scene_, block);
    if (!texture.empty())
        system.texture_path = (std::filesystem::path(request_.texture_dir) / texture).string();
    system.surface = surfaces_[system_block];
    system.alpha_channel = alpha_channel_of(system.surface.material_link);
    uint32_t texturing = property_link_of(scene_, block, "NiTexturingProperty");
    if (texturing == kNoLink) texturing = system.surface.texturing_link;
    if (texturing != kNoLink) take_texture_flip(texturing, system);

    collect_modifiers(system_block, system);

    const auto driven = emitter_controllers_.find(system_block);
    if (driven != emitter_controllers_.end()) {
        const NifBlock& controller = scene_.blocks[driven->second];
        system.birth_rate = channel_of(scene_, controller, request_.play_stopped_controllers);
        system.has_birth_rate = true;
        // The emitter active interpolator stops the births while its bool keys read false
        const uint32_t gate = controller.animation->controller.visibility_interpolator_link;
        if (const NifAnimation* visibility = find_animation(scene_, gate)) {
            system.birth_visibility.controller = system.birth_rate.controller;
            system.birth_visibility.interpolator = visibility->interpolator;
            if (const NifAnimation* keys = find_animation(scene_, visibility->interpolator.data_link))
                system.birth_visibility.keys = *keys;
            system.has_birth_visibility = true;
        }
    }
    if (system.pool_size != 0) out_.particle_systems.push_back(std::move(system));
}

} // namespace

namespace {

// KNC PROP LOG names every built part with its state a probe for a nif that draws short
void log_parts(const NifModelRequest& request, const PropModel& out) {
    static const bool logging = std::getenv("KNC_PROP_LOG") != nullptr;
    if (!logging) return;
    std::cerr << "[prop] " << request.nif_path << " " << out.parts.size() << " parts bound "
              << out.bound.center[0] << " " << out.bound.center[1] << " " << out.bound.center[2]
              << " r " << out.bound.radius << " nodes " << out.animation.nodes.size() << "\n";
    for (const PropPart& part : out.parts) {
        float lo[3] = {1e30f, 1e30f, 1e30f};
        float hi[3] = {-1e30f, -1e30f, -1e30f};
        float uv_lo[2] = {1e30f, 1e30f};
        float uv_hi[2] = {-1e30f, -1e30f};
        for (const SceneVertex& vertex : part.vertices) {
            const float point[3] = {vertex.x, vertex.y, vertex.z};
            for (int axis = 0; axis < 3; ++axis) {
                lo[axis] = std::min(lo[axis], point[axis]);
                hi[axis] = std::max(hi[axis], point[axis]);
            }
            uv_lo[0] = std::min(uv_lo[0], vertex.u);
            uv_lo[1] = std::min(uv_lo[1], vertex.v);
            uv_hi[0] = std::max(uv_hi[0], vertex.u);
            uv_hi[1] = std::max(uv_hi[1], vertex.v);
        }
        std::cerr << "[prop]   " << std::filesystem::path(part.texture_path).filename().string()
                  << " v " << part.vertices.size() << " t " << part.indices.size() / 3
                  << " box " << lo[0] << " " << lo[1] << " " << lo[2] << " to "
                  << hi[0] << " " << hi[1] << " " << hi[2]
                  << " r " << part.bound.radius
                  << " blend " << part.surface.alpha.blend_enabled()
                  << " src " << int(part.surface.alpha.source_blend())
                  << " dst " << int(part.surface.alpha.destination_blend())
                  << " fn " << int(part.surface.alpha.test_function())
                  << " test " << part.surface.alpha.test_enabled()
                  << " ref " << int(part.surface.alpha.test_threshold)
                  << " zwrite " << part.surface.depth.write_enabled()
                  << " ztest " << part.surface.depth.test_enabled()
                  << " zflags " << part.surface.depth.flags
                  << " twosided " << part.surface.two_sided
                  << " vcol " << part.has_vertex_colours
                  << " node " << part.animation.node << " uv " << part.animation.uv
                  << " flip " << part.animation.flip << " detailuv " << part.animation.detail_uv
                  << " alphachan " << part.animation.alpha
                  << " morphchan " << part.animation.morph
                  << " morphframes " << part.morph_frames.size()
                  << " matalpha " << part.surface.material.alpha
                  << " apply " << int(part.surface.texture_apply)
                  << " detail " << std::filesystem::path(part.detail_texture_path).filename().string()
                  << " uvbox " << uv_lo[0] << " " << uv_lo[1] << " to " << uv_hi[0] << " " << uv_hi[1]
                  << " emis " << part.surface.material.emissive[0] << " " << part.surface.material.emissive[1]
                  << " " << part.surface.material.emissive[2]
                  << " amb " << part.surface.material.ambient[0] << " " << part.surface.material.ambient[1]
                  << " " << part.surface.material.ambient[2]
                  << " diff " << part.surface.material.diffuse[0] << " " << part.surface.material.diffuse[1]
                  << " " << part.surface.material.diffuse[2] << "\n";
    }
    for (size_t index = 0; index < out.animation.nodes.size(); ++index) {
        const AnimatedNode& node = out.animation.nodes[index];
        if (node.billboard == BillboardFacing::None) continue;
        std::cerr << "[prop]   node " << index << " billboard " << int(node.billboard)
                  << " parent " << node.parent << " keys " << node.has_channel << "\n";
    }
    for (size_t index = 0; index < out.animation.morph_channels.size(); ++index) {
        const MorphAnimation& channel = out.animation.morph_channels[index];
        std::cerr << "[prop]   morph " << index << " frames " << channel.weights.size()
                  << " relative " << channel.relative_targets;
        for (const AnimatedChannel& weight : channel.weights)
            std::cerr << " [" << weight.keys.channel.key_count() << " keys pose "
                      << weight.interpolator.pose_value << "]";
        std::cerr << "\n";
    }
    for (size_t index = 0; index < out.animation.flip_channels.size(); ++index) {
        const FlipAnimation& flip = out.animation.flip_channels[index];
        std::cerr << "[prop]   flip " << index << " files " << flip.textures.size() << " keys "
                  << flip.channel.keys.channel.key_count() << " window "
                  << flip.channel.controller.start_time << " to " << flip.channel.controller.stop_time
                  << " freq " << flip.channel.controller.frequency << " cycle "
                  << int(flip.channel.controller.cycle_type()) << ":";
        for (size_t k = 0; k < flip.channel.keys.channel.key_count(); ++k)
            std::cerr << " " << flip.channel.keys.channel.times[k] << "="
                      << flip.channel.keys.channel.values[k];
        std::cerr << std::endl;
    }
    for (const ParticleSystemDefinition& system : out.particle_systems) {
        std::cerr << "[prop]   system " << system.name << " pool " << system.pool_size
                  << " world " << system.world_space
                  << " texture " << std::filesystem::path(system.texture_path).filename().string()
                  << " mesh " << system.mesh_particles << " half " << system.mesh_half_extent
                  << " patches " << system.mesh_uv_patches.size()
                  << " ramp " << system.colour_ramp.values.size()
                  << " blend " << system.surface.alpha.blend_enabled()
                  << " src " << int(system.surface.alpha.source_blend())
                  << " dst " << int(system.surface.alpha.destination_blend())
                  << " test " << system.surface.alpha.test_enabled()
                  << " fn " << int(system.surface.alpha.test_function())
                  << " ref " << int(system.surface.alpha.test_threshold)
                  << " matalpha " << system.surface.material.alpha
                  << " filter " << int(system.surface.base_texture_filter) << "\n";
        if (system.has_birth_visibility) {
            std::cerr << "[prop]     gate pose " << system.birth_visibility.interpolator.pose_value
                      << " keys " << system.birth_visibility.keys.channel.key_count() << ":";
            for (size_t k = 0; k < system.birth_visibility.keys.channel.key_count(); ++k)
                std::cerr << " " << system.birth_visibility.keys.channel.times[k] << "="
                          << system.birth_visibility.keys.channel.values[k];
            std::cerr << "\n";
        }
        if (system.has_birth_rate) {
            std::cerr << "[prop]     rate pose " << system.birth_rate.interpolator.pose_value
                      << " keys " << system.birth_rate.keys.channel.values.size() << ":";
            for (float key : system.birth_rate.keys.channel.values) std::cerr << " " << key;
            std::cerr << " freq " << system.birth_rate.controller.frequency
                      << " span " << system.birth_rate.controller.start_time << " to "
                      << system.birth_rate.controller.stop_time << "\n";
        }
        for (const NifPSysModifier& modifier : system.modifiers) {
            std::cerr << "[prop]     modifier " << int(modifier.kind) << " order " << modifier.order;
            if (modifier.kind == NifPSysModifierKind::GrowFade)
                std::cerr << " grow " << modifier.grow_fade.grow_time << " fade "
                          << modifier.grow_fade.fade_time;
            if (modifier.kind == NifPSysModifierKind::Gravity)
                std::cerr << " strength " << modifier.gravity.strength << " decay "
                          << modifier.gravity.decay;
            if (modifier.kind == NifPSysModifierKind::Rotation)
                std::cerr << " speed " << modifier.rotation.speed << " angle " << modifier.rotation.angle;
            if (modifier.kind >= NifPSysModifierKind::BoxEmitter)
                std::cerr << " speed " << modifier.emitter.speed << " life " << modifier.emitter.life_span
                          << " life var " << modifier.emitter.life_span_variation
                          << " radius " << modifier.emitter.initial_radius
                          << " radius var " << modifier.emitter.radius_variation
                          << " box " << modifier.emitter.box_extent[0] << " "
                          << modifier.emitter.box_extent[1] << " " << modifier.emitter.box_extent[2]
                          << " colour " << modifier.emitter.initial_colour[0] << " "
                          << modifier.emitter.initial_colour[1] << " "
                          << modifier.emitter.initial_colour[2] << " "
                          << modifier.emitter.initial_colour[3]
                          << " declination " << modifier.emitter.declination
                          << " planar " << modifier.emitter.planar_angle;
            std::cerr << "\n";
        }
    }
}

}

namespace {

// KNC PART TINT flags a part by colour index red plus green times 8 plus blue times 64
void tint_parts_by_index(PropModel& out) {
    static const bool wanted = std::getenv("KNC_PART_TINT") != nullptr;
    if (!wanted) return;
    for (size_t index = 0; index < out.parts.size(); ++index) {
        PropPart& part = out.parts[index];
        part.texture_path.clear();
        part.surface.texture_apply = NifTextureApplyMode::Modulate;
        part.surface.vertex_colour.lighting = NifLightingMode::EmissiveAmbientDiffuse;
        part.surface.vertex_colour.vertex = NifVertexMode::Ignore;
        part.surface.alpha.flags = 0;
        part.surface.material.alpha = 1.f;
        for (int channel = 0; channel < 3; ++channel) {
            const int step = static_cast<int>((index >> (channel * 3)) & 7u);
            part.surface.material.emissive[channel] = static_cast<float>(step) / 7.f;
            part.surface.material.ambient[channel] = 0.f;
            part.surface.material.diffuse[channel] = 0.f;
        }
    }
}

}

void build_prop_model(const NifScene& scene, const NifModelRequest& request, PropModel& out) {
    out.name = std::filesystem::path(request.nif_path).stem().string();
    PropPartCollector collector(scene, request, out);
    collector.collect_from_roots();
    tint_parts_by_index(out);
    log_parts(request, out);
}

bool load_prop_model(const NifModelRequest& request, PropModel& out, std::string& error) {
    NifScene scene;
    if (!read_nif_scene(request.nif_path, scene, error)) return false;
    build_prop_model(scene, request, out);
    return true;
}

}
