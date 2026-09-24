/// signup page served by the login server itself no PHP writes the exact pbkdf2 hash format PasswordHash uses
#pragma once

#include <cstdint>
#include <string>

namespace knc {

/// starts the signup page on a background thread safe to call once a bind failure never stops the game server
void StartRegisterWeb(uint16_t port, const std::string& siteName);

/// stop the page and join its thread safe if it was never started
void StopRegisterWeb();

}  // namespace knc
