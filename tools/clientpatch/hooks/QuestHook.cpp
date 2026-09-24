
#include "QuestHook.h"

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace questhook {
namespace {

// addresses below are absolute in the KnC exe base 0x00400000 so a relocated image must refuse

// evidence pilot cpp already assumes this base and all constants here are VAs
constexpr uintptr_t kImageBase = 0x00400000;

// sub 42CF50 is CTopBar Init with 12 call sites all stage inits
constexpr uintptr_t kTopBarInit = 0x0042CF50;

// evidence 0x0042CF64 MOV EDI ECX and every call site loads 0x00C69880 into ECX
constexpr uintptr_t kTopBarThis = 0x00C69880;

// 0x0042CF72 loads esi from edi plus 0x138 then calls sub 44CC50 the set reset
constexpr uintptr_t kButtonSetOffset = 0x138;

// sub 44C580 appends one button record and returns with ret 0x18
constexpr uintptr_t kButtonAdd = 0x0044C580;

// 0x00B23288 plus 0x384 is the capp stage field read as dword B2360C
constexpr uintptr_t kCurrentStage = 0x00B2360C;

// sub 42CA10 compares eax to 9 at 0x0042CC88 then jumps if zero at 0x0042CC8B
constexpr uintptr_t kGreyGuard = 0x0042CC88;

// second roomcraft 03 grey draw inside the low level block of sub 42CA10
constexpr uintptr_t kLowLevelGrey = 0x0042CB21;

// registration arguments proven from the 03 overlay array and jpt 42BD71

// overlay slot topbar plus 0x6F34 is index 15 and jpt 42BD71 15 switches to stage 26
constexpr int kQuestCmd = 15;
// 0x0042CC91 pushes 0x1C1 and 0x0042CC8F pushes 4 for the quest 03 draw
constexpr int kQuestX = 449;
constexpr int kQuestY = 4;

// overlay slot topbar plus 0x6BA4 is index 9 and jpt 42BD71 9 sends opcode 270
constexpr int kRoomCraftCmd = 9;
// 0x0042CCA8 pushes 0x25E and 0x0042CCA3 pushes 0x2DB for the roomcraft 03 draw
constexpr int kRoomCraftX = 606;
constexpr int kRoomCraftY = 731;

// every stock top bar registration passes zero shrink
constexpr int kNoShrink = 0;

// rdata only holds the 03 png names so the hook owns its own prefixes
const char kQuestPrefix[] = "Menu/Common_Top_Quest_";
const char kRoomCraftPrefix[] = "Menu/Common_Bottom_RoomCraft_";

// 0x0042D072 compares dword B2360C to 4 and jumps if zero skipping the whole main button block
constexpr int kStageChannelSelect = 4;

// sub esp 0x104 then mov eax from the gs cookie the second insn has an absolute operand so rebasing fails
const uint8_t kPrologue[] = {
    0x81, 0xEC, 0x04, 0x01, 0x00, 0x00,
    0xA1, 0x48, 0x75, 0xB0, 0x00
};

// a five byte E9 would split the six byte SUB so steal six and pad with a nop
constexpr size_t kStealLen = 6;
static_assert(sizeof(kPrologue) >= kStealLen, "prologue shorter than steal");

// only two INT3 bytes precede the entry so there is no hotpatch preamble to use
constexpr size_t kStubLen = kStealLen + 5;

// 83 F8 09 CMP EAX 9 then 74 30 JZ 0x0042CCBD
const uint8_t kGreyGuardOrig[] = { 0x83, 0xF8, 0x09, 0x74, 0x30 };
constexpr size_t kGreyJzOffset = 3;
static_assert(sizeof(kGreyGuardOrig) == 5, "grey guard signature must be 5");

// push -1 push 0x2DB push 0x25E lea ecx esi plus 0x6BA4 call sub 4412A0 resolved from the relative offset
const uint8_t kLowLevelOrig[] = {
    0x6A, 0xFF,
    0x68, 0xDB, 0x02, 0x00, 0x00,
    0x68, 0x5E, 0x02, 0x00, 0x00,
    0x8D, 0x8E, 0xA4, 0x6B, 0x00, 0x00,
    0xE8, 0x68, 0x47, 0x01, 0x00
};
static_assert(sizeof(kLowLevelOrig) == 23, "low level grey block must be 23");

// client abi fastcall is byte identical to thiscall here ecx holds this and edx is unused

using TopBarInit_t = char(__fastcall*)(void* self, void* edx, char mode);

using ButtonAdd_t = char(__fastcall*)(void* set, void* edx, const char* prefix,
                                      int x, int y, int cmd, int shrinkW, int shrinkH);

Config g_cfg;
FILE* g_log = nullptr;
char g_err[192] = {};

TopBarInit_t g_realTopBarInit = nullptr;
uint8_t* g_stub = nullptr;

uint8_t g_savedEntry[kStealLen] = {};
bool g_entryArmed = false;

uint8_t g_savedGreyJz = 0;
bool g_greyArmed = false;

uint8_t g_savedLowLevel[sizeof(kLowLevelOrig)] = {};
bool g_lowLevelArmed = false;

// registration runs on every stage change so cap the noise
int g_registerLogsLeft = 4;

void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

bool fail(const char* why) {
    strncpy_s(g_err, sizeof(g_err), why, _TRUNCATE);
    logf("questhook refuse %s", why);
    return false;
}

bool mapped(uintptr_t addr, size_t len) {
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    const uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return addr + len <= end;
}

bool matches(uintptr_t addr, const uint8_t* want, size_t len) {
    return mapped(addr, len) &&
           memcmp(reinterpret_cast<const void*>(addr), want, len) == 0;
}

// own process only never a foreign handle
bool poke(uintptr_t addr, const uint8_t* data, size_t len) {
    void* p = reinterpret_cast<void*>(addr);
    DWORD old = 0;
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(p, data, len);
    VirtualProtect(p, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return true;
}

int currentStage() {
    return *reinterpret_cast<volatile int*>(kCurrentStage);
}

char __fastcall detourTopBarInit(void* self, void*, char mode) {
    // runs the original first since sub 44CC50 zeroes all fifty slots
    const char ok = g_realTopBarInit(self, nullptr, mode);
    if (!ok) return ok;

    // mode one is the waiting room minimal bar our two spots are empty screen
    if (mode != 0) return ok;

    const int stage = currentStage();

    // stock code skips the whole main button block on channel select
    if (stage == kStageChannelSelect) return ok;

    auto add = reinterpret_cast<ButtonAdd_t>(kButtonAdd);
    void* set = static_cast<uint8_t*>(self) + kButtonSetOffset;

    char questOk = 1;
    char craftOk = 1;

    if (g_cfg.quest) {
        questOk = add(set, nullptr, kQuestPrefix,
                      kQuestX, kQuestY, kQuestCmd, kNoShrink, kNoShrink);
    }
    if (g_cfg.roomCraft) {
        craftOk = add(set, nullptr, kRoomCraftPrefix,
                      kRoomCraftX, kRoomCraftY, kRoomCraftCmd, kNoShrink, kNoShrink);
    }

    // helper returns zero only when the hover or press png failed to load
    const bool bad = (g_cfg.quest && !questOk) || (g_cfg.roomCraft && !craftOk);
    if (bad || g_registerLogsLeft > 0) {
        if (g_registerLogsLeft > 0) --g_registerLogsLeft;
        logf("questhook register stage=%d quest=%d roomcraft=%d",
             stage, questOk ? 1 : 0, craftOk ? 1 : 0);
    }
    return ok;
}

bool installEntryHook() {
    if (!matches(kTopBarInit, kPrologue, sizeof(kPrologue)))
        return fail("CTopBar Init prologue mismatch");

    auto* stub = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!stub) return fail("trampoline alloc failed");

    // stolen SUB ESP then jump back past the nop pad never into the body
    memcpy(stub, kPrologue, kStealLen);
    stub[kStealLen] = 0xE9;
    const int32_t back = static_cast<int32_t>(
        (kTopBarInit + kStealLen) - (reinterpret_cast<uintptr_t>(stub) + kStubLen));
    memcpy(stub + kStealLen + 1, &back, sizeof(back));
    FlushInstructionCache(GetCurrentProcess(), stub, kStubLen);

    memcpy(g_savedEntry, reinterpret_cast<const void*>(kTopBarInit), kStealLen);
    g_stub = stub;
    g_realTopBarInit = reinterpret_cast<TopBarInit_t>(stub);

    uint8_t patch[kStealLen];
    patch[0] = 0xE9;
    const int32_t rel = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(&detourTopBarInit) - (kTopBarInit + 5));
    memcpy(patch + 1, &rel, sizeof(rel));
    patch[5] = 0x90;  // sixth stolen byte lowest inbound target is 0x0042CF8B

    if (!poke(kTopBarInit, patch, kStealLen)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        g_stub = nullptr;
        g_realTopBarInit = nullptr;
        return fail("VirtualProtect on CTopBar Init failed");
    }

    g_entryArmed = true;
    logf("questhook hooked CTopBar Init stub %p rel %08X",
         static_cast<void*>(stub), static_cast<unsigned>(rel));
    return true;
}

// registering is not enough since sub 42CA10 paints the grey 03 overlays after the set
void installGreyPatch() {
    if (!matches(kGreyGuard, kGreyGuardOrig, sizeof(kGreyGuardOrig))) {
        logf("questhook grey guard bytes differ leaving overlays on");
        return;
    }
    g_savedGreyJz = kGreyGuardOrig[kGreyJzOffset];
    const uint8_t jmp = 0xEB;  // same displacement byte so it still lands at 0x0042CCBD
    if (!poke(kGreyGuard + kGreyJzOffset, &jmp, 1)) {
        logf("questhook grey guard VirtualProtect failed");
        return;
    }
    g_greyArmed = true;
    logf("questhook grey overlay guard is now unconditional");
}

void installLowLevelPatch() {
    if (!matches(kLowLevelGrey, kLowLevelOrig, sizeof(kLowLevelOrig))) {
        logf("questhook low level grey bytes differ leaving that draw on");
        return;
    }
    memcpy(g_savedLowLevel, kLowLevelOrig, sizeof(kLowLevelOrig));
    uint8_t nops[sizeof(kLowLevelOrig)];
    memset(nops, 0x90, sizeof(nops));
    if (!poke(kLowLevelGrey, nops, sizeof(nops))) {
        logf("questhook low level grey VirtualProtect failed");
        return;
    }
    g_lowLevelArmed = true;
    logf("questhook low level RoomCraft grey draw nopped");
}

}  // namespace

void configure(const Config& cfg) { g_cfg = cfg; }

bool install(const char* logPath) {
    if (!g_cfg.enabled) return true;
    if (g_entryArmed) return true;

    if (logPath && !g_log) fopen_s(&g_log, logPath, "w");

    if (reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) != kImageBase)
        return fail("exe is not at image base 0x00400000");

    // this pointer is always the singleton so a dead page means wrong build
    if (!mapped(kTopBarThis, kButtonSetOffset + 4)) return fail("top bar singleton not mapped");
    if (!mapped(kButtonAdd, 1)) return fail("sub_44C580 not mapped");
    if (!mapped(kCurrentStage, 4)) return fail("stage global not mapped");

    if (!g_cfg.quest && !g_cfg.roomCraft) return fail("nothing to register");

    if (!installEntryHook()) return false;

    if (g_cfg.suppressGreyOverlay) installGreyPatch();
    if (g_cfg.suppressLowLevelGrey) installLowLevelPatch();

    g_err[0] = '\0';
    logf("questhook install ok quest=%d roomcraft=%d grey=%d lowlevel=%d",
         g_cfg.quest ? 1 : 0, g_cfg.roomCraft ? 1 : 0,
         g_greyArmed ? 1 : 0, g_lowLevelArmed ? 1 : 0);
    return true;
}

void shutdown() {
    // caller must be past the game loop a thread inside the detour would fault
    if (g_lowLevelArmed) {
        poke(kLowLevelGrey, g_savedLowLevel, sizeof(g_savedLowLevel));
        g_lowLevelArmed = false;
    }
    if (g_greyArmed) {
        poke(kGreyGuard + kGreyJzOffset, &g_savedGreyJz, 1);
        g_greyArmed = false;
    }
    if (g_entryArmed) {
        poke(kTopBarInit, g_savedEntry, kStealLen);
        g_entryArmed = false;
    }
    if (g_stub) {
        VirtualFree(g_stub, 0, MEM_RELEASE);
        g_stub = nullptr;
    }
    g_realTopBarInit = nullptr;
    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
    }
}

const char* lastError() { return g_err; }

}  // namespace questhook
