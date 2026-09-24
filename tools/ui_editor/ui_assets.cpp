#include "ui_assets.h"

#include <cstdint>
#include <cstdio>

#include "PakReader.h"

#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/error.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

namespace KnC::Tools {
namespace {

const char* const kLanguages[] = {"Eng", "Frn", "Ger", "Spn", "Public"};

bx::AllocatorI& allocator() {
    static bx::DefaultAllocator shared;
    return shared;
}

std::string lower_copy(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string forward_slashes(std::string text) {
    for (char& c : text) if (c == '\\') c = '/';
    return text;
}

bool is_image_name(const std::string& lower) {
    for (const char* ext : {".png", ".dds", ".jpg", ".jpeg", ".tga", ".bmp"})
        if (lower.size() > std::strlen(ext) && lower.compare(lower.size() - std::strlen(ext), std::strlen(ext), ext) == 0)
            return true;
    return false;
}

// Strips the leading dot slash and any language image prefix
std::string strip_known_prefix(const std::string& path) {
    std::string base = forward_slashes(path);
    if (base.rfind("./", 0) == 0) base = base.substr(2);
    const std::string lower = lower_copy(base);
    for (const char* language : kLanguages) {
        const std::string prefix = lower_copy(std::string("Data/") + language + "/Image/");
        if (lower.rfind(prefix, 0) == 0) return base.substr(prefix.size());
    }
    return base;
}

std::vector<std::string> name_variants(const std::string& base) {
    std::vector<std::string> names;
    names.push_back(base);
    if (!base.empty() && base.back() == '_') {
        for (const char* suffix : {"00.png", "01.png", "02.png", "03.png"}) names.push_back(base + suffix);
        names.push_back(base.substr(0, base.size() - 1) + ".png");
    }
    const size_t slash = base.find_last_of('/');
    const std::string file = slash == std::string::npos ? base : base.substr(slash + 1);
    if (file.find('.') == std::string::npos && !file.empty() && file.back() != '_') {
        names.push_back(base + ".png");
        names.push_back(base + ".dds");
    }
    const size_t png = base.rfind(".png");
    if (png != std::string::npos && png + 4 == base.size()) names.push_back(base.substr(0, png) + ".PNG");
    const size_t upper = base.rfind(".PNG");
    if (upper != std::string::npos && upper + 4 == base.size()) names.push_back(base.substr(0, upper) + ".png");
    return names;
}

std::vector<std::string> candidate_paths(const std::string& path) {
    std::vector<std::string> candidates;
    const std::string base = strip_known_prefix(path);
    const std::string language_dir = "Data/" + g_editor.language + "/Image/";
    for (const std::string& name : name_variants(base)) {
        candidates.push_back(name);
        candidates.push_back(language_dir + name);
        candidates.push_back("Data/Public/Image/" + name);
    }
    candidates.push_back(path);
    return candidates;
}

bool read_from_pak(const std::string& relative, std::vector<uint8_t>& out) {
    PakReader& pak = GetPakReader();
    if (pak.GetEntryCount() == 0) return false;
    out = pak.ReadPath(relative);
    return !out.empty();
}

bool read_from_disk(const std::string& relative, std::vector<uint8_t>& out) {
    if (g_editor.gamePath.empty()) return false;
    std::string rel = forward_slashes(relative);
    if (rel.rfind("./", 0) == 0) rel = rel.substr(2);
    const std::filesystem::path full = std::filesystem::path(g_editor.gamePath) / rel;
    std::error_code ignored;
    if (!std::filesystem::is_regular_file(full, ignored)) return false;
    std::ifstream file(full, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    const std::streamoff size = file.tellg();
    if (size <= 0) return false;
    out.resize(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(out.data()), size);
    return static_cast<bool>(file);
}

bool read_asset_bytes(const std::string& relative, std::vector<uint8_t>& out) {
    return read_from_pak(relative, out) || read_from_disk(relative, out);
}

// Last resort a file name search over every listed asset
bool read_by_search(const std::string& path, std::vector<uint8_t>& out) {
    std::string wanted = forward_slashes(path);
    const size_t slash = wanted.rfind('/');
    if (slash != std::string::npos) wanted = wanted.substr(slash + 1);
    const size_t dot = wanted.find('.');
    if (dot != std::string::npos) wanted = wanted.substr(0, dot);
    if (!wanted.empty() && wanted.back() == '_') wanted.pop_back();
    if (wanted.empty()) return false;
    const std::string lower_wanted = lower_copy(wanted);
    for (const std::string& asset : g_editor.availableAssets) {
        const std::string lower_asset = lower_copy(asset);
        const size_t file_slash = lower_asset.find_last_of('/');
        const std::string file = file_slash == std::string::npos ? lower_asset : lower_asset.substr(file_slash + 1);
        if (file.find(lower_wanted) == std::string::npos) continue;
        if (read_asset_bytes(asset, out)) {
            std::printf("[ASSET] %s found by search as %s\n", path.c_str(), asset.c_str());
            return true;
        }
    }
    return false;
}

bool decode_rgba8(const std::vector<uint8_t>& bytes, std::vector<uint8_t>& pixels, int& width, int& height) {
    bx::Error error;
    bimg::ImageContainer* image = bimg::imageParse(&allocator(), bytes.data(), static_cast<uint32_t>(bytes.size()),
                                                   bimg::TextureFormat::RGBA8, &error);
    if (image == nullptr) return false;
    bimg::ImageMip mip;
    const bool got = bimg::imageGetRawData(*image, 0, 0, image->m_data, image->m_size, mip);
    if (got && mip.m_format == bimg::TextureFormat::RGBA8) {
        width = static_cast<int>(mip.m_width);
        height = static_cast<int>(mip.m_height);
        const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        if (size <= mip.m_size) pixels.assign(mip.m_data, mip.m_data + size);
    }
    bimg::imageFree(image);
    return !pixels.empty();
}

const LoadedTexture* remember(const std::string& path, std::unique_ptr<LoadedTexture> texture) {
    const LoadedTexture* raw = texture.get();
    g_editor.textureCache[path] = std::move(texture);
    return raw != nullptr && raw->valid() ? raw : nullptr;
}

}

bool open_game_folder(const std::string& game_path) {
    PakReader& pak = GetPakReader();
    pak.Close();
    bool opened = pak.OpenGameDir(game_path);
    if (!opened) opened = pak.Open(game_path + "/pak001.dat");
    if (opened) std::printf("[ASSET] pak entries %zu\n", pak.GetEntryCount());
    else std::printf("[ASSET] no pak under %s the Data folder alone is used\n", game_path.c_str());
    scan_available_assets();
    return opened || !g_editor.availableAssets.empty();
}

void scan_available_assets() {
    g_editor.availableAssets.clear();
    std::set<std::string> seen;
    auto add = [&](const std::string& path) {
        std::string key = lower_copy(forward_slashes(path));
        if (key.rfind("./", 0) == 0) key = key.substr(2);
        if (seen.insert(key).second) g_editor.availableAssets.push_back(path);
    };

    for (const PakEntry& entry : GetPakReader().GetEntries()) {
        const std::string lower = lower_copy(entry.path);
        if (lower.find("image/") != std::string::npos && is_image_name(lower)) add(entry.path);
    }

    if (!g_editor.gamePath.empty()) {
        std::error_code ignored;
        for (const char* language : kLanguages) {
            const std::filesystem::path root = std::filesystem::path(g_editor.gamePath) / "Data" / language / "Image";
            if (!std::filesystem::is_directory(root, ignored)) continue;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ignored)) {
                if (!entry.is_regular_file(ignored)) continue;
                const std::string relative = forward_slashes(std::filesystem::relative(entry.path(), root, ignored).string());
                if (!is_image_name(lower_copy(relative))) continue;
                add(std::string("./Data/") + language + "/Image/" + relative);
            }
        }
    }

    std::sort(g_editor.availableAssets.begin(), g_editor.availableAssets.end(),
              [](const std::string& a, const std::string& b) { return lower_copy(a) < lower_copy(b); });
    std::printf("[ASSET] %zu image assets listed\n", g_editor.availableAssets.size());
}

const LoadedTexture* load_texture(const std::string& path) {
    if (path.empty()) return nullptr;
    const auto cached = g_editor.textureCache.find(path);
    if (cached != g_editor.textureCache.end()) return cached->second->valid() ? cached->second.get() : nullptr;

    auto texture = std::make_unique<LoadedTexture>();
    std::vector<uint8_t> bytes;
    bool found = false;
    for (const std::string& candidate : candidate_paths(path)) {
        if (read_asset_bytes(candidate, bytes)) {
            found = true;
            break;
        }
    }
    if (!found) found = read_by_search(path, bytes);
    if (!found) {
        std::printf("[ASSET] not found %s\n", path.c_str());
        return remember(path, std::move(texture));
    }

    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    if (!decode_rgba8(bytes, pixels, width, height) || width <= 0 || height <= 0) {
        std::printf("[ASSET] cannot decode %s\n", path.c_str());
        return remember(path, std::move(texture));
    }

    texture->handle = bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
                                            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                                            bgfx::copy(pixels.data(), static_cast<uint32_t>(pixels.size())));
    texture->width = width;
    texture->height = height;
    if (texture->valid()) std::printf("[ASSET] loaded %s (%dx%d)\n", path.c_str(), width, height);
    else std::printf("[ASSET] texture upload failed %s\n", path.c_str());
    return remember(path, std::move(texture));
}

void resolve_element_textures(std::vector<UIElement>& elements) {
    for (UIElement& elem : elements) {
        elem.texture = load_texture(elem.assetPath);
        elem.hoverTexture = load_texture(elem.hoverAsset);
        elem.pressedTexture = load_texture(elem.pressedAsset);
        elem.disabledTexture = load_texture(elem.disabledAsset);
    }
}

std::string short_asset_path(const std::string& path) { return strip_known_prefix(path); }

void destroy_all_textures() {
    for (auto& entry : g_editor.textureCache)
        if (entry.second->valid()) bgfx::destroy(entry.second->handle);
    g_editor.textureCache.clear();
}

}
