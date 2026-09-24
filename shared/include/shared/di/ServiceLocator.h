
#pragma once

#include <memory>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>

namespace KnC {
namespace DI {

/// service locator for dependency injection register a service then get or replace it for testing
class ServiceLocator {
public:
    /// registers a service
    template<typename T>
    static void Register(std::shared_ptr<T> service) {
        auto& instance = Instance();
        auto typeIndex = std::type_index(typeid(T));
        instance.m_services[typeIndex] = service;
    }
    
    /// gets a service throws if not registered
    template<typename T>
    static std::shared_ptr<T> Get() {
        auto& instance = Instance();
        auto typeIndex = std::type_index(typeid(T));
        
        auto it = instance.m_services.find(typeIndex);
        if (it == instance.m_services.end()) {
            throw std::runtime_error("Service not registered");
        }
        
        return std::static_pointer_cast<T>(it->second);
    }
    
    /// tries to get a service returns nullptr if not registered
    template<typename T>
    static std::shared_ptr<T> TryGet() {
        auto& instance = Instance();
        auto typeIndex = std::type_index(typeid(T));
        
        auto it = instance.m_services.find(typeIndex);
        if (it == instance.m_services.end()) {
            return nullptr;
        }
        
        return std::static_pointer_cast<T>(it->second);
    }
    
    /// checks if a service is registered
    template<typename T>
    static bool Has() {
        auto& instance = Instance();
        auto typeIndex = std::type_index(typeid(T));
        return instance.m_services.find(typeIndex) != instance.m_services.end();
    }
    
    /// unregisters a service
    template<typename T>
    static void Unregister() {
        auto& instance = Instance();
        auto typeIndex = std::type_index(typeid(T));
        instance.m_services.erase(typeIndex);
    }
    
    /// clears all services
    static void Clear() {
        auto& instance = Instance();
        instance.m_services.clear();
    }

private:
    static ServiceLocator& Instance() {
        static ServiceLocator instance;
        return instance;
    }
    
    ServiceLocator() = default;
    ~ServiceLocator() = default;
    
    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;
    
    std::unordered_map<std::type_index, std::shared_ptr<void>> m_services;
};

}}

