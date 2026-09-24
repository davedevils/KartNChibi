#pragma once
// character geometry with vertices named by bones skeleton and animation clips
#include "engine/formats/nif_effect.h"
#include "engine/formats/nif_skeleton.h"
#include "engine/render/scene_vertex.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Render {

// max bone matrices per draw 24 ships with 2294 partitions merge to 299
constexpr int kBonePaletteSize = 24;
constexpr int kBoneMatrixRows = 3;
constexpr int kWeightsPerVertex = 4;

// surface vertex with palette slot weights uint8 slots float weights sum one
struct SkinnedVertex {
    SceneVertex surface;
    uint8_t     bone_slot[kWeightsPerVertex] = {0, 0, 0, 0};
    float       bone_weight[kWeightsPerVertex] = {1.f, 0.f, 0.f, 0.f};
};

// one draw geometry shares texture palette width kBonePaletteSize 1 point 7 splits
struct SkinnedPart {
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t>      indices;
    std::string                texture_path;
    // surface state from NiAlphaProperty NiZBufferProperty NiMaterialProperty same as prop
    KnC::NifSurfaceState     surface;
    bool                       has_vertex_colours = false;
};

// bones each slot names skeleton node and skin bind transform parallel
struct BonePalette {
    std::vector<int>                 nodes;
    std::vector<KnC::NifTransform> binds;
};

// animation clip with Animation ini id idle walk or swing
struct CharacterClip {
    int32_t                sequence_id = 0;
    KnC::NifSkeletonClip motion;
};

// what poses a character kept after geometry on GPU
struct CharacterRig {
    KnC::NifSkeleton         skeleton;
    std::vector<CharacterClip> clips;
    std::vector<BonePalette>   palettes;   // parallel to CharacterModel parts
};

// character model uploaded once for many spawns
struct CharacterModel {
    std::string              name;
    CharacterRig             rig;
    std::vector<SkinnedPart> parts;
    // rest pose bounding box minimum
    float bounds_min[3] = {0.f, 0.f, 0.f};
    float bounds_max[3] = {0.f, 0.f, 0.f};
};

// character standing somewhere as column major world matrix
struct CharacterInstance {
    std::size_t model_index = 0;
    int         clip = -1;
    // -1 stands the model in rest pose clip seconds negative shares scene clock across tile spawns
    float       clip_seconds = -1.f;
    float       world[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                             0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
};

// skeleton pose sized to rig refreshed once per frame
struct SkinnedPose {
    std::vector<KnC::NifPlacement> nodes;
    // bone matrices every palette kBoneMatrixRows vec4 per slot model space
    std::vector<float>       rows;
    std::vector<std::size_t> part_offset;   // into rows one per palette
};

// sample rig clip at seconds into out or rest pose
void evaluate_skinned_pose(const CharacterRig& rig, int clip, float seconds, SkinnedPose& out);

}
