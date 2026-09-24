#include "engine/render/particle_simulation.h"

#include "engine/formats/nif_animation_eval.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace KnC::Render {

namespace {

// NiPSysGrowFadeModifier clamps scale up not zero particle small
constexpr float kSmallestGrowFadeScale = 1.0e-4f;
// Seed every pool starts from client draws rand fixed
constexpr uint32_t kRandomSeed = 0x9E3779B9u;
constexpr float kTwoPi = 6.28318530718f;

// One seed per system name so two systems of one nif do not draw the same points
uint32_t seed_of(const std::string& name) {
    uint32_t seed = kRandomSeed;
    for (const char value : name) seed = seed * 16777619u + static_cast<uint8_t>(value);
    return seed != 0u ? seed : kRandomSeed;
}

// Particle spin long time loses precision reset angle
constexpr float kAngleGuard = 1.0e6f;

const NifPSysModifier* first_of_kind(const ParticleSystemDefinition& system,
                                     NifPSysModifierKind kind) {
    for (const NifPSysModifier& modifier : system.modifiers)
        if (modifier.kind == kind) return &modifier;
    return nullptr;
}

bool is_emitter(NifPSysModifierKind kind) {
    return kind == NifPSysModifierKind::BoxEmitter || kind == NifPSysModifierKind::MeshEmitter;
}

// Point one emitter meshes triangles each likely as next
void sample_triangle(const std::vector<float>& triangles, float first, float second,
                     float out[3]) {
    const size_t count = triangles.size() / 9;
    if (count == 0) return;
    const size_t chosen = std::min(static_cast<size_t>(first * static_cast<float>(count)),
                                   count - 1);
    const float* corner = &triangles[chosen * 9];
    float u = first * static_cast<float>(count) - static_cast<float>(chosen);
    float v = second;
    if (u + v > 1.f) {
        u = 1.f - u;
        v = 1.f - v;
    }
    for (int axis = 0; axis < 3; ++axis)
        out[axis] = corner[axis] + u * (corner[3 + axis] - corner[axis]) +
                    v * (corner[6 + axis] - corner[axis]);
}

// Direction through the rotation and scale rows of a placement no translation
void turn_by_matrix(const float m[16], float direction[3]) {
    float turned[3];
    for (int axis = 0; axis < 3; ++axis)
        turned[axis] = direction[0] * m[axis] + direction[1] * m[4 + axis] + direction[2] * m[8 + axis];
    for (int axis = 0; axis < 3; ++axis) direction[axis] = turned[axis];
}

// Uniform scale of a placement the length of its first row
float matrix_scale(const float m[16]) {
    return std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
}

} // namespace

void ParticleSimulation::restart() {
    particles_.clear();
    particles_.reserve(definition_->pool_size);
    now_ = 0.f;
    last_emission_ = emission_time(0.f);
    random_ = seed_of(definition_->name);
}

float ParticleSimulation::next_unit() {
    // xorshift32 client rand not reproducible capture
    random_ ^= random_ << 13;
    random_ ^= random_ >> 17;
    random_ ^= random_ << 5;
    return static_cast<float>(random_ >> 8) / static_cast<float>(1u << 24);
}

float ParticleSimulation::half_width_range() { return next_unit() - 0.5f; }

float ParticleSimulation::symmetric_range() { return next_unit() * 2.f - 1.f; }

void ParticleSimulation::advance_to(const ParticleSystemDefinition& definition,
                                    float seconds, const float* emitter_world) {
    const bool rewound = definition_ != &definition || seconds < now_;
    definition_ = &definition;
    emitter_world_ = definition.world_space ? emitter_world : nullptr;
    world_scale_ = 1.f;
    if (definition.world_space) {
        world_scale_ = matrix_scale(definition.birth);
        if (emitter_world_ != nullptr) world_scale_ *= matrix_scale(emitter_world_);
    }
    if (rewound) restart();
    if (seconds - now_ > kParticleStepSeconds * kMaxCatchUpSteps) {
        // Too far behind walk restart take last window scrub
        restart();
        now_ = seconds - kParticleStepSeconds * kMaxCatchUpSteps;
        last_emission_ = emission_time(now_);
    }
    while (now_ + kParticleStepSeconds <= seconds) step_once(now_ + kParticleStepSeconds);
    emitter_world_ = nullptr;
}

void ParticleSimulation::step_once(float to_seconds) {
    // Emission first EmitParticles back dates new particle age
    emit_batch(to_seconds);
    for (const NifPSysModifier& modifier : definition_->modifiers)
        apply_modifier(modifier, to_seconds);
    now_ = to_seconds;
}

void ParticleSimulation::apply_modifier(const NifPSysModifier& modifier, float now) {
    switch (modifier.kind) {
        case NifPSysModifierKind::AgeDeath: apply_age_death(now); return;
        case NifPSysModifierKind::Position: apply_position(now); return;
        case NifPSysModifierKind::Gravity:  apply_gravity(modifier.gravity, now); return;
        case NifPSysModifierKind::GrowFade: apply_grow_fade(modifier.grow_fade); return;
        case NifPSysModifierKind::Rotation: apply_rotation(now); return;
        case NifPSysModifierKind::Colour:   apply_colour(); return;
        // Bound update keeps bounding volume emitter controller spawn
        case NifPSysModifierKind::BoundUpdate:
        case NifPSysModifierKind::Spawn:
        case NifPSysModifierKind::BoxEmitter:
        case NifPSysModifierKind::MeshEmitter:
        case NifPSysModifierKind::Unknown: break;
    }
}

// NiPSysAgeDeathModifier Update age forward elapsed time kill
void ParticleSimulation::apply_age_death(float now) {
    for (size_t index = particles_.size(); index-- > 0;) {
        Particle& particle = particles_[index];
        particle.age += now - particle.last_update;
        if (particle.life_span >= particle.age) continue;
        particles_[index] = particles_.back();
        particles_.pop_back();
    }
}

// NiPSysPositionModifier Update plain explicit Euler elapsed time
void ParticleSimulation::apply_position(float now) {
    for (Particle& particle : particles_) {
        const float elapsed = now - particle.last_update;
        for (int axis = 0; axis < 3; ++axis)
            particle.position[axis] += elapsed * particle.velocity[axis];
        particle.last_update = now;
    }
}

// NiPSysGravityModifier Update velocity only attenuated exp force
void ParticleSimulation::apply_gravity(const NifPSysGravity& gravity, float now) {
    // The drive strength over the authored one the dust gravity follows the car speed
    const ParticleDrive* drive = definition_->drive.get();
    const float strength = (drive != nullptr && drive->has_gravity_strength ? drive->gravity_strength
                                                                              : gravity.strength) *
                           world_scale_;
    for (Particle& particle : particles_) {
        float projection = 0.f;
        for (int axis = 0; axis < 3; ++axis)
            projection += particle.position[axis] * gravity.axis[axis];
        projection *= gravity.decay;
        const float attenuation = std::exp(-std::abs(projection));
        const float elapsed = now - particle.last_update;
        for (int axis = 0; axis < 3; ++axis)
            particle.velocity[axis] +=
                gravity.axis[axis] * strength * attenuation * elapsed;
    }
}

// NiPSysGrowFadeModifier Update two linear ramps particle generation
void ParticleSimulation::apply_grow_fade(const NifPSysGrowFade& grow_fade) {
    for (Particle& particle : particles_) {
        float grow = 1.f;
        if (particle.generation == grow_fade.grow_generation && grow_fade.grow_time != 0.f &&
            particle.age < grow_fade.grow_time)
            grow = particle.age / grow_fade.grow_time;
        const float remaining = particle.life_span - particle.age;
        float fade = 1.f;
        if (particle.generation == grow_fade.fade_generation && grow_fade.fade_time != 0.f &&
            remaining < grow_fade.fade_time)
            fade = remaining / grow_fade.fade_time;
        particle.size = std::max(std::min(grow, fade), kSmallestGrowFadeScale);
    }
}

// NiPSysRotationModifier Update angle walks own speed wraps
void ParticleSimulation::apply_rotation(float now) {
    for (Particle& particle : particles_) {
        particle.angle += (now - particle.last_update) * particle.spin;
        if (particle.angle > kAngleGuard) particle.angle = 0.f;
        while (particle.angle > kTwoPi) particle.angle -= kTwoPi;
    }
}

// NiPSysColorModifier Update ramp sampled NORMALISED age clamped
void ParticleSimulation::apply_colour() {
    const NifKeyGroup& ramp = definition_->colour_ramp;
    if (ramp.key_count() == 0) return;
    const float first = ramp.times.front();
    const float last = ramp.times.back();
    for (Particle& particle : particles_) {
        if (particle.life_span <= 0.f) continue;
        const float fraction =
            std::clamp(particle.age / particle.life_span, first, last);
        NifKeySample sample;
        if (!sample_key_group(ramp, fraction, sample) || sample.count < 4) continue;
        for (int channel = 0; channel < 4; ++channel) particle.colour[channel] = sample.values[channel];
    }
}

NifController ParticleSimulation::birth_controller() const {
    NifController controller = definition_->birth_rate.controller;
    const ParticleDrive* drive = definition_->drive.get();
    if (drive != nullptr && drive->has_frequency) controller.frequency = drive->frequency;
    return controller;
}

float ParticleSimulation::birth_rate_at(float now) const {
    if (!definition_->has_birth_rate) return 0.f;
    const AnimatedChannel& channel = definition_->birth_rate;
    const NifController controller = birth_controller();
    float key_time = controller.start_time;
    if (!map_controller_time(controller, now, key_time)) return 0.f;
    const NifFloatSample sample = evaluate_float(channel.keys, channel.interpolator, key_time);
    return sample.is_driven ? std::max(sample.value, 0.f) : 0.f;
}

// A loop never wraps the count a clamp holds it at the window end a reverse walks the window
float ParticleSimulation::emission_time(float now) const {
    if (!definition_->has_birth_rate) return 0.f;
    const NifController controller = birth_controller();
    const float offset = now * controller.frequency + controller.phase - controller.start_time;
    const float duration = controller.stop_time - controller.start_time;
    switch (controller.cycle_type()) {
        case NifCycleType::Loop: return std::max(offset, 0.f);
        case NifCycleType::Clamp: return std::clamp(offset, 0.f, std::max(duration, 0.f));
        case NifCycleType::Reverse: break;
    }
    float key_time = controller.start_time;
    if (!map_controller_time(controller, now, key_time)) return 0.f;
    return key_time - controller.start_time;
}

// The emitter active interpolator of NiPSysEmitterCtlr false stops the births
bool ParticleSimulation::emitter_runs(float now) const {
    if (!definition_->has_birth_visibility) return true;
    const AnimatedChannel& channel = definition_->birth_visibility;
    NifController controller = channel.controller;
    controller.frequency = birth_controller().frequency;
    float key_time = controller.start_time;
    if (!map_controller_time(controller, now, key_time)) return true;
    const NifFloatSample sample = evaluate_float(channel.keys, channel.interpolator, key_time);
    return !sample.is_driven || sample.value >= 0.5f;
}

// Count difference two truncated cumulative totals leftover 334 emit
void ParticleSimulation::emit_batch(float now) {
    if (!definition_->has_birth_rate) return;
    const NifController controller = birth_controller();
    if (!controller.is_active()) return;
    if (!emitter_runs(now)) {
        last_emission_ = emission_time(now);
        return;
    }
    const float rate = birth_rate_at(now);
    const float span = emission_time(now);
    float before = last_emission_;
    last_emission_ = span;
    if (rate <= 0.f || span <= 0.f) return;
    // A reverse cycle walks back down the window the count restarts from the turn
    if (before > span) before = 0.f;
    const int born_before = static_cast<int>(before * rate);
    const int born_total = static_cast<int>(span * rate);
    const float interval = 1.f / rate;
    // The frequency scales the count the age of a newborn stays in real seconds
    const float per_second = controller.frequency > 0.f ? controller.frequency : 1.f;
    for (int index = 0; index < born_total - born_before; ++index)
        emit_one((span - static_cast<float>(born_before + 1 + index) * interval) / per_second, now);
}

// NiPSysEmitter EmitParticles two random ranges not same life
void ParticleSimulation::emit_one(float age, float now) {
    const NifPSysModifier* emitter = nullptr;
    for (const NifPSysModifier& modifier : definition_->modifiers)
        if (is_emitter(modifier.kind)) {
            emitter = &modifier;
            break;
        }
    if (emitter == nullptr) return;
    const NifPSysEmitter& source = emitter->emitter;
    Particle particle;
    particle.life_span = source.life_span + source.life_span_variation * half_width_range();
    if (age > particle.life_span) return;
    if (particles_.size() >= definition_->pool_size) return;

    const float speed = source.speed + source.speed_variation * half_width_range();
    const float declination = source.declination + source.declination_variation * symmetric_range();
    const float planar = source.planar_angle + source.planar_angle_variation * symmetric_range();
    particle.velocity[0] = speed * std::cos(planar) * std::sin(declination);
    particle.velocity[1] = speed * std::sin(planar) * std::sin(declination);
    particle.velocity[2] = speed * std::cos(declination);
    particle.age = age;
    particle.last_update = now - age;
    // The radius comes before the world space birth scales it a later write kept the nif units
    particle.radius = source.initial_radius + source.radius_variation * symmetric_range();
    sample_birth_position(source, particle.position);
    if (definition_->world_space) {
        // World space birth the placement turns and scales the velocity and the size too
        turn_by_matrix(definition_->birth, particle.velocity);
        particle.radius *= matrix_scale(definition_->birth);
        if (emitter_world_ != nullptr) {
            const float* m = emitter_world_;
            float placed[3];
            for (int axis = 0; axis < 3; ++axis)
                placed[axis] = particle.position[0] * m[axis] + particle.position[1] * m[4 + axis] +
                               particle.position[2] * m[8 + axis] + m[12 + axis];
            for (int axis = 0; axis < 3; ++axis) particle.position[axis] = placed[axis];
            turn_by_matrix(m, particle.velocity);
            particle.radius *= matrix_scale(m);
        }
    }
    for (int channel = 0; channel < 4; ++channel)
        particle.colour[channel] = source.initial_colour[channel];
    const NifPSysModifier* rotation = first_of_kind(*definition_, NifPSysModifierKind::Rotation);
    if (rotation != nullptr) {
        particle.angle = rotation->rotation.angle +
                         rotation->rotation.angle_variation * symmetric_range();
        particle.spin = rotation->rotation.speed +
                        rotation->rotation.speed_variation * half_width_range();
        if (rotation->rotation.random_sign && next_unit() < 0.5f) particle.spin = -particle.spin;
    }
    particles_.push_back(particle);
}

void ParticleSimulation::sample_birth_position(const NifPSysEmitter& emitter, float out[3]) {
    float sampled[3] = {0.f, 0.f, 0.f};
    if (!definition_->emitter_triangles.empty())
        sample_triangle(definition_->emitter_triangles, next_unit(), next_unit(), sampled);
    else
        // Box emitter volume width height depth about emitter
        for (int axis = 0; axis < 3; ++axis)
            sampled[axis] = (next_unit() - 0.5f) * emitter.box_extent[axis];
    const float* birth = definition_->birth;
    for (int axis = 0; axis < 3; ++axis)
        out[axis] = birth[axis] * sampled[0] + birth[4 + axis] * sampled[1] +
                    birth[8 + axis] * sampled[2] + birth[12 + axis];
}

}
