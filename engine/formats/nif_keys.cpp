// Key arrays NIF animation blocks one reader per content shared
#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

// Euler rotation keys named axis order until interpolators arrived
constexpr uint32_t kInterpolatorsFrom = make_version(10, 1, 0, 104);

constexpr uint32_t kKeyInterpolationCount = 6;
constexpr uint32_t kScalarComponents      = 1;
constexpr uint32_t kPointComponents       = 3;
// NiColorKey holds NiColorA colour key as wide as quaternion
constexpr uint32_t kQuaternionComponents  = 4;
constexpr uint32_t kEulerAxisCount        = 3;
constexpr uint32_t kTensionContinuityBiasCount = 3;
// Smallest key any content time plus one float
constexpr uint32_t kSmallestKeyBytes = 2 * sizeof(float);
// Bool key time plus one byte NiBool
constexpr uint32_t kBoolKeyBytes = sizeof(float) + 1;
// XYZ rotation key three key arrays each at least count
constexpr uint32_t kSmallestEulerKeyBytes = kEulerAxisCount * sizeof(uint32_t);

uint32_t components_of(KeyContent content) {
    switch (content) {
        case KeyContent::Point:      return kPointComponents;
        case KeyContent::Quaternion:
        case KeyContent::Colour:     return kQuaternionComponents;
        case KeyContent::Float:      break;
    }
    return kScalarComponents;
}

// Which interpolations each content streams anything else lost not empty
bool is_streamable(KeyContent content, NifKeyInterpolation interpolation) {
    if (interpolation == NifKeyInterpolation::Linear ||
        interpolation == NifKeyInterpolation::Step)
        return true;
    // No SDK generation writes curved colour key
    if (content == KeyContent::Colour) return false;
    if (interpolation == NifKeyInterpolation::TensionContinuityBias ||
        interpolation == NifKeyInterpolation::Quadratic)
        return true;
    return content == KeyContent::Quaternion &&
           interpolation == NifKeyInterpolation::XyzRotations;
}

// Floats key streams beyond value quadratic rotations tangents no
uint32_t extra_float_count(const NifKeyGroup& group) {
    if (group.interpolation == NifKeyInterpolation::TensionContinuityBias)
        return kTensionContinuityBiasCount;
    if (group.interpolation != NifKeyInterpolation::Quadratic) return 0;
    return group.components == kQuaternionComponents ? 0 : 2 * group.components;
}

// Key count array header announces minus 1 cannot be streamed
int64_t read_key_group_header(Cursor& cursor, KeyContent content, NifKeyGroup& group) {
    uint32_t count = 0;
    if (!cursor.take_u32(count)) return -1;
    group.components = components_of(content);
    if (count == 0) return 0;
    if (count > cursor.remaining() / kSmallestKeyBytes) return -1;
    uint32_t raw_interpolation = 0;
    if (!cursor.take_u32(raw_interpolation)) return -1;
    if (raw_interpolation >= kKeyInterpolationCount) return -1;
    group.interpolation = static_cast<NifKeyInterpolation>(raw_interpolation);
    if (!is_streamable(content, group.interpolation)) return -1;
    return count;
}

bool read_key(Cursor& cursor, uint32_t extra_floats, NifKeyGroup& group) {
    float time = 0.f;
    if (!cursor.take_f32(time)) return false;
    group.times.push_back(time);
    for (uint32_t component = 0; component < group.components; ++component) {
        float value = 0.f;
        if (!cursor.take_f32(value)) return false;
        group.values.push_back(value);
    }
    for (uint32_t extra = 0; extra < extra_floats; ++extra) {
        float parameter = 0.f;
        if (!cursor.take_f32(parameter)) return false;
        group.interpolation_parameters.push_back(parameter);
    }
    return true;
}

// Header already fixed group interpolation component count
bool read_key_values(Cursor& cursor, uint32_t count, NifKeyGroup& group) {
    const uint32_t extra_floats = extra_float_count(group);
    group.times.reserve(count);
    group.values.reserve(static_cast<size_t>(count) * group.components);
    group.interpolation_parameters.reserve(static_cast<size_t>(count) * extra_floats);
    for (uint32_t key = 0; key < count; ++key)
        if (!read_key(cursor, extra_floats, group)) return false;
    return true;
}

// One float key array per axis three axes key axis enum
bool read_euler_axes(Cursor& cursor, const NifHeader& header,
                     std::vector<NifKeyGroup>& axes) {
    for (size_t axis = 0; axis < axes.size(); ++axis) {
        const bool starts_a_key = axis % kEulerAxisCount == 0;
        if (starts_a_key && header.version < kInterpolatorsFrom && !skip_enum(cursor))
            return false;
        if (!read_key_group(cursor, KeyContent::Float, axes[axis])) return false;
    }
    return true;
}

} // namespace

bool read_key_group(Cursor& cursor, KeyContent content, NifKeyGroup& group) {
    const int64_t count = read_key_group_header(cursor, content, group);
    if (count < 0) return false;
    return read_key_values(cursor, static_cast<uint32_t>(count), group);
}

bool read_bool_key_group(Cursor& cursor, NifKeyGroup& group) {
    uint32_t count = 0;
    if (!cursor.take_u32(count)) return false;
    group.components = kScalarComponents;
    if (count == 0) return true;
    if (count > cursor.remaining() / kBoolKeyBytes) return false;
    uint32_t raw_interpolation = 0;
    if (!cursor.take_u32(raw_interpolation)) return false;
    if (raw_interpolation >= kKeyInterpolationCount) return false;
    group.interpolation = static_cast<NifKeyInterpolation>(raw_interpolation);
    group.times.reserve(count);
    group.values.reserve(count);
    for (uint32_t key = 0; key < count; ++key) {
        float time = 0.f;
        uint8_t is_set = 0;   // NiBool is one byte
        if (!cursor.take_f32(time) || !cursor.take_u8(is_set)) return false;
        group.times.push_back(time);
        group.values.push_back(is_set != 0 ? 1.f : 0.f);
    }
    return true;
}

bool read_rotation_key_group(Cursor& cursor, const NifHeader& header,
                             NifAnimation& animation) {
    NifKeyGroup& rotations = animation.rotations;
    const int64_t count = read_key_group_header(cursor, KeyContent::Quaternion, rotations);
    if (count < 0) return false;
    if (rotations.interpolation != NifKeyInterpolation::XyzRotations)
        return read_key_values(cursor, static_cast<uint32_t>(count), rotations);
    if (static_cast<uint64_t>(count) > cursor.remaining() / kSmallestEulerKeyBytes)
        return false;
    animation.rotation_axes.resize(static_cast<size_t>(count) * kEulerAxisCount);
    return read_euler_axes(cursor, header, animation.rotation_axes);
}

}
