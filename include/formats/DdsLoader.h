#pragma once
// DdsLoader.h - Simple DDS texture loader for Raylib

#include "raylib.h"
#include <cstdint>
#include <vector>
#include <string>

namespace knc {

// DDS file header structures
#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

struct DDS_HEADER {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT pixelFormat;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};
#pragma pack(pop)

// Load DDS texture into Raylib format
Image LoadDDS(const std::string& path);

// Load DDS as Raylib texture
Texture2D LoadTextureDDS(const std::string& path);

} // namespace knc

