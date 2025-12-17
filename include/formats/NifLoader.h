#pragma once
// NifLoader.h - Custom NIF loader (Gamebryo 1.2 compatible)
// Based on nifly structure, adapted for exact game format

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace knc {

// Forward declarations
struct NifHeader;
struct NiObject;

// Basic types matching Gamebryo 1.2
struct NiPoint3 {
    float x, y, z;
};

struct NiPoint2 {
    float u, v;
};

struct NiMatrix3 {
    float m[3][3];
};

struct NiTransform {
    NiMatrix3 rotation;
    NiPoint3 translation;
    float scale;
};

struct NiColor {
    float r, g, b;
};

struct NiColorA {
    float r, g, b, a;
};

// Triangle indices
struct NiTriangle {
    uint16_t v1, v2, v3;
};

// Geometry data - matches NiGeometryData from Gamebryo 1.2
struct NifGeometryData {
    std::vector<NiPoint3> vertices;
    std::vector<NiPoint3> normals;
    std::vector<NiColorA> vertexColors;
    std::vector<NiPoint2> uvSets;      // UV coordinates
    std::vector<NiTriangle> triangles; // For NiTriShape
    std::vector<uint16_t> stripLengths; // For NiTriStrips
    std::vector<uint16_t> strips;       // Triangle strip indices
};

// Material/texture info
struct NifMaterial {
    std::string name;
    std::string texturePath;  // DDS/TGA path
    NiColorA ambient;
    NiColorA diffuse;
    NiColorA specular;
    NiColorA emissive;
    float glossiness;
    float alpha;
};

// Scene node - matches NiNode from Gamebryo 1.2
struct NifNode {
    std::string name;
    NiTransform localTransform;
    NiTransform worldTransform;
    
    // Geometry (if this is a NiTriShape/NiTriStrips)
    bool hasGeometry = false;
    NifGeometryData geometry;
    NifMaterial material;
    
    // Children
    std::vector<std::shared_ptr<NifNode>> children;
    
    // Parent (weak reference)
    NifNode* parent = nullptr;
};

// Main NIF file structure
struct NifFile {
    std::string path;
    uint32_t version;
    std::shared_ptr<NifNode> root;
    
    // All nodes flat (for easy iteration)
    std::vector<NifNode*> allNodes;
    
    // Textures referenced
    std::vector<std::string> textures;
};

// NIF Loader class
class NifLoader {
public:
    NifLoader() = default;
    
    // Load NIF from file
    std::shared_ptr<NifFile> Load(const std::string& path);
    
    // Load NIF from memory
    std::shared_ptr<NifFile> LoadFromMemory(const uint8_t* data, size_t size, const std::string& name = "");
    
    // Get last error
    const std::string& GetError() const { return m_error; }
    
private:
    // Parsing helpers
    bool ParseHeader(const uint8_t* data, size_t size, size_t& offset);
    std::shared_ptr<NiObject> ParseBlock(const uint8_t* data, size_t size, size_t& offset, int blockType);
    std::shared_ptr<NifNode> BuildSceneGraph();
    
    // Read helpers
    template<typename T> T Read(const uint8_t* data, size_t& offset);
    std::string ReadString(const uint8_t* data, size_t& offset);
    std::string ReadSizedString(const uint8_t* data, size_t& offset);
    
    std::string m_error;
    
    // Parsed data
    uint32_t m_version = 0;
    std::vector<std::string> m_blockTypes;
    std::vector<uint32_t> m_blockTypeIndex;
    std::vector<uint32_t> m_blockSizes;
    std::vector<std::string> m_strings;
};

} // namespace knc

