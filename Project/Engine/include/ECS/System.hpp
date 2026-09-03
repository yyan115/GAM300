#pragma once

#include "Entity.hpp"
#include <cstdint>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

// Membership changes are relatively rare, while every system iterates its
// entities every frame. Keep deterministic set membership and expose a lazily
// rebuilt contiguous view for cache-friendly traversal.
class SystemEntitySet {
public:
    using const_iterator = std::vector<Entity>::const_iterator;

    void insert(Entity entity) {
        if (m_members.insert(entity).second) {
            m_denseDirty = true;
            ++m_version;
        }
    }

    void erase(Entity entity) {
        if (m_members.erase(entity) != 0) {
            m_denseDirty = true;
            ++m_version;
        }
    }

    void clear() {
        if (!m_members.empty()) {
            m_members.clear();
            m_denseDirty = true;
            ++m_version;
        }
    }

    bool contains(Entity entity) const {
        return m_members.find(entity) != m_members.end();
    }

    bool empty() const { return m_members.empty(); }
    std::size_t size() const { return m_members.size(); }
    std::uint64_t Version() const noexcept { return m_version; }

    const_iterator begin() const {
        RebuildDenseView();
        return m_dense.begin();
    }

    const_iterator end() const {
        RebuildDenseView();
        return m_dense.end();
    }

    const std::vector<Entity>& DenseView() const {
        RebuildDenseView();
        return m_dense;
    }

private:
    void RebuildDenseView() const {
        if (!m_denseDirty) return;
        m_dense.assign(m_members.begin(), m_members.end());
        m_denseDirty = false;
    }

    std::set<Entity> m_members;
    mutable std::vector<Entity> m_dense;
    mutable bool m_denseDirty = true;
    std::uint64_t m_version = 1;
};

class System {
public:
    SystemEntitySet entities;
    
    // Get the display name of this system (auto-extracted from class name)
    virtual std::string GetSystemName() const {
        // Extract class name from typeid, removing "class " prefix if present
        std::string fullName = typeid(*this).name();
        
        // On MSVC, typeid().name() returns "class ClassName"
        // On GCC/Clang, it returns mangled names
        size_t classPos = fullName.find("class ");
        if (classPos != std::string::npos) {
            fullName = fullName.substr(classPos + 6);
        }
        
        // Remove "System" suffix if present for cleaner names
        size_t systemPos = fullName.find("System");
        if (systemPos != std::string::npos && systemPos == fullName.length() - 6) {
            fullName = fullName.substr(0, systemPos);
        }
        
        return fullName;
    }
    
    virtual ~System() = default;
};
