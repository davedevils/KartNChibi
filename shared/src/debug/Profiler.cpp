#include "shared/debug/Profiler.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace KnC {
namespace Debug {

std::unordered_map<std::string, Profiler::SampleData> Profiler::s_samples;
std::mutex Profiler::s_mutex;
bool Profiler::s_enabled = true;
thread_local std::vector<Profiler::MarkerStack> Profiler::s_markerStack;

void Profiler::BeginFrame() {
    if (!s_enabled) return;
    
    std::lock_guard<std::mutex> lock(s_mutex);
    s_samples.clear();
}

void Profiler::EndFrame() {
}

void Profiler::PushMarker(const char* name) {
    if (!s_enabled) return;
    
    MarkerStack marker;
    marker.name = name;
    marker.start = std::chrono::high_resolution_clock::now();
    
    s_markerStack.push_back(marker);
}

void Profiler::PopMarker() {
    if (!s_enabled || s_markerStack.empty()) return;
    
    auto marker = s_markerStack.back();
    s_markerStack.pop_back();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - marker.start).count();
    
    RecordSample(marker.name.c_str(), duration);
}

void Profiler::RecordSample(const char* name, double durationMs) {
    if (!s_enabled) return;
    
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto& data = s_samples[name];
    data.totalDuration += durationMs;
    data.callCount++;
    data.minDuration = std::min(data.minDuration, durationMs);
    data.maxDuration = std::max(data.maxDuration, durationMs);
}

std::vector<ProfileSample> Profiler::GetSamples() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    std::vector<ProfileSample> samples;
    samples.reserve(s_samples.size());
    
    for (const auto& [name, data] : s_samples) {
        ProfileSample sample;
        sample.name = name;
        sample.duration = data.totalDuration;
        sample.callCount = data.callCount;
        sample.minDuration = data.minDuration;
        sample.maxDuration = data.maxDuration;
        sample.avgDuration = data.callCount > 0 ? data.totalDuration / data.callCount : 0.0;
        
        samples.push_back(sample);
    }
    
    std::sort(samples.begin(), samples.end(), [](const ProfileSample& a, const ProfileSample& b) {
        return a.duration > b.duration;
    });
    
    return samples;
}

void Profiler::PrintResults() {
    auto samples = GetSamples();
    
    if (samples.empty()) {
        std::cout << "No profiling data available." << std::endl;
        return;
    }
    
    std::cout << "\n=== PROFILING RESULTS ===" << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << std::left;
    
    std::cout << std::setw(40) << "Name"
              << std::setw(12) << "Total (ms)"
              << std::setw(10) << "Calls"
              << std::setw(12) << "Avg (ms)"
              << std::setw(12) << "Min (ms)"
              << std::setw(12) << "Max (ms)"
              << std::endl;
    
    std::cout << std::string(100, '-') << std::endl;
    
    for (const auto& sample : samples) {
        std::cout << std::setw(40) << sample.name
                  << std::setw(12) << sample.duration
                  << std::setw(10) << sample.callCount
                  << std::setw(12) << sample.avgDuration
                  << std::setw(12) << sample.minDuration
                  << std::setw(12) << sample.maxDuration
                  << std::endl;
    }
    
    std::cout << std::string(100, '-') << std::endl;
    
    double total = 0.0;
    for (const auto& sample : samples) {
        total += sample.duration;
    }
    
    std::cout << "Total frame time: " << total << " ms (" << (1000.0 / total) << " FPS)" << std::endl;
    std::cout << "========================\n" << std::endl;
}

void Profiler::Clear() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_samples.clear();
}

}} // namespace KnC Debug

