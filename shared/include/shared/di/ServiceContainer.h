#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <typeindex>
#include <stdexcept>

namespace KnC {

/// service lifetime management
enum class ServiceLifetime {
    /// one instance for the entire application lifetime for example logger profiler config manager
    Singleton,
    
    /// one instance per scope such as per frame or per level
    Scoped,
    
    /// new instance every time for temporary or one off objects
    Transient
};

/// type safe dependency injection container with singleton scoped and transient lifetimes and constructor injection
class ServiceContainer {
public:
    ServiceContainer() = default;
    ~ServiceContainer() = default;

    // copy disabled this follows the singleton pattern
    ServiceContainer(const ServiceContainer&) = delete;
    ServiceContainer& operator=(const ServiceContainer&) = delete;

    /// registers a service as singleton via a factory function
    template<typename TInterface, typename TImplementation = TInterface>
    void RegisterSingleton(std::function<std::shared_ptr<TInterface>()> factory) {
        auto typeId = std::type_index(typeid(TInterface));
        
        ServiceEntry entry;
        entry.lifetime = ServiceLifetime::Singleton;
        entry.factory = [factory]() -> std::shared_ptr<void> {
            return std::static_pointer_cast<void>(factory());
        };
        entry.typeName = typeid(TInterface).name();
        
        m_services[typeId] = entry;
    }

    /// registers a service as scoped
    template<typename TInterface, typename TImplementation = TInterface>
    void RegisterScoped(std::function<std::shared_ptr<TInterface>()> factory) {
        auto typeId = std::type_index(typeid(TInterface));
        
        ServiceEntry entry;
        entry.lifetime = ServiceLifetime::Scoped;
        entry.factory = [factory]() -> std::shared_ptr<void> {
            return std::static_pointer_cast<void>(factory());
        };
        entry.typeName = typeid(TInterface).name();
        
        m_services[typeId] = entry;
    }

    /// registers a service as transient
    template<typename TInterface, typename TImplementation = TInterface>
    void RegisterTransient(std::function<std::shared_ptr<TInterface>()> factory) {
        auto typeId = std::type_index(typeid(TInterface));
        
        ServiceEntry entry;
        entry.lifetime = ServiceLifetime::Transient;
        entry.factory = [factory]() -> std::shared_ptr<void> {
            return std::static_pointer_cast<void>(factory());
        };
        entry.typeName = typeid(TInterface).name();
        
        m_services[typeId] = entry;
    }

    /// registers an existing instance as singleton
    template<typename TInterface>
    void RegisterInstance(std::shared_ptr<TInterface> instance) {
        auto typeId = std::type_index(typeid(TInterface));
        
        ServiceEntry entry;
        entry.lifetime = ServiceLifetime::Singleton;
        entry.instance = std::static_pointer_cast<void>(instance);
        entry.typeName = typeid(TInterface).name();
        
        m_services[typeId] = entry;
    }

    /// resolves a service by type throws if not registered
    template<typename TInterface>
    std::shared_ptr<TInterface> Resolve() {
        auto typeId = std::type_index(typeid(TInterface));
        
        auto it = m_services.find(typeId);
        if (it == m_services.end()) {
            throw std::runtime_error(std::string("Service not registered: ") + typeid(TInterface).name());
        }
        
        ServiceEntry& entry = it->second;
        
        switch (entry.lifetime) {
            case ServiceLifetime::Singleton: {
                // Return cached instance or create new
                if (!entry.instance) {
                    if (!entry.factory) {
                        throw std::runtime_error("No factory registered for singleton");
                    }
                    entry.instance = entry.factory();
                }
                return std::static_pointer_cast<TInterface>(entry.instance);
            }
            
            case ServiceLifetime::Scoped: {
                // Return scoped instance or create new
                if (!entry.scopedInstance) {
                    if (!entry.factory) {
                        throw std::runtime_error("No factory registered for scoped service");
                    }
                    entry.scopedInstance = entry.factory();
                }
                return std::static_pointer_cast<TInterface>(entry.scopedInstance);
            }
            
            case ServiceLifetime::Transient: {
                // Always create new instance
                if (!entry.factory) {
                    throw std::runtime_error("No factory registered for transient service");
                }
                return std::static_pointer_cast<TInterface>(entry.factory());
            }
        }
        
        throw std::runtime_error("Invalid service lifetime");
    }

    /// tries to resolve a service returns nullptr if not found
    template<typename TInterface>
    std::shared_ptr<TInterface> TryResolve() {
        try {
            return Resolve<TInterface>();
        } catch (...) {
            return nullptr;
        }
    }

    /// checks if a service is registered
    template<typename TInterface>
    bool IsRegistered() const {
        auto typeId = std::type_index(typeid(TInterface));
        return m_services.find(typeId) != m_services.end();
    }

    /// begins a new scope clearing scoped instances call at frame start or level load
    void BeginScope() {
        for (auto& pair : m_services) {
            if (pair.second.lifetime == ServiceLifetime::Scoped) {
                pair.second.scopedInstance.reset();
            }
        }
    }

    /// ends scope currently the same as beginScope
    void EndScope() {
        BeginScope();
    }

    /// clears all services
    void Clear() {
        m_services.clear();
    }

    /// returns the service count
    size_t GetServiceCount() const {
        return m_services.size();
    }

    /// service info for debugging
    struct ServiceInfo {
        std::string typeName;
        ServiceLifetime lifetime;
        bool hasInstance;
        long refCount;
    };

    std::vector<ServiceInfo> GetAllServices() const {
        std::vector<ServiceInfo> result;
        
        for (const auto& pair : m_services) {
            ServiceInfo info;
            info.typeName = pair.second.typeName;
            info.lifetime = pair.second.lifetime;
            
            switch (pair.second.lifetime) {
                case ServiceLifetime::Singleton:
                    info.hasInstance = pair.second.instance != nullptr;
                    info.refCount = info.hasInstance ? pair.second.instance.use_count() : 0;
                    break;
                case ServiceLifetime::Scoped:
                    info.hasInstance = pair.second.scopedInstance != nullptr;
                    info.refCount = info.hasInstance ? pair.second.scopedInstance.use_count() : 0;
                    break;
                case ServiceLifetime::Transient:
                    info.hasInstance = false;
                    info.refCount = 0;
                    break;
            }
            
            result.push_back(info);
        }
        
        return result;
    }

private:
    struct ServiceEntry {
        ServiceLifetime lifetime;
        std::function<std::shared_ptr<void>()> factory;
        std::shared_ptr<void> instance;
        std::shared_ptr<void> scopedInstance;
        std::string typeName;
    };

    std::unordered_map<std::type_index, ServiceEntry> m_services;
};

/// global convenience container pass ServiceContainer explicitly for more complex scenarios
class Services {
public:
    static ServiceContainer& Instance() {
        static ServiceContainer instance;
        return instance;
    }

    template<typename T>
    static std::shared_ptr<T> Get() {
        return Instance().Resolve<T>();
    }

    template<typename T>
    static std::shared_ptr<T> TryGet() {
        return Instance().TryResolve<T>();
    }

    template<typename T>
    static bool Has() {
        return Instance().IsRegistered<T>();
    }
};

} // namespace KnC
