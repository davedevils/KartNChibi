// scope timers and frame profiler

#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace KnC {
namespace Debug {

// profile sample data
struct ProfileSample {
    std::string name;
    double duration;      // milliseconds
    int callCount;
    double minDuration;
    double maxDuration;
    double avgDuration;
};

// profiler for perf measurement use the profile function or scope macro then PrintResults
class Profiler {
public:
    // begin frame
    static void BeginFrame();

    // end frame
    static void EndFrame();

    // push profiling marker
    static void PushMarker(const char* name);

    // pop profiling marker
    static void PopMarker();

    // record sample
    static void RecordSample(const char* name, double durationMs);

    // get all samples
    static std::vector<ProfileSample> GetSamples();

    // print results to console
    static void PrintResults();

    // clear all data
    static void Clear();

    // enable or disable profiling
    static void SetEnabled(bool enabled) { s_enabled = enabled; }
    static bool IsEnabled() { return s_enabled; }

private:
    struct SampleData {
        double totalDuration = 0.0;
        int callCount = 0;
        double minDuration = 1e9;
        double maxDuration = 0.0;
    };
    
    static std::unordered_map<std::string, SampleData> s_samples;
    static std::mutex s_mutex;
    static bool s_enabled;
    
    struct MarkerStack {
        std::string name;
        std::chrono::high_resolution_clock::time_point start;
    };
    
    static thread_local std::vector<MarkerStack> s_markerStack;
};

// RAII profiling scope
class ProfileScope {
public:
    ProfileScope(const char* name)
        : m_name(name)
        , m_start(std::chrono::high_resolution_clock::now())
    {
        if (Profiler::IsEnabled()) {
            Profiler::PushMarker(name);
        }
    }
    
    ~ProfileScope() {
        if (Profiler::IsEnabled()) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - m_start).count();
            
            Profiler::RecordSample(m_name, duration);
            Profiler::PopMarker();
        }
    }
    
private:
    const char* m_name;
    std::chrono::high_resolution_clock::time_point m_start;
};

}} // namespace KnC Debug

#define PROFILE_FUNCTION() KnC::Debug::ProfileScope __profileScope##__LINE__(__FUNCTION__)
#define PROFILE_SCOPE(name) KnC::Debug::ProfileScope __profileScope##__LINE__(name)

#ifdef NDEBUG
    // No profiling in release
    #undef PROFILE_FUNCTION
    #undef PROFILE_SCOPE
    #define PROFILE_FUNCTION()
    #define PROFILE_SCOPE(name)
#endif

