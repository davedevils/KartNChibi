#pragma once
// What one model animates node transform texture matrix alpha
#include "engine/formats/nif_animation.h"
#include "engine/formats/nif_reader.h"

#include <vector>

namespace KnC::Render {

// One controller resolved out stream window pose keys
struct AnimatedChannel {
    NifController   controller;
    NifInterpolator interpolator;
    NifAnimation    keys;
};

// How a NiBillboardNode composes its facing three shapes cover the seven wire modes
enum class BillboardFacing : uint8_t { None, FaceCamera, RotateAboutUp, RigidFaceCamera };

// Wire mode to facing 0 faces the camera 1 turns about up 2 takes the camera axes
BillboardFacing billboard_facing(uint16_t mode);

// Node NiTransformController drives rest static placement animated
struct AnimatedNode {
    int   parent = -1;   // index into ModelAnimation nodes -1 at model root
    float rest[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                      0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    // Transform node authored channel keeps 118960 interpolators pose
    NifTransform    local;
    AnimatedChannel channel;
    // False for a node kept only for its billboard facing its local transform stands
    bool            has_channel = false;
    // NiBillboardNode the view replaces the world rotation of this node every frame
    BillboardFacing billboard = BillboardFacing::None;
};

// NiGeomMorpherController one weight channel per frame of the NiMorphData it poses
struct MorphAnimation {
    std::vector<AnimatedChannel> weights;
    // true means frame 0 is the shape and later frames are offsets from it
    bool relative_targets = true;
};

// Texture matrix NiTexturingProperty components controllers replace
struct UvAnimation {
    NifTextureMatrix rest;
    int translate_u = -1;   // indices into ModelAnimation texture floats
    int translate_v = -1;
    int rotate      = -1;
    int scale_u     = -1;
    int scale_v     = -1;
};

// NiFlipController on the base map of a mesh part the float keys truncated pick one of the files
struct FlipAnimation {
    AnimatedChannel          channel;
    std::vector<std::string> textures;
};

// Everything one model animates empty 592 of 782 prop static
struct ModelAnimation {
    std::vector<AnimatedNode>    nodes;
    std::vector<AnimatedChannel> texture_floats;
    std::vector<UvAnimation>     uv_channels;
    std::vector<AnimatedChannel> alpha_channels;
    std::vector<MorphAnimation>  morph_channels;
    std::vector<FlipAnimation>   flip_channels;

    bool empty() const {
        return nodes.empty() && uv_channels.empty() && alpha_channels.empty() &&
               morph_channels.empty() && flip_channels.empty();
    }
};

// Which animated channels drive one part model -1 rest
struct PartAnimation {
    int node  = -1;
    int uv    = -1;
    int alpha = -1;
    // Index into ModelAnimation morph channels a morphed part owns its own buffer
    int morph = -1;
    // Index into ModelAnimation flip channels the base map swaps by its keys
    int flip  = -1;
    // Index into ModelAnimation uv channels for the detail map minus one for the rest matrix
    int detail_uv = -1;
};

// Texture matrix shader takes uv centre rows uv translation
struct UvMatrix {
    float rows[4]   = {1.f, 0.f, 0.f, 1.f};   // rows are row major m00 m01 m10 m11 offset is translation u v then centre u v
    float offset[4] = {0.f, 0.f, 0.f, 0.f};
};

// What model animation holds one time sized animation refreshed
struct ModelPose {
    std::vector<float>    node_world;   // 16 column-major floats per animated node
    std::vector<UvMatrix> uv;
    std::vector<float>    alpha;
    // The file index each flip channel names this frame minus one keeps the authored map
    std::vector<int>      flip_frame;
    // One weight per frame of each morph channel
    std::vector<std::vector<float>> morph_weights;
    // Which animated node up the chain carries the billboard minus one for none
    std::vector<int>      billboard_owner;
    // The facing of each animated node copied so a draw needs the pose alone
    std::vector<BillboardFacing> node_billboard;
    // The chain below that billboard node 16 floats a node identity at the node itself
    std::vector<float>    below_billboard;
};

// Node transform column major 4x4 bgfx setTransform
void transform_matrix(const NifTransform& transform, float out[16]);

// out left right both column major out must alias neither
void multiply_matrix(const float left[16], const float right[16], float out[16]);

// Samples every channel animation seconds sizes out
void evaluate_model_animation(const ModelAnimation& animation, float seconds, ModelPose& out);

// Clock animation sampled at real seconds never day night
class AnimationClock {
public:
    void advance(float real_seconds);
    void set_seconds(float seconds);
    void set_speed(float speed);
    void toggle_running() { running_ = !running_; }
    void set_running(bool running) { running_ = running; }

    float seconds() const { return seconds_; }
    float speed() const { return speed_; }
    bool  running() const { return running_; }

private:
    float seconds_ = 0.f;
    float speed_   = 1.f;
    bool  running_ = true;
};

}
