#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

// NiPSysEmitter gained initial radius variance
constexpr uint32_t kRadiusVarianceFrom = make_version(10, 3, 0, 2);
// NiPSysRotationModifier gained speed angle variance sign flag
constexpr uint32_t kRotationVarianceFrom = make_version(20, 0, 0, 2);
// Below this NiMeshPSysData derived pools not streaming
constexpr uint32_t kGenerationPoolsFrom = make_version(10, 1, 0, 1);

// NiBool one byte on wire every SDK generation
bool skip_bool(Cursor& cursor) { return cursor.skip(1u); }

bool take_bool(Cursor& cursor, bool& value) {
    uint8_t raw_flag = 0;
    if (!cursor.take_u8(raw_flag)) return false;
    value = raw_flag != 0;
    return true;
}

bool skip_floats(Cursor& cursor, size_t float_count) {
    return cursor.skip(float_count * sizeof(float));
}

bool take_floats(Cursor& cursor, float* values, size_t float_count) {
    return cursor.take(values, float_count * sizeof(float));
}

// Base and family called it one thing base cannot work
bool read_modifier_of_kind(Cursor& cursor, const NifHeader& header, NifBlock& block,
                           NifPSysModifierKind kind) {
    if (!read_psys_modifier(cursor, header, block)) return false;
    psys_modifier_of(block).kind = kind;
    return true;
}

// NiPSysEmitter six launch floats NiColorA radius life span
bool read_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block,
                  NifPSysModifierKind kind) {
    if (!read_modifier_of_kind(cursor, header, block, kind)) return false;
    NifPSysEmitter& emitter = psys_modifier_of(block).emitter;
    // Speed declination planar angle each variance
    if (!cursor.take_f32(emitter.speed) || !cursor.take_f32(emitter.speed_variation) ||
        !cursor.take_f32(emitter.declination) ||
        !cursor.take_f32(emitter.declination_variation) ||
        !cursor.take_f32(emitter.planar_angle) ||
        !cursor.take_f32(emitter.planar_angle_variation)) return false;
    if (!take_floats(cursor, emitter.initial_colour, 4u)) return false;
    if (!cursor.take_f32(emitter.initial_radius)) return false;
    if (header.version >= kRadiusVarianceFrom &&
        !cursor.take_f32(emitter.radius_variation)) return false;
    return cursor.take_f32(emitter.life_span) &&
           cursor.take_f32(emitter.life_span_variation);
}

// NiPSysVolumeEmitter adds object local space bounds volume
bool read_volume_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block,
                         NifPSysModifierKind kind) {
    return read_emitter(cursor, header, block, kind) &&
           cursor.take_u32(psys_modifier_of(block).emitter.volume_object_link);
}

// NiPSysCollider extends NiObject not NiPSysModifier no name
bool read_collider(Cursor& cursor) {
    if (!skip_floats(cursor, 1u)) return false;
    if (!skip_bool(cursor) || !skip_bool(cursor)) return false;
    // bounce then spawn and die flags then spawn manager and collider links
    return skip_link(cursor) && skip_link(cursor) && skip_link(cursor);
}

// NiMeshPSysData default pool size per particle generation
bool read_generation_pools(Cursor& cursor) {
    uint32_t default_pool_size = 0;
    if (!cursor.take_u32(default_pool_size)) return false;
    if (!skip_bool(cursor)) return false;         // fill pools on load
    uint32_t generation_count = 0;
    if (!cursor.take_u32(generation_count) || generation_count > kMaxLinks) return false;
    return cursor.skip(static_cast<size_t>(generation_count) * sizeof(uint32_t));
}

} // namespace

bool read_psys_age_death_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_modifier_of_kind(cursor, header, block, NifPSysModifierKind::AgeDeath))
        return false;
    NifPSysModifier& modifier = psys_modifier_of(block);
    return take_bool(cursor, modifier.spawn_on_death) &&
           cursor.take_u32(modifier.spawn_modifier_link);
}

bool read_psys_spawn_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_modifier_of_kind(cursor, header, block, NifPSysModifierKind::Spawn)) return false;
    NifPSysSpawn& spawn = psys_modifier_of(block).spawn;
    if (!cursor.take_u16(spawn.generation_count)) return false;
    if (!cursor.take_f32(spawn.spawn_fraction)) return false;
    if (!cursor.take_u16(spawn.minimum_to_spawn) || !cursor.take_u16(spawn.maximum_to_spawn))
        return false;
    return cursor.take_f32(spawn.speed_chaos) && cursor.take_f32(spawn.direction_chaos) &&
           cursor.take_f32(spawn.life_span) && cursor.take_f32(spawn.life_span_variation);
}

bool read_psys_position_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    return read_modifier_of_kind(cursor, header, block, NifPSysModifierKind::Position);
}

bool read_psys_bound_update_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_modifier_of_kind(cursor, header, block, NifPSysModifierKind::BoundUpdate))
        return false;
    // 16 bits either way 10 1 0 100 sign changed not width
    uint16_t update_skip = 0;
    return cursor.take_u16(update_skip);
}

bool read_psys_grow_fade_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_modifier_of_kind(cursor, header, block, NifPSysModifierKind::GrowFade))
        return false;
    NifPSysGrowFade& grow_fade = psys_modifier_of(block).grow_fade;
    return cursor.take_f32(grow_fade.grow_time) &&
           cursor.take_u16(grow_fade.grow_generation) &&
           cursor.take_f32(grow_fade.fade_time) &&
           cursor.take_u16(grow_fade.fade_generation);
}

bool read_psys_colour_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    return read_modifier_of_kind(cursor, header, block, NifPSysModifierKind::Colour) &&
           cursor.take_u32(psys_modifier_of(block).colour_data_link);
}

bool read_psys_mesh_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_emitter(cursor, header, block, NifPSysModifierKind::MeshEmitter)) return false;
    NifPSysEmitter& emitter = psys_modifier_of(block).emitter;
    if (!read_links(cursor, emitter.emitter_meshes)) return false;
    if (!cursor.take_u32(emitter.velocity_type) || !cursor.take_u32(emitter.emission_type))
        return false;
    return take_floats(cursor, emitter.emit_axis, 3u);
}

bool read_psys_box_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_volume_emitter(cursor, header, block, NifPSysModifierKind::BoxEmitter))
        return false;
    return take_floats(cursor, psys_modifier_of(block).emitter.box_extent, 3u);
}

bool read_psys_sphere_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_volume_emitter(cursor, header, block, NifPSysModifierKind::SphereEmitter))
        return false;
    return cursor.take_f32(psys_modifier_of(block).emitter.volume_radius);
}

bool read_psys_cylinder_emitter(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_volume_emitter(cursor, header, block, NifPSysModifierKind::CylinderEmitter))
        return false;
    NifPSysEmitter& emitter = psys_modifier_of(block).emitter;
    return cursor.take_f32(emitter.volume_radius) && cursor.take_f32(emitter.volume_height);
}

bool read_psys_rotation_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_modifier_of_kind(cursor, header, block, NifPSysModifierKind::Rotation))
        return false;
    NifPSysRotation& rotation = psys_modifier_of(block).rotation;
    if (!cursor.take_f32(rotation.speed)) return false;
    if (header.version >= kRotationVarianceFrom) {
        if (!cursor.take_f32(rotation.speed_variation) || !cursor.take_f32(rotation.angle) ||
            !cursor.take_f32(rotation.angle_variation) ||
            !take_bool(cursor, rotation.random_sign)) return false;
    }
    return take_bool(cursor, rotation.random_axis) && take_floats(cursor, rotation.axis, 3u);
}

bool read_psys_gravity_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_modifier_of_kind(cursor, header, block, NifPSysModifierKind::Gravity))
        return false;
    NifPSysGravity& gravity = psys_modifier_of(block).gravity;
    if (!cursor.take_u32(gravity.object_link)) return false;
    if (!take_floats(cursor, gravity.axis, 3u)) return false;
    if (!cursor.take_f32(gravity.decay) || !cursor.take_f32(gravity.strength)) return false;
    if (!cursor.take_u32(gravity.force_type)) return false;
    return cursor.take_f32(gravity.turbulence) && cursor.take_f32(gravity.turbulence_scale);
}

// Four families decode to exact EOF retain nothing no stream effect
bool read_psys_collider_manager(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    return read_psys_modifier(cursor, header, block) && skip_link(cursor);
}

bool read_psys_bomb_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_psys_modifier(cursor, header, block)) return false;
    if (!skip_link(cursor)) return false;         // bomb axis object then axis vector decay delta and velocity then decay and symmetry enums
    if (!skip_floats(cursor, 3u)) return false;
    if (!skip_floats(cursor, 2u)) return false;
    return skip_enum(cursor) && skip_enum(cursor);
}

bool read_psys_mesh_update_modifier(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_psys_modifier(cursor, header, block)) return false;
    // The mesh templates stay the builder sizes the mesh particles on them
    return read_links(cursor, psys_modifier_of(block).mesh_links);
}

// NiPSysPlanarCollider descends NiObject NiPSysCollider not NiPSysModifier
bool read_psys_planar_collider(Cursor& cursor, const NifHeader&, NifBlock&) {
    if (!read_collider(cursor)) return false;
    if (!skip_link(cursor)) return false;         // plane object then width height extent then X and Y axis vectors
    if (!skip_floats(cursor, 2u)) return false;
    return skip_floats(cursor, 6u);
}

bool read_mesh_psys_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_particle_system_data(cursor, header, block)) return false;
    if (header.version >= kGenerationPoolsFrom && !read_generation_pools(cursor))
        return false;
    // The node holding the particle meshes kept for the mesh particle size
    return cursor.take_u32(particle_system_of(block).particle_meshes_link);
}

}
