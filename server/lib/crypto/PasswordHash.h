#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <bcrypt.h>
#  pragma comment(lib, "bcrypt.lib")
#else
#  include <openssl/evp.h>
#  include <openssl/rand.h>
#endif

namespace PasswordHash {

namespace detail {

inline std::string toHex(const unsigned char* data, size_t len) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i)
        ss << std::setw(2) << static_cast<int>(data[i]);
    return ss.str();
}

inline std::vector<unsigned char> fromHex(const std::string& hex) {
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned char b = static_cast<unsigned char>(
            std::stoul(hex.substr(i, 2), nullptr, 16));
        out.push_back(b);
    }
    return out;
}

} // namespace detail

/// hashes a password with PBKDF2 SHA256 into a formatted string with iterations salt and hash
inline std::string hash(const std::string& password, int iterations = 100000) {
    constexpr size_t SALT_LEN = 16;
    constexpr size_t HASH_LEN = 32;

    unsigned char salt[SALT_LEN];
    unsigned char out[HASH_LEN];

#ifdef _WIN32
    // --- Windows CNG path ---
    NTSTATUS st = BCryptGenRandom(
        NULL, salt, static_cast<ULONG>(SALT_LEN),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(st))
        throw std::runtime_error("PasswordHash::hash: BCryptGenRandom failed");

    BCRYPT_ALG_HANDLE hAlg = NULL;
    st = BCryptOpenAlgorithmProvider(
        &hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(st))
        throw std::runtime_error("PasswordHash::hash: BCryptOpenAlgorithmProvider failed");

    st = BCryptDeriveKeyPBKDF2(
        hAlg,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        salt, static_cast<ULONG>(SALT_LEN),
        static_cast<ULONGLONG>(iterations),
        out, static_cast<ULONG>(HASH_LEN),
        0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!BCRYPT_SUCCESS(st))
        throw std::runtime_error("PasswordHash::hash: BCryptDeriveKeyPBKDF2 failed");
#else
    if (RAND_bytes(salt, static_cast<int>(SALT_LEN)) != 1)
        throw std::runtime_error("PasswordHash::hash: RAND_bytes failed");
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            salt, static_cast<int>(SALT_LEN),
            iterations, EVP_sha256(),
            static_cast<int>(HASH_LEN), out) != 1)
        throw std::runtime_error("PasswordHash::hash: PKCS5_PBKDF2_HMAC failed");
#endif

    return "pbkdf2$" + std::to_string(iterations) + "$" +
           detail::toHex(salt, SALT_LEN) + "$" +
           detail::toHex(out, HASH_LEN);
}

/// verifies against the pbkdf2 or legacy sha256 colon format using a constant time compare
inline bool verify(const std::string& password, const std::string& stored) {
    if (stored.rfind("pbkdf2$", 0) == 0) {
        std::string body = stored.substr(7); // skips the pbkdf2 prefix
        std::istringstream ss(body);
        std::string iterStr, saltHex, hashHex;
        if (!std::getline(ss, iterStr, '$') ||
            !std::getline(ss, saltHex, '$') ||
            !std::getline(ss, hashHex))
            return false;
        if (iterStr.empty() || saltHex.empty() || hashHex.empty())
            return false;

        int iterations = 0;
        try { iterations = std::stoi(iterStr); }
        catch (...) { return false; }
        if (iterations <= 0) return false;

        std::vector<unsigned char> salt, expected;
        try {
            salt     = detail::fromHex(saltHex);
            expected = detail::fromHex(hashHex);
        } catch (const std::exception&) {
            return false;  // a corrupted hash is treated as a mismatch
        }
        if (expected.empty()) return false;

        constexpr size_t HASH_LEN = 32;
        std::vector<unsigned char> computed(HASH_LEN, 0);

#ifdef _WIN32
        BCRYPT_ALG_HANDLE hAlg = NULL;
        NTSTATUS st = BCryptOpenAlgorithmProvider(
            &hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (!BCRYPT_SUCCESS(st)) return false;
        st = BCryptDeriveKeyPBKDF2(
            hAlg,
            reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
            static_cast<ULONG>(password.size()),
            salt.data(), static_cast<ULONG>(salt.size()),
            static_cast<ULONGLONG>(iterations),
            computed.data(), static_cast<ULONG>(computed.size()),
            0);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (!BCRYPT_SUCCESS(st)) return false;
#else
        if (PKCS5_PBKDF2_HMAC(
                password.c_str(), static_cast<int>(password.size()),
                salt.data(), static_cast<int>(salt.size()),
                iterations, EVP_sha256(),
                static_cast<int>(computed.size()), computed.data()) != 1)
            return false;
#endif

        // Constant-time comparison
        volatile unsigned char diff = 0;
        size_t cmpLen = (expected.size() < computed.size())
                            ? expected.size() : computed.size();
        for (size_t i = 0; i < cmpLen; ++i)
            diff |= expected[i] ^ computed[i];
        // Also flag length mismatch
        diff |= static_cast<unsigned char>(expected.size() ^ computed.size());
        return diff == 0;

    } else if (stored.rfind("sha256:", 0) == 0) {
        // legacy insecure format is a direct string comparison for migration only
        return stored == "sha256:" + password;
    }

    return false;
}

/// true when the stored hash uses the legacy sha256 format
inline bool isLegacy(const std::string& stored) {
    return stored.rfind("sha256:", 0) == 0;
}

} // namespace PasswordHash
