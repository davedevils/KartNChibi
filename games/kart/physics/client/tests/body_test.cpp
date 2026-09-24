// Test body module math leaves catalogue file body create on the real Race 01 spawn and a short launch

#include "../body.h"
#include "../constants.h"
#include "../input.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace KnC::Kart::Client;

namespace {

bool nearly(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) <= eps;
}

constexpr float kPi = 3.14159265f;

// the 17 stats of the recorded ghost kart basic 1 they never reach the catalogue see catalogue from stats
KartStats ghost_stats() {
    const float base[17] = {0.52f, 0.52f, 0.52f, 0.52f, 0.30f, 0.52f, 0.30f, 0.30f, 0.52f,
                            0.52f, 0.52f, 0.70f, 0.52f, 0.0f, 9.0f, 37.0f, 3.5f};
    KartStats stats;
    for (size_t i = 0; i < stats.base.size() && i < 17; ++i) {
        stats.base[i] = base[i];
        stats.bonus[i] = 0.0f;
    }
    return stats;
}

std::string client_data_dir() {
    if (const char* env = std::getenv("KNC_CLIENT_DATA")) return env;
    return "Data";
}

bool file_exists(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

int check_math() {
    int failures = 0;

    Quat q = body_quat_from_axis_angle(Vec3{0.0f, 0.0f, 1.0f}, kPi * 0.5f);
    float half = kPi * 0.25f;
    if (!nearly(q.z, std::sin(half)) || !nearly(q.w, std::cos(half)) || !nearly(q.x, 0.0f) || !nearly(q.y, 0.0f)) {
        std::printf("body_quat_from_axis_angle did not match the hand computed half angle\n");
        ++failures;
    }
    Mat3 m;
    body_quat_to_matrix(m, q);
    Vec3 rotatedX = body_mat3_transform_vec(m, Vec3{1.0f, 0.0f, 0.0f});
    if (!nearly(rotatedX.x, 0.0f, 0.01f) || !nearly(rotatedX.y, 1.0f, 0.01f)) {
        std::printf("body_quat_to_matrix 90 degree turn did not land on the y axis\n");
        ++failures;
    }
    Vec3 back = body_mat3_transform_vec_transposed(m, rotatedX);
    if (!nearly(back.x, 1.0f, 0.01f) || !nearly(back.y, 0.0f, 0.01f)) {
        std::printf("body_mat3_transform_vec_transposed did not undo the rotation\n");
        ++failures;
    }
    Quat qq = body_quat_multiply(q, q);
    if (!nearly(qq.z, 1.0f, 0.01f) || !nearly(qq.w, 0.0f, 0.01f)) {
        std::printf("body_quat_multiply did not double the 90 degree turn to 180\n");
        ++failures;
    }

    // matrix to quaternion round trip on a tilted rotation every handler branch of 0x4ED9C0 keeps R
    Quat tilt = body_quat_from_axis_angle(Vec3{0.3f, 0.9f, 0.2f}, 2.4f);
    Mat3 tm;
    body_quat_to_matrix(tm, tilt);
    Quat rt;
    body_mat3_to_quat(rt, tm);
    Mat3 tm2;
    body_quat_to_matrix(tm2, rt);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (!nearly(tm.m[r][c], tm2.m[r][c], 0.001f)) {
                std::printf("body_mat3_to_quat round trip differs at %d %d\n", r, c);
                ++failures;
            }
        }
    }
    Mat3 scaled;
    body_quat_to_matrix_scaled(scaled, tilt, body_quat_norm_sq(tilt));
    if (!nearly(scaled.m[1][2], tm.m[1][2], 0.0001f)) {
        std::printf("body_quat_to_matrix_scaled differs from the plain one on a unit quaternion\n");
        ++failures;
    }

    // jacobi on a symmetric tensor with one off diagonal term the eigenvalues are known
    Mat3 tensor;
    body_mat3_set(tensor, 2.0f, 0.0f, 1.0f, 0.0f, 3.0f, 0.0f, 1.0f, 0.0f, 2.0f);
    Mat3 vectors;
    float values[3] = {0.0f, 0.0f, 0.0f};
    body_mat3_jacobi_eigen(tensor, vectors, values);
    float lo = std::fmin(values[0], std::fmin(values[1], values[2]));
    float hi = std::fmax(values[0], std::fmax(values[1], values[2]));
    if (!nearly(lo, 1.0f, 0.001f) || !nearly(hi, 3.0f, 0.001f)) {
        std::printf("body_mat3_jacobi_eigen expected 1 and 3 got %f %f %f\n", static_cast<double>(values[0]),
                    static_cast<double>(values[1]), static_cast<double>(values[2]));
        ++failures;
    }

    // catmull rom over a straight line reproduces the line clamps at both ends
    float line[5] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    CurveTable curve;
    curve_build(curve, 5, line, 0.0f, 4.0f);
    if (!nearly(curve_eval(curve, 1.5f), 1.5f) || !nearly(curve_eval(curve, 0.25f), 0.25f) ||
        !nearly(curve_eval(curve, 9.0f), 4.0f - kTireSlipMinSpeed, 0.01f) || !nearly(curve_eval(curve, -3.0f), 0.0f)) {
        std::printf("curve_eval did not reproduce a line %f %f %f\n", static_cast<double>(curve_eval(curve, 1.5f)),
                    static_cast<double>(curve_eval(curve, 9.0f)), static_cast<double>(curve_eval(curve, -3.0f)));
        ++failures;
    }

    // disc curves at zero tilt clearance and contact distance are the radius the axis offset is zero
    CurveTable off;
    CurveTable dist;
    CurveTable clear;
    wheel_disc_curves_build(0.48f, 0.4f, off, dist, clear, 20, 16.0f);
    if (!nearly(curve_eval(clear, 0.0f), 0.48f, 0.002f) || !nearly(curve_eval(dist, 0.0f), 0.48f, 0.002f) ||
        !nearly(curve_eval(off, 0.0f), 0.0f, 0.002f) || !nearly(curve_eval(off, 0.5f), -0.1f, 0.005f)) {
        std::printf("wheel_disc_curves_build wrong at zero tilt %f %f %f\n", static_cast<double>(curve_eval(clear, 0.0f)),
                    static_cast<double>(curve_eval(dist, 0.0f)), static_cast<double>(curve_eval(off, 0.5f)));
        ++failures;
    }

    // slerp end points and the half way normal length
    Quat a = body_quat_from_axis_angle(Vec3{0.0f, 0.0f, 1.0f}, 0.0f);
    Quat b = body_quat_from_axis_angle(Vec3{0.0f, 0.0f, 1.0f}, 1.0f);
    Quat mid = body_quat_slerp_d3dx(a, b, 0.5f);
    if (!nearly(body_quat_norm_sq(mid), 1.0f, 0.001f) || !nearly(mid.z, std::sin(0.25f), 0.001f)) {
        std::printf("body_quat_slerp_d3dx half way is off\n");
        ++failures;
    }

    float ramp1 = body_ramp_toward(0.5f, 2.0f);
    float ramp2 = body_ramp_toward(3.0f, 2.0f);
    float ramp3 = body_ramp_toward(0.5f, 0.5f);
    if (!nearly(ramp1, 0.75f) || !nearly(ramp2, 0.0f) || !nearly(ramp3, 0.0f)) {
        std::printf("body_ramp_toward wrong %f %f %f\n", static_cast<double>(ramp1), static_cast<double>(ramp2),
                    static_cast<double>(ramp3));
        ++failures;
    }
    return failures;
}

int check_catalogue(const std::string& dataDir) {
    int failures = 0;
    SpawnCatalogue cat;
    catalogue_from_stats(ghost_stats(), 1, cat);
    if (!nearly(cat.chassisMass, 200.0f) || cat.maxGearCeiling != 6 || !nearly(cat.gearRatio[0], 3.38f) ||
        !nearly(cat.damperFront, kSetupDamper) || !nearly(cat.tireSpring[1], kSetupTireSpring) ||
        cat.torqueCurve.count != 20 || !nearly(cat.torqueCurve.table[6], 14000.0f) || cat.driveMode != 2) {
        std::printf("catalogue_from_stats does not carry the default car block\n");
        ++failures;
    }
    // 0x48C35000 at 0x495017 and 0x494AC1 is 400000 not 100000 the first read of the immediate was wrong
    if (!nearly(cat.tireSpring[0], 400000.0f, 1.0f) || !nearly(cat.tireSpring[1], 400000.0f, 1.0f)) {
        std::printf("the setup tyre spring override must be 400000 got %f\n", static_cast<double>(cat.tireSpring[0]));
        ++failures;
    }
    std::string carPath = dataDir + "/Car/basic_1.car";
    if (file_exists(carPath)) {
        SpawnCatalogue basic;
        std::string error;
        if (!catalogue_load_car_file(basic, carPath, error)) {
            std::printf("catalogue_load_car_file failed %s\n", error.c_str());
            ++failures;
        } else if (!nearly(basic.axlePos[0].x, 1.34f) || !nearly(basic.axlePos[0].y, 1.008f) ||
                   !nearly(basic.damperFront, 6000.0f) || !nearly(basic.tireSpring[0], 500000.0f, 1.0f) ||
                   !nearly(basic.tireGripBase[1], 5.4f) || basic.torqueCurve.count != 20 ||
                   !nearly(basic.torqueCurve.xMax, 1005.31f, 0.01f)) {
            std::printf("basic_1 car fields do not match the file dump\n");
            ++failures;
        } else {
            std::printf("basic_1.car loaded front axle %f %f rear grip %f\n", static_cast<double>(basic.axlePos[0].x),
                        static_cast<double>(basic.axlePos[0].y), static_cast<double>(basic.tireGripBase[1]));
        }
    }
    return failures;
}

int check_spring_channels(CarBody& body) {
    int failures = 0;
    // body create leaves the six rates at 1 car physics setup 0x49510C writes 5 1 steer steer 1 5
    for (float rate : body.wheels.materialPairRates) {
        if (!nearly(rate, kOne)) {
            std::printf("body_create must leave the six channel rates at 1\n");
            ++failures;
        }
    }
    body_set_mass_friction(body, kSetupChannelRateMass, kSetupChannelRateFriction, 0.4f);
    // 0x4EC189 to 0x4EC1B2 the first float takes slots 0 and 5 the second 1 and 4 the third 2 3
    const float expected[6] = {5.0f, 1.0f, 0.4f, 0.4f, 1.0f, 5.0f};
    for (int i = 0; i < 6; ++i) {
        if (!nearly(body.wheels.materialPairRates[i], expected[i])) {
            std::printf("body_set_mass_friction rate %d is %f\n", i, static_cast<double>(body.wheels.materialPairRates[i]));
            ++failures;
        }
    }
    // the accel channel ramps by dt times 5 per substep so it is full after 0 dot 2 s
    for (float& c : body.wheels.springChannels) c = 0.0f;
    for (int i = 0; i < 6; ++i) body.wheels.latestSubstepInput[i] = (i == 0) ? 1.0f : 0.0f;
    int substepsToFull = 0;
    while (body.wheels.springChannels[0] < kOne && substepsToFull < 1000) {
        body_spring_channel_update(body.wheels, kFixedPhysicsStepSeconds);
        ++substepsToFull;
    }
    float secondsToFull = static_cast<float>(substepsToFull) * kFixedPhysicsStepSeconds;
    std::printf("accel channel full after %d substeps %.3f s\n", substepsToFull, static_cast<double>(secondsToFull));
    if (secondsToFull < 0.19f || secondsToFull > 0.21f) {
        std::printf("the accel channel must fill in 0 dot 2 s with the rate 5\n");
        ++failures;
    }
    // body set tire grip 0x4EC130 lands on the fifth float of each axle block the tyre model peak scale
    float keepFront = body.wheels.tireGeometryFront[4];
    float keepRear = body.wheels.tireGeometryRear[4];
    body_set_tire_grip(body, 2.5f, 3.5f);
    if (!nearly(body.wheels.tireGeometryFront[4], 2.5f) || !nearly(body.wheels.tireGeometryRear[4], 3.5f)) {
        std::printf("body_set_tire_grip did not reach the axle peak scales\n");
        ++failures;
    }
    body_set_tire_grip(body, keepFront, keepRear);
    for (int frame = 0; frame < 5; ++frame) {
        for (int i = 0; i < 6; ++i) body.wheels.latestSubstepInput[i] = ((frame + i) % 2 == 0) ? 1.0f : 0.0f;
        body_spring_channel_update(body.wheels, 1.0f / 60.0f);
    }
    for (float c : body.wheels.springChannels) {
        if (c < 0.0f || c > 1.0f) {
            std::printf("spring channel out of the 0 to 1 clamp range\n");
            ++failures;
        }
    }
    return failures;
}

int check_free_body() {
    // no track the body is placed without probes gravity pulls it down the engine spins up on the throttle
    int failures = 0;
    ColTrack track;
    SpawnCatalogue cat;
    catalogue_from_stats(ghost_stats(), 1, cat);
    CarBody body;
    bool created = body_create(body, cat, cat.tireGripBase[0], cat.tireGripBase[1], 0.0f, 0.0f, 5.0f, 0.0f, 0, track);
    if (!created) {
        std::printf("body_create without a track must succeed\n");
        ++failures;
    }
    if (!nearly(body.wheels.massProps.mass, 1000.0f, 0.01f) || body.wheels.inertiaZ <= 0.0f) {
        std::printf("mass props wrong mass %f inertia z %f\n", static_cast<double>(body.wheels.massProps.mass),
                    static_cast<double>(body.wheels.inertiaZ));
        ++failures;
    }
    if (!nearly(body.position.z, 5.0f, 0.001f) || !nearly(body.position.x, 0.0f, 0.001f)) {
        std::printf("body_create did not keep the origin at the spawn %f %f %f\n", static_cast<double>(body.position.x),
                    static_cast<double>(body.position.y), static_cast<double>(body.position.z));
        ++failures;
    }
    failures += check_spring_channels(body);
    body_apply_durability_scale(body.wheels, kDurabilitySoundScale);
    body.wheels.latestSubstepInput[0] = 1.0f;
    float z0 = body.position.z;
    for (int i = 0; i < 60; ++i) {
        body_apply_force(body, 0.0f, 0.0f, 0.0f);
        body_integrate(body, track);
        body_step_world(body, 1.0f / 60.0f, 1);
    }
    body_finalize_wheels(body);
    std::printf("free body after 1 s z %f engine %f gear %d\n", static_cast<double>(body.position.z),
                static_cast<double>(body.wheels.gearRatio), body.wheels.currentGear);
    if (!(body.position.z < z0 - 5.0f)) {
        std::printf("gravity did not pull the free body down\n");
        ++failures;
    }
    if (!(body.wheels.gearRatio > 0.0f) || body.wheels.currentGear < 1) {
        std::printf("the engine did not spin up on the throttle\n");
        ++failures;
    }
    if (std::isnan(body.position.z) || std::isnan(body.wheels.gearRatio)) {
        std::printf("nan in the free body run\n");
        ++failures;
    }
    return failures;
}

int check_race01(const std::string& dataDir) {
    // the recorded spawn of Race 01 in wire x y z the body takes minus x minus y
    int failures = 0;
    std::string folder = dataDir + "/Public/World/Race/Race_01";
    if (!file_exists(folder + "/track.col")) {
        std::printf("Race_01 col data missing skipping the spawn check\n");
        return 0;
    }
    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, folder, error)) {
        std::printf("Race_01 col load failed %s\n", error.c_str());
        return 1;
    }
    SpawnCatalogue cat;
    std::string carError;
    if (!catalogue_load_car_file(cat, dataDir + "/Car/basic_1.car", carError)) {
        catalogue_from_stats(ghost_stats(), 1, cat);
    } else {
        catalogue_apply_setup_overrides(cat);
    }
    const float x0 = -321.3f;
    const float y0 = 228.57f;
    const float z0 = 0.98f;
    CarBody body;
    bool created = body_create(body, cat, cat.tireGripBase[0], cat.tireGripBase[1], -x0, -y0, z0, 0.0f, 1, track);
    std::printf("Race_01 body_create %s hub0 %f %f %f\n", created ? "ok" : "failed",
                static_cast<double>(body.hubWorld[0].x), static_cast<double>(body.hubWorld[0].y),
                static_cast<double>(body.hubWorld[0].z));
    if (!created) {
        std::printf("body_create must succeed on the recorded spawn\n");
        return 1;
    }
    for (int i = 0; i < 4; ++i) {
        const ColCell* cell = body.wheels.contactPlane[i];
        if (!cell) continue;
        const Vec3& hub = body.hubWorld[i];
        float above = hub.x * cell->heightA + hub.y * cell->heightB + hub.z * cell->heightC + cell->heightD;
        std::printf("  wheel %d plane %.4f %.4f %.4f %.4f hub over plane %.3f\n", i,
                    static_cast<double>(cell->heightA), static_cast<double>(cell->heightB),
                    static_cast<double>(cell->heightC), static_cast<double>(cell->heightD),
                    static_cast<double>(above));
    }
    // car physics setup 0x49510C writes the channel rates right after body create the harness does the same
    body_set_mass_friction(body, kSetupChannelRateMass, kSetupChannelRateFriction, 0.4f);
    body_apply_durability_scale(body.wheels, kDurabilitySoundScale);
    body.wheels.latestSubstepInput[0] = 1.0f;
    // the client frame 1 over 60 is cut by the fixed step 0 dot 0015 into 12 substeps
    const float frame = 1.0f / 60.0f;
    const int substeps = static_cast<int>(std::ceil(frame / kFixedPhysicsStepSeconds));
    const float dt = frame / static_cast<float>(substeps);
    std::printf("  substeps %d dt %.5f\n", substeps, static_cast<double>(dt));
    // left channel held from 3s to 3 4s heading is atan2 of first column of R
    float headingBefore = 0.0f;
    for (int tick = 0; tick < 240; ++tick) {
        body.wheels.latestSubstepInput[2] = (tick >= 180 && tick < 204) ? 1.0f : 0.0f;
        for (int sub = 0; sub < substeps; ++sub) {
            body_apply_force(body, 0.0f, 0.0f, 0.0f);
            body_integrate(body, track);
            body_step_world(body, dt, 1);
        }
        body_finalize_wheels(body);
        const Mat3& r = body.wheels.orientationR;
        float heading = std::atan2(r.m[1][0], r.m[0][0]) * 57.29578f;
        if (tick == 179) headingBefore = heading;
        if (tick >= 180 && tick % 6 == 5) {
            std::printf("  t %.2f heading %.2f deg spin z %.3f speed %.1f front axis y %.3f\n", (tick + 1) / 60.0,
                        static_cast<double>(heading), static_cast<double>(body.wheels.angularVelocity.z),
                        static_cast<double>(std::sqrt(body.wheels.velocity.x * body.wheels.velocity.x +
                                                      body.wheels.velocity.y * body.wheels.velocity.y)),
                        static_cast<double>(body.wheels.wheel[0].axis.x));
        }
        if (tick < 180 && tick % 12 == 11 && std::fabs(heading) > 0.5f) {
            std::printf("heading drifted to %f on the straight\n", static_cast<double>(heading));
            ++failures;
        }
        if (tick % 12 == 11 && tick < 180) {
            const Vec3& v = body.wheels.velocity;
            float speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            float fLong = 0.0f;
            float peakSum = 0.0f;
            for (int i = 0; i < 4; ++i) {
                fLong += body.wheels.tireForceResult[i][2];
                peakSum += body.wheels.tireForceResult[i][0];
            }
            int gear = body.wheels.currentGear;
            float ratio = (gear >= 1) ? body.wheels.gearRatioScaled[gear - 1] : 0.0f;
            std::printf("  t %.1f z %.2f speed %.2f engine %.1f gear %d load %.1f wheel torque %.0f spin %.1f slip %.3f flong %.0f peak %.0f unloaded %d comp %.0f\n",
                        (tick + 1) / 60.0, static_cast<double>(body.position.z), static_cast<double>(speed),
                        static_cast<double>(body.wheels.gearRatio), gear, static_cast<double>(body.wheels.engineLoad),
                        static_cast<double>(body.wheels.engineLoad * ratio),
                        static_cast<double>(body.wheels.wheel[0].spinRate),
                        static_cast<double>(body.wheels.rk4BankB.state[0]), static_cast<double>(fLong),
                        static_cast<double>(peakSum), body_wheels_on_ground_count(body.wheels),
                        static_cast<double>(body.wheels.tireScratch[0].compression));
        }
    }
    if (std::isnan(body.position.x) || std::isnan(body.position.z) || std::isnan(body.wheels.velocity.x)) {
        std::printf("nan on the Race_01 launch\n");
        ++failures;
    }
    (void)headingBefore;
    return failures;
}

} // namespace

int main() {
    int failures = 0;
    failures += check_math();
    std::string dataDir = client_data_dir();
    failures += check_catalogue(dataDir);
    failures += check_free_body();
    failures += check_race01(dataDir);

    // gear shift and ground flag helpers
    SpawnCatalogue cat;
    catalogue_from_stats(ghost_stats(), 1, cat);
    CarBody body;
    body.catalogue = cat;
    body.wheels.currentGear = 2;
    for (int i = 0; i < 6; ++i) car_gear_shift_up(body.wheels.currentGear, cat.maxGearCeiling);
    if (body.wheels.currentGear != cat.maxGearCeiling) {
        std::printf("car_gear_shift_up did not clamp at the catalogue ceiling\n");
        ++failures;
    }
    car_gear_shift_down(body.wheels.currentGear);
    car_ground_flag_set(body, 0);
    car_ground_flag_set(body, 1);
    if (body.wheels.currentGear != 1) {
        std::printf("car_ground_flag_set did not snap the gear to 1\n");
        ++failures;
    }
    car_gear_clamp(body, -5);
    if (body.wheels.currentGear != -1) {
        std::printf("car_gear_clamp did not floor a low request to minus 1\n");
        ++failures;
    }
    body.wheels.wheelOnGround[0] = 1;
    body.wheels.wheelOnGround[1] = 1;
    body.wheels.wheelOnGround[2] = 0;
    body.wheels.wheelOnGround[3] = 1;
    if (body_wheels_on_ground_count(body.wheels) != 3) {
        std::printf("body_wheels_on_ground_count expected 3\n");
        ++failures;
    }

    if (failures == 0) {
        std::printf("body_test PASS\n");
        return 0;
    }
    std::printf("body_test FAIL %d\n", failures);
    return 1;
}
