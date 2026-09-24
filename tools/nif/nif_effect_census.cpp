#include "tools/nif/nif_effect_census.h"

#include <algorithm>
#include <iostream>

namespace KnC::census {

namespace {

bool is_geometry(const NifBlock& block) {
    return block.type == "NiTriShape" || block.type == "NiTriStrips";
}

bool is_white(const float colour[3]) {
    return colour[0] >= 1.f && colour[1] >= 1.f && colour[2] >= 1.f;
}

bool is_emissive(const NifMaterialState& material) {
    return material.emissive[0] > 0.f || material.emissive[1] > 0.f ||
           material.emissive[2] > 0.f;
}

const char* apply_mode_name(NifTextureApplyMode mode) {
    switch (mode) {
        case NifTextureApplyMode::Replace:    return "replace";
        case NifTextureApplyMode::Decal:      return "decal";
        case NifTextureApplyMode::Modulate:   return "modulate";
        case NifTextureApplyMode::Highlight:  return "highlight";
        case NifTextureApplyMode::Highlight2: return "highlight2";
    }
    return "out of range";
}

// tally one drawn surface texture alpha depth material and vertex colour stats
void accumulate_surface(const NifSurfaceState& state, bool has_base_texture,
                        bool has_vertex_colours, EffectTotals& totals) {
    ++totals.surfaces;
    ++totals.texture_apply_modes[std::string(apply_mode_name(state.texture_apply)) +
                                 (has_base_texture ? ", textured" : ", untextured")];
    // key groups by shading class texture emissive and vertex colour for the histogram
    ++totals.shading_classes[std::string(shaded_by_name(surface_shading(state, has_base_texture))) +
                             (has_base_texture ? ", textured" : ", untextured") +
                             (is_white(state.material.emissive) ? ", white emissive"
                              : is_emissive(state.material) ? ", tinted emissive"
                                                            : ", black emissive") +
                             (has_vertex_colours ? ", vertex colours" : ", no vertex colours")];
    if (state.has_depth) {
        ++totals.surfaces_with_depth;
        ++totals.depth_flags[state.depth.flags];
    }
    if (state.has_material) {
        ++totals.surfaces_with_material;
        if (is_emissive(state.material)) ++totals.surfaces_emissive;
    }
    if (state.has_vertex_colour) {
        const bool emissive_only =
            state.vertex_colour.lighting == NifLightingMode::EmissiveOnly;
        const char* vertex_mode = "ignore";
        if (state.vertex_colour.vertex == NifVertexMode::Emissive) vertex_mode = "emissive";
        else if (state.vertex_colour.vertex == NifVertexMode::AmbientDiffuse)
            vertex_mode = "ambient+diffuse";
        ++totals.vertex_colour_modes[std::string(emissive_only ? "lighting E" : "lighting E_A_D") +
                                     ", vertex " + vertex_mode];
    }
    if (!state.has_alpha) return;
    ++totals.surfaces_with_alpha;
    if (state.alpha.test_enabled())
        ++totals.alpha_tests[std::string(test_function_name(state.alpha.test_function())) + " " +
                             std::to_string(state.alpha.test_threshold) +
                             (state.alpha.no_sorting() ? ", unsorted" : "")];
    if (!state.alpha.blend_enabled()) return;
    ++totals.blend_functions[std::string(blend_function_name(state.alpha.source_blend())) + " -> " +
                             blend_function_name(state.alpha.destination_blend())];
}

void accumulate_particle_block(const NifScene& scene, const NifBlock& block,
                               const NifSurfaceState& state, bool has_base_texture,
                               bool has_vertex_colours, EffectTotals& totals) {
    if (block.psys_modifier) {
        ++totals.modifiers;
        ++totals.modifier_kinds[psys_modifier_kind_name(block.psys_modifier->kind)];
        if (block.psys_modifier->kind == NifPSysModifierKind::Unknown)
            ++totals.unknown_modifiers;
        if (!block.psys_modifier->is_active) ++totals.inactive_modifiers;
    }
    if (block.type != "NiParticleSystem" && block.type != "NiMeshParticleSystem") return;
    ++totals.systems;
    if (block.particles && block.particles->world_space) ++totals.systems_in_world_space;
    accumulate_surface(state, has_base_texture, has_vertex_colours, totals);
    const bool has_data = block.data_link < scene.blocks.size() &&
                          scene.blocks[block.data_link].particles;
    if (!has_data) {
        ++totals.systems_without_data;
        return;
    }
    const size_t pool = scene.blocks[block.data_link].particles->pool_size;
    totals.pool_particles += pool;
    totals.largest_pool = std::max(totals.largest_pool, pool);
}

} // namespace

void accumulate_effect_block(const NifScene& scene, const NifBlock& block,
                             const NifSurfaceState& state, EffectTotals& totals) {
    if (!block.vertex_colours.empty()) ++totals.vertex_colour_blocks;
    const bool drawn = is_geometry(block) || block.type == "NiParticleSystem" ||
                       block.type == "NiMeshParticleSystem";
    const bool has_base_texture = drawn && !find_base_texture_file_name(scene, block).empty();
    const bool has_vertex_colours = block.data_link < scene.blocks.size() &&
                                    !scene.blocks[block.data_link].vertex_colours.empty();
    accumulate_particle_block(scene, block, state, has_base_texture, has_vertex_colours, totals);
    if (is_geometry(block))
        accumulate_surface(state, has_base_texture, has_vertex_colours, totals);
}

void print_effect_totals(const EffectTotals& totals) {
    std::cout << "  surface state: " << totals.surfaces_with_alpha << "/" << totals.surfaces
              << " surfaces name an NiAlphaProperty, " << totals.surfaces_with_depth
              << " an NiZBufferProperty, " << totals.surfaces_with_material
              << " an NiMaterialProperty (" << totals.surfaces_emissive
              << " of those emissive); " << totals.vertex_colour_blocks
              << " data block(s) carry vertex colours\n";
    for (const auto& entry : totals.blend_functions)
        std::cout << "    blend " << entry.first << " x" << entry.second << "\n";
    for (const auto& entry : totals.alpha_tests)
        std::cout << "    alpha test " << entry.first << " x" << entry.second << "\n";
    for (const auto& entry : totals.vertex_colour_modes)
        std::cout << "    vertex colour " << entry.first << " x" << entry.second << "\n";
    for (const auto& entry : totals.texture_apply_modes)
        std::cout << "    texture apply " << entry.first << " x" << entry.second << "\n";
    for (const auto& entry : totals.shading_classes)
        std::cout << "    shaded by " << entry.first << " x" << entry.second << "\n";
    for (const auto& entry : totals.depth_flags)
        std::cout << "    NiZBufferProperty flags 0x" << std::hex << entry.first
                  << std::dec << " x" << entry.second << "\n";
    if (totals.systems == 0) return;
    std::cout << "  particles: " << totals.files_with_particles << " file(s) carry any; "
              << totals.systems << " system(s), " << totals.systems_in_world_space
              << " simulating in world space, " << totals.systems_without_data
              << " reaching no NiPSysData; pools hold " << totals.pool_particles
              << " particles, largest " << totals.largest_pool << "\n"
              << "    " << totals.modifiers << " modifier(s), "
              << totals.unknown_modifiers << " retaining nothing, "
              << totals.inactive_modifiers << " shipped inactive\n";
    for (const auto& entry : totals.modifier_kinds)
        std::cout << "    " << entry.first << " x" << entry.second << "\n";
}

// prints tuned values for known modifier kinds unknown kinds show nothing
void print_modifier_parameters(const NifPSysModifier& modifier) {
    if (modifier.kind == NifPSysModifierKind::BoxEmitter ||
        modifier.kind == NifPSysModifierKind::MeshEmitter)
        std::cout << "      speed " << modifier.emitter.speed << " +/- "
          << modifier.emitter.speed_variation << ", declination "
          << modifier.emitter.declination << " +/- "
          << modifier.emitter.declination_variation << ", planar "
          << modifier.emitter.planar_angle << " +/- "
          << modifier.emitter.planar_angle_variation << ", radius "
          << modifier.emitter.initial_radius << " +/- "
          << modifier.emitter.radius_variation << ", life "
          << modifier.emitter.life_span << " +/- "
          << modifier.emitter.life_span_variation << ", box "
          << modifier.emitter.box_extent[0] << " x "
          << modifier.emitter.box_extent[1] << " x "
          << modifier.emitter.box_extent[2] << ", colour "
          << modifier.emitter.initial_colour[0] << " "
          << modifier.emitter.initial_colour[1] << " "
          << modifier.emitter.initial_colour[2] << " "
          << modifier.emitter.initial_colour[3] << "\n";
    if (modifier.kind == NifPSysModifierKind::GrowFade)
        std::cout << "      grow " << modifier.grow_fade.grow_time << "s gen "
          << modifier.grow_fade.grow_generation << ", fade "
          << modifier.grow_fade.fade_time << "s gen "
          << modifier.grow_fade.fade_generation << "\n";
    if (modifier.kind == NifPSysModifierKind::Gravity)
        std::cout << "      axis " << modifier.gravity.axis[0] << " "
          << modifier.gravity.axis[1] << " " << modifier.gravity.axis[2]
          << ", strength " << modifier.gravity.strength << ", decay "
          << modifier.gravity.decay << "\n";
    if (modifier.kind == NifPSysModifierKind::Rotation)
        std::cout << "      speed " << modifier.rotation.speed << " +/- "
          << modifier.rotation.speed_variation << ", angle "
          << modifier.rotation.angle << " +/- "
          << modifier.rotation.angle_variation << "\n";
}

void print_file_surfaces(const NifScene& scene) {
    const std::vector<NifSurfaceState> surfaces = resolve_surface_states(scene);
    for (size_t index = 0; index < scene.blocks.size(); ++index) {
        const NifBlock& block = scene.blocks[index];
        const bool particles = block.type == "NiParticleSystem" ||
                               block.type == "NiMeshParticleSystem";
        if (!is_geometry(block) && !particles) continue;
        const NifSurfaceState& state = surfaces[index];
        std::cout << "  block " << index << " " << block.type << " '" << block.name
                  << "' texture '"
                  << find_base_texture_file_name(scene, block) << "'\n    ";
        if (!state.has_alpha) std::cout << "no NiAlphaProperty";
        else if (state.alpha.blend_enabled())
            std::cout << "blend " << blend_function_name(state.alpha.source_blend()) << " -> "
                      << blend_function_name(state.alpha.destination_blend());
        else std::cout << "NiAlphaProperty, blending off";
        if (state.has_alpha && state.alpha.test_enabled())
            std::cout << ", test " << test_function_name(state.alpha.test_function()) << " "
                      << static_cast<uint32_t>(state.alpha.test_threshold);
        std::cout << ", depth " << (state.depth.test_enabled() ? "test" : "no test") << " "
                  << (state.depth.write_enabled() ? "write" : "no write")
                  << (state.has_depth ? "" : " (default)");
        std::cout << ", emissive " << state.material.emissive[0] << " "
                  << state.material.emissive[1] << " " << state.material.emissive[2]
                  << ", diffuse " << state.material.diffuse[0] << " "
                  << state.material.diffuse[1] << " " << state.material.diffuse[2]
                  << ", material alpha " << state.material.alpha;
        if (!state.has_vertex_colour) {
            std::cout << ", no NiVertexColorProperty\n";
            continue;
        }
        std::cout << ", lighting "
                  << (state.vertex_colour.lighting == NifLightingMode::EmissiveOnly
                          ? "E" : "E_A_D")
                  << ", vertex " << static_cast<uint32_t>(state.vertex_colour.vertex)
                  << "\n";
    }
}

void print_file_particles(const NifScene& scene) {
    for (const auto& block : scene.blocks) {
        if (!block.particles || block.type == "NiPSysData") continue;
        const bool has_data = block.data_link < scene.blocks.size() &&
                              scene.blocks[block.data_link].particles;
        std::cout << "  block " << (&block - scene.blocks.data()) << " " << block.type << " '"
                  << block.name << "' pool "
                  << (has_data ? scene.blocks[block.data_link].particles->pool_size : 0)
                  << (block.particles->world_space ? ", world space\n" : ", local space\n");
        for (const uint32_t link : block.particles->modifiers) {
            if (link >= scene.blocks.size() || !scene.blocks[link].psys_modifier) continue;
            const NifPSysModifier& modifier = *scene.blocks[link].psys_modifier;
            std::cout << "    " << modifier.order << " " << scene.blocks[link].type << " '"
                      << scene.blocks[link].name << "'"
                      << (modifier.is_active ? "" : " INACTIVE") << "\n";
            print_modifier_parameters(modifier);
        }
    }
}

}
