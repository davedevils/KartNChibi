#pragma once
#include "engine/formats/nif_effect.h"
#include "engine/render/day_night.h"
#include "engine/render/model_animation.h"
#include "engine/render/particle_system.h"
#include "engine/render/skinned_model.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace KnC::Render {

// Submission order viewer toggles sky opaque ground translucent layers
enum class SceneLayer {
    Sky,
    Terrain,
    Props,
    Monsters,
    Water,
    Waypaths,
    PropHulls,
    Collision,
    // Reference floor model document stands mesh empty map
    Grid,
    Count
};

constexpr int kSceneLayerCount = static_cast<int>(SceneLayer::Count);

// One splat coat diffuse repeats mask stretches once
struct SplatCoat {
    std::string diffuse_path;
    std::string mask_path;
    // One byte coverage per texel row major mask width
    std::vector<uint8_t> mask_alpha;
    int mask_width = 0;
    int mask_height = 0;
    // Diffuse repeats across mesh mask stretches once offset continues
    float repeat[2] = {1.f, 1.f};
    float offset[2] = {0.f, 0.f};
};

struct LayerMesh {
    std::vector<SceneVertex> vertices;
    std::vector<uint32_t>    indices;
    // Lighting client baked into tile ground draws instead synthetic
    std::vector<uint32_t> baked_abgr;
    // Empty draws mesh once vertex colour collision overlay
    std::vector<SplatCoat> coats;
    bool  translucent = false;
};

// One ground piece coat stack own layer drawn order
struct GroundPatch {
    LayerMesh  mesh;
    SceneLayer layer = SceneLayer::Terrain;
};

// Bounding sphere model space negative radius not set
struct ModelBound {
    float center[3] = {0.f, 0.f, 0.f};
    float radius = -1.f;
};

// One prop model geometry shares texture animated nodes device
struct PropPart {
    std::vector<SceneVertex> vertices;
    std::vector<uint32_t>    indices;
    std::string              texture_path;
    // The detail map of the NiTexturingProperty modulated twice over the base empty for none
    std::string              detail_texture_path;
    PartAnimation            animation;
    // Part NiAlphaProperty NiZBufferProperty NiMaterialProperty ask device
    NifSurfaceState surface;
    // Meshes carry authored vertex colours material source falls back
    bool has_vertex_colours = false;
    // Authored spheres geometries merged into part own space
    ModelBound bound;
    // NiMorphData frames baked into the part space three floats a vertex empty for a still part
    std::vector<std::vector<float>> morph_frames;
    // Where in the part the morphed geometry starts the frames cover its vertices only
    uint32_t morph_first_vertex = 0;
    // NiMorphData relative targets frame 0 is the shape and the rest are offsets
    bool morph_relative = true;
    // The sphere map a NiTextureEffect above the geometry adds empty texture for none
    EnvironmentMap environment;
};

// A named node of the tree the placer asks for its posed matrix the O POS of the gauge nif
struct NamedNode {
    std::string name;
    // The nearest animated ancestor minus one at the model root and the rest below it
    int   animated_node = -1;
    float rest[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                      0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
};

// Prop mesh split one part per texture placement draws
struct PropModel {
    // Mesh stem placement named hierarchy say which model
    std::string           name;
    std::vector<PropPart> parts;
    // Every named NiNode of the tree a dummy the game places something on
    std::vector<NamedNode> named_nodes;
    // Merged way NiBound UpdateWorldBound merges children sky radius
    ModelBound bound;
    // Parts reference empty model never moves
    ModelAnimation animation;
    // NiParticleSystems hanging same tree empty for 3511 meshes
    std::vector<ParticleSystemDefinition> particle_systems;
    // Map NiAmbientLight reaches model surfaces node given light
    bool lit_by_map_ambient = true;
    // The lights the root effect list of the nif hangs over its own parts beside the world ones
    ModelLights lights;
};

// Placement model column major world matrix renderer sets
struct PropInstance {
    size_t     model_index = 0;
    SceneLayer layer = SceneLayer::Props;
    float      world[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                            0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
};

// Which four skies node names up sky twilight 203 rows
enum class SkyTime { Day, Twilight, Night };

// One time of day sky tile node textured geometry
struct SkyPhase {
    std::string name;  // node ini sky name such as s cloud04
    SkyTime     time = SkyTime::Day;
    PropModel   model;
};

// Node own fog CxFog Set NiFogProperty depth fraction
struct SceneFog {
    float depth = 0.f;  // depth 0 unfogs tile CxGame ZoneIn range scale below 1 shrinks above 1 grows fog
    float range_scale = 1.f;
    // fog function 1 RANGEFOGENABLE radial distance two 203
    bool radial = false;
    // How far can see world units fog opaque Holy Beast
    float sight_range = 0.f;
    float max_density = 1.f;
};

// Which four games after Holy Beast name ranked time hour
enum class DayPhaseName { Sunrise, Day, Sunset, Night };

// One four phase environment DJ Jianghu Saga Aura Kingdom
struct DayPhase {
    DayPhaseName name = DayPhaseName::Day;
    int          start_hour = 0;   // 24 and above means the phase owns no hour
    std::string  sky_name;
    HourColour   ambient;
    HourColour   fog_colour;
    // Aura Kingdom FogMax and FogSightRang both 0 generation
    float fog_max = 0.f;
    float sight_range = 0.f;
};

struct MapCounts {
    size_t grid_cells        = 0;
    size_t blocked_cells     = 0;
    size_t placements        = 0;
    size_t wall_placements   = 0;
    size_t unreadable_meshes = 0;
    size_t prop_models       = 0;
    size_t prop_instances    = 0;
    size_t prop_triangles    = 0;
    size_t character_models    = 0;
    size_t character_instances = 0;
    size_t character_draws     = 0;   // one per part which is one bone palette
};

// What tile spawn patrol tables hold panel reports
struct SpawnCounts {
    size_t shipped              = 0;  // rows the client's own spawn table places
    size_t boss                 = 0;
    size_t elite                = 0;
    size_t groups               = 0;  // rows naming a pack instead of a template
    size_t patrol_without_route = 0;
    size_t custom               = 0;  // custom rows we added ourselves event gated ones show only during event custom npcs counted too
    size_t custom_event_gated   = 0;
    size_t custom_npcs          = 0;
    size_t on_walk_grid         = 0;  // on walk grid stood on server surface on scene mesh is ground fallback without ground drawn at z 0
    size_t on_scene_mesh        = 0;
    size_t without_ground       = 0;
    // Tile collision grid says where stand both 0 no grid
    size_t on_blocked_ground    = 0;
    size_t off_walk_grid        = 0;
    size_t routes               = 0;
    size_t route_points         = 0;
};

// The one directional light of a KnC race from World Light nif off for a map without one
struct SunLight {
    // The way the light travels in world space unit length
    float      direction[3] = {0.f, 0.f, -1.f};
    HourColour colour;
    bool       enabled = false;
};

// One tile already reduced renderer draws nothing names game
struct MapScene {
    std::string tile_id;
    SunLight    sun;
    LayerMesh   layers[kSceneLayerCount];
    // Ground cannot be one mesh coat stack different textures
    std::vector<GroundPatch> ground_patches;
    // Uploaded once each prop instances names model draws where
    std::vector<PropModel>    prop_models;
    std::vector<PropInstance> prop_instances;
    // Skinned characters standing on tile Monsters layer spawn markers
    std::vector<CharacterModel>    character_models;
    std::vector<CharacterInstance> character_instances;
    // Day twilight night in order empty node none
    std::vector<SkyPhase> sky_phases;
    // Which four sky phases stands over each hour index
    std::array<uint8_t, kHoursPerDay> sky_phase_of_hour{};
    // Four phase environment sunrise day sunset night empty Holy Beast
    std::vector<DayPhase> day_phases;
    // Hourly ambient fog colours clock reads tile no node white
    DayNightTable day_night = flat_day_night(kDefaultBackdrop);
    // Fog shape each hour Holy Beast node one whole day 24
    std::array<SceneFog, kHoursPerDay> fog_of_hour{};
    MapCounts     counts;
    SpawnCounts   spawns;
    float         bounds_min[3] = {0.f, 0.f, 0.f};
    float         bounds_max[3] = {0.f, 0.f, 0.f};
};

// Nothing measured yet extent source starts from first point
inline void reset_scene_bounds(MapScene& out) {
    for (int axis = 0; axis < 3; ++axis) {
        out.bounds_min[axis] = std::numeric_limits<float>::max();
        out.bounds_max[axis] = std::numeric_limits<float>::lowest();
    }
}

inline void expand_scene_bounds(const float point[3], MapScene& out) {
    for (int axis = 0; axis < 3; ++axis) {
        out.bounds_min[axis] = std::min(out.bounds_min[axis], point[axis]);
        out.bounds_max[axis] = std::max(out.bounds_max[axis], point[axis]);
    }
}

inline size_t triangle_count(const MapScene& scene) {
    size_t total = 0;
    for (const LayerMesh& mesh : scene.layers) total += mesh.indices.size() / 3;
    for (const GroundPatch& patch : scene.ground_patches) total += patch.mesh.indices.size() / 3;
    return total;
}

inline const char* layer_name(SceneLayer layer) {
    switch (layer) {
        case SceneLayer::Sky:       return "sky";
        case SceneLayer::Terrain:   return "terrain";
        case SceneLayer::Water:     return "water";
        case SceneLayer::Props:     return "props";
        case SceneLayer::Monsters:  return "monsters";
        case SceneLayer::Waypaths:  return "waypaths";
        case SceneLayer::PropHulls: return "hulls";
        case SceneLayer::Collision: return "collision";
        case SceneLayer::Grid:      return "grid";
        case SceneLayer::Count:     break;
    }
    return "unknown";
}

} // namespace KnC Render
