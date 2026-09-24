// Particle half of scene renderer authored system GPU stepped drawn
#include "engine/render/scene_renderer.h"

#include "engine/formats/nif_animation_eval.h"
#include "engine/render/scene_draw_state.h"
#include "engine/render/view_order.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace KnC::Render {

namespace {

// Four vertices two triangles per particle NiDX9Renderer PackAndDrawParticles no
constexpr uint32_t kParticleVertices = 4;
constexpr uint32_t kParticleIndices  = 6;
// The quad half extent per emitter radius measured on the stock dust puffs alive at 148 km per hour
constexpr float kRadiusToHalfExtent = 0.3f;

// NiFlipController Update the float value truncated picks the file none below the first
int flip_frame(const ParticleSystemDefinition& definition, float played) {
    if (!definition.has_flip || definition.flip_textures.empty()) return -1;
    float key_time = definition.flip.controller.start_time;
    if (!map_controller_time(definition.flip.controller, played, key_time)) return -1;
    const NifFloatSample sample =
        evaluate_float(definition.flip.keys, definition.flip.interpolator, key_time);
    if (!sample.is_driven || sample.value < 0.f) return 0;
    const int last = static_cast<int>(definition.flip_textures.size()) - 1;
    return std::min(static_cast<int>(sample.value), last);
}

// quad half extent from radius times size uv patch of the sheet u is camera right v is down
void write_particle_quad(const Particle& particle, const float right[3], const float up[3],
                         const float uv_min[2], const float uv_max[2], SceneVertex* out) {
    const float extent = particle.radius * particle.size * kRadiusToHalfExtent;
    const float turn_cos = std::cos(particle.angle) * extent;
    const float turn_sin = std::sin(particle.angle) * extent;
    float along_u[3];
    float along_v[3];
    for (int axis = 0; axis < 3; ++axis) {
        along_u[axis] = right[axis] * turn_cos + up[axis] * turn_sin;
        along_v[axis] = right[axis] * turn_sin - up[axis] * turn_cos;
    }
    const float corner_u[4] = {uv_min[0], uv_max[0], uv_max[0], uv_min[0]};
    const float corner_v[4] = {uv_max[1], uv_max[1], uv_min[1], uv_min[1]};
    const float u_sign[4] = {-1.f, 1.f, 1.f, -1.f};
    const float v_sign[4] = {1.f, 1.f, -1.f, -1.f};
    const uint32_t abgr = nif_colour_abgr(particle.colour);
    for (int corner = 0; corner < 4; ++corner) {
        SceneVertex& vertex = out[corner];
        vertex.x = particle.position[0] + u_sign[corner] * along_u[0] + v_sign[corner] * along_v[0];
        vertex.y = particle.position[1] + u_sign[corner] * along_u[1] + v_sign[corner] * along_v[1];
        vertex.z = particle.position[2] + u_sign[corner] * along_u[2] + v_sign[corner] * along_v[2];
        // Zero normal colour already shaded client never lights particle
        vertex.normal_x = 0.f;
        vertex.normal_y = 0.f;
        vertex.normal_z = 0.f;
        vertex.abgr = abgr;
        vertex.u = corner_u[corner];
        vertex.v = corner_v[corner];
    }
}

// KNC PARTICLE FLAT the probe mode 0 off 1 white opaque 2 sheet opaque 3 sheet at full alpha
int flat_particle_probe() {
    const char* value = std::getenv("KNC_PARTICLE_FLAT");
    if (value == nullptr) return 0;
    const int mode = std::atoi(value);
    return mode > 0 ? mode : 1;
}

// Matrix animated node stands at or null system nothing moves
const float* animated_node_matrix(const ModelPose& pose, int node) {
    if (node < 0 || static_cast<size_t>(node) * 16 >= pose.node_world.size()) return nullptr;
    return &pose.node_world[static_cast<size_t>(node) * 16];
}

// Channel no controller drives keeps material authored alpha
float material_alpha(const ModelPose& pose, int channel, float rest) {
    if (channel < 0 || static_cast<size_t>(channel) >= pose.alpha.size()) return rest;
    return pose.alpha[static_cast<size_t>(channel)];
}

// World space pool needs one placement a model placed once hands its world twice hands none
const float* single_placement_world(const std::vector<PropInstance>& instances,
                                    std::size_t model_index) {
    const float* world = nullptr;
    for (const PropInstance& instance : instances) {
        if (instance.model_index != model_index) continue;
        if (world != nullptr) return nullptr;
        world = instance.world;
    }
    return world;
}

} // namespace

SceneRenderer::ParticleBuffers SceneRenderer::create_particle_system(
    const ParticleSystemDefinition& definition) {
    ParticleBuffers system;
    system.definition = definition;
    // The map filter of the NiTexturingProperty picks the sampler a nearest or bilerp one keeps mip zero
    const bool mipped = filters_mipmaps(definition.surface.base_texture_filter);
    const CachedTexture image = textures_.acquire(definition.texture_path, mipped);
    system.texture = bgfx::isValid(image.handle) ? image.handle : textures_.white_stand_in();
    system.sampler = diffuse_sampler(definition.surface.base_texture_clamp,
                                     definition.surface.base_texture_filter);
    system.draw_state = surface_draw_state(definition.surface);
    system.alpha_cutout = surface_alpha_cutout(definition.surface);
    // Material emissive colour tint on 857 of 864 particle systems
    const NifMaterialState& material = definition.surface.material;
    const bool tinted = material.emissive[0] > 0.f || material.emissive[1] > 0.f ||
                        material.emissive[2] > 0.f;
    system.tint.red = tinted ? material.emissive[0] : 1.f;
    system.tint.green = tinted ? material.emissive[1] : 1.f;
    system.tint.blue = tinted ? material.emissive[2] : 1.f;
    system.rest_alpha = material.alpha;
    return system;
}

// System emitter controller no birth rate emits nothing effect broken
void SceneRenderer::report_particle_systems() const {
    std::size_t systems = 0;
    std::size_t pooled = 0;
    std::size_t without_birth_rate = 0;
    for (const PropModelBuffers& model : prop_models_)
        for (const ParticleBuffers& system : model.particles) {
            ++systems;
            pooled += system.definition.pool_size;
            if (!system.definition.has_birth_rate) ++without_birth_rate;
        }
    if (systems == 0) return;
    std::cout << "[render] particle systems: " << systems << " over " << pooled
              << " pooled particle(s), " << without_birth_rate << " with no birth rate\n";
}

// One pool per system per frame same rule poses follow
void SceneRenderer::refresh_particles() {
    const float seconds = animation_.seconds();
    // KNC PARTICLE LOG prints the pools of the appended models once a second
    static const bool logging = std::getenv("KNC_PARTICLE_LOG") != nullptr;
    static int logged_frames = 0;
    const bool log_now = logging && ++logged_frames % 60 == 0;
    for (std::size_t index = 0; index < prop_models_.size(); ++index) {
        PropModelBuffers& model = prop_models_[index];
        // Tile props epoch 0 run on clock live cast own epoch
        const float played = std::max(0.f, seconds - model.particle_epoch);
        const float* emitter_world = nullptr;
        bool looked_up = false;
        for (ParticleBuffers& system : model.particles) {
            if (system.definition.world_space && !looked_up) {
                emitter_world = single_placement_world(prop_instances_, index);
                looked_up = true;
            }
            system.simulation.advance_to(system.definition, played, emitter_world);
            // The flip swaps the base map the cache hands the same handle back per path
            const int frame = flip_frame(system.definition, played);
            if (frame >= 0) {
                const CachedTexture image = textures_.acquire(
                    system.definition.flip_textures[static_cast<size_t>(frame)],
                    filters_mipmaps(system.definition.surface.base_texture_filter));
                if (bgfx::isValid(image.handle)) system.texture = image.handle;
            }
            if (log_now && model.particle_epoch > 0.f) {
                float lo[3] = {1e30f, 1e30f, 1e30f};
                float hi[3] = {-1e30f, -1e30f, -1e30f};
                float radius = 0.f;
                for (const Particle& particle : system.simulation.particles()) {
                    for (int axis = 0; axis < 3; ++axis) {
                        lo[axis] = std::min(lo[axis], particle.position[axis]);
                        hi[axis] = std::max(hi[axis], particle.position[axis]);
                    }
                    radius = particle.radius;
                }
                std::cout << "[particles] model " << index << ' ' << system.definition.name << " pool "
                          << system.simulation.particles().size() << " played " << played
                          << (emitter_world != nullptr ? " world" : "") << " box " << lo[0] << " " << lo[1]
                          << " " << lo[2] << " to " << hi[0] << " " << hi[1] << " " << hi[2]
                          << " radius " << radius << '\n';
            }
        }
    }
}

// World to view matrix rows camera axes column major row
void SceneRenderer::take_billboard_basis(const float view_matrix[16]) {
    for (int axis = 0; axis < 3; ++axis) {
        billboard_right_[axis] = view_matrix[axis * 4];
        billboard_up_[axis] = view_matrix[axis * 4 + 1];
        // The third column of a right handed view is the axis that looks back at the eye
        camera_forward_[axis] = view_matrix[axis * 4 + 2];
    }
}

void SceneRenderer::submit_particle_system(const ParticleBuffers& system, const ModelPose& pose,
                                           const float world[16]) const {
    const std::vector<Particle>& pool = system.simulation.particles();
    const uint32_t count = static_cast<uint32_t>(pool.size());
    if (count == 0) return;
    if (bgfx::getAvailTransientVertexBuffer(count * kParticleVertices, layout_) <
            count * kParticleVertices ||
        bgfx::getAvailTransientIndexBuffer(count * kParticleIndices) < count * kParticleIndices)
        return;
    // Null world means the pool already holds world space points a world space birth
    constexpr float kIdentity[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                     0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    float local[16];
    if (world == nullptr) {
        for (int i = 0; i < 16; ++i) local[i] = kIdentity[i];
    } else {
        float placed[16];
        const float* node = animated_node_matrix(pose, system.definition.node);
        if (node != nullptr) multiply_matrix(world, node, placed);
        multiply_matrix(node != nullptr ? placed : world, system.definition.rest, local);
    }
    // The placement scale reaches the quad size the camera basis stays a world one
    const float extent_scale = std::sqrt(local[0] * local[0] + local[1] * local[1] + local[2] * local[2]);
    // A mesh particle draws its template scaled by the particle size the emitter radius writes it
    const float mesh_radius = system.definition.mesh_half_extent / kRadiusToHalfExtent;
    bgfx::TransientVertexBuffer vertices;
    bgfx::TransientIndexBuffer indices;
    bgfx::allocTransientVertexBuffer(&vertices, count * kParticleVertices, layout_);
    bgfx::allocTransientIndexBuffer(&indices, count * kParticleIndices);
    SceneVertex* corner = reinterpret_cast<SceneVertex*>(vertices.data);
    uint16_t* triangle = reinterpret_cast<uint16_t*>(indices.data);
    for (uint32_t index = 0; index < count; ++index) {
        // Quads built in world space a placed pool would tilt a model space quad off the camera
        Particle placed_particle = pool[index];
        const float* p = pool[index].position;
        for (int axis = 0; axis < 3; ++axis)
            placed_particle.position[axis] = p[0] * local[axis] + p[1] * local[4 + axis] +
                                             p[2] * local[8 + axis] + local[12 + axis];
        const float* uv_min = system.definition.mesh_uv_min;
        const float* uv_max = system.definition.mesh_uv_max;
        if (system.definition.mesh_particles) {
            placed_particle.radius *= mesh_radius;
            // One face of the template per particle picked by its slot in the pool
            const std::vector<std::array<float, 4>>& patches = system.definition.mesh_uv_patches;
            if (!patches.empty()) {
                const std::array<float, 4>& patch = patches[index % patches.size()];
                uv_min = patch.data();
                uv_max = patch.data() + 2;
            }
        }
        placed_particle.radius *= extent_scale;
        write_particle_quad(placed_particle, billboard_right_, billboard_up_, uv_min, uv_max,
                            corner + index * kParticleVertices);
        const uint16_t base = static_cast<uint16_t>(index * kParticleVertices);
        const uint16_t corners[kParticleIndices] = {base, static_cast<uint16_t>(base + 1),
                                                    static_cast<uint16_t>(base + 2),
                                                    base, static_cast<uint16_t>(base + 2),
                                                    static_cast<uint16_t>(base + 3)};
        for (uint32_t step = 0; step < kParticleIndices; ++step)
            triangle[index * kParticleIndices + step] = corners[step];
    }
    SurfaceUniforms surface;
    surface.splat[2] = system.alpha_cutout;
    surface.splat[3] = material_alpha(pose, system.definition.alpha_channel, system.rest_alpha);
    // KNC PARTICLE DRAW prints the quad the shader gets the probe for a pool that never rasterises
    static const bool draw_logging = std::getenv("KNC_PARTICLE_DRAW") != nullptr;
    if (draw_logging) {
        static int draw_frames = 0;
        if (++draw_frames % 240 == 0) {
            const Particle& first = pool[0];
            const SceneVertex& one = *reinterpret_cast<const SceneVertex*>(vertices.data);
            const float towards[3] = {one.x - camera_position_[0], one.y - camera_position_[1],
                                      one.z - camera_position_[2]};
            const float ahead = towards[0] * camera_forward_[0] + towards[1] * camera_forward_[1] +
                                towards[2] * camera_forward_[2];
            std::cout << "[quad] " << system.definition.name << " eye " << camera_position_[0] << ' '
                      << camera_position_[1] << ' ' << camera_position_[2] << " along view "
                      << -ahead << " count " << count << " scale "
                      << extent_scale << " radius " << first.radius << " size " << first.size
                      << " half " << first.radius * first.size * extent_scale * kRadiusToHalfExtent
                      << " colour " << first.colour[0] << ' ' << first.colour[1] << ' '
                      << first.colour[2] << ' ' << first.colour[3] << " matalpha " << surface.splat[3]
                      << " cutout " << surface.splat[2] << " state " << std::hex
                      << (system.draw_state & ~BGFX_STATE_CULL_MASK) << std::dec << " at " << one.x
                      << ' ' << one.y << ' ' << one.z << " uv " << one.u << ' ' << one.v << '\n';
        }
    }
    surface.shade = kParticleShade;
    surface.ambient = system.tint;
    // KNC PARTICLE FLAT 1 white opaque 2 sheet opaque 3 sheet blended no depth 4 sheet blended kept
    static const int flat = flat_particle_probe();
    uint64_t state = system.draw_state & ~BGFX_STATE_CULL_MASK;
    if (flat != 0) {
        surface.splat[2] = kNoAlphaCutout;
        surface.splat[3] = 1.f;
        surface.ambient = HourColour{1.f, 1.f, 1.f};
        if (flat < 3) state = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_ALWAYS;
        if (flat == 3) state = (state & ~BGFX_STATE_DEPTH_TEST_MASK) | BGFX_STATE_DEPTH_TEST_ALWAYS;
    }
    set_shader_parameters(surface, world_fog());
    set_uv_transform(kRawTextureCoordinates);
    bgfx::setTexture(0, diffuse_sampler_, flat == 1 ? textures_.white_stand_in() : system.texture,
                     system.sampler);
    bgfx::setTexture(1, coverage_sampler_, textures_.white_stand_in(), kCoverageSampler);
    // Quad sheet seen either side neither winding culled
    bgfx::setState(state);
    bgfx::setTransform(kIdentity);
    bgfx::setVertexBuffer(0, &vertices);
    bgfx::setIndexBuffer(&indices);
    bgfx::submit(target_view_, program_);
}

// Pool every placement after blended geometry particle additive end frame
void SceneRenderer::submit_particles() const { submit_particles_of(prop_instances_); }

void SceneRenderer::submit_particles_of(const std::vector<PropInstance>& instances) const {
    for (const PropInstance& instance : instances) {
        if (!visible(instance.layer) || instance.model_index >= prop_models_.size()) continue;
        const PropModelBuffers& model = prop_models_[instance.model_index];
        for (const ParticleBuffers& system : model.particles) {
            // A world space pool born through its one placement draws at the world origin
            const bool world_born = system.definition.world_space &&
                                    single_placement_world(prop_instances_, instance.model_index) != nullptr;
            submit_particle_system(system, model.pose, world_born ? nullptr : instance.world);
        }
    }
}

}
