#include "engine/render/scene_renderer.h"

#include "engine/render/scene_draw_state.h"
#include "engine/render/shaders/fs_scene.bin.h"
#include "engine/render/shaders/vs_scene.bin.h"
#include "engine/render/shaders/vs_skinned.bin.h"
#include "engine/render/view_order.h"

#include <bgfx/embedded_shader.h>
#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace KnC::Render {

namespace {

// The sky stands where it was authored so the camera can sit a whole radius off its centre
constexpr float kSkyFarPlaneHeadroom = 1000.0f;
// Silhouette shell looked inside cull front faces leave far side
constexpr uint64_t kOutlineState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z |
                                   BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CCW;
// Sky writes no depth CxSky LoadModel clears test and write
constexpr uint64_t kSkyState = BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_NORMAL;
// Viewer overlays and sky not client lit geometry hour never tints
constexpr HourColour kNoTint;
// Device holds while geometry inherits no light Renderer ApplyLights accumulates
constexpr HourColour kBlackAmbient{0.f, 0.f, 0.f};

const UvMatrix& texture_matrix(const ModelPose& pose, int channel) {
    if (channel < 0 || static_cast<size_t>(channel) >= pose.uv.size())
        return kRawTextureCoordinates;
    return pose.uv[static_cast<size_t>(channel)];
}

// A model space direction turned by a column major placement its scale dropped
void turn_by_world(const float world[16], const float direction[3], float out[4]) {
    for (int row = 0; row < 3; ++row)
        out[row] = direction[0] * world[row] + direction[1] * world[4 + row] + direction[2] * world[8 + row];
    const float length = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (length > 0.f)
        for (int axis = 0; axis < 3; ++axis) out[axis] /= length;
    out[3] = 0.f;
}

// Gamebryo sums every NiAmbientLight that reaches a geometry into the device ambient
void add_ambient(const ModelLights& lights, HourColour& ambient) {
    ambient.red += lights.ambient[0];
    ambient.green += lights.ambient[1];
    ambient.blue += lights.ambient[2];
}

// Channel no controller keeps alpha NiMaterialProperty 219 effect surfaces 0
float material_alpha(const ModelPose& pose, int channel, float rest) {
    if (channel < 0 || static_cast<size_t>(channel) >= pose.alpha.size()) return rest;
    return pose.alpha[static_cast<size_t>(channel)];
}

// Matrix animated node stands at or null 592 prop meshes never
const float* animated_node_matrix(const ModelPose& pose, int node) {
    if (node < 0 || static_cast<size_t>(node) * 16 >= pose.node_world.size()) return nullptr;
    return &pose.node_world[static_cast<size_t>(node) * 16];
}

// NiCamera SetViewFrustum clamps near plane to far divided by
constexpr float kMaxFarNearRatio = 10000.0f;
DepthRange depth_range(float far_plane) {
    DepthRange range;
    range.far_plane = far_plane;
    range.near_plane = std::max(kClientNearPlane, far_plane / kMaxFarNearRatio);
    return range;
}

// RenderState SetFogState ramp measured from far plane node range scale
FogRamp fog_ramp(const SceneFog& fog, DepthRange range) {
    FogRamp ramp;
    ramp.density = std::min(std::max(fog.max_density, 0.f), 1.f);
    ramp.radial = fog.radial ? 1.f : 0.f;
    // Four phase generation states distance Aura Kingdom FogSightRang 0 to 120
    if (fog.sight_range > 0.f) {
        ramp.inverse_span = 1.f / fog.sight_range;
        return ramp;
    }
    if (fog.depth <= 0.f || fog.range_scale <= 0.f) {
        ramp.inverse_span = 0.f;
        return ramp;
    }
    const float span = (range.far_plane - range.near_plane) * fog.depth;
    ramp.start = range.far_plane - span;
    ramp.inverse_span = fog.range_scale / span;
    return ramp;
}

// One line per pass ramps checked against worked examples
void report_fog_ramp(const std::string& pass, DepthRange range, const FogRamp& ramp) {
    if (ramp.inverse_span <= 0.f) return;
    std::cout << "[render] " << pass << " fog " << ramp.start << ".."
              << ramp.start + 1.0f / ramp.inverse_span << " over a " << range.near_plane << ".."
              << range.far_plane << " view"
              << (ramp.radial > 0.f ? ", measured radially\n" : "\n");
}

// Frame clears to fog colour sky four shapes below steep view
uint32_t packed_rgba(const HourColour& colour) {
    const auto channel = [](float value) {
        return static_cast<uint32_t>(std::lround(std::clamp(value, 0.f, 1.f) * 255.f));
    };
    return (channel(colour.red) << 24) | (channel(colour.green) << 16) |
           (channel(colour.blue) << 8) | 0xffu;
}

// One blob per backend packed by CMake shaderc bgfx picks
const bgfx::EmbeddedShader kSceneShaders[] = {BGFX_EMBEDDED_SHADER(vs_scene),
                                              BGFX_EMBEDDED_SHADER(vs_skinned),
                                              BGFX_EMBEDDED_SHADER(fs_scene),
                                              BGFX_EMBEDDED_SHADER_END()};

void destroy_vertex_buffer(bgfx::VertexBufferHandle& vertices) {
    if (bgfx::isValid(vertices)) bgfx::destroy(vertices);
    vertices = BGFX_INVALID_HANDLE;
}

void destroy_buffers(bgfx::VertexBufferHandle& vertices, bgfx::IndexBufferHandle& indices) {
    destroy_vertex_buffer(vertices);
    if (bgfx::isValid(indices)) bgfx::destroy(indices);
    indices = BGFX_INVALID_HANDLE;
}

float dot_product(const float left[3], const float right[3]) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

void cross_product(const float left[3], const float right[3], float out[3]) {
    out[0] = left[1] * right[2] - left[2] * right[1];
    out[1] = left[2] * right[0] - left[0] * right[2];
    out[2] = left[0] * right[1] - left[1] * right[0];
}

// False when the vector is too short to give a direction the caller keeps what it had
bool normalise(float vector[3]) {
    const float length = std::sqrt(dot_product(vector, vector));
    if (length < 1e-6f) return false;
    for (int axis = 0; axis < 3; ++axis) vector[axis] /= length;
    return true;
}

// Columns 0 1 2 of a placed matrix are the node axes each scaled by the world scale
void axis_scales(const float placed[16], float out[3]) {
    for (int column = 0; column < 3; ++column) {
        const float* axis = placed + column * 4;
        out[column] = std::sqrt(dot_product(axis, axis));
    }
}

void write_axes(const float right[3], const float up[3], const float face[3],
                const float scale[3], float placed[16]) {
    for (int axis = 0; axis < 3; ++axis) {
        placed[axis] = right[axis] * scale[0];
        placed[4 + axis] = up[axis] * scale[1];
        placed[8 + axis] = face[axis] * scale[2];
    }
}

// A NiBillboardNode plate lies in its own XY plane so the view writes X right Y up Z out
void apply_billboard(BillboardFacing facing, const float eye[3], const float camera_right[3],
                     const float camera_up[3], const float camera_forward[3], float placed[16]) {
    float scale[3];
    axis_scales(placed, scale);
    if (facing == BillboardFacing::RigidFaceCamera) {
        const float face[3] = {-camera_forward[0], -camera_forward[1], -camera_forward[2]};
        write_axes(camera_right, camera_up, face, scale, placed);
        return;
    }
    float face[3] = {eye[0] - placed[12], eye[1] - placed[13], eye[2] - placed[14]};
    if (facing == BillboardFacing::RotateAboutUp) {
        if (scale[1] < 1e-6f) return;
        float up[3] = {placed[4] / scale[1], placed[5] / scale[1], placed[6] / scale[1]};
        if (!normalise(up)) return;
        const float along = dot_product(up, face);
        for (int axis = 0; axis < 3; ++axis) face[axis] -= up[axis] * along;
        if (!normalise(face)) return;
        float right[3];
        cross_product(up, face, right);
        write_axes(right, up, face, scale, placed);
        return;
    }
    if (!normalise(face)) return;
    float right[3];
    cross_product(camera_up, face, right);
    if (!normalise(right)) {
        cross_product(camera_forward, face, right);
        if (!normalise(right)) return;
    }
    float up[3];
    cross_product(face, right, up);
    write_axes(right, up, face, scale, placed);
}

void destroy_uniform(bgfx::UniformHandle& uniform) {
    if (bgfx::isValid(uniform)) bgfx::destroy(uniform);
    uniform = BGFX_INVALID_HANDLE;
}

} // namespace

SceneRenderer::~SceneRenderer() { shutdown(); }

bool SceneRenderer::create_uniforms() {
    diffuse_sampler_ = bgfx::createUniform("s_diffuse", bgfx::UniformType::Sampler);
    coverage_sampler_ = bgfx::createUniform("s_coverage", bgfx::UniformType::Sampler);
    splat_params_ = bgfx::createUniform("u_splatParams", bgfx::UniformType::Vec4);
    shade_emissive_ = bgfx::createUniform("u_shadeEmissive", bgfx::UniformType::Vec4);
    shade_ambient_ = bgfx::createUniform("u_shadeAmbient", bgfx::UniformType::Vec4);
    shade_alpha_ = bgfx::createUniform("u_shadeAlpha", bgfx::UniformType::Vec4);
    shade_diffuse_ = bgfx::createUniform("u_shadeDiffuse", bgfx::UniformType::Vec4);
    sun_direction_ = bgfx::createUniform("u_sunDirection", bgfx::UniformType::Vec4);
    sun_colour_ = bgfx::createUniform("u_sunColour", bgfx::UniformType::Vec4);
    ambient_colour_ = bgfx::createUniform("u_ambientColour", bgfx::UniformType::Vec4);
    fog_colour_uniform_ = bgfx::createUniform("u_fogColour", bgfx::UniformType::Vec4);
    fog_range_uniform_ = bgfx::createUniform("u_fogRange", bgfx::UniformType::Vec4);
    uv_rows_uniform_ = bgfx::createUniform("u_uvRows", bgfx::UniformType::Vec4);
    uv_offset_uniform_ = bgfx::createUniform("u_uvOffset", bgfx::UniformType::Vec4);
    detail_sampler_ = bgfx::createUniform("s_detail", bgfx::UniformType::Sampler);
    detail_rows_uniform_ = bgfx::createUniform("u_detailRows", bgfx::UniformType::Vec4);
    detail_offset_uniform_ = bgfx::createUniform("u_detailOffset", bgfx::UniformType::Vec4);
    detail_params_uniform_ = bgfx::createUniform("u_detailParams", bgfx::UniformType::Vec4);
    bone_rows_uniform_ = bgfx::createUniform("u_bones", bgfx::UniformType::Vec4,
                                             kBonePaletteSize * kBoneMatrixRows);
    environment_sampler_ = bgfx::createUniform("s_environment", bgfx::UniformType::Sampler);
    environment_row_u_ = bgfx::createUniform("u_environmentRowU", bgfx::UniformType::Vec4);
    environment_row_v_ = bgfx::createUniform("u_environmentRowV", bgfx::UniformType::Vec4);
    eye_position_ = bgfx::createUniform("u_eyePosition", bgfx::UniformType::Vec4);
    model_light_direction_ = bgfx::createUniform("u_modelLightDirection", bgfx::UniformType::Vec4, kModelLights);
    model_light_colour_ = bgfx::createUniform("u_modelLightColour", bgfx::UniformType::Vec4, kModelLights);
    if (bgfx::isValid(bone_rows_uniform_) && bgfx::isValid(diffuse_sampler_) &&
        bgfx::isValid(coverage_sampler_) &&
        bgfx::isValid(splat_params_) && bgfx::isValid(shade_emissive_) &&
        bgfx::isValid(shade_ambient_) && bgfx::isValid(shade_alpha_) &&
        bgfx::isValid(ambient_colour_) && bgfx::isValid(fog_colour_uniform_) &&
        bgfx::isValid(fog_range_uniform_) && bgfx::isValid(uv_rows_uniform_) &&
        bgfx::isValid(uv_offset_uniform_) && bgfx::isValid(detail_sampler_) &&
        bgfx::isValid(detail_rows_uniform_) && bgfx::isValid(detail_offset_uniform_) &&
        bgfx::isValid(detail_params_uniform_) && bgfx::isValid(environment_sampler_) &&
        bgfx::isValid(environment_row_u_) && bgfx::isValid(environment_row_v_) &&
        bgfx::isValid(eye_position_) && bgfx::isValid(model_light_direction_) &&
        bgfx::isValid(model_light_colour_))
        return true;
    std::cerr << "[render] scene uniforms could not be created\n";
    return false;
}

bool SceneRenderer::create_program() {
    layout_.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    const bgfx::RendererType::Enum backend = bgfx::getRendererType();
    program_ = bgfx::createProgram(bgfx::createEmbeddedShader(kSceneShaders, backend, "vs_scene"),
                                   bgfx::createEmbeddedShader(kSceneShaders, backend, "fs_scene"),
                                   true);
    if (bgfx::isValid(program_)) return true;
    std::cerr << "[render] the scene shaders failed to link for "
              << bgfx::getRendererName(backend) << "\n";
    return false;
}

// Skinned layout scene one plus palette slots move vertex normalised
bool SceneRenderer::create_skinned_program() {
    skinned_layout_.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
        .end();
    if (skinned_layout_.getStride() != sizeof(SkinnedVertex)) {
        std::cerr << "[render] the skinned vertex layout is " << skinned_layout_.getStride()
                  << " bytes against a " << sizeof(SkinnedVertex) << "-byte vertex\n";
        return false;
    }
    const bgfx::RendererType::Enum backend = bgfx::getRendererType();
    skinned_program_ =
        bgfx::createProgram(bgfx::createEmbeddedShader(kSceneShaders, backend, "vs_skinned"),
                            bgfx::createEmbeddedShader(kSceneShaders, backend, "fs_scene"), true);
    if (bgfx::isValid(skinned_program_)) return true;
    std::cerr << "[render] the skinned shaders failed to link for "
              << bgfx::getRendererName(backend) << "\n";
    return false;
}

bool SceneRenderer::init(const RendererSetup& setup) {
    if (setup.far_plane <= kClientNearPlane) {
        std::cerr << "[render] the far plane must be beyond the near plane of "
                  << kClientNearPlane << "\n";
        return false;
    }
    world_range_ = depth_range(setup.far_plane);
    start_hour_ = setup.hour;
    animation_.set_seconds(setup.animation_seconds);
    reset_flags_ = setup.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
    bgfx::Init parameters;
    // Count lets bgfx score backends this machine has
    parameters.type = bgfx::RendererType::Count;
    parameters.resolution.width = setup.width;
    parameters.resolution.height = setup.height;
    parameters.resolution.reset = reset_flags_;
    parameters.platformData.nwh = setup.native_window;
    parameters.callback = setup.callback;
    if (!bgfx::init(parameters)) {
        std::cerr << "[render] bgfx::init found no usable backend\n";
        return false;
    }
    started_ = true;
    std::cout << "[render] backend " << bgfx::getRendererName(bgfx::getRendererType())
              << "\n";
    width_ = setup.width;
    height_ = setup.height;
    viewport_ = {0, 0, width_, height_};
    if (!create_program() || !create_skinned_program() || !create_uniforms()) return false;
    if (!toon_.create() || !glow_.create(width_, height_)) return false;
    if (!textures_.create_white_stand_in()) return false;
    layer_visible_.fill(true);
    // Orange hulls inside meshes show only when key 5 asks
    layer_visible_[static_cast<int>(SceneLayer::PropHulls)] = false;
    bgfx::setViewMode(kSkyView, bgfx::ViewMode::Sequential);
    bgfx::setViewMode(kSceneView, bgfx::ViewMode::Sequential);
    // The hud scene views come after the overlay id and draw before it the table is rank to view
    bgfx::ViewId order[kHudSceneFirstView + kHudSceneViews];
    for (int view = 0; view < kOverlayView; ++view) order[view] = static_cast<bgfx::ViewId>(view);
    for (int slot = 0; slot < kHudSceneViews; ++slot)
        order[kOverlayView + slot] = static_cast<bgfx::ViewId>(kHudSceneFirstView + slot);
    order[kOverlayView + kHudSceneViews] = static_cast<bgfx::ViewId>(kOverlayView);
    bgfx::setViewOrder(0, kHudSceneFirstView + kHudSceneViews, order);
    target_view_ = kSceneView;
    return true;
}

// Second mesh copy difference vertex colour two shadings swap draw
bgfx::VertexBufferHandle SceneRenderer::create_baked_buffer(const LayerMesh& mesh) {
    if (mesh.baked_abgr.empty()) return BGFX_INVALID_HANDLE;
    if (mesh.baked_abgr.size() != mesh.vertices.size()) {
        std::cerr << "[render] baked colours do not match the mesh they belong to\n";
        return BGFX_INVALID_HANDLE;
    }
    std::vector<SceneVertex> lit = mesh.vertices;
    for (size_t index = 0; index < lit.size(); ++index) lit[index].abgr = mesh.baked_abgr[index];
    return bgfx::createVertexBuffer(
        bgfx::copy(lit.data(), static_cast<uint32_t>(lit.size() * sizeof(SceneVertex))), layout_);
}

// Coat unreadable mask dropped white stand in would blanket tile
void SceneRenderer::resolve_coats(const LayerMesh& mesh, LayerBuffers& buffers) {
    for (const SplatCoat& coat : mesh.coats) {
        CoatTextures resolved;
        resolved.repeat[0] = coat.repeat[0];
        resolved.repeat[1] = coat.repeat[1];
        resolved.offset[0] = coat.offset[0];
        resolved.offset[1] = coat.offset[1];
        resolved.diffuse = textures_.acquire(coat.diffuse_path).handle;
        if (!bgfx::isValid(resolved.diffuse)) resolved.diffuse = textures_.white_stand_in();
        if (!coat.mask_alpha.empty())
            resolved.coverage = textures_
                                    .acquire_alpha(coat.mask_path, coat.mask_width,
                                                   coat.mask_height, coat.mask_alpha)
                                    .handle;
        else
            resolved.coverage = coat.mask_path.empty() ? textures_.white_stand_in()
                                                       : textures_.acquire(coat.mask_path).handle;
        if (!bgfx::isValid(resolved.coverage)) continue;
        buffers.coats.push_back(resolved);
    }
    if (!mesh.coats.empty()) return;
    buffers.coats.push_back({textures_.white_stand_in(), textures_.white_stand_in()});
}

// Prop names texture client not ship draws in white
SceneRenderer::PropPartBuffers SceneRenderer::create_part(const PropPart& part) {
    PropPartBuffers buffers;
    buffers.draw_state = surface_draw_state(part.surface);
    // A stencil draw mode of both sides keeps every face the podium WIN sign is one
    if (part.surface.two_sided) buffers.draw_state &= ~BGFX_STATE_CULL_MASK;
    buffers.alpha_cutout = surface_alpha_cutout(part.surface);
    buffers.rest_alpha = part.surface.material.alpha;
    buffers.shade = surface_shade_state(part.surface, !part.texture_path.empty(),
                                        part.has_vertex_colours);
    buffers.blended = part.surface.alpha.blend_enabled();
    buffers.sortable = buffers.blended && !part.surface.alpha.no_sorting();
    buffers.bound = part.bound;
    buffers.vertices = bgfx::createVertexBuffer(
        bgfx::copy(part.vertices.data(),
                   static_cast<uint32_t>(part.vertices.size() * sizeof(SceneVertex))),
        layout_);
    buffers.indices = bgfx::createIndexBuffer(
        bgfx::copy(part.indices.data(),
                   static_cast<uint32_t>(part.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);
    const CachedTexture image = textures_.acquire(
        part.texture_path, filters_mipmaps(part.surface.base_texture_filter));
    buffers.texture = bgfx::isValid(image.handle) ? image.handle : textures_.white_stand_in();
    buffers.sampler = diffuse_sampler(part.surface.base_texture_clamp,
                                      part.surface.base_texture_filter);
    if (!part.detail_texture_path.empty()) {
        const CachedTexture detail = textures_.acquire(part.detail_texture_path, true);
        if (bgfx::isValid(detail.handle)) buffers.detail = detail.handle;
    }
    take_environment(part.environment, buffers.environment, buffers.environment_rotation);
    buffers.animation = part.animation;
    if (part.morph_frames.empty()) return buffers;
    buffers.morph_posed = part.vertices;
    buffers.morph_frames = part.morph_frames;
    buffers.morph_first_vertex = part.morph_first_vertex;
    buffers.morph_relative = part.morph_relative;
    buffers.morphed = bgfx::createDynamicVertexBuffer(
        static_cast<uint32_t>(part.vertices.size()), layout_);
    return buffers;
}

// Gamebryo poses a morphed shape as frame 0 plus every other frame times its own weight
void SceneRenderer::refresh_morphed_parts(PropModelBuffers& model) {
    for (PropPartBuffers& part : model.parts) {
        if (!bgfx::isValid(part.morphed) || part.morph_frames.empty()) continue;
        const int channel = part.animation.morph;
        if (channel < 0 || static_cast<std::size_t>(channel) >= model.pose.morph_weights.size())
            continue;
        const std::vector<float>& weights = model.pose.morph_weights[channel];
        if (weights == part.morph_weights) continue;
        part.morph_weights = weights;
        const std::size_t count = part.morph_frames[0].size() / 3;
        const std::size_t first = part.morph_relative ? 1u : 0u;
        for (std::size_t vertex = 0; vertex < count; ++vertex) {
            float point[3] = {0.f, 0.f, 0.f};
            if (part.morph_relative)
                for (int axis = 0; axis < 3; ++axis)
                    point[axis] = part.morph_frames[0][vertex * 3 + axis];
            for (std::size_t frame = first; frame < part.morph_frames.size(); ++frame) {
                const float weight = frame < weights.size() ? weights[frame] : 0.f;
                if (weight == 0.f) continue;
                const std::vector<float>& offsets = part.morph_frames[frame];
                if (offsets.size() < (vertex + 1) * 3) continue;
                for (int axis = 0; axis < 3; ++axis)
                    point[axis] += weight * offsets[vertex * 3 + axis];
            }
            SceneVertex& posed = part.morph_posed[part.morph_first_vertex + vertex];
            posed.x = point[0];
            posed.y = point[1];
            posed.z = point[2];
        }
        bgfx::update(part.morphed, 0,
                     bgfx::copy(part.morph_posed.data(),
                                static_cast<uint32_t>(part.morph_posed.size() *
                                                      sizeof(SceneVertex))));
    }
}

void SceneRenderer::destroy_part(PropPartBuffers& part) {
    destroy_buffers(part.vertices, part.indices);
    if (bgfx::isValid(part.morphed)) bgfx::destroy(part.morphed);
    part.morphed = BGFX_INVALID_HANDLE;
}

SceneRenderer::PropModelBuffers SceneRenderer::create_model(const PropModel& model) {
    PropModelBuffers buffers;
    buffers.parts.reserve(model.parts.size());
    for (const PropPart& part : model.parts) {
        buffers.parts.push_back(create_part(part));
        buffers.parts.back().lit_by_map_ambient = model.lit_by_map_ambient;
        buffers.parts.back().lights = model.lights;
        // The flip files of the part uploaded once the pose names the frame
        const int flip = part.animation.flip;
        if (flip < 0 || static_cast<size_t>(flip) >= model.animation.flip_channels.size()) continue;
        const bool mipped = filters_mipmaps(part.surface.base_texture_filter);
        for (const std::string& path : model.animation.flip_channels[static_cast<size_t>(flip)].textures) {
            const CachedTexture image = textures_.acquire(path, mipped);
            buffers.parts.back().flip_textures.push_back(
                bgfx::isValid(image.handle) ? image.handle : buffers.parts.back().texture);
        }
    }
    buffers.particles.reserve(model.particle_systems.size());
    for (const ParticleSystemDefinition& system : model.particle_systems)
        buffers.particles.push_back(create_particle_system(system));
    buffers.animation = model.animation;
    buffers.named_nodes = model.named_nodes;
    // Pose sized before first draw not on first frame
    evaluate_model_animation(buffers.animation, animation_.seconds(), buffers.pose);
    refresh_morphed_parts(buffers);
    return buffers;
}

void SceneRenderer::upload_prop_models(const MapScene& scene) {
    prop_models_.reserve(scene.prop_models.size());
    for (const PropModel& model : scene.prop_models)
        prop_models_.push_back(create_model(model));
    prop_instances_ = scene.prop_instances;
    scene_prop_instances_ = prop_instances_.size();
    report_particle_systems();
}

std::size_t SceneRenderer::append_prop_model(const PropModel& model) {
    prop_models_.push_back(create_model(model));
    prop_models_.back().particle_epoch = animation_.seconds();
    return prop_models_.size() - 1;
}

void SceneRenderer::set_appended_prop_instances(const std::vector<PropInstance>& instances) {
    prop_instances_.resize(scene_prop_instances_);
    prop_instances_.insert(prop_instances_.end(), instances.begin(), instances.end());
}

void SceneRenderer::pose_prop_model_at(std::size_t model_index, float played) {
    if (model_index >= prop_models_.size()) return;
    PropModelBuffers& model = prop_models_[model_index];
    model.particle_epoch = animation_.seconds() - std::max(played, 0.f);
    if (model.animation.empty()) return;
    evaluate_model_animation(model.animation, std::max(played, 0.f), model.pose);
    refresh_morphed_parts(model);
}

bool SceneRenderer::prop_node_matrix(std::size_t model_index, const std::string& name,
                                     float out[16]) const {
    if (model_index >= prop_models_.size()) return false;
    const PropModelBuffers& model = prop_models_[model_index];
    for (const NamedNode& node : model.named_nodes) {
        if (node.name != name) continue;
        const std::size_t slot = static_cast<std::size_t>(node.animated_node) * 16;
        if (node.animated_node < 0 || slot + 16 > model.pose.node_world.size()) {
            for (int i = 0; i < 16; ++i) out[i] = node.rest[i];
            return true;
        }
        multiply_matrix(&model.pose.node_world[slot], node.rest, out);
        return true;
    }
    return false;
}

void SceneRenderer::restart_prop_particles(std::size_t model_index, float seconds) {
    if (model_index >= prop_models_.size()) return;
    prop_models_[model_index].particle_epoch = seconds;
}

SceneRenderer::SkinnedPartBuffers SceneRenderer::create_skinned_part(const SkinnedPart& part,
                                                                    std::size_t slots) {
    SkinnedPartBuffers buffers;
    buffers.vertices = bgfx::createVertexBuffer(
        bgfx::copy(part.vertices.data(),
                   static_cast<uint32_t>(part.vertices.size() * sizeof(SkinnedVertex))),
        skinned_layout_);
    buffers.indices = bgfx::createIndexBuffer(
        bgfx::copy(part.indices.data(),
                   static_cast<uint32_t>(part.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);
    const CachedTexture image = textures_.acquire(
        part.texture_path, filters_mipmaps(part.surface.base_texture_filter));
    buffers.texture = bgfx::isValid(image.handle) ? image.handle : textures_.white_stand_in();
    buffers.sampler = diffuse_sampler(part.surface.base_texture_clamp,
                                      part.surface.base_texture_filter);
    buffers.draw_state = surface_draw_state(part.surface);
    buffers.alpha_cutout = surface_alpha_cutout(part.surface);
    buffers.rest_alpha = part.surface.material.alpha;
    buffers.shade = surface_shade_state(part.surface, !part.texture_path.empty(),
                                        part.has_vertex_colours);
    buffers.blended = part.surface.alpha.blend_enabled();
    buffers.palette_slots = static_cast<uint16_t>(slots);
    take_environment(part.environment, buffers.environment, buffers.environment_rotation);
    return buffers;
}

// Rig outlives geometry vertices on GPU kept only what poses
void SceneRenderer::upload_one_character(const CharacterModel& model) {
    CharacterBuffers buffers;
    buffers.rig = model.rig;
    for (int axis = 0; axis < 3; ++axis) {
        buffers.bound_centre[axis] = 0.5f * (model.bounds_min[axis] + model.bounds_max[axis]);
        const float reach = 0.5f * (model.bounds_max[axis] - model.bounds_min[axis]);
        buffers.bound_radius += reach * reach;
    }
    buffers.bound_radius = std::sqrt(buffers.bound_radius);
    buffers.parts.reserve(model.parts.size());
    for (std::size_t part = 0; part < model.parts.size(); ++part) {
        buffers.parts.push_back(
            create_skinned_part(model.parts[part], model.rig.palettes[part].nodes.size()));
        buffers.parts.back().lights = model.lights;
    }
    evaluate_skinned_pose(buffers.rig, -1, animation_.seconds(), buffers.pose);
    characters_.push_back(std::move(buffers));
}

void SceneRenderer::append_character_model(const CharacterModel& model) {
    upload_one_character(model);
}

void SceneRenderer::upload_characters(const MapScene& scene) {
    characters_.reserve(scene.character_models.size());
    for (const CharacterModel& model : scene.character_models) upload_one_character(model);
    character_instances_ = scene.character_instances;
    // Scene one character is model document transport opens on clip
    if (character_instances_.size() == 1) character_clip_ = character_instances_.front().clip;
}

// Phase bound sets far plane two skies one tile no share
void SceneRenderer::upload_sky_phases(const MapScene& scene) {
    sky_phases_.reserve(scene.sky_phases.size());
    for (const SkyPhase& phase : scene.sky_phases) {
        SkyPhaseBuffers buffers;
        buffers.name = phase.name;
        buffers.model = create_model(phase.model);
        buffers.range = phase_range(phase);
        // One ramp sky pass hour viewer opened at sky phase outlives
        const SceneFog& opening = scene.fog_of_hour[static_cast<size_t>(start_hour_)];
        if (opening.sight_range <= 0.f) buffers.fog = fog_ramp(opening, buffers.range);
        report_fog_ramp(phase.name, buffers.range, buffers.fog);
        sky_phases_.push_back(std::move(buffers));
    }
}

void SceneRenderer::configure_fog(const std::array<SceneFog, kHoursPerDay>& fog_of_hour) {
    const SceneFog& opening = fog_of_hour[static_cast<size_t>(start_hour_)];
    if (opening.sight_range <= 0.f && opening.depth <= 0.f) {
        std::cout << "[render] the tile's node asks for no fog\n";
        return;
    }
    // SetFogRangeScale clamps scale 0 to 1e-5 54 of 203 rows ship
    if (opening.sight_range <= 0.f && opening.range_scale <= 0.f) {
        std::cout << "[render] the tile's node asks for fog range scale "
                  << opening.range_scale << ", which is how the client switches fog off\n";
        return;
    }
    // Against client range not editor shipped depth range scale fractions
    const DepthRange client_range = depth_range(kClientFarPlane);
    for (int hour = 0; hour < kHoursPerDay; ++hour)
        world_fog_of_hour_[static_cast<size_t>(hour)] =
            fog_ramp(fog_of_hour[static_cast<size_t>(hour)], client_range);
    node_fogged_ = true;
    // Hour viewer opened at not clock reset after
    report_fog_ramp("world", client_range, world_fog_of_hour_[static_cast<size_t>(start_hour_)]);
}

const FogRamp& SceneRenderer::world_fog() const {
    return world_fog_of_hour_[static_cast<size_t>(cycle_.hour())];
}

// Frustum only fog ramp fixed world units sky phases own far
void SceneRenderer::set_far_plane(float far_plane) {
    if (far_plane <= kClientNearPlane) {
        std::cerr << "[render] a far plane of " << far_plane << " is not beyond the near plane"
                  << " of " << kClientNearPlane << "\n";
        return;
    }
    world_range_ = depth_range(far_plane);
}

void SceneRenderer::release_scene() {
    for (LayerBuffers& buffers : layers_) {
        destroy_buffers(buffers.vertices, buffers.indices);
        destroy_vertex_buffer(buffers.baked_vertices);
        buffers.coats.clear();
    }
    for (GroundPatchBuffers& patch : ground_patches_) {
        destroy_buffers(patch.buffers.vertices, patch.buffers.indices);
        destroy_vertex_buffer(patch.buffers.baked_vertices);
    }
    ground_patches_.clear();
    for (PropModelBuffers& model : prop_models_)
        for (PropPartBuffers& part : model.parts) destroy_part(part);
    prop_models_.clear();
    prop_instances_.clear();
    scene_prop_instances_ = 0;
    for (CharacterBuffers& character : characters_)
        for (SkinnedPartBuffers& part : character.parts)
            destroy_buffers(part.vertices, part.indices);
    characters_.clear();
    character_instances_.clear();
    instance_poses_.clear();
    character_clip_ = -1;
    for (SkyPhaseBuffers& phase : sky_phases_)
        for (PropPartBuffers& part : phase.model.parts) destroy_part(part);
    sky_phases_.clear();
    sky_phase_ = -1;
    node_fogged_ = false;
    world_fog_of_hour_.fill(FogRamp{});
}

// Replaces layer overlay edit rebuilt sent again without rest scene
void SceneRenderer::upload_layer(SceneLayer layer, const LayerMesh& mesh) {
    fill_layer_buffers(mesh, layers_[static_cast<int>(layer)]);
}

// One ground patch per mesh each carries coat stack
void SceneRenderer::upload_ground_patches(const MapScene& scene) {
    ground_patches_.resize(scene.ground_patches.size());
    for (std::size_t index = 0; index < scene.ground_patches.size(); ++index) {
        ground_patches_[index].layer = scene.ground_patches[index].layer;
        fill_layer_buffers(scene.ground_patches[index].mesh, ground_patches_[index].buffers);
    }
}

void SceneRenderer::fill_layer_buffers(const LayerMesh& mesh, LayerBuffers& buffers) {
    destroy_buffers(buffers.vertices, buffers.indices);
    destroy_vertex_buffer(buffers.baked_vertices);
    buffers.coats.clear();
    buffers.translucent = mesh.translucent;
    if (mesh.indices.empty()) return;
    buffers.vertices = bgfx::createVertexBuffer(
        bgfx::copy(mesh.vertices.data(),
                   static_cast<uint32_t>(mesh.vertices.size() * sizeof(SceneVertex))),
        layout_);
    buffers.indices = bgfx::createIndexBuffer(
        bgfx::copy(mesh.indices.data(),
                   static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);
    buffers.baked_vertices = create_baked_buffer(mesh);
    resolve_coats(mesh, buffers);
}

void SceneRenderer::update_character_instances(const std::vector<CharacterInstance>& instances) {
    character_instances_ = instances;
}

void SceneRenderer::upload(const MapScene& scene) {
    release_scene();
    for (int index = 0; index < kSceneLayerCount; ++index)
        upload_layer(static_cast<SceneLayer>(index), scene.layers[index]);
    upload_ground_patches(scene);
    upload_prop_models(scene);
    upload_characters(scene);
    configure_fog(scene.fog_of_hour);
    upload_sky_phases(scene);
    sky_phase_of_hour_ = scene.sky_phase_of_hour;
    // Zone in takes hour no cross fade CxDayNight ForceHourSnap
    cycle_.reset(scene.day_night, start_hour_);
    sun_ = scene.sun;
    std::cout << "[render] textures missing: " << textures_.missing_count() << "\n";
}

void SceneRenderer::resize(uint16_t width, uint16_t height) {
    if (width == 0 || height == 0) return;
    width_ = width;
    height_ = height;
    bgfx::reset(width_, height_, reset_flags_);
    set_viewport({0, 0, width_, height_});
}

void SceneRenderer::set_viewport(const ViewportRect& viewport) {
    if (viewport.width == 0 || viewport.height == 0) return;
    viewport_ = viewport;
    glow_.set_output_origin(viewport_.x, viewport_.y);
    // Glow chain off screen frame not match viewport stretch world
    if (!glow_.resize(viewport_.width, viewport_.height)) glow_.destroy();
}

bool SceneRenderer::glowing() const { return glow_enabled_ && glow_.ready(); }

// Game sky off viewport tool view geometry neutral ground not hour
const HourColour& SceneRenderer::backdrop() const {
    if (visible(SceneLayer::Sky)) return cycle_.fog();
    return kDefaultBackdrop;
}

void SceneRenderer::toggle(SceneLayer layer) {
    set_visible(layer, !visible(layer));
}

void SceneRenderer::set_visible(SceneLayer layer, bool shown) {
    layer_visible_[static_cast<int>(layer)] = shown;
}

bool SceneRenderer::visible(SceneLayer layer) const {
    return layer_visible_[static_cast<int>(layer)];
}

bool SceneRenderer::layer_populated(SceneLayer layer) const {
    if (layer == SceneLayer::Sky) return !sky_phases_.empty();
    if (layer == SceneLayer::Monsters && !character_instances_.empty()) return true;
    if (bgfx::isValid(layers_[static_cast<int>(layer)].vertices)) return true;
    for (const GroundPatchBuffers& patch : ground_patches_)
        if (patch.layer == layer) return true;
    for (const PropInstance& instance : prop_instances_)
        if (instance.layer == layer) return true;
    return false;
}

void SceneRenderer::toggle_fog() { set_fog(!fog_enabled_); }

void SceneRenderer::set_fog(bool fogged) { fog_enabled_ = fogged; }

void SceneRenderer::toggle_baked_shading() { set_baked_shading(!baked_shading_); }

void SceneRenderer::set_baked_shading(bool baked) { baked_shading_ = baked; }

void SceneRenderer::toggle_cel_shading() { set_cel_shading(!cel_shading_); }

void SceneRenderer::set_cel_shading(bool celled) { cel_shading_ = celled; }

void SceneRenderer::toggle_glow() { set_glow(!glow_enabled_); }

void SceneRenderer::set_glow(bool glowing) { glow_enabled_ = glowing; }

// The stock picks the night nif from the launch flag of 0x0014 not from an hour
size_t SceneRenderer::sky_phase_index() const {
    if (sky_phase_ >= 0) return static_cast<size_t>(sky_phase_);
    return sky_phase_of_hour_[static_cast<size_t>(cycle_.hour())];
}

void SceneRenderer::set_sky_phase(int phase) { sky_phase_ = phase; }

size_t SceneRenderer::sky_phase_count() const { return sky_phases_.size(); }

std::string SceneRenderer::sky_name() const {
    const size_t phase = sky_phase_index();
    return phase < sky_phases_.size() ? sky_phases_[phase].name : std::string();
}

// Sky no authored bound no far plane draws world says clipped
DepthRange SceneRenderer::phase_range(const SkyPhase& phase) const {
    if (phase.model.bound.radius > 0.f)
        return depth_range(phase.model.bound.radius * 2.f + kSkyFarPlaneHeadroom);
    std::cerr << "[render] sky " << phase.name << " carries no bounding sphere\n";
    return world_range_;
}

// Tile node named no sky needs frustum for view clears
DepthRange SceneRenderer::sky_range() const {
    const size_t phase = sky_phase_index();
    return phase < sky_phases_.size() ? sky_phases_[phase].range : world_range_;
}

// Terrain shader multiplies hour ambient into baked vertex colour prop
const HourColour& SceneRenderer::ambient_of(SceneLayer layer) const {
    const bool client_geometry = layer == SceneLayer::Terrain || layer == SceneLayer::Water;
    return client_geometry ? cycle_.ambient() : kNoTint;
}

void SceneRenderer::set_uv_transform(const UvMatrix& matrix) const {
    bgfx::setUniform(uv_rows_uniform_, matrix.rows);
    bgfx::setUniform(uv_offset_uniform_, matrix.offset);
    // Every draw resets the detail stage a prop part with one turns it on after
    const float off[4] = {0.f, 0.f, 0.f, 0.f};
    bgfx::setUniform(detail_params_uniform_, off);
}

// D3D9 fixed function stage two of a detail map is MODULATE2X the shader doubles the product
void SceneRenderer::set_detail_stage(const PropPartBuffers* part, const ModelPose* pose) const {
    if (part == nullptr || !bgfx::isValid(part->detail)) return;
    const UvMatrix& matrix = texture_matrix(*pose, part->animation.detail_uv);
    bgfx::setUniform(detail_rows_uniform_, matrix.rows);
    bgfx::setUniform(detail_offset_uniform_, matrix.offset);
    const float on[4] = {1.f, 0.f, 0.f, 0.f};
    bgfx::setUniform(detail_params_uniform_, on);
    bgfx::setTexture(2, detail_sampler_, part->detail, part->sampler);
}

// One sample every model moves taken before frame draws
void SceneRenderer::refresh_poses() {
    const float seconds = animation_.seconds();
    for (PropModelBuffers& model : prop_models_)
        if (!model.animation.empty()) {
            evaluate_model_animation(model.animation,
                                     std::max(0.f, seconds - model.particle_epoch),
                                     model.pose);
            refresh_morphed_parts(model);
        }
    for (SkyPhaseBuffers& phase : sky_phases_)
        if (!phase.model.animation.empty())
            evaluate_model_animation(phase.model.animation, seconds, phase.model.pose);
    refresh_particles();
    refresh_character_poses();
}

// Character posed once per model spawns place one skeleton walk
void SceneRenderer::refresh_character_poses() {
    if (characters_.empty()) return;
    std::vector<int> clip_of(characters_.size(), character_clip_);
    if (character_clip_ < 0)
        for (const CharacterInstance& instance : character_instances_)
            if (instance.model_index < clip_of.size())
                clip_of[instance.model_index] = instance.clip;
    for (std::size_t model = 0; model < characters_.size(); ++model)
        evaluate_skinned_pose(characters_[model].rig, clip_of[model], animation_.seconds(),
                              characters_[model].pose);
    refresh_instance_poses();
}

void SceneRenderer::refresh_instance_poses() {
    instance_poses_.resize(character_instances_.size());
    for (std::size_t index = 0; index < character_instances_.size(); ++index) {
        const CharacterInstance& instance = character_instances_[index];
        if (instance.clip_seconds < 0.f || instance.model_index >= characters_.size()) continue;
        // Transport names one clip speaks every body scene model document
        const int clip = character_clip_ < 0 ? instance.clip : character_clip_;
        evaluate_skinned_pose(characters_[instance.model_index].rig, clip,
                              instance.clip_seconds, instance_poses_[index]);
    }
}

const SkinnedPose& SceneRenderer::pose_of_instance(std::size_t index) const {
    const CharacterInstance& instance = character_instances_[index];
    if (instance.clip_seconds >= 0.f && index < instance_poses_.size())
        return instance_poses_[index];
    return characters_[instance.model_index].pose;
}

void SceneRenderer::set_character_clip(int clip) { character_clip_ = clip; }

const std::vector<CharacterClip>& SceneRenderer::character_clips() const {
    static const std::vector<CharacterClip> kNone;
    if (character_instances_.size() != 1) return kNone;
    const std::size_t model = character_instances_.front().model_index;
    return model < characters_.size() ? characters_[model].rig.clips : kNone;
}

// bgfx hands uniform to submit every draw sets them
void SceneRenderer::set_shader_parameters(const SurfaceUniforms& surface,
                                          const FogRamp& ramp) const {
    bgfx::setUniform(splat_params_, surface.splat);
    bgfx::setUniform(shade_emissive_, surface.shade.emissive);
    bgfx::setUniform(shade_ambient_, surface.shade.ambient);
    bgfx::setUniform(shade_alpha_, surface.shade.alpha);
    bgfx::setUniform(shade_diffuse_, surface.shade.diffuse);
    const float ambient_uniform[4] = {surface.ambient.red, surface.ambient.green,
                                      surface.ambient.blue, 0.f};
    bgfx::setUniform(ambient_colour_, ambient_uniform);
    const float sun_direction[4] = {sun_.direction[0], sun_.direction[1], sun_.direction[2],
                                    sun_.enabled && !hud_pass_ ? 1.f : 0.f};
    bgfx::setUniform(sun_direction_, sun_direction);
    const float sun_colour[4] = {sun_.colour.red, sun_.colour.green, sun_.colour.blue, 0.f};
    bgfx::setUniform(sun_colour_, sun_colour);
    const HourColour& fog_colour = cycle_.fog();
    const float colour[4] = {fog_colour.red, fog_colour.green, fog_colour.blue,
                             fog_enabled_ && node_fogged_ && !hud_pass_ ? ramp.density : 0.f};
    bgfx::setUniform(fog_colour_uniform_, colour);
    bgfx::setUniform(fog_range_uniform_, &ramp);
    set_model_stage(nullptr, BGFX_INVALID_HANDLE, nullptr, nullptr);
}

void SceneRenderer::take_environment(const EnvironmentMap& map, bgfx::TextureHandle& texture,
                                     float rotation[9]) {
    if (map.texture.empty()) return;
    const CachedTexture image = textures_.acquire(map.texture, true);
    if (!bgfx::isValid(image.handle)) return;
    texture = image.handle;
    for (int entry = 0; entry < 9; ++entry) rotation[entry] = map.rotation[entry];
}

// NiTextureEffect sphere map D3D camera space reflection vector turned into the effect frame then scaled half
void SceneRenderer::set_model_stage(const ModelLights* lights, bgfx::TextureHandle environment,
                                    const float rotation[9], const float world[16]) const {
    float directions[kModelLights][4] = {};
    float colours[kModelLights][4] = {};
    float row_u[4] = {0.f, 0.f, 0.f, 0.f};
    float row_v[4] = {0.f, 0.f, 0.f, 0.f};
    const float eye[4] = {camera_position_[0], camera_position_[1], camera_position_[2], 0.f};
    if (lights != nullptr && world != nullptr) {
        for (int light = 0; light < lights->count && light < kModelLights; ++light) {
            turn_by_world(world, lights->direction[light], directions[light]);
            directions[light][3] = 1.f;
            for (int channel = 0; channel < 3; ++channel) colours[light][channel] = lights->colour[light][channel];
        }
    }
    if (bgfx::isValid(environment) && rotation != nullptr && world != nullptr) {
        // NiTextureEffect UpdateProjection a sphere map reads world rotation columns 2 and 1 with half weights
        float column_z[3] = {rotation[2], rotation[5], rotation[8]};
        float column_y[3] = {rotation[1], rotation[4], rotation[7]};
        float world_z[4], world_y[4];
        turn_by_world(world, column_z, world_z);
        turn_by_world(world, column_y, world_y);
        for (int axis = 0; axis < 3; ++axis) {
            row_u[axis] = 0.5f * world_z[axis];
            row_v[axis] = -0.5f * world_y[axis];
        }
        row_u[3] = 1.f;
    }
    bgfx::setUniform(model_light_direction_, &directions[0][0], kModelLights);
    bgfx::setUniform(model_light_colour_, &colours[0][0], kModelLights);
    bgfx::setUniform(environment_row_u_, row_u);
    bgfx::setUniform(environment_row_v_, row_v);
    bgfx::setUniform(eye_position_, eye);
    bgfx::setTexture(3, environment_sampler_,
                     bgfx::isValid(environment) ? environment : textures_.white_stand_in(), kDiffuseSampler);
}

// First opaque coat owns depth buffer coats stacked pass LEQUAL
void SceneRenderer::submit_coat(const LayerBuffers& buffers, size_t coat_index,
                                const HourColour& ambient) const {
    const bool blended = buffers.translucent || coat_index > 0;
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LEQUAL;
    state |= blended ? BGFX_STATE_BLEND_NORMAL : BGFX_STATE_WRITE_Z;
    SurfaceUniforms surface;
    surface.splat[0] = buffers.coats[coat_index].repeat[0];
    surface.splat[1] = buffers.coats[coat_index].repeat[1];
    surface.shade = kTerrainShade;
    surface.ambient = ambient;
    set_shader_parameters(surface, world_fog());
    UvMatrix placed;
    placed.offset[0] = buffers.coats[coat_index].offset[0];
    placed.offset[1] = buffers.coats[coat_index].offset[1];
    set_uv_transform(placed);
    bgfx::setTexture(0, diffuse_sampler_, buffers.coats[coat_index].diffuse, kDiffuseSampler);
    bgfx::setTexture(1, coverage_sampler_, buffers.coats[coat_index].coverage, kCoverageSampler);
    bgfx::setState(state);
    const bool baked = baked_shading_ && bgfx::isValid(buffers.baked_vertices);
    bgfx::setVertexBuffer(0, baked ? buffers.baked_vertices : buffers.vertices);
    bgfx::setIndexBuffer(buffers.indices);
    bgfx::submit(target_view_, program_);
}

void SceneRenderer::submit_layer_buffers(const LayerBuffers& buffers,
                                         const HourColour& ambient) const {
    if (!bgfx::isValid(buffers.vertices)) return;
    for (size_t coat = 0; coat < buffers.coats.size(); ++coat)
        submit_coat(buffers, coat, ambient);
}

void SceneRenderer::submit_layer(SceneLayer layer) const {
    if (!visible(layer)) return;
    const HourColour& ambient = ambient_of(layer);
    submit_layer_buffers(layers_[static_cast<int>(layer)], ambient);
    for (const GroundPatchBuffers& patch : ground_patches_)
        if (patch.layer == layer) submit_layer_buffers(patch.buffers, ambient);
}

// The file the flip channel of the part names this frame the authored map when it names none
bgfx::TextureHandle SceneRenderer::flipped_texture(const PropPartBuffers& part,
                                                   const ModelPose& pose) {
    const int channel = part.animation.flip;
    if (channel < 0 || static_cast<size_t>(channel) >= pose.flip_frame.size()) return part.texture;
    const int frame = pose.flip_frame[static_cast<size_t>(channel)];
    if (frame < 0 || static_cast<size_t>(frame) >= part.flip_textures.size()) return part.texture;
    return part.flip_textures[static_cast<size_t>(frame)];
}

// A billboard node has its world rotation replaced then the chain below it carries on
const float* SceneRenderer::placed_matrix(const PropPartBuffers& part, const ModelPose& pose,
                                          const float world[16], float storage[16]) const {
    const int node_index = part.animation.node;
    const float* node = animated_node_matrix(pose, node_index);
    if (node == nullptr) return world;
    const int owner = node_index >= 0 && static_cast<std::size_t>(node_index) <
                                            pose.billboard_owner.size()
                          ? pose.billboard_owner[static_cast<std::size_t>(node_index)]
                          : -1;
    if (owner < 0) {
        multiply_matrix(world, node, storage);
        return storage;
    }
    float turned[16];
    multiply_matrix(world, &pose.node_world[static_cast<std::size_t>(owner) * 16], turned);
    apply_billboard(pose.node_billboard[static_cast<std::size_t>(owner)], camera_position_, billboard_right_, billboard_up_,
                    camera_forward_, turned);
    multiply_matrix(turned, &pose.below_billboard[static_cast<std::size_t>(node_index) * 16],
                    storage);
    return storage;
}

// Mesh uploaded once static placement own world matrix animated node
void SceneRenderer::bind_prop_part(const PropPartBuffers& part, const ModelPose& pose,
                                   const float world[16]) const {
    SurfaceUniforms surface;
    surface.splat[2] = part.alpha_cutout;
    surface.splat[3] = material_alpha(pose, part.animation.alpha, part.rest_alpha);
    surface.shade = part.shade;
    surface.ambient = hud_pass_ ? hud_ambient_
                                : (part.lit_by_map_ambient ? cycle_.ambient() : kBlackAmbient);
    add_ambient(part.lights, surface.ambient);
    set_shader_parameters(surface, world_fog());
    set_uv_transform(texture_matrix(pose, part.animation.uv));
    set_detail_stage(&part, &pose);
    bgfx::setTexture(0, diffuse_sampler_, flipped_texture(part, pose), part.sampler);
    float animated[16];
    const float* placed = placed_matrix(part, pose, world, animated);
    set_model_stage(&part.lights, part.environment, part.environment_rotation, placed);
    bgfx::setTransform(placed);
    if (bgfx::isValid(part.morphed)) bgfx::setVertexBuffer(0, part.morphed);
    else bgfx::setVertexBuffer(0, part.vertices);
    bgfx::setIndexBuffer(part.indices);
}

// Plain textured lighting client map model two cartoon passes NSB
void SceneRenderer::submit_prop_part(const PropPartBuffers& part, const ModelPose& pose,
                                     const float world[16]) const {
    const uint64_t state = part.draw_state;
    if (!cel_shading_) {
        bind_prop_part(part, pose, world);
        bgfx::setTexture(1, coverage_sampler_, textures_.white_stand_in(), kCoverageSampler);
        bgfx::setState(state);
        bgfx::submit(target_view_, program_);
        return;
    }
    bind_prop_part(part, pose, world);
    toon_.bind_surface();
    bgfx::setState(state);
    bgfx::submit(target_view_, toon_.surface());
    // Shell line closed surface far side hides blended card sheet
    if (part.blended) return;
    bind_prop_part(part, pose, world);
    toon_.bind_outline();
    bgfx::setState(kOutlineState);
    bgfx::submit(target_view_, toon_.outline());
}

// NiAlphaAccumulator sort key is bound centre on camera view axis which looks back so far sorts first
float SceneRenderer::translucent_depth(const PropPartBuffers& part,
                                       const float placed[16]) const {
    const bx::Vec3 origin(placed[12], placed[13], placed[14]);
    const bx::Vec3 back(camera_forward_[0], camera_forward_[1], camera_forward_[2]);
    if (part.bound.radius < 0.f) return -bx::dot(back, origin);
    const bx::Vec3 centre = bx::mul(
        bx::Vec3(part.bound.center[0], part.bound.center[1], part.bound.center[2]), placed);
    // NiAlphaAccumulator sorts by the bound centre a big box no longer paints over a sign on its face
    return -bx::dot(back, centre);
}

// FinishAccumulating walks ascending array last to first queue drawn
void SceneRenderer::submit_translucent() const {
    for (const TranslucentDraw& draw : translucent_)
        submit_prop_part(*draw.part, *draw.pose, draw.world);
}

// Placement names layer props on Props mesh tile no height
void SceneRenderer::submit_instances() { submit_instances_of(prop_instances_); }

void SceneRenderer::submit_instances_of(const std::vector<PropInstance>& instances) {
    translucent_.clear();
    for (const PropInstance& instance : instances) {
        if (!visible(instance.layer) || instance.model_index >= prop_models_.size()) continue;
        const PropModelBuffers& model = prop_models_[instance.model_index];
        for (const PropPartBuffers& part : model.parts) {
            if (!part.sortable) {
                submit_prop_part(part, model.pose, instance.world);
                continue;
            }
            float placed[16];
            const float* matrix = placed_matrix(part, model.pose, instance.world, placed);
            translucent_.push_back(
                {&part, &model.pose, instance.world, translucent_depth(part, matrix)});
        }
    }
    std::sort(translucent_.begin(), translucent_.end(),
              [](const TranslucentDraw& left, const TranslucentDraw& right) {
                  return left.depth > right.depth;
              });
    submit_translucent();
}

// Skinned draw needs palette texture placement world matrix spawn shares
void SceneRenderer::bind_character_part(const SkinnedPartBuffers& part,
                                        const SkinnedPose& pose, std::size_t index,
                                        const float world[16]) const {
    SurfaceUniforms surface;
    surface.splat[2] = part.alpha_cutout;
    surface.splat[3] = part.rest_alpha;
    surface.shade = part.shade;
    surface.ambient = cycle_.ambient();
    add_ambient(part.lights, surface.ambient);
    set_shader_parameters(surface, world_fog());
    set_model_stage(&part.lights, part.environment, part.environment_rotation, world);
    set_uv_transform(kRawTextureCoordinates);
    const float* palette = pose.rows.data() + pose.part_offset[index];
    bgfx::setUniform(bone_rows_uniform_, palette,
                     static_cast<uint16_t>(part.palette_slots * kBoneMatrixRows));
    bgfx::setTexture(0, diffuse_sampler_, part.texture, part.sampler);
    bgfx::setTransform(world);
    bgfx::setVertexBuffer(0, part.vertices);
    bgfx::setIndexBuffer(part.indices);
}

// Client cel shades every character 1393 bodies toon shader
void SceneRenderer::submit_character_part(const SkinnedPartBuffers& part,
                                          const SkinnedPose& pose, std::size_t index,
                                          const float world[16]) const {
    const uint64_t state = part.draw_state;
    if (!cel_shading_) {
        bind_character_part(part, pose, index, world);
        bgfx::setTexture(1, coverage_sampler_, textures_.white_stand_in(), kCoverageSampler);
        bgfx::setState(state);
        bgfx::submit(target_view_, skinned_program_);
        return;
    }
    bind_character_part(part, pose, index, world);
    toon_.bind_surface();
    bgfx::setState(state);
    bgfx::submit(target_view_, toon_.skinned_surface());
    // Cutout part sheet shell paints whole same reason blended card
    if (part.blended) return;
    bind_character_part(part, pose, index, world);
    toon_.bind_skinned_outline();
    bgfx::setState(kOutlineState);
    bgfx::submit(target_view_, toon_.skinned_outline());
}

// Characters ride Monsters layer key hides spawn markers hides bodies
void SceneRenderer::submit_characters() const {
    if (!visible(SceneLayer::Monsters)) return;
    for (std::size_t index = 0; index < character_instances_.size(); ++index) {
        const CharacterInstance& instance = character_instances_[index];
        if (instance.model_index >= characters_.size()) continue;
        const CharacterBuffers& character = characters_[instance.model_index];
        // Populated tile four thousand draws bodies camera sees few dozen
        if (!inside_frustum(character, instance.world)) continue;
        const SkinnedPose& pose = pose_of_instance(index);
        for (std::size_t part = 0; part < character.parts.size(); ++part)
            submit_character_part(character.parts[part], pose, part, instance.world);
    }
}

void SceneRenderer::submit_sky_part(const PropPartBuffers& part, const SkyPhaseBuffers& sky,
                                    const float world[16]) const {
    const ModelPose& pose = sky.model.pose;
    SurfaceUniforms surface;
    surface.splat[3] = material_alpha(pose, part.animation.alpha, part.rest_alpha);
    surface.shade = part.shade;
    surface.ambient = kNoTint;
    set_shader_parameters(surface, sky.fog);
    set_uv_transform(texture_matrix(pose, part.animation.uv));
    bgfx::setTexture(0, diffuse_sampler_, part.texture, part.sampler);
    bgfx::setTexture(1, coverage_sampler_, textures_.white_stand_in(), kCoverageSampler);
    // The dome hangs under an animated node on most tracks its rest transform sets the horizon
    float animated[16];
    bgfx::setTransform(placed_matrix(part, pose, world, animated));
    bgfx::setState(kSkyState);
    bgfx::setVertexBuffer(0, part.vertices);
    bgfx::setIndexBuffer(part.indices);
    bgfx::submit(sky_view_, program_);
}

// The stock loads the sky nif like the track nif so the dome stands still over the world
void SceneRenderer::submit_sky(const float camera_position[3]) const {
    (void)camera_position;
    const size_t phase = sky_phase_index();
    if (!visible(SceneLayer::Sky) || phase >= sky_phases_.size()) return;
    static const float kIdentity[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                        0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    const SkyPhaseBuffers& sky = sky_phases_[phase];
    for (const PropPartBuffers& part : sky.model.parts) submit_sky_part(part, sky, kIdentity);
}

void SceneRenderer::set_field_of_view(float vertical_radians, float horizontal_radians) {
    // A lens narrower than a degree or wider than a flat plane cannot project
    if (vertical_radians > 0.02f && vertical_radians < 3.1f) fov_vertical_ = vertical_radians;
    fov_horizontal_ = horizontal_radians > 0.02f && horizontal_radians < 3.1f ? horizontal_radians : 0.f;
}

void SceneRenderer::scene_projection(DepthRange range, float out[16]) const {
    const float aspect =
        static_cast<float>(viewport_.width) / static_cast<float>(viewport_.height);
    // Half extents of the view plane on the near plane bx scales the extents by the near distance
    const float up = std::tan(fov_vertical_ * 0.5f) * range.near_plane;
    const float right =
        (fov_horizontal_ > 0.f ? std::tan(fov_horizontal_ * 0.5f) * range.near_plane : up * aspect);
    // KnC data is right handed z up a left handed projection mirrors every banner text
    bx::mtxProj(out, up, -up, -right, right, range.near_plane, range.far_plane,
                bgfx::getCaps()->homogeneousDepth, bx::Handedness::Right);
}

// Gribb Hartmann combined matrix side plane clip space half space
void SceneRenderer::update_scene_frustum(const float view_matrix[16]) {
    float projection[16];
    scene_projection(world_range_, projection);
    float combined[16];
    bx::mtxMul(combined, view_matrix, projection);
    // Left right bottom top far as w plus x minus y z
    const int column[5] = {0, 0, 1, 1, 2};
    const float sign[5] = {1.f, -1.f, 1.f, -1.f, -1.f};
    for (int plane = 0; plane < 5; ++plane)
        for (int axis = 0; axis < 4; ++axis)
            scene_frustum_.planes[plane][axis] =
                combined[axis * 4 + 3] + sign[plane] * combined[axis * 4 + column[plane]];
    for (int plane = 0; plane < 5; ++plane) {
        float* values = scene_frustum_.planes[plane];
        const float length =
            std::sqrt(values[0] * values[0] + values[1] * values[1] + values[2] * values[2]);
        if (!(length > 0.f)) {
            scene_frustum_.valid = false;
            return;
        }
        for (int axis = 0; axis < 4; ++axis) values[axis] /= length;
    }
    scene_frustum_.valid = true;
}

// Model sphere moved placement uniform scale translation radius scales
bool SceneRenderer::inside_frustum(const CharacterBuffers& character,
                                   const float world[16]) const {
    if (!scene_frustum_.valid || character.bound_radius <= 0.f) return true;
    const float scale = world[0];
    float centre[3];
    for (int axis = 0; axis < 3; ++axis)
        centre[axis] = world[12 + axis] + scale * character.bound_centre[axis];
    const float radius = character.bound_radius * (scale > 0.f ? scale : 1.f);
    for (const float* plane : scene_frustum_.planes) {
        const float distance = plane[0] * centre[0] + plane[1] * centre[1] +
                               plane[2] * centre[2] + plane[3];
        if (distance < -radius) return false;
    }
    return true;
}

void SceneRenderer::set_view_projection(bgfx::ViewId view, const float view_matrix[16],
                                       DepthRange range) const {
    float projection[16];
    scene_projection(range, projection);
    // Off screen frame viewport own size pass origin screen
    const bool offscreen = glowing();
    bgfx::setViewRect(view, offscreen ? 0 : viewport_.x, offscreen ? 0 : viewport_.y,
                      viewport_.width, viewport_.height);
    bgfx::setViewTransform(view, view_matrix, projection);
}

void SceneRenderer::draw(const float view_matrix[16], const float camera_position[3]) {
    refresh_poses();
    target_view_ = kSceneView;
    hud_pass_ = false;
    hud_scenes_drawn_ = 0;
    // Viewport uncovered painted or editor frame shows last thing back
    bgfx::setViewRect(kFrameClearView, 0, 0, width_, height_);
    bgfx::setViewClear(kFrameClearView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       packed_rgba(backdrop()), 1.0f, 0);
    bgfx::touch(kFrameClearView);
    // Glow needs finished world texture world off screen frame on
    bgfx::FrameBufferHandle target = BGFX_INVALID_HANDLE;
    if (glowing()) target = glow_.scene_target();
    bgfx::setViewFrameBuffer(kSkyView, target);
    bgfx::setViewFrameBuffer(kSceneView, target);
    set_view_projection(kSkyView, view_matrix, sky_range());
    set_view_projection(kSceneView, view_matrix, world_range_);
    update_scene_frustum(view_matrix);
    take_billboard_basis(view_matrix);
    for (int axis = 0; axis < 3; ++axis) camera_position_[axis] = camera_position[axis];
    bgfx::setViewClear(kSkyView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, packed_rgba(backdrop()),
                       1.0f, 0);
    // Sky view owns clear runs when tile names no sky
    bgfx::touch(kSkyView);
    submit_sky(camera_position);
    submit_layer(SceneLayer::Terrain);
    submit_layer(SceneLayer::Grid);
    submit_instances();
    // Empty source fills layer draws nothing costs nothing
    submit_layer(SceneLayer::Monsters);
    submit_characters();
    submit_particles();
    submit_layer(SceneLayer::Water);
    submit_layer(SceneLayer::Waypaths);
    submit_layer(SceneLayer::PropHulls);
    submit_layer(SceneLayer::Collision);
    if (glowing()) glow_.submit();
}

// The rect takes its own view the depth is cleared inside it so the frame behind never hides the art
void SceneRenderer::draw_hud_scene(const HudScene& scene) {
    if (!started_ || scene.width == 0 || scene.height == 0 || scene.instances.empty()) return;
    if (hud_scenes_drawn_ >= kHudSceneViews) return;
    const bgfx::ViewId view = static_cast<bgfx::ViewId>(kHudSceneFirstView + hud_scenes_drawn_++);
    bgfx::setViewFrameBuffer(view, BGFX_INVALID_HANDLE);
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(view, scene.x, scene.y, scene.width, scene.height);
    bgfx::setViewClear(view, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
    float projection[16];
    const float near_plane = std::max(scene.near_plane, kClientNearPlane);
    const float up = std::tan(scene.fov_vertical * 0.5f) * near_plane;
    const float aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
    const float right = scene.fov_horizontal > 0.f
                            ? std::tan(scene.fov_horizontal * 0.5f) * near_plane
                            : up * aspect;
    bx::mtxProj(projection, up, -up, -right, right, near_plane, scene.far_plane,
                bgfx::getCaps()->homogeneousDepth, bx::Handedness::Right);
    bgfx::setViewTransform(view, scene.view, projection);
    bgfx::touch(view);
    // The camera basis of the pass the billboards and the sort read it the frame one comes back after
    float saved_right[3];
    float saved_up[3];
    float saved_forward[3];
    float saved_position[3];
    for (int axis = 0; axis < 3; ++axis) {
        saved_right[axis] = billboard_right_[axis];
        saved_up[axis] = billboard_up_[axis];
        saved_forward[axis] = camera_forward_[axis];
        saved_position[axis] = camera_position_[axis];
        camera_position_[axis] = scene.eye[axis];
    }
    take_billboard_basis(scene.view);
    target_view_ = view;
    hud_pass_ = true;
    hud_ambient_ = scene.ambient;
    submit_instances_of(scene.instances);
    submit_particles_of(scene.instances);
    hud_pass_ = false;
    target_view_ = kSceneView;
    for (int axis = 0; axis < 3; ++axis) {
        billboard_right_[axis] = saved_right[axis];
        billboard_up_[axis] = saved_up[axis];
        camera_forward_[axis] = saved_forward[axis];
        camera_position_[axis] = saved_position[axis];
    }
}

// The sky the layers the props the riders and their particles through a second camera the rect is cleared first
void SceneRenderer::draw_world_inset(const HudScene& scene) {
    if (!started_ || scene.width == 0 || scene.height == 0) return;
    if (hud_scenes_drawn_ >= kHudSceneViews) return;
    const bgfx::ViewId view = static_cast<bgfx::ViewId>(kHudSceneFirstView + hud_scenes_drawn_++);
    bgfx::setViewFrameBuffer(view, BGFX_INVALID_HANDLE);
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(view, scene.x, scene.y, scene.width, scene.height);
    bgfx::setViewClear(view, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, packed_rgba(backdrop()), 1.0f, 0);
    float projection[16];
    const float near_plane = std::max(scene.near_plane, kClientNearPlane);
    const float up = std::tan(scene.fov_vertical * 0.5f) * near_plane;
    const float aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
    const float right = scene.fov_horizontal > 0.f
                            ? std::tan(scene.fov_horizontal * 0.5f) * near_plane
                            : up * aspect;
    bx::mtxProj(projection, up, -up, -right, right, near_plane, scene.far_plane,
                bgfx::getCaps()->homogeneousDepth, bx::Handedness::Right);
    bgfx::setViewTransform(view, scene.view, projection);
    bgfx::touch(view);
    float saved_right[3];
    float saved_up[3];
    float saved_forward[3];
    float saved_position[3];
    for (int axis = 0; axis < 3; ++axis) {
        saved_right[axis] = billboard_right_[axis];
        saved_up[axis] = billboard_up_[axis];
        saved_forward[axis] = camera_forward_[axis];
        saved_position[axis] = camera_position_[axis];
        camera_position_[axis] = scene.eye[axis];
    }
    const ViewFrustum saved_frustum = scene_frustum_;
    // The frame frustum would drop riders the inset sees so the inset culls none
    scene_frustum_.valid = false;
    take_billboard_basis(scene.view);
    target_view_ = view;
    sky_view_ = view;
    submit_sky(scene.eye);
    submit_layer(SceneLayer::Terrain);
    submit_layer(SceneLayer::Grid);
    submit_instances();
    submit_layer(SceneLayer::Monsters);
    submit_characters();
    submit_particles();
    submit_layer(SceneLayer::Water);
    target_view_ = kSceneView;
    sky_view_ = kSkyView;
    scene_frustum_ = saved_frustum;
    for (int axis = 0; axis < 3; ++axis) {
        billboard_right_[axis] = saved_right[axis];
        billboard_up_[axis] = saved_up[axis];
        camera_forward_[axis] = saved_forward[axis];
        camera_position_[axis] = saved_position[axis];
    }
}

void SceneRenderer::draw_frame_veil(const HourColour& colour, float alpha) {
    if (!started_ || alpha <= 0.f || hud_scenes_drawn_ >= kHudSceneViews) return;
    if (bgfx::getAvailTransientVertexBuffer(4, layout_) < 4 || bgfx::getAvailTransientIndexBuffer(6) < 6)
        return;
    const bgfx::ViewId view = static_cast<bgfx::ViewId>(kHudSceneFirstView + hud_scenes_drawn_++);
    bgfx::setViewFrameBuffer(view, BGFX_INVALID_HANDLE);
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(view, 0, 0, width_, height_);
    bgfx::setViewClear(view, BGFX_CLEAR_NONE);
    float ortho[16];
    bx::mtxOrtho(ortho, 0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 0.f, bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(view, nullptr, ortho);
    bgfx::TransientVertexBuffer vertices;
    bgfx::TransientIndexBuffer indices;
    bgfx::allocTransientVertexBuffer(&vertices, 4, layout_);
    bgfx::allocTransientIndexBuffer(&indices, 6);
    SceneVertex* corner = reinterpret_cast<SceneVertex*>(vertices.data);
    const float veil[4] = {colour.red, colour.green, colour.blue, std::min(alpha, 1.f)};
    const uint32_t abgr = nif_colour_abgr(veil);
    const float at[4][2] = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
    for (int i = 0; i < 4; ++i) {
        corner[i] = SceneVertex();
        corner[i].x = at[i][0];
        corner[i].y = at[i][1];
        corner[i].z = 0.5f;
        corner[i].abgr = abgr;
        corner[i].u = at[i][0];
        corner[i].v = at[i][1];
    }
    uint16_t* triangle = reinterpret_cast<uint16_t*>(indices.data);
    const uint16_t order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; ++i) triangle[i] = order[i];
    SurfaceUniforms surface;
    surface.shade = kUnlitShade;
    hud_pass_ = true;
    set_shader_parameters(surface, world_fog());
    hud_pass_ = false;
    set_uv_transform(kRawTextureCoordinates);
    bgfx::setTexture(0, diffuse_sampler_, textures_.white_stand_in(), kCoverageSampler);
    bgfx::setTexture(1, coverage_sampler_, textures_.white_stand_in(), kCoverageSampler);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
    bgfx::setVertexBuffer(0, &vertices);
    bgfx::setIndexBuffer(&indices);
    bgfx::submit(view, program_);
}

void SceneRenderer::shutdown() {
    if (!started_) return;
    release_scene();
    textures_.destroy_all();
    toon_.destroy();
    glow_.destroy();
    destroy_uniform(diffuse_sampler_);
    destroy_uniform(coverage_sampler_);
    destroy_uniform(splat_params_);
    destroy_uniform(shade_emissive_);
    destroy_uniform(shade_ambient_);
    destroy_uniform(shade_alpha_);
    destroy_uniform(ambient_colour_);
    destroy_uniform(shade_diffuse_);
    destroy_uniform(sun_direction_);
    destroy_uniform(sun_colour_);
    destroy_uniform(fog_colour_uniform_);
    destroy_uniform(fog_range_uniform_);
    destroy_uniform(uv_rows_uniform_);
    destroy_uniform(uv_offset_uniform_);
    destroy_uniform(detail_sampler_);
    destroy_uniform(detail_rows_uniform_);
    destroy_uniform(detail_offset_uniform_);
    destroy_uniform(detail_params_uniform_);
    destroy_uniform(bone_rows_uniform_);
    destroy_uniform(environment_sampler_);
    destroy_uniform(environment_row_u_);
    destroy_uniform(environment_row_v_);
    destroy_uniform(eye_position_);
    destroy_uniform(model_light_direction_);
    destroy_uniform(model_light_colour_);
    if (bgfx::isValid(program_)) bgfx::destroy(program_);
    program_ = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(skinned_program_)) bgfx::destroy(skinned_program_);
    skinned_program_ = BGFX_INVALID_HANDLE;
    bgfx::shutdown();
    started_ = false;
}

}
