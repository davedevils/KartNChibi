#include "engine/formats/nif_internal.h"

#include <memory>
#include <utility>

namespace KnC::nif {

namespace {

constexpr uint32_t kSkinPartitionFrom = make_version(10, 1, 0, 101);
constexpr uint32_t kSortAccumUntil    = make_version(20, 0, 0, 4);

// Skin payload allocated on first use
NifSkin& skin_of(NifBlock& block) {
    if (!block.skin) block.skin = std::make_shared<NifSkin>();
    return *block.skin;
}

} // namespace

// NiSkinInstance binds skinned geometry to skeleton
bool read_skin_instance(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    NifSkin& skin = skin_of(block);
    if (!cursor.take_u32(skin.data_link)) return false;
    if (header.version >= kSkinPartitionFrom && !cursor.take_u32(skin.partition_link))
        return false;
    if (!cursor.take_u32(skin.skeleton_root_link)) return false;
    return read_links(cursor, skin.bones);
}

// NiSortAdjustNode node child sort accumulator link before 20 0 0 4
bool read_sort_adjust_node(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_node(cursor, header, block)) return false;
    if (!skip_enum(cursor)) return false;                       // sorting mode
    if (header.version >= kSortAccumUntil) return true;
    return skip_link(cursor);                                   // accumulator
}

// NiRoomGroup node shell rooms
bool read_room_group(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_node(cursor, header, block)) return false;
    if (!skip_link(cursor)) return false;                       // shell
    std::vector<uint32_t> rooms;
    return read_links(cursor, rooms);
}

// NiBoolTimelineInterpolator streams nothing beyond base
bool read_bool_timeline_interpolator(Cursor& cursor, const NifHeader& header,
                                     NifBlock& block) {
    return read_bool_interpolator(cursor, header, block);
}

namespace {

constexpr uint32_t kSkinPartitionInDataUntil = make_version(10, 1, 0, 101);

// NiTransform rotation then translation unlike NiAVObject 52 bytes
bool take_transform(Cursor& cursor, NifTransform& out) {
    return cursor.take(out.rotation, sizeof(out.rotation)) &&
           cursor.take(out.translation, sizeof(out.translation)) &&
           cursor.take(&out.scale, sizeof(out.scale));
}
// NiBound NiPlane both vector plus one float
bool skip_bound_or_plane(Cursor& cursor) { return cursor.skip(4 * sizeof(float)); }

// One NiSkinData bone bind transform sphere vertices vertex count always
bool read_skin_bone(Cursor& cursor, NifComponentFormat weight_format, NifSkinBone& bone) {
    if (!take_transform(cursor, bone.bind) || !skip_bound_or_plane(cursor)) return false;
    uint16_t vertex_count = 0;
    if (!cursor.take_u16(vertex_count)) return false;
    if (weight_format == NifComponentFormat::Absent) return true;
    bone.vertex_indices.resize(vertex_count);
    bone.weights.resize(vertex_count);
    for (uint16_t weight = 0; weight < vertex_count; ++weight)
        if (!cursor.take_u16(bone.vertex_indices[weight]) ||
            !take_component(cursor, weight_format, bone.weights[weight])) return false;
    return true;
}

} // namespace

// NiSkinData bind pose per bone transform sphere optional weights
bool read_skin_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    NifSkin& skin = skin_of(block);
    if (!take_transform(cursor, skin.skin_to_root)) return false;
    uint32_t bone_count = 0;
    if (!cursor.take_u32(bone_count) || bone_count > kMaxLinks) return false;
    if (header.version < kSkinPartitionInDataUntil && !skip_link(cursor)) return false;
    NifComponentFormat weight_format = NifComponentFormat::Absent;
    if (!take_component_format(cursor, header, weight_format)) return false;

    skin.bone_binds.resize(bone_count);
    for (NifSkinBone& bone : skin.bone_binds)
        if (!read_skin_bone(cursor, weight_format, bone)) return false;
    return true;
}

// NiRoom node wall planes portals fixtures
bool read_room(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_node(cursor, header, block)) return false;
    uint32_t wall_count = 0;
    if (!cursor.take_u32(wall_count) || wall_count > kMaxLinks) return false;
    for (uint32_t wall = 0; wall < wall_count; ++wall)
        if (!skip_bound_or_plane(cursor)) return false;
    std::vector<uint32_t> links;
    return read_links(cursor, links) &&      // in portals then out portals then fixtures
           read_links(cursor, links) &&
           read_links(cursor, links);
}

namespace {

// Five counts open NiSkinPartition record size every following array
struct PartitionCounts {
    uint16_t vertices = 0, triangles = 0, bones = 0, strips = 0, bones_per_vertex = 0;
};

bool read_partition_counts(Cursor& cursor, PartitionCounts& counts) {
    return cursor.take_u16(counts.vertices) && cursor.take_u16(counts.triangles) &&
           cursor.take_u16(counts.bones) && cursor.take_u16(counts.strips) &&
           cursor.take_u16(counts.bones_per_vertex);
}

// Strip lengths precede presence byte gate indices read either way
bool read_partition_indices(Cursor& cursor, const PartitionCounts& counts,
                            NifSkinPartitionRecord& record) {
    std::vector<uint16_t> strip_lengths;
    size_t index_count = static_cast<size_t>(counts.triangles) * 3;
    if (counts.strips != 0) {
        if (!take_u16_array(cursor, counts.strips, strip_lengths)) return false;
        index_count = 0;
        for (const uint16_t length : strip_lengths)
            index_count += length;
    }
    uint8_t present = 0;
    if (!cursor.take_u8(present)) return false;
    if (!present) return true;

    std::vector<uint16_t> indices;
    if (!take_u16_array(cursor, index_count, indices)) return false;
    if (counts.strips == 0) {
        record.triangles = std::move(indices);
        return true;
    }
    size_t offset = 0;
    for (const uint16_t length : strip_lengths) {
        expand_strip(indices.data() + offset, length, record.triangles);
        offset += length;
    }
    return true;
}

// One NiSkinPartition record arrays behind presence byte triangle depends strip
bool read_partition_record(Cursor& cursor, const NifHeader& header,
                           NifSkinPartitionRecord& record) {
    PartitionCounts counts;
    if (!read_partition_counts(cursor, counts)) return false;
    record.bones_per_vertex = counts.bones_per_vertex;
    if (!take_u16_array(cursor, counts.bones, record.bones)) return false;

    uint8_t present = 0;
    if (!cursor.take_u8(present)) return false;
    if (present && !take_u16_array(cursor, counts.vertices, record.vertex_map))
        return false;

    // Palette weights same packing 26496 partitions 0x0F two 9550 0x01 four
    const size_t interactions =
        static_cast<size_t>(counts.bones_per_vertex) * counts.vertices;
    NifComponentFormat weight_format = NifComponentFormat::Absent;
    if (!take_component_format(cursor, header, weight_format)) return false;
    if (!read_component_array(cursor, weight_format, interactions, record.weights))
        return false;

    if (!read_partition_indices(cursor, counts, record)) return false;

    if (!cursor.take_u8(present)) return false;
    if (!present) return true;
    if (interactions > cursor.remaining()) return false;
    record.bone_indices.resize(interactions);
    return interactions == 0 || cursor.take(record.bone_indices.data(), interactions);
}

} // namespace

namespace {

// One BoundingVolume collision type picks body box capsule union fails
bool skip_bounding_volume(Cursor& cursor) {
    uint32_t collision_type = 0;
    if (!cursor.take_u32(collision_type)) return false;
    // case 0 sphere 16 case 1 box 60 case 2 capsule 32 case 5 half space 28 case 4 union
    switch (collision_type) {
        case 0: return cursor.skip(16);
        case 1: return cursor.skip(60);
        case 2: return cursor.skip(32);
        case 5: return cursor.skip(28);
        case 4: break;
        default: return false;
    }
    uint32_t volume_count = 0;
    if (!cursor.take_u32(volume_count) || volume_count > kMaxLinks) return false;
    for (uint32_t volume = 0; volume < volume_count; ++volume)
        if (!skip_bounding_volume(cursor)) return false;
    return true;
}

} // namespace

// NiCollisionData object hit test volume optional AK 13 49 77 bytes
bool read_collision_data(Cursor& cursor, const NifHeader& header, NifBlock&) {
    if (!skip_link(cursor)) return false;                  // the NiAVObject it guards then propagation mode
    if (!skip_enum(cursor)) return false;
    if (header.version >= make_version(10, 1, 0, 0) && !skip_enum(cursor)) return false;
    uint8_t uses_alternate_bounding_volume = 0;
    if (!cursor.take_u8(uses_alternate_bounding_volume)) return false;
    if (uses_alternate_bounding_volume != 1) return true;
    return skip_bounding_volume(cursor);
}

bool read_skin_partition(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    uint32_t partition_count = 0;
    if (!cursor.take_u32(partition_count) || partition_count > kMaxLinks) return false;
    NifSkin& skin = skin_of(block);
    skin.partitions.resize(partition_count);
    for (NifSkinPartitionRecord& record : skin.partitions)
        if (!read_partition_record(cursor, header, record)) return false;
    return true;
}

// NiPortal polygon opening between two rooms
bool read_portal(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_av_object(cursor, header, block)) return false;
    uint16_t flags = 0, unused = 0, vertex_count = 0;
    if (!cursor.take_u16(flags) || !cursor.take_u16(unused) ||
        !cursor.take_u16(vertex_count)) return false;
    if (!cursor.skip(static_cast<size_t>(vertex_count) * 3 * sizeof(float))) return false;
    return skip_link(cursor);                 // adjoining portal
}

// NiDynamicEffect NiAVObject on flag nodes skipped intentionally
bool read_dynamic_effect(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_av_object(cursor, header, block)) return false;
    uint8_t is_enabled = 0;
    if (header.version >= kDynamicEffectSwitchFrom && !cursor.take_u8(is_enabled))
        return false;
    std::vector<uint32_t> unaffected_nodes;
    return read_links(cursor, unaffected_nodes);
}

// NiTextureEffect projected texture projection transform
bool read_texture_effect(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_dynamic_effect(cursor, header, block)) return false;
    if (!cursor.skip(12 * sizeof(float))) return false;      // Projection matrix and translation then filter enum
    if (!skip_enum(cursor)) return false;
    if (header.version >= make_version(20, 5, 0, 4)) {
        uint16_t max_anisotropy = 0;
        if (!cursor.take_u16(max_anisotropy)) return false;
    }
    auto effect = std::make_shared<NifTextureEffect>();
    uint32_t clamp = 0;
    if (!cursor.take_u32(clamp) || !cursor.take_u32(effect->texture_type) ||
        !cursor.take_u32(effect->coordinate_type) || !cursor.take_u32(effect->texture_link))
        return false;
    block.texture_effect = effect;
    uint8_t plane_enabled = 0;
    if (!cursor.take_u8(plane_enabled)) return false;
    if (!cursor.skip(4 * sizeof(float))) return false;        // model plane
    if (header.version >= make_version(10, 3, 0, 4)) return true;
    return cursor.skip(2 * sizeof(int16_t));                  // two legacy shorts
}

// Four types add nothing beyond controller base
bool read_vis_controller(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_single_interp_controller(cursor, block);
}

// NiVisData bare key array no enum no padding visibility step
bool read_vis_data(Cursor& cursor, const NifHeader&, NifBlock& block) {
    constexpr uint32_t kVisKeyBytes = sizeof(float) + 1;
    NifKeyGroup& keys = animation_of(block).channel;
    uint32_t key_count = 0;
    if (!cursor.take_u32(key_count)) return false;
    if (key_count > cursor.remaining() / kVisKeyBytes) return false;
    keys.components = 1;
    keys.interpolation = NifKeyInterpolation::Step;
    keys.times.reserve(key_count);
    keys.values.reserve(key_count);
    for (uint32_t key = 0; key < key_count; ++key) {
        float   time = 0.f;
        uint8_t is_visible = 0;
        if (!cursor.take_f32(time) || !cursor.take_u8(is_visible)) return false;
        keys.times.push_back(time);
        keys.values.push_back(is_visible != 0 ? 1.f : 0.f);
    }
    return true;
}

// NiExtraDataController names extra data it drives
bool read_extra_data_controller(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_single_interp_controller(cursor, block)) return false;
    std::string extra_data_name;
    return read_object_name(cursor, header, extra_data_name);
}

bool read_light_colour_controller(Cursor& cursor, const NifHeader&, NifBlock& block) {
    if (!read_single_interp_controller(cursor, block)) return false;
    uint16_t flags = 0;
    return cursor.take_u16(flags);
}

}
