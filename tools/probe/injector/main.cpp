#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#pragma comment(lib, "advapi32.lib")

static void enable_debug_privilege() {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) return;
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (LookupPrivilegeValueA(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid))
        AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(token);
}

static DWORD find_pid(const std::string& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 entry{}; entry.dwSize = sizeof entry;
    DWORD pid = 0;
    if (Process32First(snap, &entry)) {
        do {
            if (_stricmp(name.c_str(), entry.szExeFile) == 0) { pid = entry.th32ProcessID; break; }
        } while (Process32Next(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

int main(int argc, char** argv) {
    if (argc != 3) { std::printf("usage: %s <process-name> <dll-path>\n", argv[0]); return 2; }
    const std::string proc = argv[1];
    const std::string dll = argv[2];

    enable_debug_privilege();
    const DWORD pid = find_pid(proc);
    if (!pid) { std::printf("process not found: %s\n", proc.c_str()); return 1; }

    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                           PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                           FALSE, pid);
    if (!h) { std::printf("OpenProcess failed: %lu\n", GetLastError()); return 1; }

    const size_t bytes = (dll.size() + 1);
    void* remote = VirtualAllocEx(h, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { std::printf("VirtualAllocEx failed: %lu\n", GetLastError()); CloseHandle(h); return 1; }
    if (!WriteProcessMemory(h, remote, dll.c_str(), bytes, nullptr)) {
        std::printf("WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(h, remote, 0, MEM_RELEASE); CloseHandle(h); return 1;
    }

    auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));
    HANDLE thread = CreateRemoteThread(h, nullptr, 0, load, remote, 0, nullptr);
    if (!thread) {
        std::printf("CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(h, remote, 0, MEM_RELEASE); CloseHandle(h); return 1;
    }

    WaitForSingleObject(thread, INFINITE);
    DWORD module_addr = 0;
    if (!GetExitCodeThread(thread, &module_addr)) {
        std::printf("GetExitCodeThread failed: %lu\n", GetLastError());
    }
    std::printf("injected into pid %lu (LoadLibrary returned 0x%lX)\n", pid, module_addr);

    CloseHandle(thread);
    VirtualFreeEx(h, remote, 0, MEM_RELEASE);
    CloseHandle(h);
    return module_addr ? 0 : 1;
}
