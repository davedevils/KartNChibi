#include "NifEmbeddedTextures.h"

#include "engine/formats/nif_reader.h"
#include "engine/render/map_scene.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace KnC::Client {

namespace {

namespace fs = std::filesystem;

std::string lowerCopy(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

uint32_t u32At(const std::vector<uint8_t>& d, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, d.data() + at, 4);
    return v;
}

void putU32(std::vector<uint8_t>& d, size_t at, uint32_t v) { std::memcpy(d.data() + at, &v, 4); }

// the NiPixelFormat values of the 10 2 0 0 files 0 rgb 1 rgba 4 dxt1 5 dxt3 6 dxt5
constexpr uint32_t kFmtRgb = 0;
constexpr uint32_t kFmtRgba = 1;
constexpr uint32_t kFmtDxt1 = 4;
constexpr uint32_t kFmtDxt3 = 5;
constexpr uint32_t kFmtDxt5 = 6;
// the masks based pixel format ends at 10 3 0 3 the room nifs are 10 2 0 0
constexpr uint32_t kMaskFormatUntil = 0x0A030003;
constexpr uint32_t kMaxMips = 16;
constexpr uint32_t kNoLink = 0xFFFFFFFFu;

// one NiPixelData body found in the file bytes
struct PixelBody {
    uint32_t format = 0;
    uint32_t masks[4] = {0, 0, 0, 0};
    uint32_t bytesPerPixel = 0;
    uint32_t mips = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    size_t dataAt = 0;
    uint32_t dataBytes = 0;
    uint32_t level0Bytes = 0;
};

uint32_t levelBytes(uint32_t format, uint32_t w, uint32_t h, uint32_t bytesPerPixel) {
    const uint32_t bw = (w + 3) / 4;
    const uint32_t bh = (h + 3) / 4;
    if (format == kFmtDxt1) return bw * bh * 8;
    if (format == kFmtDxt3 || format == kFmtDxt5) return bw * bh * 16;
    return w * h * bytesPerPixel;
}

// the legacy body is format masks bits fast compare tiling palette mip count stride mip table and byte count
bool bodyAt(const std::vector<uint8_t>& d, size_t i, PixelBody& out) {
    if (i + 52 > d.size()) return false;
    const uint32_t format = u32At(d, i);
    if (format != kFmtRgb && format != kFmtRgba && format != kFmtDxt1 && format != kFmtDxt3 && format != kFmtDxt5) return false;
    if (u32At(d, i + 36) != kNoLink) return false;
    const uint32_t mips = u32At(d, i + 40);
    if (mips < 1 || mips > kMaxMips) return false;
    const uint32_t bytesPerPixel = u32At(d, i + 44);
    if ((format == kFmtRgb || format == kFmtRgba) && bytesPerPixel != 3 && bytesPerPixel != 4) return false;
    const size_t table = i + 48;
    if (table + 12 * static_cast<size_t>(mips) + 4 > d.size()) return false;
    uint32_t total = 0;
    uint32_t w0 = 0, h0 = 0, l0 = 0;
    for (uint32_t m = 0; m < mips; ++m) {
        const uint32_t w = u32At(d, table + 12 * m);
        const uint32_t h = u32At(d, table + 12 * m + 4);
        const uint32_t off = u32At(d, table + 12 * m + 8);
        if (w == 0 || h == 0 || w > 4096 || h > 4096 || off != total) return false;
        const uint32_t bytes = levelBytes(format, w, h, bytesPerPixel);
        if (m == 0) { w0 = w; h0 = h; l0 = bytes; }
        total += bytes;
    }
    const size_t countAt = table + 12 * static_cast<size_t>(mips);
    if (u32At(d, countAt) != total || total == 0) return false;
    if (countAt + 4 + total > d.size()) return false;
    out.format = format;
    for (int k = 0; k < 4; ++k) out.masks[k] = u32At(d, i + 4 + 4 * k);
    out.bytesPerPixel = bytesPerPixel;
    out.mips = mips;
    out.width = w0;
    out.height = h0;
    out.dataAt = countAt + 4;
    out.dataBytes = total;
    out.level0Bytes = l0;
    return true;
}

bool writeDds(const fs::path& path, const std::vector<uint8_t>& d, const PixelBody& body) {
    const bool compressed = body.format == kFmtDxt1 || body.format == kFmtDxt3 || body.format == kFmtDxt5;
    std::vector<uint8_t> head(128, 0);
    putU32(head, 0, 0x20534444u);
    putU32(head, 4, 124);
    uint32_t flags = 0x1u | 0x2u | 0x4u | 0x1000u;
    if (body.mips > 1) flags |= 0x20000u;
    flags |= compressed ? 0x80000u : 0x8u;
    putU32(head, 8, flags);
    putU32(head, 12, body.height);
    putU32(head, 16, body.width);
    putU32(head, 20, compressed ? body.level0Bytes : body.width * body.bytesPerPixel);
    putU32(head, 28, body.mips);
    putU32(head, 76, 32);
    if (compressed) {
        putU32(head, 80, 0x4u);
        const char* fourcc = body.format == kFmtDxt1 ? "DXT1" : body.format == kFmtDxt3 ? "DXT3" : "DXT5";
        std::memcpy(head.data() + 84, fourcc, 4);
    } else {
        const bool alpha = body.bytesPerPixel == 4;
        putU32(head, 80, 0x40u | (alpha ? 0x1u : 0u));
        putU32(head, 88, body.bytesPerPixel * 8);
        // the file masks when the exporter wrote them else the byte order red first
        const bool masked = body.masks[0] || body.masks[1] || body.masks[2];
        putU32(head, 92, masked ? body.masks[0] : 0x000000FFu);
        putU32(head, 96, masked ? body.masks[1] : 0x0000FF00u);
        putU32(head, 100, masked ? body.masks[2] : 0x00FF0000u);
        putU32(head, 104, alpha ? (masked ? body.masks[3] : 0xFF000000u) : 0u);
    }
    uint32_t caps = 0x1000u;
    if (body.mips > 1) caps |= 0x400000u | 0x8u;
    putU32(head, 108, caps);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(head.data()), static_cast<std::streamsize>(head.size()));
    out.write(reinterpret_cast<const char*>(d.data() + body.dataAt), static_cast<std::streamsize>(body.dataBytes));
    return static_cast<bool>(out);
}

bool namesTexture(const std::string& lower) {
    for (const char* ext : {".dds", ".tga", ".bmp", ".png", ".jpg"}) {
        const size_t n = std::strlen(ext);
        if (lower.size() > n && lower.compare(lower.size() - n, n, ext) == 0) return true;
    }
    return false;
}

// an internal NiSourceTexture is the external byte 0 the sized name then the link of its pixel block
void findTextureLinks(const std::vector<uint8_t>& d, const std::vector<uint32_t>& pixelBlocks,
                      std::map<std::string, size_t>& nameToBody) {
    for (size_t i = 5; i + 4 < d.size(); ++i) {
        if (d[i - 5] != 0) continue;
        const uint32_t len = u32At(d, i - 4);
        if (len < 5 || len > 200 || i + len + 4 > d.size()) continue;
        bool printable = true;
        for (uint32_t k = 0; k < len && printable; ++k) printable = d[i + k] >= 0x20 && d[i + k] < 0x7F;
        if (!printable) continue;
        const std::string name = lowerCopy(std::string(reinterpret_cast<const char*>(d.data() + i), len));
        if (!namesTexture(name)) continue;
        const uint32_t link = u32At(d, i + len);
        for (size_t k = 0; k < pixelBlocks.size(); ++k) {
            if (pixelBlocks[k] == link) { nameToBody[name] = k; break; }
        }
    }
}

void applyPath(const std::map<std::string, std::string>& embedded, std::string& path) {
    if (path.empty() || embedded.empty()) return;
    std::error_code ignored;
    if (fs::exists(path, ignored)) return;
    const auto it = embedded.find(lowerCopy(fs::path(path).filename().string()));
    if (it != embedded.end()) path = it->second;
}

}

std::map<std::string, std::string> extractEmbeddedTextures(const std::string& nifPath, const std::string& cacheDir) {
    std::map<std::string, std::string> out;
    KnC::NifHeader header;
    if (!KnC::read_nif_header(nifPath, header)) return out;
    if (header.version >= kMaskFormatUntil) return out;
    std::vector<uint32_t> pixelBlocks;
    for (size_t i = 0; i < header.object_types.size(); ++i) {
        const uint16_t t = header.object_types[i];
        if (t < header.type_names.size() && header.type_names[t] == "NiPixelData") pixelBlocks.push_back(static_cast<uint32_t>(i));
    }
    if (pixelBlocks.empty()) return out;
    std::ifstream in(nifPath, std::ios::binary);
    if (!in) return out;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<PixelBody> bodies;
    for (size_t i = 0; i + 52 <= d.size() && bodies.size() < pixelBlocks.size();) {
        PixelBody body;
        if (bodyAt(d, i, body)) { bodies.push_back(body); i = body.dataAt + body.dataBytes; continue; }
        ++i;
    }
    std::map<std::string, size_t> nameToBody;
    findTextureLinks(d, pixelBlocks, nameToBody);
    std::error_code ignored;
    fs::create_directories(cacheDir, ignored);
    for (const auto& [name, k] : nameToBody) {
        if (k >= bodies.size()) continue;
        const PixelBody& body = bodies[k];
        const fs::path path = fs::path(cacheDir) / (fs::path(name).stem().string() + ".dds");
        const uintmax_t wanted = 128 + body.dataBytes;
        if (!fs::exists(path, ignored) || fs::file_size(path, ignored) != wanted) {
            if (!writeDds(path, d, body)) { std::printf("[nif] could not write %s\n", path.string().c_str()); continue; }
        }
        out[name] = path.string();
    }
    std::printf("[nif] %s embeds %zu textures %zu bodies %zu written\n", nifPath.c_str(), nameToBody.size(), bodies.size(), out.size());
    return out;
}

void applyEmbeddedTextures(const std::map<std::string, std::string>& embedded, KnC::Render::PropModel& model) {
    for (KnC::Render::PropPart& part : model.parts) applyPath(embedded, part.texture_path);
    for (KnC::Render::ParticleSystemDefinition& system : model.particle_systems) applyPath(embedded, system.texture_path);
}

void applyEmbeddedTextures(const std::map<std::string, std::string>& embedded, KnC::Render::MapScene& scene) {
    for (KnC::Render::PropModel& model : scene.prop_models) applyEmbeddedTextures(embedded, model);
    for (KnC::Render::SkyPhase& phase : scene.sky_phases) applyEmbeddedTextures(embedded, phase.model);
    auto coats = [&](KnC::Render::LayerMesh& mesh) {
        for (KnC::Render::SplatCoat& coat : mesh.coats) { applyPath(embedded, coat.diffuse_path); applyPath(embedded, coat.mask_path); }
    };
    for (KnC::Render::GroundPatch& patch : scene.ground_patches) coats(patch.mesh);
    for (KnC::Render::LayerMesh& layer : scene.layers) coats(layer);
}

}
