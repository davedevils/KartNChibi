#pragma once
// NIF stream and kf clips as glTF document data model reader holds
#include "engine/formats/gltf_writer.h"
#include "engine/formats/nif_reader.h"

#include <cstddef>
#include <string>
#include <vector>

namespace KnC {

// One kf stream bind onto model NiControllerSequence becomes glTF animation
struct NifGltfClipStream {
    const NifScene* stream = nullptr;
    // Prefixed onto sequence own name empty nameless clips told apart
    std::string     name;
};

struct NifGltfOptions {
    // Samples per second clip time every channel resampled trade file size
    float sample_rate = 30.f;
    // Root node turn stream Z up into Y up off leaves axes
    bool  convert_to_y_up = true;
    // Every geometry block not only base texture 98879 of 202231 draws none
    bool  include_untextured = false;
};

// Base map document names file converted from client DDS glTF PNG
struct NifGltfTexture {
    std::string source_name;
    std::string uri;
};

// Export report what turned out every place not exact counted
struct NifGltfReport {
    std::size_t nodes = 0;
    std::size_t meshes = 0;
    std::size_t primitives = 0;
    std::size_t triangles = 0;
    std::size_t materials = 0;
    std::size_t unlit_materials = 0;
    std::size_t skins = 0;
    std::size_t joints = 0;
    std::size_t animations = 0;
    std::size_t animation_channels = 0;
    // Channels resampled B spline off quadratic TCB reason channel sampled
    std::size_t bspline_channels = 0;
    std::size_t curved_key_channels = 0;
    std::size_t linear_key_channels = 0;
    // Sequence entry naming node model not got spline control points
    std::size_t unbound_sequence_entries = 0;
    // Two channels one clip driving same node path glTF allows dropped
    std::size_t duplicate_channels = 0;
    // Geometry skipped by reason left out
    std::size_t skipped_untextured = 0;
    std::size_t skipped_without_uv = 0;
    std::size_t skipped_unreachable = 0;
    // Geometry data block carries vertices no triangle
    std::size_t skipped_without_triangles = 0;
    // Geometry data block no vertex array NiPSysData 1001 particle systems
    std::size_t skipped_without_vertices = 0;
    // Skin bone links not all reach model node exported unskinned
    std::size_t skins_with_unreachable_bones = 0;
    // Vertices NiSkinData over four influences JOINTS 0 largest weight
    std::size_t vertices_over_four_influences = 0;
    float       worst_dropped_weight = 0.f;
    // Largest sum minus 1 authored weights before export renormalised
    float       worst_weight_sum_error = 0.f;
    // Largest distance skinned vertex exported skin bind pose geometry
    float       worst_bind_pose_error = 0.f;
    // Largest deviation orthonormality node rotation largest error quaternion
    float       worst_rotation_orthonormality = 0.f;
    float       worst_quaternion_error = 0.f;
    // Material colour client adds term glTF multiply vertex lit other
    std::size_t materials_with_dropped_term = 0;
    float       worst_dropped_term = 0.f;
    // Chain names no NiMaterialProperty renderer default no bytes extras
    std::size_t materials_without_a_material_property = 0;
    // Clips own frequency not 1 sample times divided glTF play
    std::size_t clips_with_frequency = 0;
    std::size_t clips_without_duration = 0;

    std::vector<NifGltfTexture> textures;
};

// One export model clip streams bound onto it how to write
struct NifGltfRequest {
    const NifScene*                model = nullptr;
    std::vector<NifGltfClipStream> clips;
    NifGltfOptions                 options;
};

struct NifGltfResult {
    GltfDocument  document;
    NifGltfReport report;
    std::string   error;
};

// Build document false when model no node shortfall counted exported
bool build_nif_gltf(const NifGltfRequest& request, NifGltfResult& out);

// glTF material extras surface property chain resolved PBR one baseColorFactor
struct NifGltfMaterialExtras {
    NifMaterialState     material;
    NifAlphaState        alpha;
    NifDepthState        depth;
    NifVertexColourState vertex_colour;
    NifTextureApplyMode  texture_apply = NifTextureApplyMode::Replace;
    NifTextureClamp      base_texture_clamp = NifTextureClamp::WrapUWrapV;
    // Chain names block own each family false renderer default no bytes
    bool has_material      = false;
    bool has_alpha         = false;
    bool has_depth         = false;
    bool has_vertex_colour = false;
};

// Fill out from extras build nif gltf false not set exporter
bool read_nif_gltf_material_extras(const std::vector<GltfExtra>& extras,
                                   NifGltfMaterialExtras& out);

// Unit quaternion x y z w as row major 3x3 rotation NIF
void gltf_rotation_of_quaternion(const float quaternion[4], float out[9]);

} // namespace KnC
