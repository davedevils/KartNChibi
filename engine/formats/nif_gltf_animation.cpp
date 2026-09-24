// Every clip resampled 103 of 139 character kf B spline key groups
#include "engine/formats/nif_gltf_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace KnC {

namespace {

// LINEAR sampler fewest two samples constant channel never changes collapsed
constexpr int64_t kFewestSamples = 2;
constexpr int64_t kMostSamples   = 8192;

// Clip channel sampled across window component not driven stays empty
struct SampledChannel {
    // translations and scales 3 floats a sample rotations 4
    std::vector<float> translations;
    std::vector<float> rotations;
    std::vector<float> scales;
    // Step key group holds value until next LINEAR ramps glTF STEP
    GltfInterpolation translation_curve = GltfInterpolation::Linear;
    GltfInterpolation rotation_curve    = GltfInterpolation::Linear;
    GltfInterpolation scale_curve       = GltfInterpolation::Linear;
};

// Channel carrying spline ignores keys curve not step however groups
GltfInterpolation curve_of(const NifSkeletonChannel& channel, const NifKeyGroup& group) {
    if (!channel.spline.is_empty()) return GltfInterpolation::Linear;
    return group.interpolation == NifKeyInterpolation::Step ? GltfInterpolation::Step
                                                           : GltfInterpolation::Linear;
}

// Sample stands in clip 0 at first 1 at last
float sample_fraction(uint32_t step, uint32_t samples) {
    return static_cast<float>(step) / static_cast<float>(samples - 1);
}

uint32_t sample_count(float duration, float rate) {
    const int64_t steps = std::lround(static_cast<double>(duration) * rate) + 1;
    return static_cast<uint32_t>(std::clamp(steps, kFewestSamples, kMostSamples));
}

// Curves glTF cannot spell channel authored spline counted first ignores
void count_curve(const NifSkeletonChannel& channel, NifGltfReport& report) {
    if (!channel.spline.is_empty()) {
        ++report.bspline_channels;
        return;
    }
    const NifKeyInterpolation groups[3] = {channel.keys.translations.interpolation,
                                           channel.keys.rotations.interpolation,
                                           channel.keys.scales.interpolation};
    for (const NifKeyInterpolation interpolation : groups)
        if (interpolation == NifKeyInterpolation::Quadratic ||
            interpolation == NifKeyInterpolation::TensionContinuityBias) {
            ++report.curved_key_channels;
            return;
        }
    ++report.linear_key_channels;
}

// Quaternion sampled hemisphere flip LINEAR long way round keep same
void keep_hemisphere(const std::vector<float>& rotations, float quaternion[4]) {
    if (rotations.size() < 4) return;
    const size_t previous = rotations.size() - 4;
    float dot = 0.f;
    for (int component = 0; component < 4; ++component)
        dot += rotations[previous + component] * quaternion[component];
    if (dot >= 0.f) return;
    for (int component = 0; component < 4; ++component)
        quaternion[component] = -quaternion[component];
}

void append_rotation(const NifTransform& transform, std::vector<float>& out) {
    float quaternion[4];
    gltf_quaternion_of(transform.rotation, quaternion);
    keep_hemisphere(out, quaternion);
    out.insert(out.end(), quaternion, quaternion + 4);
}

// Channel components driven read once at clip start held whole
void sample_channel(const NifSkeletonChannel& channel, const NifSkeletonClip& clip,
                    uint32_t samples, SampledChannel& out) {
    const NifTransformSample driven = sample_nif_channel(channel, clip.start_time);
    out.translation_curve = curve_of(channel, channel.keys.translations);
    out.rotation_curve = curve_of(channel, channel.keys.rotations);
    out.scale_curve = curve_of(channel, channel.keys.scales);
    for (uint32_t step = 0; step < samples; ++step) {
        const NifTransformSample sample = sample_nif_channel(
            channel, clip.start_time + clip.duration() * sample_fraction(step, samples));
        if (driven.has_translation)
            out.translations.insert(out.translations.end(), sample.transform.translation,
                                    sample.transform.translation + 3);
        if (driven.has_rotation) append_rotation(sample.transform, out.rotations);
        if (driven.has_scale)
            for (int axis = 0; axis < 3; ++axis) out.scales.push_back(sample.transform.scale);
    }
}

// Sample holds first one constant bone poses never moves samples
bool is_constant(const std::vector<float>& values, uint32_t width) {
    for (size_t index = width; index < values.size(); ++index)
        if (values[index] != values[index % width]) return false;
    return true;
}

// Clip key axis as glTF time zero start divided by frequency
std::vector<float> sample_times(const NifSkeletonClip& clip, uint32_t samples) {
    const float frequency = clip.frequency != 0.f ? clip.frequency : 1.f;
    std::vector<float> times(samples);
    // Fraction samples taken reader multiply time by frequency lands moment
    for (uint32_t step = 0; step < samples; ++step)
        times[step] = clip.duration() * sample_fraction(step, samples) / frequency;
    return times;
}

// glTF animation being filled bundled append path three arguments drift
class ClipWriter {
public:
    ClipWriter(GltfDocument& document, NifGltfReport& report) : document_(document), report_(report) {}

    void begin(const std::string& name, std::vector<float> times);
    void add_path(uint32_t node, GltfPath path, const std::vector<float>& values);
    void set_curve(GltfInterpolation curve) { curve_ = curve; }
    // False when clip drove nothing clip glTF must not carry
    bool finish();

private:
    GltfDocument&      document_;
    NifGltfReport&     report_;
    GltfAnimation      animation_;
    std::vector<float> times_;
    GltfInterpolation  curve_ = GltfInterpolation::Linear;
    std::set<std::pair<uint32_t, uint8_t>> driven_;
};

void ClipWriter::begin(const std::string& name, std::vector<float> times) {
    animation_ = GltfAnimation();
    animation_.name = name;
    times_ = std::move(times);
    driven_.clear();
}

void ClipWriter::add_path(uint32_t node, GltfPath path, const std::vector<float>& values) {
    if (values.empty()) return;
    const uint32_t width = path == GltfPath::Rotation ? 4u : 3u;
    if (values.size() != times_.size() * width) return;
    // glTF allows one channel per node path stream can name twice
    if (!driven_.emplace(node, static_cast<uint8_t>(path)).second) {
        ++report_.duplicate_channels;
        return;
    }
    std::vector<float> times = times_;
    std::vector<float> sampled = values;
    if (is_constant(values, width)) {
        times = {times_.front(), times_.back()};
        sampled.assign(values.begin(), values.begin() + width);
        sampled.insert(sampled.end(), values.begin(), values.begin() + width);
    }
    GltfAnimationSampler sampler;
    sampler.interpolation = curve_;
    sampler.input = gltf_add_floats(document_, times, GltfElement::Scalar, GltfTarget::None);
    sampler.output = gltf_add_floats(
        document_, sampled, width == 4 ? GltfElement::Vec4 : GltfElement::Vec3, GltfTarget::None);
    animation_.samplers.push_back(sampler);
    animation_.channels.push_back(
        {static_cast<uint32_t>(animation_.samplers.size() - 1), node, path});
}

bool ClipWriter::finish() {
    if (animation_.channels.empty()) return false;
    report_.animation_channels += animation_.channels.size();
    ++report_.animations;
    document_.animations.push_back(std::move(animation_));
    return true;
}

bool is_transform_controller(const std::string& type) {
    return type == "NiTransformController" || type == "NiKeyframeController";
}

// Every clip one model namespace of names two kf files idle
class AnimationCollector {
public:
    AnimationCollector(const NifSkeleton& skeleton, NifGltfResult& out, float rate)
        : skeleton_(skeleton), out_(out), rate_(rate), writer_(out.document, out.report) {}

    void add_sequences(const NifScene& stream, const std::string& stream_name);
    void add_controllers(const NifScene& model);

private:
    void add_clip(NifSkeletonClip& clip);
    std::string unique_name(const std::string& wanted);
    bool bind_controller(const NifScene& model, const NifBlock& block, NifSkeletonChannel& out);

    const NifSkeleton&    skeleton_;
    NifGltfResult&        out_;
    float                 rate_;
    ClipWriter            writer_;
    std::set<std::string> taken_;
};

std::string AnimationCollector::unique_name(const std::string& wanted) {
    const std::string name = wanted.empty() ? "clip" : wanted;
    if (taken_.insert(name).second) return name;
    for (int suffix = 2;; ++suffix) {
        const std::string tried = name + "#" + std::to_string(suffix);
        if (taken_.insert(tried).second) return tried;
    }
}

void AnimationCollector::add_clip(NifSkeletonClip& clip) {
    if (!(clip.duration() > 0.f)) {
        ++out_.report.clips_without_duration;
        return;
    }
    if (clip.frequency != 1.f) ++out_.report.clips_with_frequency;
    const uint32_t samples = sample_count(clip.duration(), rate_);
    writer_.begin(unique_name(clip.name), sample_times(clip, samples));
    for (const NifSkeletonChannel& channel : clip.channels) {
        if (channel.node < 0) continue;
        count_curve(channel, out_.report);
        SampledChannel sampled;
        sample_channel(channel, clip, samples, sampled);
        const uint32_t node = static_cast<uint32_t>(channel.node);
        writer_.set_curve(sampled.translation_curve);
        writer_.add_path(node, GltfPath::Translation, sampled.translations);
        writer_.set_curve(sampled.rotation_curve);
        writer_.add_path(node, GltfPath::Rotation, sampled.rotations);
        writer_.set_curve(sampled.scale_curve);
        writer_.add_path(node, GltfPath::Scale, sampled.scales);
    }
    writer_.finish();
}

void AnimationCollector::add_sequences(const NifScene& stream, const std::string& stream_name) {
    for (const uint32_t sequence : find_nif_sequences(stream)) {
        NifSkeletonClip clip;
        std::size_t unbound = 0;
        if (!bind_nif_sequence(stream, sequence, skeleton_, clip, unbound)) continue;
        out_.report.unbound_sequence_entries += unbound;
        if (clip.name.empty()) clip.name = stream_name;
        add_clip(clip);
    }
}

// Controller model stream carries itself not kf 190 of 782 meshes
bool AnimationCollector::bind_controller(const NifScene& model, const NifBlock& block,
                                         NifSkeletonChannel& out) {
    const NifController& controller = block.animation->controller;
    out.node = skeleton_.node_of(controller.target_link);
    if (out.node < 0) return false;
    const NifAnimation* interpolator = find_animation(model, controller.interpolator_link);
    if (interpolator == nullptr) return false;
    out.interpolator = interpolator->interpolator;
    const NifAnimation* keys = find_animation(model, interpolator->interpolator.data_link);
    if (keys != nullptr) out.keys = *keys;
    if (interpolator->bspline.data_link == kNoLink) return true;
    if (resolve_nif_bspline(model, *interpolator, out.spline)) return true;
    ++out_.report.unbound_sequence_entries;
    return false;
}

void AnimationCollector::add_controllers(const NifScene& model) {
    NifSkeletonClip clip;
    clip.name = "controllers";
    bool spanned = false;
    for (const NifBlock& block : model.blocks) {
        if (block.animation == nullptr || !is_transform_controller(block.type)) continue;
        NifSkeletonChannel channel;
        if (!bind_controller(model, block, channel)) continue;
        const NifController& controller = block.animation->controller;
        clip.start_time = spanned ? std::min(clip.start_time, controller.start_time)
                                  : controller.start_time;
        clip.stop_time = spanned ? std::max(clip.stop_time, controller.stop_time)
                                 : controller.stop_time;
        spanned = true;
        clip.channels.push_back(std::move(channel));
    }
    if (clip.channels.empty()) return;
    add_clip(clip);
}

} // namespace

void append_nif_gltf_animations(const NifGltfRequest& request, const NifSkeleton& skeleton,
                                NifGltfResult& out) {
    AnimationCollector collector(skeleton, out, request.options.sample_rate);
    collector.add_sequences(*request.model, "model");
    collector.add_controllers(*request.model);
    for (const NifGltfClipStream& clip : request.clips) {
        if (clip.stream == nullptr) continue;
        collector.add_sequences(*clip.stream, clip.name);
    }
}

} // namespace KnC
