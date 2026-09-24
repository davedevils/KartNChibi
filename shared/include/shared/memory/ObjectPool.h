
#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <mutex>

namespace KnC {
namespace Memory {

/// thread safe object pool that pre allocates objects for fast acquire and release
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t initialSize = 32)
        : m_growSize(initialSize)
    {
        Reserve(initialSize);
    }
    
    ~ObjectPool() {
        Clear();
    }
    
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ObjectPool(ObjectPool&&) = default;
    ObjectPool& operator=(ObjectPool&&) = default;
    
    T* Acquire() {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_available.empty()) {
            Grow(m_growSize);
        }

        T* obj = m_available.back();
        m_available.pop_back();
        m_inUse++;
        
        return obj;
    }
    
    void Release(T* obj) {
        if (!obj) return;
        
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_resetter) {
            m_resetter(obj);
        }

        m_available.push_back(obj);
        m_inUse--;
    }
    
    void SetResetter(std::function<void(T*)> resetter) {
        m_resetter = std::move(resetter);
    }
    
    void Reserve(size_t count) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Grow(count);
    }
    
    size_t GetTotalCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_storage.size();
    }
    
    size_t GetAvailableCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_available.size();
    }
    
    size_t GetInUseCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_inUse;
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& ptr : m_storage) {
            delete ptr;
        }
        
        m_storage.clear();
        m_available.clear();
        m_inUse = 0;
    }

private:
    void Grow(size_t count) {
        // no lock here since the caller must lock
        
        size_t oldSize = m_storage.size();
        m_storage.reserve(oldSize + count);
        m_available.reserve(oldSize + count);
        
        for (size_t i = 0; i < count; i++) {
            T* obj = new T();
            m_storage.push_back(obj);
            m_available.push_back(obj);
        }
    }
    
    std::vector<T*> m_storage;
    std::vector<T*> m_available;
    size_t m_inUse = 0;
    size_t m_growSize;

    std::function<void(T*)> m_resetter;

    mutable std::mutex m_mutex;
};

/// RAII wrapper that automatically releases the object back to the pool
template<typename T>
class PooledObject {
public:
    PooledObject(ObjectPool<T>* pool, T* obj)
        : m_pool(pool), m_obj(obj) {}
    
    ~PooledObject() {
        if (m_pool && m_obj) {
            m_pool->Release(m_obj);
        }
    }
    
    PooledObject(const PooledObject&) = delete;
    PooledObject& operator=(const PooledObject&) = delete;

    PooledObject(PooledObject&& other) noexcept
        : m_pool(other.m_pool), m_obj(other.m_obj)
    {
        other.m_pool = nullptr;
        other.m_obj = nullptr;
    }
    
    PooledObject& operator=(PooledObject&& other) noexcept {
        if (this != &other) {
            if (m_pool && m_obj) {
                m_pool->Release(m_obj);
            }

            m_pool = other.m_pool;
            m_obj = other.m_obj;
            other.m_pool = nullptr;
            other.m_obj = nullptr;
        }
        return *this;
    }
    
    T* operator->() { return m_obj; }
    const T* operator->() const { return m_obj; }
    
    T& operator*() { return *m_obj; }
    const T& operator*() const { return *m_obj; }
    
    T* Get() { return m_obj; }
    const T* Get() const { return m_obj; }
    
    explicit operator bool() const { return m_obj != nullptr; }

private:
    ObjectPool<T>* m_pool;
    T* m_obj;
};

template<typename T>
PooledObject<T> AcquireScoped(ObjectPool<T>& pool) {
    return PooledObject<T>(&pool, pool.Acquire());
}

}}

