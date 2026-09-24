/// hooks send and recv inside ws2 32 itself since the exe resolves them at runtime

#pragma once

#include <cstdio>

#include <cstdint>

namespace spy {

struct Config {
    bool enabled = false;
    /// installs every hook but writes nothing a shipped client still needs the other fixes
    bool quiet = false;
    bool hexDump = true;        // full payload hex per frame line max bytes truncates long payloads
    int  maxHexBytes = 256;
    bool logHeartbeat = false;  // heartbeat floods log hide by default cam fix seeds camera distance and height
    bool camFix = true;
    bool injectButtons = true;  // adds quest and room craft buttons to bottom bar arm guard hides a client bug reproduce with 0
    bool armFix = true;
    unsigned long redirectIp = 0;  // redirects outbound connects to this ip and port zero on either means no override network byte order
    unsigned short redirectPort = 0;
    bool cwdFix = false;  // hooks SetCurrentDirectory so a foreign install folder cannot escape our own folder
};

// reads the spy section of patches ini
void configure(const Config& cfg);

// installs the ws2 32 hooks returns false when a prologue looks unexpected
bool install(const char* logPath);

/// where hook diagnostics go separate from the packet log
void setDiagLog(FILE* f);

void shutdown();

}  // namespace spy
