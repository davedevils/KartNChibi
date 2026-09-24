#include "config.h"
#include "ring_buffer.h"
#include "hook_engine.h"
#include "sink.h"
#include "poll.h"
#include "bp_engine.h"
#include "bp_queue.h"
#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <windows.h>

using namespace probe;

namespace {
std::unique_ptr<RingBuffer> g_ring;
std::unique_ptr<ProbeConfig> g_config;
std::unique_ptr<BpQueue> g_bp_queue;
HMODULE g_self = nullptr;

std::string dll_dir() {
    char buf[MAX_PATH];
    const DWORD n = GetModuleFileNameA(g_self, buf, MAX_PATH);
    std::string path(buf, n);
    const size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

std::string config_path() {
    return dll_dir() + "probe.cfg";
}

void log_line(const std::string& msg) {
    std::ofstream f(dll_dir() + "knc_probe.log", std::ios::app);
    f << msg << '\n';
}

DWORD WINAPI init_thread(LPVOID) {
    const std::string cfg_path = config_path();
    std::ifstream in(cfg_path, std::ios::binary);
    if (!in) { log_line("probe.cfg not found at " + cfg_path); return 1; }
    std::stringstream ss; ss << in.rdbuf();

    g_config = std::make_unique<ProbeConfig>();
    std::string err;
    if (!parse_config(ss.str(), *g_config, err)) { log_line("config error: " + err); return 1; }

    const HMODULE mod = GetModuleHandleA(g_config->module.c_str());
    if (!mod) { log_line("target module not loaded: " + g_config->module); return 1; }

    rebase_abs_captures(*g_config, reinterpret_cast<uint32_t>(mod));
    g_ring = std::make_unique<RingBuffer>(g_config->ring_capacity ? g_config->ring_capacity : 4096);
    start_sink(*g_ring, *g_config);
    if (!install_hooks(*g_config, *g_ring, reinterpret_cast<uint32_t>(mod))) {
        log_line("install_hooks failed");
        stop_sink();
        return 1;
    }
    start_poll(*g_ring, *g_config);
    g_bp_queue = std::make_unique<BpQueue>(256);
    sink_set_bp_queue(g_bp_queue.get());
    bp_init(g_bp_queue.get());
    log_line("knc_probe armed: " + std::to_string(g_config->hooks.size()) + " hooks");
    return 0;
}
}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = module;
        DisableThreadLibraryCalls(module);
        CloseHandle(CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr));
    } else if (reason == DLL_PROCESS_DETACH) {
        if (lpReserved != nullptr) return TRUE;  // process terminating os reclaims resources do not join or unhook under loader lock
        bp_shutdown();
        sink_set_bp_queue(nullptr);
        stop_poll();
        uninstall_hooks();
        stop_sink();
    }
    return TRUE;
}
