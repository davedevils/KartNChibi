#include "engine/formats/nif_internal.h"

#include <memory>

namespace KnC::nif {

namespace {

// NiGeometry replaced optional shader with named material list
constexpr uint32_t kMaterialListFrom = make_version(20, 2, 0, 5);
// Particle rotation split angle and axis below quaternion both
constexpr uint32_t kSplitRotationFrom = make_version(10, 3, 0, 5);
// Per particle rotation speeds joined NiPSysData
constexpr uint32_t kRotationSpeedsFrom = make_version(20, 0, 0, 2);

// One word texture sets low six bits normal basis top four
constexpr uint16_t kTextureSetCountMask   = 0x003F;
constexpr uint16_t kNormalBasisMethodMask = 0xF000;

bool take_bool(Cursor& cursor, bool& value) {
    uint8_t raw_flag = 0;
    if (!cursor.take_u8(raw_flag)) return false;
    value = raw_flag != 0;
    return true;
}

bool skip_floats(Cursor& cursor, size_t float_count) {
    return cursor.skip(float_count * sizeof(float));
}

// Per vertex array flag misread loses stream 20 3 1 0 NifComponentFormat
bool skip_optional_components(Cursor& cursor, const NifHeader& header, size_t count) {
    NifComponentFormat format = NifComponentFormat::Absent;
    if (!take_component_format(cursor, header, format)) return false;
    return skip_components(cursor, format, count);
}

// Array present flag indicates existence even if empty
bool read_optional_components(Cursor& cursor, const NifHeader& header, size_t count,
                              std::vector<float>& out, bool& present) {
    NifComponentFormat format = NifComponentFormat::Absent;
    if (!take_component_format(cursor, header, format)) return false;
    present = format != NifComponentFormat::Absent;
    return read_component_array(cursor, format, count, out);
}

// From 20 2 0 5 named material list below optional shader
bool read_geometry_materials(Cursor& cursor, const NifHeader& header) {
    if (header.version < kMaterialListFrom) {
        bool has_shader = false;
        if (!take_bool(cursor, has_shader)) return false;
        if (!has_shader) return true;
        std::string shader_name;
        uint32_t implementation = 0;
        return read_object_name(cursor, header, shader_name) &&
               cursor.take_u32(implementation);
    }
    uint32_t material_count = 0;
    if (!cursor.take_u32(material_count) || material_count > kMaxLinks) return false;
    for (uint32_t index = 0; index < material_count; ++index) {
        std::string material_name;
        uint32_t material_extra_data = 0;
        if (!read_object_name(cursor, header, material_name) ||
            !cursor.take_u32(material_extra_data)) return false;
    }
    uint32_t active_material = 0;
    return cursor.take_u32(active_material);
}

// NiGeometry base particle system NiTriShape data link to NiPSysData
bool read_geometry_object(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_av_object(cursor, header, block)) return false;
    if (!cursor.take_u32(block.data_link)) return false;
    if (!cursor.take_u32(block.skin_instance_link)) return false;
    if (!read_geometry_materials(cursor, header)) return false;
    if (header.version < kGeometryDirtyFlagFrom) return true;
    uint8_t default_material_is_dirty = 0;
    return cursor.take_u8(default_material_is_dirty);
}

// NiGeometryData read here not shared vertex count sizes arrays
bool read_geometry_vertex_data(Cursor& cursor, const NifHeader& header,
                               uint16_t& vertex_count) {
    if (header.version >= kGeometryGroupIdFrom) {
        uint32_t group_id = 0;
        if (!cursor.take_u32(group_id)) return false;
    }
    uint8_t keep_flags = 0;
    uint8_t compress_flags = 0;
    if (!cursor.take_u16(vertex_count)) return false;
    if (!cursor.take_u8(keep_flags) || !cursor.take_u8(compress_flags)) return false;
    NifComponentFormat vertex_format = NifComponentFormat::Absent;
    if (!take_component_format(cursor, header, vertex_format)) return false;
    if (!skip_components(cursor, vertex_format, static_cast<size_t>(vertex_count) * 3u))
        return false;

    uint16_t data_flags = 0;
    if (!cursor.take_u16(data_flags)) return false;
    // Named normal basis method appends tangents bitangents
    const size_t vectors_per_vertex =
        (data_flags & kNormalBasisMethodMask) != 0 ? 3u : 1u;
    if (!skip_optional_components(
            cursor, header, static_cast<size_t>(vertex_count) * vectors_per_vertex * 3u))
        return false;

    if (!skip_floats(cursor, 4u)) return false;   // Bounding sphere centre radius
    if (!skip_optional_components(cursor, header, static_cast<size_t>(vertex_count) * 4u))
        return false;
    // vertex colours then texture sets count no flag width from position array
    const size_t texture_set_count = data_flags & kTextureSetCountMask;
    if (!skip_components(cursor, vertex_format,
                         static_cast<size_t>(vertex_count) * texture_set_count * 2u))
        return false;

    uint16_t consistency_flags = 0;
    if (!cursor.take_u16(consistency_flags)) return false;
    if (header.version < kAdditionalGeometryDataFrom) return true;
    return skip_link(cursor);
}

// NiParticlesData radius size rotation per vertex
bool read_particles_data(Cursor& cursor, const NifHeader& header, NifBlock& block,
                         uint16_t& vertex_count) {
    if (!read_geometry_vertex_data(cursor, header, vertex_count)) return false;
    NifParticleSystem& system = particle_system_of(block);
    const size_t count = vertex_count;
    system.pool_size = vertex_count;
    bool present = false;
    if (!read_optional_components(cursor, header, count, system.radii, present)) return false;
    if (!cursor.take_u16(system.active_count)) return false;
    if (!read_optional_components(cursor, header, count, system.sizes, present)) return false;
    if (!skip_optional_components(cursor, header, count * 4u)) return false;  // rotations
    if (header.version < kSplitRotationFrom) return true;
    return skip_optional_components(cursor, header, count) &&        // rotation angles then rotation axes
           skip_optional_components(cursor, header, count * 3u);
}

// NiParticleInfo per vertex velocity ages generation code
bool read_particle_descriptions(Cursor& cursor, const NifHeader& header,
                                uint16_t vertex_count) {
    size_t floats_per_particle = 3u + 3u;   // Velocity age life span last update then rotation axis below split gate
    if (header.version < kSplitRotationFrom) floats_per_particle += 3u;
    const size_t bytes_per_particle =
        floats_per_particle * sizeof(float) + 2u * sizeof(uint16_t);
    return cursor.skip(static_cast<size_t>(vertex_count) * bytes_per_particle);
}

} // namespace

NifParticleSystem& particle_system_of(NifBlock& block) {
    if (!block.particles) block.particles = std::make_shared<NifParticleSystem>();
    return *block.particles;
}

NifPSysModifier& psys_modifier_of(NifBlock& block) {
    if (!block.psys_modifier) block.psys_modifier = std::make_shared<NifPSysModifier>();
    return *block.psys_modifier;
}

// NiParticleSystem through NiParticles adds nothing to NiGeometry
bool read_particle_system(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_geometry_object(cursor, header, block)) return false;
    NifParticleSystem& system = particle_system_of(block);
    if (!take_bool(cursor, system.world_space)) return false;
    return read_links(cursor, system.modifiers);
}

// NiMeshParticleSystem streams nothing of its own
bool read_mesh_particle_system(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    return read_particle_system(cursor, header, block);
}

bool read_particle_system_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    uint16_t vertex_count = 0;
    if (!read_particles_data(cursor, header, block, vertex_count)) return false;
    if (!read_particle_descriptions(cursor, header, vertex_count)) return false;
    if (header.version >= kRotationSpeedsFrom) {
        std::vector<float> rotation_speeds;
        bool present = false;
        if (!read_optional_components(cursor, header, vertex_count, rotation_speeds, present))
            return false;
        particle_system_of(block).has_rotation_speeds = present;
    }
    uint16_t added_particle_count = 0;
    uint16_t added_particle_base = 0;
    return cursor.take_u16(added_particle_count) &&
           cursor.take_u16(added_particle_base);
}

// NiPSysModifier base every modifier block opens family sets kind
bool read_psys_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_name(cursor, header, block.name)) return false;
    NifPSysModifier& modifier = psys_modifier_of(block);
    return cursor.take_u32(modifier.order) &&
           cursor.take_u32(modifier.target_system_link) &&
           take_bool(cursor, modifier.is_active);
}

}
