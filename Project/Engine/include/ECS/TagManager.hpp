/* Start Header ************************************************************************/
/*!
\file       TagManager.hpp
\author     Muhammad Zikry
\date       2025
\brief      Manages tags within the ECS system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include "Engine.h"

class ENGINE_API TagManager {
public:
    static TagManager& GetInstance();

    // Tag management
    int AddTag(const std::string& tag);
    bool RemoveTag(const std::string& tag);
    bool RemoveTag(int index);
    int GetTagIndex(const std::string& tag) const;
    const std::string& GetTagName(int index) const;
    const std::vector<std::string>& GetAllTags() const { return tags; }
    int GetTagCount() const { return static_cast<int>(tags.size()); }

    // Default tags
    void InitializeDefaultTags();

private:
    TagManager();
    ~TagManager() = default;
    TagManager(const TagManager&) = delete;
    TagManager& operator=(const TagManager&) = delete;

    std::vector<std::string> tags;
    std::unordered_map<std::string, int> tagToIndex;
};