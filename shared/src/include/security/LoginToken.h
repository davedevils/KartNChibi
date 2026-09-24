/// the launcher token rule of the direct login path 0x07 action 4

#pragma once
#include <functional>
#include <string>

namespace knc {

/// AuthHandler generateToken hands out 32 letters and digits
inline bool looksLikeLauncherToken(const std::string& s) {
    if (s.size() != 32) return false;
    for (char c : s) {
        const bool alnum = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!alnum) return false;
    }
    return true;
}

using LauncherTokenCheck = std::function<bool(const std::string& user, const std::string& token)>;

/// a password shaped like a token logs in only when the launcher store holds that token for that user
inline bool launcherTokenAccepted(const std::string& user, const std::string& password,
                                  const LauncherTokenCheck& check) {
    if (user.empty() || !looksLikeLauncherToken(password)) return false;
    return check && check(user, password);
}

}
