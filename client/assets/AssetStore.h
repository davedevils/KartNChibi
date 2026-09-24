// pak and Data folder lookup with a bgfx texture cache decoded through bimg
#pragma once

#include <bgfx/bgfx.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace KnC::Client {

// one GPU texture owned by the store the screens only point at it
struct Texture {
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    int width = 0;
    int height = 0;
    bool valid() const { return bgfx::isValid(handle); }
};

class AssetStore {
public:
    AssetStore();
    ~AssetStore();

    // opens the pak under the game folder from its index cache or scans it once on a thread
    bool open(const std::string& gameDir, const std::string& language);

    // raw bytes of a Data relative path pak first then disk the disk first while the pak scan runs
    bool readBytes(const std::string& relative, std::vector<uint8_t>& out) const;
    bool readText(const std::string& relative, std::string& out) const;
    // true when the relative path exists in the pak or on disk
    bool exists(const std::string& relative) const;
    // the bytes of the first pak file of that file name any folder safe from a loader thread
    bool readPakByName(const std::string& fileName, std::vector<uint8_t>& out) const;

    // cached texture for an asset path as the UI JSON names it null when nothing decodes
    const Texture* texture(const std::string& assetPath);
    // reads and decodes images on a loader thread the next texture call of one only uploads it
    void prefetchImages(const std::vector<std::string>& assetPaths);
    // frees the decoded images no texture call took
    void dropPrefetchedImages();
    // one white pixel for plain rectangles
    const Texture* white();

    void destroyAll();

    const std::string& gameDir() const { return m_gameDir; }
    const std::string& language() const { return m_language; }
    // the pak table size zero before the first scan lands
    size_t pakEntryCount() const;
    // true while the first scan of the pak still runs on its thread
    bool pakScanning() const { return m_scanning.load(); }
    // waits for the pak table the first pak read does the same
    void waitPak() const;

private:
    // one file of the pak its path as the pak spells it the data offset and the size
    struct PakItem {
        std::string path;
        uint32_t offset = 0;
        uint32_t size = 0;
    };
    std::vector<std::string> candidates(const std::string& path) const;
    bool readBySearch(const std::string& path, std::vector<uint8_t>& out, std::string& found) const;
    const Texture* remember(const std::string& key, std::unique_ptr<Texture> tex);
    bool readPak(const std::string& relative, std::vector<uint8_t>& out) const;
    bool readDisk(const std::string& relative, std::vector<uint8_t>& out) const;
    bool pakHas(const std::string& relative) const;
    bool diskHas(const std::string& relative) const;
    // the index cache of this pak in the temp folder keyed by its size and write time
    std::string cachePath() const;
    bool loadIndex(const std::string& path);
    bool saveIndex(const std::string& path) const;
    // the byte scan of the whole pak through the shared reader the slow first start
    void scanPak();
    void adoptItems(std::vector<PakItem> items) const;
    // the image list of the search fallback built the first time a name is searched
    void buildImageIndex() const;

    std::string m_gameDir;
    std::string m_language = "Eng";
    std::string m_pakPath;
    // the pak table by lower case path with the pak file open for the reads
    mutable std::vector<PakItem> m_items;
    mutable std::unordered_map<std::string, size_t> m_byPath;
    // the first entry of each lower case file name as the shared reader keeps it
    mutable std::unordered_map<std::string, size_t> m_byName;
    FILE* m_pakFile = nullptr;
    // one seek and read at a time on the shared handle and one join of the scan
    mutable std::mutex m_readLock;
    mutable std::mutex m_waitLock;
    // the first start scans on this thread every pak read joins it first
    mutable std::thread m_scan;
    mutable std::atomic<bool> m_scanning{false};
    mutable std::vector<PakItem> m_scanned;
    mutable std::vector<std::string> m_imageIndex;
    mutable bool m_imageIndexed = false;
    std::map<std::string, std::unique_ptr<Texture>> m_cache;
    std::unique_ptr<Texture> m_white;
    // rgba pixels a loader thread decoded by asset path the frame thread uploads and drops each
    struct DecodedImage {
        std::string found;
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
    };
    std::mutex m_decodedLock;
    std::unordered_map<std::string, DecodedImage> m_decoded;
    // the device texture of rgba pixels cached under the asset path
    const Texture* upload(const std::string& assetPath, const std::string& found, const std::vector<uint8_t>& pixels,
                          int width, int height, bool prefetched);
};

}
