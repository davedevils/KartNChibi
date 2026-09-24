#pragma once
// The animation a NIF stream carries what each controller drives the pose its interpolator falls back
#include <cstdint>
#include <string>
#include <vector>

namespace KnC {

// A link that names no block Gamebryo writes minus 1 for an absent reference
constexpr uint32_t kNoLink = 0xFFFFFFFFu;
// The same minus 1 read as a byte offset into an NiStringPalette IDTag field standing at it is empty
constexpr uint32_t kNoStringOffset = 0xFFFFFFFFu;

// 10 1 0 104 the stream version at which NiInterpolator arrived splits every animation family at once
constexpr uint32_t kInterpolatorsFrom = 0x0A010068u;

// Wire values of NiAnimationKey KeyType
enum class NifKeyInterpolation : uint32_t {
    None                  = 0,
    Linear                = 1,
    Quadratic             = 2,
    TensionContinuityBias = 3,
    XyzRotations          = 4,
    Step                  = 5,
};

// How a controller repeats once time runs past its stop time
enum class NifCycleType : uint32_t { Loop = 0, Reverse = 1, Clamp = 2 };

// NiTimeController flags bits 1 2 hold cycle type bit 3 the active flag
constexpr uint16_t kCycleTypeShift      = 1;
constexpr uint16_t kCycleTypeMask       = 0x3;
constexpr uint16_t kControllerActiveBit = 0x8;

// One animated channel values holds components floats per key 1 for float bool 3 position 4 quaternion
struct NifKeyGroup {
    NifKeyInterpolation interpolation = NifKeyInterpolation::None;
    uint32_t            components    = 0;
    std::vector<float>  times;
    std::vector<float>  values;
    // Per key forward then backward tangent of quadratic key or its tension bias continuity
    std::vector<float>  interpolation_parameters;

    size_t key_count() const { return times.size(); }
};

// NiTimeController the block it drives the interpolator it samples and the window it runs in
struct NifController {
    uint32_t target_link       = kNoLink;
    uint32_t next_link         = kNoLink;
    // NiSingleInterpController only below 10 1 0 104 where interpolators do not exist same slot holds key data
    uint32_t interpolator_link = kNoLink;
    // NiPSysEmitterCtlr only the bool interpolator that says whether the emitter runs this frame
    uint32_t visibility_interpolator_link = kNoLink;
    uint16_t flags             = 0;
    float    frequency         = 1.f;
    float    phase             = 0.f;
    float    start_time        = 0.f;
    float    stop_time         = 0.f;

    NifCycleType cycle_type() const {
        return static_cast<NifCycleType>((flags >> kCycleTypeShift) & kCycleTypeMask);
    }
    bool is_active() const { return (flags & kControllerActiveBit) != 0; }
};

// NiInterpolator the key data it samples and pose it holds when that data has no keys
struct NifInterpolator {
    uint32_t data_link = kNoLink;
    float pose_value = 0.f;
    float pose_translation[3] = {0.f, 0.f, 0.f};
    float pose_rotation[4] = {1.f, 0.f, 0.f, 0.f};
    float pose_colour[4] = {0.f, 0.f, 0.f, 0.f};
    float pose_scale = 1.f;
};

// Which component of texture matrix a NiTextureTransformController writes wire values
enum class NifTextureTransformOperation : uint32_t {
    TranslateU = 0,
    TranslateV = 1,
    Rotate     = 2,
    ScaleU     = 3,
    ScaleV     = 4,
};

// NiTextureTransformController only the map it retargets and matrix component its float drives
struct NifTextureTransform {
    uint32_t map_index = 0;
    bool     is_shader_map = false;
    NifTextureTransformOperation operation = NifTextureTransformOperation::TranslateU;
};

// NiFlipController the map it swaps and the NiSourceTexture blocks its float keys index truncated
struct NifTextureFlip {
    uint32_t              affected_map = 0;
    std::vector<uint32_t> textures;
};

// One NiMorphData frame the name the artist gave it and three floats a vertex
struct NifMorphTarget {
    std::string        name;
    std::vector<float> vectors;
};

// NiMorphData the base frame first then the targets and whether those hold offsets
struct NifMorphData {
    uint32_t vertex_count     = 0;
    // Relative targets frame 0 is the shape and the rest are offsets added to it
    bool     relative_targets = true;
    std::vector<NifMorphTarget> targets;
};

// NiGeomMorpherController the NiMorphData it poses and one weight interpolator a frame
struct NifGeomMorpher {
    uint32_t              data_link = kNoLink;
    std::vector<uint32_t> weight_interpolator_links;
    bool                  always_update = false;
};

// A channel a NiBSplineTransformInterpolator leaves undriven wire value 16 bits wide even 32 field
constexpr uint32_t kNoBSplineChannel = 0xFFFFu;

// Where one channel control points start in NiBSplineData array quantisation bias plus multiplier point
struct NifBSplineChannel {
    uint32_t offset     = kNoBSplineChannel;
    float    bias       = 0.f;
    float    multiplier = 1.f;

    bool is_driven() const { return offset != kNoBSplineChannel; }
};

// NiBSplineTransformInterpolator cubic B spline over own time window 103 of 139 client streams animate through these
struct NifBSplineTransform {
    float    start_time = 0.f;
    float    stop_time  = 0.f;
    uint32_t data_link  = kNoLink;
    uint32_t basis_link = kNoLink;
    NifBSplineChannel translation;
    NifBSplineChannel rotation;
    NifBSplineChannel scale;
};

// One NiControllerSequence entry the interpolator to play and node it drives target name empty below 20 1 0 1
struct NifSequenceEntry {
    std::string target_name;
    uint32_t    interpolator_link = kNoLink;
    uint32_t    controller_link   = kNoLink;
    // Before 20 1 0 1 an IDTag writes five strings as byte offsets into NiStringPalette only node has consumer
    uint32_t    palette_link       = kNoLink;
    uint32_t    target_name_offset = kNoStringOffset;
};

// One NiTextKeyExtraData key a moment on clip own time axis and the word client animation callback dispatches
struct NifTextKey {
    float       time = 0.f;
    std::string text;
};

// One NiControllerSequence entry one named animation of kf stream and the interpolators it plays
struct NifSequence {
    std::string  accumulation_root_name;
    NifCycleType cycle      = NifCycleType::Loop;
    float        weight     = 1.f;
    float        frequency  = 1.f;
    float        start_time = 0.f;
    float        stop_time  = 0.f;
    // The NiTextKeyExtraData block holding the moments above which stands apart from sequence shared by nothing
    uint32_t     text_keys_link = kNoLink;
    std::vector<NifSequenceEntry> entries;
};

// What one block contributes to an animation a block is exactly one of controller interpolator key data or sequence
struct NifAnimation {
    NifController   controller;
    NifInterpolator interpolator;
    // Single channel data NiFloatData NiPosData NiColorData NiBoolData
    NifKeyGroup channel;
    // NiTransformData and NiKeyframeData drive three channels at once
    NifKeyGroup translations;
    NifKeyGroup rotations;
    NifKeyGroup scales;
    // Filled instead of rotations when that group reports XyzRotations one float channel per axis
    std::vector<NifKeyGroup> rotation_axes;
    // NiTextureTransformController only
    NifTextureTransform texture_transform;
    // NiFlipController only
    NifTextureFlip texture_flip;
    // NiGeomMorpherController only
    NifGeomMorpher morpher;
    // NiMorphData only
    NifMorphData morph;
    // NiControllerSequence only
    NifSequence sequence;
    // NiTextKeyExtraData only the named moments of whichever sequence links this block
    std::vector<NifTextKey> text_keys;
    // NiBSplineTransformInterpolator only data link stays kNoLink on every other block says interpolator not spline
    NifBSplineTransform bspline;
    // NiBSplineData only the control points every spline in the stream indexes into
    std::vector<int16_t> compact_control_points;
    std::vector<float>   control_points;
    // NiBSplineBasisData only how many control points one of its curves spans which fixes knot vector
    uint32_t basis_control_points = 0;
};

} // namespace KnC
