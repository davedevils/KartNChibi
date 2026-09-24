#pragma once
// NiParticleSystem lifted from stream to what simulation needs data only
#include "engine/formats/nif_effect.h"
#include "engine/render/model_animation.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace KnC::Render {

// Numbers the game writes on a live system after the load the wheel dust sets both by the speed
struct ParticleDrive {
    // NiTimeController frequency of the emitter controller car dust 0x4A44A0 writes it every frame
    bool  has_frequency = false;
    float frequency     = 1.f;
    // NiPSysGravityModifier strength the same call writes 250 minus the speed clamped 50 to 250
    bool  has_gravity_strength = false;
    float gravity_strength     = 0.f;
};

// Authored particle system model space node or minus 1 and rest
struct ParticleSystemDefinition {
    std::string name;
    int   node    = -1;
    float rest[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                      0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    // World space system leaves particles behind when node moves local carries
    bool     world_space = false;
    uint16_t pool_size   = 0;
    // Model space birth matrix world space at minus 1 local at origin
    float birth[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};

    // System drawn with NiTexturingProperty base map device state property list
    std::string     texture_path;
    NifSurfaceState surface;
    // Fades system material or minus 1 for none 219 effect need fade
    int             alpha_channel = -1;
    // NiFlipController on the base map the float keys truncated pick one of these files
    std::vector<std::string> flip_textures;
    AnimatedChannel          flip;
    bool                     has_flip = false;

    // Modifiers sorted by NiPSysModifier order inactive ones dropped never run
    std::vector<NifPSysModifier> modifiers;
    // NiColorData ramp sampled by NiPSysColorModifier resolved from stream
    NifKeyGroup colour_ramp;
    // Birth rate NiPSysEmitterCtlr drives in particles per second undriven
    AnimatedChannel birth_rate;
    bool            has_birth_rate = false;
    // NiPSysEmitterCtlr emitter active interpolator the bool that gates the births
    AnimatedChannel birth_visibility;
    bool            has_birth_visibility = false;
    // World space triangles from NiPSysMeshEmitter meshes three vertices per triangle
    std::vector<float> emitter_triangles;
    // NiMeshParticleSystem draws a mesh per particle scaled by radius the quad stands in with this half extent
    bool  mesh_particles   = false;
    float mesh_half_extent = 1.f;
    // The uv patch of the particle mesh the quad samples the same piece of the sheet
    float mesh_uv_min[2] = {0.f, 0.f};
    float mesh_uv_max[2] = {1.f, 1.f};
    // One patch per face of the template u0 v0 u1 v1 a particle shows one face like a turned piece
    std::vector<std::array<float, 4>> mesh_uv_patches;
    // Shared with the placer that moves the numbers per frame null keeps the authored ones
    std::shared_ptr<ParticleDrive> drive;
};

}
