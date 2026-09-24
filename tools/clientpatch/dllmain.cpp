
#include <windows.h>
#include <unknwn.h>

#include "spy.h"
#include "pilot.h"

#include <cstdarg>
#include <cstdio>
#include <share.h>
#include <cstring>
#include <string>
#include <vector>

// linker provides this it is the base of our own dll
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {

HMODULE g_realDinput8 = nullptr;

using DirectInput8Create_t = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
DirectInput8Create_t g_realCreate = nullptr;

std::string moduleDir(HMODULE mod) {
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(mod, buf, MAX_PATH);
    std::string p(buf);
    size_t cut = p.find_last_of("\\/");
    return cut == std::string::npos ? std::string(".") : p.substr(0, cut);
}

FILE* g_log = nullptr;

void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

spy::Config g_spy;
pilot::Config g_pilot;

struct Patch {
    uintptr_t addr = 0;
    std::vector<uint8_t> orig;
    std::vector<uint8_t> repl;
    std::string name;
};

// hex pairs separated by spaces
bool parseBytes(const std::string& s, std::vector<uint8_t>& out) {
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i + 1 >= s.size()) break;
        char hex[3] = { s[i], s[i + 1], 0 };
        char* end = nullptr;
        long v = strtol(hex, &end, 16);
        if (end != hex + 2) return false;
        out.push_back(static_cast<uint8_t>(v));
        i += 2;
    }
    return !out.empty();
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// one patch per line address then original bytes then new bytes then why
std::vector<Patch> loadPatches(const std::string& path) {
    std::vector<Patch> out;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "r") != 0 || !f) {
        logf("no patches file at %s", path.c_str());
        return out;
    }
    char line[512];
    int lineNo = 0;
    while (fgets(line, sizeof(line), f)) {
        ++lineNo;
        std::string s = trim(line);
        if (s.empty() || s[0] == '#' || s[0] == ';') continue;

        // spy keys live in the same file so there is one thing to edit
        if (s.compare(0, 4, "spy.") == 0) {
            const size_t eq = s.find('=');
            if (eq == std::string::npos) continue;
            const std::string k = trim(s.substr(4, eq - 4));
            const std::string v = trim(s.substr(eq + 1));
            if (k == "enabled")        g_spy.enabled = (v == "1" || v == "true");
            else if (k == "hexdump")   g_spy.hexDump = (v == "1" || v == "true");
            else if (k == "maxhex")    g_spy.maxHexBytes = atoi(v.c_str());
            else if (k == "heartbeat") g_spy.logHeartbeat = (v == "1" || v == "true");
            else if (k == "camfix")    g_spy.camFix = (v == "1" || v == "true");
            else if (k == "buttons")   g_spy.injectButtons = (v == "1" || v == "true");
            else if (k == "armfix")    g_spy.armFix = (v == "1" || v == "true");
            else if (k == "cwdfix")    g_spy.cwdFix = (v == "1" || v == "true");
            else if (k == "redirect") {
                // dotted ipv4 to network order no winsock needed here
                unsigned int a = 0, b = 0, c = 0, d = 0;
                if (sscanf_s(v.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4 &&
                    a < 256 && b < 256 && c < 256 && d < 256) {
                    g_spy.redirectIp = a | (b << 8) | (c << 16) | (d << 24);
                }
            }
            else if (k == "redirectport") {
                // cannot bind 50017 windows reserves 50000 to 50059 for hyper v so the port must move too
                const int port = atoi(v.c_str());
                if (port > 0 && port < 65536) g_spy.redirectPort = static_cast<unsigned short>(port);
            }
            continue;
        }
        if (s.compare(0, 6, "pilot.") == 0) {
            const size_t eq = s.find('=');
            if (eq == std::string::npos) continue;
            const std::string k = trim(s.substr(6, eq - 6));
            const std::string v = trim(s.substr(eq + 1));
            if (k == "enabled")  g_pilot.enabled = (v == "1" || v == "true");
            else if (k == "d3d") g_pilot.d3d = (v == "1" || v == "true");
            continue;
        }

        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t i = 0; i <= s.size(); ++i) {
            if (i == s.size() || s[i] == '|') {
                parts.push_back(trim(s.substr(start, i - start)));
                start = i + 1;
            }
        }
        if (parts.size() < 3) {
            logf("line %d malformed skipped", lineNo);
            continue;
        }

        Patch p;
        p.addr = static_cast<uintptr_t>(strtoull(parts[0].c_str(), nullptr, 0));
        p.name = parts.size() > 3 ? parts[3] : "unnamed";
        if (!p.addr || !parseBytes(parts[1], p.orig) || !parseBytes(parts[2], p.repl)) {
            logf("line %d unparsable skipped", lineNo);
            continue;
        }
        if (p.orig.size() != p.repl.size()) {
            logf("line %d length mismatch skipped", lineNo);
            continue;
        }
        out.push_back(std::move(p));
    }
    fclose(f);
    return out;
}

// reads DestDir from HKLM Software OGPlanet Games 31 under WOW6432Node and overwrites it since a stale value causes invalid directory
void fixInstallPath(const std::string& dir) {
    std::string want = dir;
    if (!want.empty() && want.back() != '\\') want += '\\';   // the client stores it with one

    HKEY key = nullptr;
    LONG rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\OGPlanet\\Games\\31", 0,
                            KEY_READ | KEY_WRITE, &key);
    if (rc != ERROR_SUCCESS) {
        DWORD disp = 0;
        rc = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\OGPlanet\\Games\\31", 0, nullptr,
                             0, KEY_READ | KEY_WRITE, nullptr, &key, &disp);
    }
    if (rc != ERROR_SUCCESS) {
        logf("registry: cannot open or create the key, %ld, run as administrator", rc);
        return;
    }

    char had[MAX_PATH] = {};
    DWORD len = sizeof(had), type = 0;
    if (RegQueryValueExA(key, "DestDir", nullptr, &type,
                         reinterpret_cast<BYTE*>(had), &len) == ERROR_SUCCESS) {
        logf("registry: DestDir was %s", had);
        if (_stricmp(had, want.c_str()) == 0) {
            logf("registry: already correct");
            RegCloseKey(key);
            return;
        }
    } else {
        logf("registry: DestDir was missing");
    }

    rc = RegSetValueExA(key, "DestDir", 0, REG_SZ,
                        reinterpret_cast<const BYTE*>(want.c_str()),
                        static_cast<DWORD>(want.size() + 1));
    logf(rc == ERROR_SUCCESS ? "registry: DestDir now %s" : "registry: write failed for %s",
         want.c_str());
    RegCloseKey(key);
}

// reads the real server target from launcher ini in plain text instead of the obfuscated network2 ini
void readLauncherTarget(const std::string& dir) {
    FILE* f = nullptr;
    if (fopen_s(&f, (dir + "\launcher.ini").c_str(), "r") != 0 || !f) {
        logf("no launcher.ini, connection left as the client composed it");
        return;
    }
    char line[512];
    unsigned int a = 0, b = 0, c = 0, d = 0;
    while (fgets(line, sizeof(line), f)) {
        std::string s = trim(line);
        if (s.compare(0, 9, "ServerIP=") == 0) {
            if (sscanf_s(s.c_str() + 9, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 &&
                a < 256 && b < 256 && c < 256 && d < 256) {
                g_spy.redirectIp = a | (b << 8) | (c << 16) | (d << 24);
            }
        } else if (s.compare(0, 11, "ServerPort=") == 0) {
            const int port = atoi(s.c_str() + 11);
            if (port > 0 && port < 65536) g_spy.redirectPort = static_cast<unsigned short>(port);
        }
    }
    fclose(f);
    if (g_spy.redirectIp) {
        logf("launcher.ini target %u.%u.%u.%u:%u", a, b, c, d,
             static_cast<unsigned>(g_spy.redirectPort));
    } else {
        logf("launcher.ini has no usable ServerIP");
    }
}

void applyPatches() {
    const std::string dir = moduleDir(reinterpret_cast<HMODULE>(&__ImageBase));
    auto patches = loadPatches(dir + "\\patches.ini");
    if (patches.empty()) return;

    MEMORY_BASIC_INFORMATION mbi = {};
    int ok = 0, skipped = 0;

    for (const auto& p : patches) {
        void* target = reinterpret_cast<void*>(p.addr);

        if (!VirtualQuery(target, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) {
            logf("SKIP %s addr %p not mapped", p.name.c_str(), target);
            ++skipped;
            continue;
        }

        // never blind write a wrong address corrupts the client silently
        if (memcmp(target, p.orig.data(), p.orig.size()) != 0) {
            logf("SKIP %s addr %p original bytes do not match", p.name.c_str(), target);
            ++skipped;
            continue;
        }

        DWORD old = 0;
        if (!VirtualProtect(target, p.repl.size(), PAGE_EXECUTE_READWRITE, &old)) {
            logf("SKIP %s addr %p VirtualProtect failed %lu", p.name.c_str(), target, GetLastError());
            ++skipped;
            continue;
        }
        memcpy(target, p.repl.data(), p.repl.size());
        VirtualProtect(target, p.repl.size(), old, &old);
        FlushInstructionCache(GetCurrentProcess(), target, p.repl.size());

        logf("OK   %s addr %p %zu bytes", p.name.c_str(), target, p.repl.size());
        ++ok;
    }
    logf("applied %d skipped %d", ok, skipped);
}

}  // namespace

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD ver, REFIID iid,
                                             LPVOID* out, LPUNKNOWN outer) {
    if (!g_realCreate) return E_FAIL;
    return g_realCreate(inst, ver, iid, out, outer);
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    DisableThreadLibraryCalls(mod);

    const std::string dir = moduleDir(mod);
    // missing patches ini means a release client no code patch or packet log but hooks still install
    const std::string iniPath = dir + "\\patches.ini";
    const bool devMode = GetFileAttributesA(iniPath.c_str()) != INVALID_FILE_ATTRIBUTES;

    // clientpatch log always writes so whether the dll loaded is never a mystery packet log is release only omission
    g_log = _fsopen((dir + "\\clientpatch.log").c_str(), "w", _SH_DENYNO);
    logf("clientpatch attach, %s mode", devMode ? "dev" : "release");

    if (!devMode) {
        // hooks always stay on only logging is toggled since buttons and cwd fix live inside the spy
        g_spy.enabled = true;
        g_spy.quiet   = true;
        g_spy.hexDump = false;
        g_spy.logHeartbeat = false;
        // cwd fix only applies to a full client since a partial one still needs its own working directory
        g_spy.cwdFix =
            GetFileAttributesA((dir + "\\KnC.exe").c_str()) != INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesA((dir + "\\Data").c_str())    != INVALID_FILE_ATTRIBUTES;
        // these default true in spy h stated here so a future default change cannot silently disable them
        g_spy.injectButtons = true;
        g_spy.camFix        = true;
        g_spy.armFix        = true;
    }

    char sys[MAX_PATH] = {};
    GetSystemDirectoryA(sys, MAX_PATH);
    g_realDinput8 = LoadLibraryA((std::string(sys) + "\\dinput8.dll").c_str());
    if (!g_realDinput8) {
        logf("FATAL real dinput8 not found");
        return FALSE;
    }
    g_realCreate = reinterpret_cast<DirectInput8Create_t>(
        GetProcAddress(g_realDinput8, "DirectInput8Create"));
    if (!g_realCreate) {
        logf("FATAL DirectInput8Create not found in real dinput8");
        return FALSE;
    }

    // exe is fully mapped before our DllMain so absolute addresses are already valid when read
    fixInstallPath(dir);

    // release has no patches ini so the redirect comes from launcher ini
    if (!devMode) readLauncherTarget(dir);

    if (devMode) applyPatches();

    // only runs when asked since forcing cwd on an incomplete client strands it without paks
    if (g_spy.cwdFix) {
        SetCurrentDirectoryA(dir.c_str());
        char now[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, now);
        logf("set current dir -> %s (now %s)", dir.c_str(), now);
    } else {
        logf("cwdfix OFF, KnC.exe or Data not found next to the dll");
    }

    spy::setDiagLog(g_log);   // g spy already filled from spy keys diag log goes to clientpatch log
    spy::configure(g_spy);
    if (g_spy.enabled) {
        const bool ok = spy::install((dir + "\\packets.log").c_str());
        logf("spy %s", ok ? "installed" : "failed");
    } else {
        logf("spy disabled");
    }

    pilot::configure(g_pilot);
    if (g_pilot.enabled) {
        logf("pilot %s", pilot::install() ? "listening on knc_pilot" : "failed");
    } else {
        logf("pilot disabled");
    }
    return TRUE;
}
