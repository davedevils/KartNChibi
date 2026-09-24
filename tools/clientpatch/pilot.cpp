/// cursor is pinned not lParam sub 404D60 routes via sub 447800 to sub 409500 for one send

#include "pilot.h"

#include <windows.h>
#include <objidl.h>  // gdiplus needs IStream and lean windows strips it

#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }  // NOMINMAX breaks gdiplus
#include <gdiplus.h>

#include <d3d9.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

// linker gives this it is our own dll base
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace pilot {
namespace {

namespace addr {

// FUN 00404410 is SetUIState arg is the stage index
constexpr uintptr_t kSetUIState = 0x00404410;
constexpr uintptr_t kStageCtx   = 0x00B23288;
constexpr uintptr_t kStage      = 0x00B2360C;   // kStageCtx plus 900 kInputGate is kStageCtx plus 908
constexpr uintptr_t kInputGate  = 0x00B23614;

constexpr uintptr_t kDialogShown = 0x00F727F0;
constexpr uintptr_t kDialogType  = 0x00F727F4;

constexpr uintptr_t kSelChar = 0x01A20B18;
constexpr uintptr_t kSelKart = 0x01A20B1C;

// key poll 0x44B4C0 loads GetAsyncKeyState from this iat slot for every vk
constexpr uintptr_t kAsyncKeySlot = 0x0059F350;
// input manager object held byte at plus 4 plus vk edge int at plus 0x104 plus vk times 4
constexpr uintptr_t kInputMgr = 0x005DEAF0;

// game object car N at plus N times stride local index at plus 0x6B0
constexpr uintptr_t kGameBase  = 0x01B19090;
constexpr size_t    kCarStride = 0xA7260;
constexpr size_t    kCarCap    = 30;

constexpr uintptr_t kOwnChars      = 0x01A56BD0;
constexpr size_t    kOwnCharStride = 0x2C;
constexpr size_t    kOwnCharCount  = 0xB04;
constexpr size_t    kOwnCharCap    = 64;

constexpr uintptr_t kOwnKarts      = 0x01A576D8;
constexpr size_t    kOwnKartStride = 0x38;
constexpr size_t    kOwnKartCount  = 0xE04;
constexpr size_t    kOwnKartCap    = 64;

// only id at plus four marks a live row driver key stays zero on this build
struct Catalog {
    uintptr_t base;
    size_t stride;
    size_t idOff;
    size_t keyOff;
    size_t nameOff;   // zero means record has no name
    size_t cap;
};

constexpr Catalog kDrvCat = { 0x01A20B30, 0xD8,  0x04, 0x0C, 0x18, 32 };
constexpr Catalog kVehCat = { 0x01A22638, 0x140, 0x04, 0x08, 0,    64 };

// cursor object offsets read off sub 44b6e0 and sub 44b830
constexpr uintptr_t kCursorObj  = 0x00E52688;
constexpr uintptr_t kCursorHwnd = kCursorObj + 308;
constexpr uintptr_t kCursorX    = kCursorObj + 344;
constexpr uintptr_t kCursorY    = kCursorObj + 348;
constexpr uintptr_t kCursorBtn  = kCursorObj + 352;

// viewport offsets read off sub 43d650 and sub 43d690
constexpr uintptr_t kViewHwnd   = 0x00D6E190;
constexpr uintptr_t kViewW      = 0x00D6E1A8;
constexpr uintptr_t kViewH      = 0x00D6E1AC;
constexpr uintptr_t kViewScaleX = 0x00D6E1E0;
constexpr uintptr_t kViewScaleY = 0x00D6E1E4;
constexpr uintptr_t kViewMode   = 0x00D6E1E8;

struct Named { const char* name; uintptr_t va; size_t len; };

const Named kNamed[] = {
    { "stage",       kStage,       4 },
    { "stagectx",    kStageCtx,    4 },
    { "inputgate",   kInputGate,   1 },
    { "dialog",      kDialogShown, 1 },
    { "dialogtype",  kDialogType,  4 },
    { "selchar",     kSelChar,     4 },
    { "selkart",     kSelKart,     4 },
    { "ownchars",    kOwnChars,    4 },
    { "ownkarts",    kOwnKarts,    4 },
    { "drivercat",   kDrvCat.base, 4 },
    { "vehiclecat",  kVehCat.base, 4 },
    { "cursorx",     kCursorX,     4 },
    { "cursory",     kCursorY,     4 },
    { "cursorobj",   kCursorObj,   4 },
    { "vieww",       kViewW,       4 },
    { "viewh",       kViewH,       4 },
    { "viewscalex",  kViewScaleX,  4 },
    { "viewscaley",  kViewScaleY,  4 },
    { "viewmode",    kViewMode,    1 },
    { "asynckeyslot", kAsyncKeySlot, 4 },
    { "inputmgr",    kInputMgr,    4 },
    { "game",        kGameBase,    4 },
    { "localcar",    kGameBase + 0x6B0, 4 },
};

}  // namespace addr

using SetUIState_t = void(__cdecl*)(void* ctx, int state);

constexpr wchar_t kWindowTitle[] = L"Chibi Kart";
constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\knc_pilot";
constexpr UINT WM_PILOT_DRAIN = WM_APP + 0x51;

Config g_cfg;
std::atomic<bool> g_running{false};

std::atomic<HWND> g_hwnd{nullptr};
WNDPROC g_prevProc = nullptr;
ULONG_PTR g_gdiplusToken = 0;

bool mapped(uintptr_t va, size_t len, bool needWrite) {
    if (!va || !len) return false;
    MEMORY_BASIC_INFORMATION mbi = {};
    uintptr_t end = va + len;
    uintptr_t cur = va;
    while (cur < end) {
        if (!VirtualQuery(reinterpret_cast<void*>(cur), &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        const DWORD p = mbi.Protect;
        if (p & (PAGE_NOACCESS | PAGE_GUARD)) return false;
        if (needWrite && !(p & (PAGE_READWRITE | PAGE_WRITECOPY |
                                PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            return false;
        cur = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}

// seh needs body with no unwinding objects
bool copyGuarded(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool readMem(uintptr_t va, void* dst, size_t n) {
    if (!mapped(va, n, false)) return false;
    return copyGuarded(dst, reinterpret_cast<const void*>(va), n);
}

bool writeMem(uintptr_t va, const void* src, size_t n) {
    if (!mapped(va, n, true)) return false;
    return copyGuarded(reinterpret_cast<void*>(va), src, n);
}

int32_t readI32(uintptr_t va, int32_t fallback = 0) {
    int32_t v = fallback;
    return readMem(va, &v, 4) ? v : fallback;
}

uint8_t readU8(uintptr_t va, uint8_t fallback = 0) {
    uint8_t v = fallback;
    return readMem(va, &v, 1) ? v : fallback;
}

float readF32(uintptr_t va, float fallback = 1.0f) {
    float v = fallback;
    return readMem(va, &v, 4) ? v : fallback;
}

int readStage() { return readI32(addr::kStage, -1); }

struct Frame {
    std::vector<uint8_t> bgra;  // top down stride w times four
    int w = 0;
    int h = 0;
    bool valid() const { return w > 0 && h > 0 && bgra.size() >= size_t(w) * h * 4; }
};

// near flat image means compositor gave nothing
bool looksBlank(const Frame& f) {
    if (!f.valid()) return true;
    const uint32_t* px = reinterpret_cast<const uint32_t*>(f.bgra.data());
    const uint32_t first = px[0] & 0x00FFFFFFu;
    const int stepX = f.w > 128 ? f.w / 128 : 1;
    const int stepY = f.h > 128 ? f.h / 128 : 1;
    int total = 0, same = 0;
    for (int y = 0; y < f.h; y += stepY) {
        for (int x = 0; x < f.w; x += stepX) {
            ++total;
            if ((px[size_t(y) * f.w + x] & 0x00FFFFFFu) == first) ++same;
        }
    }
    // minimized window leaks title stub so one odd pixel is not content
    return total == 0 || same * 100 >= total * 99;
}

bool pngEncoder(CLSID* out) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (!size) return false;
    std::vector<uint8_t> blob(size);
    auto* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(blob.data());
    Gdiplus::GetImageEncoders(num, size, info);
    for (UINT i = 0; i < num; ++i)
        if (wcscmp(info[i].MimeType, L"image/png") == 0) { *out = info[i].Clsid; return true; }
    return false;
}

bool savePng(const Frame& f, const std::string& path, std::string* err) {
    CLSID png{};
    if (!pngEncoder(&png)) { *err = "no png encoder"; return false; }
    Gdiplus::Bitmap bmp(f.w, f.h, f.w * 4, PixelFormat32bppRGB,
                        const_cast<BYTE*>(f.bgra.data()));
    if (bmp.GetLastStatus() != Gdiplus::Ok) { *err = "bitmap wrap failed"; return false; }
    const int need = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(need > 0 ? need - 1 : 0, L'\0');
    if (need > 0) MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &wpath[0], need);
    if (bmp.Save(wpath.c_str(), &png, nullptr) != Gdiplus::Ok) { *err = "png save failed"; return false; }
    return true;
}

// game thread only PrintWindow reenters the window proc
bool gdiGrab(HWND h, Frame* out, std::string* method, std::string* err) {
    RECT rc{};
    if (!GetClientRect(h, &rc)) { *err = "GetClientRect failed"; return false; }
    const int w = rc.right - rc.left, hgt = rc.bottom - rc.top;

    // minimized has no live surface gdi hands back black stub
    if (IsIconic(h)) { *err = "window minimized so gdi has no client surface"; return false; }
    if (w <= 0 || hgt <= 0) { *err = "empty client rect"; return false; }

    HDC src = GetDC(h);
    if (!src) { *err = "GetDC failed"; return false; }
    HDC mem = CreateCompatibleDC(src);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -hgt;  // top down so rows match gdiplus
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(src, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);

        struct Attempt { UINT flags; bool print; const char* name; };
        const Attempt tries[] = {
            { 0x00000003u, true,  "printwindow-full" },  // PW CLIENTONLY or PW RENDERFULLCONTENT
            { 0x00000001u, true,  "printwindow" },
            { 0u,          false, "bitblt" },
        };

        for (const Attempt& a : tries) {
            memset(bits, 0, size_t(w) * hgt * 4);
            const BOOL got = a.print ? PrintWindow(h, mem, a.flags)
                                     : BitBlt(mem, 0, 0, w, hgt, src, 0, 0, SRCCOPY);
            if (!got) continue;
            GdiFlush();
            out->w = w;
            out->h = hgt;
            out->bgra.assign(static_cast<uint8_t*>(bits),
                             static_cast<uint8_t*>(bits) + size_t(w) * hgt * 4);
            if (looksBlank(*out)) { *method = a.name; continue; }
            *method = a.name;
            ok = true;
            break;
        }
        if (!ok) *err = "all gdi paths returned a flat image";

        SelectObject(mem, old);
    } else {
        *err = "CreateDIBSection failed";
    }

    if (bmp) DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(h, src);
    return ok;
}

using Present_t = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using CreateDevice_t = HRESULT(__stdcall*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
                                           D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using D3DCreate9_t = IDirect3D9*(WINAPI*)(UINT);

constexpr int kMaxPresent = 4;
Present_t g_realPresent[kMaxPresent] = {};
void* g_hookedVt[kMaxPresent] = {};
std::atomic<int> g_presentHooks{0};

CreateDevice_t g_realCreateDevice = nullptr;
D3DCreate9_t g_realD3DCreate9 = nullptr;

std::atomic<bool> g_capArm{false};
std::atomic<bool> g_capDone{false};
std::mutex g_capMu;
Frame g_capFrame;
std::string g_capErr;
std::atomic<int> g_presentSeen{0};
std::atomic<int> g_d3dCreates{0};
std::atomic<int> g_devCreates{0};

void grabBackbuffer(IDirect3DDevice9* dev);
IDirect3D9* WINAPI hookD3DCreate9(UINT ver);

void onPresent(IDirect3DDevice9* dev) {
    ++g_presentSeen;
    if (g_capArm.exchange(false)) grabBackbuffer(dev);
}

// one thunk per vtable d3d9 has several device classes
template <int I>
HRESULT __stdcall hookPresentN(IDirect3DDevice9* dev, const RECT* a, const RECT* b,
                               HWND c, const RGNDATA* d) {
    onPresent(dev);
    return g_realPresent[I](dev, a, b, c, d);
}

using SCPresent_t = HRESULT(__stdcall*)(IDirect3DSwapChain9*, const RECT*, const RECT*,
                                        HWND, const RGNDATA*, DWORD);
SCPresent_t g_realSCPresent = nullptr;
std::atomic<int> g_scPresents{0};

// engine can present through swap chain not device
HRESULT __stdcall hookSCPresent(IDirect3DSwapChain9* sc, const RECT* a, const RECT* b,
                                HWND c, const RGNDATA* d, DWORD f) {
    ++g_scPresents;
    if (g_capArm.load()) {
        IDirect3DDevice9* dev = nullptr;
        if (SUCCEEDED(sc->GetDevice(&dev)) && dev) {
            onPresent(dev);
            dev->Release();
        }
    }
    return g_realSCPresent(sc, a, b, c, d, f);
}

using EndScene_t = HRESULT(__stdcall*)(IDirect3DDevice9*);
EndScene_t g_realEndScene = nullptr;
std::atomic<int> g_endScenes{0};

// some engines never call device Present so watch EndScene too
HRESULT __stdcall hookEndScene(IDirect3DDevice9* dev) {
    ++g_endScenes;
    if (g_capArm.load()) onPresent(dev);
    return g_realEndScene(dev);
}

void* const kPresentThunks[kMaxPresent] = {
    reinterpret_cast<void*>(&hookPresentN<0>), reinterpret_cast<void*>(&hookPresentN<1>),
    reinterpret_cast<void*>(&hookPresentN<2>), reinterpret_cast<void*>(&hookPresentN<3>),
};

bool patchSlot(void** vtable, int index, void* hook, void** saveOrig) {
    if (*saveOrig) return true;
    DWORD old = 0;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE, &old)) return false;
    *saveOrig = vtable[index];
    vtable[index] = hook;
    VirtualProtect(&vtable[index], sizeof(void*), old, &old);
    return true;
}

void grabBackbuffer(IDirect3DDevice9* dev) {
    Frame f;
    std::string err;

    IDirect3DSurface9* back = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || !back) {
        err = "GetBackBuffer failed";
    } else {
        D3DSURFACE_DESC d = {};
        back->GetDesc(&d);
        IDirect3DSurface9* sys = nullptr;
        if (FAILED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format,
                                                    D3DPOOL_SYSTEMMEM, &sys, nullptr)) || !sys) {
            err = "CreateOffscreenPlainSurface failed";
        } else {
            HRESULT hr = dev->GetRenderTargetData(back, sys);
            if (FAILED(hr)) {
                // multisampled backbuffer cannot copy straight down
                IDirect3DSurface9* rt = nullptr;
                if (SUCCEEDED(dev->CreateRenderTarget(d.Width, d.Height, d.Format,
                                                      D3DMULTISAMPLE_NONE, 0, FALSE, &rt, nullptr)) && rt) {
                    if (SUCCEEDED(dev->StretchRect(back, nullptr, rt, nullptr, D3DTEXF_NONE)))
                        hr = dev->GetRenderTargetData(rt, sys);
                    rt->Release();
                }
            }
            if (FAILED(hr)) {
                err = "GetRenderTargetData failed";
            } else if (d.Format != D3DFMT_X8R8G8B8 && d.Format != D3DFMT_A8R8G8B8) {
                err = "backbuffer format not 32 bit rgb";
            } else {
                D3DLOCKED_RECT lr = {};
                if (FAILED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                    err = "LockRect failed";
                } else {
                    f.w = int(d.Width);
                    f.h = int(d.Height);
                    f.bgra.resize(size_t(f.w) * f.h * 4);
                    for (int y = 0; y < f.h; ++y)
                        memcpy(f.bgra.data() + size_t(y) * f.w * 4,
                               static_cast<const uint8_t*>(lr.pBits) + size_t(y) * lr.Pitch,
                               size_t(f.w) * 4);
                    sys->UnlockRect();
                }
            }
            sys->Release();
        }
        back->Release();
    }

    {
        std::lock_guard<std::mutex> lk(g_capMu);
        g_capFrame = std::move(f);
        g_capErr = err;
    }
    g_capDone = true;
}

// same vtable twice means nothing new to patch
bool hookDeviceVtable(IDirect3DDevice9* dev) {
    void** vt = *reinterpret_cast<void***>(dev);
    for (int i = 0; i < kMaxPresent; ++i)
        if (g_hookedVt[i] == vt) return false;
    const int slot = g_presentHooks.load();
    if (slot >= kMaxPresent) return false;
    void* orig = nullptr;
    if (!patchSlot(vt, 17, kPresentThunks[slot], &orig) || !orig) return false;
    g_realPresent[slot] = reinterpret_cast<Present_t>(orig);
    g_hookedVt[slot] = vt;
    g_presentHooks = slot + 1;
    return true;
}

HRESULT __stdcall hookCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND focus,
                                   DWORD flags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out) {
    ++g_devCreates;
    const HRESULT hr = g_realCreateDevice(self, adapter, type, focus, flags, pp, out);
    if (SUCCEEDED(hr) && out && *out) {
        hookDeviceVtable(*out);
        patchSlot(*reinterpret_cast<void***>(*out), 42, &hookEndScene,
                  reinterpret_cast<void**>(&g_realEndScene));
        IDirect3DSwapChain9* sc = nullptr;
        if (SUCCEEDED((*out)->GetSwapChain(0, &sc)) && sc) {
            patchSlot(*reinterpret_cast<void***>(sc), 3, &hookSCPresent,
                      reinterpret_cast<void**>(&g_realSCPresent));
            sc->Release();
        }
    }
    return hr;
}

IDirect3D9* WINAPI hookD3DCreate9(UINT ver) {
    ++g_d3dCreates;
    IDirect3D9* d3d = g_realD3DCreate9(ver);
    if (d3d)
        patchSlot(*reinterpret_cast<void***>(d3d), 16, &hookCreateDevice,
                  reinterpret_cast<void**>(&g_realCreateDevice));
    return d3d;
}

// probe device vtable is not the client one so patch the export itself
uint8_t* g_d3dTramp = nullptr;
std::atomic<bool> g_inlineHooked{false};

bool inlineHookD3DCreate9() {
    if (g_inlineHooked) return true;
    HMODULE m = GetModuleHandleW(L"d3d9.dll");
    if (!m) return false;
    auto* target = reinterpret_cast<uint8_t*>(GetProcAddress(m, "Direct3DCreate9"));
    if (!target) return false;

    // only hotpatch prologue mov edi edi push ebp mov ebp esp
    static const uint8_t kProlog[5] = { 0x8B, 0xFF, 0x55, 0x8B, 0xEC };
    if (memcmp(target, kProlog, sizeof(kProlog)) != 0) return false;

    auto* tramp = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) return false;
    memcpy(tramp, target, 5);
    tramp[5] = 0xE9;
    *reinterpret_cast<int32_t*>(tramp + 6) = int32_t((target + 5) - (tramp + 10));

    DWORD old = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(tramp, 0, MEM_RELEASE); return false; }
    target[0] = 0xE9;
    *reinterpret_cast<int32_t*>(target + 1) =
        int32_t(reinterpret_cast<uint8_t*>(&hookD3DCreate9) - (target + 5));
    VirtualProtect(target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 5);

    g_d3dTramp = tramp;
    g_realD3DCreate9 = reinterpret_cast<D3DCreate9_t>(tramp);
    g_inlineHooked = true;
    return true;
}

using LoadLibraryA_t = HMODULE(WINAPI*)(LPCSTR);
using LoadLibraryW_t = HMODULE(WINAPI*)(LPCWSTR);
using LoadLibraryExA_t = HMODULE(WINAPI*)(LPCSTR, HANDLE, DWORD);
using LoadLibraryExW_t = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);

LoadLibraryA_t g_realLLA = nullptr;
LoadLibraryW_t g_realLLW = nullptr;
LoadLibraryExA_t g_realLLExA = nullptr;
LoadLibraryExW_t g_realLLExW = nullptr;
std::atomic<int> g_armed{0};

void armModule(HMODULE mod);

// patch export before caller can look it up
void afterLoad(HMODULE m) {
    if (!m) return;
    inlineHookD3DCreate9();
    armModule(m);
}

HMODULE WINAPI hookLoadLibraryA(LPCSTR n) { HMODULE m = g_realLLA(n); afterLoad(m); return m; }
HMODULE WINAPI hookLoadLibraryW(LPCWSTR n) { HMODULE m = g_realLLW(n); afterLoad(m); return m; }
HMODULE WINAPI hookLoadLibraryExA(LPCSTR n, HANDLE f, DWORD g) {
    HMODULE m = g_realLLExA(n, f, g);
    afterLoad(m);
    return m;
}
HMODULE WINAPI hookLoadLibraryExW(LPCWSTR n, HANDLE f, DWORD g) {
    HMODULE m = g_realLLExW(n, f, g);
    afterLoad(m);
    return m;
}

// swap one named import slot bound imports fall back to value match
bool patchIat(HMODULE mod, const char* fname, void* hook, void** origOut) {
    if (!mod) return false;
    auto base = reinterpret_cast<uint8_t*>(mod);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return false;

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    const ULONG_PTR want = k32 ? reinterpret_cast<ULONG_PTR>(GetProcAddress(k32, fname)) : 0;

    auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
    for (; imp->Name; ++imp) {
        if (!imp->FirstThunk) continue;
        auto* live = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);
        auto* orig = imp->OriginalFirstThunk
                         ? reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->OriginalFirstThunk)
                         : nullptr;
        for (; live->u1.Function; ++live) {
            bool hit = false;
            if (orig && orig->u1.AddressOfData) {
                if (!IMAGE_SNAP_BY_ORDINAL(orig->u1.Ordinal)) {
                    auto* byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + orig->u1.AddressOfData);
                    hit = strcmp(reinterpret_cast<const char*>(byName->Name), fname) == 0;
                }
                ++orig;
            } else {
                hit = want && live->u1.Function == want;
            }
            if (!hit) continue;
            if (live->u1.Function == reinterpret_cast<ULONG_PTR>(hook)) return true;

            DWORD old = 0;
            if (!VirtualProtect(&live->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) return false;
            if (origOut && !*origOut) *origOut = reinterpret_cast<void*>(live->u1.Function);
            live->u1.Function = reinterpret_cast<ULONG_PTR>(hook);
            VirtualProtect(&live->u1.Function, sizeof(void*), old, &old);
            return true;
        }
    }
    return false;
}

// never arm a system dll kernel32 forwards to own import and recurses
bool isSystemModule(HMODULE mod) {
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(mod, path, MAX_PATH)) return true;
    wchar_t win[MAX_PATH] = {};
    if (!GetWindowsDirectoryW(win, MAX_PATH)) return true;
    const size_t n = wcslen(win);
    return _wcsnicmp(path, win, n) == 0;
}

void armModule(HMODULE mod) {
    if (!mod || mod == reinterpret_cast<HMODULE>(&__ImageBase)) return;
    if (isSystemModule(mod)) return;
    int n = 0;
    n += patchIat(mod, "LoadLibraryA", &hookLoadLibraryA, reinterpret_cast<void**>(&g_realLLA));
    n += patchIat(mod, "LoadLibraryW", &hookLoadLibraryW, reinterpret_cast<void**>(&g_realLLW));
    n += patchIat(mod, "LoadLibraryExA", &hookLoadLibraryExA, reinterpret_cast<void**>(&g_realLLExA));
    n += patchIat(mod, "LoadLibraryExW", &hookLoadLibraryExW, reinterpret_cast<void**>(&g_realLLExW));
    if (n) ++g_armed;
}

// peb walk not toolhelp DllMain still holds the loader lock
void armAllLoaded() {
    auto peb = reinterpret_cast<uint8_t*>(__readfsdword(0x30));
    if (!peb) return;
    auto ldr = *reinterpret_cast<uint8_t**>(peb + 0x0C);
    if (!ldr) return;
    auto* head = reinterpret_cast<LIST_ENTRY*>(ldr + 0x14);
    for (LIST_ENTRY* e = head->Flink; e && e != head; e = e->Flink) {
        auto base = *reinterpret_cast<HMODULE*>(reinterpret_cast<uint8_t*>(e) + 0x10);
        armModule(base);
    }
}

bool hookIatGetProcAddress() {
    if (!g_realLLA) g_realLLA = &LoadLibraryA;
    if (!g_realLLW) g_realLLW = &LoadLibraryW;
    if (!g_realLLExA) g_realLLExA = &LoadLibraryExA;
    if (!g_realLLExW) g_realLLExW = &LoadLibraryExW;
    armAllLoaded();
    armModule(GetModuleHandleW(nullptr));
    return true;
}

// pull d3d9 ourselves engine can load and call it inside one millisecond
void d3dWatchLoop() {
    LoadLibraryW(L"d3d9.dll");
    for (int i = 0; i < 120000 && g_running && !g_inlineHooked; ++i) {
        if (inlineHookD3DCreate9()) return;
        Sleep(1);
    }
}

// seh only a driver that hates a second device must not kill the client
int probeDummyDevice(D3DCreate9_t create, const char** why) {
    IDirect3D9* d3d = nullptr;
    HWND w = nullptr;
    int hooked = 0;
    __try {
        d3d = create(D3D_SDK_VERSION);
        if (!d3d) { *why = "Direct3DCreate9 returned null"; return 0; }
        // half built object means d3d9 still loading do not call it
        void** vt = *reinterpret_cast<void***>(d3d);
        if (!mapped(reinterpret_cast<uintptr_t>(vt), 17 * sizeof(void*), false)) {
            *why = "d3d9 not initialized yet";
            return 0;
        }

        w = CreateWindowExW(0, L"STATIC", L"knc pilot d3d probe", WS_POPUP,
                            0, 0, 1, 1, nullptr, nullptr, nullptr, nullptr);
        if (!w) { *why = "probe window failed"; return 0; }

        // d3d9 gives a different device class per devtype and per mt flag
        const DWORD base = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES |
                           D3DCREATE_FPU_PRESERVE;
        struct Variant { D3DDEVTYPE type; DWORD flags; };
        const Variant variants[] = {
            { D3DDEVTYPE_HAL, base },
            { D3DDEVTYPE_HAL, base | D3DCREATE_MULTITHREADED },
            { D3DDEVTYPE_HAL, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES |
                              D3DCREATE_FPU_PRESERVE },
            { D3DDEVTYPE_REF, base },
        };

        int made = 0;
        for (const Variant& v : variants) {
            D3DPRESENT_PARAMETERS pp = {};
            pp.Windowed = TRUE;
            pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            pp.BackBufferFormat = D3DFMT_UNKNOWN;
            pp.hDeviceWindow = w;
            IDirect3DDevice9* dev = nullptr;
            if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, v.type, w, v.flags, &pp, &dev)) || !dev)
                continue;
            ++made;
            if (hookDeviceVtable(dev)) ++hooked;
            dev->Release();
        }
        if (!made) *why = "no probe device could be created";
        else if (!hooked) *why = "present slot not writable";
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *why = "probe faulted";
    }
    if (w) DestroyWindow(w);
    if (d3d) d3d->Release();
    return hooked;
}

std::mutex g_d3dMu;

// lazy on purpose a startup probe races EngineDLL loading d3d9
bool ensureD3DHook(std::string* err) {
    if (g_presentHooks) return true;
    if (!g_cfg.d3d) { *err = "disabled by pilot.d3d"; return false; }
    std::lock_guard<std::mutex> lk(g_d3dMu);
    if (g_presentHooks) return true;

    // LoadLibrary waits for loader GetModuleHandle can give a half load
    HMODULE d3d9 = LoadLibraryW(L"d3d9.dll");
    if (!d3d9) { *err = "d3d9 not loadable"; return false; }
    auto create = reinterpret_cast<D3DCreate9_t>(GetProcAddress(d3d9, "Direct3DCreate9"));
    if (!create) { *err = "Direct3DCreate9 missing"; return false; }

    const char* why = "unknown";
    if (!probeDummyDevice(create, &why)) { *err = why; return false; }
    return true;
}

// pipe thread only game thread must be free to reach Present
bool d3dGrab(Frame* out, std::string* err) {
    if (!ensureD3DHook(err)) return false;
    g_capDone = false;
    g_capArm = true;
    for (int i = 0; i < 200 && !g_capDone; ++i) Sleep(10);
    g_capArm = false;
    if (!g_capDone) { *err = "client did not present within 2s"; return false; }
    std::lock_guard<std::mutex> lk(g_capMu);
    if (!g_capErr.empty()) { *err = g_capErr; return false; }
    *out = g_capFrame;
    if (!out->valid()) { *err = "backbuffer copy empty"; return false; }
    return true;
}

struct Pin {
    int32_t oldX = 0, oldY = 0;
    bool held = false;
};

// pin cursor object the ui reads it not lParam
bool pinCursor(int x, int y, Pin* p) {
    if (!readMem(addr::kCursorX, &p->oldX, 4) || !readMem(addr::kCursorY, &p->oldY, 4))
        return false;
    const int32_t nx = x, ny = y;
    if (!writeMem(addr::kCursorX, &nx, 4) || !writeMem(addr::kCursorY, &ny, 4)) return false;
    p->held = true;
    return true;
}

void unpinCursor(Pin* p) {
    if (!p->held) return;
    writeMem(addr::kCursorX, &p->oldX, 4);
    writeMem(addr::kCursorY, &p->oldY, 4);
    p->held = false;
}

std::string logicalOf(int x, int y) {
    const float sx = readF32(addr::kViewScaleX, 1.0f);
    const float sy = readF32(addr::kViewScaleY, 1.0f);
    const int lx = sx > 0.0f ? int(x / sx + 0.5f) : x;
    const int ly = sy > 0.0f ? int(y / sy + 0.5f) : y;
    return std::to_string(lx) + "," + std::to_string(ly);
}

std::string doMouse(HWND h, int x, int y, int button) {
    const int w = readI32(addr::kViewW, 0), ht = readI32(addr::kViewH, 0);
    if (w > 0 && ht > 0 && (x < 0 || y < 0 || x >= w || y >= ht))
        return "err point outside " + std::to_string(w) + "x" + std::to_string(ht);

    Pin pin;
    if (!pinCursor(x, y, &pin)) return "err cursor object not writable";

    const LPARAM lp = MAKELPARAM(x, y);
    SendMessageW(h, WM_MOUSEMOVE, 0, lp);
    if (button == 1) {
        SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, lp);
        SendMessageW(h, WM_LBUTTONUP, 0, lp);
    } else if (button == 2) {
        SendMessageW(h, WM_RBUTTONDOWN, MK_RBUTTON, lp);
        SendMessageW(h, WM_RBUTTONUP, 0, lp);
    }
    unpinCursor(&pin);

    const char* what = button == 1 ? "click" : button == 2 ? "rclick" : "move";
    return std::string("ok ") + what + " " + std::to_string(x) + "," + std::to_string(y) +
           " logical " + logicalOf(x, y);
}

// client drops keys when activate gate is down raise it for the send
struct Gate {
    uint8_t old = 1;
    bool forced = false;
};

void gateUp(Gate* g) {
    g->old = readU8(addr::kInputGate, 1);
    if (g->old == 0) {
        const uint8_t one = 1;
        g->forced = writeMem(addr::kInputGate, &one, 1);
    }
}

void gateDown(Gate* g) {
    if (g->forced) writeMem(addr::kInputGate, &g->old, 1);
}

// exe imports TranslateMessage so real press is down then char then up
wchar_t charForVk(int vk) {
    switch (vk) {
        case VK_BACK:   return 8;
        case VK_TAB:    return 9;
        case VK_RETURN: return 13;
        case VK_ESCAPE: return 27;
        case VK_SPACE:  return 32;
        default: break;
    }
    const UINT c = MapVirtualKeyW(UINT(vk), MAPVK_VK_TO_CHAR) & 0x7FFFu;
    return c >= 32 && c < 0xFFFF ? wchar_t(c) : 0;
}

std::string doKeys(HWND h, const std::vector<int>& vks) {
    Gate g;
    gateUp(&g);
    std::string list;
    for (int vk : vks) {
        const UINT sc = MapVirtualKeyW(UINT(vk), MAPVK_VK_TO_VSC);
        const LPARAM down = LPARAM(1u | (sc << 16));
        const LPARAM up = LPARAM(1u | (sc << 16) | (1u << 30) | (1u << 31));
        SendMessageW(h, WM_KEYDOWN, WPARAM(vk), down);
        const wchar_t ch = charForVk(vk);
        if (ch) SendMessageW(h, WM_CHAR, WPARAM(ch), down);
        SendMessageW(h, WM_KEYUP, WPARAM(vk), up);
        if (!list.empty()) list += ",";
        list += std::to_string(vk);
    }
    gateDown(&g);
    return "ok key " + list + (g.forced ? " gate forced" : "");
}

std::string doText(HWND h, const std::string& s) {
    if (s.empty()) return "err text needs a string";
    const int need = MultiByteToWideChar(CP_ACP, 0, s.c_str(), int(s.size()), nullptr, 0);
    std::wstring w(need > 0 ? need : 0, L'\0');
    if (need > 0) MultiByteToWideChar(CP_ACP, 0, s.c_str(), int(s.size()), &w[0], need);

    Gate g;
    gateUp(&g);
    for (wchar_t c : w) {
        if (c == L'\n') c = L'\r';
        SendMessageW(h, WM_CHAR, WPARAM(c), 1);
    }
    gateDown(&g);
    return "ok text " + std::to_string(w.size()) + " chars";
}

struct KeyName { const char* name; int vk; };
const KeyName kKeyNames[] = {
    { "enter", VK_RETURN }, { "return", VK_RETURN }, { "esc", VK_ESCAPE },
    { "escape", VK_ESCAPE }, { "space", VK_SPACE }, { "tab", VK_TAB },
    { "back", VK_BACK }, { "backspace", VK_BACK }, { "del", VK_DELETE },
    { "up", VK_UP }, { "down", VK_DOWN }, { "left", VK_LEFT }, { "right", VK_RIGHT },
    { "home", VK_HOME }, { "end", VK_END }, { "pgup", VK_PRIOR }, { "pgdn", VK_NEXT },
    { "f1", VK_F1 }, { "f2", VK_F2 }, { "f3", VK_F3 }, { "f4", VK_F4 },
    { "f5", VK_F5 }, { "f6", VK_F6 }, { "f7", VK_F7 }, { "f8", VK_F8 },
    { "f9", VK_F9 }, { "f10", VK_F10 }, { "f11", VK_F11 }, { "f12", VK_F12 },
};

int parseVk(const std::string& t) {
    if (t.empty()) return -1;
    std::string low;
    for (char c : t) low += char(tolower(static_cast<unsigned char>(c)));
    for (const KeyName& k : kKeyNames)
        if (low == k.name) return k.vk;
    if (low.size() == 1 && isalnum(static_cast<unsigned char>(low[0])))
        return toupper(static_cast<unsigned char>(low[0]));
    char* end = nullptr;
    const long v = strtol(t.c_str(), &end, 0);
    if (end && *end == 0 && v > 0 && v < 256) return int(v);
    return -1;
}

// the key poll asks GetAsyncKeyState per vk each frame so a held set answered there is a real press
using AsyncKey_t = SHORT(WINAPI*)(int);
AsyncKey_t g_realAsyncKey = nullptr;
std::atomic<uint8_t> g_held[256];
std::atomic<int> g_heldCount{0};
std::atomic<int> g_asyncCalls{0};
std::atomic<int> g_asyncHits{0};
std::atomic<bool> g_asyncHooked{false};

SHORT WINAPI hookAsyncKey(int vk) {
    ++g_asyncCalls;
    if (vk >= 0 && vk < 256 && g_held[vk].load(std::memory_order_relaxed)) {
        ++g_asyncHits;
        return SHORT(0x8000);
    }
    return g_realAsyncKey ? g_realAsyncKey(vk) : 0;
}

// the import walk by name first then the known slot so the poll sees the hook either way
bool hookAsyncKeyState() {
    if (g_asyncHooked) return true;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) user32 = LoadLibraryW(L"user32.dll");
    auto real = user32 ? reinterpret_cast<AsyncKey_t>(GetProcAddress(user32, "GetAsyncKeyState")) : nullptr;
    if (!real) return false;

    void* orig = nullptr;
    patchIat(GetModuleHandleW(nullptr), "GetAsyncKeyState", reinterpret_cast<void*>(&hookAsyncKey), &orig);
    g_realAsyncKey = orig ? reinterpret_cast<AsyncKey_t>(orig) : real;

    void* slot = nullptr;
    if (readMem(addr::kAsyncKeySlot, &slot, sizeof(slot)) && slot != reinterpret_cast<void*>(&hookAsyncKey)) {
        if (slot == reinterpret_cast<void*>(real)) {
            DWORD old = 0;
            auto* p = reinterpret_cast<void**>(addr::kAsyncKeySlot);
            if (VirtualProtect(p, sizeof(void*), PAGE_READWRITE, &old)) {
                *p = reinterpret_cast<void*>(&hookAsyncKey);
                VirtualProtect(p, sizeof(void*), old, &old);
            }
        } else {
            return false;
        }
    }
    g_asyncHooked = true;
    return true;
}

void forceGateUp() {
    if (readU8(addr::kInputGate, 1) == 0) {
        const uint8_t one = 1;
        writeMem(addr::kInputGate, &one, 1);
    }
}

// the poll is skipped while the activate gate is down so a held key keeps the gate up
void holdKeeperLoop() {
    while (g_running) {
        if (g_heldCount.load() > 0) forceGateUp();
        Sleep(30);
    }
}

std::string heldList() {
    std::string s;
    for (int vk = 0; vk < 256; ++vk) {
        if (!g_held[vk].load()) continue;
        if (!s.empty()) s += ",";
        s += std::to_string(vk);
    }
    return s.empty() ? "-" : s;
}

std::string doHold(const std::vector<std::string>& a, bool down) {
    if (!g_asyncHooked && !hookAsyncKeyState()) return "err GetAsyncKeyState not hooked";
    std::vector<int> vks;
    for (const std::string& t : a) {
        const int vk = parseVk(t);
        if (vk < 0) return "err unknown key " + t;
        vks.push_back(vk);
    }
    if (vks.empty()) return "err need a key";
    std::string list;
    for (int vk : vks) {
        const uint8_t was = g_held[vk].exchange(down ? 1 : 0);
        if (down && !was) ++g_heldCount;
        if (!down && was) --g_heldCount;
        if (!list.empty()) list += ",";
        list += std::to_string(vk);
    }
    if (down) forceGateUp();
    return std::string("ok ") + (down ? "hold " : "release ") + list + " held=" + heldList() +
           " gate=" + std::to_string(int(readU8(addr::kInputGate, 0)));
}

std::string doHoldMask() {
    int cleared = 0;
    for (int vk = 0; vk < 256; ++vk)
        if (g_held[vk].exchange(0)) ++cleared;
    g_heldCount = 0;
    return "ok holdmask cleared " + std::to_string(cleared);
}

// what the client array holds for each held vk proves the poll took the hook
std::string heldReport() {
    std::string s = "ok held=" + heldList();
    s += " hooked=" + std::string(g_asyncHooked ? "1" : "0");
    s += " calls=" + std::to_string(g_asyncCalls.load());
    s += " hits=" + std::to_string(g_asyncHits.load());
    void* slot = nullptr;
    readMem(addr::kAsyncKeySlot, &slot, sizeof(slot));
    s += " slot=" + std::string(slot == reinterpret_cast<void*>(&hookAsyncKey) ? "hook" : "other");
    s += " gate=" + std::to_string(int(readU8(addr::kInputGate, 0)));
    std::string bytes;
    for (int vk = 0; vk < 256; ++vk) {
        if (!g_held[vk].load()) continue;
        if (!bytes.empty()) bytes += ",";
        bytes += std::to_string(vk) + ":" + std::to_string(int(readU8(addr::kInputMgr + 4 + vk, 0))) +
                 "/" + std::to_string(readI32(addr::kInputMgr + 0x104 + size_t(vk) * 4, 0));
    }
    s += " client=" + (bytes.empty() ? "-" : bytes);
    return s;
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) ++i;
        size_t start = i;
        while (i < s.size() && !isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i > start) out.push_back(s.substr(start, i - start));
    }
    return out;
}

bool resolveAddr(const std::string& tok, uintptr_t* va, size_t* deflen) {
    std::string low;
    for (char c : tok) low += char(tolower(static_cast<unsigned char>(c)));
    for (const addr::Named& n : addr::kNamed)
        if (low == n.name) { *va = n.va; *deflen = n.len; return true; }
    char* end = nullptr;
    const unsigned long long v = strtoull(tok.c_str(), &end, 0);
    if (end && *end == 0 && v) { *va = uintptr_t(v); *deflen = 4; return true; }
    return false;
}

std::string hexAddr(uintptr_t v) {
    char b[16];
    sprintf_s(b, "0x%08X", unsigned(v));
    return b;
}

std::string f2s(float v) {
    char b[32];
    sprintf_s(b, "%.4f", v);
    return b;
}

// local car telemetry off the game object for a line follower on the pipe thread
std::string carReport() {
    const int idx = readI32(addr::kGameBase + 0x6B0, -1);
    std::string s = "ok stage=" + std::to_string(readStage()) + " idx=" + std::to_string(idx);
    if (idx < 0 || idx >= int(addr::kCarCap)) return s + " err no local car";
    const uintptr_t car = addr::kGameBase + size_t(idx) * addr::kCarStride;
    s += " x=" + f2s(readF32(car + 0x3244, 0.f));
    s += " y=" + f2s(readF32(car + 0x3248, 0.f));
    s += " z=" + f2s(readF32(car + 0x324C, 0.f));
    s += " yaw=" + f2s(readF32(car + 0x3220, 0.f));
    s += " speed=" + f2s(readF32(car + 0x3234, 0.f));
    s += " kmh=" + f2s(readF32(car + 0x32F4, 0.f));
    s += " drift=" + std::to_string(readI32(car + 0x35A4, 0));
    s += " gauge=" + f2s(readF32(car + 0x35A8, 0.f));
    s += " boost=" + std::to_string(readI32(car + 0x3300, 0));
    s += " cp=" + std::to_string(readI32(car + 0x3334, 0));
    s += " rank=" + std::to_string(readI32(car + 0x3724, 0));
    s += " prog=" + std::to_string(readI32(car + 0x6BC, 0));
    s += " accel=" + std::to_string(readI32(addr::kGameBase + 0x18, 0));
    s += " brake=" + std::to_string(readI32(addr::kGameBase + 0x1C, 0));
    s += " right=" + std::to_string(readI32(addr::kGameBase + 0x20, 0));
    s += " left=" + std::to_string(readI32(addr::kGameBase + 0x24, 0));
    s += " stuck=" + std::to_string(readI32(addr::kGameBase + 0x2C, 0));
    s += " held=" + heldList();
    return s;
}

// live row means non zero id at plus four
int catalogScan(const addr::Catalog& c, std::string* ids, std::string* keys, std::string* names) {
    int n = 0;
    for (size_t i = 0; i < c.cap; ++i) {
        const uintptr_t rec = c.base + i * c.stride;
        const int32_t id = readI32(rec + c.idOff, 0);
        if (!id) continue;
        ++n;
        if (ids) { if (!ids->empty()) *ids += ","; *ids += std::to_string(id); }
        if (keys) {
            if (!keys->empty()) *keys += ",";
            *keys += std::to_string(readI32(rec + c.keyOff, 0));
        }
        if (names && c.nameOff) {
            char buf[17] = {};
            readMem(rec + c.nameOff, buf, 16);
            for (char& ch : buf) if (ch && (ch < 32 || ch > 126)) ch = 0;
            if (!names->empty()) *names += ",";
            *names += buf[0] ? buf : "?";
        }
    }
    return n;
}

std::string catalogReport(const addr::Catalog& c) {
    std::string ids, keys, names;
    const int n = catalogScan(c, &ids, &keys, c.nameOff ? &names : nullptr);
    std::string s = "ok count=" + std::to_string(n) + " ids=" + (ids.empty() ? "-" : ids) +
                    " keys=" + (keys.empty() ? "-" : keys);
    if (c.nameOff) s += " names=" + (names.empty() ? "-" : names);
    return s;
}

std::string ownedList(uintptr_t base, size_t stride, size_t countOff, size_t cap) {
    int n = readI32(base + countOff, 0);
    if (n < 0) n = 0;
    if (n > int(cap)) n = int(cap);
    std::string ids;
    for (int i = 0; i < n; ++i) {
        if (!ids.empty()) ids += ",";
        ids += std::to_string(readI32(base + 4 + size_t(i) * stride, 0));
    }
    return "ok count=" + std::to_string(n) + " ids=" + (ids.empty() ? "-" : ids);
}

std::string dumpAll(HWND h) {
    const float sx = readF32(addr::kViewScaleX, 1.0f);
    const float sy = readF32(addr::kViewScaleY, 1.0f);
    const int cx = readI32(addr::kCursorX, 0), cy = readI32(addr::kCursorY, 0);
    int ownChars = readI32(addr::kOwnChars + addr::kOwnCharCount, 0);
    int ownKarts = readI32(addr::kOwnKarts + addr::kOwnKartCount, 0);
    if (ownChars < 0 || ownChars > int(addr::kOwnCharCap)) ownChars = -1;
    if (ownKarts < 0 || ownKarts > int(addr::kOwnKartCap)) ownKarts = -1;

    std::string s = "ok stage=" + std::to_string(readStage());
    s += " dialog=" + std::to_string(int(readU8(addr::kDialogShown, 0)));
    s += " dialogtype=" + std::to_string(readI32(addr::kDialogType, 0));
    s += " selchar=" + std::to_string(readI32(addr::kSelChar, 0));
    s += " selkart=" + std::to_string(readI32(addr::kSelKart, 0));
    s += " ownchars=" + std::to_string(ownChars);
    s += " ownkarts=" + std::to_string(ownKarts);
    s += " drivers=" + std::to_string(catalogScan(addr::kDrvCat, nullptr, nullptr, nullptr));
    s += " vehicles=" + std::to_string(catalogScan(addr::kVehCat, nullptr, nullptr, nullptr));
    s += " cursor=" + std::to_string(cx) + "," + std::to_string(cy);
    s += " logical=" + logicalOf(cx, cy);
    s += " res=" + std::to_string(readI32(addr::kViewW, 0)) + "x" +
         std::to_string(readI32(addr::kViewH, 0));
    s += " scale=" + f2s(sx) + "," + f2s(sy);
    s += " viewmode=" + std::to_string(int(readU8(addr::kViewMode, 0)));
    s += " gate=" + std::to_string(int(readU8(addr::kInputGate, 0)));

    RECT rc{};
    GetClientRect(h, &rc);
    s += " client=" + std::to_string(rc.right - rc.left) + "x" + std::to_string(rc.bottom - rc.top);
    s += " iconic=" + std::to_string(IsIconic(h) ? 1 : 0);
    s += " fg=" + std::to_string(GetForegroundWindow() == h ? 1 : 0);
    s += " hwnd=" + hexAddr(reinterpret_cast<uintptr_t>(h));
    return s;
}

// hook internals live here so dump stays about game state
std::string d3dDiag() {
    return "ok inline=" + std::string(g_inlineHooked ? "1" : "0") +
           " armed=" + std::to_string(g_armed.load()) +
           " d3dcreate=" + std::to_string(g_d3dCreates.load()) +
           " devcreate=" + std::to_string(g_devCreates.load()) +
           " devhook=" + std::to_string(g_presentHooks.load()) +
           " present=" + std::to_string(g_presentSeen.load()) +
           " scpresent=" + std::to_string(g_scPresents.load()) +
           " endscene=" + std::to_string(g_endScenes.load());
}

const char* kHelp =
    "ok ping | stage | waitstage N [MS] | dump | peek ADDR|NAME [LEN] | poke ADDR|NAME HEX | "
    "click X Y | rclick X Y | move X Y | key VK... | text STRING | cursor | "
    "shot [PATH] [auto|gdi|d3d] | chars | karts | drivers | vehicles | goto N | title | "
    "hold VK... | release VK... | holdmask | held | car | d3ddiag | help";

struct Command {
    std::string verb;
    std::string arg;
    std::string reply;
    bool done = false;
    bool started = false;
    bool abandoned = false;
};

std::mutex g_mu;
std::condition_variable g_cv;
std::queue<std::shared_ptr<Command>> g_queue;

// runs on the game thread only
void execute(Command& c) {
    HWND h = g_hwnd.load();
    const std::vector<std::string> a = split(c.arg);

    if (c.verb == "ping") {
        c.reply = "ok pong";
    } else if (c.verb == "help") {
        c.reply = kHelp;
    } else if (c.verb == "stage") {
        c.reply = "ok " + std::to_string(readStage());
    } else if (c.verb == "dump") {
        c.reply = dumpAll(h);
    } else if (c.verb == "d3ddiag") {
        c.reply = d3dDiag();
    } else if (c.verb == "cursor") {
        const int cx = readI32(addr::kCursorX, 0), cy = readI32(addr::kCursorY, 0);
        c.reply = "ok raw=" + std::to_string(cx) + "," + std::to_string(cy) +
                  " logical=" + logicalOf(cx, cy) +
                  " scale=" + f2s(readF32(addr::kViewScaleX, 1.0f)) + "," +
                  f2s(readF32(addr::kViewScaleY, 1.0f)) +
                  " res=" + std::to_string(readI32(addr::kViewW, 0)) + "x" +
                  std::to_string(readI32(addr::kViewH, 0)) +
                  " viewmode=" + std::to_string(int(readU8(addr::kViewMode, 0))) +
                  " btn=" + std::to_string(int(readU8(addr::kCursorBtn, 0)));
    } else if (c.verb == "peek") {
        uintptr_t va = 0;
        size_t len = 4;
        if (a.empty() || !resolveAddr(a[0], &va, &len)) { c.reply = "err bad address"; return; }
        if (a.size() > 1) len = size_t(strtoul(a[1].c_str(), nullptr, 0));
        if (len == 0 || len > 256) { c.reply = "err len must be 1 to 256"; return; }
        std::vector<uint8_t> buf(len);
        if (!readMem(va, buf.data(), len)) { c.reply = "err address not readable"; return; }
        std::string hex;
        for (uint8_t b : buf) { char t[4]; sprintf_s(t, "%02X", b); hex += t; }
        c.reply = "ok " + hexAddr(va) + " " + std::to_string(len) + " " + hex;
        if (len == 4) {
            int32_t v = 0;
            memcpy(&v, buf.data(), 4);
            float fv = 0;
            memcpy(&fv, buf.data(), 4);
            c.reply += " i32=" + std::to_string(v) + " f32=" + f2s(fv);
        } else if (len == 1) {
            c.reply += " u8=" + std::to_string(int(buf[0]));
        }
    } else if (c.verb == "poke") {
        uintptr_t va = 0;
        size_t len = 4;
        if (a.size() < 2 || !resolveAddr(a[0], &va, &len)) { c.reply = "err bad address"; return; }
        std::vector<uint8_t> bytes;
        const std::string& hx = a[1];
        if (hx.size() % 2 || hx.empty() || hx.size() > 512) { c.reply = "err bad hex"; return; }
        for (size_t i = 0; i < hx.size(); i += 2) {
            char t[3] = { hx[i], hx[i + 1], 0 };
            char* end = nullptr;
            const long v = strtol(t, &end, 16);
            if (end != t + 2) { c.reply = "err bad hex"; return; }
            bytes.push_back(uint8_t(v));
        }
        if (!writeMem(va, bytes.data(), bytes.size())) { c.reply = "err address not writable"; return; }
        c.reply = "ok poke " + hexAddr(va) + " " + std::to_string(bytes.size()) + " bytes";
    } else if (c.verb == "click" || c.verb == "rclick" || c.verb == "move") {
        if (!h) { c.reply = "err no window"; return; }
        if (a.size() < 2) { c.reply = "err need X Y"; return; }
        const int x = atoi(a[0].c_str()), y = atoi(a[1].c_str());
        c.reply = doMouse(h, x, y, c.verb == "click" ? 1 : c.verb == "rclick" ? 2 : 0);
    } else if (c.verb == "key") {
        if (!h) { c.reply = "err no window"; return; }
        std::vector<int> vks;
        for (const std::string& t : a) {
            const int vk = parseVk(t);
            if (vk < 0) { c.reply = "err unknown key " + t; return; }
            vks.push_back(vk);
        }
        if (vks.empty()) { c.reply = "err need a key"; return; }
        c.reply = doKeys(h, vks);
    } else if (c.verb == "text") {
        if (!h) { c.reply = "err no window"; return; }
        c.reply = doText(h, c.arg);
    } else if (c.verb == "chars") {
        c.reply = ownedList(addr::kOwnChars, addr::kOwnCharStride, addr::kOwnCharCount, addr::kOwnCharCap);
    } else if (c.verb == "karts") {
        c.reply = ownedList(addr::kOwnKarts, addr::kOwnKartStride, addr::kOwnKartCount, addr::kOwnKartCap);
    } else if (c.verb == "drivers") {
        c.reply = catalogReport(addr::kDrvCat);
    } else if (c.verb == "vehicles") {
        c.reply = catalogReport(addr::kVehCat);
    } else if (c.verb == "goto") {
        const int state = atoi(c.arg.c_str());
        if (state < 0 || state > 40) { c.reply = "err state out of range"; return; }
        auto fn = reinterpret_cast<SetUIState_t>(addr::kSetUIState);
        fn(reinterpret_cast<void*>(addr::kStageCtx), state);
        c.reply = "ok stage now " + std::to_string(readStage());
    } else if (c.verb == "title") {
        char buf[256] = {};
        if (h) GetWindowTextA(h, buf, sizeof(buf) - 1);
        c.reply = std::string("ok ") + buf;
    } else if (c.verb == "__gdishot") {
        if (!h) { c.reply = "err no window"; return; }
        Frame f;
        std::string method, err;
        if (!gdiGrab(h, &f, &method, &err)) { c.reply = "err " + err; return; }
        std::string serr;
        if (!savePng(f, c.arg, &serr)) { c.reply = "err " + serr; return; }
        c.reply = "ok " + c.arg + " " + std::to_string(f.w) + "x" + std::to_string(f.h) + " " + method;
    } else {
        c.reply = "err unknown verb";
    }
}

void drain() {
    for (;;) {
        std::shared_ptr<Command> c;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            if (g_queue.empty()) return;
            c = g_queue.front();
            g_queue.pop();
            // a shared pointer keeps the command alive so an abandoned one is just skipped not dangling
            if (c->abandoned) c.reset();
            else c->started = true;
        }
        if (!c) continue;
        execute(*c);
        {
            std::lock_guard<std::mutex> lk(g_mu);
            c->done = true;
        }
        g_cv.notify_all();
    }
}

LRESULT CALLBACK pilotProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_PILOT_DRAIN) { drain(); return 0; }
    return CallWindowProcW(g_prevProc, h, msg, w, l);
}

// blocks calling pipe thread until game thread answered
std::string dispatch(const std::string& verb, const std::string& arg) {
    HWND h = g_hwnd.load();
    if (!h) return "err window not attached yet";

    auto c = std::make_shared<Command>();
    c->verb = verb;
    c->arg = arg;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_queue.push(c);
    }
    PostMessageW(h, WM_PILOT_DRAIN, 0, 0);

    std::unique_lock<std::mutex> lk(g_mu);
    if (!g_cv.wait_for(lk, std::chrono::seconds(20), [&] { return c->done; })) {
        // not started stays skipped in the queue a running one keeps this pointer until it finishes
        if (!c->started) c->abandoned = true;
        return "err timeout game thread busy";
    }
    return c->reply;
}

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string doWaitStage(const std::vector<std::string>& a) {
    if (a.empty()) return "err need a stage";
    const int want = atoi(a[0].c_str());
    int timeout = a.size() > 1 ? atoi(a[1].c_str()) : 15000;
    if (timeout < 0) timeout = 0;
    if (timeout > 600000) timeout = 600000;
    const int64_t start = nowMs();
    for (;;) {
        const int cur = readStage();
        const int64_t spent = nowMs() - start;
        if (cur == want)
            return "ok stage " + std::to_string(cur) + " after " + std::to_string(spent) + "ms";
        if (spent >= timeout)
            return "err timeout stage " + std::to_string(cur) + " after " + std::to_string(spent) + "ms";
        Sleep(20);
    }
}

bool isMode(const std::string& s) { return s == "auto" || s == "gdi" || s == "d3d"; }

// path may hold spaces so only a trailing keyword is the mode
std::string doShot(const std::string& arg) {
    std::string path = arg;
    std::string mode = "auto";
    const size_t sp = path.find_last_of(" \t");
    if (sp != std::string::npos && isMode(path.substr(sp + 1))) {
        mode = path.substr(sp + 1);
        path = path.substr(0, sp);
    } else if (isMode(path)) {
        mode = path;
        path.clear();
    }
    while (!path.empty() && isspace(static_cast<unsigned char>(path.back()))) path.pop_back();
    while (!path.empty() && isspace(static_cast<unsigned char>(path.front()))) path.erase(0, 1);
    if (path.empty()) path = "pilot_shot.png";

    std::string gdiErr = "skipped";
    if (mode != "d3d") {
        const std::string r = dispatch("__gdishot", path);
        if (r.compare(0, 2, "ok") == 0) return r;
        gdiErr = r.size() > 4 ? r.substr(4) : r;
        if (mode == "gdi") return r;
    }

    Frame f;
    std::string err;
    if (!d3dGrab(&f, &err))
        return "err gdi " + gdiErr + " and d3d " + err;
    std::string serr;
    if (!savePng(f, path, &serr)) return "err " + serr;
    return "ok " + path + " " + std::to_string(f.w) + "x" + std::to_string(f.h) + " d3d";
}

// waits and polls stay off game thread so frames keep running
std::string handle(const std::string& verb, const std::string& arg) {
    const std::vector<std::string> a = split(arg);
    if (verb == "waitstage") return doWaitStage(a);
    if (verb == "shot") return doShot(arg);
    // held keys and telemetry stay off the game thread so a follower runs at its own rate
    if (verb == "hold") return doHold(a, true);
    if (verb == "release") return doHold(a, false);
    if (verb == "holdmask") return doHoldMask();
    if (verb == "held") return heldReport();
    if (verb == "car") return carReport();
    return dispatch(verb, arg);
}

void serveClient(HANDLE pipe) {
    char buf[4096] = {};
    DWORD got = 0;
    if (ReadFile(pipe, buf, sizeof(buf) - 1, &got, nullptr) && got) {
        std::string line(buf, got);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();

        const size_t sp = line.find(' ');
        const std::string verb = sp == std::string::npos ? line : line.substr(0, sp);
        const std::string arg = sp == std::string::npos ? "" : line.substr(sp + 1);

        std::string reply = handle(verb, arg);
        reply += "\n";
        DWORD wrote = 0;
        WriteFile(pipe, reply.data(), static_cast<DWORD>(reply.size()), &wrote, nullptr);
        FlushFileBuffers(pipe);
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

void pipeLoop() {
    while (g_running) {
        HANDLE pipe = CreateNamedPipeW(kPipeName, PIPE_ACCESS_DUPLEX,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       8, 8192, 8192, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) { Sleep(500); continue; }
        if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            // one thread per client else long waitstage blocks every verb
            std::thread(serveClient, pipe).detach();
        } else {
            CloseHandle(pipe);
        }
    }
}

// the window does not exist during DllMain so wait for it
void attachLoop() {
    for (int i = 0; i < 1200 && g_running; ++i) {
        HWND h = FindWindowW(nullptr, kWindowTitle);
        if (h) {
            g_prevProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&pilotProc)));
            g_hwnd = h;
            return;
        }
        Sleep(100);
    }
}

}  // namespace

void configure(const Config& cfg) { g_cfg = cfg; }

bool install() {
    if (!g_cfg.enabled) return true;

    Gdiplus::GdiplusStartupInput si;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &si, nullptr) != Gdiplus::Ok) return false;

    // must land before client resolves Direct3DCreate9
    if (g_cfg.d3d) hookIatGetProcAddress();

    g_running = true;
    std::thread(attachLoop).detach();
    std::thread(pipeLoop).detach();
    if (g_cfg.d3d) std::thread(d3dWatchLoop).detach();
    // the exe iat is mapped before DllMain so the key hook lands before the first poll
    hookAsyncKeyState();
    std::thread(holdKeeperLoop).detach();
    return true;
}

void shutdown() {
    g_running = false;
    HWND h = g_hwnd.load();
    if (h && g_prevProc)
        SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_prevProc));
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
}

}  // namespace pilot
