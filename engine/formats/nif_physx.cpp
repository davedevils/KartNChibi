#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

// NiPhysXScene begins 20 2 0 8 only keep meshes gate fires
constexpr uint32_t kPhysXPropListFrom     = make_version(20, 3, 0, 2);
constexpr uint32_t kPhysXKeepMeshesUntil  = make_version(20, 3, 0, 1);
constexpr uint32_t kPhysXSubStepsFrom     = make_version(20, 3, 0, 9);
constexpr uint32_t kPhysXModifiedMeshFrom = make_version(20, 4, 0, 0);

// One version shipped NiPhysXSceneDesc 20 2 0 8 two compound maps
constexpr uint32_t kPhysXSceneDescVersion = make_version(20, 2, 0, 8);

// NxScene group collision matrix 32 groups 1024 NiBool all set
constexpr size_t kGroupCollisionFlags = 1024;
// NxFilterOp triple then eight NxGroupsMask words
constexpr size_t kFilterWords = 3 + 8;

bool skip_uints(Cursor& cursor, size_t count) {
    return cursor.skip(count * sizeof(uint32_t));
}

bool skip_ref_list(Cursor& cursor) {
    uint32_t count = 0;
    if (!cursor.take_u32(count) || count > kMaxLinks) return false;
    return skip_uints(cursor, count);
}

} // namespace

// NiPhysXScene NxScene decoded length name 560 blocks 87 bytes no physics
bool read_physx_scene(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    if (!cursor.skip(14 * sizeof(float)))       // Scene transform world scale
        return false;
    if (header.version >= kPhysXPropListFrom && !skip_ref_list(cursor)) return false;
    if (!skip_ref_list(cursor) || !skip_ref_list(cursor))   // Sources destinations
        return false;
    if (header.version >= kPhysXModifiedMeshFrom && !skip_ref_list(cursor)) return false;
    if (!cursor.skip(sizeof(float))) return false;          // time step
    if (header.version <= kPhysXKeepMeshesUntil && !cursor.skip(1))
        return false;                                       // keep meshes NiBool
    if (header.version >= kPhysXSubStepsFrom && !skip_uints(cursor, 2))
        return false;                                       // sub-step counts
    uint16_t flags = 0;
    return skip_link(cursor) && cursor.take_u16(flags);      // Snapshot flags
}

// NiPhysXSceneDesc 560 identical 1145 bytes gravity 0 0 -9 81
bool read_physx_scene_desc(Cursor& cursor, const NifHeader& header, NifBlock&) {
    if (header.version != kPhysXSceneDescVersion) return false;
    if (!cursor.skip(4 * sizeof(float)) || !skip_uints(cursor, 2))
        return false;   // Gravity max time step max iterations method
    uint8_t has_bound = 0, has_limits = 0;
    if (!cursor.take_u8(has_bound)) return false;
    if (has_bound != 0 && !cursor.skip(6 * sizeof(float))) return false;
    if (!cursor.take_u8(has_limits)) return false;
    if (has_limits != 0 && !skip_uints(cursor, 4)) return false;
    if (!skip_uints(cursor, 3))   // Simulation type hardware scene type pipeline
        return false;
    if (!cursor.skip(2))          // Ground plane bounds plane NiBools
        return false;
    if (!skip_uints(cursor, 5))   // Flags two thread counts thread bg masks
        return false;
    if (!skip_ref_list(cursor) || !skip_ref_list(cursor) || !skip_ref_list(cursor))
        return false;             // Actors joints materials all empty
    return cursor.skip(kGroupCollisionFlags) && skip_uints(cursor, kFilterWords) &&
           cursor.skip(1) &&      // the filter NiBool then state count
           skip_uints(cursor, 1);
}

}
