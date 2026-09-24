#include "hook_engine.h"
#include "capture.h"
#include "probe_record.h"
#include <atomic>
#include <windows.h>
#include "MinHook.h"

namespace probe {

namespace {
RingBuffer* g_ring = nullptr;
const ProbeConfig* g_cfg = nullptr;
std::atomic<uint64_t> g_seq{0};
void* g_trampolines[kMaxHooks] = {nullptr};

uint64_t now_us() {
    static const long long freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<uint64_t>(c.QuadPart / freq) * 1000000ull +
           static_cast<uint64_t>(c.QuadPart % freq) * 1000000ull / freq;
}
}  // namespace

// called by each naked detour the first stack slot is the return address then each arg
extern "C" void __cdecl probe_dispatch(uint32_t hook_index, uint32_t eax,
                                       uint32_t ecx, uint32_t edx, uint32_t* stack) {
    static thread_local bool in_hook = false;
    if (in_hook) return;                              // reentrancy guard
    in_hook = true;

    const HookDesc& hook = g_cfg->hooks[hook_index];
    ProbeRecord rec{};
    rec.seq = g_seq.fetch_add(1, std::memory_order_relaxed);
    rec.t_us = now_us();
    rec.thread_id = GetCurrentThreadId();
    rec.hook_id = static_cast<uint16_t>(hook_index);
    rec.opcode = kNoOpcode;

    HookContext ctx{};
    ctx.eax = eax; ctx.ecx = ecx; ctx.edx = edx; ctx.stack = stack;

    uint8_t n = 0;
    for (const CaptureDesc& cap : hook.captures) {
        if (n >= kMaxCapturesPerHook) break;
        resolve_capture(cap, ctx, g_cfg->max_capture_bytes, rec.blobs[n]);
        if (cap.label == "opcode" && rec.blobs[n].size >= 1) {
            uint16_t op = 0;
            for (uint32_t i = 0; i < rec.blobs[n].size && i < 2; i++)
                op |= static_cast<uint16_t>(rec.blobs[n].data[i]) << (8 * i);
            rec.opcode = op;
        }
        ++n;
    }
    rec.blob_count = n;
    g_ring->try_push(rec);
    in_hook = false;
}

// one naked detour per hook index registers are pushed in a fixed order after pushad and pushfd
#pragma warning(push)
#pragma warning(disable: 4731 4740)

__declspec(naked) static void detour_0() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  0
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 0*4]
    }
}
__declspec(naked) static void detour_1() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  1
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 1*4]
    }
}
__declspec(naked) static void detour_2() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  2
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 2*4]
    }
}
__declspec(naked) static void detour_3() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  3
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 3*4]
    }
}
__declspec(naked) static void detour_4() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  4
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 4*4]
    }
}
__declspec(naked) static void detour_5() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  5
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 5*4]
    }
}
__declspec(naked) static void detour_6() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  6
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 6*4]
    }
}
__declspec(naked) static void detour_7() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  7
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 7*4]
    }
}
__declspec(naked) static void detour_8() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  8
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 8*4]
    }
}
__declspec(naked) static void detour_9() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  9
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 9*4]
    }
}
__declspec(naked) static void detour_10() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  10
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 10*4]
    }
}
__declspec(naked) static void detour_11() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  11
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 11*4]
    }
}
__declspec(naked) static void detour_12() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  12
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 12*4]
    }
}
__declspec(naked) static void detour_13() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  13
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 13*4]
    }
}
__declspec(naked) static void detour_14() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  14
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 14*4]
    }
}
__declspec(naked) static void detour_15() {
    __asm {
        pushad
        pushfd
        mov   eax, [esp+32]
        mov   ecx, [esp+28]
        mov   edx, [esp+24]
        lea   ebx, [esp+36]
        push  ebx
        push  edx
        push  ecx
        push  eax
        push  15
        call  probe_dispatch
        add   esp, 20
        popfd
        popad
        jmp   dword ptr [g_trampolines + 15*4]
    }
}

#pragma warning(pop)

static void* const g_detours[kMaxHooks] = {
    detour_0, detour_1, detour_2, detour_3, detour_4, detour_5, detour_6, detour_7,
    detour_8, detour_9, detour_10, detour_11, detour_12, detour_13, detour_14, detour_15,
};

bool install_hooks(const ProbeConfig& cfg, RingBuffer& ring, uint32_t runtime_base) {
    if (cfg.hooks.size() > static_cast<size_t>(kMaxHooks)) return false;
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
    g_ring = &ring;
    g_cfg = &cfg;

    for (int i = 0; i < static_cast<int>(cfg.hooks.size()); i++) {
        if (cfg.hooks[i].is_poll) continue;   // polls are not MinHook-installed
        void* target = reinterpret_cast<void*>(
            rebase_address(cfg.hooks[i].address, cfg.image_base, runtime_base));
        if (MH_CreateHook(target, g_detours[i], &g_trampolines[i]) != MH_OK) {
            uninstall_hooks();  // tear down earlier hooks so the client stays unpatched
            return false;
        }
        if (MH_EnableHook(target) != MH_OK) {
            uninstall_hooks();
            return false;
        }
    }
    return true;
}

void uninstall_hooks() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    for (int i = 0; i < kMaxHooks; i++) g_trampolines[i] = nullptr;
    g_ring = nullptr;
    g_cfg = nullptr;
    g_seq.store(0, std::memory_order_relaxed);
}

}  // namespace probe
