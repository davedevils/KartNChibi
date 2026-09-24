#include "engine/render/png_screenshot.h"

#include <bimg/bimg.h>
#include <bx/error.h>
#include <bx/file.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace KnC::Render {

void PngScreenshotCallback::fatal(const char* file_path, uint16_t line, bgfx::Fatal::Enum code,
                                  const char* message) {
    std::fprintf(stderr, "[render] bgfx fatal %d at %s:%u: %s\n", static_cast<int>(code),
                 file_path, line, message);
    std::abort();
}

void PngScreenshotCallback::traceVargs(const char* file_path, uint16_t line, const char* format,
                                       va_list arguments) {
    std::fprintf(stderr, "[bgfx] %s:%u ", file_path, line);
    std::vfprintf(stderr, format, arguments);
}

void PngScreenshotCallback::profilerBegin(const char*, uint32_t, const char*, uint16_t) {}

void PngScreenshotCallback::profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) {}

void PngScreenshotCallback::profilerEnd() {}

// Size 0 makes bgfx compile every time no shader cache
uint32_t PngScreenshotCallback::cacheReadSize(uint64_t) { return 0; }

bool PngScreenshotCallback::cacheRead(uint64_t, void*, uint32_t) { return false; }

void PngScreenshotCallback::cacheWrite(uint64_t, const void*, uint32_t) {}

namespace {

// Frame carries one colour bgfx gives four bytes pixel compared
bool frame_is_one_colour(const void* pixels, uint32_t width, uint32_t height, uint32_t pitch) {
    const auto* rows = static_cast<const uint8_t*>(pixels);
    const uint32_t first = *reinterpret_cast<const uint32_t*>(rows);
    for (uint32_t row = 0; row < height; ++row) {
        const auto* texels = reinterpret_cast<const uint32_t*>(rows + row * pitch);
        for (uint32_t column = 0; column < width; ++column)
            if (texels[column] != first) return false;
    }
    return true;
}

} // namespace

// The pixels are copied and the PNG encodes on its own thread a capture no longer stalls the frame
void PngScreenshotCallback::screenShot(const char* file_path, uint32_t width, uint32_t height,
                                       uint32_t pitch, bgfx::TextureFormat::Enum format,
                                       const void* pixels, uint32_t, bool flip_vertically) {
    const auto* bytes = static_cast<const uint8_t*>(pixels);
    std::vector<uint8_t> copy(bytes, bytes + static_cast<size_t>(pitch) * height);
    std::string path(file_path);
    std::lock_guard<std::mutex> lock(writers_lock_);
    writers_.emplace_back([this, path, width, height, pitch, format, flip_vertically,
                           copy = std::move(copy)]() {
        bx::FileWriter writer;
        bx::Error error;
        if (!bx::open(&writer, path.c_str(), false, &error)) {
            std::fprintf(stderr, "[render] cannot open %s for writing\n", path.c_str());
            return;
        }
        const int32_t written =
            bimg::imageWritePng(&writer, width, height, pitch, copy.data(),
                                static_cast<bimg::TextureFormat::Enum>(format), flip_vertically,
                                &error);
        bx::close(&writer);
        if (written <= 0 || !error.isOk()) {
            std::fprintf(stderr, "[render] PNG encode failed for %s\n", path.c_str());
            return;
        }
        wrote_one_colour_ = frame_is_one_colour(copy.data(), width, height, pitch);
        wrote_file_ = true;
        std::fprintf(stderr, "[render] wrote %s (%ux%u)\n", path.c_str(), width, height);
    });
}

void PngScreenshotCallback::finish() {
    std::vector<std::thread> pending;
    {
        std::lock_guard<std::mutex> lock(writers_lock_);
        pending.swap(writers_);
    }
    for (std::thread& writer : pending)
        if (writer.joinable()) writer.join();
}

void PngScreenshotCallback::captureBegin(uint32_t, uint32_t, uint32_t,
                                         bgfx::TextureFormat::Enum, bool) {}

void PngScreenshotCallback::captureEnd() {}

void PngScreenshotCallback::captureFrame(const void*, uint32_t) {}

}
