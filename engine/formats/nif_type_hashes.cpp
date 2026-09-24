// RTTI table type names as hash not string djb2 hash 0xBD8B18FD
#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

uint32_t hash_type_name(const char* name) {
    uint32_t hash = 0;
    for (const char* character = name; *character != '\0'; ++character)
        hash = hash * 33u + static_cast<unsigned char>(*character);
    return hash;
}

// Measured union RTTI tables HBO JS AK KnC streams names
const char* const kTypeNames[] = {
    "NiAlphaController", "NiAlphaProperty", "NiAmbientLight", "NiBSplineBasisData",
    "NiBSplineCompFloatInterpolator", "NiBSplineCompPoint3Interpolator",
    "NiBSplineCompTransformInterpolator", "NiBSplineData",
    "NiBillboardNode", "NiBoolData", "NiBoolInterpolator", "NiBoolTimelineInterpolator",
    "NiBooleanExtraData", "NiCamera", "NiCollisionData", "NiColorData",
    "NiColorExtraData", "NiColorExtraDataController", "NiColorInterpolator",
    "NiControllerSequence",
    "NiDeferredLightDimmerController", "NiDeferredPointLight", "NiDirectionalLight",
    "NiPointLight", "NiSpotLight",
    "NiDitherProperty",
    "NiFlipController", "NiFloatData", "NiFloatExtraData", "NiFloatExtraDataController",
    "NiFloatInterpolator", "NiGeomMorpherController", "NiIntegerExtraData",
    "NiIntegersExtraData",
    "NiKeyframeController", "NiKeyframeData", "NiLODNode", "NiLightColorController",
    "NiLines", "NiLinesData", "NiLookAtInterpolator", "NiMaterialColorController",
    "NiMaterialProperty", "NiMeshPSysData", "NiMeshParticleSystem", "NiMorphData",
    "NiMultiTargetTransformController", "NiNode", "NiPSysAgeDeathModifier",
    "NiPSysBombModifier",
    "NiPSysBoundUpdateModifier", "NiPSysBoxEmitter", "NiPSysColliderManager",
    "NiPSysColorModifier",
    "NiPSysCylinderEmitter", "NiPSysData", "NiPSysEmitterCtlr",
    "NiPSysEmitterDeclinationCtlr",
    "NiPSysEmitterDeclinationVarCtlr", "NiPSysEmitterInitialRadiusCtlr",
    "NiPSysEmitterLifeSpanCtlr", "NiPSysEmitterPlanarAngleCtlr",
    "NiPSysEmitterPlanarAngleVarCtlr", "NiPSysEmitterSpeedCtlr", "NiPSysGravityModifier",
    "NiPSysGravityStrengthCtlr",
    "NiPSysGrowFadeModifier", "NiPSysInitialRotAngleCtlr", "NiPSysInitialRotAngleVarCtlr",
    "NiPSysInitialRotSpeedCtlr",
    "NiPSysInitialRotSpeedVarCtlr", "NiPSysMeshEmitter", "NiPSysMeshUpdateModifier",
    "NiPSysModifierActiveCtlr",
    "NiPSysPlanarCollider", "NiPSysPositionModifier", "NiPSysResetOnLoopCtlr",
    "NiPSysRotationModifier",
    "NiPSysSpawnModifier", "NiPSysSphereEmitter", "NiPSysUpdateCtlr", "NiParticleSystem",
    "NiPathInterpolator", "NiPersistentSrcTextureRendererData", "NiPhysXScene",
    "NiPhysXSceneDesc",
    "NiPixelData", "NiPoint3Interpolator", "NiPortal", "NiPosData",
    "NiRangeLODData", "NiRoom", "NiRoomGroup", "NiShadeProperty",
    "NiSkinData", "NiSkinInstance", "NiSkinPartition", "NiSortAdjustNode",
    "NiSourceTexture", "NiSpecularProperty", "NiStencilProperty", "NiStringExtraData",
    "NiStringPalette", "NiTextKeyExtraData", "NiTextureEffect",
    "NiTextureTransformController",
    "NiTexturingProperty", "NiTransformController", "NiTransformData",
    "NiTransformInterpolator",
    "NiTriShape", "NiTriShapeData", "NiTriStrips", "NiTriStripsData",
    "NiVertexColorProperty", "NiVisController", "NiVisData", "NiZBufferProperty",
};

std::string hex_hash(uint32_t hash) {
    static const char kDigits[] = "0123456789ABCDEF";
    std::string text = "hash:";
    for (int shift = 28; shift >= 0; shift -= 4)
        text.push_back(kDigits[(hash >> shift) & 0xFu]);
    return text;
}

} // namespace

std::string type_name_for_hash(uint32_t hash) {
    for (const char* name : kTypeNames)
        if (hash_type_name(name) == hash) return name;
    return hex_hash(hash);
}

}
