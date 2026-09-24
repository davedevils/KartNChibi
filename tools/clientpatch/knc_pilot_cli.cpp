
#include <windows.h>

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: knc-pilot <verb> [args]   try  knc-pilot help\n");
        return 2;
    }

    std::string cmd = argv[1];
    for (int i = 2; i < argc; ++i) { cmd += ' '; cmd += argv[i]; }
    cmd += '\n';

    HANDLE pipe = INVALID_HANDLE_VALUE;
    // client may be mid frame retry briefly before giving up
    for (int attempt = 0; attempt < 40; ++attempt) {
        pipe = CreateFileW(L"\\\\.\\pipe\\knc_pilot", GENERIC_READ | GENERIC_WRITE,
                           0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) break;
        const DWORD e = GetLastError();
        if (e != ERROR_PIPE_BUSY && e != ERROR_FILE_NOT_FOUND) break;
        Sleep(250);
    }
    if (pipe == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "err client not reachable, is it running with pilot.enabled = 1\n");
        return 2;
    }

    DWORD n = 0;
    if (!WriteFile(pipe, cmd.data(), static_cast<DWORD>(cmd.size()), &n, nullptr)) {
        fprintf(stderr, "err write failed\n");
        CloseHandle(pipe);
        return 2;
    }

    char buf[8192] = {};
    DWORD got = 0;
    if (!ReadFile(pipe, buf, sizeof(buf) - 1, &got, nullptr) || !got) {
        fprintf(stderr, "err no reply\n");
        CloseHandle(pipe);
        return 2;
    }
    CloseHandle(pipe);

    std::string reply(buf, got);
    while (!reply.empty() && (reply.back() == '\n' || reply.back() == '\r')) reply.pop_back();
    printf("%s\n", reply.c_str());

    return reply.compare(0, 2, "ok") == 0 ? 0 : 1;
}
