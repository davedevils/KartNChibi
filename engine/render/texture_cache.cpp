#include "engine/render/texture_cache.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/error.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace KnC::Render {

namespace {

double ms_since(std::chrono::steady_clock::time_point at) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - at).count();
}

TextureLoadStats& stats() {
    static TextureLoadStats shared;
    return shared;
}

bx::AllocatorI& allocator() {
    static bx::DefaultAllocator shared;
    return shared;
}

bool read_whole_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    const std::streamoff size = file.tellg();
    if (size <= 0) return false;
    out.resize(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(out.data()), size);
    return static_cast<bool>(file);
}

// dds by extension or by the four byte magic dds files start with
bool looks_like_dds(const std::string& path, const std::vector<uint8_t>& bytes) {
    if (path.size() >= 4) {
        std::string suffix = path.substr(path.size() - 4);
        for (char& letter : suffix) letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        if (suffix == ".dds") return true;
    }
    return bytes.size() >= 4 && bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S' && bytes[3] == ' ';
}

// mip 0 artist authored rest generated share convention
bool decode_top_level(const bimg::ImageContainer& image, std::vector<uint8_t>& out) {
    bimg::ImageMip mip;
    if (!bimg::imageGetRawData(image, 0, 0, image.m_data, image.m_size, mip)) return false;
    out.resize(static_cast<size_t>(mip.m_width) * mip.m_height * 4);
    bimg::imageDecodeToRgba8(&allocator(), out.data(), mip.m_data, mip.m_width, mip.m_height,
                             mip.m_width * 4, mip.m_format);
    return true;
}

// A texel below this alpha hides a texture with one is reported translucent
constexpr int kOpaqueFloor = 250;

bool carries_translucency(const std::vector<uint8_t>& rgba8) {
    for (size_t texel = 0; texel + 3 < rgba8.size(); texel += 4)
        if (rgba8[texel + 3] < kOpaqueFloor) return true;
    return false;
}

// mip chain copied out decoded image freed a sheet with no mip filter keeps its top level alone
bgfx::TextureHandle create_texture(const bimg::ImageContainer& image, bool mipmapped) {
    const bool chain = mipmapped && image.m_numMips > 1;
    if (!chain) {
        bimg::ImageMip top;
        if (bimg::imageGetRawData(image, 0, 0, image.m_data, image.m_size, top))
            return bgfx::createTexture2D(
                static_cast<uint16_t>(top.m_width), static_cast<uint16_t>(top.m_height), false, 1,
                static_cast<bgfx::TextureFormat::Enum>(image.m_format), BGFX_TEXTURE_NONE,
                bgfx::copy(top.m_data, top.m_size));
    }
    return bgfx::createTexture2D(
        static_cast<uint16_t>(image.m_width), static_cast<uint16_t>(image.m_height),
        chain, image.m_numLayers,
        static_cast<bgfx::TextureFormat::Enum>(image.m_format), BGFX_TEXTURE_NONE,
        bgfx::copy(image.m_data, image.m_size));
}

CachedTexture uploaded(const bimg::ImageContainer& image, bgfx::TextureHandle handle,
                       bool translucent) {
    return {handle, static_cast<uint16_t>(image.m_width), static_cast<uint16_t>(image.m_height),
            translucent};
}

// One file read parsed and inspected off the device the GPU texture is made from it later
struct PreparedImage {
    bimg::ImageContainer* image = nullptr;
    std::string source;
    bool translucent = false;
};

std::mutex& stats_lock() {
    static std::mutex shared;
    return shared;
}

void add_stat(double TextureLoadStats::*field, double ms) {
    std::lock_guard<std::mutex> lock(stats_lock());
    stats().*field += ms;
}

TextureBytesSource& bytes_source() {
    static TextureBytesSource source;
    return source;
}

std::string lowered(std::string text) {
    for (char& letter : text) letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    return text;
}

bool is_image_extension(const std::string& suffix) {
    return suffix == ".dds" || suffix == ".png" || suffix == ".tga" || suffix == ".bmp" ||
           suffix == ".jpg" || suffix == ".jpeg";
}

// No folder or pak match tries beside it wheel dust build names GRASS dds ships only GRASS screen png
std::string beside_by_stem(const std::string& path) {
    std::error_code ignored;
    const std::filesystem::path wanted(path);
    const std::filesystem::path folder = wanted.parent_path();
    if (folder.empty() || !std::filesystem::exists(folder, ignored)) return std::string();
    const std::string stem = lowered(wanted.stem().string());
    const std::string screen = stem + "_screen";
    for (const auto& entry : std::filesystem::directory_iterator(folder, ignored)) {
        if (!entry.is_regular_file(ignored)) continue;
        if (!is_image_extension(lowered(entry.path().extension().string()))) continue;
        const std::string found = lowered(entry.path().stem().string());
        if (found == stem || found == screen) return entry.path().string();
    }
    return std::string();
}

// Disk then pak then same stem beside it then parse and alpha read safe on any thread no device touch
bool prepare_image(const std::string& path, PreparedImage& out) {
    {
        std::lock_guard<std::mutex> lock(stats_lock());
        ++stats().count;
    }
    auto at = std::chrono::steady_clock::now();
    std::vector<uint8_t> bytes;
    out.source = path;
    if (!read_whole_file(path, bytes) &&
        (!bytes_source() || !bytes_source()(path, bytes) || bytes.empty())) {
        const std::string beside = beside_by_stem(path);
        if (beside.empty() || !read_whole_file(beside, bytes)) {
            std::cerr << "[render] texture missing: " << path << "\n";
            return false;
        }
        std::cout << "[render] texture " << path << " drawn from " << beside << "\n";
        out.source = beside;
    }
    add_stat(&TextureLoadStats::read_ms, ms_since(at));
    at = std::chrono::steady_clock::now();
    bx::Error error;
    // dds keeps its dedicated reader everything else goes through the bimg decoders
    out.image = looks_like_dds(out.source, bytes)
                    ? bimg::imageParseDds(&allocator(), bytes.data(), static_cast<uint32_t>(bytes.size()), &error)
                    : bimg::imageParse(&allocator(), bytes.data(), static_cast<uint32_t>(bytes.size()),
                                       bimg::TextureFormat::Count, &error);
    add_stat(&TextureLoadStats::parse_ms, ms_since(at));
    if (out.image == nullptr) {
        std::cerr << "[render] texture is not a decodable image: " << out.source << "\n";
        return false;
    }
    at = std::chrono::steady_clock::now();
    std::vector<uint8_t> top_level;
    const bool inspected = decode_top_level(*out.image, top_level);
    out.translucent = inspected && carries_translucency(top_level);
    add_stat(&TextureLoadStats::inspect_ms, ms_since(at));
    if (!inspected) std::cerr << "[render] alpha of " << out.source << " could not be inspected\n";
    return true;
}

// The file goes to the device as authored the fragment hands the device straight alpha
CachedTexture upload_prepared(PreparedImage& prepared, bool mipmapped) {
    const auto at = std::chrono::steady_clock::now();
    const bgfx::TextureHandle handle = create_texture(*prepared.image, mipmapped);
    add_stat(&TextureLoadStats::create_ms, ms_since(at));
    const CachedTexture loaded = uploaded(*prepared.image, handle, prepared.translucent);
    bimg::imageFree(prepared.image);
    prepared.image = nullptr;
    return loaded;
}

CachedTexture read_texture(const std::string& path, bool mipmapped) {
    PreparedImage prepared;
    if (!prepare_image(path, prepared)) return {};
    return upload_prepared(prepared, mipmapped);
}

// The images a loader thread prepared by path the device side takes each one once
struct PreparedStore {
    std::mutex lock;
    std::unordered_map<std::string, PreparedImage> images;
};

PreparedStore& prepared_store() {
    static PreparedStore shared;
    return shared;
}

bool take_prepared(const std::string& path, PreparedImage& out) {
    PreparedStore& store = prepared_store();
    std::lock_guard<std::mutex> lock(store.lock);
    const auto found = store.images.find(path);
    if (found == store.images.end()) return false;
    out = found->second;
    store.images.erase(found);
    return true;
}

} // namespace

void set_texture_bytes_source(TextureBytesSource source) { bytes_source() = std::move(source); }

TextureLoadStats take_texture_load_stats() {
    std::lock_guard<std::mutex> lock(stats_lock());
    const TextureLoadStats out = stats();
    stats() = TextureLoadStats{};
    return out;
}

size_t prefetch_textures(const std::vector<std::string>& paths, unsigned threads, const std::atomic<bool>* stop,
                         bool background) {
    std::vector<std::string> wanted;
    {
        PreparedStore& store = prepared_store();
        std::lock_guard<std::mutex> lock(store.lock);
        for (const std::string& path : paths) {
            if (path.empty() || store.images.count(path) != 0) continue;
            if (std::find(wanted.begin(), wanted.end(), path) != wanted.end()) continue;
            wanted.push_back(path);
        }
    }
    if (wanted.empty()) return 0;
    std::atomic<size_t> next{0};
    std::atomic<size_t> ready{0};
    auto work = [&]() {
        for (size_t at = next++; at < wanted.size(); at = next++) {
            if (stop != nullptr && stop->load()) return;
            PreparedImage prepared;
            if (!prepare_image(wanted[at], prepared)) continue;
            PreparedStore& store = prepared_store();
            std::lock_guard<std::mutex> lock(store.lock);
            auto& slot = store.images[wanted[at]];
            if (slot.image != nullptr) bimg::imageFree(slot.image);
            slot = prepared;
            ++ready;
        }
    };
    const unsigned count = std::max(1u, std::min<unsigned>(threads, static_cast<unsigned>(wanted.size())));
    std::vector<std::thread> helpers;
    // background helpers yield to the frame loop the caller keeps its own priority
    for (unsigned i = 1; i < count; ++i)
        helpers.emplace_back([&work, background]() {
            if (background) lower_current_thread_priority();
            work();
        });
    work();
    for (std::thread& helper : helpers) helper.join();
    return ready.load();
}

void lower_current_thread_priority() {
#if defined(_WIN32)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

void drop_prefetched_textures() {
    PreparedStore& store = prepared_store();
    std::lock_guard<std::mutex> lock(store.lock);
    for (auto& entry : store.images)
        if (entry.second.image != nullptr) bimg::imageFree(entry.second.image);
    store.images.clear();
}

TextureCache::~TextureCache() { destroy_all(); }

bool TextureCache::create_white_stand_in() {
    const uint32_t opaque_white = 0xffffffffu;
    white_ = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE,
                                   bgfx::copy(&opaque_white, sizeof(opaque_white)));
    if (bgfx::isValid(white_)) return true;
    std::cerr << "[render] could not create the white stand-in texture\n";
    return false;
}

CachedTexture TextureCache::acquire(const std::string& path, bool mipmapped) {
    // A surface that names no texture is untextured not missing the white stand in carries it
    if (path.empty()) return {};
    // One entry per pair so a sheet asked for flat and another asked for mipped never share
    const std::string key = mipmapped ? path : path + "?flat";
    const auto cached = textures_.find(key);
    if (cached != textures_.end()) return cached->second;

    // A loader thread may have read and parsed it already then only the device upload is left
    PreparedImage prepared;
    const CachedTexture loaded = take_prepared(path, prepared) ? upload_prepared(prepared, mipmapped)
                                                               : read_texture(path, mipmapped);
    if (!bgfx::isValid(loaded.handle)) ++missing_;
    textures_.emplace(key, loaded);
    return loaded;
}

// alpha mask clamped no mip chain authored size not blurred
CachedTexture TextureCache::acquire_alpha(const std::string& key, int width, int height,
                                          const std::vector<uint8_t>& alpha) {
    const auto cached = textures_.find(key);
    if (cached != textures_.end()) return cached->second;
    CachedTexture built;
    if (width <= 0 || height <= 0 ||
        alpha.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
        std::cerr << "[render] " << key << ": " << alpha.size() << " coverage byte(s) do not"
                  << " fill " << width << "x" << height << "\n";
        ++missing_;
        textures_.emplace(key, built);
        return built;
    }
    built.width = static_cast<uint16_t>(width);
    built.height = static_cast<uint16_t>(height);
    built.translucent = true;
    built.handle = bgfx::createTexture2D(
        built.width, built.height, false, 1, bgfx::TextureFormat::A8, BGFX_TEXTURE_NONE,
        bgfx::copy(alpha.data(), static_cast<uint32_t>(alpha.size())));
    textures_.emplace(key, built);
    return built;
}

void TextureCache::destroy_all() {
    for (const auto& entry : textures_)
        if (bgfx::isValid(entry.second.handle)) bgfx::destroy(entry.second.handle);
    textures_.clear();
    if (bgfx::isValid(white_)) bgfx::destroy(white_);
    white_ = BGFX_INVALID_HANDLE;
}

}
