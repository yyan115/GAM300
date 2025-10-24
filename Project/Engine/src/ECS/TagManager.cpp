#include "pch.h"
#include "ECS/TagManager.hpp"

TagManager& TagManager::GetInstance() {
    static TagManager instance;
    return instance;
}

TagManager::TagManager() {
    InitializeDefaultTags();
}

int TagManager::AddTag(const std::string& tag) {
    if (tag.empty()) return -1;

    auto it = tagToIndex.find(tag);
    if (it != tagToIndex.end()) {
        return it->second; // Tag already exists
    }

    int index = static_cast<int>(tags.size());
    tags.push_back(tag);
    tagToIndex[tag] = index;
    return index;
}

bool TagManager::RemoveTag(const std::string& tag) {
    auto it = tagToIndex.find(tag);
    if (it == tagToIndex.end()) return false;

    int index = it->second;
    return RemoveTag(index);
}

bool TagManager::RemoveTag(int index) {
    if (index < 0 || index >= static_cast<int>(tags.size())) return false;

    std::string tagName = tags[index];

    // Remove from vector
    tags.erase(tags.begin() + index);

    // Update the index mapping
    tagToIndex.clear();
    for (int i = 0; i < static_cast<int>(tags.size()); ++i) {
        tagToIndex[tags[i]] = i;
    }

    return true;
}

int TagManager::GetTagIndex(const std::string& tag) const {
    auto it = tagToIndex.find(tag);
    return (it != tagToIndex.end()) ? it->second : -1;
}

const std::string& TagManager::GetTagName(int index) const {
    if (index >= 0 && index < static_cast<int>(tags.size())) {
        return tags[index];
    }
    static const std::string emptyString = "";
    return emptyString;
}

void TagManager::InitializeDefaultTags() {
    // Unity-like default tags
    AddTag("Untagged");
    AddTag("Respawn");
    AddTag("Finish");
    AddTag("EditorOnly");
    AddTag("MainCamera");
    AddTag("Player");
    AddTag("GameController");
    AddTag("Enemy");
    AddTag("NPC");
    AddTag("Collectible");
}