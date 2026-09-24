#include "bp_engine.h"
#include "bp_hit.h"
#include "safe_mem.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>

namespace probe {
namespace {

static constexpr DWORD g_auto_resume_ms = 30000;

struct BpEntry {
    uint32_t addr;
    int      dr;
    bool     active;
    bool     suspend;
    bool     int3;
    uint8_t  orig;            // INT3 saved original byte
    uint64_t hits;
    HANDLE   resume_event;    // suspend manual reset event nullptr if unused
};

// step over correlation keyed by thread id so TF clears and 0xCC rearms on the same thread that stepped
struct StepRec { DWORD tid; uint32_t addr; bool used; };

BpEntry g_bps[16];
StepRec g_steps[16];   // one in flight step per thread sized to match BP capacity
std::mutex g_mutex;
BpQueue* g_queue = nullptr;
PVOID g_veh = nullptr;
std::atomic<uint64_t> g_seq{0};

uint64_t now_us() {
    static const long long f = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x.QuadPart; }();
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (uint64_t)(c.QuadPart / f) * 1000000ull + (uint64_t)(c.QuadPart % f) * 1000000ull / f;
}

// applies DR0 to DR3 and DR7 from g bps to every thread of this process
void apply_hw_to_all_threads() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{}; te.dwSize = sizeof te;
    const DWORD pid = GetCurrentProcessId();
    if (Thread32First(snap, &te)) do {
        if (te.th32OwnerProcessID != pid) continue;
        HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
        if (!th) continue;
        CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(th, &ctx)) {
            DWORD dr7 = 0; DWORD* drN[4] = {&ctx.Dr0, &ctx.Dr1, &ctx.Dr2, &ctx.Dr3};
            *drN[0] = *drN[1] = *drN[2] = *drN[3] = 0;
            for (const BpEntry& e : g_bps) {
                if (e.active && !e.int3 && e.dr >= 0 && e.dr < 4) {
                    *drN[e.dr] = e.addr;
                    dr7 |= (1u << (2 * e.dr));
                }
            }
            ctx.Dr7 = dr7;
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            SetThreadContext(th, &ctx);
        }
        CloseHandle(th);
    } while (Thread32Next(snap, &te));
    CloseHandle(snap);
}

int find_free_dr() {
    bool used[4] = {false,false,false,false};
    for (const BpEntry& e : g_bps) if (e.active && !e.int3 && e.dr >= 0) used[e.dr] = true;
    for (int i = 0; i < 4; i++) if (!used[i]) return i;
    return -1;
}

void fill_hit(BpHit& h, CONTEXT* c, BpEntry& e) {
    h.seq = g_seq.fetch_add(1, std::memory_order_relaxed);
    h.t_us = now_us();
    h.thread_id = GetCurrentThreadId();
    h.addr = e.addr; h.mode = e.suspend ? 1 : 0;
    h.regs[0]=c->Eax; h.regs[1]=c->Ecx; h.regs[2]=c->Edx; h.regs[3]=c->Ebx;
    h.regs[4]=c->Esp; h.regs[5]=c->Ebp; h.regs[6]=c->Esi; h.regs[7]=c->Edi;
    h.eip = c->Eip; h.eflags = c->EFlags;
    h.stack_len = (uint32_t)safe_read(reinterpret_cast<void*>(c->Esp), h.stack, kBpStackBytes);
    h.code_len  = (uint32_t)safe_read(reinterpret_cast<void*>(c->Eip), h.code, kBpCodeBytes);
}

// writes a single byte to executable memory VirtualProtect to RWX then restore
bool write_byte(uint32_t addr, uint8_t byte) {
    void* p = reinterpret_cast<void*>(addr);
    DWORD old_prot = 0;
    if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old_prot)) {
        OutputDebugStringA("hbo_probe: VirtualProtect failed for INT3 byte write\n");
        return false;
    }
    *reinterpret_cast<uint8_t*>(addr) = byte;
    VirtualProtect(p, 1, old_prot, &old_prot);
    return true;
}

// records a pending step over for this thread returns false if no free slot
bool record_step(DWORD tid, uint32_t addr) {
    for (StepRec& s : g_steps) if (!s.used) { s = StepRec{tid, addr, true}; return true; }
    OutputDebugStringA("hbo_probe: no free INT3 step slot\n");
    return false;
}

StepRec* find_step(DWORD tid) {
    for (StepRec& s : g_steps) if (s.used && s.tid == tid) return &s;
    return nullptr;
}

// suspend wait dups the event under lock so the waiter never touches the original after unlocking
void suspend_wait(std::unique_lock<std::mutex>& lock, HANDLE original_event) {
    HANDLE dup = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), original_event, GetCurrentProcess(),
                         &dup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        OutputDebugStringA("hbo_probe: DuplicateHandle failed, skipping suspend wait\n");
        return;  // lock stays held treat as immediate resume
    }
    ResetEvent(original_event);
    lock.unlock();
    const DWORD w = WaitForSingleObject(dup, g_auto_resume_ms);
    if (w == WAIT_TIMEOUT) OutputDebugStringA("hbo_probe: suspend auto-resumed (timeout)\n");
    else if (w == WAIT_FAILED) OutputDebugStringA("hbo_probe: suspend wait failed\n");
    CloseHandle(dup);
    lock.lock();
}

LONG CALLBACK veh(EXCEPTION_POINTERS* ep) {
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_SINGLE_STEP && code != EXCEPTION_BREAKPOINT)
        return EXCEPTION_CONTINUE_SEARCH;
    static thread_local bool in_veh = false;
    if (in_veh) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = ep->ContextRecord;
    const DWORD tid = GetCurrentThreadId();
    std::unique_lock<std::mutex> lock(g_mutex, std::try_to_lock);
    if (!lock.owns_lock()) return EXCEPTION_CONTINUE_SEARCH;

    if (code == EXCEPTION_BREAKPOINT) {
        // windows sets Eip to point at the 0xCC not after it
        const uint32_t trap_addr = c->Eip;
        BpEntry* hit = nullptr;
        for (BpEntry& e : g_bps) if (e.active && e.int3 && e.addr == trap_addr) { hit = &e; break; }
        if (!hit) return EXCEPTION_CONTINUE_SEARCH;

        in_veh = true;
        ++hit->hits;
        BpHit rec{}; fill_hit(rec, c, *hit);
        rec.code[0] = hit->orig;   // dump the real instruction byte not the 0xCC trap
        if (g_queue) g_queue->try_push(rec);

        // capture step fields while locked a concurrent disarm may invalidate hit so do not deref it after
        const uint32_t step_addr = hit->addr;
        const uint8_t  step_orig = hit->orig;
        const bool     do_suspend = hit->suspend && hit->resume_event;
        const HANDLE   ev = hit->resume_event;

        if (do_suspend) suspend_wait(lock, ev);

        // step over removes the 0xCC globally for one instruction so another thread may miss a hit there
        write_byte(step_addr, step_orig);
        if (record_step(tid, step_addr)) {
            c->EFlags |= 0x100;    // TF fires single step after this instruction
        } else {
            write_byte(step_addr, 0xCC);  // cant track so rearm immediately and skip the step
        }

        lock.unlock();
        in_veh = false;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // single step case one this thread stepped over a restored instruction so rearm INT3
    if (StepRec* s = find_step(tid)) {
        const uint32_t a = s->addr;
        BpEntry* e = nullptr;
        for (BpEntry& b : g_bps) if (b.active && b.int3 && b.addr == a) { e = &b; break; }
        if (e) write_byte(a, 0xCC);          // rearm if not disarmed meanwhile consume the record and clear TF
        s->used = false;
        c->EFlags &= ~0x100u;
        lock.unlock();
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // case two a normal hardware breakpoint single step from DR
    BpEntry* hit = nullptr;
    for (BpEntry& e : g_bps) if (e.active && !e.int3 && e.addr == c->Eip) { hit = &e; break; }
    if (!hit) return EXCEPTION_CONTINUE_SEARCH;

    in_veh = true;
    ++hit->hits;
    BpHit rec{}; fill_hit(rec, c, *hit);
    if (g_queue) g_queue->try_push(rec);
    c->Dr6 = 0;
    c->EFlags |= 0x10000;   // RF stops retrapping the same instruction

    // capture before any wait a concurrent disarm may invalidate hit
    const bool   hw_suspend = hit->suspend && hit->resume_event;
    const HANDLE hw_ev = hit->resume_event;
    if (hw_suspend) suspend_wait(lock, hw_ev);

    lock.unlock();
    in_veh = false;
    return EXCEPTION_CONTINUE_EXECUTION;
}
}  // namespace

void bp_init(BpQueue* queue) {
    { std::lock_guard<std::mutex> lock(g_mutex);
      g_queue = queue;
      for (BpEntry& e : g_bps) { e = BpEntry{}; e.dr = -1; e.resume_event = nullptr; }
      for (StepRec& s : g_steps) s = StepRec{};
      apply_hw_to_all_threads();
    }
    if (!g_veh) g_veh = AddVectoredExceptionHandler(1, veh);
}

void bp_shutdown() {
    { std::lock_guard<std::mutex> lock(g_mutex);
      for (BpEntry& e : g_bps) {
          if (!e.active) continue;
          if (e.int3) write_byte(e.addr, e.orig);
          if (e.resume_event) { SetEvent(e.resume_event); CloseHandle(e.resume_event); }
          e = BpEntry{}; e.dr = -1;
      }
      // a thread mid step at shutdown is a rare explicit unload edge acceptable
      for (StepRec& s : g_steps) s = StepRec{};
      apply_hw_to_all_threads();
    }
    if (g_veh) { RemoveVectoredExceptionHandler(g_veh); g_veh = nullptr; }
    g_queue = nullptr;
}

bool bp_arm(uint32_t addr, bool suspend, bool int3) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const BpEntry& e : g_bps) if (e.active && e.addr == addr) return false;  // dup
    int slot = -1; for (int i = 0; i < 16; i++) if (!g_bps[i].active) { slot = i; break; }
    if (slot < 0) return false;

    BpEntry& e = g_bps[slot];
    e = BpEntry{};
    e.addr = addr;
    e.active = true;
    e.suspend = suspend;
    e.int3 = int3;
    e.hits = 0;
    e.resume_event = nullptr;

    if (suspend) {
        e.resume_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);  // manual reset non signaled
        if (!e.resume_event) { e = BpEntry{}; e.dr = -1; return false; }
    }

    if (int3) {
        e.dr = -1;
        if (safe_read(reinterpret_cast<void*>(addr), &e.orig, 1) != 1) {
            if (e.resume_event) CloseHandle(e.resume_event);
            e = BpEntry{}; e.dr = -1;
            return false;
        }
        if (!write_byte(addr, 0xCC)) {
            if (e.resume_event) CloseHandle(e.resume_event);
            e = BpEntry{}; e.dr = -1;
            return false;
        }
        // INT3 is global so no per thread DR manipulation is needed
        return true;
    }

    int dr = find_free_dr();
    if (dr < 0) {
        if (e.resume_event) CloseHandle(e.resume_event);
        e = BpEntry{}; e.dr = -1;
        return false;
    }
    e.dr = dr;
    apply_hw_to_all_threads();
    return true;
}

bool bp_disarm(uint32_t addr) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (BpEntry& e : g_bps) {
        if (!e.active || e.addr != addr) continue;
        // do not touch g steps a mid step thread still needs its debug exception to clear TF
        if (e.int3) write_byte(e.addr, e.orig);
        if (e.resume_event) { SetEvent(e.resume_event); CloseHandle(e.resume_event); }
        e = BpEntry{}; e.dr = -1;
        apply_hw_to_all_threads();
        return true;
    }
    return false;
}

void bp_resume(uint32_t addr, bool all) {
    std::lock_guard<std::mutex> lock(g_mutex);
    // SetEvent only since it is manual reset the waiter resets under lock before waiting so this cannot be lost
    for (BpEntry& e : g_bps) {
        if (!e.active || !e.suspend || !e.resume_event) continue;
        if (all || e.addr == addr) {
            SetEvent(e.resume_event);
            if (!all) return;
        }
    }
}

std::string bp_list() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string s;
    for (const BpEntry& e : g_bps) if (e.active) {
        char line[96];
        std::snprintf(line, sizeof line, "0x%08x %s %s hits=%llu\n",
                      e.addr, e.suspend ? "suspend" : "trace", e.int3 ? "int3" : "hw",
                      (unsigned long long)e.hits);
        s += line;
    }
    return s;
}
}  // namespace probe
