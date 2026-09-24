#include "engine/render/model_animation.h"

#include "engine/formats/nif_animation_eval.h"

#include <algorithm>
#include <cmath>

namespace KnC::Render {

namespace {

constexpr float kIdentityMatrix[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};

// Key axis time controller stands at window start 2151 inactive
float key_time_of(const AnimatedChannel& channel, float seconds) {
    float key_time = channel.controller.start_time;
    map_controller_time(channel.controller, seconds, key_time);
    return key_time;
}

// Undriven channels keep node own transform snap identity
NifTransform posed_transform(const AnimatedNode& node, float seconds) {
    // A node kept only for its billboard facing carries no keys and no interpolator pose
    if (!node.has_channel) return node.local;
    const NifTransformSample sample = evaluate_transform(
        node.channel.keys, node.channel.interpolator, key_time_of(node.channel, seconds));
    NifTransform posed = node.local;
    if (sample.has_translation)
        for (int axis = 0; axis < 3; ++axis)
            posed.translation[axis] = sample.transform.translation[axis];
    if (sample.has_rotation)
        for (int cell = 0; cell < 9; ++cell)
            posed.rotation[cell] = sample.transform.rotation[cell];
    if (sample.has_scale) posed.scale = sample.transform.scale;
    return posed;
}

float driven_value(const AnimatedChannel& channel, float seconds, float rest) {
    const NifFloatSample sample =
        evaluate_float(channel.keys, channel.interpolator, key_time_of(channel, seconds));
    return sample.is_driven ? sample.value : rest;
}

// Samples one animation into one pose member per stage
class ModelAnimationSampler {
public:
    ModelAnimationSampler(const ModelAnimation& animation, float seconds, ModelPose& out)
        : animation_(animation), seconds_(seconds), out_(out) {}

    void sample_all();

private:
    void  sample_node(size_t index);
    void  sample_uv(size_t index);
    void  sample_morph(size_t index);
    void  take_billboard_chain(size_t index, const float placed[16]);
    // Value one texture matrix component at channel rest
    float uv_component(int index, float rest) const;

    const ModelAnimation& animation_;
    float                 seconds_;
    ModelPose&            out_;
};

void ModelAnimationSampler::sample_node(size_t index) {
    const AnimatedNode& node = animation_.nodes[index];
    float local[16];
    transform_matrix(posed_transform(node, seconds_), local);
    float placed[16];
    multiply_matrix(node.rest, local, placed);
    const float* parent = kIdentityMatrix;
    if (node.parent >= 0) parent = &out_.node_world[static_cast<size_t>(node.parent) * 16];
    multiply_matrix(parent, placed, &out_.node_world[index * 16]);
    take_billboard_chain(index, placed);
}

// A billboard node owns itself and hands every node below it the chain since that node
void ModelAnimationSampler::take_billboard_chain(size_t index, const float placed[16]) {
    float* below = &out_.below_billboard[index * 16];
    std::copy(kIdentityMatrix, kIdentityMatrix + 16, below);
    const AnimatedNode& node = animation_.nodes[index];
    out_.node_billboard[index] = node.billboard;
    if (node.billboard != BillboardFacing::None) {
        out_.billboard_owner[index] = static_cast<int>(index);
        return;
    }
    const int parent = node.parent;
    if (parent < 0 || out_.billboard_owner[static_cast<size_t>(parent)] < 0) {
        out_.billboard_owner[index] = -1;
        return;
    }
    out_.billboard_owner[index] = out_.billboard_owner[static_cast<size_t>(parent)];
    multiply_matrix(&out_.below_billboard[static_cast<size_t>(parent) * 16], placed, below);
}

// Gamebryo poses the shape as frame 0 plus every other frame times its own weight
void ModelAnimationSampler::sample_morph(size_t index) {
    const MorphAnimation& channel = animation_.morph_channels[index];
    std::vector<float>& weights = out_.morph_weights[index];
    weights.resize(channel.weights.size());
    for (size_t frame = 0; frame < channel.weights.size(); ++frame)
        weights[frame] = driven_value(channel.weights[frame], seconds_, 0.f);
}

float ModelAnimationSampler::uv_component(int index, float rest) const {
    if (index < 0 || static_cast<size_t>(index) >= animation_.texture_floats.size()) return rest;
    return driven_value(animation_.texture_floats[index], seconds_, rest);
}

// NiTexturingProperty Map TransformMethod 1 Max composes centre
void ModelAnimationSampler::sample_uv(size_t index) {
    const UvAnimation& channel = animation_.uv_channels[index];
    const float scale_u = uv_component(channel.scale_u, channel.rest.scale[0]);
    const float scale_v = uv_component(channel.scale_v, channel.rest.scale[1]);
    const float angle = uv_component(channel.rotate, channel.rest.rotation);
    const float turn_cos = std::cos(angle);
    const float turn_sin = std::sin(angle);
    UvMatrix& matrix = out_.uv[index];
    matrix.rows[0] = scale_u * turn_cos;
    matrix.rows[1] = scale_u * -turn_sin;
    matrix.rows[2] = scale_v * turn_sin;
    matrix.rows[3] = scale_v * turn_cos;
    matrix.offset[0] = uv_component(channel.translate_u, channel.rest.translation[0]);
    matrix.offset[1] = uv_component(channel.translate_v, channel.rest.translation[1]);
    matrix.offset[2] = channel.rest.centre[0];
    matrix.offset[3] = channel.rest.centre[1];
}

void ModelAnimationSampler::sample_all() {
    out_.node_world.resize(animation_.nodes.size() * 16);
    out_.below_billboard.resize(animation_.nodes.size() * 16);
    out_.billboard_owner.assign(animation_.nodes.size(), -1);
    out_.node_billboard.assign(animation_.nodes.size(), BillboardFacing::None);
    // Node parent always precedes collector pushes animated node once
    for (size_t index = 0; index < animation_.nodes.size(); ++index) sample_node(index);
    out_.morph_weights.resize(animation_.morph_channels.size());
    for (size_t index = 0; index < animation_.morph_channels.size(); ++index)
        sample_morph(index);
    out_.uv.resize(animation_.uv_channels.size());
    for (size_t index = 0; index < animation_.uv_channels.size(); ++index) sample_uv(index);
    out_.alpha.resize(animation_.alpha_channels.size());
    for (size_t index = 0; index < animation_.alpha_channels.size(); ++index)
        out_.alpha[index] = driven_value(animation_.alpha_channels[index], seconds_, 1.f);
    out_.flip_frame.resize(animation_.flip_channels.size());
    for (size_t index = 0; index < animation_.flip_channels.size(); ++index) {
        // NiFlipController Update the float value truncated picks the file none below the first
        const FlipAnimation& flip = animation_.flip_channels[index];
        const float value = driven_value(flip.channel, seconds_, -1.f);
        const int last = static_cast<int>(flip.textures.size()) - 1;
        out_.flip_frame[index] = value < 0.f ? -1 : std::min(static_cast<int>(value), last);
    }
}

} // namespace

// modes 3 4 face view centre like rigid modes 5 9 are Bethesda spellings of rotate about up mode 1
BillboardFacing billboard_facing(uint16_t mode) {
    switch (mode) {
        case 0: return BillboardFacing::FaceCamera;
        case 1: return BillboardFacing::RotateAboutUp;
        case 2: return BillboardFacing::RigidFaceCamera;
        case 3: return BillboardFacing::RigidFaceCamera;
        case 4: return BillboardFacing::RigidFaceCamera;
        case 5: return BillboardFacing::RotateAboutUp;
        case 9: return BillboardFacing::RotateAboutUp;
        default: break;
    }
    return BillboardFacing::FaceCamera;
}

void multiply_matrix(const float left[16], const float right[16], float out[16]) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.f;
            for (int step = 0; step < 4; ++step)
                sum += left[step * 4 + row] * right[column * 4 + step];
            out[column * 4 + row] = sum;
        }
    }
}

// NifTransform holds rotation row by row scales uniform
void transform_matrix(const NifTransform& transform, float out[16]) {
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row)
            out[column * 4 + row] = transform.scale * transform.rotation[row * 3 + column];
        out[column * 4 + 3] = 0.f;
    }
    for (int axis = 0; axis < 3; ++axis) out[12 + axis] = transform.translation[axis];
    out[15] = 1.f;
}

void evaluate_model_animation(const ModelAnimation& animation, float seconds, ModelPose& out) {
    ModelAnimationSampler(animation, seconds, out).sample_all();
}

void AnimationClock::advance(float real_seconds) {
    if (!running_) return;
    seconds_ += real_seconds * speed_;
}

void AnimationClock::set_seconds(float seconds) { seconds_ = std::max(seconds, 0.f); }

void AnimationClock::set_speed(float speed) { speed_ = std::max(speed, 0.f); }

}
