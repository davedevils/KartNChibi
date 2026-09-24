#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

// Interpolators replaced per controller data blocks below deprecated stream
constexpr uint32_t kInterpolatorsReplacedDataFrom = make_version(10, 1, 0, 104);

// NiPSysModifierCtlr single interp controller name modifier drives finds
bool read_modifier_ctlr(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_single_interp_controller(cursor, block)) return false;
    // Modifier name not block NiTimeController carries no name
    std::string modifier_name;
    return read_object_name(cursor, header, modifier_name);
}

// NiPSysModifierFloatCtlr NiPSysModifierBoolCtlr stream same pre interp data
bool read_modifier_ctlr_with_legacy_data(Cursor& cursor, const NifHeader& header,
                                         NifBlock& block) {
    if (!read_modifier_ctlr(cursor, header, block)) return false;
    if (header.version >= kInterpolatorsReplacedDataFrom) return true;
    return skip_link(cursor);
}

} // namespace

bool read_psys_emitter_ctlr(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_modifier_ctlr(cursor, header, block)) return false;
    // From 10 1 0 104 the link is the emitter active interpolator below it NiPSysEmitterCtlrData
    if (header.version < kInterpolatorsReplacedDataFrom) return skip_link(cursor);
    return cursor.take_u32(animation_of(block).controller.visibility_interpolator_link);
}

bool read_psys_update_ctlr(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_time_controller(cursor, block);
}

bool read_psys_modifier_float_ctlr(Cursor& cursor, const NifHeader& header,
                                   NifBlock& block) {
    return read_modifier_ctlr_with_legacy_data(cursor, header, block);
}

bool read_psys_reset_on_loop_ctlr(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_time_controller(cursor, block);
}

bool read_psys_modifier_active_ctlr(Cursor& cursor, const NifHeader& header,
                                    NifBlock& block) {
    return read_modifier_ctlr_with_legacy_data(cursor, header, block);
}

}
