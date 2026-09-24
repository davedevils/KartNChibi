#include "AssetStore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "PakReader.h"

#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/error.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>

namespace KnC::Client {

namespace {

const char* const kLanguages[] = {"Eng", "Frn", "Ger", "Spn", "Public"};
// the first line of an index cache the format number bumps when the columns change
const char* const kIndexHeader = "knc pak index 1";

bx::AllocatorI& allocator() {
    static bx::DefaultAllocator shared;
    return shared;
}

std::string lowerCopy(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string forwardSlashes(std::string text) {
    for (char& c : text) if (c == '\\') c = '/';
    return text;
}

// the pak spells its paths with a leading dot slash the lookups drop it and the case
std::string pakKey(const std::string& path) {
    std::string key = lowerCopy(forwardSlashes(path));
    if (key.rfind("./", 0) == 0) key = key.substr(2);
    return key;
}

bool isImageName(const std::string& lower) {
    for (const char* ext : {".png", ".dds", ".jpg", ".jpeg", ".tga", ".bmp"}) {
        const size_t n = std::strlen(ext);
        if (lower.size() > n && lower.compare(lower.size() - n, n, ext) == 0) return true;
    }
    return false;
}

// strips the leading dot slash and any language image prefix
std::string stripKnownPrefix(const std::string& path) {
    std::string base = forwardSlashes(path);
    if (base.rfind("./", 0) == 0) base = base.substr(2);
    const std::string lower = lowerCopy(base);
    for (const char* language : kLanguages) {
        const std::string prefix = lowerCopy(std::string("Data/") + language + "/Image/");
        if (lower.rfind(prefix, 0) == 0) return base.substr(prefix.size());
    }
    return base;
}

// the button convention a trailing underscore names the four state files
std::vector<std::string> nameVariants(const std::string& base) {
    std::vector<std::string> names;
    names.push_back(base);
    if (!base.empty() && base.back() == '_') {
        for (const char* suffix : {"00.png", "01.png", "02.png", "03.png", "00.PNG", "01.PNG"}) names.push_back(base + suffix);
        names.push_back(base.substr(0, base.size() - 1) + ".png");
    }
    const size_t slash = base.find_last_of('/');
    const std::string file = slash == std::string::npos ? base : base.substr(slash + 1);
    if (file.find('.') == std::string::npos && !file.empty() && file.back() != '_') {
        names.push_back(base + ".png");
        names.push_back(base + ".PNG");
        names.push_back(base + ".dds");
    }
    const size_t png = base.rfind(".png");
    if (png != std::string::npos && png + 4 == base.size()) names.push_back(base.substr(0, png) + ".PNG");
    const size_t upper = base.rfind(".PNG");
    if (upper != std::string::npos && upper + 4 == base.size()) names.push_back(base.substr(0, upper) + ".png");
    return names;
}

bool decodeRgba8(const std::vector<uint8_t>& bytes, std::vector<uint8_t>& pixels, int& width, int& height) {
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

double nowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}

AssetStore::AssetStore() {}

AssetStore::~AssetStore() {
    waitPak();
    destroyAll();
    if (m_pakFile) std::fclose(m_pakFile);
}

std::string AssetStore::cachePath() const {
    std::error_code ignored;
    const std::filesystem::path pak(m_pakPath);
    const auto size = std::filesystem::file_size(pak, ignored);
    const auto stamp = std::filesystem::last_write_time(pak, ignored).time_since_epoch().count();
    const std::filesystem::path dir = std::filesystem::temp_directory_path(ignored) / "knc_client" / "pak_index";
    std::filesystem::create_directories(dir, ignored);
    const std::string name = pak.stem().string() + "_" + std::to_string(size) + "_" + std::to_string(stamp) + ".txt";
    return (dir / name).string();
}

bool AssetStore::open(const std::string& gameDir, const std::string& language) {
    m_gameDir = forwardSlashes(gameDir);
    while (!m_gameDir.empty() && m_gameDir.back() == '/') m_gameDir.pop_back();
    if (!language.empty()) m_language = language;
    waitPak();
    if (m_pakFile) { std::fclose(m_pakFile); m_pakFile = nullptr; }
    m_items.clear();
    m_byPath.clear();
    m_imageIndex.clear();
    m_imageIndexed = false;
    m_pakPath = m_gameDir + "/pak001.dat";
    std::error_code ignored;
    if (!std::filesystem::is_regular_file(m_pakPath, ignored)) {
        std::printf("[assets] no pak under %s the Data folder alone is used\n", m_gameDir.c_str());
        return std::filesystem::is_directory(std::filesystem::path(m_gameDir) / "Data", ignored);
    }
    m_pakFile = std::fopen(m_pakPath.c_str(), "rb");
    if (!m_pakFile) {
        std::printf("[assets] cannot open %s the Data folder alone is used\n", m_pakPath.c_str());
        return true;
    }
    const std::string cache = cachePath();
    const double start = nowMs();
    if (loadIndex(cache)) {
        std::printf("[assets] pak index %zu entries from %s in %.0f ms\n", m_items.size(), cache.c_str(), nowMs() - start);
        return true;
    }
    // the first start reads the whole pak once the logo and the login come off the disk meanwhile
    std::printf("[assets] no pak index yet the pak is scanned on a thread and cached at %s\n", cache.c_str());
    m_scanning.store(true);
    m_scan = std::thread([this]() { scanPak(); });
    return true;
}

// items land in the table only on the next wait call not right after the scan
void AssetStore::scanPak() {
    const double start = nowMs();
    KnC::PakReader reader;
    std::vector<PakItem> items;
    if (reader.Open(m_pakPath)) {
        items.reserve(reader.GetEntryCount());
        for (const KnC::PakEntry& entry : reader.GetEntries()) {
            PakItem item;
            item.path = entry.path;
            item.offset = entry.offset;
            item.size = entry.size;
            items.push_back(std::move(item));
        }
    }
    std::printf("[assets] pak scan %zu entries in %.0f ms\n", items.size(), nowMs() - start);
    m_scanned = std::move(items);
    m_scanning.store(false);
}

void AssetStore::waitPak() const {
    std::lock_guard<std::mutex> lock(m_waitLock);
    if (!m_scan.joinable()) return;
    m_scan.join();
    adoptItems(std::move(m_scanned));
    m_scanned.clear();
    if (!m_items.empty()) {
        const std::string cache = cachePath();
        if (saveIndex(cache)) std::printf("[assets] pak index written %s\n", cache.c_str());
    }
}

void AssetStore::adoptItems(std::vector<PakItem> items) const {
    m_items = std::move(items);
    m_byPath.clear();
    m_byPath.reserve(m_items.size() * 2);
    m_byName.clear();
    m_byName.reserve(m_items.size() * 2);
    for (size_t i = 0; i < m_items.size(); ++i) {
        const std::string key = pakKey(m_items[i].path);
        m_byPath.emplace(key, i);
        const size_t slash = key.find_last_of('/');
        m_byName.emplace(slash == std::string::npos ? key : key.substr(slash + 1), i);
    }
}

// one line per entry offset size path after the header line
bool AssetStore::loadIndex(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    std::string line;
    if (!std::getline(file, line)) return false;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line != kIndexHeader) return false;
    std::vector<PakItem> items;
    items.reserve(20000);
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        const size_t a = line.find(' ');
        const size_t b = a == std::string::npos ? a : line.find(' ', a + 1);
        if (b == std::string::npos) continue;
        PakItem item;
        item.offset = static_cast<uint32_t>(std::strtoul(line.c_str(), nullptr, 10));
        item.size = static_cast<uint32_t>(std::strtoul(line.c_str() + a + 1, nullptr, 10));
        item.path = line.substr(b + 1);
        items.push_back(std::move(item));
    }
    if (items.empty()) return false;
    adoptItems(std::move(items));
    return true;
}

bool AssetStore::saveIndex(const std::string& path) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file << kIndexHeader << '\n';
    for (const PakItem& item : m_items) file << item.offset << ' ' << item.size << ' ' << item.path << '\n';
    return static_cast<bool>(file);
}

size_t AssetStore::pakEntryCount() const {
    return m_items.size();
}

bool AssetStore::pakHas(const std::string& relative) const {
    return m_byPath.find(pakKey(relative)) != m_byPath.end();
}

bool AssetStore::readPak(const std::string& relative, std::vector<uint8_t>& out) const {
    if (!m_pakFile) return false;
    const auto it = m_byPath.find(pakKey(relative));
    if (it == m_byPath.end()) return false;
    const PakItem& item = m_items[it->second];
    if (item.size == 0) return false;
    out.resize(item.size);
    std::lock_guard<std::mutex> lock(m_readLock);
    if (std::fseek(m_pakFile, static_cast<long>(item.offset), SEEK_SET) != 0) return false;
    return std::fread(out.data(), 1, item.size, m_pakFile) == item.size;
}

// the texture cache asks by file name the index is warm since the start so no second scan runs
bool AssetStore::readPakByName(const std::string& fileName, std::vector<uint8_t>& out) const {
    if (!m_pakFile) return false;
    waitPak();
    const std::string wanted = lowerCopy(forwardSlashes(fileName));
    const size_t slash = wanted.find_last_of('/');
    const auto it = m_byName.find(slash == std::string::npos ? wanted : wanted.substr(slash + 1));
    if (it == m_byName.end()) return false;
    const PakItem& item = m_items[it->second];
    if (item.size == 0) return false;
    out.resize(item.size);
    std::lock_guard<std::mutex> lock(m_readLock);
    if (std::fseek(m_pakFile, static_cast<long>(item.offset), SEEK_SET) != 0) return false;
    return std::fread(out.data(), 1, item.size, m_pakFile) == item.size;
}

bool AssetStore::diskHas(const std::string& relative) const {
    if (m_gameDir.empty()) return false;
    std::string rel = forwardSlashes(relative);
    if (rel.rfind("./", 0) == 0) rel = rel.substr(2);
    std::error_code ignored;
    return std::filesystem::is_regular_file(std::filesystem::path(m_gameDir) / rel, ignored);
}

bool AssetStore::readDisk(const std::string& relative, std::vector<uint8_t>& out) const {
    if (m_gameDir.empty()) return false;
    std::string rel = forwardSlashes(relative);
    if (rel.rfind("./", 0) == 0) rel = rel.substr(2);
    const std::filesystem::path full = std::filesystem::path(m_gameDir) / rel;
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

// the pak first as before the disk first only while the first scan still runs
bool AssetStore::readBytes(const std::string& relative, std::vector<uint8_t>& out) const {
    if (pakScanning() && readDisk(relative, out)) return true;
    waitPak();
    if (readPak(relative, out)) return true;
    return readDisk(relative, out);
}

bool AssetStore::readText(const std::string& relative, std::string& out) const {
    std::vector<uint8_t> bytes;
    if (!readBytes(relative, bytes)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

bool AssetStore::exists(const std::string& relative) const {
    if (pakScanning() && diskHas(relative)) return true;
    waitPak();
    return pakHas(relative) || diskHas(relative);
}

std::vector<std::string> AssetStore::candidates(const std::string& path) const {
    std::vector<std::string> list;
    const std::string base = stripKnownPrefix(path);
    const std::string languageDir = "Data/" + m_language + "/Image/";
    for (const std::string& name : nameVariants(base)) {
        list.push_back(name);
        list.push_back(languageDir + name);
        list.push_back("Data/Public/Image/" + name);
    }
    list.push_back(path);
    return list;
}

// every image of the pak and of the Data image folders built once for the search fallback
void AssetStore::buildImageIndex() const {
    if (m_imageIndexed) return;
    // while the first scan runs the list comes off the disk alone and is kept once the pak lands
    const bool scanning = pakScanning();
    m_imageIndexed = !scanning;
    m_imageIndex.clear();
    const double start = nowMs();
    std::set<std::string> seen;
    auto add = [&](const std::string& path) {
        if (seen.insert(pakKey(path)).second) m_imageIndex.push_back(path);
    };
    if (!scanning) {
        for (const PakItem& item : m_items) {
            const std::string lower = lowerCopy(item.path);
            if (lower.find("image/") != std::string::npos && isImageName(lower)) add(item.path);
        }
    }
    std::error_code ignored;
    for (const char* lang : kLanguages) {
        const std::filesystem::path root = std::filesystem::path(m_gameDir) / "Data" / lang / "Image";
        if (!std::filesystem::is_directory(root, ignored)) continue;
        // the root spelling of the iterator so the relative part is a substring not a path compare
        const std::string rootText = forwardSlashes(root.string()) + "/";
        for (std::filesystem::recursive_directory_iterator it(root, ignored), end; it != end; it.increment(ignored)) {
            if (!it->is_regular_file(ignored)) continue;
            const std::string full = forwardSlashes(it->path().string());
            if (full.size() <= rootText.size() || !isImageName(lowerCopy(full))) continue;
            add(std::string("./Data/") + lang + "/Image/" + full.substr(rootText.size()));
        }
    }
    std::printf("[assets] %zu image assets listed in %.0f ms\n", m_imageIndex.size(), nowMs() - start);
}

// last resort a file name search over every listed image
bool AssetStore::readBySearch(const std::string& path, std::vector<uint8_t>& out, std::string& found) const {
    std::string wanted = forwardSlashes(path);
    const size_t slash = wanted.rfind('/');
    if (slash != std::string::npos) wanted = wanted.substr(slash + 1);
    const size_t dot = wanted.find('.');
    if (dot != std::string::npos) wanted = wanted.substr(0, dot);
    if (!wanted.empty() && wanted.back() == '_') wanted.pop_back();
    if (wanted.empty()) return false;
    buildImageIndex();
    const std::string lowerWanted = lowerCopy(wanted);
    for (const std::string& asset : m_imageIndex) {
        const std::string lowerAsset = lowerCopy(asset);
        const size_t fileSlash = lowerAsset.find_last_of('/');
        const std::string file = fileSlash == std::string::npos ? lowerAsset : lowerAsset.substr(fileSlash + 1);
        if (file.find(lowerWanted) == std::string::npos) continue;
        if (readBytes(asset, out)) { found = asset; return true; }
    }
    return false;
}

const Texture* AssetStore::remember(const std::string& key, std::unique_ptr<Texture> tex) {
    const Texture* raw = tex.get();
    m_cache[key] = std::move(tex);
    return raw != nullptr && raw->valid() ? raw : nullptr;
}

const Texture* AssetStore::texture(const std::string& assetPath) {
    if (assetPath.empty()) return nullptr;
    const auto cached = m_cache.find(assetPath);
    if (cached != m_cache.end()) return cached->second->valid() ? cached->second.get() : nullptr;
    DecodedImage prefetched;
    {
        std::lock_guard<std::mutex> lock(m_decodedLock);
        const auto decoded = m_decoded.find(assetPath);
        if (decoded != m_decoded.end()) {
            prefetched = std::move(decoded->second);
            m_decoded.erase(decoded);
        }
    }
    if (!prefetched.pixels.empty())
        return upload(assetPath, prefetched.found, prefetched.pixels, prefetched.width, prefetched.height, true);

    auto tex = std::make_unique<Texture>();
    std::vector<uint8_t> bytes;
    std::string found;
    for (const std::string& candidate : candidates(assetPath)) {
        if (readBytes(candidate, bytes)) { found = candidate; break; }
    }
    if (found.empty() && readBySearch(assetPath, bytes, found))
        std::printf("[assets] %s found by search as %s\n", assetPath.c_str(), found.c_str());
    if (found.empty()) {
        std::printf("[assets] not found %s\n", assetPath.c_str());
        return remember(assetPath, std::move(tex));
    }

    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    if (!decodeRgba8(bytes, pixels, width, height) || width <= 0 || height <= 0) {
        std::printf("[assets] cannot decode %s\n", found.c_str());
        return remember(assetPath, std::move(tex));
    }
    return upload(assetPath, found, pixels, width, height, false);
}

const Texture* AssetStore::upload(const std::string& assetPath, const std::string& found,
                                  const std::vector<uint8_t>& pixels, int width, int height, bool prefetched) {
    auto tex = std::make_unique<Texture>();
    tex->handle = bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
                                        bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                                        bgfx::copy(pixels.data(), static_cast<uint32_t>(pixels.size())));
    tex->width = width;
    tex->height = height;
    if (tex->valid())
        std::printf("[assets] loaded %s as %s (%dx%d)%s\n", assetPath.c_str(), found.c_str(), width, height,
                    prefetched ? " decoded on a loader thread" : "");
    else std::printf("[assets] texture upload failed %s\n", found.c_str());
    return remember(assetPath, std::move(tex));
}

// the exact candidates only the search fallback builds an index the frame thread owns
void AssetStore::prefetchImages(const std::vector<std::string>& assetPaths) {
    for (const std::string& path : assetPaths) {
        if (path.empty()) continue;
        {
            std::lock_guard<std::mutex> lock(m_decodedLock);
            if (m_decoded.count(path) != 0) continue;
        }
        DecodedImage image;
        std::vector<uint8_t> bytes;
        for (const std::string& candidate : candidates(path)) {
            if (readBytes(candidate, bytes)) { image.found = candidate; break; }
        }
        if (image.found.empty() || !decodeRgba8(bytes, image.pixels, image.width, image.height) || image.width <= 0 ||
            image.height <= 0)
            continue;
        std::lock_guard<std::mutex> lock(m_decodedLock);
        m_decoded.emplace(path, std::move(image));
    }
}

void AssetStore::dropPrefetchedImages() {
    std::lock_guard<std::mutex> lock(m_decodedLock);
    m_decoded.clear();
}

const Texture* AssetStore::white() {
    if (m_white && m_white->valid()) return m_white.get();
    m_white = std::make_unique<Texture>();
    const uint8_t pixel[4] = {255, 255, 255, 255};
    m_white->handle = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
                                            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(pixel, 4));
    m_white->width = 1;
    m_white->height = 1;
    return m_white.get();
}

void AssetStore::destroyAll() {
    for (auto& entry : m_cache)
        if (entry.second->valid()) bgfx::destroy(entry.second->handle);
    m_cache.clear();
    if (m_white && m_white->valid()) bgfx::destroy(m_white->handle);
    m_white.reset();
}

}
