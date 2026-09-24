#pragma once

// Rigid body client kart physics ported 1 to 1 wrapper car 0x211C wheel set car 0x245C catalogue car 0x2EA0

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "constants.h"
#include "math_helpers.h"
#include "stats.h"
#include "world_collision.h"

namespace KnC::Kart::Client {

// curve object 0x14 bytes count table pointer xmin xmax scale read by curve eval 0x4EE5D0
struct CurveTable {
    int count = 0;
    std::vector<float> table;
    float xMin = 0.0f;
    float xMax = 0.0f;
    float scale = 0.0f;
};

// 0x140 byte block of Data Car car loaded by catalogue read file 0x4EFF20 defaults match default stats never write this

struct SpawnCatalogue {
    float chassisExtent[3] = {kDefaultCarChassisExtentX, kDefaultCarChassisExtentY, kDefaultCarChassisExtentZ};  // 0x00 car 0x2EA0 full box halved at setup 0x0c car 0x2EAC
    float chassisMass = kDefaultCarChassisMass;
    float lowerExtent[3] = {kDefaultCarLowerExtentX, kDefaultCarLowerExtentY, kDefaultCarLowerExtentZ};  // 0x10 car 0x2EB0 second box 0x1c car 0x2EBC
    float lowerMass = kDefaultCarLowerMass;
    float lowerOffsetX = kDefaultCarLowerOffsetX;  // 0x20 car 0x2EC0 second box offset x 0x24 car 0x2EC4 second box offset z
    float lowerOffsetZ = kDefaultCarLowerOffsetZ;
    float wheelRadius[2] = {kDefaultCarWheelRadius, kDefaultCarWheelRadius};  // 0x28 car 0x2EC8 front rear 0x30 car 0x2ED0
    float wheelWidth[2] = {kDefaultCarWheelWidth, kDefaultCarWheelWidth};
    float wheelMass[2] = {kDefaultCarWheelMass, kDefaultCarWheelMass};       // 0x38 car 0x2ED8
    Vec3 axlePos[2] = {Vec3{kDefaultCarFrontAxleX, kDefaultCarAxleHalfTrack, kDefaultCarAxleZ},
                       Vec3{kDefaultCarRearAxleX, kDefaultCarAxleHalfTrack, kDefaultCarAxleZ}};  // 0x40 car 0x2EE0 x forward y left z up y mirrored for the right wheel 0x58 car 0x2EF8 spin axis
    Vec3 axleAxis[2] = {Vec3{kDefaultCarAxleToe, kOne, 0.0f}, Vec3{-kDefaultCarAxleToe, kOne, 0.0f}};
    Vec3 axleDir[2] = {Vec3{0.0f, 0.0f, kOne}, Vec3{0.0f, 0.0f, kOne}};  // 0x70 car 0x2F10 suspension travel direction 0x88 car 0x2F28 third direction for the steer swing
    Vec3 steerUp = Vec3{0.0f, 0.0f, kOne};
    float maxSteerDeg = kDefaultCarMaxSteerDeg;  // 0x94 car 0x2F34 0x98 car 0x2F38 0 front 1 rear 2 both
    int driveMode = kDefaultCarDriveMode;
    float clutch = kDefaultCarClutch;  // 0x9c car 0x2F3C engine to wheel coupling 0xa0 car 0x2F40 gear count
    int maxGearCeiling = kDefaultCarGearCount;
    float gearRatio[6] = {kDefaultCarGear1, kDefaultCarGear2, kDefaultCarGear3, kDefaultCarGear4, kDefaultCarGear5, kDefaultCarGear6};  // 0xa4 car 0x2F44 0xbc car 0x2F5C
    float finalDrive = kDefaultCarFinalDrive;
    float reverseRatio = kDefaultCarReverseRatio;  // 0xc0 car 0x2F60 0xc4 car 0x2F64 engine speed above this shifts up
    float upshiftOmega = kDefaultCarUpshiftOmega;
    float downshiftOmega = kDefaultCarDownshiftOmega;  // 0xc8 car 0x2F68 engine speed below this shifts down 0xcc car 0x2F6C
    float engineDragCap = kDefaultCarEngineDragCap;
    float engineDragCoef = kDefaultCarEngineDragCoef;  // 0xd0 car 0x2F70 0xd4 car 0x2F74
    float engineInertia = kDefaultCarEngineInertia;
    CurveTable torqueCurve{kDefaultCarTorqueCount,
                           std::vector<float>(std::begin(kDefaultCarTorqueTable), std::end(kDefaultCarTorqueTable)),
                           0.0f, kDefaultCarTorqueXMax, kDefaultCarTorqueScale};  // 0xd8 car 0x2F78 the table follows the block in the file 0xec car 0x2F8C spin rate to brake torque
    float brakeCoef = kDefaultCarBrakeCoef;
    float brakeTorqueFront = kDefaultCarBrakeFront;  // 0xf0 car 0x2F90 0xf4 car 0x2F94
    float brakeTorqueRear = kDefaultCarBrakeRear;
    float handbrakeTorqueRear = kDefaultCarHandbrakeRear;  // 0xf8 car 0x2F98 0xfc car 0x2F9C
    float neutralDisplayThreshold = kDefaultCarNeutralDisplay;
    float springFront = kDefaultCarSpring;  // 0x100 car 0x2FA0 0x104 car 0x2FA4
    float springRear = kDefaultCarSpring;
    float damperFront = kDefaultCarDamper;  // 0x108 car 0x2FA8 setup writes 6000 0x10c car 0x2FAC setup writes 6000
    float damperRear = kDefaultCarDamper;
    float antirollFront = kDefaultCarAntiroll;  // 0x110 car 0x2FB0 0x114 car 0x2FB4
    float antirollRear = kDefaultCarAntiroll;
    int discCurveCount[2] = {kDefaultCarDiscCurveCount, kDefaultCarDiscCurveCount};  // 0x118 car 0x2FB8 0x120 car 0x2FC0
    float discEdgePower[2] = {kDefaultCarDiscEdgePower, kDefaultCarDiscEdgePower};
    float tireSpring[2] = {kDefaultCarTireSpring, kDefaultCarTireSpring};  // 0x128 car 0x2FC8 setup writes 400000 0x130 car 0x2FD0 goes through car 0x32DC 0x32E0 into the tyre model
    float tireGripBase[2] = {kDefaultCarTireGripFront, kDefaultCarTireGripRear};
    float tireGripAux[2] = {kDefaultCarTireGripFront, kDefaultCarTireGripRear};  // 0x138 car 0x2FD8 copied into the scratch never read

    // legacy placeholder inputs of the first port not part of the client block kept so older callers build
    float geometryA = 0.0f;
    float geometryB = 0.0f;
    float scaledArray[8] = {};
    float scaleFactor = 0.0f;
    float fieldC0 = 0.0f;
    float fieldC4 = 0.0f;
    float fieldC8 = 0.0f;
    Vec3 catalogueVec;
    float tractionLimitInput = 0.0f;
    float curveSource[11] = {};
    float secondaryBoundA = 0.0f;
    float secondaryBoundB = 0.0f;
};

// mass properties block wheel set 0xe8 to 0x120 mass props box 0x4F0070 compose 0x4F0130
struct MassProps {
    float mass = 0.0f;  // this 0xe8 car 0x2544 this 0xec car 0x2548 written by body principal axes
    float invMass = 0.0f;
    float gravityForce = 0.0f;  // this 0xf0 car 0x254C minus gravity scale times mass this 0xf4 mass weighted position
    Vec3 pos;
    Vec3 firstMoment;  // this 0x100 mass times position sums this 0x10c
    float ixx = 0.0f;
    float iyy = 0.0f;  // this 0x110 this 0x114
    float izz = 0.0f;
    float pxy = 0.0f;  // this 0x118 product sum x y this 0x11c product sum y z
    float pyz = 0.0f;
    float pxz = 0.0f;          // this 0x120 product sum x z
};

// per wheel tire force scratch wheel set 0x630 stride 0x4C car 0x2A8C bound by tire scratch bind 0x4F3290
struct WheelTireScratch {
    float springRate = 0.0f;  // this 0x0 catalogue 0x128 tyre spring this 0x4 tyre spring times 0 dot 1 never read
    float springRateTenth = 0.0f;
    float gripBaseNeg = 0.0f;  // this 0x8 minus catalogue 0x130 never read this 0xc catalogue 0x138 never read
    float gripAux = 0.0f;
    float radius = 0.0f;  // this 0x10 catalogue 0x28 wheel radius this 0x14 the literal 2
    int two = 2;
    int axle = 0;  // this 0x18 0x1c 0x20 hold axle curve pointers port keeps axle index this 0x48 car 0x2AD4 family tick reads this
    float compression = 0.0f;
};

// wheel object wheel set 0x204 stride 0xB8 car 0x2660 wheel 0 car 0x2718 wheel 1
struct WheelObject {
    float width = 0.0f;  // this 0x0 catalogue width this 0x4 catalogue radius
    float radius = 0.0f;
    Vec3 axis;  // this 0x8 spin axis in principal frame car 0x2668 this 0x14 hub rest point in principal frame car 0x2674
    Vec3 base;
    Vec3 dir;  // this 0x20 suspension travel direction car 0x2680 this 0x2c
    float mass = 0.0f;
    float inertia = 0.0f;  // this 0x30 mass times radius squared times half this 0x34
    float invMass = 0.0f;
    float invInertia = 0.0f;  // this 0x38 this 0x3c car 0x269C wheel frame axis then horizontal then third
    Mat3 localRotation;
    float travel = 0.0f;  // this 0x60 car 0x26C0 suspension travel along dir this 0x64 car 0x26C4
    float travelRate = 0.0f;
    float spinAngle = 0.0f;  // this 0x68 car 0x26C8 wrapped to two pi the tick reads it for the visual spin this 0x6c car 0x26CC
    float spinRate = 0.0f;
    float suspForce = 0.0f;  // this 0x70 car 0x26D0 accumulator along dir zeroed every stage this 0x74 car 0x26D4 accumulator about axis zeroed every stage
    float axleTorque = 0.0f;
    float travel0 = 0.0f;  // this 0x78 rk4 begin copies this 0x7c
    float rate0 = 0.0f;
    float spin0 = 0.0f;  // this 0x80 this 0x84
    float spinRate0 = 0.0f;
    float k1Rate = 0.0f;  // this 0x88 stage samples and accelerations this 0x8c
    float k1Acc = 0.0f;
    float k1Spin = 0.0f;  // this 0x90 this 0x94
    float k1SpinAcc = 0.0f;
    float k2Rate = 0.0f;  // this 0x98 this 0x9c
    float k2Acc = 0.0f;
    float k2Spin = 0.0f;  // this 0xa0 this 0xa4
    float k2SpinAcc = 0.0f;
    float k3Rate = 0.0f;  // this 0xa8 this 0xac
    float k3Acc = 0.0f;
    float k3Spin = 0.0f;  // this 0xb0 this 0xb4
    float k3SpinAcc = 0.0f;
};

// engine state wheel set 0x524 car 0x2980 engine init 0x4F3750 rk4 stages 0x4F37B0 to 0x4F38A0
struct EngineState {
    float dragCap = 0.0f;  // this 0x524 idx 0 catalogue 0xcc this 0x528 idx 1 catalogue 0xd0
    float dragCoef = 0.0f;
    float inertia = 0.0f;  // this 0x52c idx 2 catalogue 0xd4 this 0x530 idx 3 to idx 7 copy of the catalogue curve
    CurveTable torque;
    float invInertia = 0.0f;  // this 0x544 idx 8 this 0x548 idx 9 set by engine rk4 begin
    float throttle = 0.0f;
    float unused54c = 0.0f;  // this 0x54c idx 0xa never read this 0x554 idx 0xc omega before the step
    float prevOmega = 0.0f;
    float omega0 = 0.0f;  // this 0x558 idx 0xd omega at stage 1 this 0x55c idx 0xe stage 1 increment
    float k1 = 0.0f;
    float sample1 = 0.0f;  // this 0x560 idx 0xf this 0x564 idx 0x10
    float k2 = 0.0f;
    float sample2 = 0.0f;  // this 0x568 idx 0x11 this 0x56c idx 0x12
    float k3 = 0.0f;
};

// wheel and contact sub object car 0x245C ADD ECX 0x340 before body world step
struct WheelSet {
    // chassis rk4 snapshots and stage samples body chassis rk4 begin 0x4F0A40 integrate k1 to k4 this 0x0 T at begin
    Vec3 chassisT0;
    Vec3 chassisV0;  // this 0xc velocity at begin this 0x18 spin at begin
    Vec3 chassisW0;
    Quat chassisQ0;  // this 0x24 orientation at begin this 0x34 orientation matrix at begin
    Mat3 chassisR0;
    Vec3 k1Vel;  // this 0x58 car 0x24B4 clamped velocity stage 1 sample this 0x64
    Vec3 k1Omega;
    Vec3 k1Acc;  // this 0x70 half dt times force over mass this 0x7c
    Vec3 k1AngAcc;
    Vec3 k2Vel;  // this 0x88 this 0x94
    Vec3 k2Omega;
    Vec3 k2Acc;  // this 0xa0 this 0xac
    Vec3 k2AngAcc;
    Vec3 k3Vel;  // this 0xb8 this 0xc4
    Vec3 k3Omega;
    Vec3 k3Acc;  // this 0xd0 this 0xdc
    Vec3 k3AngAcc;

    MassProps massProps;   // this 0xe8 to 0x120

    Mat3 principalAxes;  // this 0x124 car 0x2580 rows are principal axes this 0x148 car 0x25A4 minus mass centre in principal frame
    Vec3 originOffset;
    float inertiaX = 0.0f;  // this 0x154 principal moments this 0x158
    float invInertiaX = 0.0f;
    float inertiaY = 0.0f;  // this 0x15c this 0x160
    float invInertiaY = 0.0f;
    float inertiaZ = 0.0f;  // this 0x164 this 0x168
    float invInertiaZ = 0.0f;
    float gyroX = 0.0f;  // this 0x16c iy minus iz over ix this 0x170 iz minus ix over iy
    float gyroY = 0.0f;
    float gyroZ = 0.0f;  // this 0x174 ix minus iy over iz this 0x178 iz minus iy never read
    float inertiaDiffA = 0.0f;
    float inertiaDiffB = 0.0f;  // this 0x17c ix minus iz never read this 0x180 iy minus ix never read
    float inertiaDiffC = 0.0f;
    float linearDrag = 0.0f;  // this 0x184 body set pose writes 0 drag term is dead this 0x188 to 0x190 also writes 0
    float angularDrag[3] = {};

    Vec3 translationT;  // this 0x194 car 0x25F0 T mass centre position in world this 0x1a0 car 0x25FC velocity body apply force adds
    Vec3 velocity;
    Mat3 orientationR;  // this 0x1ac car 0x2608 R rebuilt every rk4 stage from quaternion this 0x1d0 car 0x262C orientation quaternion drift writes reference
    Quat orientationQ;
    Vec3 angularVelocity;  // this 0x1e0 car 0x263C spin in the principal frame this 0x1ec car 0x2648 world force reset to gravity every stage
    Vec3 forceAccum;
    Vec3 torqueAccum;       // this 0x1f8 car 0x2654 principal frame torque reset to zero every stage

    WheelObject wheel[4];   // this 0x204 stride 0xB8 car 0x2660

    Vec3 steerBaseAxisLeft;  // this 0x4e4 front axle axis in the principal frame left this 0x4f0 mirrored right
    Vec3 steerBaseAxisRight;
    Vec3 steerSwingLeft;  // this 0x4fc axis cross up the steer turns the axle toward it this 0x508
    Vec3 steerSwingRight;
    float ackermannRatio = 0.0f;  // this 0x514 half track over wheelbase this 0x518 catalogue 0x94 times deg to rad
    float maxSteerRad = 0.0f;
    int driveMode = 0;  // this 0x51c catalogue 0x98 this 0x520 mean of the two ackermann tangents
    float steerAverage = 0.0f;

    EngineState engine;  // this 0x524 car 0x2980 this 0x550 car 0x29AC the engine speed the tick reads as rpm source name kept
    float gearRatio = 0.0f;

    int driveModeCopy = 0;  // this 0x570 catalogue 0x98 this 0x574 catalogue 0x9c
    float clutch = 0.0f;
    float upshiftOmega = 0.0f;  // this 0x578 car 0x29D4 catalogue 0xc4 this 0x57c car 0x29D8 catalogue 0xc8
    float downshiftOmega = 0.0f;
    int gearCount = 0;  // this 0x580 car 0x29DC catalogue 0xa0 this 0x584 car 0x29E0 catalogue ratio times final drive
    float gearRatioScaled[6] = {};
    float reverseRatio = 0.0f;  // this 0x59c catalogue 0xc0 this 0x5a0 car 0x29FC catalogue 0xec
    float brakeCoef = 0.0f;
    float brakeTorqueFront = 0.0f;  // this 0x5a4 catalogue 0xf0 this 0x5a8 catalogue 0xf4
    float brakeTorqueRear = 0.0f;
    float handbrakeTorqueRear = 0.0f;  // this 0x5ac catalogue 0xf8 this 0x5b0 catalogue 0xfc
    float neutralDisplayThreshold = 0.0f;
    float springFront = 0.0f;  // this 0x5b4 car 0x2A10 catalogue 0x100 this 0x5b8 catalogue 0x104
    float springRear = 0.0f;
    float damperFront = 0.0f;  // this 0x5bc catalogue 0x108 this 0x5c0 catalogue 0x10c
    float damperRear = 0.0f;
    float antirollFront = 0.0f;  // this 0x5c4 car 0x2A20 catalogue 0x110 this 0x5c8 car 0x2A24 catalogue 0x114
    float antirollRear = 0.0f;

    // spring channel state 6 channels body spring channel update this 0x5cc car 0x2A28 live value per channel
    float springChannels[6] = {};
    float prevSpringChannels[6] = {};  // this 0x5e4 car 0x2A40 previous tick cache this 0x5fc car 0x2A58 active flags from body world step
    float latestSubstepInput[6] = {};
    float materialPairRates[6] = {};  // this 0x614 car 0x2A70 mass mass friction friction steer steer

    WheelTireScratch tireScratch[4]; // this 0x630 car 0x2A8C stride 0x4C compression at local 0x48

    CurveTable discAxisOffset[2];  // this 0x760 front 0x774 rear axis offset of the contact point this 0x788 front 0x79c rear hub to contact distance
    CurveTable discContactDist[2];
    CurveTable discClearance[2];  // this 0x7b0 front 0x7c4 rear rest height plane this 0x7d8 front 0x7ec rear spring spring tenth grip base aux radius
    float tireParams[2][5] = {};

    float tireGeometryFront[7] = {};  // this 0x800 car 0x2C5C wheel 0 and 1 body create immediates this 0x81c car 0x2C78 wheel 2 and 3
    float tireGeometryRear[7] = {};
    float tireGeometryShared[8] = {};  // this 0x838 car 0x2C94 shared by all four wheels this 0x858 car 0x2CB4 grip scale times mass times a quarter
    float tractionLimitFront = 0.0f;
    float tractionLimitRear = 0.0f;  // this 0x85c car 0x2CB8

    float tireForceResult[4][5] = {}; // this 0x860 car 0x2CBC stride 0x14 peak lat long load ratio slip flag

    // relaxation length tyre states two banks lateral A longitudinal B gear rk4 blends
    struct Rk4Bank {
        float state[4] = {};  // integrated value per wheel stage scratch y0 plus half dt times k
        float working[4] = {};
        float k[4] = {};  // derivative sample for the current stage saved k1 used by combine
        float kStage2[4] = {};
        float kSum3[4] = {};  // saved k2 used by combine saved k3 used by combine
        float kSum4[4] = {};
    };
    Rk4Bank rk4BankA;  // this 0x8b0 group car 0x2D0C lateral slip state this 0x910 group car 0x2D6C longitudinal slip state
    Rk4Bank rk4BankB;

    const ColCell* contactPlane[4] = {};  // this 0x970 car 0x2DCC col cell under hub its plane this 0x980 car 0x2DDC clutch torque between engine wheels
    float engineLoad = 0.0f;
    float brakeTorqueFrontNow = 0.0f;  // this 0x984 brake channel times catalogue 0xf0 this 0x988 handbrake and brake channels times catalogue 0xf8 0xf4
    float brakeTorqueRearNow = 0.0f;
    int currentGear = -1;  // this 0x98c car 0x2DE8 minus 1 is neutral this 0x990 car 0x2DEC 0 when brake channel passes 0 dot 9
    int displayGear = 0;

    std::uint8_t wheelOnGround[4] = {};  // this 0x994 car 0x2DF0 byte 1 means no load this 0x998 car 0x2DF4 spin in world axes for axis correct
    Vec3 axisCorrectScratch;
    Quat axisCorrectTarget;  // this 0x9a4 car 0x2E00 tilt correction times orientation this 0x9b4 car 0x2E10 slerp output copied back
    Quat axisCorrectSlerp;

    Vec3 tireForceWorld[4];          // port only the world force of each tyre at the last solve for the harness trace
};

// rigid body wrapper car 0x211C

struct CarBody {
    Vec3 hubWorld[4];  // this 0x004 car 0x2120 stride 0xC hub points probes use this 0x34 car 0x2150 stride 0x24 R times wheel rotation
    Mat3 wheelWorldRotation[4];
    Vec3 position;  // this 0x0c4 car 0x21E0 body origin R times origin offset plus T this 0x0d0 car 0x21EC copy velocity at integrate
    Vec3 forceAccelCache;
    Mat3 chassisRotation;  // this 0x0dc car 0x21F8 R times principal axes no reader found this 0x240 car 0x235C query cell 0xc piece 0x3c
    BspQuery wheelQuery[4];
    BspQuery groundQuery;       // car 0x3608 the tick side query kept for the tick callers

    WheelSet wheels; // this 0x340 car 0x245C the wheel and contact sub object

    Quat referenceOrientation; // this 0x510 car 0x262C same memory as wheels orientationQ synced at every entry

    int collisionHappened = 0;  // this 0xd04 car 0x2E20 a collision happened this substep this 0xd08 car 0x2E24 sweep fraction of the failed locate
    float raycastT = 0.0f;
    Vec3 fallbackContactPoint;  // this 0xd0c car 0x2E28 hub with x y negated this 0xd18 car 0x2E34 unit normal of the blocking edge
    Vec3 collisionNormal;

    float rpmRatioValue = 0.0f;  // this 0xd24 this 0xd28
    float rpmRatioMin = 0.0f;
    float rpmRatioMax = 0.0f;  // this 0xd2c this 0xd30
    float lerpEndpointA = 0.0f;
    float lerpEndpointB = 0.0f;  // this 0xd34 this 0xd38
    float wheelLoadScale = 0.0f;
    float toleranceBandA = 0.0f;  // this 0xd3c this 0xd40
    float toleranceBandB = 0.0f;

    std::uint64_t contactCallback = 0;  // this 0xd44 car 0x2E60 piece pointer in client a handle here this 0xd48 car 0x2E64 edge that blocked last locate
    const ColEdge* hitEdge[4] = {};

    Quat signFlippedReference;  // this 0xd58 car 0x2E74 orientation with x y negated for the presentation this 0xd78 car 0x2E94
    float rpmRatio = 0.0f;
    float wheelLoadRatio = 0.0f;  // this 0xd7c car 0x2E98 this 0xd80 car 0x2E9C
    float finalLerp = 0.0f;

    std::uint8_t overValidGround = 0; // car 0x9D8 set by car ground flag set not body owned but needed here

    SpawnCatalogue catalogue; // car 0x2EA0 the block the body was built from its vectors normalized at setup
};

// rk4 step coefficients gear rk4 step coeffs 0x4EFFC0 writes the globals 0x2F26D48 to 0x2F26D54
struct GearRk4Coeffs {
    float dt = 0.0f;
    float halfDt = 0.0f;
    float quarterDt = 0.0f;
    float sixthDt = 0.0f;
};


// catalogue read file 0x4EFF20 0x140 bytes then the torque curve count xmin xmax scale and the table
bool catalogue_load_car_file(SpawnCatalogue& out, const std::string& path, std::string& error);

// car physics setup 0x494D50 and car apply kart loadout 0x490A70 write four floats after the load
void catalogue_apply_setup_overrides(SpawnCatalogue& cat);

// the stats and the kind never reach the block the client loads Data Car name car this gives default car
void catalogue_from_stats(const KartStats& stats, int vehicle_kind, SpawnCatalogue& out);

// curve build 0x4EE540 copies the table xmax keeps a margin scale is count minus 1 over the span
void curve_build(CurveTable& out, int count, const float* table, float xMin, float xMax);

// curve eval 0x4EE5D0 catmull rom on a unit spaced table clamped to the range
float curve_eval(const CurveTable& curve, float x);

// wheel disc curves build 0x4F3130 three tables over the tilt of a disc against a plane
bool wheel_disc_curves_build(float radius, float width, CurveTable& axisOffset, CurveTable& contactDist,
                             CurveTable& clearance, int count, float edgePower);


void mass_props_zero(MassProps& p);
void mass_props_box(MassProps& p, float hx, float hy, float hz, float mass);
void mass_props_compose(MassProps& into, const MassProps& body, const Mat3& rot, const Vec3& offset);
void body_principal_axes(WheelSet& wheels);
void body_geometry_setup(WheelSet& wheels, SpawnCatalogue& cat, float gripScale);
void engine_init(WheelSet& wheels, const SpawnCatalogue& cat);


void body_set_material_pair(WheelSet& wheels, const float in[6]);
// body set mass friction 0x4EC180 the rates a b c c b a setup 0x49510C passes 5 1 car 0x32E8
void body_set_mass_friction(CarBody& body, float mass, float friction, float steerScale);
// body set tire grip 0x4EC130 car apply force if valid 0x499000 forwards the friction pair every tick
void body_set_tire_grip(CarBody& body, float gripFront, float gripRear);

bool body_create(CarBody& body, const SpawnCatalogue& catalogue, float gripFront, float gripRear,
                 float negPosX, float negPosY, float posZ, float negYawDeg,
                 std::uint64_t handle, const ColTrack& colTrack);
int body_load_wheel_config(WheelSet& wheels, SpawnCatalogue& catalogue, float gripScale);
bool body_place_and_probe(CarBody& body, float negPosX, float negPosY, float posZ,
                          float negYawDeg, const ColTrack& colTrack);
void body_set_pose(WheelSet& wheels, const Vec3& pos, const Quat& q, const Vec3& vel, const Vec3& spin,
                   float linearDragScale, float angularDragScale);
void body_state_reset_pose(WheelSet& wheels, const Vec3& pos, const Quat& q);
void body_wheel_state_reset(WheelSet& wheels, const Vec3& pos, const Quat& q);
void body_steer_front_wheels(WheelSet& wheels, float steer);


// binds the four wheel queries to the piece under each hub the world helper takes wire x y
void body_ground_probe_response(CarBody& body, std::uint64_t contactCallback, const ColTrack& track);

void body_apply_force(CarBody& body, float fx, float fy, float fz);
void body_integrate(CarBody& body, const ColTrack& track);

void body_get_shape_point(const ColPiece& piece, Vec3& out, int index);
void body_wheel_contact_fallback(CarBody& body, int wheelIndex);

void body_update_transform(CarBody& body);
void body_update_wheel_transforms(CarBody& body);

// body wheel frame transform 0x4F1FA0 hub position velocity world axis and spin rate of one wheel
void body_wheel_frame_transform(WheelSet& wheels, int wheelIndex, Vec3& outHubPos, Vec3& outHubVel,
                                Vec3& outAxisWorld, float& outSpinRate);

// body wheel contact local 0x4F2060 folds a world contact force into the wheel and the chassis
void body_wheel_contact_local(WheelSet& wheels, int wheelIndex, const Vec3& worldPoint, const Vec3& worldForce);

// body apply local force 0x4F19B0 torque about the origin in the body frame force in world
void body_apply_local_force(WheelSet& wheels, const Vec3& forceLocal, const Vec3& pointLocal);


// body step world 0x4EC560 the tick pushes car 0x18 six input flags as floats and car 0x9D4 as flag
void body_step_world(CarBody& body, float dt, const float in[6], int throttlingFlag);
// same with the six channels left as the wheel set holds them
void body_step_world(CarBody& body, float dt, int throttlingFlag);
void body_world_step(WheelSet& wheels, float dt, const float in[6], int throttlingFlag);
void body_spring_channel_update(WheelSet& wheels, float dt);
void body_gear_update(WheelSet& wheels, float dt, float steer, float ch0, float ch1,
                      float ch4, float ch5, int throttlingFlag);

void gear_rk4_step_coeffs(GearRk4Coeffs& coeffs, float dt);
void gear_rk4_state_copy(WheelSet& wheels);
void gear_rk4_stage2_blend(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void gear_rk4_stage3_blend(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void gear_rk4_stage4_blend(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void gear_rk4_combine(WheelSet& wheels, const GearRk4Coeffs& coeffs);

// chassis rk4 begin 0x4F0A40 and wheel rk4 begin 0x4F2140 snapshots and the gravity force
void body_chassis_rk4_begin(WheelSet& wheels);
void body_wheel_rk4_begin(WheelSet& wheels);

// chassis rk4 integrator body chassis integrate k1 to k4 advance T q v and spin see BODY MOTION
void body_chassis_integrate_k1(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void body_chassis_integrate_k2(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void body_chassis_integrate_k3(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void body_chassis_integrate_k4(WheelSet& wheels, const GearRk4Coeffs& coeffs);

// chassis rk4 stage wrappers 0x4F2240 0x4F24D0 0x4F2760 0x4F29F0 also step the four wheel travels and spins
void body_chassis_rk4_stage1(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void body_chassis_rk4_stage2(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void body_chassis_rk4_stage3(WheelSet& wheels, const GearRk4Coeffs& coeffs);
void body_chassis_rk4_stage4(WheelSet& wheels, const GearRk4Coeffs& coeffs);

// engine rk4 begin 0x4F37A0 and stages 0x4F37B0 0x4F3800 0x4F3850 combine 0x4F38A0 on the engine speed
void engine_rk4_begin(WheelSet& wheels, float throttle);
void engine_rk4_stage1(WheelSet& wheels, float load, const GearRk4Coeffs& coeffs);
void engine_rk4_stage2(WheelSet& wheels, float load, const GearRk4Coeffs& coeffs);
void engine_rk4_stage3(WheelSet& wheels, float load, const GearRk4Coeffs& coeffs);
void engine_rk4_combine(WheelSet& wheels, float load, const GearRk4Coeffs& coeffs);

// driven wheel spin rate 0x4F1DD0 was gear wheel solver dt mean spin of the driven axle
float driven_wheel_spin_rate(const WheelSet& wheels);

// gear wheel force solve 0x4EECD0 drive brake suspension then the tyre model per wheel
void gear_wheel_force_solve(WheelSet& wheels);

// gear tire force model 0x4F32E0 disc against the plane then a magic formula on two slip states
bool gear_tire_force_model(WheelSet& wheels, WheelTireScratch& scratch, const Vec3& hubPos, const Vec3& hubVel,
                           const Vec3& axisWorld, float spinRate, const ColCell* plane, Vec3& outContact,
                           Vec3& outForce, const float geom[7], const float shared[8], float outResult[5],
                           float slipLat, float slipLong, float& dSlipLat, float& dSlipLong, float bias);


void body_finalize_wheels(CarBody& body);
void body_wheel_axis_correct(WheelSet& wheels);

float body_ramp_toward(float value, float target);
int body_wheels_on_ground_count(const WheelSet& wheels);
void body_apply_durability_scale(WheelSet& wheels, float value);

void car_gear_clamp(CarBody& body, int requestedGear);
void car_ground_flag_set(CarBody& body, std::uint8_t groundValid);

} // namespace KnC Kart Client
