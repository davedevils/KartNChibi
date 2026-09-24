#pragma once
#include <bgfx/bgfx.h>

#include <atomic>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace KnC::Render {

// texture see through check size texel to 0 to 1
struct CachedTexture {
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    uint16_t            width = 0;
    uint16_t            height = 0;
    bool                translucent = false;
};

// Kept for the callers both read the file as authored the shader blends straight alpha
enum class TextureAlpha { Premultiplied, Straight };

// A second place to look when a texture name resolves to no file on disk the pak of the game
using TextureBytesSource = std::function<bool(const std::string& path, std::vector<uint8_t>& out)>;

// One source for the process the track loader points it at the game pak null clears it
void set_texture_bytes_source(TextureBytesSource source);

// The files read since the last take and the ms each step of them cost
struct TextureLoadStats {
    size_t count = 0;
    double read_ms = 0.0;
    double parse_ms = 0.0;
    double inspect_ms = 0.0;
    double create_ms = 0.0;
};

// Hands the counters over and zeroes them the race load prints them per step
TextureLoadStats take_texture_load_stats();

// Reads and inspects files off the render thread returns how many were prepared stop ends it after files in flight
size_t prefetch_textures(const std::vector<std::string>& paths, unsigned threads,
                         const std::atomic<bool>* stop = nullptr, bool background = false);

// Frees every prepared image no acquire took
void drop_prefetched_textures();

// A background thread calls it first so the frame loop wins the cores when the host is busy
void lower_current_thread_priority();

// DDS to GPU texture one upload per path the file goes to the device as it was authored
class TextureCache {
public:
    explicit TextureCache(TextureAlpha alpha = TextureAlpha::Premultiplied) { (void)alpha; }
    ~TextureCache();

    // white stand in for untextured missing diffuse
    bool create_white_stand_in();
    bgfx::TextureHandle white_stand_in() const { return white_; }

    // Invalid handle when file missing or not decodable DDS a no mip NiTexturingProperty filter uploads only its authored size
    CachedTexture acquire(const std::string& path, bool mipmapped = true);

    // coverage mask packed in file single channel alpha held as key
    CachedTexture acquire_alpha(const std::string& key, int width, int height,
                                const std::vector<uint8_t>& alpha);

    size_t missing_count() const { return missing_; }
    void   destroy_all();

private:
    std::map<std::string, CachedTexture> textures_;
    bgfx::TextureHandle white_ = BGFX_INVALID_HANDLE;
    size_t missing_ = 0;
};

}
