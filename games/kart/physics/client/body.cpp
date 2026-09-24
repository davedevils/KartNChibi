#include "body.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "constants.h"

namespace KnC::Kart::Client {

namespace {

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float read_f32(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, 4);
    return f;
}

int read_i32(const unsigned char* p) {
    int i;
    std::memcpy(&i, p, 4);
    return i;
}

void read_vec3(const unsigned char* p, Vec3& out) {
    out.x = read_f32(p);
    out.y = read_f32(p + 4);
    out.z = read_f32(p + 8);
}

// the orientation quaternion is one memory in the client the wrapper and the wheel set alias it
void sync_orientation_in(CarBody& body) {
    body.wheels.orientationQ = body.referenceOrientation;
}

void sync_orientation_out(CarBody& body) {
    body.referenceOrientation = body.wheels.orientationQ;
}

// wheel local rotation matrix from the spin axis col0 axis col1 horizontal col2 third 0x4ED100 calls
void wheel_frame_from_axis(Mat3& out, const Vec3& a, bool mirrored) {
    float n = kOne / std::sqrt(a.y * a.y + a.x * a.x);
    if (!mirrored) {
        body_mat3_set(out, a.x, -(n * a.y), -(n * a.z * a.x), a.y, n * a.x, -(n * a.z * a.y), a.z, 0.0f, kOne / n);
    } else {
        body_mat3_set(out, -a.x, n * a.y, -(a.z * a.x * n), -a.y, -(n * a.x), -(n * a.z * a.y), -a.z, 0.0f, kOne / n);
    }
}

// quaternion advance q plus factor times q0 times the spin as a pure quaternion body frame
void quat_advance(Quat& q, const Quat& q0, const Vec3& w, float factor) {
    q.w = q.w - (q0.x * w.x + q0.y * w.y + w.z * q0.z) * factor;
    q.x = (w.z * q0.y + (q0.w * w.x - w.y * q0.z)) * factor + q.x;
    q.y = ((w.x * q0.z + w.y * q0.w) - w.z * q0.x) * factor + q.y;
    q.z = (w.z * q0.w + (w.y * q0.x - q0.y * w.x)) * factor + q.z;
}

// renormalize gate 0 dot 9999 to 1 dot 0001 first order step then R from the quaternion 0x4ED800
void quat_renormalize_and_matrix(WheelSet& wheels) {
    Quat& q = wheels.orientationQ;
    float n = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    if (n < kQuatRenormLow || kQuatRenormHigh < n) {
        float s = (n + kOne) / (n + n);
        q.x = s * q.x;
        q.y = s * q.y;
        q.z = s * q.z;
        q.w = s * q.w;
        n = s * s * n;
    }
    body_quat_to_matrix_scaled(wheels.orientationR, q, n);
}

// angular acceleration times factor euler equations with the principal moments
Vec3 chassis_ang_acc(const WheelSet& w, const Vec3& spin, const Vec3& torque, float factor) {
    Vec3 r;
    r.x = (w.invInertiaX * torque.x + w.gyroX * spin.y * spin.z) * factor;
    r.y = (w.invInertiaY * torque.y + w.gyroY * spin.x * spin.z) * factor;
    r.z = (w.invInertiaZ * torque.z + w.gyroZ * spin.x * spin.y) * factor;
    return r;
}

// force and torque reset after a stage velocity times drag plus gravity on z spin times angular drag
void chassis_reset_force(WheelSet& w, const Vec3& v, const Vec3& spin) {
    w.forceAccum.x = v.x * w.linearDrag;
    w.forceAccum.y = v.y * w.linearDrag;
    w.forceAccum.z = v.z * w.linearDrag + w.massProps.gravityForce;
    w.torqueAccum.x = spin.x * w.angularDrag[0];
    w.torqueAccum.y = spin.y * w.angularDrag[1];
    w.torqueAccum.z = spin.z * w.angularDrag[2];
}

// global 0x2F26D44 body gravity scale set 0x4EFFB0 the grip scale at spawn the durability value every substep
float g_body_gravity_scale = 0.0f;


const CurveTable& scratch_axis_offset(const WheelSet& w, const WheelTireScratch& s) { return w.discAxisOffset[s.axle]; }
const CurveTable& scratch_contact_dist(const WheelSet& w, const WheelTireScratch& s) { return w.discContactDist[s.axle]; }
const CurveTable& scratch_clearance(const WheelSet& w, const WheelTireScratch& s) { return w.discClearance[s.axle]; }

} // namespace


bool catalogue_load_car_file(SpawnCatalogue& out, const std::string& path, std::string& error) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        error = "cannot open " + path;
        return false;
    }
    unsigned char block[0x140];
    if (std::fread(block, sizeof(block), 1, f) != 1) {
        std::fclose(f);
        error = "car block shorter than 0x140 bytes";
        return false;
    }
    SpawnCatalogue cat;
    for (int i = 0; i < 3; ++i) cat.chassisExtent[i] = read_f32(block + 0x00 + i * 4);
    cat.chassisMass = read_f32(block + 0x0c);
    for (int i = 0; i < 3; ++i) cat.lowerExtent[i] = read_f32(block + 0x10 + i * 4);
    cat.lowerMass = read_f32(block + 0x1c);
    cat.lowerOffsetX = read_f32(block + 0x20);
    cat.lowerOffsetZ = read_f32(block + 0x24);
    for (int i = 0; i < 2; ++i) {
        cat.wheelRadius[i] = read_f32(block + 0x28 + i * 4);
        cat.wheelWidth[i] = read_f32(block + 0x30 + i * 4);
        cat.wheelMass[i] = read_f32(block + 0x38 + i * 4);
        read_vec3(block + 0x40 + i * 12, cat.axlePos[i]);
        read_vec3(block + 0x58 + i * 12, cat.axleAxis[i]);
        read_vec3(block + 0x70 + i * 12, cat.axleDir[i]);
    }
    read_vec3(block + 0x88, cat.steerUp);
    cat.maxSteerDeg = read_f32(block + 0x94);
    cat.driveMode = read_i32(block + 0x98);
    cat.clutch = read_f32(block + 0x9c);
    cat.maxGearCeiling = read_i32(block + 0xa0);
    for (int i = 0; i < 6; ++i) cat.gearRatio[i] = read_f32(block + 0xa4 + i * 4);
    cat.finalDrive = read_f32(block + 0xbc);
    cat.reverseRatio = read_f32(block + 0xc0);
    cat.upshiftOmega = read_f32(block + 0xc4);
    cat.downshiftOmega = read_f32(block + 0xc8);
    cat.engineDragCap = read_f32(block + 0xcc);
    cat.engineDragCoef = read_f32(block + 0xd0);
    cat.engineInertia = read_f32(block + 0xd4);
    cat.brakeCoef = read_f32(block + 0xec);
    cat.brakeTorqueFront = read_f32(block + 0xf0);
    cat.brakeTorqueRear = read_f32(block + 0xf4);
    cat.handbrakeTorqueRear = read_f32(block + 0xf8);
    cat.neutralDisplayThreshold = read_f32(block + 0xfc);
    cat.springFront = read_f32(block + 0x100);
    cat.springRear = read_f32(block + 0x104);
    cat.damperFront = read_f32(block + 0x108);
    cat.damperRear = read_f32(block + 0x10c);
    cat.antirollFront = read_f32(block + 0x110);
    cat.antirollRear = read_f32(block + 0x114);
    for (int i = 0; i < 2; ++i) {
        cat.discCurveCount[i] = read_i32(block + 0x118 + i * 4);
        cat.discEdgePower[i] = read_f32(block + 0x120 + i * 4);
        cat.tireSpring[i] = read_f32(block + 0x128 + i * 4);
        cat.tireGripBase[i] = read_f32(block + 0x130 + i * 4);
        cat.tireGripAux[i] = read_f32(block + 0x138 + i * 4);
    }
    // curve read file 0x4EE6E0 count xmin xmax scale then count floats a zero count is an empty curve
    unsigned char head[16];
    if (std::fread(head, sizeof(head), 1, f) != 1) {
        std::fclose(f);
        error = "car torque curve head missing";
        return false;
    }
    cat.torqueCurve.count = read_i32(head);
    cat.torqueCurve.xMin = read_f32(head + 4);
    cat.torqueCurve.xMax = read_f32(head + 8);
    cat.torqueCurve.scale = read_f32(head + 12);
    cat.torqueCurve.table.clear();
    if (cat.torqueCurve.count > 0) {
        cat.torqueCurve.table.resize(static_cast<size_t>(cat.torqueCurve.count));
        size_t got = std::fread(cat.torqueCurve.table.data(), 4, cat.torqueCurve.table.size(), f);
        if (got != cat.torqueCurve.table.size()) {
            std::fclose(f);
            error = "car torque curve table short";
            return false;
        }
    }
    std::fclose(f);
    out = cat;
    return true;
}

void catalogue_apply_setup_overrides(SpawnCatalogue& cat) {
    // car physics setup 0x495009 0x49501A and car apply kart loadout 0x494AB5 0x494AC6
    cat.damperFront = kSetupDamper;
    cat.damperRear = kSetupDamper;
    cat.tireSpring[0] = kSetupTireSpring;
    cat.tireSpring[1] = kSetupTireSpring;
}

void catalogue_from_stats(const KartStats& stats, int vehicle_kind, SpawnCatalogue& out) {
    // car apply kart loadout 0x490A70 loads Data Car name car no stat and no kind touch the block
    (void)stats;
    (void)vehicle_kind;
    out = SpawnCatalogue{};
    catalogue_apply_setup_overrides(out);
}

void curve_build(CurveTable& out, int count, const float* table, float xMin, float xMax) {
    out.count = count;
    out.table.assign(table, table + (count > 0 ? count : 0));
    out.xMax = xMax - kTireSlipMinSpeed;
    out.xMin = xMin;
    out.scale = static_cast<float>(count - 1) / (xMax - xMin);
}

float curve_eval(const CurveTable& curve, float x) {
    if (curve.count < 2 || curve.table.size() < static_cast<size_t>(curve.count)) return 0.0f;
    if (x < curve.xMin) {
        x = curve.xMin;
    } else if (!(x <= curve.xMax)) {
        x = curve.xMax;
    }
    float u = (x - curve.xMin) * curve.scale;
    int i = static_cast<int>(u);
    float t = u - static_cast<float>(i);
    if (i < 0) i = 0;
    if (i > curve.count - 2) i = curve.count - 2; // port guard the client would read past the table here
    float p0 = curve.table[static_cast<size_t>(i)];
    float p1 = curve.table[static_cast<size_t>(i + 1)];
    float t2 = t * t;
    float t3 = t2 * t;
    float pm1 = (i == 0) ? (p0 + p0) - p1 : curve.table[static_cast<size_t>(i - 1)];
    float p2 = (i == curve.count - 2) ? (p1 + p1) - p0 : curve.table[static_cast<size_t>(i + 2)];
    float w0 = ((kCurveCubicThree * t3 - t2 * kCurveCubicFive) + kCurveCubicTwo) * kHalf;
    float w1 = ((t2 * kCurveCubicFour - kCurveCubicThree * t3) + t) * kHalf;
    float wm = (((t2 + t2) - t3) - t) * kHalf;
    float w2 = (t3 - t2) * kHalf;
    return w0 * p0 + w1 * p1 + pm1 * wm + p2 * w2;
}

bool wheel_disc_curves_build(float radius, float width, CurveTable& axisOffset, CurveTable& contactDist,
                             CurveTable& clearance, int count, float edgePower) {
    if (count <= 0) return false;
    float halfW = width * kHalf;
    float rim = radius - halfW;
    std::vector<float> t0(static_cast<size_t>(count));
    std::vector<float> t1(static_cast<size_t>(count));
    std::vector<float> t2(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(count - 1);
        float c = std::sqrt(kOne - t * t);
        float v1 = halfW;
        if (i < count - 1) v1 = ((kOne - std::pow(t, edgePower)) / c) * rim + halfW;
        t0[static_cast<size_t>(i)] = -(t * halfW);
        t1[static_cast<size_t>(i)] = v1;
        t2[static_cast<size_t>(i)] = c * rim + halfW;
    }
    curve_build(axisOffset, count, t0.data(), 0.0f, kOne);
    curve_build(contactDist, count, t1.data(), 0.0f, kOne);
    curve_build(clearance, count, t2.data(), 0.0f, kOne);
    return true;
}


void mass_props_zero(MassProps& p) {
    p.mass = 0.0f;
    p.pos = Vec3{};
    p.firstMoment = Vec3{};
    p.ixx = p.iyy = p.izz = 0.0f;
    p.pxy = p.pyz = p.pxz = 0.0f;
}

void mass_props_box(MassProps& p, float hx, float hy, float hz, float mass) {
    p.mass = mass;
    p.pos = Vec3{};
    p.firstMoment = Vec3{};
    p.pxy = p.pyz = p.pxz = 0.0f;
    p.ixx = (hz * hz + hy * hy) * mass * kRk4ThirdWeight;
    p.iyy = (hx * hx + hz * hz) * mass * kRk4ThirdWeight;
    p.izz = (hx * hx + hy * hy) * mass * kRk4ThirdWeight;
}

void mass_props_compose(MassProps& A, const MassProps& B, const Mat3& rot, const Vec3& p) {
    // mass props compose 0x4F0130 transcribed term by term m is the row major rotation
    const float* m = &rot.m[0][0];
    Vec3 posB;
    body_vec3_transform(posB, B.pos, rot, p);
    float total = A.mass + B.mass;
    float inv = kOne / total;
    body_vec3_scale(A.pos, inv * A.mass);
    body_vec3_madd_inplace(A.pos, posB, inv * B.mass);
    A.mass = total;
    const Vec3& bf = B.firstMoment;
    A.firstMoment.x = m[2] * bf.z + B.mass * p.x + m[1] * bf.y + m[0] * bf.x + A.firstMoment.x;
    A.firstMoment.y = p.y * B.mass + m[5] * bf.z + m[3] * bf.x + m[4] * bf.y + A.firstMoment.y;
    A.firstMoment.z = m[6] * bf.x + m[8] * bf.z + B.mass * p.z + m[7] * bf.y + A.firstMoment.z;
    float sxx = ((B.iyy + B.izz) - B.ixx) * kHalf;
    float szz = ((B.iyy + B.ixx) - B.izz) * kHalf;
    float syy = ((B.izz + B.ixx) - B.iyy) * kHalf;
    float cx = m[2] * p.x * bf.z + m[2] * B.pyz * m[1] + B.pxy * m[0] * m[1] + bf.y * p.x * m[1] +
               m[2] * B.pxz * m[0] + m[0] * p.x * bf.x;
    float qx = m[1] * m[1] * syy + m[0] * m[0] * sxx + p.x * p.x * B.mass + m[2] * m[2] * szz + cx + cx;
    float cy = p.y * m[3] * bf.x + B.pxy * m[3] * m[4] + p.y * m[5] * bf.z + B.pyz * m[5] * m[4] +
               B.pxz * m[3] * m[5] + bf.y * p.y * m[4];
    float qy = m[5] * m[5] * szz + m[4] * m[4] * syy + m[3] * m[3] * sxx + p.y * p.y * B.mass + cy + cy;
    float cz = m[6] * p.z * bf.x + (p.z * bf.z + m[6] * B.pxz) * m[8] +
               (bf.y * p.z + m[8] * B.pyz + m[6] * B.pxy) * m[7];
    float qz = m[8] * m[8] * szz + m[7] * m[7] * syy + m[6] * m[6] * sxx + p.z * p.z * B.mass + cz + cz;
    A.ixx = qz + A.ixx + qy;
    A.iyy = qz + A.iyy + qx;
    A.izz = qy + qx + A.izz;
    A.pxy = B.pxy * m[3] * m[1] + p.y * p.x * B.mass + syy * m[4] * m[1] + bf.y * p.x * m[4] +
            B.pxy * m[0] * m[4] + B.pyz * m[5] * m[1] + m[2] * B.pxz * m[3] + m[2] * B.pyz * m[4] +
            p.y * m[0] * bf.x + m[2] * m[5] * szz + B.pxz * m[0] * m[5] + m[5] * p.x * bf.z +
            m[2] * p.y * bf.z + m[3] * p.x * bf.x + bf.y * p.y * m[1] + m[0] * m[3] * sxx + A.pxy;
    A.pyz = p.y * p.z * B.mass + bf.y * p.z * m[4] + m[8] * B.pxz * m[3] + m[8] * B.pyz * m[4] +
            m[6] * B.pxz * m[5] + m[6] * m[3] * sxx + m[8] * m[5] * szz + m[7] * B.pxy * m[3] +
            syy * m[7] * m[4] + m[6] * B.pxy * m[4] + p.z * m[5] * bf.z + m[8] * p.y * bf.z +
            m[3] * p.z * bf.x + m[6] * p.y * bf.x + (B.pyz * m[5] + bf.y * p.y) * m[7] + A.pyz;
    A.pxz = m[7] * B.pxy * m[0] + bf.y * p.z * m[1] + m[8] * B.pyz * m[1] + m[6] * m[2] * B.pxz +
            m[2] * B.pyz * m[7] + m[8] * p.x * bf.z + m[2] * p.z * bf.z + m[6] * p.x * bf.x +
            m[8] * m[2] * szz + syy * m[7] * m[1] + m[6] * m[0] * sxx + m[6] * B.pxy * m[1] +
            p.z * p.x * B.mass + bf.y * m[7] * p.x + m[8] * B.pxz * m[0] + m[0] * p.z * bf.x + A.pxz;
}

void body_principal_axes(WheelSet& wheels) {
    // body principal axes 0x4F0710 shift to the mass centre diagonalize keep the inverses and gyro terms
    MassProps& mp = wheels.massProps;
    Mat3 identity;
    Vec3 v;
    float invMass = kOne / mp.mass;
    body_vec3_set(v, -(invMass * mp.firstMoment.x), -(invMass * mp.firstMoment.y), -(invMass * mp.firstMoment.z));
    MassProps copy = mp;
    mass_props_zero(mp);
    mass_props_compose(mp, copy, identity, v);
    Mat3 tensor;
    body_mat3_set(tensor, mp.ixx, -mp.pxy, -mp.pxz, -mp.pxy, mp.iyy, -mp.pyz, -mp.pxz, -mp.pyz, mp.izz);
    float d[3] = {0.0f, 0.0f, 0.0f};
    body_mat3_jacobi_eigen(tensor, wheels.principalAxes, d);
    body_mat3_transpose_inplace(wheels.principalAxes);
    wheels.originOffset = body_mat3_transform_vec(wheels.principalAxes, v);
    wheels.inertiaX = d[0];
    wheels.inertiaY = d[1];
    wheels.inertiaZ = d[2];
    mp.invMass = kOne / mp.mass;
    wheels.invInertiaX = kOne / d[0];
    wheels.invInertiaY = kOne / d[1];
    wheels.invInertiaZ = kOne / d[2];
    mp.gravityForce = -(g_body_gravity_scale * mp.mass);
    wheels.gyroX = (d[1] - d[2]) * wheels.invInertiaX;
    wheels.gyroY = (d[2] - d[0]) * wheels.invInertiaY;
    wheels.gyroZ = (d[0] - d[1]) * wheels.invInertiaZ;
    wheels.inertiaDiffA = d[2] - d[1];
    wheels.inertiaDiffB = d[0] - d[2];
    wheels.inertiaDiffC = d[1] - d[0];
}

void body_geometry_setup(WheelSet& wheels, SpawnCatalogue& cat, float gripScale) {
    // body geometry setup 0x4F2AB0 two boxes composed wheels placed principal frame body gravity scale set 0x4EFFB0 principal axes read back
    g_body_gravity_scale = gripScale;
    mass_props_box(wheels.massProps, cat.chassisExtent[0] * kHalf, cat.chassisExtent[1] * kHalf,
                   cat.chassisExtent[2] * kHalf, cat.chassisMass);
    MassProps lower;
    mass_props_box(lower, cat.lowerExtent[0] * kHalf, cat.lowerExtent[1] * kHalf, cat.lowerExtent[2] * kHalf,
                   cat.lowerMass);
    Vec3 offset;
    body_vec3_set(offset, cat.lowerOffsetX, 0.0f, cat.lowerOffsetZ);
    Mat3 identity; // 0xAECCB4 is the identity matrix read as bytes
    mass_props_compose(wheels.massProps, lower, identity, offset);
    body_principal_axes(wheels);

    body_vec3_normalize(cat.axleDir[0]);
    body_vec3_normalize(cat.axleDir[1]);
    body_vec3_normalize(cat.axleAxis[0]);
    body_vec3_normalize(cat.axleAxis[1]);
    body_vec3_normalize(cat.steerUp);

    Vec3 dirs[4] = {cat.axleDir[0], Vec3{cat.axleDir[0].x, -cat.axleDir[0].y, cat.axleDir[0].z},
                    cat.axleDir[1], Vec3{cat.axleDir[1].x, -cat.axleDir[1].y, cat.axleDir[1].z}};
    Vec3 poses[4] = {cat.axlePos[0], Vec3{cat.axlePos[0].x, -cat.axlePos[0].y, cat.axlePos[0].z},
                     cat.axlePos[1], Vec3{cat.axlePos[1].x, -cat.axlePos[1].y, cat.axlePos[1].z}};
    Vec3 axes[4] = {cat.axleAxis[0], Vec3{-cat.axleAxis[0].x, cat.axleAxis[0].y, -cat.axleAxis[0].z},
                    cat.axleAxis[1], Vec3{-cat.axleAxis[1].x, cat.axleAxis[1].y, -cat.axleAxis[1].z}};
    Vec3 ups[2] = {cat.steerUp, Vec3{cat.steerUp.x, -cat.steerUp.y, cat.steerUp.z}};

    const Mat3& blk = wheels.principalAxes;
    for (int i = 0; i < 4; ++i) {
        WheelObject& w = wheels.wheel[i];
        w.dir = body_mat3_transform_vec(blk, dirs[i]);
        body_vec3_transform(w.base, poses[i], blk, wheels.originOffset);
    }
    wheels.steerBaseAxisLeft = body_mat3_transform_vec(blk, axes[0]);
    wheels.steerBaseAxisRight = body_mat3_transform_vec(blk, axes[1]);
    wheels.wheel[2].axis = body_mat3_transform_vec(blk, axes[2]);
    wheels.wheel[3].axis = body_mat3_transform_vec(blk, axes[3]);
    Vec3* swings[2] = {&wheels.steerSwingLeft, &wheels.steerSwingRight};
    const Vec3* bases[2] = {&wheels.steerBaseAxisLeft, &wheels.steerBaseAxisRight};
    for (int i = 0; i < 2; ++i) {
        Vec3 up = body_mat3_transform_vec(blk, ups[i]);
        body_vec3_cross(*swings[i], up, *bases[i]); // 0x4F2E42 pushes the base first so the swing is up cross base
        body_vec3_normalize(*swings[i]);
    }
    wheels.maxSteerRad = cat.maxSteerDeg * kDegToRadPlace;
    wheels.ackermannRatio = poses[0].y / (poses[0].x - poses[2].x);
    for (int i = 0; i < 4; ++i) {
        int axle = (i < 2) ? 0 : 1;
        WheelObject& w = wheels.wheel[i];
        w.mass = cat.wheelMass[axle];
        w.inertia = cat.wheelMass[axle] * cat.wheelRadius[axle] * cat.wheelRadius[axle] * kHalf;
        w.radius = cat.wheelRadius[axle];
        w.width = cat.wheelWidth[axle];
        w.invMass = kOne / w.mass;
        w.invInertia = kOne / w.inertia;
    }
    wheel_frame_from_axis(wheels.wheel[2].localRotation, wheels.wheel[2].axis, false);
    wheel_frame_from_axis(wheels.wheel[3].localRotation, wheels.wheel[3].axis, true);
    wheels.driveMode = cat.driveMode;
}

void engine_init(WheelSet& wheels, const SpawnCatalogue& cat) {
    // engine init 0x4F3750 three floats the curve copy 0x4EE790 and the inverse inertia
    EngineState& e = wheels.engine;
    e.dragCap = cat.engineDragCap;
    e.dragCoef = cat.engineDragCoef;
    e.inertia = cat.engineInertia;
    e.torque = cat.torqueCurve;
    if (e.torque.count == 0) e.torque.table.clear();
    e.invInertia = kOne / cat.engineInertia;
}


void body_set_material_pair(WheelSet& wheels, const float in[6]) {
    for (int i = 0; i < 6; ++i) wheels.materialPairRates[i] = in[i];
}

void body_set_mass_friction(CarBody& body, float mass, float friction, float steerScale) {
    // body set mass friction 0x4EC180 six rates a b c c b a pack into three floats as pairs
    float pair[6] = {mass, friction, steerScale, steerScale, friction, mass};
    body_set_material_pair(body.wheels, pair);
}

void body_set_tire_grip(CarBody& body, float gripFront, float gripRear) {
    // wrapper 0xb50 and 0xb6c are wheel set 0x810 and 0x82c the peak scale of each axle tyre model
    body.wheels.tireGeometryFront[4] = gripFront;
    body.wheels.tireGeometryRear[4] = gripRear;
}

int body_load_wheel_config(WheelSet& wheels, SpawnCatalogue& cat, float gripScale) {
    // body load wheel config 0x4EE830 geometry engine gear copies disc curves scratch bind traction
    body_geometry_setup(wheels, cat, gripScale);
    engine_init(wheels, cat);
    wheels.driveModeCopy = cat.driveMode;
    wheels.clutch = cat.clutch;
    wheels.upshiftOmega = cat.upshiftOmega;
    wheels.downshiftOmega = cat.downshiftOmega;
    wheels.gearCount = cat.maxGearCeiling;
    if (cat.maxGearCeiling >= 7) return 0x14;
    for (int i = 0; i < wheels.gearCount && i < 6; ++i) {
        wheels.gearRatioScaled[i] = cat.gearRatio[i] * cat.finalDrive;
    }
    wheels.reverseRatio = cat.reverseRatio;
    wheels.brakeCoef = cat.brakeCoef;
    wheels.brakeTorqueFront = cat.brakeTorqueFront;
    wheels.brakeTorqueRear = cat.brakeTorqueRear;
    wheels.handbrakeTorqueRear = cat.handbrakeTorqueRear;
    wheels.neutralDisplayThreshold = cat.neutralDisplayThreshold;
    wheels.springFront = cat.springFront;
    wheels.damperFront = cat.damperFront;
    wheels.antirollFront = cat.antirollFront;
    wheels.springRear = cat.springRear;
    wheels.damperRear = cat.damperRear;
    wheels.antirollRear = cat.antirollRear;
    for (int axle = 0; axle < 2; ++axle) {
        if (!wheel_disc_curves_build(cat.wheelRadius[axle], cat.wheelWidth[axle], wheels.discAxisOffset[axle],
                                     wheels.discContactDist[axle], wheels.discClearance[axle],
                                     cat.discCurveCount[axle], cat.discEdgePower[axle])) {
            return 0x14;
        }
        wheels.tireParams[axle][0] = cat.tireSpring[axle];
        wheels.tireParams[axle][1] = cat.tireSpring[axle] * kCatalogueElementScale;
        wheels.tireParams[axle][2] = cat.tireGripBase[axle];
        wheels.tireParams[axle][3] = cat.tireGripAux[axle];
        wheels.tireParams[axle][4] = cat.wheelRadius[axle];
    }
    for (int i = 0; i < 4; ++i) {
        // tire scratch bind 0x4F3290 copies the five params negates the third and keeps the curve pointers
        int axle = (i < 2) ? 0 : 1;
        WheelTireScratch& s = wheels.tireScratch[i];
        s.springRate = wheels.tireParams[axle][0];
        s.springRateTenth = wheels.tireParams[axle][1];
        s.gripBaseNeg = -wheels.tireParams[axle][2];
        s.gripAux = wheels.tireParams[axle][3];
        s.radius = wheels.tireParams[axle][4];
        s.two = 2;
        s.axle = axle;
    }
    float traction = gripScale * wheels.massProps.mass * kTractionLimitQuarter;
    wheels.axisCorrectScratch = Vec3{};
    wheels.tractionLimitFront = traction;
    wheels.tractionLimitRear = traction;
    return 0;
}

void body_set_pose(WheelSet& wheels, const Vec3& pos, const Quat& q, const Vec3& vel, const Vec3& spin,
                   float linearDragScale, float angularDragScale) {
    // body set pose 0x4F0900 R is the quaternion times the principal axes T keeps the origin at pos
    Mat3 rq;
    body_quat_to_matrix_scaled(rq, q, body_quat_norm_sq(q));
    Mat3 axesT;
    body_mat3_transpose_copy(axesT, wheels.principalAxes);
    body_mat3_multiply(wheels.orientationR, rq, axesT);
    body_mat3_to_quat(wheels.orientationQ, wheels.orientationR);
    Vec3 originWorld = body_mat3_transform_vec(wheels.orientationR, wheels.originOffset);
    body_vec3_sub(wheels.translationT, pos, originWorld);
    wheels.angularVelocity = body_mat3_transform_vec(wheels.principalAxes, spin);
    Vec3 com = body_mat3_transform_vec_transposed(wheels.principalAxes, wheels.originOffset);
    body_vec3_negate(com);
    Vec3 swing;
    body_vec3_cross(swing, spin, com);
    wheels.velocity = body_mat3_transform_vec(wheels.orientationR, swing);
    body_vec3_add(wheels.velocity, vel);
    wheels.linearDrag = -(linearDragScale * wheels.massProps.mass);
    wheels.angularDrag[0] = -(angularDragScale * wheels.inertiaX);
    wheels.angularDrag[1] = -(angularDragScale * wheels.inertiaY);
    wheels.angularDrag[2] = -(angularDragScale * wheels.inertiaZ);
}

void body_state_reset_pose(WheelSet& wheels, const Vec3& pos, const Quat& q) {
    // body state reset pose 0x4F1A80 the velocity spin and both drags come in as zero
    Vec3 zero;
    body_set_pose(wheels, pos, q, zero, zero, 0.0f, 0.0f);
    for (WheelObject& w : wheels.wheel) {
        w.travel = 0.0f;
        w.travelRate = 0.0f;
        w.spinAngle = 0.0f;
        w.spinRate = 0.0f;
    }
}

void body_wheel_state_reset(WheelSet& wheels, const Vec3& pos, const Quat& q) {
    // body wheel state reset 0x4EEAD0 pose then straight wheels then the engine channels banks and results
    body_state_reset_pose(wheels, pos, q);
    body_steer_front_wheels(wheels, 0.0f);
    wheels.gearRatio = 0.0f;
    for (float& c : wheels.springChannels) c = 0.0f;
    for (float& v : wheels.rk4BankA.state) v = 0.0f;
    for (float& v : wheels.rk4BankA.kStage2) v = 0.0f;
    for (float& v : wheels.rk4BankA.working) v = 0.0f;
    for (float& v : wheels.rk4BankA.k) v = 0.0f;
    for (float& v : wheels.rk4BankA.kSum3) v = 0.0f;
    for (float& v : wheels.rk4BankA.kSum4) v = 0.0f;
    for (float& v : wheels.rk4BankB.state) v = 0.0f;
    for (auto& result : wheels.tireForceResult) {
        for (float& f : result) f = 0.0f;
    }
}

void body_steer_front_wheels(WheelSet& wheels, float steer) {
    // body steer front wheels 0x4F1BE0 ackermann tangents swing the two front axles then rebuild their frames
    float tn = std::tan(-(steer * wheels.maxSteerRad));
    float a = tn / (kOne - tn * wheels.ackermannRatio);
    float b = tn / (tn * wheels.ackermannRatio + kOne);
    wheels.steerAverage = (b + a) * kHalf;
    WheelObject& w0 = wheels.wheel[0];
    WheelObject& w1 = wheels.wheel[1];
    body_vec3_madd(w0.axis, wheels.steerBaseAxisLeft, wheels.steerSwingLeft, a * kSteerSwingScale);
    body_vec3_normalize(w0.axis);
    body_vec3_madd(w1.axis, wheels.steerBaseAxisRight, wheels.steerSwingRight, b * kSteerSwingScale);
    body_vec3_normalize(w1.axis);
    wheel_frame_from_axis(w0.localRotation, w0.axis, false);
    wheel_frame_from_axis(w1.localRotation, w1.axis, true);
}

bool body_place_and_probe(CarBody& body, float negPosX, float negPosY, float posZ,
                          float negYawDeg, const ColTrack& colTrack) {
    // body place and probe 0x4EC290 pose the wheel set twice the wheel transforms then bind each hub
    Vec3 pos;
    body_vec3_set(pos, negPosX, negPosY, posZ);
    Vec3 up;
    body_vec3_set(up, 0.0f, 0.0f, kOne);
    Quat q = body_quat_from_axis_angle(up, negYawDeg * kDegToRadPlace);
    body_wheel_state_reset(body.wheels, pos, q);
    sync_orientation_out(body);
    body_update_wheel_transforms(body);
    body_update_wheel_transforms(body);
    body_update_transform(body); // the client leaves position to the next integrate the port fills it now

    if (body.contactCallback == 0) return true;
    for (int i = 0; i < 4; ++i) {
        const Vec3& hub = body.hubWorld[i];
        float y = 0.0f;
        int surface = 0;
        // the physics frame is the col frame the world helper takes wire x y so the signs flip here
        bool hit = world_locate_piece_by_height(body.wheelQuery[i], colTrack, -hub.x, -hub.y, hub.z, &y, &surface);
        if (!hit) return false;
        body.wheels.contactPlane[i] = body.wheelQuery[i].cell;
    }
    return true;
}

bool body_create(CarBody& body, const SpawnCatalogue& catalogue, float gripFront, float gripRear,
                 float negPosX, float negPosY, float posZ, float negYawDeg,
                 std::uint64_t handle, const ColTrack& colTrack) {
    // body create 0x4EC950 the third and fourth args are catalogue 0x130 0x134 through car 0x32DC 0x32E0
    body.catalogue = catalogue;
    body_load_wheel_config(body.wheels, body.catalogue, kGripScaleLoad);
    WheelSet& w = body.wheels;
    w.tireGeometryFront[0] = kTireCorneringStiffness;
    w.tireGeometryFront[1] = kTireLongStiffness;
    w.tireGeometryFront[2] = kTireShapeC;
    w.tireGeometryFront[3] = kTireShapeE;
    w.tireGeometryFront[4] = gripFront;
    w.tireGeometryFront[5] = kTireLoadSensitivity;
    w.tireGeometryFront[6] = kTireLatLongRatio;
    w.tireGeometryRear[0] = kTireCorneringStiffness;
    w.tireGeometryRear[1] = kTireLongStiffness;
    w.tireGeometryRear[2] = kTireShapeC;
    w.tireGeometryRear[3] = kTireShapeE;
    w.tireGeometryRear[4] = gripRear;
    w.tireGeometryRear[5] = kTireLoadSensitivity;
    w.tireGeometryRear[6] = kTireLatLongRatio;
    w.tireGeometryShared[0] = kTireRelaxLong;
    w.tireGeometryShared[1] = kTireRelaxLat;
    w.tireGeometryShared[2] = kTireLowSpeedBlend;
    w.tireGeometryShared[3] = kTireLoadRatioShape;
    w.tireGeometryShared[4] = kTireSlipFlagThreshold;
    w.tireGeometryShared[5] = kTireGeometryFifty;
    w.tireGeometryShared[6] = kTireGeometryFifty;
    w.tireGeometryShared[7] = kTireGeometryTwenty;
    float ones[6] = {kOne, kOne, kOne, kOne, kOne, kOne};
    body_set_material_pair(w, ones);
    body.contactCallback = handle;
    return body_place_and_probe(body, negPosX, negPosY, posZ, negYawDeg, colTrack);
}


void body_ground_probe_response(CarBody& body, std::uint64_t contactCallback, const ColTrack& track) {
    body.contactCallback = contactCallback;
    if (contactCallback == 0) return;
    for (int i = 0; i < 4; ++i) {
        const Vec3& hub = body.hubWorld[i];
        float y = 0.0f;
        int surface = 0;
        world_locate_piece_by_height(body.wheelQuery[i], track, -hub.x, -hub.y, hub.z, &y, &surface);
    }
}

void body_apply_force(CarBody& body, float fx, float fy, float fz) {
    Vec3 force;
    body_vec3_set(force, fx, fy, fz);
    body_vec3_add(body.wheels.velocity, force);
}

void body_integrate(CarBody& body, const ColTrack& track) {
    (void)track; // the queries hold their piece the track is bound by the probe response
    sync_orientation_in(body);
    body_update_transform(body);
    body_update_wheel_transforms(body);
    body.forceAccelCache = body.wheels.velocity;

    if (body.contactCallback == 0) return;

    body.collisionHappened = 0; // 0x4ECB35 the flag clears at the top of every integrate
    for (int i = 0; i < 4; ++i) {
        const Vec3& hub = body.hubWorld[i];
        BspQuery& ctx = body.wheelQuery[i];
        if (!ctx.piece) continue;
        const ColCell* cell = nullptr;
        float t = 0.0f;
        bool hit = world_bsp_locate_point(ctx, hub.x, hub.y, &cell, &t);
        body.raycastT = t;
        if (!hit) {
            body.hitEdge[i] = ctx.lastEdge;
            body_wheel_contact_fallback(body, i);
            body.collisionHappened = 1;
        }
        body.wheels.contactPlane[i] = ctx.cell;
        if (body.collisionHappened == 1) break; // 0x4ECB86 the loop ends on the first failed wheel
    }
}

void body_get_shape_point(const ColPiece& piece, Vec3& out, int index) {
    // body get shape point 0x4ECE10 this is the piece its array at 0x34 holds the vertices stride 0xC
    out = Vec3{};
    if (index < 0 || static_cast<size_t>(index) >= piece.unreadD.size()) return;
    const auto& raw = piece.unreadD[static_cast<size_t>(index)];
    std::memcpy(&out.x, raw.data(), 4);
    std::memcpy(&out.y, raw.data() + 4, 4);
    std::memcpy(&out.z, raw.data() + 8, 4);
}

void body_wheel_contact_fallback(CarBody& body, int wheelIndex) {
    // body wheel contact fallback 0x4EC460 the blocking edge names two vertices the normal is their 2D side
    const ColEdge* edge = body.hitEdge[wheelIndex];
    const ColPiece* piece = body.wheelQuery[wheelIndex].piece;
    Vec3 pA;
    Vec3 pB;
    if (edge && piece) {
        int ia = static_cast<int>(edge->reserved & 0xffffu);
        int ib = static_cast<int>((edge->reserved >> 16) & 0xffffu);
        body_get_shape_point(*piece, pA, ia);
        body_get_shape_point(*piece, pB, ib);
    }
    Vec3 normal{pB.y - pA.y, pA.x - pB.x, pA.z - pB.z};
    body_vec3_normalize_d3dx(normal);
    body.collisionNormal = normal;
    const Vec3& hub = body.hubWorld[wheelIndex];
    body.fallbackContactPoint = Vec3{-hub.x, -hub.y, hub.z};
}

void body_update_transform(CarBody& body) {
    // body update transform 0x4F1B10 this equals the wheel set position is R times offset plus T
    WheelSet& w = body.wheels;
    body_mat3_multiply(body.chassisRotation, w.orientationR, w.principalAxes);
    body_vec3_transform(body.position, w.originOffset, w.orientationR, w.translationT);
}

void body_update_wheel_transforms(CarBody& body) {
    // body update wheel transforms 0x4F1B50 hub is R times base plus dir times travel plus T
    WheelSet& w = body.wheels;
    for (int i = 0; i < 4; ++i) {
        WheelObject& wheel = w.wheel[i];
        Vec3 local;
        body_vec3_madd(local, wheel.base, wheel.dir, wheel.travel);
        body_vec3_transform(body.hubWorld[i], local, w.orientationR, w.translationT);
        body_mat3_multiply(body.wheelWorldRotation[i], w.orientationR, wheel.localRotation);
    }
}

void body_wheel_frame_transform(WheelSet& wheels, int wheelIndex, Vec3& outHubPos, Vec3& outHubVel,
                                Vec3& outAxisWorld, float& outSpinRate) {
    WheelObject& w = wheels.wheel[wheelIndex];
    Vec3 arm;
    body_vec3_madd(arm, w.base, w.dir, w.travel);
    Vec3 hubVelLocal;
    body_vec3_cross(hubVelLocal, wheels.angularVelocity, arm);
    body_vec3_madd_inplace(hubVelLocal, w.dir, w.travelRate);
    body_vec3_transform(outHubPos, arm, wheels.orientationR, wheels.translationT);
    body_vec3_transform(outHubVel, hubVelLocal, wheels.orientationR, wheels.velocity);
    outAxisWorld = body_mat3_transform_vec(wheels.orientationR, w.axis);
    outSpinRate = w.spinRate;
}

void body_wheel_contact_local(WheelSet& wheels, int wheelIndex, const Vec3& worldPoint, const Vec3& worldForce) {
    WheelObject& w = wheels.wheel[wheelIndex];
    Vec3 pointLocal;
    body_vec3_world_to_local(pointLocal, worldPoint, wheels.orientationR, wheels.translationT);
    Vec3 forceLocal = body_mat3_transform_vec_transposed(wheels.orientationR, worldForce);
    Vec3 arm;
    body_vec3_madd(arm, w.base, w.dir, w.travel);
    Vec3 lever;
    body_vec3_sub(lever, pointLocal, arm);
    Vec3 torque;
    body_vec3_cross(torque, lever, forceLocal);
    w.axleTorque = body_vec3_dot(torque, w.axis) + w.axleTorque;
    w.suspForce = body_vec3_dot(forceLocal, w.dir) + w.suspForce;
    body_apply_local_force(wheels, forceLocal, pointLocal);
}

void body_apply_local_force(WheelSet& wheels, const Vec3& forceLocal, const Vec3& pointLocal) {
    Vec3 torque;
    body_vec3_cross(torque, pointLocal, forceLocal);
    body_vec3_add(wheels.torqueAccum, torque);
    Vec3 forceWorld = body_mat3_transform_vec(wheels.orientationR, forceLocal);
    body_vec3_add(wheels.forceAccum, forceWorld);
}


void body_step_world(CarBody& body, float dt, const float in[6], int throttlingFlag) {
    // channels accel brake left right handbrake reverse the tick reads them at car 0x18 to 0x2C
    sync_orientation_in(body);
    body_world_step(body.wheels, dt, in, throttlingFlag);
    sync_orientation_out(body);
}

void body_step_world(CarBody& body, float dt, int throttlingFlag) {
    float in[6] = {body.wheels.latestSubstepInput[0], body.wheels.latestSubstepInput[1],
                   body.wheels.latestSubstepInput[2], body.wheels.latestSubstepInput[3],
                   body.wheels.latestSubstepInput[4], body.wheels.latestSubstepInput[5]};
    body_step_world(body, dt, in, throttlingFlag);
}

void body_world_step(WheelSet& wheels, float dt, const float in[6], int throttlingFlag) {
    for (int i = 0; i < 6; ++i) wheels.latestSubstepInput[i] = in[i];
    body_spring_channel_update(wheels, dt);
    float ch0 = wheels.springChannels[0];
    float ch1 = wheels.springChannels[1];
    float ch2 = wheels.springChannels[2];
    float ch3 = wheels.springChannels[3];
    float ch4 = wheels.springChannels[4];
    float ch5 = wheels.springChannels[5];
    body_gear_update(wheels, dt, ch2 - ch3, ch0, ch1, ch4, ch5, throttlingFlag);
}

void body_spring_channel_update(WheelSet& wheels, float dt) {
    for (int i = 0; i < 6; ++i) wheels.prevSpringChannels[i] = wheels.springChannels[i];
    for (int i = 0; i < 6; ++i) {
        float active = wheels.latestSubstepInput[i];
        float rate = wheels.materialPairRates[i];
        float channel = wheels.springChannels[i];
        bool sideChannel = (i == 2) || (i == 3);
        if (active == 0.0f) {
            float decayConst = sideChannel ? kSpringDecaySideChannels : kSpringDecayFourChannels;
            float value = channel - decayConst * rate * dt;
            float floor = sideChannel ? rate * kSpringFloorConst : kZero;
            channel = (value < floor) ? 0.0f : value;
        } else {
            if (sideChannel && channel < rate * kSpringFloorConst) {
                channel = rate * kSpringFloorConst;
            }
            float value = dt * rate + channel;
            channel = (kOne < value) ? 1.0f : value;
        }
        wheels.springChannels[i] = channel;
    }
}

void body_gear_update(WheelSet& wheels, float dt, float steer, float ch0, float ch1,
                      float ch4, float ch5, int throttlingFlag) {
    // body gear update 0x4EFA90 steer shift then four interleaved rk4 stages of wheels chassis and engine
    body_steer_front_wheels(wheels, steer);
    float throttle = ch0;
    if (throttlingFlag == 1) {
        int gear = wheels.currentGear;
        if (gear == -1) {
            if (ch0 <= kWheelAxisAngleTolerance) {
                throttle = ch5;
            } else {
                wheels.currentGear = 1;
            }
        } else if (ch5 <= kWheelAxisAngleTolerance) {
            float omega = wheels.gearRatio;
            if (omega <= wheels.upshiftOmega) {
                bool blocked = (wheels.downshiftOmega <= omega) || (gear < 2);
                if (!blocked) {
                    // this 0x57c plus gear times 4 is the ratio two below this 0x584 plus lower times 4 the one below
                    int lower = gear - 1;
                    float ratio = wheels.gearRatioScaled[gear - 2] / wheels.gearRatioScaled[gear - 1];
                    wheels.currentGear = lower;
                    wheels.gearRatio = ratio * omega;
                }
            } else if (gear < wheels.gearCount) {
                // this 0x584 plus gear times 4 is the next ratio this 0x57c plus higher times 4 the current one
                int higher = gear + 1;
                float ratio = wheels.gearRatioScaled[gear] / wheels.gearRatioScaled[gear - 1];
                wheels.currentGear = higher;
                wheels.gearRatio = ratio * omega;
            }
        } else {
            wheels.currentGear = -1;
            throttle = ch5;
        }
    }

    if (wheels.neutralDisplayThreshold < ch1 || wheels.neutralDisplayThreshold < ch4) {
        wheels.displayGear = 0;
    } else {
        wheels.displayGear = wheels.currentGear;
    }
    wheels.brakeTorqueFrontNow = ch1 * wheels.brakeTorqueFront;
    wheels.brakeTorqueRearNow = ch4 * wheels.handbrakeTorqueRear + ch1 * wheels.brakeTorqueRear;

    GearRk4Coeffs coeffs;
    gear_rk4_step_coeffs(coeffs, dt);
    body_wheel_rk4_begin(wheels);
    engine_rk4_begin(wheels, throttle);
    gear_rk4_state_copy(wheels);
    gear_wheel_force_solve(wheels);
    body_chassis_rk4_stage1(wheels, coeffs);
    engine_rk4_stage1(wheels, wheels.engineLoad, coeffs);
    gear_rk4_stage2_blend(wheels, coeffs);
    gear_wheel_force_solve(wheels);
    body_chassis_rk4_stage2(wheels, coeffs);
    engine_rk4_stage2(wheels, wheels.engineLoad, coeffs);
    gear_rk4_stage3_blend(wheels, coeffs);
    gear_wheel_force_solve(wheels);
    body_chassis_rk4_stage3(wheels, coeffs);
    engine_rk4_stage3(wheels, wheels.engineLoad, coeffs);
    gear_rk4_stage4_blend(wheels, coeffs);
    gear_wheel_force_solve(wheels);
    body_chassis_rk4_stage4(wheels, coeffs);
    engine_rk4_combine(wheels, wheels.engineLoad, coeffs);
    gear_rk4_combine(wheels, coeffs);
}


void body_chassis_rk4_begin(WheelSet& w) {
    // body chassis rk4 begin 0x4F0A40 snapshots then the force is drag plus gravity torque is drag
    chassis_reset_force(w, w.velocity, w.angularVelocity);
    w.chassisT0 = w.translationT;
    w.chassisQ0 = w.orientationQ;
    w.chassisR0 = w.orientationR;
    w.chassisV0 = w.velocity;
    w.chassisW0 = w.angularVelocity;
}

void body_wheel_rk4_begin(WheelSet& w) {
    // body wheel rk4 begin 0x4F2140 the chassis begin then each wheel keeps its four values and clears the sums
    body_chassis_rk4_begin(w);
    for (WheelObject& wheel : w.wheel) {
        wheel.travel0 = wheel.travel;
        wheel.rate0 = wheel.travelRate;
        wheel.spin0 = wheel.spinAngle;
        wheel.spinRate0 = wheel.spinRate;
        wheel.axleTorque = 0.0f;
        wheel.suspForce = 0.0f;
    }
}


void body_chassis_integrate_k1(WheelSet& w, const GearRk4Coeffs& coeffs) {
    Vec3& v = w.velocity;
    w.k1Acc.x = coeffs.halfDt * w.massProps.invMass * w.forceAccum.x;
    w.k1Acc.y = w.forceAccum.y * w.massProps.invMass * coeffs.halfDt;
    w.k1Acc.z = w.forceAccum.z * w.massProps.invMass * coeffs.halfDt;
    w.k1AngAcc = chassis_ang_acc(w, w.angularVelocity, w.torqueAccum, coeffs.halfDt);
    v.x = clampf(v.x, kChassisVelocityClampXYNeg, kChassisVelocityClampXY);
    v.y = clampf(v.y, kChassisVelocityClampXYNeg, kChassisVelocityClampXY);
    v.z = clampf(v.z, kChassisVelocityClampZNeg, kChassisVelocityClampZ);
    w.k1Vel = v;
    w.k1Omega = w.angularVelocity;
    w.translationT.x = coeffs.halfDt * w.k1Vel.x + w.translationT.x;
    w.translationT.y = coeffs.halfDt * w.k1Vel.y + w.translationT.y;
    w.translationT.z = coeffs.halfDt * w.k1Vel.z + w.translationT.z;
    quat_advance(w.orientationQ, w.chassisQ0, w.k1Omega, coeffs.quarterDt);
    quat_renormalize_and_matrix(w);
    body_vec3_add(v, w.k1Acc);
    body_vec3_add(w.angularVelocity, w.k1AngAcc);
    chassis_reset_force(w, v, w.angularVelocity);
}

void body_chassis_integrate_k2(WheelSet& w, const GearRk4Coeffs& coeffs) {
    w.k2Acc.x = coeffs.halfDt * w.massProps.invMass * w.forceAccum.x;
    w.k2Acc.y = w.forceAccum.y * w.massProps.invMass * coeffs.halfDt;
    w.k2Acc.z = w.forceAccum.z * w.massProps.invMass * coeffs.halfDt;
    w.k2AngAcc = chassis_ang_acc(w, w.angularVelocity, w.torqueAccum, coeffs.halfDt);
    w.k2Vel = w.velocity;
    w.k2Omega = w.angularVelocity;
    w.translationT.x = coeffs.halfDt * w.k2Vel.x + w.chassisT0.x;
    w.translationT.y = coeffs.halfDt * w.k2Vel.y + w.chassisT0.y;
    w.translationT.z = coeffs.halfDt * w.k2Vel.z + w.chassisT0.z;
    w.orientationQ = w.chassisQ0;
    quat_advance(w.orientationQ, w.chassisQ0, w.k2Omega, coeffs.quarterDt);
    quat_renormalize_and_matrix(w);
    w.velocity.x = w.k1Vel.x + w.k2Acc.x;
    w.velocity.y = w.k1Vel.y + w.k2Acc.y;
    w.velocity.z = w.k1Vel.z + w.k2Acc.z;
    w.angularVelocity.x = w.k1Omega.x + w.k2AngAcc.x;
    w.angularVelocity.y = w.k1Omega.y + w.k2AngAcc.y;
    w.angularVelocity.z = w.k1Omega.z + w.k2AngAcc.z;
    chassis_reset_force(w, w.velocity, w.angularVelocity);
}

void body_chassis_integrate_k3(WheelSet& w, const GearRk4Coeffs& coeffs) {
    w.k3Acc.x = coeffs.dt * w.massProps.invMass * w.forceAccum.x;
    w.k3Acc.y = w.forceAccum.y * w.massProps.invMass * coeffs.dt;
    w.k3Acc.z = w.forceAccum.z * w.massProps.invMass * coeffs.dt;
    w.k3AngAcc = chassis_ang_acc(w, w.angularVelocity, w.torqueAccum, coeffs.dt);
    w.k3Vel = w.velocity;
    w.k3Omega = w.angularVelocity;
    w.translationT.x = coeffs.dt * w.k3Vel.x + w.chassisT0.x;
    w.translationT.y = coeffs.dt * w.k3Vel.y + w.chassisT0.y;
    w.translationT.z = coeffs.dt * w.k3Vel.z + w.chassisT0.z;
    w.orientationQ = w.chassisQ0;
    quat_advance(w.orientationQ, w.chassisQ0, w.k3Omega, coeffs.halfDt);
    quat_renormalize_and_matrix(w);
    w.velocity.x = w.k1Vel.x + w.k3Acc.x;
    w.velocity.y = w.k1Vel.y + w.k3Acc.y;
    w.velocity.z = w.k1Vel.z + w.k3Acc.z;
    w.angularVelocity.x = w.k1Omega.x + w.k3AngAcc.x;
    w.angularVelocity.y = w.k1Omega.y + w.k3AngAcc.y;
    w.angularVelocity.z = w.k1Omega.z + w.k3AngAcc.z;
    chassis_reset_force(w, w.velocity, w.angularVelocity);
}

void body_chassis_integrate_k4(WheelSet& w, const GearRk4Coeffs& coeffs) {
    const Vec3& k1 = w.k1Vel;
    const Vec3& k2 = w.k2Vel;
    const Vec3& k3 = w.k3Vel;
    const Vec3& k4 = w.velocity;
    w.translationT.x = (k3.x + k2.x + k3.x + k2.x + k4.x + k1.x) * coeffs.sixthDt + w.chassisT0.x;
    w.translationT.y = (k3.y + k2.y + k3.y + k2.y + k4.y + k1.y) * coeffs.sixthDt + w.chassisT0.y;
    w.translationT.z = (k3.z + k2.z + k3.z + k2.z + k4.z + k1.z) * coeffs.sixthDt + w.chassisT0.z;
    Vec3 wsum;
    wsum.x = (w.k1Omega.x + w.angularVelocity.x) * kHalf + w.k3Omega.x + w.k2Omega.x;
    wsum.y = (w.angularVelocity.y + w.k1Omega.y) * kHalf + w.k3Omega.y + w.k2Omega.y;
    wsum.z = (w.k1Omega.z + w.angularVelocity.z) * kHalf + w.k3Omega.z + w.k2Omega.z;
    w.orientationQ = w.chassisQ0;
    quat_advance(w.orientationQ, w.chassisQ0, wsum, coeffs.sixthDt);
    quat_renormalize_and_matrix(w);
    Vec3 acc4;
    acc4.x = coeffs.halfDt * w.forceAccum.x * w.massProps.invMass;
    acc4.y = coeffs.halfDt * w.forceAccum.y * w.massProps.invMass;
    acc4.z = coeffs.halfDt * w.forceAccum.z * w.massProps.invMass;
    Vec3 ang4 = chassis_ang_acc(w, w.angularVelocity, w.torqueAccum, coeffs.halfDt);
    w.velocity.x = (w.k2Acc.x + w.k2Acc.x + acc4.x + w.k3Acc.x + w.k1Acc.x) * kRk4ThirdWeight + k1.x;
    w.velocity.y = (w.k2Acc.y + w.k2Acc.y + acc4.y + w.k3Acc.y + w.k1Acc.y) * kRk4ThirdWeight + k1.y;
    w.velocity.z = (w.k2Acc.z + w.k2Acc.z + acc4.z + w.k3Acc.z + w.k1Acc.z) * kRk4ThirdWeight + k1.z;
    Vec3 spin;
    spin.x = (w.k2AngAcc.x + w.k2AngAcc.x + ang4.x + w.k3AngAcc.x + w.k1AngAcc.x) * kRk4ThirdWeight + w.k1Omega.x;
    spin.y = (w.k2AngAcc.y + w.k2AngAcc.y + ang4.y + w.k3AngAcc.y + w.k1AngAcc.y) * kRk4ThirdWeight + w.k1Omega.y;
    spin.z = (w.k2AngAcc.z + w.k2AngAcc.z + ang4.z + w.k3AngAcc.z + w.k1AngAcc.z) * kRk4ThirdWeight + w.k1Omega.z;
    w.angularVelocity = spin;
}

// wheel stage helpers the client inlines these four blocks in each stage wrapper

static void wheel_stage_sample(WheelObject& w, float& rateSample, float& accSample, float& spinSample,
                               float& spinAccSample, float factor) {
    accSample = w.invMass * w.suspForce * factor;
    spinAccSample = w.invInertia * w.axleTorque * factor;
    rateSample = w.travelRate;
    spinSample = w.spinRate;
}

void body_chassis_rk4_stage1(WheelSet& wheels, const GearRk4Coeffs& coeffs) {
    body_chassis_integrate_k1(wheels, coeffs);
    for (WheelObject& w : wheels.wheel) {
        wheel_stage_sample(w, w.k1Rate, w.k1Acc, w.k1Spin, w.k1SpinAcc, coeffs.halfDt);
        w.travel = w.travelRate * coeffs.halfDt + w.travel;
        float spin = w.spinRate * coeffs.halfDt;
        w.axleTorque = 0.0f;
        w.suspForce = 0.0f;
        w.spinAngle = spin + w.spinAngle;
        w.travelRate = w.travelRate + w.k1Acc;
        w.spinRate = w.k1SpinAcc + w.spinRate;
    }
}

void body_chassis_rk4_stage2(WheelSet& wheels, const GearRk4Coeffs& coeffs) {
    body_chassis_integrate_k2(wheels, coeffs);
    for (WheelObject& w : wheels.wheel) {
        wheel_stage_sample(w, w.k2Rate, w.k2Acc, w.k2Spin, w.k2SpinAcc, coeffs.halfDt);
        w.travel = w.travelRate * coeffs.halfDt + w.travel0;
        float spin = w.spinRate * coeffs.halfDt;
        w.axleTorque = 0.0f;
        w.suspForce = 0.0f;
        w.spinAngle = spin + w.spin0;
        w.travelRate = w.k1Rate + w.k2Acc;
        w.spinRate = w.k1Spin + w.k2SpinAcc;
    }
}

void body_chassis_rk4_stage3(WheelSet& wheels, const GearRk4Coeffs& coeffs) {
    body_chassis_integrate_k3(wheels, coeffs);
    for (WheelObject& w : wheels.wheel) {
        wheel_stage_sample(w, w.k3Rate, w.k3Acc, w.k3Spin, w.k3SpinAcc, coeffs.dt);
        w.travel = w.travelRate * coeffs.dt + w.travel0;
        float spin = w.spinRate * coeffs.dt;
        w.axleTorque = 0.0f;
        w.suspForce = 0.0f;
        w.spinAngle = spin + w.spin0;
        w.travelRate = w.k1Rate + w.k3Acc;
        w.spinRate = w.k1Spin + w.k3SpinAcc;
    }
}

void body_chassis_rk4_stage4(WheelSet& wheels, const GearRk4Coeffs& coeffs) {
    body_chassis_integrate_k4(wheels, coeffs);
    for (WheelObject& w : wheels.wheel) {
        w.travel = (w.k3Rate + w.k2Rate + w.k3Rate + w.k2Rate + w.travelRate + w.k1Rate) * coeffs.sixthDt + w.travel0;
        w.spinAngle = (w.k3Spin + w.k2Spin + w.k3Spin + w.k2Spin + w.spinRate + w.k1Spin) * coeffs.sixthDt + w.spin0;
        w.travelRate = (w.k2Acc + w.k2Acc + w.invMass * w.suspForce * coeffs.halfDt + w.k3Acc + w.k1Acc) *
                           kRk4ThirdWeight + w.k1Rate;
        w.spinRate = (w.k2SpinAcc + w.k2SpinAcc + w.invInertia * w.axleTorque * coeffs.halfDt + w.k3SpinAcc +
                      w.k1SpinAcc) * kRk4ThirdWeight + w.k1Spin;
        w.spinAngle = static_cast<float>(std::fmod(static_cast<double>(w.spinAngle), kWheelSpinWrap));
    }
}


static float engine_derivative(const WheelSet& wheels, float omega, float load, float factor) {
    const EngineState& e = wheels.engine;
    float torque = curve_eval(e.torque, omega) * e.throttle;
    float drag = e.dragCoef * omega;
    if (e.dragCap < drag) drag = e.dragCap;
    return ((torque - drag) - load) * e.invInertia * factor;
}

void engine_rk4_begin(WheelSet& wheels, float throttle) {
    wheels.engine.prevOmega = wheels.gearRatio;
    wheels.engine.throttle = throttle;
}

void engine_rk4_stage1(WheelSet& wheels, float load, const GearRk4Coeffs& coeffs) {
    EngineState& e = wheels.engine;
    e.omega0 = wheels.gearRatio;
    e.k1 = engine_derivative(wheels, wheels.gearRatio, load, coeffs.halfDt);
    wheels.gearRatio = e.k1 + wheels.gearRatio;
}

void engine_rk4_stage2(WheelSet& wheels, float load, const GearRk4Coeffs& coeffs) {
    EngineState& e = wheels.engine;
    e.sample1 = wheels.gearRatio;
    e.k2 = engine_derivative(wheels, wheels.gearRatio, load, coeffs.halfDt);
    wheels.gearRatio = e.omega0 + e.k2;
}

void engine_rk4_stage3(WheelSet& wheels, float load, const GearRk4Coeffs& coeffs) {
    EngineState& e = wheels.engine;
    e.sample2 = wheels.gearRatio;
    e.k3 = engine_derivative(wheels, wheels.gearRatio, load, coeffs.dt);
    wheels.gearRatio = e.omega0 + e.k3;
}

void engine_rk4_combine(WheelSet& wheels, float load, const GearRk4Coeffs& coeffs) {
    EngineState& e = wheels.engine;
    float k4 = engine_derivative(wheels, wheels.gearRatio, load, coeffs.halfDt);
    wheels.gearRatio = (e.k2 + e.k2 + k4 + e.k3 + e.k1) * kRk4ThirdWeight + e.omega0;
}


void gear_rk4_step_coeffs(GearRk4Coeffs& coeffs, float dt) {
    coeffs.dt = dt;
    coeffs.halfDt = dt * kHalf;
    coeffs.quarterDt = dt * kSmoothQuarter;
    coeffs.sixthDt = dt * kRk4SixthWeight;
}

static void rk4_bank_state_copy(WheelSet::Rk4Bank& bank) {
    for (int i = 0; i < 4; ++i) bank.working[i] = bank.state[i];
}

void gear_rk4_state_copy(WheelSet& wheels) {
    rk4_bank_state_copy(wheels.rk4BankA);
    rk4_bank_state_copy(wheels.rk4BankB);
}

static void rk4_bank_stage2_blend(WheelSet::Rk4Bank& bank, float halfDt) {
    for (int i = 0; i < 4; ++i) {
        bank.kStage2[i] = bank.k[i];
        bank.working[i] = bank.k[i] * halfDt + bank.state[i];
    }
}

void gear_rk4_stage2_blend(WheelSet& wheels, const GearRk4Coeffs& coeffs) {
    rk4_bank_stage2_blend(wheels.rk4BankA, coeffs.halfDt);
    rk4_bank_stage2_blend(wheels.rk4BankB, coeffs.halfDt);
}

static void rk4_bank_stage3_blend(WheelSet::Rk4Bank& bank, float halfDt) {
    for (int i = 0; i < 4; ++i) {
        bank.kSum3[i] = bank.k[i];
        bank.working[i] = bank.k[i] * halfDt + bank.state[i];
    }
}

void gear_rk4_stage3_blend(WheelSet& wheels, const GearRk4Coeffs& coeffs) {
    rk4_bank_stage3_blend(wheels.rk4BankA, coeffs.halfDt);
    rk4_bank_stage3_blend(wheels.rk4BankB, coeffs.halfDt);
}

static void rk4_bank_stage4_blend(WheelSet::Rk4Bank& bank, float dt) {
    for (int i = 0; i < 4; ++i) {
        float k = bank.k[i];
        bank.kSum4[i] = k;
        bank.working[i] = dt * k + bank.state[i];
        bank.kSum4[i] = k + bank.kSum3[i];
    }
}

void gear_rk4_stage4_blend(WheelSet& wheels, const GearRk4Coeffs& coeffs) {
    rk4_bank_stage4_blend(wheels.rk4BankA, coeffs.dt);
    rk4_bank_stage4_blend(wheels.rk4BankB, coeffs.dt);
}

static void rk4_bank_combine(WheelSet::Rk4Bank& bank, float sixthDt) {
    for (int i = 0; i < 4; ++i) {
        float value = (bank.kSum4[i] + bank.kSum4[i] + bank.k[i] + bank.kStage2[i]) * sixthDt +
                      bank.state[i];
        if (value > kRk4ClampMax) {
            value = kRk4ClampMax;
        } else if (value < kRk4ClampMin) {
            value = kRk4ClampMin;
        }
        bank.state[i] = value;
    }
}

void gear_rk4_combine(WheelSet& wheels, const GearRk4Coeffs& coeffs) {
    rk4_bank_combine(wheels.rk4BankA, coeffs.sixthDt);
    rk4_bank_combine(wheels.rk4BankB, coeffs.sixthDt);
}


float driven_wheel_spin_rate(const WheelSet& wheels) {
    const WheelObject* w = wheels.wheel;
    switch (wheels.driveMode) {
        case 0:
            return (w[1].spinRate + w[0].spinRate) * kHalf;
        case 1:
            return (w[3].spinRate + w[2].spinRate) * kHalf;
        case 2:
            return (w[1].spinRate + w[0].spinRate) * kGearSolverDtBlendA +
                   (w[3].spinRate + w[2].spinRate) * kGearSolverDtBlendB;
        default:
            return kZero;
    }
}

static void gear_drive_torque_apply(WheelSet& wheels, float torque) {
    // gear drive torque apply 0x4F1E40 half to each driven wheel a quarter to all four in mode 2
    WheelObject* w = wheels.wheel;
    if (wheels.driveMode == 0) {
        w[0].axleTorque = torque * kDriveTorqueHalf + w[0].axleTorque;
        w[1].axleTorque = torque * kDriveTorqueHalf + w[1].axleTorque;
    } else if (wheels.driveMode == 1) {
        w[2].axleTorque = torque * kDriveTorqueHalf + w[2].axleTorque;
        w[3].axleTorque = torque * kDriveTorqueHalf + w[3].axleTorque;
    } else if (wheels.driveMode == 2) {
        float quarter = torque * kDriveTorqueQuarter;
        for (int i = 0; i < 4; ++i) w[i].axleTorque = quarter + w[i].axleTorque;
    }
}

void gear_wheel_force_solve(WheelSet& wheels) {
    // gear wheel force solve 0x4EECD0 clutch torque brakes suspension then the tyre model per wheel
    float wheelSpin = driven_wheel_spin_rate(wheels);
    int gear = wheels.displayGear;
    float driveTorque;
    if (gear >= 1) {
        float ratio = wheels.gearRatioScaled[gear - 1];
        wheels.engineLoad = (wheels.gearRatio - wheelSpin * ratio) * wheels.clutch;
        driveTorque = wheels.engineLoad * ratio;
    } else if (gear < 0) {
        wheels.engineLoad = (wheels.gearRatio - wheelSpin * wheels.reverseRatio) * wheels.clutch;
        driveTorque = wheels.engineLoad * wheels.reverseRatio;
    } else {
        driveTorque = 0.0f;
        wheels.engineLoad = 0.0f;
    }
    gear_drive_torque_apply(wheels, driveTorque);

    for (int i = 0; i < 4; ++i) {
        float cap = (i < 2) ? wheels.brakeTorqueFrontNow : wheels.brakeTorqueRearNow;
        float t = wheels.wheel[i].spinRate * wheels.brakeCoef;
        if (t <= cap) {
            if (t < -cap) t = -cap;
        } else {
            t = cap;
        }
        wheels.wheel[i].axleTorque = -t + wheels.wheel[i].axleTorque;
    }

    float travel[4];
    float rate[4];
    for (int i = 0; i < 4; ++i) {
        travel[i] = wheels.wheel[i].travel;
        rate[i] = wheels.wheel[i].travelRate;
    }
    float diffFront = (travel[0] - travel[1]) * wheels.antirollFront;
    float diffRear = (travel[2] - travel[3]) * wheels.antirollRear;
    wheels.wheel[0].suspForce += (-(travel[0] * wheels.springFront) - rate[0] * wheels.damperFront) - diffFront;
    wheels.wheel[1].suspForce += diffFront - (travel[1] * wheels.springFront + rate[1] * wheels.damperFront);
    wheels.wheel[2].suspForce += (-(travel[2] * wheels.springRear) - rate[2] * wheels.damperRear) - diffRear;
    wheels.wheel[3].suspForce += diffRear - (rate[3] * wheels.damperRear + travel[3] * wheels.springRear);

    for (int i = 0; i < 4; ++i) {
        const float* geom = (i < 2) ? wheels.tireGeometryFront : wheels.tireGeometryRear;
        float bias = (i < 2) ? wheels.tractionLimitFront : wheels.tractionLimitRear;
        Vec3 hubPos;
        Vec3 hubVel;
        Vec3 axisWorld;
        float spinRate = 0.0f;
        body_wheel_frame_transform(wheels, i, hubPos, hubVel, axisWorld, spinRate);
        Vec3 contact;
        Vec3 force;
        bool loaded = gear_tire_force_model(
            wheels, wheels.tireScratch[i], hubPos, hubVel, axisWorld, spinRate, wheels.contactPlane[i], contact,
            force, geom, wheels.tireGeometryShared, wheels.tireForceResult[i], wheels.rk4BankA.working[i],
            wheels.rk4BankB.working[i], wheels.rk4BankA.k[i], wheels.rk4BankB.k[i], bias);
        wheels.tireForceWorld[i] = loaded ? force : Vec3{};
        if (loaded) {
            body_wheel_contact_local(wheels, i, contact, force);
            wheels.wheelOnGround[i] = 0;
        } else {
            wheels.wheelOnGround[i] = 1;
        }
    }
}

bool gear_tire_force_model(WheelSet& wheels, WheelTireScratch& scratch, const Vec3& hubPos, const Vec3& hubVel,
                           const Vec3& axisWorld, float spinRate, const ColCell* plane, Vec3& outContact,
                           Vec3& outForce, const float geom[7], const float shared[8], float outResult[5],
                           float slipLat, float slipLong, float& dSlipLat, float& dSlipLong, float bias) {
    if (!plane) {
        // the client always has a cell here the port treats a missing one as no contact
        scratch.compression = 0.0f;
        for (int i = 0; i < 5; ++i) outResult[i] = 0.0f;
        dSlipLat = 0.0f;
        dSlipLong = 0.0f;
        outContact = hubPos;
        outForce = Vec3{};
        return false;
    }
    Vec3 n{plane->heightA, plane->heightB, plane->heightC};
    float planeD = plane->heightD;

    float d = body_vec3_dot(axisWorld, n);
    std::uint32_t dBits;
    std::memcpy(&dBits, &d, 4);
    std::uint32_t signBit = dBits & 0x80000000u;
    float ad = std::fabs(d);
    float clearance = curve_eval(scratch_clearance(wheels, scratch), ad);
    float height = body_vec3_dot(hubPos, n) + planeD;
    float compression = (clearance - height) * scratch.springRate + bias;
    scratch.compression = compression;

    float tilt = ad;
    if (kTireCamberCapForLoad < tilt) tilt = kTireCamberCapForLoad;
    float cap = kTireLoadCapBase - tilt * kTireLoadCapSlope;
    float load;
    if (compression <= cap) {
        load = (compression < -cap) ? -cap : compression;
    } else {
        load = cap;
    }
    float axisOffset = curve_eval(scratch_axis_offset(wheels, scratch), ad);
    float contactDist = curve_eval(scratch_contact_dist(wheels, scratch), ad);
    Vec3 forward;
    body_vec3_cross(forward, axisWorld, n);
    Vec3 down;
    body_vec3_cross(down, axisWorld, forward);
    Vec3 lateral;
    body_vec3_cross(lateral, forward, n);
    std::uint32_t offBits;
    std::memcpy(&offBits, &axisOffset, 4);
    offBits ^= signBit;
    float axisOffsetSigned;
    std::memcpy(&axisOffsetSigned, &offBits, 4);
    body_vec3_madd2(outContact, hubPos, down, contactDist, axisWorld, axisOffsetSigned);

    float vFwd;
    float vLat;
    if (ad <= kQuatRenormLow) {
        float sc = kOne / std::sqrt(kOne - ad * ad);
        body_vec3_scale(lateral, sc);
        body_vec3_scale(forward, sc);
        vFwd = body_vec3_dot(hubVel, forward);
        vLat = body_vec3_dot(hubVel, lateral);
    } else {
        vFwd = 0.0f;
        vLat = kZero;
    }
    float blend = shared[2];
    float vs;
    if (vFwd <= blend) {
        if (-blend <= vFwd) {
            vs = ((vFwd / blend) * vFwd + blend) * kHalf;
        } else {
            vs = -vFwd;
        }
    } else {
        vs = vFwd;
    }
    dSlipLat = (vLat - slipLat * vs) / shared[1];
    dSlipLong = ((spinRate * scratch.radius - vFwd) - vs * slipLong) / shared[0];

    if (load < kOne) {
        for (int i = 0; i < 5; ++i) outResult[i] = 0.0f;
        outForce = Vec3{};
        return false;
    }
    float e = std::exp(-(load * geom[5]));
    float f1 = e * geom[4];
    float f2 = e * geom[4] * geom[6];
    float nLat = (geom[0] * slipLat) / (f2 * load);
    float peak = f1 * load;
    float nLong = (geom[1] * slipLong) / peak;
    float sq = nLat * nLat + nLong * nLong;
    float mag = std::sqrt(sq);
    float fLat = geom[0] * slipLat;
    float fLong = geom[1] * slipLong;
    if (kTireSlipMinSpeed <= mag) {
        float x = mag / geom[2];
        float q = sq * kTireShapeQuarter;
        float a = (geom[0] * q + geom[1]) * nLong;
        float b = (q + kOne) * nLat * geom[1];
        float ang = std::atan(x);
        float ang2 = std::atan(x - (x - ang) * geom[3]);
        float sn = std::sin(ang2 * geom[2]);
        float m = sn / std::sqrt(a * a + b * b);
        fLong = m * a * f1 * load;
        fLat = -(m * b) * f2 * load;
    }
    Vec3 f;
    body_vec3_scaled(f, forward, fLong);
    body_vec3_madd_inplace(f, lateral, fLat);
    body_vec3_madd(outForce, f, n, load);
    outResult[0] = peak;
    outResult[2] = fLong;
    outResult[1] = fLat;
    outResult[3] = (mag / (mag + shared[3])) * load;
    outResult[4] = (mag <= shared[4]) ? 0.0f : kTireSlipFlagBits;
    return true;
}


void body_finalize_wheels(CarBody& body) {
    sync_orientation_in(body);
    body_wheel_axis_correct(body.wheels);
    sync_orientation_out(body);

    const Quat& q = body.wheels.orientationQ;
    body.signFlippedReference.x = -q.x;
    body.signFlippedReference.y = -q.y;
    body.signFlippedReference.z = q.z;
    body.signFlippedReference.w = q.w;

    float rpmSource = body.wheels.gearRatio; // car 0x29AC the engine speed
    float t = rpmSource * body.rpmRatioValue;
    if (t < body.rpmRatioMin) t = body.rpmRatioMin;
    if (t > body.rpmRatioMax) t = body.rpmRatioMax;
    body.rpmRatio = t;

    float loadSum = body.wheels.tireForceResult[0][3] + body.wheels.tireForceResult[1][3] +
                    body.wheels.tireForceResult[2][3] + body.wheels.tireForceResult[3][3];
    float loadRatio = loadSum * body.wheelLoadScale;
    if (loadRatio < kFinalizeLoadRatioClampFloor) loadRatio = kFinalizeLoadRatioClampFloor;
    if (loadRatio > kOne) loadRatio = kOne;
    body.wheelLoadRatio = loadRatio;

    body.finalLerp = body.lerpEndpointA + (body.lerpEndpointB - body.lerpEndpointA) * t;
}

void body_wheel_axis_correct(WheelSet& wheels) {
    // body wheel axis correct 0x4EFC90 everything below runs only when the body tilts a degree or more
    Mat3& r = wheels.orientationR;
    float tilt = std::acos(r.m[2][2]) - kDegToRadPlace;
    if (tilt < kWheelAxisAngleTolerance) return;

    Vec3 axis;
    body_vec3_set(axis, r.m[1][2], -r.m[0][2], 0.0f);
    Quat correction = body_quat_from_axis_angle(axis, tilt);
    wheels.axisCorrectTarget = body_quat_multiply(correction, wheels.orientationQ);
    wheels.axisCorrectSlerp = wheels.orientationQ;
    wheels.axisCorrectSlerp = body_quat_slerp_d3dx(wheels.axisCorrectSlerp, wheels.axisCorrectTarget, kAxisCorrectSlerp);
    wheels.orientationQ = wheels.axisCorrectSlerp;
    body_quat_to_matrix(r, wheels.orientationQ);

    wheels.axisCorrectScratch = body_mat3_transform_vec(r, wheels.angularVelocity);
    int ground = body_wheels_on_ground_count(wheels);
    float f = kGripScaleBase - static_cast<float>(ground);
    Vec3& s = wheels.axisCorrectScratch;
    s.x = body_ramp_toward(s.x, f) * s.x;
    s.y = body_ramp_toward(s.y, f) * s.y;
    if (ground > kAxisCorrectZCountGate) {
        s.z = 0.0f;
    } else {
        s.z = body_ramp_toward(s.z, kAxisCorrectZRamp) * s.z;
    }
    wheels.angularVelocity = body_mat3_transform_vec_transposed(r, s);
}

float body_ramp_toward(float value, float target) {
    float absVal = value;
    if (absVal <= kZero) absVal = -absVal;
    if (absVal < target && kOne < target) {
        return (kOne / target) * (target - absVal);
    }
    return 0.0f;
}

int body_wheels_on_ground_count(const WheelSet& wheels) {
    int count = 0;
    for (std::uint8_t flag : wheels.wheelOnGround) {
        if (flag == 1) ++count;
    }
    return count;
}

void body_apply_durability_scale(WheelSet& wheels, float value) {
    // body apply durability scale 0x4F1A60 the global gravity scale then the gravity force on the mass
    g_body_gravity_scale = value;
    wheels.massProps.gravityForce = -(value * wheels.massProps.mass);
}


void car_gear_clamp(CarBody& body, int requestedGear) {
    if (requestedGear < -1) {
        body.wheels.currentGear = -1;
        return;
    }
    int maxGear = body.catalogue.maxGearCeiling;
    if (maxGear < requestedGear) requestedGear = maxGear;
    body.wheels.currentGear = requestedGear;
}

void car_ground_flag_set(CarBody& body, std::uint8_t groundValid) {
    body.overValidGround = groundValid;
    if (groundValid == 1) {
        // 0x49A942 also zeroes the motion queue count car 0x3370 the remote mailbox owns that field
        int maxGear = body.catalogue.maxGearCeiling;
        body.wheels.currentGear = (maxGear < 1) ? maxGear : 1;
    }
}

} // namespace KnC Kart Client
