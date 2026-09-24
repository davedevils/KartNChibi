#pragma once

// Kart physics client constants ported from reverse physics docs image base 0x400000 all sections included

#include <cmath>

namespace KnC::Kart::Client {

// CONSTANTS generic and shared

constexpr float kZero = 0.0f;  // 0x59f44c generic zero sign flip test decay floor 0x59f480 generic one clamp ceiling full extension
constexpr float kOne = 1.0f;
constexpr float kHalf = 0.5f;  // 0x59f414 generic half wheel height averaging half angle 0x59f404 landing snap test wheel bump gate boost 10 kmh margin
constexpr float kAirborneLandingGate = 10.0f;
constexpr float kTickDeadzoneStep = 0.1f;  // 0x5a2494 per tick increment yaw pitch roll deadzone bound 0x5a32d4 generic quarter smoothing yaw correction box feature weight
constexpr float kSmoothQuarter = 0.25f;
constexpr float kStatGaugeNormalize = 0.8f;  // 0x5a322c stat to gauge scale drift ceiling off gas force 0x5a1650 drift charge floor turn force tire impulse scale
constexpr float kDriftChargeFloorScale = 0.3f;
constexpr float kTurnRateClamp = 1.5f;     // 0x5a6a48 low speed yaw turn rate roll turn force clamp ceiling

// CONSTANTS drift gauge state machine

constexpr float kDriftGaugeTickScale = 80.0f;  // 0x5a32a0 per tick drift gauge scale dt times this 0x5a3298 minimum speed to start a drift
constexpr float kDriftMinSpeed = 20.0f;
constexpr float kDriftLowSpeedSkip = 30.0f;  // 0x5a3244 speed above which low speed yaw fallback is skipped 0x5a324c gauge deadzone bound for the low speed fallback
constexpr float kDriftLowSpeedGaugeFloor = -10.0f;
constexpr float kDriftGaugeCap = 45.0f;  // 0x5eb700 drift gauge cap plus and minus 0x5a6aa0 drift gauge decay rate on stick release
constexpr float kDriftGaugeReleaseDecay = 0.12f;
constexpr float kDriftResidualGaugeFloor = -0.5f;  // 0x5a68c4 residual gauge deadzone lower bound 0x5a6a9c smoothed gauge time constant not drifting
constexpr float kDriftSmoothTauIdle = 16.0f;
constexpr float kDriftSmoothTauActive = 8.0f;  // 0x5a15f0 smoothed gauge time constant drifting also grip base 0x5a69f0 yaw turn rate while stuck flag steers the car
constexpr float kStuckYawTurnRate = 300.0f;

// CONSTANTS mini turbo

constexpr float kMiniTurboThresholdFloor = 0.2f;  // 0x5a15ec mini turbo threshold clamp floor 0x59f480 mini turbo threshold clamp ceiling same as kOne
constexpr float kMiniTurboThresholdCeil = 1.0f;
constexpr float kMiniTurboStatToFraction = 0.8f;  // 0x5a322c wire stat 10 and 11 raw to fraction scale 0x5eb704 stage 1 gauge threshold multiplier wire stat 10
constexpr float kMiniTurboStage1GaugeMul = 10.0f;
constexpr float kMiniTurboStage1HoldMulMs = 800.0f;  // 0x5eb708 stage 1 hold time threshold multiplier ms wire stat 11 0x5eb70c stage 2 to 3 boost window after release
constexpr float kMiniTurboStage3WindowMs = 1500.0f;

// CONSTANTS steering and turn force

constexpr float kStatClampCeilTwo = 2.0f;  // 0x5a24ec countersteer gain wire stat 1 and 5 clamp ceiling 0x5a32b8 steer gain scale engine force speed band boundary
constexpr float kSteerScaleThree = 3.0f;
constexpr float kSteerGainClampCeil = 4.0f;  // 0x5a0054 steering gain wire stat 2 max clamp 0x5a6a98 pi steer gain radian conversion factor A
constexpr float kDegToRadFactorA = 3.14159265f;
constexpr float kDegToRadFactorB = 0.0055556f;  // 0x5a6a94 1 over 180 steer gain radian conversion factor B 0x5a164c drift steer stat 9 raw to value scale
constexpr float kDriftSteerScale = 0.6f;
constexpr float kDriftSteerClampFloor = 1.2f;  // 0x5a32b0 drift steer wire stat 9 min clamp 0x5a3214 drift steer wire stat 9 max clamp
constexpr float kDriftSteerClampCeil = 1.8f;
constexpr float kDegToRad = 0.0174533f;  // 0x5a1e88 pi over 180 drift wheel spin yaw torque scale 0x5a6a30 front wheel cosmetic steer angle scale from yaw rate
constexpr float kYawRateToWheelAngle = 6.0f;
constexpr float kWheelAngleClampFloor = -0.523599f;  // 0x5a6ac8 and 0x5a6acc front wheel visual steer angle clamp minus and plus 30 deg min max
constexpr float kWheelAngleClampCeil = 0.523599f;
constexpr float kDriftTurnForceMul = 1.6f;  // 0x5a68cc turn force multiplier while actively drifting 0x5a6a68 speed threshold for extra drift turn torque
constexpr float kDriftExtraTorqueSpeedKmh = 60.0f;
constexpr float kShakeSumScale = 0.000001f;     // 0x5a6ad0 camera and body shake sum scale down

// CONSTANTS speed rpm engine force durability

constexpr float kMaxSpeedToKmh = 320.0f;  // 0x5eb6fc max speed unit to km per hour conversion factor 0x5a69a8 raw speed units to km per hour factor
constexpr float kSpeedToKmh = 1.728f;
constexpr float kRadPerSecToRpm = 9.5493f;  // 0x5a6ac4 rad per second to rpm conversion 60 over 2 pi 0x5a6ac0 rpm clamp floor
constexpr float kRpmClampFloor = 500.0f;
constexpr float kRpmClampCeil = 10000.0f;  // 0x5a371c rpm clamp ceiling 0x5a05e0 positive bound of the yaw rate snap deadzone
constexpr float kYawRateDeadzoneHigh = 0.01f;
constexpr float kYawRateDeadzoneLow = -0.01f;  // 0x5a69a4 negative bound of the yaw rate snap deadzone 0x5a1648 cap substituted into the wheel random bump multiplier
constexpr float kWheelBumpCap = 100.0f;
constexpr float kWheelBumpFinalScale = 0.000004f;  // 0x5a69a0 random wheel bump final scale 0x5a3720 suspension compression normalized floor global constant
constexpr float kSuspensionCompressionFloor = -1.0f;
constexpr float kSuspensionCompressionScale = 0.00001f;  // 0x5a6ad8 suspension compression raw to normalized scale 0x5a6b08 per tick decay of airborne air time scale
constexpr float kAirTimeScaleDecay = 0.92f;
constexpr float kRollTurnForceClampCeil = 50.0f;  // 0x59f450 roll based turn force clamp ceiling 0x5a6a10 durability value scaled for an engine side effect
constexpr float kDurabilitySoundScale = 19.6f;
constexpr float kEngineForceDurabilityScaleA = 0.6f;  // 0x5eb6f4 engine force durability penalty scale A 0x5a6adc engine force durability penalty scale B
constexpr float kEngineForceDurabilityScaleB = 0.16f;
constexpr float kEngineDragAirborneNoBoost = 0.997f;  // 0x5a6af8 and 0x5a328c throttle drag decay per tick two wheels in air no boost vs item boost
constexpr float kEngineDecayAirborneBoost = 0.99f;
constexpr float kEngineTaperHighSpeed = 0.988f;  // 0x5a6ae8 engine force taper speed 3 or above 0x5a6aec engine force taper speed 1 to 3
constexpr float kEngineTaperMidSpeed = 0.96f;
constexpr float kEngineDriftStartCapScale = 0.012f;  // 0x5a6af4 engine force reduction cap tied to drift start boost 0x5a6af0 per tick decay of the drift start torque boost
constexpr float kDriftStartBoostDecay = 0.97f;
constexpr float kEngineItemPenalty = 0.978f;  // 0x5a6b00 engine force penalty holding item kind 3 below count 1 0x5a6b04 engine force penalty in the reversed camera view
constexpr float kEngineRearCameraPenalty = 0.75f;
constexpr float kEngineSlowFlagMul = 8.0f;  // 0x5a15f0 engine force multiplier while slow flagged same as kDriftSmoothTauActive 0x5a69e4 turn force multiplier while slow flagged
constexpr float kTurnForceSlowFlagMul = 0.94f;
constexpr float kEngineForceDriftGasHeld = 0.6f;  // 0x5a164c drifting and holding gas same as kDriftSteerScale 0x5a322c drifting and off gas same as kStatGaugeNormalize
constexpr float kEngineForceDriftGasOff = 0.8f;
constexpr float kCrashRecoveryDecayState0 = 0.005f;  // 0x5a3be8 crash recovery state 0 decay per tick 0x5a3294 state 1 growth cap body pitch offset clamp ceiling
constexpr float kCrashRecoveryGrowthCap = 0.4f;
constexpr float kReverseSlowSpeedKmh = 12.0f;  // 0x5a32cc reverse speed threshold to turn slow flag on 0x5a6ad4 speed threshold clearing the slow flag
constexpr float kSlowFlagClearSpeedKmh = 18.0f;
constexpr float kAxisCorrection90Deg = 90.0f;  // 0x5a323c 90 degree axis correction for yaw pitch roll atan2 0x5a329c smoothing lerp factor for body pitch and roll
constexpr float kPitchRollSmoothBlend = 0.0625f;
constexpr float kYawPitchRollDeadzoneLow = -0.1f; // 0x5a32d8 negative bound paired with kTickDeadzoneStep

// CONSTANTS durability curve

constexpr float kDurabilityCurveFreq = 0.0157080f; // 0x5a69dc pi over 200 durability curve formula frequency

// durability at index i is 1 minus sin of i times pi over 200 game 0x1396fe0
inline float body_durability_curve_value(int index) {
    if (index < 0) index = 0;
    if (index > 99) index = 99;
    return 1.0f - std::sin(static_cast<float>(index) * kDurabilityCurveFreq);
}

// RIGID BODY constants read 0x59f414 0x59f480 0x59f44c already declared above reused here

constexpr float kAirborneSpeedGate = 10.0f;  // 0x59f404 tick level airborne speed gate same as kAirborneLandingGate 0x5a83f8 body finalize wheels load ratio clamp floor
constexpr float kFinalizeLoadRatioClampFloor = 0.0f;
constexpr float kFinalizeToleranceBand = 100.0f;  // 0x5a1648 body finalize wheels tolerance band same as kWheelBumpCap 0x5a32d4 load wheel config traction limit same as kSmoothQuarter
constexpr float kTractionLimitQuarter = 0.25f;
constexpr float kDegToRadPlace = 0.0174533f;  // 0x5a1e88 place and probe deg to rad same as kDegToRad 0x5a83e8 wheel axis correct angle deviation threshold
constexpr float kWheelAxisAngleTolerance = 0.0f;
constexpr float kCatalogueElementScale = 0.1f;  // 0x5a2494 load wheel config per element scale same as kTickDeadzoneStep 0x5eb6f0 finalize wheels engine force prep same as steering scale
constexpr float kEngineForcePrepA = 0.4f;
constexpr float kEngineForcePrepB = 0.6f;  // 0x5eb6f4 substep engine force prep same as kEngineForceDurabilityScaleA 0x5a6ad8 suspension compression scale same as kSuspensionCompressionScale
constexpr float kSuspensionBumpScale = 0.00001f;
constexpr float kEngineForcePrepC = 0.16f;  // 0x5a6adc substep engine force prep same as kEngineForceDurabilityScaleB 0x5a6ae0 substep engine force prep symmetric pair with kEngineForcePrepC
constexpr float kEngineForcePrepD = -0.16f;
constexpr float kGripScaleBase = 8.0f;  // 0x5a15f0 wheel axis correct grip scale base 8 minus wheels on ground 0x5a323c tick yaw rate wrap near durability scale
constexpr float kYawRateWrap90 = 90.0f;
constexpr float kSpringFloorConst = 0.04f;  // 0x5a3eb8 spring channel update floor clamp channels 2 and 3 0x5a0054 spring channel decay constant channels 0 1 4 5
constexpr float kSpringDecayFourChannels = 4.0f;
constexpr float kSpringDecaySideChannels = 16.0f; // 0x5a6a9c spring channel decay constant channels 2 and 3

// TICK HELPERS constants read

constexpr float kLowSpeedParticle = 40.0f;  // 0x5a1644 low versus high speed particle threshold 0x5a3218 collision response spin term durability gate
constexpr float kCollisionSpinDurabilityGate = 1.05f;
constexpr float kCollisionSpeedFifteen = 15.0f;  // 0x5a3230 collision impulse scale speed baseline 0x5a3c78 math wrap angle 360 step 0x5a3c7c holds minus 360
constexpr float kCollisionAngleWrap360 = 360.0f;
constexpr int kAngleWrapMaxSteps = 64;  // 0x44D9C0 and 0x44DB60 unrolled by 8 the counter stops at 0x40 0x5a3ec0 particle high speed gate
constexpr float kCollisionHighSpeed = 25.0f;
constexpr float kRadToDeg = 57.2958f;  // 0x5a3c64 180 over pi math atan2 deg conversion 0x5a6a14 boost speed gate kmh mini turbo target base
constexpr float kBoostSpeedGateKmh = 120.0f;
constexpr float kBoostSuspensionDecay = 0.86f;  // 0x5a6a1c rear suspension linked float decay per tick 0x5a6a34 collision high speed engine pitch bump scale
constexpr float kCollisionEnginePitchScale = 250.0f;
constexpr float kCollisionRestitutionDurability = 0.06f;  // 0x5a6a44 collision restitution durability term 0x5a6a74 draft factor dz lower bound
constexpr float kDraftMinZ = -8.0f;
constexpr float kSuspensionShakeScale = 0.000204082f;  // 0x5a6a80 suspension shake sum scale vehicle kind 2 0x5a6a88 suspension shake clamp floor
constexpr float kSuspensionShakeClampFloor = -25.0f;
constexpr float kSuspensionShakeSign = -2.0f;  // 0x5a6a8c suspension shake sign flip 0x5a6aa8 lean wobble state 7 8 9 grow back constant
constexpr float kLeanWobbleGrowBack = 0.95f;
constexpr float kLeanWobbleDecayPair = 0.82f;  // 0x5a6aac lean wobble state 4 5 decay constant 0x5a6ab0 lean wobble state 1 2 3 grow back constant
constexpr float kLeanWobbleGrowBack118 = 1.18f;
constexpr float kEngineForceHighSpeedTurn90 = 90.0f; // 0x5eb6f8 turn force baseline additive term

// crash recovery windows game 0x13972f4 state machine milliseconds state 1 wait to land lower bound
constexpr int kCrashRecoveryLandWindowLowMs = 800;
constexpr int kCrashRecoveryLandWindowHighMs = 2001;  // state 1 wait to land upper bound state 2 settle window
constexpr int kCrashRecoverySettleMs = 1001;
constexpr int kCrashRecoveryKeyWindowMs = 200;  // state 3 recover key press reward window state 3 overall timeout with no key press
constexpr int kCrashRecoveryTimeoutMs = 1000;

// EFFECTS AND GEAR effect codes

constexpr float kEffect100SpeedScale = 0.967f;  // 0x5a6a00 effect 100 speed scale 0x5a6a04 effect 200 speed scale
constexpr float kEffect200SpeedScale = 0.98f;
constexpr float kEffect400And1000SpeedScale = 0.88f;  // 0x5a69e8 effect 400 and 1000 speed scale 0x5a69ec effect 500 speed scale
constexpr float kEffect500SpeedScale = 0.93f;
constexpr float kEffect700SpeedScale = 0.94f;  // 0x5a69e4 effect 700 speed scale same address as kTurnForceSlowFlagMul 0x5a69fc effect 100 end cap effect 400 clamp
constexpr float kEffect100EndCap = 540.0f;
constexpr float kEffect100RampRateMs = 1000.0f;  // 0x5a3bf4 effect 100 and 400 wobble ramp rate per second 0x59ff80 effect 200 wobble decay rate per second
constexpr float kEffect200DecayRateMs = 200.0f;
constexpr float kEffect300ZeroTerm = 0.0f;  // 0x59f44c effect 300 zeroes velocity same as kZero 0x5a69e0 effect 300 secondary accumulator rate
constexpr float kEffect300AccumRate = 200000.0f;
constexpr float kEffect300QuadraticTerm = 29.4f;  // 0x5a69f4 effect 300 end condition quadratic term 0x5a69f0 effect 300 wobble recovery rate docs disagree hex decodes 300
constexpr float kEffect300RecoveryRate = 300.0f;
constexpr float kEffect300SecondaryMul = 1.005f;  // 0x5a69f8 effect 300 secondary accumulator per frame multiplier 0x42080000 immediate effect 300 apply seed value
constexpr float kEffect300ApplySeed = 34.0f;
// car remote effect update 0x496600 a remote spin turns 800 a second and ends past 720
constexpr float kRemoteEffect100Rate = 800.0f;
constexpr float kRemoteEffect100End = 720.0f;
// 0x496600 the remote flip of 200 falls on 19 6 the remote wobble of 400 700 1000 holds at 1080
constexpr float kRemoteEffect200Gravity = 19.6f;
constexpr float kRemoteEffectWobbleCap = 1080.0f;

// EFFECTS AND GEAR RK4 and tire model

constexpr float kRk4SixthWeight = 0.1667f;  // 0x5a8378 gear rk4 step coeffs one sixth final weight 0x5a8370 gear rk4 combine clamp lower bound also 0xc9742400 snap
constexpr float kRk4ClampMin = -1000000.0f;
constexpr float kRk4ClampMax = 1000000.0f;  // 0x5a8374 gear rk4 combine clamp upper bound also 0x49742400 snap 0x5a3bec gear tire force model minimum slip speed gate
constexpr float kTireSlipMinSpeed = 0.001f;
constexpr float kGearSolverDtBlendA = 0.2f;  // 0x5a15ec and 0x5a1650 gear wheel solver dt mode 2 blend same as kMiniTurboThresholdFloor and kDriftChargeFloorScale
constexpr float kGearSolverDtBlendB = 0.3f;

// BODY MOTION chassis rk4 velocity clamp and dt fractions

constexpr float kChassisVelocityClampXY = 120.0f;  // 0x5a6a14 chassis integrate k1 velocity x and y high clamp 0x5a8390 chassis integrate k1 velocity x and y low clamp
constexpr float kChassisVelocityClampXYNeg = -120.0f;
constexpr float kChassisVelocityClampZ = 60.0f;  // 0x5a6a68 chassis integrate k1 velocity z high clamp 0x5a6a90 chassis integrate k1 velocity z low clamp
constexpr float kChassisVelocityClampZNeg = -60.0f;

// BODY MOTION settled the k1 to k4 fields the quaternion renormalize gate and the third weight

constexpr float kRk4ThirdWeight = 0.33333334f;  // 0x5a6b2c chassis wheel and engine rk4 combine one third also box inertia 0x5a8388 double quaternion norm squared renormalize low gate
constexpr double kQuatRenormLow = 0.9999;
constexpr double kQuatRenormHigh = 1.0001;  // 0x5a8380 double quaternion norm squared renormalize high gate 0x5a24ec quat to matrix scaled two over the norm squared
constexpr float kQuatScaledTwo = 2.0f;
constexpr double kWheelSpinWrap = 6.2831855;  // 0x5a83a8 double wheel spin angle fmod two pi 0x5a8398 double drive torque split over two driven wheels
constexpr double kDriveTorqueHalf = 0.5;
constexpr double kDriveTorqueQuarter = 0.25;  // 0x5a83a0 double drive torque split over four wheels 0x5a6a48 steer front wheels tangent times this swings the axle
constexpr float kSteerSwingScale = 1.5f;
constexpr float kNormalizeGate = -7.59e15f;  // 0x5a3c70 bit pattern 0xd9d7bdbb vec3 normalize never rejects 0x3df5c28f immediate at 0x4efd23 slerp weight toward the tilt fix
constexpr float kAxisCorrectSlerp = 0.12f;
constexpr float kAxisCorrectZRamp = 6.0f;  // 0x40c00000 at 0x4efe5d spin z ramp target two unloaded wheels or less 0x4efe31 more unloaded wheels zeroes z spin
constexpr int kAxisCorrectZCountGate = 2;
constexpr float kJacobiThreshold = 0.022222f;  // 0x5a8360 jacobi sweep threshold 0 dot 2 over 9 0x5a1648 jacobi small rotation gate same as kWheelBumpCap
constexpr float kJacobiHundred = 100.0f;
constexpr int kJacobiSweeps = 50;  // immediate 0x32 in 0x4edbf0 jacobi sweep cap d3dx quaternion slerp falls back to a lerp under this gap
constexpr float kSlerpLinearGate = 0.001f;
constexpr float kGripScaleLoad = 0.98f;  // 0x3f7ae148 at 0x4ec95a body create grip and gravity scale 0x3ac49ba6 at 0x495607 game 0x30 tick cuts frame into ceil frame
constexpr float kFixedPhysicsStepSeconds = 0.0015f;

// SUSPENSION AND TELEPORT tyre model constants read and the body create immediates

constexpr float kCurveCubicThree = 3.0f;  // 0x5a32b8 curve eval cubic weight same as kSteerScaleThree 0x5a3238 curve eval cubic weight
constexpr float kCurveCubicFive = 5.0f;
constexpr float kCurveCubicTwo = 2.0f;  // 0x5a24ec curve eval cubic weight same as kStatClampCeilTwo 0x5a0054 curve eval cubic weight same as kSpringDecayFourChannels
constexpr float kCurveCubicFour = 4.0f;
constexpr float kTireLoadCapBase = 400000.0f;  // 0x5a83b0 tyre load cap at zero tilt 0x5a8374 tyre load cap drops by this per unit of tilt
constexpr float kTireLoadCapSlope = 1000000.0f;
constexpr float kTireCamberCapForLoad = 0.2f;  // 0x5a15ec tilt clamp inside the load cap same as kMiniTurboThresholdFloor 0x5a32d4 slip magnitude squared weight same as kSmoothQuarter
constexpr float kTireShapeQuarter = 0.25f;
constexpr float kTireSlipFlagBits = 1.4013e-45f;  // integer 1 stored in the float slot of result 4 0x47a7dbd5 immediate at 0x4ec9c9 body create lateral stiffness
constexpr float kTireCorneringStiffness = 85947.66f;
constexpr float kTireLongStiffness = 85943.0f;  // 0x47a7db80 immediate body create longitudinal stiffness 0x3f99999a immediate body create magic formula shape C
constexpr float kTireShapeC = 1.2f;
constexpr float kTireShapeE = -0.2f;  // 0xbe4ccccd immediate body create magic formula shape E 0x3851b717 immediate body create the peak drops with load
constexpr float kTireLoadSensitivity = 0.00005f;
constexpr float kTireLatLongRatio = 1.0f;  // 0x3f800000 immediate body create lateral over longitudinal peak 0x3e4ccccd immediate body create longitudinal relaxation length
constexpr float kTireRelaxLong = 0.2f;
constexpr float kTireRelaxLat = 0.2f;  // 0x3e4ccccd immediate body create lateral relaxation length 0x3f800000 immediate body create low speed parabola width
constexpr float kTireLowSpeedBlend = 1.0f;
constexpr float kTireLoadRatioShape = 6.0f;  // 0x40c00000 immediate body create load ratio shape 0x40400000 immediate body create slip flag threshold
constexpr float kTireSlipFlagThreshold = 3.0f;
constexpr float kTireGeometryFifty = 50.0f;  // 0x42480000 immediate body create shared 5 and 6 never read 0x41a00000 immediate body create shared 7 never read
constexpr float kTireGeometryTwenty = 20.0f;
constexpr float kSetupDamper = 6000.0f;  // 0x45bb8000 at 0x495009 damper and 0x48c35000 at 0x49501a tyre spring both front and rear
constexpr float kSetupTireSpring = 400000.0f;
constexpr float kSetupChannelRateMass = 5.0f;  // 0x40a00000 at 0x495101 channel rates 0 and 5 accel reverse 0x3f800000 at 0x4950fc channel rates 1 and 4 brake handbrake
constexpr float kSetupChannelRateFriction = 1.0f;

// CATALOGUE Data Car default car byte for byte offsets in the file the exe reads it through dx8 rlg dll

constexpr float kDefaultCarChassisExtentX = 4.0f;  // file 0x000 chassis box full extent file 0x004
constexpr float kDefaultCarChassisExtentY = 2.2f;
constexpr float kDefaultCarChassisExtentZ = 0.8f;  // file 0x008 file 0x00c
constexpr float kDefaultCarChassisMass = 200.0f;
constexpr float kDefaultCarLowerExtentX = 1.4f;  // file 0x010 second box full extent file 0x014
constexpr float kDefaultCarLowerExtentY = 0.8f;
constexpr float kDefaultCarLowerExtentZ = 0.2f;  // file 0x018 file 0x01c
constexpr float kDefaultCarLowerMass = 800.0f;
constexpr float kDefaultCarLowerOffsetX = 0.056f;  // file 0x020 bit pattern 0x3d656042 file 0x024
constexpr float kDefaultCarLowerOffsetZ = -0.54f;
constexpr float kDefaultCarWheelRadius = 0.48f;  // file 0x028 and 0x02c file 0x030 and 0x034
constexpr float kDefaultCarWheelWidth = 0.4f;
constexpr float kDefaultCarWheelMass = 80.0f;  // file 0x038 and 0x03c file 0x040
constexpr float kDefaultCarFrontAxleX = 1.26f;
constexpr float kDefaultCarAxleHalfTrack = 1.208f;  // file 0x044 bit pattern 0x3f9a9fbe file 0x048
constexpr float kDefaultCarAxleZ = -0.2f;
constexpr float kDefaultCarRearAxleX = -1.26f;  // file 0x04c file 0x058 bit pattern 0x3d0f0915 axis x the rear axle has it negated
constexpr float kDefaultCarAxleToe = 0.034920849f;
constexpr float kDefaultCarMaxSteerDeg = 16.0f;  // file 0x094 file 0x098
constexpr int kDefaultCarDriveMode = 2;
constexpr float kDefaultCarClutch = 20.0f;  // file 0x09c file 0x0a0
constexpr int kDefaultCarGearCount = 6;
constexpr float kDefaultCarGear1 = 3.38f;  // file 0x0a4 file 0x0a8
constexpr float kDefaultCarGear2 = 2.05f;
constexpr float kDefaultCarGear3 = 1.43f;  // file 0x0ac file 0x0b0
constexpr float kDefaultCarGear4 = 1.09f;
constexpr float kDefaultCarGear5 = 0.87f;  // file 0x0b4 file 0x0b8
constexpr float kDefaultCarGear6 = 0.7f;
constexpr float kDefaultCarFinalDrive = 2.2f;  // file 0x0bc file 0x0c0
constexpr float kDefaultCarReverseRatio = -8.0f;
constexpr float kDefaultCarUpshiftOmega = 837.75842f;  // file 0x0c4 bit pattern 0x4451708a file 0x0c8 bit pattern 0x43f0db05
constexpr float kDefaultCarDownshiftOmega = 481.71109f;
constexpr float kDefaultCarEngineDragCap = 80.0f;  // file 0x0cc file 0x0d0
constexpr float kDefaultCarEngineDragCoef = 80.0f;
constexpr float kDefaultCarEngineInertia = 4.0f;  // file 0x0d4 file 0x0ec
constexpr float kDefaultCarBrakeCoef = 3000.0f;
constexpr float kDefaultCarBrakeFront = 20000.0f;  // file 0x0f0 file 0x0f4
constexpr float kDefaultCarBrakeRear = 30000.0f;
constexpr float kDefaultCarHandbrakeRear = 60000.0f;  // file 0x0f8 file 0x0fc
constexpr float kDefaultCarNeutralDisplay = 0.9f;
constexpr float kDefaultCarSpring = 80000.0f;  // file 0x100 and 0x104 file 0x108 and 0x10c
constexpr float kDefaultCarDamper = 6800.0f;
constexpr float kDefaultCarAntiroll = 6800.0f;  // file 0x110 and 0x114 file 0x118 and 0x11c
constexpr int kDefaultCarDiscCurveCount = 20;
constexpr float kDefaultCarDiscEdgePower = 16.0f;  // file 0x120 and 0x124 file 0x128 and 0x12c
constexpr float kDefaultCarTireSpring = 800000.0f;
constexpr float kDefaultCarTireGripFront = 3.8f;  // file 0x130 and 0x138 file 0x134 and 0x13c
constexpr float kDefaultCarTireGripRear = 4.8f;
constexpr int kDefaultCarTorqueCount = 20;  // file 0x140 torque curve count file 0x148 bit pattern 0x447b53c8 engine speed at the last entry
constexpr float kDefaultCarTorqueXMax = 1005.3091f;
constexpr float kDefaultCarTorqueScale = 0.018899601f;  // file 0x14c bit pattern 0x3c9ad36c 19 over the span file 0x150 to 0x19c engine torque per engine speed step
constexpr float kDefaultCarTorqueTable[20] = {
    12269.0f, 12683.001f, 13099.002f, 13440.0f, 13724.001f, 13934.001f, 14000.0f, 14000.0f, 13790.064f, 13373.626f,
    12817.188f, 12120.759f, 11283.958f, 10308.976f, 9055.4199f, 7523.251f, 5851.7944f, 3901.6938f, 1951.5431f, 1.350266f};

// SUSPENSION AND TELEPORT

constexpr float kSuspensionCompressionReadScale = 0.00001f;  // 0x5a6ad8 tick reads suspension compression same address as kSuspensionBumpScale 0x5a32d4 tick weights wheel compression into bump same as kSmoothQuarter
constexpr float kSuspensionBumpWeight = 0.25f;
constexpr float kSuspensionBumpSignGate = -1.0f;  // 0x5a3720 tick sign gate on scaled compression same as kSuspensionCompressionFloor 0x5a32a0 respawn crash recovery reprobe distance bound
constexpr float kRespawnReprobeDistance = 80.0f;
constexpr float kRespawnTeleportHeightBias = 3.0f; // 0x5a32b8 respawn state machine teleport height bias

constexpr int kRespawnDirectionDebounceMs = 2501;  // 0x9c5 respawn direction change debounce 0x7d0 respawn commit timer
constexpr int kRespawnCommitTimeoutMs = 2000;
constexpr int kRespawnGridRowWidth = 400;         // 0x190 respawn grid table row width
// 0x4A0A96 an index step of 5 or more is a lap wrap not a wrong way move
constexpr int kRespawnIndexStepLimit = 5;
// 0x4A0B40 tracks 63 and 42 run list 0 backward over these rows so they never escalate
constexpr int kRespawnLoopTrackA = 63;
constexpr int kRespawnLoopTrackAFirst = 116;
constexpr int kRespawnLoopTrackALast = 130;
constexpr int kRespawnLoopTrackB = 42;
constexpr int kRespawnLoopTrackBFirst = 56;
constexpr int kRespawnLoopTrackBLast = 86;
// 0x487250 the battle arena track id the watchdog stands still there
constexpr int kBattleTrackId = 30000000;

// TICK car boost start 0x496BE0 and car boost update 0x496E50 immediates with the code address

constexpr float kBoostDurationKind0ScaleMs = 400.0f;  // 0x5a6a18 kind 0 ms per unit stat 3 plus one clamped 1 to 2 0x4970c8 kind 1 plus part bonus
constexpr int kBoostDurationKind1Ms = 3800;
constexpr int kBoostDurationKind2Ms = 6000;  // 0x4970d9 immediate kind 2 plus the part bonus 0x4970e6 immediate kind 3
constexpr int kBoostDurationKind3Ms = 6000;
constexpr int kBoostDurationKind4Ms = 5000;  // 0x4970f2 immediate kind 4 0x4970fe immediate kind 5
constexpr int kBoostDurationKind5Ms = 15000;
constexpr int kBoostDurationKind6Ms = 5;  // 0x497039 immediate kind 6 the cancel pseudo kind 0x49710a immediate kind 7
constexpr int kBoostDurationKind7Ms = 1500;
constexpr int kBoostDurationPartBonusMs = 500;  // 0x49701e immediate part catalogue type 0x16 adds to kind 1 and 2 0x496cfe immediate part catalogue type 0x15 decay bonus
constexpr float kBoostPartBonusScale = 0.04f;
constexpr float kBoostMiniTurboDecayStrength = 6.0f;  // 0x496d31 immediate kind 0 decay strength car 0x3308 0x59f480 kind 0 target speed scale floor
constexpr float kBoostMiniTurboSpeedScaleFloor = 1.0f;
constexpr float kBoostMiniTurboSpeedScaleCeil = 1.2f;  // 0x5a32b0 kind 0 target speed scale ceil 0x496d98 immediate 0x435c0000 kind 1 target speed
constexpr float kBoostKind1TargetKmh = 220.0f;
constexpr float kBoostKind2TargetKmh = 260.0f;  // 0x496da9 immediate 0x43820000 kind 2 target speed 0x496dc9 immediate 0x43480000 kind 3 5 7 target speed kind 4 untouched
constexpr float kBoostItemTargetKmh = 200.0f;
constexpr float kBoostHudKind0 = 0.7f;  // 0x496e11 immediate game 0x1397304 when the kind is 0 0x496dfd immediate game 0x1397304 for every other kind
constexpr float kBoostHudOther = 0.3f;
constexpr float kBoostPushYawScale = 1.8f;     // 0x5a3214 boost push heading is yaw minus smoothed gauge times this

// TICK car substep collision response 0x498960 immediates and the reads it makes

constexpr float kCollisionRestitutionNormal = 0.98f;  // 0x498b2b immediate 0x3f7ae148 restitution not stuck 0x498b35 immediate 0x3f70a3d7 restitution while stuck
constexpr float kCollisionRestitutionStuck = 0.94f;
constexpr float kCollisionImpulseFloor = 0.5f;  // 0x498b97 immediate impulse scale floor also 0x59f414 0x498bb2 immediate impulse scale ceiling also 0x5a32b8
constexpr float kCollisionImpulseCeil = 3.0f;
constexpr float kCollisionSpinCeil = 1.5f;  // 0x498c13 immediate spin term ceiling also 0x5a6a48 0x5a3eb8 spin term one minus curve times kmh times this
constexpr float kCollisionSpinKmhScale = 0.04f;
constexpr float kCollisionFrictionFloorLow = 0.8f;  // 0x498d46 immediate 0x3f4ccccd friction floor under 3 wheels 0x5a32a8 impact heading fold band
// the calls before 0x49D0A5 and at 0x4A3082 both push 1 0 as the collision response time argument
constexpr float kCollisionImpulseTime = 1.0f;
constexpr float kCollisionAngleFold180 = 180.0f;
constexpr float kCollisionAngleFold270 = 270.0f;  // 0x5a6a50 impact heading fold band 0x5a6a4c one over 90 folded heading to 0 1
constexpr float kCollisionAngleToUnit = 0.0111111f;
constexpr float kCollisionAngleToIndex = 100.0f;     // 0x5a1648 unit heading to curve index

// TICK car physics tick local 0x49C0D0 reads settled in round four

constexpr float kSpeedCurveOffsetKmh = 30.0f;  // 0x5a3244 speed curve index is kmh minus this times half 0x59f414 speed curve index scale
constexpr float kSpeedCurveScale = 0.5f;
constexpr float kGravityPerSubstepScale = -0.16f;  // 0x5a6ae0 times the tuning at 0x5eb6f0 gives the world z push 0x5a05e0 wire stat 0 per tick velocity gain
constexpr float kVelocityScaleStat = 0.01f;
constexpr float kVelocityScaleCeil = 1.01f;  // 0x5a6ae4 per tick velocity gain ceiling 0x5a6afc throttle loss per kmh above the turn force cap
constexpr float kThrottleSpeedGapScale = 0.00014f;
constexpr float kLateralPushMagnitude = 0.3f;  // 0x49ceb6 immediate 0x3e99999a push along the heading while turning 0x5a2494 air kick ramp per tick global 0x2eb0688
constexpr float kAirKickStep = 0.1f;
constexpr float kAirKickCap = 8.0f;  // 0x5a15f0 air kick ramp cap and reset value 0x5a32b8 the ramp pushes only above this
constexpr float kAirKickPushGate = 3.0f;
constexpr float kLandingHeightScale = 100.0f;  // 0x5a1648 landing particle height term 0x5a2494 engine kick clears under this
constexpr float kEngineKickFloor = 0.1f;
constexpr float kFrictionPairHalf = 0.5f;  // 0x59f414 friction pair average 0x5a15ec per wheel grip is wire stat 12 times grip plus extra times this
constexpr float kGripExtraScale = 0.2f;
constexpr float kFrictionPairDriftGas = 0.6f;  // 0x5a164c friction pair x while drifting on gas 0x5a322c friction pair x while drifting off gas
constexpr float kFrictionPairDriftOffGas = 0.8f;
constexpr float kFrictionPairSlow = 8.0f;  // 0x5a15f0 friction pair both while slow flagged 0x5a6b04 friction pair both while the view global 0x2f0ddb0 is 2
constexpr float kFrictionPairRearView = 0.75f;

// TICK ghost ring car ghost sample record 0x49FAD0

constexpr int kGhostRecordIntervalTicks = 10;  // 0x49fb0e immediate every 10 ticks outside race mode 0xd 0x49feaa immediate 0x960 wrap outside race mode 0xd
constexpr int kGhostRingWrapNormal = 2400;
constexpr int kGhostRingWrapMode0xD = 24000;  // 0x49fe9e immediate wrap in race mode 0xd every tick

// TICK round six the marker sweep every value read on the bytes

constexpr float kDraftMaxDistance = 150.0f;  // 0x5eb710 and 0x5eb714 car draft factor max distance and cone half angle read at 0x49cbe1 and 0x49cbdb
constexpr float kDraftConeHalfAngleDeg = 30.0f;
constexpr int kPetChaiChancePercent = 3;  // 0x49b14d pet kind 0x14 rolls rand mod 100 under this pet key 30 kind 0x451490 adds 0 04 boost decay
constexpr int kPetKindBoostDecay = 0x15;
constexpr int kPetKindBoostDuration = 0x16;  // 0x451490 pet key to kind key 40 adds 500 ms to kind 1 and 2 key 20 is Chai chance
constexpr int kPetKindChai = 0x14;
constexpr float kPushApartVelocityScale = 0.9940f;  // 0x3f7e76c9 at 0x498880 self body velocity scale 0x5a322c x and y of unit delta added to body velocity
constexpr float kPushApartVelocityDirScale = 0.8f;
constexpr float kPushApartOverlapMaxDistance = 10.0f;  // 0x59f404 world car overlap test plane distance gate 0x5a322c the two chassis y extents summed times this
constexpr float kPushApartOverlapExtentScale = 0.8f;
constexpr float kRivalCueSpeedKmh = 30.0f;  // 0x5a3244 car rival nearby cue needs this speed 0x41f00000 immediate at 0x49a1d8 nearest racing car range
constexpr float kRivalCueRange = 30.0f;
constexpr float kRivalCueAngleLow = 70.0f;  // 0x5a6998 bearing window low bound 0x5a6994 bearing window high bound
constexpr float kRivalCueAngleHigh = 290.0f;
constexpr int kRivalCuePeriodMs = 3000;  // 0x49a175 immediate 2999 the cue repeats after this driver animation state the rival cue skips
constexpr int kDriverAnimHit = 3;
constexpr int kDriverAnimEleven = 0xb;  // driver animation state the rival cue skips 0x5a6a28 one over 32768 raw pad axis minus 0x8000 to minus 1
constexpr float kPadAxisScale = 3.0517578e-05f;
constexpr float kPadSteerRightThreshold = 0.2f;  // 0x5a15ec pad axis above this synthesises slot 3 0x5a6a20 pad axis below this synthesises slot 2
constexpr float kPadSteerLeftThreshold = -0.2f;
constexpr float kPadBrakeAnalog = 65536.0f;  // 0x5a6a24 analog pedal value with the brake button 0x47000000 immediate at 0x4972f5 analog pedal value with no button
constexpr float kPadAnalogCenter = 32768.0f;
constexpr int kPadPressedHoldMs = 100;  // 0x497375 immediate the pad pressed latch releases after this 0x5a32d0 device 1 steer axis decays by this fraction when idle
constexpr float kMouseSteerDecay = 0.03125f;
constexpr float kMouseSteerGain = 0.00390625f;  // 0x5a6a2c device 1 mouse delta to steer axis 0x5a3bf4 brake key brakes above this rpm else it reverses
constexpr float kReverseBrakeRpm = 1000.0f;
constexpr float kReverseBrakeSpeed = 1.0f;  // 0x59f480 brake key brakes at or above this speed 0x3f59999a immediate at 0x497d8a race mode 9 coast brake
constexpr float kCoastBrakeMode9 = 0.85f;
constexpr int VK_DEBUG_BOOST_KIND0 = 0x31;  // 0x49c3b0 key 1 and 0x49c3ce key 2 at green light start kind 0 and kind 1 boost
constexpr int VK_DEBUG_BOOST_KIND1 = 0x32;
constexpr int VK_DEBUG_BOOST_KIND2 = 0x33;  // 0x49c3ec key 3 at green light starts kind 2 boost 0x5a6b1c gauge nibble to units decode adds one extra step
constexpr float kRemoteGaugeNibbleUnit = 0.0666667f;
constexpr float kRemoteGroundClearance = 0.34f;  // 0x5a6b24 remote z floor above the cell plane 0x5a1650 remote z under plane plus clearance plus this snaps
constexpr float kRemoteGroundSnapBand = 0.3f;
constexpr float kRemoteAnchorDropGate = 0.8f;  // 0x5a322c a falling anchor target more than this over the plane snaps 0x59f414 PUSH cell fan step
constexpr float kRemotePushScanStep = 0.5f;
constexpr float kRemotePushScanRange = 30.0f;  // 0x5a3244 PUSH cell fan range 0x5a1648 edge normal times this gives the slide bearing point
constexpr float kRemoteEdgeSlideReach = 100.0f;
constexpr float kRemoteEdgeSlideStep = 0.2f;  // 0x3e4ccccd immediate at 0x49f6a0 slide step along the edge 0x5a15ec anchor distance times this is the slide length
constexpr float kRemoteEdgeSlideScale = 0.2f;
constexpr float kRemoteRecoveryHeightGate = 0.64f;  // 0x5a6b10 recovery state 2 holds while this high over the plane 0x5a6b14 recovery state 1 scale growth per tick
constexpr float kRemoteRecoveryGrow = 1.068f;
constexpr float kRemoteRecoveryDecay = 0.958f;  // 0x5a6b0c recovery state 3 scale decay per tick 0x49fa1a immediate 0x5dd state 2 holds this long
constexpr int kRemoteRecoveryHoldMs = 1501;
constexpr float kLaunchKickScale = 1.6f;  // 0x5a68cc car launch pad kick value times this 0x42200000 at 0x4c8640 car push along yaw on the drop
constexpr float kGimmickDropPushStrength = 40.0f;
constexpr float kGimmickDropHeightBias = 5.0f;  // 0x5a3238 the drop places the car this high over the follow row 0x40666666 at 0x4c8350 slot target advance per tick
constexpr float kGimmickCarryStep = 3.6f;
constexpr float kGimmickCarryEase = 0.1f;  // 0x5a2494 slot position eases by this toward its target 0x5a3230 respawn follow advance point steps past this range
constexpr float kFollowAdvanceRadius = 15.0f;
// 0x4C7F6E the carry pool grabs its car with the effect 600 the blue rabbit pool 0x4BA380 with 900
constexpr int kGimmickCarryGrabCode = 600;
constexpr int kGimmickBlueGrabCode = 900;
constexpr int kGimmickGrabLockMs = 600;  // 0x4c8078 immediate state 1 to 100 after this 0x4c80d8 immediate the carry window
constexpr int kGimmickCarryMs = 3000;
constexpr int kGimmickCarryLicenceMs = 6000;  // 0x4c80ea immediate the carry window in licence test 1 0x4c80f0 immediate added for a remote car
constexpr int kGimmickCarryRemoteExtraMs = 1000;
constexpr int kGimmickCarryAbilityExtraMs = 2000;  // 0x4c8100 immediate added with ability 3 0x4c80a5 immediate 0x514 the land effect plays after this
constexpr int kGimmickCarryLandMs = 1300;
constexpr int kGimmickReleaseBoostLowMs = 200;  // 0x4c8709 immediate release boost window low 0x4c8716 immediate release boost window high
constexpr int kGimmickReleaseBoostHighMs = 1000;
constexpr int kGimmickReleaseSlotMs = 1300;  // 0x4c8746 immediate 0x514 the slot frees after this car has ability 0x4b8580 id checked by the carry window
constexpr int kGimmickAbilityCarry = 3;
constexpr int kLicenceTestCollisionCount = 0x17; // 0x498dbb immediate licence test 23 counts the collisions

} // namespace KnC Kart Client
