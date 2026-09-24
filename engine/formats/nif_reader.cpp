#include "engine/formats/nif_reader.h"
#include "engine/formats/nif_internal.h"
#include "engine/formats/nif_scene_graph.h"

#include <fstream>
#include <memory>
#include <string>

namespace KnC {

using namespace nif;

namespace {

bool read_format_line(const std::vector<char>& bytes, uint64_t& after_line) {
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] != '\n') continue;
        const std::string line(bytes.data(), i);
        if (line.find("File Format") == std::string::npos) return false;
        after_line = i + 1;
        return true;
    }
    return false;
}

// Names at 20 3 1 2 hashes not strings see nif type hashes cpp
bool read_rtti_names(Cursor& cursor, NifHeader& out) {
    if (out.version != kTypeHashesAt) {
        for (std::string& name : out.type_names)
            if (!cursor.take_string(name)) return false;
        return true;
    }
    for (std::string& name : out.type_names) {
        uint32_t hash = 0;
        if (!cursor.take_u32(hash)) return false;
        name = type_name_for_hash(hash);
    }
    return true;
}

bool read_rtti_table(Cursor& cursor, NifHeader& out) {
    uint16_t type_count = 0;
    if (!cursor.take_u16(type_count)) return false;
    out.type_names.resize(type_count);
    if (!read_rtti_names(cursor, out)) return false;

    // From 20 2 0 5 high bit marks block reader may skip
    const bool has_skippable_flag = out.version >= kSizeTableFrom;
    constexpr uint16_t kSkippableMask = 0x8000;

    out.object_types.resize(out.object_count);
    out.object_skippable.assign(out.object_count, false);
    for (uint32_t i = 0; i < out.object_count; ++i) {
        uint16_t index = 0;
        if (!cursor.take_u16(index)) return false;
        if (has_skippable_flag && (index & kSkippableMask) != 0) {
            out.object_skippable[i] = true;
            index &= static_cast<uint16_t>(~kSkippableMask);
        }
        if (index >= type_count) return false;
        out.object_types[i] = index;
    }
    return true;
}

bool read_string_table(Cursor& cursor, NifHeader& out) {
    out.strings_offset = cursor.position();
    uint32_t string_count = 0;
    if (!cursor.take_u32(string_count) || !cursor.take_u32(out.max_string_length)) return false;
    if (out.max_string_length > kMaxNameLength) return false;
    out.strings.resize(string_count);
    for (std::string& value : out.strings)
        if (!cursor.take_string(value) || value.size() > out.max_string_length) return false;
    out.strings_end_offset = cursor.position();
    return true;
}

bool skip_object_groups(Cursor& cursor) {
    uint32_t group_count = 0;
    if (!cursor.take_u32(group_count)) return false;
    return cursor.skip(static_cast<size_t>(group_count) * sizeof(uint32_t));
}

// Fills header leaves cursor first block body error names section
bool parse_header(const std::vector<char>& bytes, NifHeader& out, Cursor& cursor,
                  std::string& error) {
    uint64_t after_line = 0;
    if (!read_format_line(bytes, after_line)) {
        error = "header has no 'File Format' line";
        return false;
    }
    out = NifHeader{};
    cursor.seek(after_line);

    uint32_t raw_version = 0;
    if (!cursor.take_u32(raw_version)) {
        error = "header ends before its version";
        return false;
    }
    // On disk components stored least significant first
    out.version = make_version(static_cast<uint8_t>(raw_version >> 24),
                               static_cast<uint8_t>(raw_version >> 16),
                               static_cast<uint8_t>(raw_version >> 8),
                               static_cast<uint8_t>(raw_version));

    if (out.version >= kEndianFlagFrom) {
        uint8_t little_endian = 1;
        if (!cursor.take_u8(little_endian)) {
            error = "header ends before its endian flag";
            return false;
        }
        out.little_endian = little_endian != 0;
    }
    // Every take memcpy big endian stream byte swapped garbage refuse
    if (!out.little_endian) {
        error = "stream declares big-endian, which this reader does not decode";
        return false;
    }
    if (out.version >= kUserVersionFrom && !cursor.take_u32(out.user_version)) {
        error = "header ends before its user version";
        return false;
    }
    if (!cursor.take_u32(out.object_count)) {
        error = "header ends before its object count";
        return false;
    }

    // Metadata above X Legend ships refuse not pretend layout verified
    if (out.version >= kMetadataFrom) {
        error = "stream carries a metadata section, whose layout is unverified here";
        return false;
    }

    if (out.version >= kRttiTableFrom && !read_rtti_table(cursor, out)) {
        error = "block type table does not parse";
        return false;
    }
    if (out.version >= kSizeTableFrom) {
        out.object_sizes_offset = cursor.position();
        out.object_sizes.resize(out.object_count);
        for (uint32_t& size : out.object_sizes)
            if (!cursor.take_u32(size)) {
                error = "block size table does not parse";
                return false;
            }
    }
    if (out.version >= kStringTableFrom && !read_string_table(cursor, out)) {
        error = "fixed string table does not parse";
        return false;
    }
    if (out.version >= kObjectGroupsFrom && !skip_object_groups(cursor)) {
        error = "object group table does not parse";
        return false;
    }

    out.body_offset = cursor.position();
    if (out.body_offset < bytes.size()) return true;
    error = "header runs to the end of the stream, leaving no block bodies";
    return false;
}

bool load_file(const std::string& path, std::vector<char>& bytes) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input.is_open()) return false;
    bytes.resize(static_cast<size_t>(input.tellg()));
    input.seekg(0, std::ios::beg);
    return static_cast<bool>(
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())));
}

} // namespace

namespace nif {

bool skip_link(Cursor& cursor) {
    uint32_t link = 0;
    return cursor.take_u32(link);
}

bool skip_enum(Cursor& cursor) {
    uint32_t value = 0;
    return cursor.take_u32(value);
}

bool read_object_name(Cursor& cursor, const NifHeader& header, std::string& out) {
    uint32_t index = kNoLink;
    return read_object_name(cursor, header, out, index);
}

bool read_object_name(Cursor& cursor, const NifHeader& header, std::string& out,
                      uint32_t& index_out) {
    index_out = kNoLink;
    if (header.version < kStringTableFrom) return cursor.take_string(out);
    if (!cursor.take_u32(index_out)) return false;
    if (index_out == kNoLink) { out.clear(); return true; }
    if (index_out >= header.strings.size()) return false;
    out = header.strings[index_out];
    return true;
}

bool read_links(Cursor& cursor, std::vector<uint32_t>& out) {
    uint32_t count = 0;
    if (!cursor.take_u32(count) || count > kMaxLinks) return false;
    out.resize(count);
    for (uint32_t& link : out)
        if (!cursor.take_u32(link)) return false;
    return true;
}

// Sized before allocating desync parse count no file hold fail
bool take_u16_array(Cursor& cursor, size_t count, std::vector<uint16_t>& out) {
    if (count > cursor.remaining() / sizeof(uint16_t)) return false;
    out.resize(count);
    return count == 0 || cursor.take(out.data(), count * sizeof(uint16_t));
}

bool take_f32_array(Cursor& cursor, size_t count, std::vector<float>& out) {
    if (count > cursor.remaining() / sizeof(float)) return false;
    out.resize(count);
    return count == 0 || cursor.take(out.data(), count * sizeof(float));
}

bool read_object_net(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    // Name position splice decoder knows same text elsewhere body
    block.name_offset = cursor.position() - block.byte_offset;
    block.has_name = true;
    return read_object_name(cursor, header, block.name, block.name_index) &&
           read_links(cursor, block.extra_data) &&
           cursor.take_u32(block.controller_link);
}

bool read_av_object(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    // 16 bits not 32 measured 32 bit read rotation two bytes scale
    if (!cursor.take_u16(block.object_flags)) return false;
    block.transform_offset = cursor.position() - block.byte_offset;
    if (!cursor.take(block.transform.translation, sizeof(block.transform.translation)))
        return false;
    if (!cursor.take(block.transform.rotation, sizeof(block.transform.rotation)))
        return false;
    if (!cursor.take(&block.transform.scale, sizeof(block.transform.scale))) return false;
    block.has_transform = true;
    return read_links(cursor, block.properties) && cursor.take_u32(block.collision_link);
}

// NiDynamicEffect switch node list NiLight range dimmer colors
bool read_light(Cursor& cursor, const NifHeader& header, NifBlock& block,
                bool has_affected_node_list) {
    if (!read_av_object(cursor, header, block)) return false;
    if (!block.light) block.light = std::make_shared<NifLight>();
    NifLight& light = *block.light;
    if (header.version >= kDynamicEffectSwitchFrom && !cursor.take_u8(light.is_enabled))
        return false;
    if (has_affected_node_list && !read_links(cursor, light.unaffected_nodes)) return false;
    if (header.version >= kLightRangeFrom &&
        (!cursor.take(light.range, sizeof(light.range)) ||
         !cursor.take_u8(light.group_mask)))
        return false;
    return cursor.take_f32(light.dimmer) &&
           cursor.take(light.ambient, sizeof(light.ambient)) &&
           cursor.take(light.diffuse, sizeof(light.diffuse)) &&
           cursor.take(light.specular, sizeof(light.specular));
}

// NiStringPalette allocated buffer NUL separated names length every SDK
bool read_string_palette(Cursor& cursor, const NifHeader&, NifBlock& block) {
    uint32_t allocated_size = 0;
    if (!cursor.take_u32(allocated_size) || allocated_size > (1u << 24)) return false;
    block.string_palette.resize(allocated_size);
    uint32_t end_of_buffer = 0;
    return (allocated_size == 0 || cursor.take(block.string_palette.data(), allocated_size)) &&
           cursor.take_u32(end_of_buffer);
}

// Palette string offset to next NUL buffer
std::string palette_string(const std::string& palette, uint32_t offset) {
    if (offset >= palette.size()) return std::string();
    const size_t end = palette.find('\0', offset);
    return palette.substr(offset, end == std::string::npos ? std::string::npos : end - offset);
}

// Palette block written after sequence names resolved once every block
void resolve_sequence_names(NifScene& scene) {
    for (NifBlock& block : scene.blocks) {
        if (!block.animation) continue;
        for (NifSequenceEntry& entry : block.animation->sequence.entries) {
            if (entry.palette_link >= scene.blocks.size()) continue;
            entry.target_name = palette_string(scene.blocks[entry.palette_link].string_palette,
                                               entry.target_name_offset);
        }
    }
}


NifAnimation& animation_of(NifBlock& block) {
    if (!block.animation) block.animation = std::make_shared<NifAnimation>();
    return *block.animation;
}

bool read_time_controller(Cursor& cursor, NifBlock& block) {
    NifController& controller = animation_of(block).controller;
    return cursor.take_u32(controller.next_link) &&
           cursor.take_u16(controller.flags) &&
           cursor.take_f32(controller.frequency) &&
           cursor.take_f32(controller.phase) &&
           cursor.take_f32(controller.start_time) &&
           cursor.take_f32(controller.stop_time) &&
           cursor.take_u32(controller.target_link);
}

bool read_single_interp_controller(Cursor& cursor, NifBlock& block) {
    return read_time_controller(cursor, block) &&
           cursor.take_u32(animation_of(block).controller.interpolator_link);
}
bool read_node(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_av_object(cursor, header, block)) return false;
    return read_links(cursor, block.children) && read_links(cursor, block.effects);
}

bool read_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    return read_object_name(cursor, header, block.name);
}

bool read_string_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    std::string value;
    return read_extra_data(cursor, header, block) &&
           read_object_name(cursor, header, value);
}

// NiGeometry NiTriShape NiTriStrips descendants
bool read_geometry(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_av_object(cursor, header, block)) return false;
    if (!cursor.take_u32(block.data_link)) return false;
    if (!cursor.take_u32(block.skin_instance_link)) return false;
    NifGeometrySource& source = geometry_source_of(block);
    if (header.version >= kSizeTableFrom) {
        uint32_t material_count = 0;
        if (!cursor.take_u32(material_count) || material_count > kMaxLinks) return false;
        source.materials.resize(material_count);
        for (NifGeometryMaterial& material : source.materials)
            if (!read_object_name(cursor, header, material.name, material.name_index) ||
                !cursor.take_u32(material.extra_data)) return false;
        if (!cursor.take_u32(source.active_material)) return false;
    } else {
        // Below 20 2 0 5 optional shader not material Gamebryo 4 dropped
        if (!cursor.take_u8(source.has_shader)) return false;
        if (source.has_shader &&
            (!read_object_name(cursor, header, source.shader_name, source.shader_name_index) ||
             !cursor.take_u32(source.shader_implementation))) return false;
    }
    if (header.version < kGeometryDirtyFlagFrom) return true;
    return cursor.take_u8(source.is_dirty);   // NiBool is one byte at every version
}

// From 10 1 0 106 the mode is its own word below that the NiAVObject flags carry it
bool read_billboard_node(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_node(cursor, header, block)) return false;
    if (!cursor.take_u16(block.billboard_mode)) return false;
    block.has_billboard = true;
    return true;
}

bool read_switch_node(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_node(cursor, header, block)) return false;
    uint16_t flags = 0;
    uint32_t active_index = 0;
    return cursor.take_u16(flags) && cursor.take_u32(active_index);
}

bool read_lod_node(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    return read_switch_node(cursor, header, block) && skip_link(cursor);
}

} // namespace nif

namespace {

// One table adding decoder single line definition missing separate
bool has_decoder(const std::string& type);

bool dispatch(const std::string& type, Cursor& cursor,
              const NifHeader& header, NifBlock& block) {
    if (type == "NiNode")                return read_node(cursor, header, block);
    if (type == "NiZBufferProperty")     return read_z_buffer_property(cursor, header, block);
    if (type == "NiVertexColorProperty") return read_vertex_colour_property(cursor, header, block);
    if (type == "NiAlphaProperty")       return read_alpha_property(cursor, header, block);
    if (type == "NiMaterialProperty")    return read_material_property(cursor, header, block);
    if (type == "NiTriShape" || type == "NiTriStrips")
                                         return read_geometry(cursor, header, block);
    if (type == "NiStringExtraData")     return read_string_extra_data(cursor, header, block);
    if (type == "NiBillboardNode")       return read_billboard_node(cursor, header, block);
    if (type == "NiSwitchNode")          return read_switch_node(cursor, header, block);
    if (type == "NiLODNode")             return read_lod_node(cursor, header, block);
    if (type == "NiColorExtraData")      return read_colour_extra_data(cursor, header, block);
    if (type == "NiFloatExtraData")      return read_float_extra_data(cursor, header, block);
    if (type == "NiIntegerExtraData")    return read_integer_extra_data(cursor, header, block);
    if (type == "NiTexturingProperty")   return read_texturing_property(cursor, header, block);
    if (type == "NiSourceTexture")       return read_source_texture(cursor, header, block);
    if (type == "NiStencilProperty")     return read_stencil_property(cursor, header, block);
    if (type == "NiSpecularProperty")    return read_specular_property(cursor, header, block);
    if (type == "NiDitherProperty")      return read_dither_property(cursor, header, block);
    if (type == "NiTriShapeData") return read_tri_shape_data(cursor, header, block);
    if (type == "NiTriStripsData") return read_tri_strips_data(cursor, header, block);
    if (type == "NiRangeLODData") return read_range_lod_data(cursor, header, block);
    if (type == "NiLines") return read_geometry(cursor, header, block);
    if (type == "NiLinesData") return read_lines_data(cursor, header, block);
    if (type == "NiShadeProperty") return read_shade_property(cursor, header, block);
    if (type == "NiCollisionData") return read_collision_data(cursor, header, block);
    if (type == "NiPSysSphereEmitter") return read_psys_sphere_emitter(cursor, header, block);
    if (type == "NiPSysCylinderEmitter") return read_psys_cylinder_emitter(cursor, header, block);
    if (type == "NiTransformController") return read_transform_controller(cursor, header, block);
    if (type == "NiTransformInterpolator") return read_transform_interpolator(cursor, header, block);
    if (type == "NiTransformData") return read_transform_data(cursor, header, block);
    if (type == "NiFloatInterpolator") return read_float_interpolator(cursor, header, block);
    if (type == "NiFloatData") return read_float_data(cursor, header, block);
    if (type == "NiAlphaController") return read_alpha_controller(cursor, header, block);
    if (type == "NiTextureTransformController") return read_texture_transform_controller(cursor, header, block);
    if (type == "NiFlipController") return read_flip_controller(cursor, header, block);
    if (type == "NiGeomMorpherController") return read_geom_morpher_controller(cursor, header, block);
    if (type == "NiMorphData") return read_morph_data(cursor, header, block);
    if (type == "NiLookAtInterpolator") return read_look_at_interpolator(cursor, header, block);
    if (type == "NiPathInterpolator") return read_path_interpolator(cursor, header, block);
    if (type == "NiPosData") return read_pos_data(cursor, header, block);
    if (type == "NiCamera") return read_camera(cursor, header, block);
    if (type == "NiDirectionalLight") return read_directional_light(cursor, header, block);
    if (type == "NiDeferredPointLight") return read_deferred_point_light(cursor, header, block);
    if (type == "NiPointLight") return read_point_light(cursor, header, block);
    if (type == "NiSpotLight") return read_spot_light(cursor, header, block);
    if (type == "NiPhysXScene") return read_physx_scene(cursor, header, block);
    if (type == "NiPhysXSceneDesc") return read_physx_scene_desc(cursor, header, block);
    if (type == "NiDeferredLightDimmerController") return read_deferred_light_dimmer_controller(cursor, header, block);
    if (type == "NiMaterialColorController") return read_material_colour_controller(cursor, header, block);
    if (type == "NiMultiTargetTransformController") return read_multi_target_transform_controller(cursor, header, block);
    if (type == "NiBooleanExtraData") return read_boolean_extra_data(cursor, header, block);
    if (type == "NiIntegersExtraData") return read_integers_extra_data(cursor, header, block);
    if (type == "NiControllerSequence") return read_controller_sequence(cursor, header, block);
    if (type == "NiTextKeyExtraData") return read_text_key_extra_data(cursor, header, block);
    if (type == "NiBoolInterpolator") return read_bool_interpolator(cursor, header, block);
    if (type == "NiBoolData") return read_bool_data(cursor, header, block);
    if (type == "NiBSplineCompTransformInterpolator") return read_bspline_comp_transform_interpolator(cursor, header, block);
    if (type == "NiBSplineData") return read_bspline_data(cursor, header, block);
    if (type == "NiBSplineBasisData") return read_bspline_basis_data(cursor, header, block);
    if (type == "NiStringPalette") return read_string_palette(cursor, header, block);
    if (type == "NiParticleSystem") return read_particle_system(cursor, header, block);
    if (type == "NiMeshParticleSystem") return read_mesh_particle_system(cursor, header, block);
    if (type == "NiPSysData") return read_particle_system_data(cursor, header, block);
    if (type == "NiPSysModifier") return read_psys_modifier(cursor, header, block);
    if (type == "NiPSysEmitterCtlr") return read_psys_emitter_ctlr(cursor, header, block);
    if (type == "NiPSysUpdateCtlr") return read_psys_update_ctlr(cursor, header, block);
    if (type == "NiPSysAgeDeathModifier") return read_psys_age_death_modifier(cursor, header, block);
    if (type == "NiPSysSpawnModifier") return read_psys_spawn_modifier(cursor, header, block);
    if (type == "NiPSysPositionModifier") return read_psys_position_modifier(cursor, header, block);
    if (type == "NiPSysBoundUpdateModifier") return read_psys_bound_update_modifier(cursor, header, block);
    if (type == "NiPSysGrowFadeModifier") return read_psys_grow_fade_modifier(cursor, header, block);
    if (type == "NiPSysColorModifier") return read_psys_colour_modifier(cursor, header, block);
    if (type == "NiPSysMeshEmitter") return read_psys_mesh_emitter(cursor, header, block);
    if (type == "NiPSysBoxEmitter") return read_psys_box_emitter(cursor, header, block);
    if (type == "NiPSysRotationModifier") return read_psys_rotation_modifier(cursor, header, block);
    if (type == "NiPSysGravityModifier") return read_psys_gravity_modifier(cursor, header, block);
    if (type == "NiPSysColliderManager") return read_psys_collider_manager(cursor, header, block);
    if (type == "NiPSysPlanarCollider") return read_psys_planar_collider(cursor, header, block);
    if (type == "NiPSysBombModifier") return read_psys_bomb_modifier(cursor, header, block);
    if (type == "NiPSysMeshUpdateModifier") return read_psys_mesh_update_modifier(cursor, header, block);
    if (type == "NiMeshPSysData") return read_mesh_psys_data(cursor, header, block);
    if (type == "NiPoint3Interpolator") return read_point3_interpolator(cursor, header, block);
    if (type == "NiColorData") return read_colour_data(cursor, header, block);
    if (type == "NiAmbientLight") return read_ambient_light(cursor, header, block);
    if (type == "NiPixelData") return read_pixel_data(cursor, header, block);
    if (type == "NiPersistentSrcTextureRendererData") return read_persistent_src_texture_renderer_data(cursor, header, block);
    if (type == "NiSkinInstance") return read_skin_instance(cursor, header, block);
    if (type == "NiSkinData") return read_skin_data(cursor, header, block);
    if (type == "NiSkinPartition") return read_skin_partition(cursor, header, block);
    if (type == "NiPortal") return read_portal(cursor, header, block);
    if (type == "NiTextureEffect") return read_texture_effect(cursor, header, block);
    if (type == "NiVisController") return read_vis_controller(cursor, header, block);
    if (type == "NiVisData") return read_vis_data(cursor, header, block);
    if (type == "NiColorExtraDataController") return read_extra_data_controller(cursor, header, block);
    if (type == "NiFloatExtraDataController") return read_extra_data_controller(cursor, header, block);
    if (type == "NiLightColorController") return read_light_colour_controller(cursor, header, block);
    if (type == "NiRoom") return read_room(cursor, header, block);
    if (type == "NiSortAdjustNode") return read_sort_adjust_node(cursor, header, block);
    if (type == "NiRoomGroup") return read_room_group(cursor, header, block);
    if (type == "NiBoolTimelineInterpolator") return read_bool_timeline_interpolator(cursor, header, block);
    if (type == "NiBSplineCompFloatInterpolator") return read_bspline_comp_float_interpolator(cursor, header, block);
    if (type == "NiColorInterpolator") return read_colour_interpolator(cursor, header, block);
    if (type == "NiKeyframeController") return read_keyframe_controller(cursor, header, block);
    if (type == "NiKeyframeData") return read_keyframe_data(cursor, header, block);
    if (type == "NiPSysEmitterCtlrData") return read_psys_emitter_ctlr_data(cursor, header, block);
    if (type == "NiPSysResetOnLoopCtlr") return read_psys_reset_on_loop_ctlr(cursor, header, block);
    if (type == "NiPSysModifierActiveCtlr") return read_psys_modifier_active_ctlr(cursor, header, block);
    // Eighteen controllers pass throughs same base
    if (type == "NiPSysEmitterSpeedCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysEmitterLifeSpanCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysEmitterInitialRadiusCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysGravityStrengthCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysEmitterDeclinationCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysEmitterDeclinationVarCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysEmitterPlanarAngleCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysEmitterPlanarAngleVarCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysInitialRotAngleCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysInitialRotAngleVarCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysInitialRotSpeedCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysInitialRotSpeedVarCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysFieldMagnitudeCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysFieldAttenuationCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysFieldMaxDistanceCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysAirFieldAirFrictionCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysAirFieldInheritVelocityCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    if (type == "NiPSysAirFieldSpreadCtlr")
        return read_psys_modifier_float_ctlr(cursor, header, block);
    return false;
}

bool has_decoder(const std::string& type) {
    static const char* const kKnown[] = {
        "NiNode", "NiZBufferProperty", "NiVertexColorProperty", "NiAlphaProperty",
        "NiMaterialProperty", "NiTriShape", "NiTriStrips", "NiStringExtraData",
        "NiBillboardNode", "NiSwitchNode", "NiLODNode", "NiColorExtraData",
        "NiFloatExtraData", "NiIntegerExtraData", "NiTexturingProperty",
        "NiSourceTexture", "NiStencilProperty", "NiSpecularProperty", "NiDitherProperty",
        "NiTriShapeData", "NiTriStripsData", "NiRangeLODData",
        "NiLines", "NiLinesData", "NiShadeProperty", "NiCollisionData",
        "NiPSysSphereEmitter", "NiPSysCylinderEmitter", "NiTransformController", "NiTransformInterpolator", "NiTransformData", "NiFloatInterpolator", "NiFloatData", "NiAlphaController", "NiTextureTransformController", "NiFlipController", "NiGeomMorpherController", "NiMorphData", "NiLookAtInterpolator", "NiPathInterpolator", "NiPosData",
        "NiCamera", "NiDirectionalLight", "NiDeferredPointLight", "NiPointLight", "NiSpotLight", "NiDeferredLightDimmerController", "NiPhysXScene", "NiPhysXSceneDesc", "NiMaterialColorController", "NiMultiTargetTransformController", "NiBooleanExtraData", "NiIntegersExtraData",
        "NiControllerSequence", "NiTextKeyExtraData", "NiBoolInterpolator", "NiBoolData", "NiBSplineCompTransformInterpolator", "NiBSplineData", "NiBSplineBasisData", "NiStringPalette",
        "NiParticleSystem", "NiMeshParticleSystem", "NiPSysData", "NiPSysModifier",
        "NiPSysEmitterCtlr", "NiPSysUpdateCtlr", "NiPSysResetOnLoopCtlr", "NiPSysModifierActiveCtlr", "NiPSysEmitterSpeedCtlr", "NiPSysEmitterLifeSpanCtlr", "NiPSysEmitterInitialRadiusCtlr", "NiPSysGravityStrengthCtlr", "NiPSysEmitterDeclinationCtlr", "NiPSysEmitterDeclinationVarCtlr", "NiPSysEmitterPlanarAngleCtlr", "NiPSysEmitterPlanarAngleVarCtlr", "NiPSysInitialRotAngleCtlr", "NiPSysInitialRotAngleVarCtlr", "NiPSysInitialRotSpeedCtlr", "NiPSysInitialRotSpeedVarCtlr", "NiPSysFieldMagnitudeCtlr", "NiPSysFieldAttenuationCtlr", "NiPSysFieldMaxDistanceCtlr", "NiPSysAirFieldAirFrictionCtlr", "NiPSysAirFieldInheritVelocityCtlr", "NiPSysAirFieldSpreadCtlr",
        "NiPSysAgeDeathModifier", "NiPSysSpawnModifier", "NiPSysPositionModifier", "NiPSysBoundUpdateModifier", "NiPSysGrowFadeModifier", "NiPSysColorModifier", "NiPSysMeshEmitter", "NiPSysBoxEmitter", "NiPSysRotationModifier", "NiPSysGravityModifier", "NiPSysColliderManager", "NiPSysPlanarCollider", "NiPSysBombModifier", "NiPSysMeshUpdateModifier", "NiMeshPSysData",
        "NiPoint3Interpolator", "NiColorData", "NiAmbientLight", "NiPixelData", "NiPersistentSrcTextureRendererData",
        "NiSkinInstance", "NiSkinData", "NiRoom", "NiSkinPartition", "NiPortal", "NiTextureEffect", "NiVisController", "NiVisData",
        "NiColorExtraDataController", "NiFloatExtraDataController", "NiLightColorController", "NiSortAdjustNode", "NiRoomGroup", "NiBoolTimelineInterpolator", "NiBSplineCompFloatInterpolator", "NiColorInterpolator", "NiKeyframeController", "NiKeyframeData", "NiPSysEmitterCtlrData",
    };
    for (const char* known : kKnown)
        if (type == known) return true;
    return false;
}

// Footer closes stream root count one link per root
void read_footer_roots(Cursor& cursor, std::vector<uint32_t>& out) {
    uint32_t root_count = 0;
    if (!cursor.take_u32(root_count) || root_count > kMaxLinks) return;
    for (uint32_t index = 0; index < root_count; ++index) {
        uint32_t root = kNoLink;
        if (!cursor.take_u32(root)) {
            out.clear();
            return;
        }
        out.push_back(root);
    }
}

// Blocks stream order span cursor position before after 20 2 0 5
bool read_blocks(Cursor& cursor, NifScene& out, std::string& error) {
    out.blocks.resize(out.header.object_count);
    for (uint32_t i = 0; i < out.header.object_count; ++i) {
        NifBlock& block = out.blocks[i];
        block.type = out.header.type_names[out.header.object_types[i]];
        block.byte_offset = cursor.position();

        // NiObject LoadBinary streams group id below 10 1 0 114 block body
        if (out.header.version < kGeometryGroupIdFrom && !cursor.take_u32(block.object_group_id)) {
            error = "block " + std::to_string(i) + " has no group id";
            return false;
        }

        bool decoded = dispatch(block.type, cursor, out.header, block);
        if (!decoded && !has_decoder(block.type) && out.header.object_skippable[i] &&
            i < out.header.object_sizes.size()) {
            // No decoder header says length step over type bug hide
            cursor.seek(block.byte_offset);
            decoded = cursor.skip(out.header.object_sizes[i]);
            block.skipped = decoded;
        }

        if (!decoded) {
            error = "block " + std::to_string(i) + " of type '" + block.type +
                    "' at offset " + std::to_string(cursor.position()) +
                    (has_decoder(block.type) ? " FAILED to decode" : " has no decoder");
            return false;
        }
        block.byte_length = cursor.position() - block.byte_offset;

        // From 20 2 0 5 header states block length decoder stop wrong
        if (i < out.header.object_sizes.size() &&
            block.byte_length != out.header.object_sizes[i]) {
            error = "block " + std::to_string(i) + " of type '" + block.type +
                    "' MIS-SIZED: decoded " + std::to_string(block.byte_length) +
                    " bytes, header declares " +
                    std::to_string(out.header.object_sizes[i]);
            return false;
        }
    }
    return true;
}

} // namespace

namespace {

// Block property list changes inherited state names family first wins
NifSurfaceState composed_surface_state(const NifScene& scene, const NifBlock& block,
                                       NifSurfaceState state) {
    bool alpha_seen = false;
    bool depth_seen = false;
    bool material_seen = false;
    bool vertex_colour_seen = false;
    bool texturing_seen = false;
    for (const uint32_t property_link : block.properties) {
        if (property_link >= scene.blocks.size()) continue;
        const NifBlock& property = scene.blocks[property_link];
        // Draw mode 3 of a stencil property asks for both sides
        if (property.type == "NiStencilProperty") {
            if (property.has_stencil && property.stencil_draw_mode == 3) state.two_sided = true;
            continue;
        }
        if (!property.render_state) continue;
        if (property.type == "NiAlphaProperty" && !alpha_seen) {
            state.alpha = property.render_state->alpha;
            state.alpha_link = property_link;
            state.has_alpha = true;
            alpha_seen = true;
        } else if (property.type == "NiZBufferProperty" && !depth_seen) {
            state.depth = property.render_state->depth;
            state.depth_link = property_link;
            state.has_depth = true;
            depth_seen = true;
        } else if (property.type == "NiMaterialProperty" && !material_seen) {
            state.material = property.render_state->material;
            state.material_link = property_link;
            state.has_material = true;
            material_seen = true;
        } else if (property.type == "NiVertexColorProperty" && !vertex_colour_seen) {
            state.vertex_colour = property.render_state->vertex_colour;
            state.vertex_colour_link = property_link;
            state.has_vertex_colour = true;
            vertex_colour_seen = true;
        } else if (property.type == "NiTexturingProperty" && !texturing_seen) {
            state.texture_apply = property.render_state->texture_apply;
            state.base_texture_clamp = property.render_state->base_texture_clamp;
            state.base_texture_filter = property.render_state->base_texture_filter;
            state.texturing_link = property_link;
            texturing_seen = true;
        }
    }
    return state;
}

} // namespace

std::vector<NifSurfaceState> resolve_surface_states(const NifScene& scene) {
    std::vector<NifSurfaceState> states(scene.blocks.size());
    std::vector<uint8_t> visited(scene.blocks.size(), 0);
    std::vector<uint32_t> pending;
    for (const uint32_t root : find_root_block_indices(scene)) {
        if (root >= scene.blocks.size() || visited[root] != 0) continue;
        visited[root] = 1;
        states[root] = composed_surface_state(scene, scene.blocks[root], NifSurfaceState{});
        pending.push_back(root);
    }
    while (!pending.empty()) {
        const uint32_t index = pending.back();
        pending.pop_back();
        for (const uint32_t child : scene.blocks[index].children) {
            if (child >= scene.blocks.size() || visited[child] != 0) continue;
            visited[child] = 1;
            states[child] = composed_surface_state(scene, scene.blocks[child], states[index]);
            pending.push_back(child);
        }
    }
    return states;
}

std::string find_base_texture_file_name(const NifScene& scene, const NifBlock& geometry) {
    for (const uint32_t property_link : geometry.properties) {
        if (property_link >= scene.blocks.size()) continue;
        const NifBlock& property = scene.blocks[property_link];
        if (property.type != "NiTexturingProperty") continue;
        if (property.base_texture_link >= scene.blocks.size()) continue;
        const NifBlock& texture = scene.blocks[property.base_texture_link];
        if (texture.type != "NiSourceTexture") continue;
        return texture.texture_file_name;
    }
    return {};
}

std::string find_detail_texture_file_name(const NifScene& scene, const NifBlock& geometry) {
    for (const uint32_t property_link : geometry.properties) {
        if (property_link >= scene.blocks.size()) continue;
        const NifBlock& property = scene.blocks[property_link];
        if (property.type != "NiTexturingProperty") continue;
        if (property.detail_texture_link >= scene.blocks.size()) continue;
        const NifBlock& texture = scene.blocks[property.detail_texture_link];
        if (texture.type != "NiSourceTexture") continue;
        return texture.texture_file_name;
    }
    return {};
}

std::vector<std::string> find_extra_texture_file_names(const NifScene& scene, const NifBlock& geometry) {
    std::vector<std::string> names;
    for (const uint32_t property_link : geometry.properties) {
        if (property_link >= scene.blocks.size()) continue;
        const NifBlock& property = scene.blocks[property_link];
        if (property.type != "NiTexturingProperty") continue;
        for (const uint32_t link : property.extra_texture_links) {
            if (link >= scene.blocks.size()) continue;
            const NifBlock& texture = scene.blocks[link];
            if (texture.type == "NiSourceTexture" && !texture.texture_file_name.empty())
                names.push_back(texture.texture_file_name);
        }
    }
    return names;
}

const NifAnimation* find_animation(const NifScene& scene, uint32_t link) {
    if (link >= scene.blocks.size()) return nullptr;
    return scene.blocks[link].animation.get();
}

bool read_nif_header(const std::string& path, NifHeader& out) {
    std::vector<char> bytes;
    if (!load_file(path, bytes)) return false;
    Cursor cursor(bytes);
    std::string error;
    return parse_header(bytes, out, cursor, error);
}

bool read_nif_scene(const std::string& path, NifScene& out, std::string& error) {
    std::vector<char> bytes;
    if (!load_file(path, bytes)) { error = "cannot read " + path; return false; }

    out = NifScene{};
    Cursor cursor(bytes);
    if (!parse_header(bytes, out.header, cursor, error)) return false;

    if (!read_blocks(cursor, out, error)) return false;
    resolve_sequence_names(out);
    resolve_legacy_sequence_timing(out);

    out.trailing_bytes = bytes.size() - cursor.position();
    read_footer_roots(cursor, out.roots);
    return true;
}

} // namespace KnC
