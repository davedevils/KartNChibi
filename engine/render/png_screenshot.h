#pragma once
#include <bgfx/bgfx.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace KnC::Render {

// bgfx callback turns screenshot to PNG everything else inert no cache
class PngScreenshotCallback : public bgfx::CallbackI {
public:
    ~PngScreenshotCallback() override { finish(); }

    // Waits for every PNG still encoding on its own thread the frame loop never waits for one
    void finish();

    // Both answers wait for the encoders first
    bool wrote_file() { finish(); return wrote_file_; }
    // Every pixel written carries one colour baseline that cannot fail
    bool wrote_one_colour() { finish(); return wrote_one_colour_; }

    void fatal(const char* file_path, uint16_t line, bgfx::Fatal::Enum code,
               const char* message) override;
    void traceVargs(const char* file_path, uint16_t line, const char* format,
                    va_list arguments) override;
    void profilerBegin(const char* name, uint32_t abgr, const char* file_path,
                       uint16_t line) override;
    void profilerBeginLiteral(const char* name, uint32_t abgr, const char* file_path,
                              uint16_t line) override;
    void profilerEnd() override;
    uint32_t cacheReadSize(uint64_t id) override;
    bool cacheRead(uint64_t id, void* destination, uint32_t size) override;
    void cacheWrite(uint64_t id, const void* source, uint32_t size) override;
    void screenShot(const char* file_path, uint32_t width, uint32_t height, uint32_t pitch,
                    bgfx::TextureFormat::Enum format, const void* pixels, uint32_t size,
                    bool flip_vertically) override;
    void captureBegin(uint32_t width, uint32_t height, uint32_t pitch,
                      bgfx::TextureFormat::Enum format, bool flip_vertically) override;
    void captureEnd() override;
    void captureFrame(const void* pixels, uint32_t size) override;

private:
    // The encoders of the captures the render callback only copies the pixels
    std::mutex writers_lock_;
    std::vector<std::thread> writers_;
    // Set on render thread read on main after bgfx shut down
    std::atomic<bool> wrote_file_{false};
    std::atomic<bool> wrote_one_colour_{false};
};

}
