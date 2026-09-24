#include "ui_screenshot.h"

#include <bimg/bimg.h>
#include <bx/error.h>
#include <bx/file.h>

#include <cstdio>
#include <cstdlib>

namespace KnC::Tools {

void UiScreenshotCallback::fatal(const char* file_path, uint16_t line, bgfx::Fatal::Enum code, const char* message) {
    std::fprintf(stderr, "[ui_editor] bgfx fatal %d at %s:%u: %s\n", static_cast<int>(code), file_path, line, message);
    std::abort();
}

void UiScreenshotCallback::traceVargs(const char* file_path, uint16_t line, const char* format, va_list arguments) {
    std::fprintf(stderr, "[bgfx] %s:%u ", file_path, line);
    std::vfprintf(stderr, format, arguments);
}

void UiScreenshotCallback::profilerBegin(const char*, uint32_t, const char*, uint16_t) {}
void UiScreenshotCallback::profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) {}
void UiScreenshotCallback::profilerEnd() {}
uint32_t UiScreenshotCallback::cacheReadSize(uint64_t) { return 0; }
bool UiScreenshotCallback::cacheRead(uint64_t, void*, uint32_t) { return false; }
void UiScreenshotCallback::cacheWrite(uint64_t, const void*, uint32_t) {}

namespace {

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

}

void UiScreenshotCallback::screenShot(const char* file_path, uint32_t width, uint32_t height, uint32_t pitch,
                                      bgfx::TextureFormat::Enum format, const void* pixels, uint32_t,
                                      bool flip_vertically) {
    bx::FileWriter writer;
    bx::Error error;
    if (!bx::open(&writer, file_path, false, &error)) {
        std::fprintf(stderr, "[ui_editor] cannot open %s for writing\n", file_path);
        return;
    }
    const int32_t written = bimg::imageWritePng(&writer, width, height, pitch, pixels,
                                                static_cast<bimg::TextureFormat::Enum>(format), flip_vertically, &error);
    bx::close(&writer);
    if (written <= 0 || !error.isOk()) {
        std::fprintf(stderr, "[ui_editor] PNG encode failed for %s\n", file_path);
        return;
    }
    wrote_file_ = true;
    wrote_one_colour_ = frame_is_one_colour(pixels, width, height, pitch);
    std::fprintf(stderr, "[ui_editor] wrote %s (%ux%u)\n", file_path, width, height);
}

void UiScreenshotCallback::captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) {}
void UiScreenshotCallback::captureEnd() {}
void UiScreenshotCallback::captureFrame(const void*, uint32_t) {}

}
