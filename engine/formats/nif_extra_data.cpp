#include "engine/formats/nif_internal.h"

namespace KnC::nif {

// NiColorExtraData holds NiColorA payload is RGBA not three float NiColor
bool read_colour_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_name(cursor, header, block.name)) return false;
    float colour_rgba[4] = {};
    return cursor.take(colour_rgba, sizeof(colour_rgba));
}

bool read_float_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_name(cursor, header, block.name)) return false;
    float float_value = 0.f;
    return cursor.take(&float_value, sizeof(float_value));
}

bool read_integer_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_name(cursor, header, block.name)) return false;
    uint32_t integer_value = 0;
    return cursor.take_u32(integer_value);
}

}
