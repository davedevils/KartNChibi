#include "engine/render/skinned_model.h"

#include <cstddef>

namespace KnC::Render {

namespace {

// placement as three vec4 rows shader reads row per dot product
void write_rows(const KnC::NifPlacement& placement, float* out) {
    for (int row = 0; row < kBoneMatrixRows; ++row) {
        for (int column = 0; column < 3; ++column)
            out[row * 4 + column] = placement.scale * placement.rotation[row * 3 + column];
        out[row * 4 + 3] = placement.translation[row];
    }
}

// bone matrix current pose composed onto bind time transform
void write_slot(const SkinnedPose& pose, const BonePalette& palette, std::size_t slot,
                float* out) {
    const int node = palette.nodes[slot];
    if (node < 0 || static_cast<std::size_t>(node) >= pose.nodes.size()) {
        write_rows(KnC::NifPlacement(), out);
        return;
    }
    write_rows(KnC::compose_placement(pose.nodes[static_cast<std::size_t>(node)],
                                        palette.binds[slot]),
               out);
}

std::size_t size_palettes(const CharacterRig& rig, SkinnedPose& out) {
    out.part_offset.resize(rig.palettes.size());
    std::size_t floats = 0;
    for (std::size_t part = 0; part < rig.palettes.size(); ++part) {
        out.part_offset[part] = floats;
        floats += rig.palettes[part].nodes.size() * kBoneMatrixRows * 4;
    }
    return floats;
}

} // namespace

void evaluate_skinned_pose(const CharacterRig& rig, int clip, float seconds, SkinnedPose& out) {
    if (clip >= 0 && static_cast<std::size_t>(clip) < rig.clips.size())
        pose_nif_skeleton(rig.skeleton, rig.clips[static_cast<std::size_t>(clip)].motion, seconds,
                          out.nodes);
    else rest_nif_skeleton(rig.skeleton, out.nodes);
    out.rows.resize(size_palettes(rig, out));
    for (std::size_t part = 0; part < rig.palettes.size(); ++part) {
        float* rows = out.rows.data() + out.part_offset[part];
        for (std::size_t slot = 0; slot < rig.palettes[part].nodes.size(); ++slot)
            write_slot(out, rig.palettes[part], slot, rows + slot * kBoneMatrixRows * 4);
    }
}

}
