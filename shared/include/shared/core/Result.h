/**
 * @file Result.h
 * @brief Result<T, E> type for error handling
 * 
 * Rust-inspired Result type for explicit error handling.
 * Prefer this over exceptions for expected errors.
 */

#pragma once

#include <utility>
#include <stdexcept>

namespace KnC {

/**
 * @brief Result type that can hold either a value (Ok) or an error (Err)
 * 
 * @tparam T Value type
 * @tparam E Error type
 * 
 * @example
 * Result<int, std::string> divide(int a, int b) {
 *     if (b == 0) {
 *         return Err<std::string>("Division by zero");
 *     }
 *     return Ok(a / b);
 * }
 * 
 * auto result = divide(10, 2);
 * if (result.IsOk()) {
 *     printf("Result: %d\n", result.Unwrap());
 * } else {
 *     printf("Error: %s\n", result.UnwrapErr().c_str());
 * }
 */
template<typename T, typename E>
class Result {
public:
    // Constructors
    Result(const Result&) = default;
    Result(Result&&) noexcept = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) noexcept = default;
    
    ~Result() {
        Destroy();
    }
    
    /**
     * @brief Check if Result contains a value (Ok)
     */
    bool IsOk() const { return m_hasValue; }
    
    /**
     * @brief Check if Result contains an error (Err)
     */
    bool IsErr() const { return !m_hasValue; }
    
    /**
     * @brief Get the value (panics if Err)
     */
    T& Unwrap() {
        if (!m_hasValue) {
            throw std::runtime_error("Called Unwrap() on Err");
        }
        return m_value;
    }
    
    const T& Unwrap() const {
        if (!m_hasValue) {
            throw std::runtime_error("Called Unwrap() on Err");
        }
        return m_value;
    }
    
    /**
     * @brief Get the error (panics if Ok)
     */
    E& UnwrapErr() {
        if (m_hasValue) {
            throw std::runtime_error("Called UnwrapErr() on Ok");
        }
        return m_error;
    }
    
    const E& UnwrapErr() const {
        if (m_hasValue) {
            throw std::runtime_error("Called UnwrapErr() on Ok");
        }
        return m_error;
    }
    
    /**
     * @brief Get value or default
     */
    T UnwrapOr(T defaultValue) const {
        return m_hasValue ? m_value : defaultValue;
    }
    
    /**
     * @brief Get reference to value or nullptr
     */
    T* Ok() {
        return m_hasValue ? &m_value : nullptr;
    }
    
    const T* Ok() const {
        return m_hasValue ? &m_value : nullptr;
    }
    
    /**
     * @brief Get reference to error or nullptr
     */
    E* Err() {
        return !m_hasValue ? &m_error : nullptr;
    }
    
    const E* Err() const {
        return !m_hasValue ? &m_error : nullptr;
    }

private:
    // Only constructible through Ok() and Err() helpers
    template<typename, typename>
    friend class Result;
    
    template<typename U, typename F>
    friend Result<U, F> Ok(U value);
    
    template<typename F, typename U>
    friend Result<U, F> Err(F error);
    
    explicit Result(T value, bool) : m_hasValue(true) {
        new (&m_value) T(std::move(value));
    }
    
    explicit Result(E error) : m_hasValue(false) {
        new (&m_error) E(std::move(error));
    }
    
    void Destroy() {
        if (m_hasValue) {
            m_value.~T();
        } else {
            m_error.~E();
        }
    }
    
    union {
        T m_value;
        E m_error;
    };
    bool m_hasValue;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Create an Ok result
 * Usage: Ok<int, std::string>(42)
 */
template<typename T, typename E>
Result<T, E> Ok(T value) {
    return Result<T, E>(std::move(value), true);
}

/**
 * @brief Create an Err result
 * Usage: Err<std::string, int>("error")
 */
template<typename E, typename T>
Result<T, E> Err(E error) {
    return Result<T, E>(std::move(error));
}

} // namespace KnC

