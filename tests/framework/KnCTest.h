
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <cmath>
#include <algorithm>

#include "rhi/RHI.h"
#include "rhi/RHITypes.h"

// Window include needed for SetDimensionsForTest since Renderer reads its width and height
#include "core/platform/Window.h"

#include <GLFW/glfw3.h>

namespace KnCTest {

enum BackendFlag : uint32_t {
    DX12 = 1 << 0,
    VK   = 1 << 1,
    WGPU = 1 << 2,
    ALL  = DX12 | VK | WGPU
};

inline const char* BackendName(KnC::RHI::GraphicsAPI api) {
    switch (api) {
        case KnC::RHI::GraphicsAPI::DirectX12: return "DirectX12";
        case KnC::RHI::GraphicsAPI::Vulkan:    return "Vulkan";
        case KnC::RHI::GraphicsAPI::WebGPU:    return "WebGPU";
    }
    return "Unknown";
}

inline KnC::RHI::GraphicsAPI FlagToAPI(BackendFlag flag) {
    switch (flag) {
        case DX12: return KnC::RHI::GraphicsAPI::DirectX12;
        case VK:   return KnC::RHI::GraphicsAPI::Vulkan;
        case WGPU: return KnC::RHI::GraphicsAPI::WebGPU;
        default:   return KnC::RHI::GraphicsAPI::DirectX12;
    }
}

enum class TestStatus { Pass, Fail, Skip };

struct TestResult {
    std::string testName;
    std::string backendName;
    TestStatus status = TestStatus::Pass;
    std::string message;
    double durationMs = 0.0;
};

struct TestCase {
    std::string name;
    uint32_t backends;
    bool needsWindow;   // true means a visual test that needs RHI init
    std::function<void()> func;
};

class TestRunner;
inline TestRunner& GetRunner();

struct AssertionFailure {
    std::string file;
    int line;
    std::string expr;
    std::string message;
};

inline thread_local std::vector<AssertionFailure> t_failures;
inline thread_local bool t_aborted = false;
inline thread_local bool t_skipped = false;
inline thread_local std::string t_skipReason;

inline void ResetTestState() {
    t_failures.clear();
    t_aborted = false;
    t_skipped = false;
    t_skipReason.clear();
}

class TestRunner {
public:
    std::string suiteName;
    std::vector<TestCase> tests;
    std::vector<TestResult> results;

    uint32_t backendFilter = ALL;
    bool visualMode = false;
    std::string outputDir = "tests/output";
    bool goldenMode = false;
    int windowWidth = 800;
    int windowHeight = 600;

    GLFWwindow* window = nullptr;
    KnC::RHI::GraphicsAPI currentAPI;

    void RegisterTest(const std::string& name, uint32_t backends, bool needsWindow, std::function<void()> func) {
        tests.push_back({name, backends, needsWindow, std::move(func)});
    }

    bool ParseArgs(int argc, char** argv) {
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--visual") { visualMode = true; }
            else if (arg == "--auto") { visualMode = false; }
            else if (arg == "--golden") { goldenMode = true; }
            else if (arg.rfind("--output=", 0) == 0) { outputDir = arg.substr(9); }
            else if (arg.rfind("--backend=", 0) == 0) {
                backendFilter = 0;
                std::string list = arg.substr(10);
                if (list.find("dx12") != std::string::npos) backendFilter |= DX12;
                if (list.find("vulkan") != std::string::npos || list.find("vk") != std::string::npos) backendFilter |= VK;
                if (list.find("webgpu") != std::string::npos || list.find("wgpu") != std::string::npos) backendFilter |= WGPU;
            }
            else if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                    << "  --backend=dx12,vk,wgpu  Filter backends\n"
                    << "  --visual                Visual mode (pause between tests)\n"
                    << "  --auto                  Auto mode (default)\n"
                    << "  --golden                Generate golden reference images\n"
                    << "  --output=path           Output directory\n";
                return false;
            }
        }
        return true;
    }

    bool InitBackend(KnC::RHI::GraphicsAPI api) {
        currentAPI = api;

        if (window) { glfwDestroyWindow(window); window = nullptr; }

        // sets GLFW hints since no native GL context is needed for DX12 Vulkan or WebGPU
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

        window = glfwCreateWindow(windowWidth, windowHeight,
            (std::string("KnC Test - ") + BackendName(api)).c_str(), nullptr, nullptr);
        if (!window) return false;

        if (!KnC::RHI::RHI::Initialize(api, window, windowWidth, windowHeight))
            return false;

        // sets Window dimensions since the test framework bypasses Window Init leaving width and height at 0
        KnC::Engine::Window::SetDimensionsForTest(windowWidth, windowHeight);
        return true;
    }

    void ShutdownBackend() {
        KnC::RHI::RHI::Shutdown();
    }

    int Run(int argc, char** argv) {
        if (!ParseArgs(argc, argv)) return 0;

        if (!glfwInit()) {
            std::cerr << "[ERROR] Failed to init GLFW\n";
            return 1;
        }

        std::cout << "\n========================================\n";
        std::cout << "  KnC Test Suite: " << suiteName << "\n";
        std::cout << "========================================\n\n";

        BackendFlag backendOrder[] = { DX12, VK, WGPU };
        KnC::RHI::GraphicsAPI apiOrder[] = {
            KnC::RHI::GraphicsAPI::DirectX12,
            KnC::RHI::GraphicsAPI::Vulkan,
            KnC::RHI::GraphicsAPI::WebGPU
        };

        for (int b = 0; b < 3; b++) {
            BackendFlag bf = backendOrder[b];

            if (!(backendFilter & bf)) continue;

            bool hasTests = false;
            for (auto& t : tests) {
                if (t.backends & bf) { hasTests = true; break; }
            }
            if (!hasTests) continue;

            std::cout << "--- Backend: " << BackendName(apiOrder[b]) << " ---\n";

            bool backendOk = false;
            bool backendInitAttempted = false;

            for (auto& t : tests) {
                if (!(t.backends & bf)) continue;

                ResetTestState();
                auto start = std::chrono::high_resolution_clock::now();

                TestResult result;
                result.testName = t.name;
                result.backendName = BackendName(apiOrder[b]);

                if (t.needsWindow && !backendInitAttempted) {
                    backendInitAttempted = true;
                    backendOk = InitBackend(apiOrder[b]);
                    if (!backendOk) {
                        std::cout << "  [SKIP] Backend " << BackendName(apiOrder[b]) << " init failed\n";
                    }
                }

                if (t.needsWindow && !backendOk) {
                    result.status = TestStatus::Skip;
                    result.message = "Backend init failed";
                    results.push_back(result);
                    PrintResult(result);
                    continue;
                }

                try {
                    t.func();
                } catch (const std::exception& e) {
                    t_failures.push_back({"", 0, "", std::string("Exception: ") + e.what()});
                } catch (...) {
                    t_failures.push_back({"", 0, "", "Unknown exception"});
                }

                auto end = std::chrono::high_resolution_clock::now();
                result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();

                if (t_skipped) {
                    result.status = TestStatus::Skip;
                    result.message = t_skipReason;
                } else if (!t_failures.empty()) {
                    result.status = TestStatus::Fail;
                    result.message = t_failures[0].message;
                    if (!t_failures[0].expr.empty())
                        result.message = t_failures[0].expr + " -- " + result.message;
                } else {
                    result.status = TestStatus::Pass;
                }

                results.push_back(result);
                PrintResult(result);

                // always present after RHI tests so output is visible
                if (t.needsWindow && backendOk) {
                    if (KnC::RHI::RHI::GetDevice()) {
                        KnC::RHI::RHI::GetDevice()->Present();
                    }
                    glfwPollEvents();
                    if (visualMode) {
                        std::cout << "  (Press ENTER to continue...)\n";
                        std::cin.get();
                    }
                }
            }

            if (backendInitAttempted && backendOk) {
                ShutdownBackend();
            }
        }

        PrintSummary();
        WriteJUnitXML();

        if (window) { glfwDestroyWindow(window); window = nullptr; }
        glfwTerminate();

        for (auto& r : results) {
            if (r.status == TestStatus::Fail) return 1;
        }
        return 0;
    }

    void PrintResult(const TestResult& r) {
        const char* tag = "";
        switch (r.status) {
            case TestStatus::Pass: tag = "\033[32m[PASS]\033[0m"; break;
            case TestStatus::Fail: tag = "\033[31m[FAIL]\033[0m"; break;
            case TestStatus::Skip: tag = "\033[33m[SKIP]\033[0m"; break;
        }
        std::cout << "  " << tag << " " << r.testName;
        if (!r.message.empty()) std::cout << " -- " << r.message;
        std::cout << " (" << (int)r.durationMs << "ms)\n";
    }

    void PrintSummary() {
        int pass = 0, fail = 0, skip = 0;
        for (auto& r : results) {
            if (r.status == TestStatus::Pass) pass++;
            else if (r.status == TestStatus::Fail) fail++;
            else skip++;
        }
        std::cout << "\n========================================\n";
        std::cout << "  Results: " << pass << " passed, "
                  << fail << " failed, " << skip << " skipped"
                  << " (total: " << results.size() << ")\n";
        std::cout << "========================================\n";
    }

    void WriteJUnitXML() {
        // create output dir best effort
        #ifdef _WIN32
        std::string cmd = "mkdir \"" + outputDir + "\" 2>NUL";
        #else
        std::string cmd = "mkdir -p \"" + outputDir + "\"";
        #endif
        (void)system(cmd.c_str());

        std::string path = outputDir + "/results_" + suiteName + ".xml";
        std::ofstream f(path);
        if (!f.is_open()) {
            std::cerr << "[WARN] Cannot write JUnit XML to " << path << "\n";
            return;
        }

        int pass = 0, fail = 0, skip = 0;
        double totalTime = 0;
        for (auto& r : results) {
            if (r.status == TestStatus::Pass) pass++;
            else if (r.status == TestStatus::Fail) fail++;
            else skip++;
            totalTime += r.durationMs;
        }

        f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        f << "<testsuites tests=\"" << results.size() << "\" failures=\"" << fail
          << "\" skipped=\"" << skip << "\" time=\"" << (totalTime / 1000.0) << "\">\n";
        f << "  <testsuite name=\"" << suiteName << "\" tests=\"" << results.size()
          << "\" failures=\"" << fail << "\" skipped=\"" << skip << "\">\n";

        for (auto& r : results) {
            f << "    <testcase name=\"" << r.testName << "\" classname=\""
              << r.backendName << "\" time=\"" << (r.durationMs / 1000.0) << "\"";
            if (r.status == TestStatus::Pass) {
                f << "/>\n";
            } else {
                f << ">\n";
                if (r.status == TestStatus::Fail) {
                    f << "      <failure message=\"" << XmlEscape(r.message) << "\"/>\n";
                } else {
                    f << "      <skipped message=\"" << XmlEscape(r.message) << "\"/>\n";
                }
                f << "    </testcase>\n";
            }
        }

        f << "  </testsuite>\n</testsuites>\n";
        f.close();
        std::cout << "JUnit XML: " << path << "\n";
    }

    static std::string XmlEscape(const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default: out += c;
            }
        }
        return out;
    }
};

inline TestRunner& GetRunner() {
    static TestRunner runner;
    return runner;
}

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t bfType = 0x4D42; // 'BM'
    uint32_t bfSize = 0;
    uint16_t bfReserved1 = 0;
    uint16_t bfReserved2 = 0;
    uint32_t bfOffBits = 54;
};
struct BMPInfoHeader {
    uint32_t biSize = 40;
    int32_t  biWidth = 0;
    int32_t  biHeight = 0;       // positive means bottom up
    uint16_t biPlanes = 1;
    uint16_t biBitCount = 24;    // 24-bit BGR
    uint32_t biCompression = 0;
    uint32_t biSizeImage = 0;
    int32_t  biXPelsPerMeter = 2835;
    int32_t  biYPelsPerMeter = 2835;
    uint32_t biClrUsed = 0;
    uint32_t biClrImportant = 0;
};
#pragma pack(pop)

// saves an RGBA pixel buffer as a 24 bit bottom up BGR BMP
inline bool SaveBMP(const std::string& path, const uint8_t* rgba, int width, int height) {
    int rowBytes = ((width * 3 + 3) & ~3); // BMP rows padded to 4-byte boundary
    uint32_t imageSize = (uint32_t)(rowBytes * height);

    BMPFileHeader fh;
    fh.bfSize = 54 + imageSize;

    BMPInfoHeader ih;
    ih.biWidth = width;
    ih.biHeight = height;
    ih.biSizeImage = imageSize;

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
    f.write(reinterpret_cast<const char*>(&ih), sizeof(ih));

    // writes pixel rows bottom up converting RGBA to BGR
    std::vector<uint8_t> row(rowBytes, 0);
    for (int y = height - 1; y >= 0; y--) {
        const uint8_t* src = rgba + y * width * 4;
        for (int x = 0; x < width; x++) {
            row[x * 3 + 0] = src[x * 4 + 2];
            row[x * 3 + 1] = src[x * 4 + 1];
            row[x * 3 + 2] = src[x * 4 + 0];
        }
        f.write(reinterpret_cast<const char*>(row.data()), rowBytes);
    }
    f.close();
    return true;
}

// captures a screenshot from the current RHI device returns the saved path or empty on failure
inline std::string CaptureScreenshot(const char* name) {
    auto& runner = GetRunner();
    auto* dev = KnC::RHI::RHI::GetDevice();
    if (!dev) return "";

    int w = runner.windowWidth;
    int h = runner.windowHeight;
    std::vector<uint8_t> pixels(w * h * 4);

    if (!dev->ReadPixels(0, 0, w, h, pixels.data())) {
        std::cerr << "  [WARN] ReadPixels failed for screenshot '" << name << "'\n";
        return "";
    }

    #ifdef _WIN32
    std::string mkdirCmd = "mkdir \"" + runner.outputDir + "\" 2>NUL";
    #else
    std::string mkdirCmd = "mkdir -p \"" + runner.outputDir + "\"";
    #endif
    (void)system(mkdirCmd.c_str());

    std::string filename = runner.outputDir + "/" +
        BackendName(runner.currentAPI) + "_" + name + ".bmp";
    if (SaveBMP(filename, pixels.data(), w, h)) {
        std::cout << "  [SCREENSHOT] " << filename << "\n";
        return filename;
    }
    return "";
}

// compares two BMP files pixel by pixel within a per channel tolerance
inline bool CompareScreenshot(const std::string& testPath, const std::string& goldenPath, int tolerance) {
    std::ifstream fTest(testPath, std::ios::binary | std::ios::ate);
    std::ifstream fGolden(goldenPath, std::ios::binary | std::ios::ate);
    if (!fTest.is_open() || !fGolden.is_open()) return false;

    auto sizeT = fTest.tellg();
    auto sizeG = fGolden.tellg();
    if (sizeT != sizeG) return false;

    fTest.seekg(0);
    fGolden.seekg(0);

    std::vector<uint8_t> dataT((size_t)sizeT);
    std::vector<uint8_t> dataG((size_t)sizeG);
    fTest.read(reinterpret_cast<char*>(dataT.data()), sizeT);
    fGolden.read(reinterpret_cast<char*>(dataG.data()), sizeG);

    // skip BMP headers 54 bytes compare pixel data
    if ((size_t)sizeT <= 54) return false;
    for (size_t i = 54; i < (size_t)sizeT; i++) {
        int diff = std::abs((int)dataT[i] - (int)dataG[i]);
        if (diff > tolerance) return false;
    }
    return true;
}

// hashes the current framebuffer with FNV-1a 32 bit
inline uint32_t HashFramebuffer() {
    auto& runner = GetRunner();
    auto* dev = KnC::RHI::RHI::GetDevice();
    if (!dev) return 0;

    int w = runner.windowWidth;
    int h = runner.windowHeight;
    std::vector<uint8_t> pixels(w * h * 4);

    if (!dev->ReadPixels(0, 0, w, h, pixels.data())) return 0;

    uint32_t hash = 0x811c9dc5u;
    for (size_t i = 0; i < pixels.size(); i++) {
        hash ^= pixels[i];
        hash *= 0x01000193u;
    }
    return hash;
}

#define KNC_TEST_SUITE(name) \
    namespace { struct SuiteInit_ { SuiteInit_() { KnCTest::GetRunner().suiteName = name; } } _suiteInit_; }

// registers a visual RHI test that needs window and backend init
#define KNC_TEST(name, backends) \
    static void _test_func_##name(); \
    namespace { struct _Reg_##name { _Reg_##name() { \
        KnCTest::GetRunner().RegisterTest(#name, backends, true, _test_func_##name); \
    } } _reg_instance_##name; } \
    static void _test_func_##name()

// registers a non visual test that needs no window or RHI
#define KNC_TEST_NOWINDOW(name) \
    static void _test_func_##name(); \
    namespace { struct _Reg_##name { _Reg_##name() { \
        KnCTest::GetRunner().RegisterTest(#name, KnCTest::ALL, false, _test_func_##name); \
    } } _reg_instance_##name; } \
    static void _test_func_##name()

// soft check continues on failure
#define KNC_CHECK(expr) \
    do { if (!(expr)) { \
        KnCTest::t_failures.push_back({__FILE__, __LINE__, #expr, "CHECK failed"}); \
    } } while(0)

// hard check aborts test on failure
#define KNC_REQUIRE(expr) \
    do { if (!(expr)) { \
        KnCTest::t_failures.push_back({__FILE__, __LINE__, #expr, "REQUIRE failed"}); \
        KnCTest::t_aborted = true; \
        return; \
    } } while(0)

// equality check with diff message
#define KNC_CHECK_EQUAL(a, b) \
    do { auto _a_ = (a); auto _b_ = (b); if (!(_a_ == _b_)) { \
        std::ostringstream _ss_; _ss_ << #a " != " #b " (" << _a_ << " vs " << _b_ << ")"; \
        KnCTest::t_failures.push_back({__FILE__, __LINE__, _ss_.str(), "CHECK_EQUAL failed"}); \
    } } while(0)

// float near-equality check
#define KNC_CHECK_NEAR(a, b, eps) \
    do { auto _a_ = (a); auto _b_ = (b); if (std::abs(_a_ - _b_) > (eps)) { \
        std::ostringstream _ss_; _ss_ << #a " !~ " #b " (diff=" << std::abs(_a_-_b_) << ", eps=" << (eps) << ")"; \
        KnCTest::t_failures.push_back({__FILE__, __LINE__, _ss_.str(), "CHECK_NEAR failed"}); \
    } } while(0)

#define KNC_SKIP(reason) \
    do { KnCTest::t_skipped = true; KnCTest::t_skipReason = reason; return; } while(0)

// screenshot capture saves a BMP via RHI ReadPixels
#define KNC_SCREENSHOT(name) \
    do { KnCTest::CaptureScreenshot(name); } while(0)

#define KNC_CHECK_SCREENSHOT(name, tol) \
    do { \
        std::string path = KnCTest::CaptureScreenshot(name); \
        if (path.empty()) { \
            KnCTest::t_failures.push_back({__FILE__, __LINE__, "screenshot capture", "ReadPixels failed for " name}); \
        } else { \
            std::string goldenPath = KnCTest::GetRunner().outputDir + "/golden/" + \
                KnCTest::BackendName(KnCTest::GetRunner().currentAPI) + "_" + name + ".bmp"; \
            if (!KnCTest::CompareScreenshot(path, goldenPath, tol)) { \
                KnCTest::t_failures.push_back({__FILE__, __LINE__, "screenshot compare", \
                    std::string("Screenshot ") + name + " differs from golden (tol=" + std::to_string(tol) + ")"}); \
            } \
        } \
    } while(0)

#define KNC_CHECK_FRAMEBUFFER_HASH(expected) \
    do { \
        uint32_t hash = KnCTest::HashFramebuffer(); \
        if (hash != (expected)) { \
            std::ostringstream _ss_; \
            _ss_ << "Framebuffer hash 0x" << std::hex << hash << " != expected 0x" << (expected); \
            KnCTest::t_failures.push_back({__FILE__, __LINE__, _ss_.str(), "FRAMEBUFFER_HASH failed"}); \
        } \
    } while(0)

// main entry point macro put at end of each test cpp
#define KNC_TEST_MAIN() \
    int main(int argc, char** argv) { \
        return KnCTest::GetRunner().Run(argc, argv); \
    }

} // namespace KnCTest
