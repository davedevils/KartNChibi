// bgfx callback that writes the requested screenshot as a PNG
#pragma once

#include <bgfx/bgfx.h>

#include <atomic>

namespace KnC::Tools {

class UiScreenshotCallback : public bgfx::CallbackI {
public:
    ~UiScreenshotCallback() override = default;

    bool wrote_file() const { return wrote_file_; }
    bool wrote_one_colour() const { return wrote_one_colour_; }

    void fatal(const char* file_path, uint16_t line, bgfx::Fatal::Enum code, const char* message) override;
    void traceVargs(const char* file_path, uint16_t line, const char* format, va_list arguments) override;
    void profilerBegin(const char* name, uint32_t abgr, const char* file_path, uint16_t line) override;
    void profilerBeginLiteral(const char* name, uint32_t abgr, const char* file_path, uint16_t line) override;
    void profilerEnd() override;
    uint32_t cacheReadSize(uint64_t id) override;
    bool cacheRead(uint64_t id, void* destination, uint32_t size) override;
    void cacheWrite(uint64_t id, const void* source, uint32_t size) override;
    void screenShot(const char* file_path, uint32_t width, uint32_t height, uint32_t pitch,
                    bgfx::TextureFormat::Enum format, const void* pixels, uint32_t size,
                    bool flip_vertically) override;
    void captureBegin(uint32_t width, uint32_t height, uint32_t pitch, bgfx::TextureFormat::Enum format,
                      bool flip_vertically) override;
    void captureEnd() override;
    void captureFrame(const void* pixels, uint32_t size) override;

private:
    std::atomic<bool> wrote_file_{false};
    std::atomic<bool> wrote_one_colour_{false};
};

}
