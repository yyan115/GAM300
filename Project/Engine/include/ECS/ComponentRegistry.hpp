// Reflection/ComponentRegistry.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <optional>
#include "Reflection/ReflectionBase.hpp" // for TypeDescriptor & TypeResolver

class ECSManager;
using Entity = unsigned int;
class ComponentRegistry {
public:
    using GetterFn = std::function<void* (ECSManager*, Entity)>;

    struct ComponentInfo {
        GetterFn getter;
        TypeDescriptor* typeDesc = nullptr;
    };

    static ComponentRegistry& Instance() {
        static ComponentRegistry inst;
        return inst;
    }

    // Register by providing a typed getter lambda. We attempt to resolve the TypeDescriptor
    // for T via TypeResolver<T>::Get() automatically.
    template<typename T>
    void Register(const std::string& name, std::function<T* (ECSManager*, Entity)> getter) {
        std::lock_guard<std::mutex> lk(m_mutex);
        ComponentInfo ci;
        // store getter adapted to void*
        ci.getter = [g = std::move(getter)](ECSManager* ecs, Entity e) -> void* {
            T* p = g(ecs, e);
            return reinterpret_cast<void*>(p);
        };
        // try to resolve reflection type descriptor for T
        TypeDescriptor* td = nullptr;
        try {
            td = TypeResolver<T>::Get();
        } catch (...) {
            td = nullptr;
        }
        ci.typeDesc = td;
        m_map[name] = std::move(ci);
    }

    // Alternative: register using a raw GetterFn and explicit TypeDescriptor (if you need to)
    void RegisterRaw(const std::string& name, GetterFn getter, TypeDescriptor* td = nullptr) {
        std::lock_guard<std::mutex> lk(m_mutex);
        ComponentInfo ci;
        ci.getter = std::move(getter);
        ci.typeDesc = td;
        m_map[name] = std::move(ci);
    }

    bool Has(const std::string& name) const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_map.find(name) != m_map.end();
    }

    // Return the GetterFn (or empty std::function if not found)
    GetterFn GetGetter(const std::string& name) const {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_map.find(name);
        if (it == m_map.end()) return GetterFn{};
        return it->second.getter;
    }

    // Fill out a ComponentInfo struct for callers that need the TypeDescriptor too.
    // Returns true if found.
    bool Get(const std::string& name, ComponentInfo& out) const {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_map.find(name);
        if (it == m_map.end()) return false;
        out = it->second;
        return true;
    }

private:
    ComponentRegistry() = default;
    ~ComponentRegistry() = default;
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ComponentInfo> m_map;
};
