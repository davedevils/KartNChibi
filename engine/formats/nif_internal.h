#pragma once
// Shared primitives NIF block decoders one family per translation unit
#include "engine/formats/nif_reader.h"

#include <cstring>
#include <string>
#include <vector>

namespace KnC::nif {

// Versions stored one byte per component most significant first
constexpr uint32_t make_version(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(c) << 8) | d;
}

constexpr uint32_t kEndianFlagFrom   = make_version(20, 0, 0, 3);
constexpr uint32_t kUserVersionFrom  = make_version(10, 0, 1, 8);
constexpr uint32_t kRttiTableFrom    = make_version(5, 0, 0, 1);
constexpr uint32_t kSizeTableFrom    = make_version(20, 2, 0, 5);
constexpr uint32_t kStringTableFrom  = make_version(20, 1, 0, 1);
constexpr uint32_t kObjectGroupsFrom = make_version(5, 0, 0, 6);
constexpr uint32_t kMetadataFrom     = make_version(20, 9, 0, 1);

// At exactly 20 3 1 2 RTTI table 32 bit type hash 4916 streams
constexpr uint32_t kTypeHashesAt = make_version(20, 3, 1, 2);

// Two enum members left stream at 20 1 0 2 10 2 0 0 writes
constexpr uint32_t kPropertyEnumsUntil = make_version(20, 1, 0, 2);

// NiDynamicEffect on off switch Gamebryo 4 light range 30 3 0 5
constexpr uint32_t kDynamicEffectSwitchFrom = make_version(10, 1, 0, 102);
constexpr uint32_t kLightRangeFrom          = make_version(30, 3, 0, 5);

// Three gates NiGeometry data block turn on nif encode writes fields
constexpr uint32_t kGeometryGroupIdFrom        = make_version(10, 1, 0, 114);
constexpr uint32_t kAdditionalGeometryDataFrom = make_version(10, 3, 0, 7);
constexpr uint32_t kGeometryDirtyFlagFrom      = make_version(20, 2, 0, 7);

// Name string longer than lost stream 4096 tight 20644 byte UserPropBuffer
constexpr uint32_t kMaxNameLength = 65536;
// Link count limit likewise
constexpr uint32_t kMaxLinks = 65536;

// Bounds checked forward reader file memory every take false past
class Cursor {
public:
    explicit Cursor(const std::vector<char>& bytes) : bytes_(bytes) {}

    bool take(void* destination, size_t count) {
        if (position_ + count > bytes_.size()) return false;
        std::memcpy(destination, bytes_.data() + position_, count);
        position_ += count;
        return true;
    }

    bool take_u8(uint8_t& value)   { return take(&value, sizeof(value)); }
    bool take_u16(uint16_t& value) { return take(&value, sizeof(value)); }
    bool take_u32(uint32_t& value) { return take(&value, sizeof(value)); }
    bool take_f32(float& value)    { return take(&value, sizeof(value)); }

    // Length prefixed not NUL terminated on disk
    bool take_string(std::string& out) {
        uint32_t length = 0;
        if (!take_u32(length) || length > kMaxNameLength) return false;
        out.resize(length);
        return length == 0 || take(out.data(), length);
    }

    bool skip(size_t count) {
        if (position_ + count > bytes_.size()) return false;
        position_ += count;
        return true;
    }

    uint64_t position() const { return position_; }
    uint64_t remaining() const { return bytes_.size() - position_; }
    void seek(uint64_t position) { position_ = position; }

private:
    const std::vector<char>& bytes_;
    uint64_t position_ = 0;
};

// Name length prefixed below 20 1 0 1 string table index above
bool read_object_name(Cursor& cursor, const NifHeader& header, std::string& out);
// Same also yielding table entry kNoLink below 20 1 0 1
bool read_object_name(Cursor& cursor, const NifHeader& header, std::string& out,
                      uint32_t& index_out);
// uint32 count followed by that many uint32 link ids
bool read_links(Cursor& cursor, std::vector<uint32_t>& out);
bool skip_link(Cursor& cursor);
// Bare array known length sized filled in one read
bool take_u16_array(Cursor& cursor, size_t count, std::vector<uint16_t>& out);
bool take_f32_array(Cursor& cursor, size_t count, std::vector<float>& out);
// Enums 32 bits on wire
bool skip_enum(Cursor& cursor);

// Below 20 3 1 0 byte front vertex array bool 343516 AK blocks
enum class NifComponentFormat : uint8_t {
    Absent       = 0x00,
    // Float32 four bytes SignedByte one byte over 127 packed normals UnsignedByte one byte over 255 packed colours Float16 two bytes
    Float32      = 0x01,
    SignedByte   = 0x06,
    UnsignedByte = 0x07,
    Float16      = 0x0F,
};

constexpr uint32_t kComponentFormatFrom = make_version(20, 3, 1, 0);

size_t component_width(NifComponentFormat format);
// Reads format byte below 20 3 1 0 non zero float unrecognised
bool take_component_format(Cursor& cursor, const NifHeader& header,
                           NifComponentFormat& format);
// Same yielding byte below 20 3 1 0 non zero Float32 encoder
bool take_component_format(Cursor& cursor, const NifHeader& header,
                           NifComponentFormat& format, uint8_t& raw_format);
// Array written width format appended exact inverse read component
void write_component_array(NifComponentFormat format, const std::vector<float>& values,
                           std::string& out);
// One array whatever width format declared expanded floats consumer
bool read_component_array(Cursor& cursor, NifComponentFormat format, size_t count,
                          std::vector<float>& out);
// One float same array record interleaves it with index
bool take_component(Cursor& cursor, NifComponentFormat format, float& out);
bool skip_components(Cursor& cursor, NifComponentFormat format, size_t count);

// Name whose Gamebryo hash is hash or hash XXXXXXXX when no
std::string type_name_for_hash(uint32_t hash);

// NiObjectNET name extra data links controller link
bool read_object_net(Cursor& cursor, const NifHeader& header, NifBlock& block);
// NiAVObject flags 16 bits translation 3x3 rotation scale property
bool read_av_object(Cursor& cursor, const NifHeader& header, NifBlock& block);
// NiNode NiAVObject plus child effect links
bool read_node(Cursor& cursor, const NifHeader& header, NifBlock& block);
// NiAVObject NiDynamicEffect NiLight one NiAmbientLight NiDirectionalLight
bool read_light(Cursor& cursor, const NifHeader& header, NifBlock& block,
                bool has_affected_node_list);

// NiTimeController next link uint16 flags four floats target 26 B
bool read_time_controller(Cursor& cursor, NifBlock& block);
// NiSingleInterpController above plus interpolator link
bool read_single_interp_controller(Cursor& cursor, NifBlock& block);
// Block animation payload allocated on first use
NifAnimation& animation_of(NifBlock& block);

// What one key holds decides stride interpolations format streams
enum class KeyContent { Float, Point, Quaternion, Colour };

// Count then interpolation enum that many keys empty no enum
bool read_key_group(Cursor& cursor, KeyContent content, NifKeyGroup& group);
// NiBoolData keys time one byte NiBool kept 0 0 or 1 0
bool read_bool_key_group(Cursor& cursor, NifKeyGroup& group);
// Rotations quaternion keys or XYZ one float channel per axis
bool read_rotation_key_group(Cursor& cursor, const NifHeader& header,
                             NifAnimation& animation);

bool read_texturing_property(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_z_buffer_property(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_vertex_colour_property(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_alpha_property(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_material_property(Cursor& cursor, const NifHeader& header, NifBlock& block);
// Block render state payload allocated on first use
NifRenderState& render_state_of(NifBlock& block);
// Payload values read from allocated first use four families glTF
NifPropertySpans& property_spans_of(NifBlock& block);
bool read_source_texture(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_stencil_property(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_specular_property(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_dither_property(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_shade_property(Cursor& cursor, const NifHeader& header, NifBlock& block);

// Append triangle strip as triangles corners alternate winding repeated
void expand_strip(const uint16_t* strip, size_t length, std::vector<uint16_t>& out);

bool read_tri_shape_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_tri_strips_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_range_lod_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_lines_data(Cursor& cursor, const NifHeader& header, NifBlock& block);

bool read_transform_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_transform_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_transform_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_float_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_float_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_alpha_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_texture_transform_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_flip_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_geom_morpher_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_morph_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_look_at_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_path_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_pos_data(Cursor& cursor, const NifHeader& header, NifBlock& block);

bool read_colour_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_float_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_integer_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block);

// kf animation stream what made of
bool read_controller_sequence(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_text_key_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_bool_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_bool_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_bspline_comp_transform_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_bspline_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_bspline_basis_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
// Below 10 1 0 104 sequence no window cycle rate controllers hold
void resolve_legacy_sequence_timing(NifScene& scene);

bool read_string_palette(Cursor& cursor, const NifHeader& header, NifBlock& block);

// Block geometry source payload allocated first NiGeometry NiGeometryData
NifGeometrySource& geometry_source_of(NifBlock& block);

// Block particle payloads allocated on first use
NifParticleSystem& particle_system_of(NifBlock& block);
NifPSysModifier& psys_modifier_of(NifBlock& block);

bool read_particle_system(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_mesh_particle_system(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_particle_system_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);

// PhysX snapshot stream carry read length scene name neither retains
bool read_physx_scene(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_physx_scene_desc(Cursor& cursor, const NifHeader& header, NifBlock& block);

bool read_camera(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_directional_light(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_deferred_point_light(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_point_light(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_spot_light(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_deferred_light_dimmer_controller(Cursor& cursor, const NifHeader& header,
                                           NifBlock& block);
bool read_material_colour_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_multi_target_transform_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_boolean_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_integers_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_bspline_comp_float_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_colour_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_keyframe_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_keyframe_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_emitter_ctlr_data(Cursor& cursor, const NifHeader& header, NifBlock& block);

bool read_psys_age_death_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_spawn_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_position_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_bound_update_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_grow_fade_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_colour_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_mesh_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_box_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_sphere_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_cylinder_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_rotation_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_gravity_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_collider_manager(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_planar_collider(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_bomb_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_mesh_update_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_mesh_psys_data(Cursor& cursor, const NifHeader& header, NifBlock& block);

bool read_psys_emitter_ctlr(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_update_ctlr(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_modifier_float_ctlr(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_reset_on_loop_ctlr(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_psys_modifier_active_ctlr(Cursor& cursor, const NifHeader& header, NifBlock& block);

bool read_ambient_light(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_pixel_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_point3_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_persistent_src_texture_renderer_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_colour_data(Cursor& cursor, const NifHeader& header, NifBlock& block);

bool read_skin_instance(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_sort_adjust_node(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_room_group(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_skin_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_room(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_skin_partition(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_portal(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_texture_effect(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_vis_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_vis_data(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_extra_data_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_light_colour_controller(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_bool_timeline_interpolator(Cursor& cursor, const NifHeader& header, NifBlock& block);
bool read_collision_data(Cursor& cursor, const NifHeader& header, NifBlock& block);

}
