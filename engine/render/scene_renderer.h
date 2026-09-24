#pragma once
#include "engine/render/day_night.h"
#include "engine/render/glow_pass.h"
#include "engine/render/map_scene.h"
#include "engine/render/model_animation.h"
#include "engine/render/particle_simulation.h"
#include "engine/render/scene_draw_state.h"
#include "engine/render/texture_cache.h"
#include "engine/render/toon_program.h"

#include <bgfx/bgfx.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Render {

// Client depth range far plane 240 from the quality table near 0 1 FUN 0045E4B0
constexpr float kClientNearPlane = 0.1f;
constexpr float kClientFarPlane  = 240.0f;

// The lens every pass draws through vertical field of view 60 degrees DAT 0075DDCC times DAT 006BC294
constexpr float kVerticalFieldOfView = 60.0f;
// The same lens in radians the default of the per frame field of view setter
constexpr float kDefaultFieldOfViewRadians = kVerticalFieldOfView * 3.14159265f / 180.f;

// Camera depth range one pass draws with CxSky Render swaps far
struct DepthRange {
    float near_plane = kClientNearPlane;
    float far_plane  = kClientFarPlane;
};

// Linear fog ramp depth range shader u fogRange where starts
struct FogRamp {
    float start        = 0.f;
    float inverse_span = 0.f;
    float radial       = 0.f;
    // Fog opaqueness thickest shader reads coverage ceiling alpha slot vec4
    float density      = 1.f;
};

// Rectangle window world drawn into editor frame centre panel whole
struct ViewportRect {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
};

// Full daylight viewer not open dark client starts midnight
constexpr int kDefaultHour = 12;

struct RendererSetup {
    void*            native_window = nullptr;
    uint16_t         width  = 0;
    uint16_t         height = 0;
    bgfx::CallbackI* callback = nullptr;
    bool             vsync = true;
    // Far plane world drawn node fog depth measured against
    float far_plane = kClientFarPlane;
    int hour = kDefaultHour;
    // Animation clock starts real seconds capture states frame writes
    float animation_seconds = 0.f;
};

// small 3D scene drawn in a frame rect through its own camera before the overlay the race gauge uses it
struct HudScene {
    // The rect in frame pixels the origin at the top left of the frame
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    // The camera view matrix of the scene right handed like the world one and where its eye stands
    float view[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                      0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    float eye[3] = {0.f, 0.f, 0.f};
    // The flat ambient the parts take one leaves the authored colours as they are
    HourColour ambient;
    // The lens in radians a horizontal of zero follows the rect aspect
    float fov_vertical = kDefaultFieldOfViewRadians;
    // The placements drawn each names an appended prop model
    std::vector<PropInstance> instances;
    float fov_horizontal = 0.f;
    float near_plane = kClientNearPlane;
    float far_plane = kClientFarPlane;
};

// Draws MapScene layers splatting textured ones coat per pass props
class SceneRenderer {
public:
    ~SceneRenderer();

    bool init(const RendererSetup& setup);
    // Drops uploaded before opening second document not leak first buffers
    void upload(const MapScene& scene);
    void resize(uint16_t width, uint16_t height);
    // Window world lands resizing resets whole window
    void set_viewport(const ViewportRect& viewport);
    const ViewportRect& viewport() const { return viewport_; }
    // Depth range world drawn against pick project sky own never
    DepthRange world_range() const { return world_range_; }
    // Far plane world drawn scene size fog ramp world units
    void set_far_plane(float far_plane);
    // The lens of the frame vertical field radians the horizontal one zero follows the viewport aspect
    void set_field_of_view(float vertical_radians, float horizontal_radians = 0.f);
    float vertical_field_of_view() const { return fov_vertical_; }
    float horizontal_field_of_view() const { return fov_horizontal_; }
    // The world projection of this lens a hud projects its name tags with it
    void projection(float out[16]) const { scene_projection(world_range_, out); }
    // Replaces layer geometry overlay edit rebuilt other layer texture stays
    void upload_layer(SceneLayer layer, const LayerMesh& mesh);
    // Characters stand edit moved marker nothing uploaded bodies GPU matrices
    void update_character_instances(const std::vector<CharacterInstance>& instances);
    // One body into scene already up tile knows server says drop
    void append_character_model(const CharacterModel& model);
    // One prop model scene already up effect spell cast returns landed
    std::size_t append_prop_model(const PropModel& model);
    // Placements after scene own replaced whole effect flight stop existing
    void set_appended_prop_instances(const std::vector<PropInstance>& instances);
    // Runs appended model pools seconds scene clock cast restarts emitter
    void restart_prop_particles(std::size_t model_index, float seconds);
    // Poses an appended model that many seconds into its clips at once the pools follow it
    void pose_prop_model_at(std::size_t model_index, float played);
    // The posed model space matrix of a named node of an appended model false when it has none
    bool prop_node_matrix(std::size_t model_index, const std::string& name, float out[16]) const;
    void toggle(SceneLayer layer);
    void set_visible(SceneLayer layer, bool shown);
    bool visible(SceneLayer layer) const;
    // Anything uploaded onto layer panel checkbox on shows nothing
    bool layer_populated(SceneLayer layer) const;
    // Ground shading lighting tile ships or synthetic lambert own slope
    void toggle_baked_shading();
    void set_baked_shading(bool baked);
    bool baked_shading() const { return baked_shading_; }
    // Distance fades node fog colour client always tool see tile
    void toggle_fog();
    void set_fog(bool fogged);
    bool fog_enabled() const { return fog_enabled_; }
    // Props scene meshes cartoon shader outline pass 1 of 865 shipped
    void toggle_cel_shading();
    void set_cel_shading(bool celled);
    bool cel_shading() const { return cel_shading_; }
    // Finished frame bright passed blurred added back on default FullScreenGlow 1
    void toggle_glow();
    void set_glow(bool glowing);
    bool glow_enabled() const { return glow_enabled_; }
    // The directional light upload takes the scene's own this replaces it live
    void set_sun(const SunLight& sun) { sun_ = sun; }
    const SunLight& sun() const { return sun_; }
    // One clock hour picks terrain ambient fog colour sky caller runs
    DayNightCycle& day_night() { return cycle_; }
    // Animation clock runs real seconds caller advances owns pausing scrubbing
    AnimationClock& animation() { return animation_; }
    // Plays one clip every character scene model document one tile minus
    void set_character_clip(int clip);
    int  character_clip() const { return character_clip_; }
    // Clips one character scene play transport empty scene holding none
    const std::vector<CharacterClip>& character_clips() const;
    // Bodies stand scene model document tile transport ask which kind
    std::size_t character_count() const { return character_instances_.size(); }
    // Sky hour now shows empty tile node named none
    std::string sky_name() const;
    // Which sky phase draws minus one picks it by the hour the race sets it from the launch flag
    void set_sky_phase(int phase);
    int  sky_phase() const { return sky_phase_; }
    std::size_t sky_phase_count() const;
    // Submits sky visible layer caller presents frame overlay draw camera
    void draw(const float view_matrix[16], const float camera_position[3]);
    // draws one hud scene into its rect depth cleared colour kept at most kHudSceneViews per frame extras dropped
    void draw_hud_scene(const HudScene& scene);
    // The whole world of the frame through a second camera in a rect its instances list is not read
    void draw_world_inset(const HudScene& scene);
    // flat colour veil over the whole frame after world before hud scenes uses one of the hud scene views
    void draw_frame_veil(const HourColour& colour, float alpha);
    void shutdown();

private:
    // One splat pass diffuse to tile mask lets through
    struct CoatTextures {
        bgfx::TextureHandle diffuse  = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle coverage = BGFX_INVALID_HANDLE;
        float repeat[2] = {1.f, 1.f};
        float offset[2] = {0.f, 0.f};
    };

    struct LayerBuffers {
        bgfx::VertexBufferHandle  vertices = BGFX_INVALID_HANDLE;
        // Mesh tile baked lighting vertex colour
        bgfx::VertexBufferHandle  baked_vertices = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle   indices  = BGFX_INVALID_HANDLE;
        std::vector<CoatTextures> coats;
        bool  translucent = false;
    };

    // Geometry prop model shares texture GPU device state NIF translated
    struct PropPartBuffers {
        bgfx::VertexBufferHandle vertices = BGFX_INVALID_HANDLE;
        // A morphed part draws from this instead the pose rewrites it when a weight moves
        bgfx::DynamicVertexBufferHandle morphed = BGFX_INVALID_HANDLE;
        // The baked part and one NiMorphData frame per target three floats a vertex
        std::vector<SceneVertex>        morph_posed;
        std::vector<std::vector<float>> morph_frames;
        std::vector<float>              morph_weights;
        uint32_t                        morph_first_vertex = 0;
        // The files of the flip channel of the part the pose picks one a frame
        std::vector<bgfx::TextureHandle> flip_textures;
        bool                            morph_relative = true;
        bgfx::IndexBufferHandle  indices  = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle      texture  = BGFX_INVALID_HANDLE;
        // Address modes base map ready for bgfx setTexture
        uint32_t                 sampler = kDiffuseSampler;
        // The detail map modulated twice over the base invalid for none
        bgfx::TextureHandle      detail  = BGFX_INVALID_HANDLE;
        // Blend depth test and depth write ready for bgfx setState
        uint64_t                 draw_state = 0;
        // Alpha texel discarded 0 keeps every texel
        float                    alpha_cutout = 0.f;
        // Part properties light vertex colour
        ShadeState               shade;
        // NiMaterialProperty alpha NiAlphaController replaces 219 effect surfaces authored
        float                    rest_alpha = 1.f;
        // Part translucent bucket drawn after opaque one tree order
        bool                     blended = false;
        // Bucket reorder NiAlphaProperty bit 13 accumulator surface tree 29 of
        bool                     sortable = false;
        // Part sphere space vertices accumulator sort key measured
        ModelBound               bound;
        PartAnimation            animation;
        // Map ambient light reaches surface emissive whole colour
        bool                     lit_by_map_ambient = true;
        // The sphere map the part adds invalid for none and the model space rotation of its effect
        bgfx::TextureHandle      environment = BGFX_INVALID_HANDLE;
        float                    environment_rotation[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
        // The lights of the model own tree
        ModelLights              lights;
    };

    // One draw NiAlphaAccumulator holding what draw stands key ordered
    struct TranslucentDraw {
        const PropPartBuffers* part  = nullptr;
        const ModelPose*       pose  = nullptr;
        const float*           world = nullptr;
        float                  depth = 0.f;
    };

    // Particle system model authored as pool authoring produces quads drawn
    struct ParticleBuffers {
        ParticleSystemDefinition definition;
        ParticleSimulation       simulation;
        bgfx::TextureHandle      texture = BGFX_INVALID_HANDLE;
        uint32_t                 sampler = kDiffuseSampler;
        uint64_t                 draw_state = 0;
        float                    alpha_cutout = 0.f;
        // Material emissive colour rest alpha 864 emitters white particle tint
        HourColour               tint;
        float                    rest_alpha = 1.f;
    };

    // Model GPU animates pose stands in sampled once frame
    struct PropModelBuffers {
        std::vector<PropPartBuffers>  parts;
        std::vector<ParticleBuffers>  particles;
        ModelAnimation                animation;
        ModelPose                     pose;
        std::vector<NamedNode>        named_nodes;
        // Scene clock model pools last started tile props clock effect cast
        float                         particle_epoch = 0.f;
    };

    // Skinned draw GPU geometry texture palette slots pose bind
    struct SkinnedPartBuffers {
        bgfx::VertexBufferHandle vertices = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle  indices  = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle      texture  = BGFX_INVALID_HANDLE;
        uint32_t                 sampler = kDiffuseSampler;
        uint64_t                 draw_state = 0;
        float                    alpha_cutout = 0.f;
        float                    rest_alpha = 1.f;
        ShadeState               shade;
        bool                     blended = false;
        uint16_t                 palette_slots = 0;
        bgfx::TextureHandle      environment = BGFX_INVALID_HANDLE;
        float                    environment_rotation[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
        ModelLights              lights;
    };

    // Character GPU what poses it sampled once frame spawns
    struct CharacterBuffers {
        std::vector<SkinnedPartBuffers> parts;
        CharacterRig                    rig;
        SkinnedPose                     pose;
        // Model space sphere rest bounds fit placement tested frustum
        float bound_centre[3] = {0.f, 0.f, 0.f};
        float bound_radius    = 0.f;
    };

    // Four side planes scene view far plane world space normal distance
    struct ViewFrustum {
        float planes[5][4] = {};
        bool  valid = false;
    };

    // Time of day sky GPU depth range CxSky Render swaps far plane
    struct SkyPhaseBuffers {
        std::string      name;
        PropModelBuffers model;
        DepthRange       range;
        FogRamp          fog;
    };

    bool create_program();
    bool create_skinned_program();
    bool create_uniforms();
    void release_scene();
    // World off screen frame glow reads back
    bool glowing() const;
    // Viewport cleared to hour fog game sky up editor neutral ground
    const HourColour& backdrop() const;
    void configure_fog(const std::array<SceneFog, kHoursPerDay>& fog_of_hour);
    // Fog shape clock hour Holy Beast fills 24 alike
    const FogRamp& world_fog() const;
    void set_view_projection(bgfx::ViewId view, const float view_matrix[16],
                             DepthRange range) const;
    bgfx::VertexBufferHandle create_baked_buffer(const LayerMesh& mesh);
    void resolve_coats(const LayerMesh& mesh, LayerBuffers& buffers);
    // Uploads mesh coats into buffers caller owns nine fixed layers
    void fill_layer_buffers(const LayerMesh& mesh, LayerBuffers& buffers);
    void upload_ground_patches(const MapScene& scene);
    PropPartBuffers create_part(const PropPart& part);
    // Writes the posed shape of every morphed part of one model into its dynamic buffer
    static void refresh_morphed_parts(PropModelBuffers& model);
    static void destroy_part(PropPartBuffers& part);
    // Part stands this frame placement or placement carried node controller
    const float* placed_matrix(const PropPartBuffers& part, const ModelPose& pose,
                                      const float world[16], float storage[16]) const;
    // NiAlphaAccumulator sort key placed part
    float translucent_depth(const PropPartBuffers& part, const float placed[16]) const;
    // Accumulator queued far to near
    void submit_translucent() const;
    ParticleBuffers create_particle_system(const ParticleSystemDefinition& definition);
    // Pools every model stepped once frame before draws
    void refresh_particles();
    void submit_particles() const;
    // The pools of these placements a hud scene hands its own list the frame hands the scene one
    void submit_particles_of(const std::vector<PropInstance>& instances) const;
    void submit_particle_system(const ParticleBuffers& system, const ModelPose& pose,
                                const float world[16]) const;
    // Camera world space right up plane particle quad built
    void take_billboard_basis(const float view_matrix[16]);
    PropModelBuffers create_model(const PropModel& model);
    void upload_prop_models(const MapScene& scene);
    void report_particle_systems() const;
    SkinnedPartBuffers create_skinned_part(const SkinnedPart& part, std::size_t slots);
    void upload_characters(const MapScene& scene);
    void upload_one_character(const CharacterModel& model);
    void submit_characters() const;
    // Projection depth range viewport aspect
    void scene_projection(DepthRange range, float out[16]) const;
    // Scene view stands this frame placed model seen
    void update_scene_frustum(const float view_matrix[16]);
    bool inside_frustum(const CharacterBuffers& character, const float world[16]) const;
    void bind_character_part(const SkinnedPartBuffers& part, const SkinnedPose& pose,
                             std::size_t index, const float world[16]) const;
    void submit_character_part(const SkinnedPartBuffers& part, const SkinnedPose& pose,
                               std::size_t index, const float world[16]) const;
    // Pose placement draws own clock model shares
    const SkinnedPose& pose_of_instance(std::size_t index) const;
    void upload_sky_phases(const MapScene& scene);
    // Pass ramp depth range drawn against world far plane CxSky sky
    void set_shader_parameters(const SurfaceUniforms& surface, const FogRamp& ramp) const;
    // Every draw sets one unanimated surface sets identity
    void set_uv_transform(const UvMatrix& matrix) const;
    // The detail stage of a draw off for every draw that binds none
    void set_detail_stage(const PropPartBuffers* part, const ModelPose* pose) const;
    // The model own lights and the sphere map turned by the placement a null light set clears both
    void set_model_stage(const ModelLights* lights, bgfx::TextureHandle environment,
                         const float rotation[9], const float world[16]) const;
    // Uploads the sphere map of a part a missing file leaves the part without one
    void take_environment(const EnvironmentMap& map, bgfx::TextureHandle& texture, float rotation[9]);
    // Samples every model animates once frame before draws
    void refresh_poses();
    void refresh_character_poses();
    // Samples again per placement carries clock
    void refresh_instance_poses();
    // Hour ambient reaches client geometry nothing else
    const HourColour& ambient_of(SceneLayer layer) const;
    size_t sky_phase_index() const;
    DepthRange phase_range(const SkyPhase& phase) const;
    DepthRange sky_range() const;
    void submit_layer(SceneLayer layer) const;
    void submit_layer_buffers(const LayerBuffers& buffers, const HourColour& ambient) const;
    void submit_coat(const LayerBuffers& buffers, size_t coat_index,
                     const HourColour& ambient) const;
    void submit_instances();
    // The parts of these placements opaque first then the translucent ones far to near
    void submit_instances_of(const std::vector<PropInstance>& instances);
    // Prop draw needs program bgfx clears state cartoon passes call
    void bind_prop_part(const PropPartBuffers& part, const ModelPose& pose,
                        const float world[16]) const;
    static bgfx::TextureHandle flipped_texture(const PropPartBuffers& part, const ModelPose& pose);
    void submit_prop_part(const PropPartBuffers& part, const ModelPose& pose,
                          const float world[16]) const;
    void submit_sky(const float camera_position[3]) const;
    void submit_sky_part(const PropPartBuffers& part, const SkyPhaseBuffers& sky,
                         const float world[16]) const;

    TextureCache          textures_;
    ViewFrustum           scene_frustum_;
    float                 billboard_right_[3] = {1.f, 0.f, 0.f};
    float                 billboard_up_[3]    = {0.f, 1.f, 0.f};
    // Camera world direction accumulator projects bound onto
    float                 camera_forward_[3]  = {0.f, 0.f, 1.f};
    // Where the eye stands this frame a NiBillboardNode that faces the camera needs it
    float                 camera_position_[3] = {0.f, 0.f, 0.f};
    ToonProgram           toon_;
    GlowPass              glow_;
    bgfx::ProgramHandle   program_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle   skinned_program_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   diffuse_sampler_  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   coverage_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   splat_params_     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   shade_emissive_   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   shade_ambient_    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   shade_alpha_      = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   shade_diffuse_    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   sun_direction_    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   sun_colour_       = BGFX_INVALID_HANDLE;
    SunLight              sun_;
    bgfx::UniformHandle   ambient_colour_     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   fog_colour_uniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   fog_range_uniform_  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   uv_rows_uniform_    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   uv_offset_uniform_  = BGFX_INVALID_HANDLE;
    // The detail map stage its sampler its matrix and the switch that turns it on
    bgfx::UniformHandle   detail_sampler_     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   detail_rows_uniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   detail_offset_uniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   detail_params_uniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   bone_rows_uniform_  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   environment_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   environment_row_u_  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   environment_row_v_  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   eye_position_       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   model_light_direction_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle   model_light_colour_ = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout    layout_;
    bgfx::VertexLayout    skinned_layout_;
    LayerBuffers          layers_[kSceneLayerCount];
    // Ground source not fold one layer drawn on layer after mesh
    struct GroundPatchBuffers {
        LayerBuffers buffers;
        SceneLayer   layer = SceneLayer::Terrain;
    };
    std::vector<GroundPatchBuffers> ground_patches_;
    ViewportRect          viewport_;
    std::array<bool, kSceneLayerCount> layer_visible_{};
    // Parallel to MapScene prop models indexed by PropInstance model index
    std::vector<PropModelBuffers> prop_models_;
    std::vector<PropInstance> prop_instances_;
    // Placements uploaded scene owns past appended live session replaced
    std::size_t scene_prop_instances_ = 0;
    std::vector<CharacterBuffers>  characters_;
    std::vector<CharacterInstance> character_instances_;
    // Parallel to character instances filled placements carry clock own rest
    std::vector<SkinnedPose>       instance_poses_;
    // Refilled every frame not reallocated populated tile queues few thousand
    std::vector<TranslucentDraw> translucent_;
    std::vector<SkyPhaseBuffers> sky_phases_;
    // Phase each hour shows copied scene uploaded from
    std::array<uint8_t, kHoursPerDay> sky_phase_of_hour_{};
    DayNightCycle         cycle_;
    AnimationClock        animation_;
    int                   start_hour_ = kDefaultHour;
    // Minus 1 leaves every spawn on clip placed with
    int                   character_clip_ = -1;
    // Minus 1 lets the hour pick the sky phase else the index the caller asked for
    int                   sky_phase_ = -1;
    // Tile node asked fog colour clock
    bool                  node_fogged_ = false;
    std::array<FogRamp, kHoursPerDay> world_fog_of_hour_{};
    DepthRange            world_range_;
    // The lens of the frame radians the horizontal zero means the viewport aspect picks it
    float                 fov_vertical_ = kDefaultFieldOfViewRadians;
    float                 fov_horizontal_ = 0.f;
    bool                  fog_enabled_ = true;
    // The view the prop and particle submits go to draw sets the scene one a hud scene its own
    bgfx::ViewId          target_view_ = 2;
    // The view the sky dome goes to a world inset takes its own
    bgfx::ViewId          sky_view_ = 1;
    // A hud scene takes no fog and no sun the count of them drawn since the last frame
    bool                  hud_pass_ = false;
    HourColour            hud_ambient_;
    int                   hud_scenes_drawn_ = 0;
    bool                  baked_shading_ = true;
    bool                  cel_shading_ = false;
    bool                  glow_enabled_ = true;
    uint32_t              reset_flags_ = 0;
    uint16_t              width_  = 0;
    uint16_t              height_ = 0;
    bool                  started_ = false;
};

}
