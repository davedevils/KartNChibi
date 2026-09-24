#pragma once
// One NiParticleSystem stepped as HBOnline does it at a fixed 60 Hz
#include "engine/render/particle_system.h"

#include <cstdint>
#include <vector>

namespace KnC::Render {

// Live particle with NiParticleInfo and four per particle arrays one record
struct Particle {
    float position[3] = {0.f, 0.f, 0.f};
    float velocity[3] = {0.f, 0.f, 0.f};
    float colour[4]   = {1.f, 1.f, 1.f, 1.f};
    float radius      = 0.f;   // radius is NiPSysData at 0x44 size is NiPSysData at 0x4c stored by grow fade modifier
    float size        = 1.f;
    float angle       = 0.f;   // angle is NiPSysData at 0x54 radians spin is NiPSysData at 0x60 radians per second
    float spin        = 0.f;
    float age         = 0.f;
    float life_span   = 0.f;
    float last_update = 0.f;
    uint16_t generation = 0;
};

// Simulation steps at 60 Hz fixed size makes scrub repeatable
constexpr float kParticleStepSeconds = 1.f / 60.f;

// Pool behind this many steps caught up by restart not step
constexpr int kMaxCatchUpSteps = 4096;

class ParticleSimulation {
public:
    // Steps to absolute seconds a world space system births land in world space through the emitter matrix
    void advance_to(const ParticleSystemDefinition& definition, float seconds,
                    const float* emitter_world = nullptr);

    const std::vector<Particle>& particles() const { return particles_; }
    float seconds() const { return now_; }

private:
    void restart();
    void step_once(float to_seconds);
    void emit_batch(float now);
    void emit_one(float age, float now);
    void apply_modifier(const NifPSysModifier& modifier, float now);
    void apply_age_death(float now);
    void apply_position(float now);
    void apply_gravity(const NifPSysGravity& gravity, float now);
    void apply_grow_fade(const NifPSysGrowFade& grow_fade);
    void apply_rotation(float now);
    void apply_colour();
    // Birth rate at now or 0 when emitter drives none
    float birth_rate_at(float now) const;
    // False while the emitter active interpolator of the controller reads false
    bool emitter_runs(float now) const;
    // The emitter controller with the drive frequency over the authored one
    KnC::NifController birth_controller() const;
    // Scaled seconds the emission counts on clamp stops at the end loop runs on
    float emission_time(float now) const;
    void sample_birth_position(const NifPSysEmitter& emitter, float out[3]);
    // Client two ranges off rand full width for life speed plus minus
    float half_width_range();
    float symmetric_range();
    float next_unit();

    const ParticleSystemDefinition* definition_ = nullptr;
    // Valid during one advance call only the world of the placement that emits
    const float* emitter_world_ = nullptr;
    // World space pool the birth and placement scale the gravity like the velocity
    float world_scale_ = 1.f;
    std::vector<Particle> particles_;
    float    now_       = 0.f;
    uint32_t random_    = 0;
    // Emission time at the last step the count is the difference of two truncated totals
    float    last_emission_ = 0.f;
};

}
