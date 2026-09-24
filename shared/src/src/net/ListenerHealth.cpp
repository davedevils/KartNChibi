
#include "net/ListenerHealth.h"

#include <chrono>
#include <cstdio>

namespace knc {

namespace {

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// out of descriptors or out of memory a retry needs a pause
bool isResourceShortage(const std::error_code& ec) {
    const auto cond = ec.default_error_condition();
    if (cond.category() != std::generic_category()) return false;
    switch (static_cast<std::errc>(cond.value())) {
        case std::errc::too_many_files_open:
        case std::errc::too_many_files_open_in_system:
        case std::errc::not_enough_memory:
        case std::errc::no_buffer_space:
            return true;
        default:
            return false;
    }
}

}

int acceptRetryDelayMs(const std::error_code& ec) {
    if (!ec) return 0;
    if (isResourceShortage(ec)) return 250;   // let the descriptors come back before the next try
    return 0;
}

bool acceptErrorIsFatal(const std::error_code& ec) {
    if (!ec) return false;
    // only a closed or cancelled acceptor ends the loop every client side error is transient
    const auto cond = ec.default_error_condition();
    if (cond.category() != std::generic_category()) return false;
    switch (static_cast<std::errc>(cond.value())) {
        case std::errc::bad_file_descriptor:
        case std::errc::invalid_argument:
        case std::errc::operation_canceled:
            return true;
        default:
            return false;
    }
}

void ListenerHealth::touch() {
    m_lastMs.store(nowMs(), std::memory_order_relaxed);
}

int64_t ListenerHealth::ageMs() const {
    const int64_t last = m_lastMs.load(std::memory_order_relaxed);
    const int64_t age = nowMs() - last;
    return age < 0 ? 0 : age;
}

bool ListenerHealth::stale(int64_t maxAgeMs) const {
    return ageMs() > maxAgeMs;
}

bool ListenerHealth::flush() const {
    if (m_file.empty()) return false;
    const auto wall = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
    std::FILE* f = std::fopen(m_file.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "%lld\n", static_cast<long long>(wall));
    std::fclose(f);
    return true;
}

}
