#include "engine/formats/nif_animation_eval.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <vector>

namespace KnC {

namespace {

constexpr uint32_t kMaxSampleComponents  = 4;
constexpr uint32_t kQuaternionComponents = 4;
constexpr uint32_t kPointComponents      = 3;
constexpr uint32_t kScalarComponents     = 1;
constexpr uint32_t kEulerAxisCount       = 3;
// Tension bias and continuity in the order NiTCBKey streams them
constexpr size_t kTcbParameterCount = 3;
// Past this dot product the arc is too short for sin angle to divide by normalised lerp within precision
constexpr float kSlerpLinearLimit = 0.9995f;

struct Quaternion {
    float w = 1.f;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// Keys either side of a time and the fraction between them out of range gives one key
struct KeyInterval {
    size_t previous = 0;
    size_t next     = 0;
    float  progress = 0.f;
};

// The step into a key and step out of it one sided at either end
struct KeyDeltas {
    float before = 0.f;
    float after  = 0.f;
};

KeyInterval locate_interval(const NifKeyGroup& group, float time) {
    const std::vector<float>& times = group.times;
    KeyInterval interval;
    if (time <= times.front()) return interval;
    if (time >= times.back()) {
        interval.previous = times.size() - 1;
        interval.next     = interval.previous;
        return interval;
    }
    const auto after = std::upper_bound(times.begin(), times.end(), time);
    interval.next     = static_cast<size_t>(after - times.begin());
    interval.previous = interval.next - 1;
    const float span = times[interval.next] - times[interval.previous];
    if (span > 0.f) interval.progress = (time - times[interval.previous]) / span;
    return interval;
}

float value_at(const NifKeyGroup& group, size_t key, uint32_t component) {
    return group.values[key * group.components + component];
}

// Cubic Hermite values at two ends then tangent that leaves first and reaches second
float hermite(const float ends[2], const float tangents[2], float progress) {
    const float squared = progress * progress;
    const float cubed   = squared * progress;
    return (2.f * cubed - 3.f * squared + 1.f) * ends[0] +
           (-2.f * cubed + 3.f * squared) * ends[1] +
           (cubed - 2.f * squared + progress) * tangents[0] +
           (cubed - squared) * tangents[1];
}

KeyDeltas key_deltas(const NifKeyGroup& group, size_t key, uint32_t component) {
    const size_t last  = group.key_count() - 1;
    const float  value = value_at(group, key, component);
    KeyDeltas deltas;
    if (key > 0) deltas.before = value - value_at(group, key - 1, component);
    if (key < last) deltas.after = value_at(group, key + 1, component) - value;
    if (key == 0) deltas.before = deltas.after;
    if (key == last) deltas.after = deltas.before;
    return deltas;
}

// Kochanek Bartels derivatives at one key tangent leaving it then reaching it
void tcb_tangents(const KeyDeltas& deltas, const float tcb[3], float out[2]) {
    const float relaxed          = 1.f - tcb[0];
    const float bias_previous    = 1.f + tcb[1];
    const float bias_next        = 1.f - tcb[1];
    const float continuity_plus  = 1.f + tcb[2];
    const float continuity_minus = 1.f - tcb[2];
    out[0] = 0.5f * relaxed * (bias_previous * continuity_minus * deltas.before +
                               bias_next * continuity_plus * deltas.after);
    out[1] = 0.5f * relaxed * (bias_previous * continuity_plus * deltas.before +
                               bias_next * continuity_minus * deltas.after);
}

float sample_tcb(const NifKeyGroup& group, const KeyInterval& interval,
                 uint32_t component) {
    const float* leaving_tcb  = &group.interpolation_parameters[interval.previous *
                                                                kTcbParameterCount];
    const float* reaching_tcb = &group.interpolation_parameters[interval.next *
                                                                kTcbParameterCount];
    float leaving[2]  = {0.f, 0.f};
    float reaching[2] = {0.f, 0.f};
    tcb_tangents(key_deltas(group, interval.previous, component), leaving_tcb, leaving);
    tcb_tangents(key_deltas(group, interval.next, component), reaching_tcb, reaching);
    const float ends[2]     = {value_at(group, interval.previous, component),
                               value_at(group, interval.next, component)};
    const float tangents[2] = {leaving[0], reaching[1]};
    return hermite(ends, tangents, interval.progress);
}

float sample_quadratic(const NifKeyGroup& group, const KeyInterval& interval,
                       uint32_t component) {
    const size_t stride     = 2 * static_cast<size_t>(group.components);
    const float  ends[2]    = {value_at(group, interval.previous, component),
                               value_at(group, interval.next, component)};
    const float tangents[2] = {
        group.interpolation_parameters[interval.previous * stride + component],
        group.interpolation_parameters[interval.next * stride + group.components +
                                       component]};
    return hermite(ends, tangents, interval.progress);
}

float sample_component(const NifKeyGroup& group, const KeyInterval& interval,
                       uint32_t component) {
    const float previous = value_at(group, interval.previous, component);
    if (interval.previous == interval.next) return previous;
    switch (group.interpolation) {
        case NifKeyInterpolation::Step:
            return previous;
        case NifKeyInterpolation::Quadratic:
            return sample_quadratic(group, interval, component);
        case NifKeyInterpolation::TensionContinuityBias:
            return sample_tcb(group, interval, component);
        case NifKeyInterpolation::None:
        case NifKeyInterpolation::Linear:
        case NifKeyInterpolation::XyzRotations:
            break;
    }
    const float next = value_at(group, interval.next, component);
    return previous + interval.progress * (next - previous);
}

// Interpolation parameters one key of this group must carry
size_t parameters_per_key(const NifKeyGroup& group) {
    if (group.interpolation == NifKeyInterpolation::TensionContinuityBias)
        return kTcbParameterCount;
    if (group.interpolation == NifKeyInterpolation::Quadratic)
        return 2 * static_cast<size_t>(group.components);
    return 0;
}

bool holds_every_key(const NifKeyGroup& group) {
    const size_t keys = group.key_count();
    return group.values.size() >= keys * group.components &&
           group.interpolation_parameters.size() >= keys * parameters_per_key(group);
}

Quaternion normalise(const Quaternion& value) {
    const float length = std::sqrt(value.w * value.w + value.x * value.x +
                                   value.y * value.y + value.z * value.z);
    if (!(length > 0.f)) return Quaternion();
    const float inverse = 1.f / length;
    return {value.w * inverse, value.x * inverse, value.y * inverse,
            value.z * inverse};
}

Quaternion quaternion_at(const NifKeyGroup& group, size_t key) {
    const size_t base = key * kQuaternionComponents;
    return {group.values[base], group.values[base + 1], group.values[base + 2],
            group.values[base + 3]};
}

// A quaternion and its negation are the same rotation flip far end into near hemisphere before blending
Quaternion slerp(const Quaternion& from, const Quaternion& to, float progress) {
    float cosine = from.w * to.w + from.x * to.x + from.y * to.y + from.z * to.z;
    Quaternion target = to;
    if (cosine < 0.f) {
        target = {-to.w, -to.x, -to.y, -to.z};
        cosine = -cosine;
    }
    float from_weight = 1.f - progress;
    float to_weight   = progress;
    if (cosine < kSlerpLinearLimit) {
        const float angle = std::acos(cosine);
        const float sine  = std::sin(angle);
        from_weight = std::sin((1.f - progress) * angle) / sine;
        to_weight   = std::sin(progress * angle) / sine;
    }
    return normalise({from.w * from_weight + target.w * to_weight,
                      from.x * from_weight + target.x * to_weight,
                      from.y * from_weight + target.y * to_weight,
                      from.z * from_weight + target.z * to_weight});
}

Quaternion multiply(const Quaternion& left, const Quaternion& right) {
    return {left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
            left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
            left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
            left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w};
}

Quaternion axis_quaternion(uint32_t axis, float angle) {
    const float half = 0.5f * angle;
    const float sine = std::sin(half);
    Quaternion rotation;
    rotation.w = std::cos(half);
    if (axis == 0) rotation.x = sine;
    else if (axis == 1) rotation.y = sine;
    else rotation.z = sine;
    return rotation;
}

void write_quaternion(const Quaternion& rotation, float out[4]) {
    out[0] = rotation.w;
    out[1] = rotation.x;
    out[2] = rotation.y;
    out[3] = rotation.z;
}

// Row major 3x3 for a column vector layout NifTransform rotation uses
void write_rotation_matrix(const Quaternion& rotation, float out[9]) {
    const Quaternion unit = normalise(rotation);
    const float xx = unit.x * unit.x;
    const float yy = unit.y * unit.y;
    const float zz = unit.z * unit.z;
    const float xy = unit.x * unit.y;
    const float xz = unit.x * unit.z;
    const float yz = unit.y * unit.z;
    const float wx = unit.w * unit.x;
    const float wy = unit.w * unit.y;
    const float wz = unit.w * unit.z;
    out[0] = 1.f - 2.f * (yy + zz);
    out[1] = 2.f * (xy - wz);
    out[2] = 2.f * (xz + wy);
    out[3] = 2.f * (xy + wz);
    out[4] = 1.f - 2.f * (xx + zz);
    out[5] = 2.f * (yz - wx);
    out[6] = 2.f * (xz - wy);
    out[7] = 2.f * (yz + wx);
    out[8] = 1.f - 2.f * (xx + yy);
}

// The three per axis channels compose as Rz Ry Rx x turns first the Cosmo head pose proves the order
bool sample_euler_axes(const std::vector<NifKeyGroup>& axes, float time,
                       float out_quaternion[4]) {
    if (axes.size() != kEulerAxisCount) return false;
    Quaternion composed;
    bool sampled_any = false;
    for (uint32_t axis = 0; axis < kEulerAxisCount; ++axis) {
        NifKeySample angle;
        if (!sample_key_group(axes[axis], time, angle)) continue;
        composed    = multiply(axis_quaternion(axis, angle.values[0]), composed);
        sampled_any = true;
    }
    write_quaternion(composed, out_quaternion);
    return sampled_any;
}

// NiQuatTransform streams minus FLT MAX for a component it does not pose measured client tree never poses part
bool is_posed(float value) { return value > -FLT_MAX; }

void resolve_translation(const NifKeySample& sampled, const float pose[3],
                         NifTransformSample& out) {
    const float* source = nullptr;
    if (sampled.count == kPointComponents) source = sampled.values;
    else if (is_posed(pose[0])) source = pose;
    if (source == nullptr) return;
    std::copy(source, source + kPointComponents, out.transform.translation);
    out.has_translation = true;
}

void resolve_scale(const NifKeySample& sampled, float pose, NifTransformSample& out) {
    const bool from_keys = sampled.count == kScalarComponents;
    if (!from_keys && !is_posed(pose)) return;
    out.transform.scale = from_keys ? sampled.values[0] : pose;
    out.has_scale       = true;
}

void resolve_rotation(const float quaternion[4], NifTransformSample& out) {
    const Quaternion rotation = {quaternion[0], quaternion[1], quaternion[2],
                                 quaternion[3]};
    write_rotation_matrix(rotation, out.transform.rotation);
    out.has_rotation = true;
}

// Every B spline a Gamebryo transform interpolator carries is cubic so four control points drive any position
constexpr int kBSplineDegree     = 3;
constexpr int kBSplineSpanPoints = kBSplineDegree + 1;

// The clamped knot vector NiBSplineBasis lays out degree repeated at each end unit steps between
float bspline_knot(int index, int spans) {
    return static_cast<float>(std::min(std::max(index - kBSplineDegree, 0), spans));
}

// The four non zero cubic basis functions at position on span by Cox de Boor recurrence
void bspline_basis(int span, int spans, float position, float out[kBSplineSpanPoints]) {
    float left[kBSplineSpanPoints]  = {0.f, 0.f, 0.f, 0.f};
    float right[kBSplineSpanPoints] = {0.f, 0.f, 0.f, 0.f};
    out[0] = 1.f;
    for (int degree = 1; degree <= kBSplineDegree; ++degree) {
        left[degree]  = position - bspline_knot(span + 1 - degree, spans);
        right[degree] = bspline_knot(span + degree, spans) - position;
        float carried = 0.f;
        for (int point = 0; point < degree; ++point) {
            const float share = out[point] / (right[point + 1] + left[degree - point]);
            out[point] = carried + right[point + 1] * share;
            carried    = left[degree - point] * share;
        }
        out[degree] = carried;
    }
}

// Blends one channel control points at progress of curve false when channel not driven or too few points
bool sample_bspline_channel(const std::vector<float>& points, int components,
                            float progress, float* out) {
    const int count = static_cast<int>(points.size()) / components;
    if (count < kBSplineSpanPoints) return false;
    const int   spans    = count - kBSplineDegree;
    const float position = progress * static_cast<float>(spans);
    // The end of the window belongs to last span not to one past it
    const int span  = progress >= 1.f ? count - 1
                                      : kBSplineDegree + static_cast<int>(position);
    const int first = span - kBSplineDegree;
    float weight[kBSplineSpanPoints] = {0.f, 0.f, 0.f, 0.f};
    bspline_basis(span, spans, position, weight);
    for (int component = 0; component < components; ++component) out[component] = 0.f;
    for (int point = 0; point < kBSplineSpanPoints; ++point)
        for (int component = 0; component < components; ++component)
            out[component] +=
                weight[point] * points[(first + point) * components + component];
    return true;
}

// How far into its own window time sits outside it curve held at end it ran past
float bspline_progress(const NifBSplineSampler& spline, float time) {
    const float window = spline.stop_time - spline.start_time;
    if (!(window > 0.f)) return 0.f;
    return std::min(std::max((time - spline.start_time) / window, 0.f), 1.f);
}

float wrap(float offset, float period) {
    const float wrapped = std::fmod(offset, period);
    return wrapped < 0.f ? wrapped + period : wrapped;
}

// Where a time past the window start lands once the cycle has had its say
float cycle_offset(NifCycleType cycle, float offset, float duration) {
    if (cycle == NifCycleType::Clamp)
        return std::min(std::max(offset, 0.f), duration);
    if (cycle == NifCycleType::Loop) return wrap(offset, duration);
    // Reverse plays the window forwards then backwards so its period doubles
    const float period  = 2.f * duration;
    const float bounced = wrap(offset, period);
    return bounced <= duration ? bounced : period - bounced;
}

} // namespace

bool sample_key_group(const NifKeyGroup& group, float time, NifKeySample& out) {
    out.count = 0;
    if (group.key_count() == 0) return false;
    if (group.components == 0 || group.components > kMaxSampleComponents) return false;
    if (!holds_every_key(group)) return false;
    const KeyInterval interval = locate_interval(group, time);
    for (uint32_t component = 0; component < group.components; ++component)
        out.values[component] = sample_component(group, interval, component);
    out.count = group.components;
    return true;
}

bool sample_quaternion_group(const NifKeyGroup& group, float time,
                             float out_quaternion[4]) {
    if (group.key_count() == 0 || group.components != kQuaternionComponents)
        return false;
    if (group.values.size() < group.key_count() * kQuaternionComponents) return false;
    const KeyInterval interval = locate_interval(group, time);
    // Ten keys in the shipped tree are up to 0 0011 off unit length so sampler cannot assume rotations
    Quaternion sampled = normalise(quaternion_at(group, interval.previous));
    const bool holds = interval.previous == interval.next ||
                       group.interpolation == NifKeyInterpolation::Step;
    if (!holds)
        sampled = slerp(sampled, normalise(quaternion_at(group, interval.next)),
                        interval.progress);
    write_quaternion(sampled, out_quaternion);
    return true;
}

bool map_controller_time(const NifController& controller, float time,
                         float& out_key_time) {
    if (!controller.is_active()) return false;
    const float scaled   = time * controller.frequency + controller.phase;
    const float duration = controller.stop_time - controller.start_time;
    if (!(duration > 0.f)) {
        out_key_time = controller.start_time;
        return true;
    }
    out_key_time = controller.start_time +
                   cycle_offset(controller.cycle_type(),
                                scaled - controller.start_time, duration);
    return true;
}

bool sample_rotation(const NifAnimation& transform_data, float time,
                     float out_quaternion[4]) {
    if (transform_data.rotations.interpolation == NifKeyInterpolation::XyzRotations)
        return sample_euler_axes(transform_data.rotation_axes, time, out_quaternion);
    return sample_quaternion_group(transform_data.rotations, time, out_quaternion);
}

NifTransformSample evaluate_transform(const NifAnimation& transform_data,
                                      const NifInterpolator& interpolator, float time) {
    NifTransformSample result;
    NifKeySample translation;
    sample_key_group(transform_data.translations, time, translation);
    resolve_translation(translation, interpolator.pose_translation, result);

    float rotation[kQuaternionComponents] = {1.f, 0.f, 0.f, 0.f};
    if (sample_rotation(transform_data, time, rotation))
        resolve_rotation(rotation, result);
    else if (is_posed(interpolator.pose_rotation[0]))
        resolve_rotation(interpolator.pose_rotation, result);

    NifKeySample scale;
    sample_key_group(transform_data.scales, time, scale);
    resolve_scale(scale, interpolator.pose_scale, result);
    return result;
}

NifTransformSample evaluate_bspline(const NifBSplineSampler& spline,
                                    const NifInterpolator& interpolator, float time) {
    NifTransformSample result;
    const float progress = bspline_progress(spline, time);

    NifKeySample translation;
    if (sample_bspline_channel(spline.translations, static_cast<int>(kPointComponents),
                               progress, translation.values))
        translation.count = kPointComponents;
    resolve_translation(translation, interpolator.pose_translation, result);

    float rotation[kQuaternionComponents] = {1.f, 0.f, 0.f, 0.f};
    if (sample_bspline_channel(spline.rotations, static_cast<int>(kQuaternionComponents),
                               progress, rotation))
        resolve_rotation(rotation, result);
    else if (is_posed(interpolator.pose_rotation[0]))
        resolve_rotation(interpolator.pose_rotation, result);

    NifKeySample scale;
    if (sample_bspline_channel(spline.scales, static_cast<int>(kScalarComponents), progress,
                               scale.values))
        scale.count = kScalarComponents;
    resolve_scale(scale, interpolator.pose_scale, result);
    return result;
}

NifFloatSample evaluate_float(const NifAnimation& channel_data,
                              const NifInterpolator& interpolator, float time) {
    NifFloatSample result;
    NifKeySample sampled;
    sample_key_group(channel_data.channel, time, sampled);
    if (sampled.count == kScalarComponents) {
        result.value     = sampled.values[0];
        result.is_driven = true;
        return result;
    }
    if (!is_posed(interpolator.pose_value)) return result;
    result.value     = interpolator.pose_value;
    result.is_driven = true;
    return result;
}

} // namespace KnC
