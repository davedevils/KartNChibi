#pragma once
// Turns keys nif animation h retains into a value at a time one sample per key group no allocation
#include "engine/formats/nif_animation.h"
#include "engine/formats/nif_reader.h"

#include <cstdint>
#include <vector>

namespace KnC {

// One sampled key four floats because quaternion or RGBA colour is the widest value any NIF channel streams
struct NifKeySample {
    float    values[4] = {0.f, 0.f, 0.f, 0.f};
    uint32_t count     = 0;
};

// What a single float channel holds at one time a texture scroll an alpha fade a morph weight
struct NifFloatSample {
    float value     = 0.f;
    bool  is_driven = false;
};

// What NiTransformData drives at one time 118960 of 129280 client transform interpolators pose no scale
struct NifTransformSample {
    NifTransform transform;
    bool has_translation = false;
    bool has_rotation    = false;
    bool has_scale       = false;
};

// Samples group component by component at time on its own key axis out of range clamps to first or last
bool sample_key_group(const NifKeyGroup& group, float time, NifKeySample& out);

// Samples four component rotation channel at time slerping along shorter arc wire order w x y z always unit
bool sample_quaternion_group(const NifKeyGroup& group, float time,
                             float out_quaternion[4]);

// Maps controller time onto key axis frequency and phase first then cycle type over start time stop time
bool map_controller_time(const NifController& controller, float time,
                         float& out_key_time);

// The rotation a NiTransformData holds at time as quaternion composing three per axis float channels
bool sample_rotation(const NifAnimation& transform_data, float time,
                     float out_quaternion[4]);

// Composes translation rotation and scale at time falling back to interpolator pose for channel with no keys
NifTransformSample evaluate_transform(const NifAnimation& transform_data,
                                      const NifInterpolator& interpolator, float time);

// A NiBSplineTransformInterpolator resolved to the values it interpolates own time window and control points dequantised
struct NifBSplineSampler {
    float start_time = 0.f;
    float stop_time  = 0.f;
    std::vector<float> translations;   // three floats per control point
    std::vector<float> rotations;
    std::vector<float> scales;         // one per control point

    bool is_empty() const {
        return translations.empty() && rotations.empty() && scales.empty();
    }
};

// Same composition off a clamped cubic B spline with unit knots HBOnline FUN 00497da0
NifTransformSample evaluate_bspline(const NifBSplineSampler& spline,
                                    const NifInterpolator& interpolator, float time);

// The value a single channel interpolator holds at time its pose included
NifFloatSample evaluate_float(const NifAnimation& channel_data,
                              const NifInterpolator& interpolator, float time);

} // namespace KnC
