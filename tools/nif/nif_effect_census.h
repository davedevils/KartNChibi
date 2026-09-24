#pragma once
// counts shading alpha and particle system stats across NIF files
#include "engine/formats/nif_reader.h"

#include <cstddef>
#include <map>
#include <string>

namespace KnC::census {

// running totals across all files for surfaces particle systems and modifiers
struct EffectTotals {
    size_t files_with_particles = 0;
    size_t surfaces = 0;              // geometry and particle systems together
    size_t surfaces_with_alpha = 0;
    size_t surfaces_with_depth = 0;
    size_t surfaces_with_material = 0;
    size_t surfaces_emissive = 0;
    size_t vertex_colour_blocks = 0;
    size_t systems = 0;
    size_t systems_in_world_space = 0;
    size_t systems_without_data = 0;
    size_t pool_particles = 0;
    size_t largest_pool = 0;
    size_t modifiers = 0;
    size_t unknown_modifiers = 0;
    size_t inactive_modifiers = 0;
    std::map<std::string, size_t> blend_functions;
    std::map<std::string, size_t> alpha_tests;
    std::map<std::string, size_t> vertex_colour_modes;
    // shading class comes from SetupVertexColorLighting
    std::map<std::string, size_t> shading_classes;
    std::map<std::string, size_t> texture_apply_modes;
    std::map<uint16_t, size_t> depth_flags;
    std::map<std::string, size_t> modifier_kinds;
};

// accumulate feeds one block into totals print totals summarizes all blocks
void accumulate_effect_block(const NifScene& scene, const NifBlock& block,
                             const NifSurfaceState& state, EffectTotals& totals);
void print_effect_totals(const EffectTotals& totals);

// prints one line per surface with its texture alpha depth material and vertex colour state
void print_file_surfaces(const NifScene& scene);
// prints one line per particle system with its pool size and modifier list
void print_file_particles(const NifScene& scene);

}
