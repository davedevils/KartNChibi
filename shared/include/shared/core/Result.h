/// Result of T E type for explicit error handling prefer this over exceptions for expected errors

#pragma once

#include <utility>
#include <stdexcept>

namespace KnC {

/// result type that holds either a value or an error
template<typename T, typename E>
class Result {
public:
    Result(const Result&) = default;
    Result(Result&&) noexcept = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) noexcept = default;
    
    ~Result() {
        Destroy();
    }
    
    /// check if Result contains a value
    bool IsOk() const { return m_hasValue; }
    
    /// check if Result contains an error
    bool IsErr() const { return !m_hasValue; }
    
    /// get the value panics if Err
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
    
    /// get the error panics if Ok
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
    
    /// get value or default
    T UnwrapOr(T defaultValue) const {
        return m_hasValue ? m_value : defaultValue;
    }
    
    /// get reference to value or nullptr
    T* Ok() {
        return m_hasValue ? &m_value : nullptr;
    }
    
    const T* Ok() const {
        return m_hasValue ? &m_value : nullptr;
    }
    
    /// get reference to error or nullptr
    E* Err() {
        return !m_hasValue ? &m_error : nullptr;
    }
    
    const E* Err() const {
        return !m_hasValue ? &m_error : nullptr;
    }

private:
    // only constructible through the Ok and Err helpers
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

/// create an Ok result
template<typename T, typename E>
Result<T, E> Ok(T value) {
    return Result<T, E>(std::move(value), true);
}

/// create an Err result
template<typename E, typename T>
Result<T, E> Err(E error) {
    return Result<T, E>(std::move(error));
}

} // namespace KnC

