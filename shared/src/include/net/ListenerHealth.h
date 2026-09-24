/// accept loop liveness the listening port alone never proves the loop still turns

#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <system_error>

namespace knc {

/// delay before the accept loop arms again zero means at once avoids a hot spin on repeated errors
int acceptRetryDelayMs(const std::error_code& ec);

/// true when the error kills the acceptor for good every other error is transient
bool acceptErrorIsFatal(const std::error_code& ec);

/// stamp refreshed from inside the loop so a probe can tell a deaf listener from a live one
class ListenerHealth {
public:
    ListenerHealth() { touch(); }

    /// marks the loop alive right now cheap enough for every accept
    void touch();

    /// milliseconds since the last touch
    int64_t ageMs() const;

    /// true when the loop has not touched the stamp inside the window
    bool stale(int64_t maxAgeMs) const;

    /// file the container probe reads an empty path turns the file off
    void setFile(const std::string& path) { m_file = path; }
    const std::string& file() const { return m_file; }

    /// writes the unix seconds of the last touch returns false when the file cannot be written
    bool flush() const;

private:
    std::string m_file;
    std::atomic<int64_t> m_lastMs{0};
};

}
