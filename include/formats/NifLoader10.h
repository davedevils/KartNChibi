#pragma once
// NifLoader10.h - NIF loader for Gamebryo 10.x format
// Based on niftools/nifxml + niftools/niflib structure

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <map>

namespace knc {

// Forward declarations
class NiObject;

// NIF Version helper
struct NiVersion {
    uint32_t file = 0;
    uint32_t user = 0;
    
    static uint32_t ToFile(int a, int b, int c, int d) {
        return (a << 24) | (b << 16) | (c << 8) | d;
    }
    
    // Version constants
    static const uint32_t V10_0_1_0 = 0x0A000100;
    static const uint32_t V10_0_1_2 = 0x0A000102;
    static const uint32_t V10_0_1_3 = 0x0A000103;
    static const uint32_t V10_1_0_0 = 0x0A010000;
    static const uint32_t V10_1_0_106 = 0x0A01006A;
    static const uint32_t V10_1_0_114 = 0x0A010072;
    static const uint32_t V10_2_0_0 = 0x0A020000;
    static const uint32_t V20_0_0_4 = 0x14000004;
};

// Basic vector types (matching nif.xml)
struct NiVector3 { float x = 0, y = 0, z = 0; };
struct NiVector4 { float x = 0, y = 0, z = 0, w = 0; };
struct NiTexCoord { float u = 0, v = 0; };
struct NiColor4 { float r = 0, g = 0, b = 0, a = 0; };
struct NiMatrix33 { float m[9] = {1,0,0, 0,1,0, 0,0,1}; };
struct NiQuaternion { float w = 1, x = 0, y = 0, z = 0; };
struct NiBound { NiVector3 center; float radius = 0; };

// NiTransform (matches nif.xml struct)
struct NiTransform {
    NiMatrix33 rotation;
    NiVector3 translation;
    float scale = 1.0f;
};

// Reference to another block
struct NiRef {
    int32_t index = -1;
    bool IsValid() const { return index >= 0; }
};

// KeyType enum (from nif.xml)
enum class KeyType : uint32_t {
    LINEAR = 1,
    QUADRATIC = 2,
    TBC = 3,
    XYZ_ROTATION = 4,
    CONST = 5
};

// Key<T> structure (from nif.xml)
template<typename T>
struct NiKey {
    float time = 0;
    T value;
    T forward;   // For QUADRATIC
    T backward;  // For QUADRATIC
    NiVector3 tbc; // For TBC (tension, bias, continuity)
};

// KeyGroup<T> (from nif.xml)
template<typename T>
struct NiKeyGroup {
    uint32_t numKeys = 0;
    KeyType interpolation = KeyType::LINEAR;
    std::vector<NiKey<T>> keys;
};

// NIF file stream
class NiStream {
public:
    NiStream(const uint8_t* data, size_t size);
    
    uint8_t ReadU8();
    uint16_t ReadU16();
    uint32_t ReadU32();
    int32_t ReadI32();
    float ReadFloat();
    std::string ReadString();  // Sized string: uint32 len + chars
    std::string ReadShortString(); // uint8 len + chars
    void ReadBytes(void* dest, size_t count);
    
    // Read name using string table (for version >= 20.1.0.1) or sized string
    std::string ReadName();
    
    NiVector3 ReadVector3();
    NiVector4 ReadVector4();
    NiTexCoord ReadTexCoord();
    NiColor4 ReadColor4();
    NiMatrix33 ReadMatrix33();
    NiQuaternion ReadQuaternion();
    NiBound ReadBound();
    NiTransform ReadTransform();
    NiRef ReadRef();
    
    template<typename T>
    void ReadKeyGroup(NiKeyGroup<T>& kg);
    
    void Skip(size_t bytes);
    size_t Position() const { return m_pos; }
    size_t Size() const { return m_size; }
    bool Eof() const { return m_pos >= m_size; }
    
    const NiVersion& GetVersion() const { return m_version; }
    void SetVersion(const NiVersion& v) { m_version = v; }
    
    // Set string table reference for name reading
    void SetStringTable(const std::vector<std::string>* table) { m_stringTable = table; }
    
private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos = 0;
    NiVersion m_version;
    const std::vector<std::string>* m_stringTable = nullptr;
};

//=============================================================================
// NIF Objects (based on nif.xml hierarchy)
//=============================================================================

// Base NIF object
class NiObject {
public:
    virtual ~NiObject() = default;
    virtual void Read(NiStream& stream) = 0;
    virtual const char* GetTypeName() const = 0;
    
    std::string blockType;
    int blockIndex = -1;
};

// NiObjectNET - named object with extra data and controller
class NiObjectNET : public NiObject {
public:
    std::string name;
    std::vector<NiRef> extraDataRefs;
    NiRef controllerRef;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiObjectNET"; }
};

// NiExtraData - base for extra data blocks
class NiExtraData : public NiObject {
public:
    std::string name;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiExtraData"; }
};

// NiAVObject - scene graph base
class NiAVObject : public NiObjectNET {
public:
    uint16_t flags = 0;
    NiTransform transform;
    std::vector<NiRef> propertyRefs;
    NiRef collisionRef;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiAVObject"; }
};

// NiNode - scene graph node with children
class NiNode : public NiAVObject {
public:
    std::vector<NiRef> childRefs;
    std::vector<NiRef> effectRefs;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiNode"; }
};

// NiGeometryData - base mesh data (matches nif.xml)
class NiGeometryData : public NiObject {
public:
    int32_t groupId = 0;
    uint16_t numVertices = 0;
    uint8_t keepFlags = 0;
    uint8_t compressFlags = 0;
    bool hasVertices = true;
    std::vector<NiVector3> vertices;
    uint16_t dataFlags = 0;
    bool hasNormals = false;
    std::vector<NiVector3> normals;
    NiBound boundingSphere;
    bool hasVertexColors = false;
    std::vector<NiColor4> vertexColors;
    std::vector<std::vector<NiTexCoord>> uvSets;
    uint16_t consistencyFlags = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiGeometryData"; }
};

// NiTriBasedGeomData
class NiTriBasedGeomData : public NiGeometryData {
public:
    uint16_t numTriangles = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTriBasedGeomData"; }
};

// NiTriStripsData (matches nif.xml)
class NiTriStripsData : public NiTriBasedGeomData {
public:
    uint16_t numStrips = 0;
    std::vector<uint16_t> stripLengths;
    bool hasPoints = true;
    std::vector<std::vector<uint16_t>> points;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTriStripsData"; }
    
    // Convert strips to triangle list
    std::vector<uint16_t> GetTriangles() const;
};

// NiTriStrips - geometry node
class NiTriStrips : public NiAVObject {
public:
    NiRef dataRef;
    NiRef skinInstanceRef;
    bool hasShader = false;
    std::string shaderName;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTriStrips"; }
};

// NiTriShapeData - triangle mesh data (like NiTriStripsData but with individual triangles)
class NiTriShapeData : public NiTriBasedGeomData {
public:
    bool hasTriangles = true;
    std::vector<uint16_t> triangles;  // indices (3 per triangle)
    
    // Match point groups for stripping
    uint16_t numMatchGroups = 0;
    std::vector<std::vector<uint16_t>> matchGroups;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTriShapeData"; }
    
    // Get triangles as flat list
    std::vector<uint16_t> GetTriangles() const { return triangles; }
};

// NiTriShape - triangle mesh (like NiTriStrips but with individual triangles)
class NiTriShape : public NiAVObject {
public:
    NiRef dataRef;
    NiRef skinInstanceRef;
    bool hasShader = false;
    std::string shaderName;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTriShape"; }
};

// NiSourceTexture
class NiSourceTexture : public NiObjectNET {
public:
    uint8_t useExternal = 1;
    std::string fileName;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiSourceTexture"; }
};

// NiKeyframeData (animation keys)
class NiKeyframeData : public NiObject {
public:
    NiKeyGroup<NiQuaternion> rotationKeys;
    NiKeyGroup<float> xRotations, yRotations, zRotations;
    NiKeyGroup<NiVector3> translations;
    NiKeyGroup<float> scales;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiKeyframeData"; }
};

// NiTransformData (same as NiKeyframeData for v10.2+)
class NiTransformData : public NiKeyframeData {
public:
    const char* GetTypeName() const override { return "NiTransformData"; }
};

// NiPosData
class NiPosData : public NiObject {
public:
    NiKeyGroup<NiVector3> data;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiPosData"; }
};

// NiFloatData
class NiFloatData : public NiObject {
public:
    NiKeyGroup<float> data;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiFloatData"; }
};

// NiTransformInterpolator
class NiTransformInterpolator : public NiObject {
public:
    NiVector3 translation;
    NiQuaternion rotation;
    float scale = 1.0f;
    NiRef dataRef;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTransformInterpolator"; }
};

// NiPoint3Interpolator
class NiPoint3Interpolator : public NiObject {
public:
    NiVector3 value;
    NiRef dataRef;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiPoint3Interpolator"; }
};

// NiFloatInterpolator
class NiFloatInterpolator : public NiObject {
public:
    float value = 0;
    NiRef dataRef;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiFloatInterpolator"; }
};

// NiTimeController base
class NiTimeController : public NiObject {
public:
    NiRef nextControllerRef;
    uint16_t flags = 0;
    float frequency = 1.0f;
    float phase = 0;
    float startTime = 0;
    float stopTime = 0;
    NiRef targetRef;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTimeController"; }
};

// NiSingleInterpController
class NiSingleInterpController : public NiTimeController {
public:
    NiRef interpolatorRef;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiSingleInterpController"; }
};

// NiKeyframeController
class NiKeyframeController : public NiSingleInterpController {
public:
    const char* GetTypeName() const override { return "NiKeyframeController"; }
};

// NiTransformController
class NiTransformController : public NiKeyframeController {
public:
    const char* GetTypeName() const override { return "NiTransformController"; }
};

// NiAlphaController
class NiAlphaController : public NiSingleInterpController {
public:
    const char* GetTypeName() const override { return "NiAlphaController"; }
};

// NiProperty base class
class NiProperty : public NiObjectNET {
public:
    const char* GetTypeName() const override { return "NiProperty"; }
};

// TexDesc structure (from nif.xml)
struct TexDesc {
    NiRef sourceRef;
    uint32_t clampMode = 3;
    uint32_t filterMode = 2;
    uint32_t uvSet = 0;
    int16_t ps2L = 0;
    int16_t ps2K = -75;
    bool hasTextureTransform = false;
    NiTexCoord translation;
    NiTexCoord scale = {1.0f, 1.0f};
    float rotation = 0;
    uint32_t transformMethod = 0;
    NiTexCoord center;
};

// NiTexturingProperty (complex - from nif.xml)
class NiTexturingProperty : public NiProperty {
public:
    uint32_t applyMode = 2;
    uint32_t textureCount = 7;
    bool hasBaseTexture = false;
    TexDesc baseTexture;
    bool hasDarkTexture = false;
    TexDesc darkTexture;
    bool hasDetailTexture = false;
    TexDesc detailTexture;
    bool hasGlossTexture = false;
    TexDesc glossTexture;
    bool hasGlowTexture = false;
    TexDesc glowTexture;
    bool hasBumpMapTexture = false;
    TexDesc bumpMapTexture;
    float bumpMapLumaScale = 0;
    float bumpMapLumaOffset = 0;
    float bumpMapMatrix[4] = {0};
    uint32_t numShaderTextures = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTexturingProperty"; }
};

// NiZBufferProperty
class NiZBufferProperty : public NiProperty {
public:
    uint16_t flags = 3;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiZBufferProperty"; }
};

// NiVertexColorProperty
class NiVertexColorProperty : public NiProperty {
public:
    uint16_t flags = 0;
    uint32_t vertexMode = 0;
    uint32_t lightingMode = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiVertexColorProperty"; }
};

// NiMaterialProperty
class NiMaterialProperty : public NiProperty {
public:
    uint16_t flags = 0;
    NiColor4 ambientColor;
    NiColor4 diffuseColor;
    NiColor4 specularColor;
    NiColor4 emissiveColor;
    float glossiness = 0;
    float alpha = 1.0f;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiMaterialProperty"; }
};

// NiAlphaProperty
class NiAlphaProperty : public NiProperty {
public:
    uint16_t flags = 0;
    uint8_t threshold = 128;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiAlphaProperty"; }
};

// NiStencilProperty
class NiStencilProperty : public NiProperty {
public:
    uint16_t flags = 0;
    uint32_t stencilEnabled = 0;
    uint32_t stencilFunction = 0;
    uint32_t stencilRef = 0;
    uint32_t stencilMask = 0xFFFFFFFF;
    uint32_t failAction = 0;
    uint32_t zFailAction = 0;
    uint32_t passAction = 0;
    uint32_t drawMode = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiStencilProperty"; }
};

// NiSpecularProperty
class NiSpecularProperty : public NiProperty {
public:
    uint16_t flags = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiSpecularProperty"; }
};

// NiDynamicEffect - base for lights and effects
class NiDynamicEffect : public NiAVObject {
public:
    bool switchState = true;
    std::vector<NiRef> affectedNodes;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiDynamicEffect"; }
};

// NiTextureEffect
class NiTextureEffect : public NiDynamicEffect {
public:
    NiMatrix33 modelProjectionMatrix;
    NiVector3 modelProjectionTranslation;
    uint32_t textureFiltering = 0;
    uint32_t textureClamping = 0;
    uint32_t textureType = 0;
    uint32_t coordinateGenerationType = 0;
    NiRef sourceTextureRef;
    uint8_t enablePlane = 0;
    // ... additional fields if needed
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTextureEffect"; }
};

// NiLight
class NiLight : public NiDynamicEffect {
public:
    float dimmer = 1.0f;
    NiVector3 ambientColor;
    NiVector3 diffuseColor;
    NiVector3 specularColor;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiLight"; }
};

// NiDirectionalLight
class NiDirectionalLight : public NiLight {
public:
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiDirectionalLight"; }
};

// NiAmbientLight
class NiAmbientLight : public NiLight {
public:
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiAmbientLight"; }
};

// NiBillboardNode - always faces camera
class NiBillboardNode : public NiNode {
public:
    uint16_t billboardMode = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiBillboardNode"; }
};

// NiStringExtraData - string extra data
class NiStringExtraData : public NiExtraData {
public:
    std::string stringData;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiStringExtraData"; }
};

// NiPixelData - embedded pixel data (usually skip for external textures)
class NiPixelData : public NiObject {
public:
    // Complex structure, we'll skip most of it
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiPixelData"; }
};

// NiFlipController - flipbook animation
class NiFlipController : public NiSingleInterpController {
public:
    uint32_t textureSlot = 0;
    float delta = 0;
    uint32_t numSources = 0;
    std::vector<NiRef> sources;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiFlipController"; }
};

// NiTextureTransformController
class NiTextureTransformController : public NiSingleInterpController {
public:
    uint8_t shaderMap = 0;
    uint32_t textureSlot = 0;
    uint32_t operation = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiTextureTransformController"; }
};

// NiMaterialColorController
class NiMaterialColorController : public NiSingleInterpController {
public:
    uint16_t targetColor = 0;
    
    void Read(NiStream& stream) override;
    const char* GetTypeName() const override { return "NiMaterialColorController"; }
};

// Unknown block placeholder
class NiUnknown : public NiObject {
public:
    std::vector<uint8_t> data;
    
    void Read(NiStream& stream) override {}
    const char* GetTypeName() const override { return "NiUnknown"; }
};

//=============================================================================
// Main NIF file class
//=============================================================================

class NifFile10 {
public:
    bool Load(const std::string& path);
    bool LoadFromMemory(const uint8_t* data, size_t size);
    
    NiVersion GetVersion() const { return m_version; }
    
    // Access blocks
    size_t GetNumBlocks() const { return m_blocks.size(); }
    NiObject* GetBlock(size_t index);
    
    template<typename T>
    T* GetBlock(size_t index) {
        return dynamic_cast<T*>(GetBlock(index));
    }
    
    // Find blocks by type
    template<typename T>
    std::vector<T*> GetBlocksOfType() {
        std::vector<T*> result;
        for (auto& block : m_blocks) {
            if (T* p = dynamic_cast<T*>(block.get())) {
                result.push_back(p);
            }
        }
        return result;
    }
    
    NiNode* GetRootNode();
    
    const std::string& GetError() const { return m_error; }
    const std::vector<std::string>& GetBlockTypes() const { return m_blockTypes; }
    
private:
    bool ParseHeader(NiStream& stream);
    std::unique_ptr<NiObject> CreateBlock(const std::string& typeName);
    
    NiVersion m_version;
    std::vector<std::string> m_blockTypes;
    std::vector<uint16_t> m_blockTypeIndices;
    std::vector<uint32_t> m_blockSizes;
    std::vector<std::string> m_stringTable;
    std::vector<std::unique_ptr<NiObject>> m_blocks;
    std::string m_error;
public:
    // Get string from string table by index
    const std::string& GetString(uint32_t index) const {
        static std::string empty;
        if (index < m_stringTable.size()) return m_stringTable[index];
        return empty;
    }
};

} // namespace knc
