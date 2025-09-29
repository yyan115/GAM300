#pragma once
/*********************************************************************************
* @File			ReflectionBase.hpp
* @Author		Soh Wei Jie, weijie.soh@digipen.edu
* @Co-Author	-
* @Date			26/9/2025
* @Brief        Implements the engine's reflection and persistence building blocks:
*                   - TypeDescriptor base and concrete descriptors for user structs, STL containers
*                     (std::vector, std::unordered_map, std::pair), std::shared_ptr, and a GUID_128 type.
*                   - TypeResolver machinery to obtain descriptors for reflected and primitive types,
*                     with lazy resolution support for templated containers.
*                   - JSON serialization/deserialization designed around rapidjson (Writer + Document),
*                     producing compact JSON for storage and network interchange.
*                   - Thread-safe type registry protected by a mutex; registration macros
*                     (REFL_SERIALIZABLE / REFL_REGISTER_START / REFL_REGISTER_PROPERTY / REFL_REGISTER_END)
*                     to declare reflection for user types.
*                   - Portable persistence helpers (little-endian conversion), alignment macro, and
*                     Base64 encoding/decoding used to serialize opaque shared_ptr<void> blobs (expects
*                     a size-prefix contract).
*                   - Mobile-friendly behavior: avoids global namespace pollution, throws descriptive
*                     exceptions for malformed input or resolution failures (rather than asserts),
*                     and guards descriptor registration to be safe on resource-constrained platforms.
*                   Usage notes: user types must call InitReflection via the provided macros to register
*                   members; TypeResolver will return primitive descriptors for non-reflected types.
*
* Copyright (C) 2025 DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/
#include "pch.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
// Note: intentionally *not* using `using namespace rapidjson;` in header to avoid polluting global namespace.

#include "Base64.hpp"
#include "Utilities/GUID.hpp"

#include <type_traits>

#ifdef _WIN32
    #ifdef ENGINE_EXPORTS
        #define ENGINE_API __declspec(dllexport)
    #else
        #define ENGINE_API __declspec(dllimport)
    #endif
#else
    // Linux/GCC
    #ifdef ENGINE_EXPORTS
        #define ENGINE_API __attribute__((visibility("default")))
    #else
        #define ENGINE_API
    #endif
#endif

// The compiler ensures that the starting address of the variable is divisible by X.
// 
// Example usage: __FLX_ALIGNAS(16) float myArray[4];
// Each float is 4 bytes, so the array is 16 bytes.
// 
// Proper alignment can improve memory access performance, particularly when dealing with
// vectorized operations, SIMD (Single Instruction, Multiple Data) instructions, or GPU operations.
// 
// Misaligned memory access can result in performance penalties because the CPU or GPU
// may need to perform additional work to handle unaligned access.
#define ENGINE_ALIGNAS(X) alignas(X)

#pragma region Reflection

// Helper for endianness conversion for persisted integer sizes.
// CHANGE: use fixed-width conversions so persistence is stable across ABIs (32/64-bit).
static inline uint64_t ToLittleEndian_u64(uint64_t v)
{
    // On little-endian architectures this is a no-op. For generality, provide a portable conversion.
    // Most Android devices are little-endian but doing this guarantees correctness.
    uint64_t out = 0;
    uint8_t* src = reinterpret_cast<uint8_t*>(&v);
    for (size_t i = 0; i < sizeof(uint64_t); ++i) out |= static_cast<uint64_t>(src[i]) << (8 * i);
    return out;
}

static inline uint64_t FromLittleEndian_u64(uint64_t v)
{
    // symmetric to ToLittleEndian_u64 for portability.
    uint64_t out = 0;
    uint8_t* src = reinterpret_cast<uint8_t*>(&v);
    for (size_t i = 0; i < sizeof(uint64_t); ++i) out |= static_cast<uint64_t>(src[i]) << (8 * i);
    return out;
}

struct DefaultResolver;
struct TypeDescriptor_Struct;

// ---------- TypeDescriptor (PIMPL) ----------
// Exported type must not contain STL members directly to avoid C4251.
// We therefore keep an opaque Impl* pointer here and provide accessor APIs.
// ctor/dtor and Impl are implemented in the .cpp so all STL allocations happen
// inside the DLL.
struct ENGINE_API TypeDescriptor
{
    using json = rapidjson::Value;

    // opaque impl pointer (defined in ReflectionBase.cpp)
    struct Impl;
    Impl* pImpl;

    // ctor/dtor are defined in the .cpp (so allocations/deallocations occur inside DLL)
    TypeDescriptor(const std::string& name_, size_t size_);
    virtual ~TypeDescriptor();

    // non-copyable to avoid accidental cross-CRT copies of internal state
    TypeDescriptor(const TypeDescriptor&) = delete;
    TypeDescriptor& operator=(const TypeDescriptor&) = delete;

    // minimal accessors to avoid exposing std::string in the header
    const char* GetName() const;
    void SetName(const char* name);
    size_t GetSize() const;
    void SetSize(size_t s);

    bool operator<(const TypeDescriptor& other) const { return std::string(GetName()) < std::string(other.GetName()); }
    virtual std::string ToString() const { return std::string(GetName() ? GetName() : ""); }

    virtual void Dump(const void* obj, std::ostream& os = std::cout, int indent_level = 0) const = 0;
    virtual void Serialize(const void* obj, std::ostream& out) const = 0;

    // CHANGE: Provide SerializeJson adaptor (can be overridden)
    virtual void SerializeJson(const void* obj, rapidjson::Document& out) const;

    virtual void Deserialize(void* obj, const json& value) const = 0;

    // Registry helpers (function bodies may be implemented inline; they return references to TU-local statics)
    static std::unordered_map<std::string, TypeDescriptor*>& type_descriptor_lookup();
    static std::mutex& descriptor_registry_mutex();
};

#define TYPE_DESCRIPTOR_LOOKUP TypeDescriptor::type_descriptor_lookup()

// Forward-declare primitive descriptor getter
template <typename T>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor();

// DefaultResolver unchanged in behavior (relies on T::Reflection detection)
struct DefaultResolver
{
    template <typename T> static char func(decltype(&T::Reflection));
    template <typename T> static int func(...);
    template <typename T>
    struct IsReflected { enum { value = (sizeof(func<T>(nullptr)) == sizeof(char)) }; };

    template <typename T, typename std::enable_if<IsReflected<T>::value, int>::type = 0>
    static TypeDescriptor* Get() { return &T::Reflection; }

    template <typename T, typename std::enable_if<!IsReflected<T>::value, int>::type = 0>
    static TypeDescriptor* Get() { return GetPrimitiveDescriptor<T>(); }
};

template <typename T>
struct TypeResolver { static TypeDescriptor* Get() { return DefaultResolver::Get<T>(); } };

// -------------------------------------------------------------
// TypeDescriptor for user-defined structs/classes (PIMPL friendly).
// Member contains only PODs / pointers so it can be used in header safely.
// The actual vector<Member> storage is kept inside the .cpp via a struct_impl pointer.
// -------------------------------------------------------------
struct ENGINE_API TypeDescriptor_Struct : TypeDescriptor
{
    // opaque pointer to struct-specific implementation (allocated in .cpp)
    // keep as void* to remain ABI-stable (POD pointer)
    void* struct_impl = nullptr;

    struct Member { const char* name; /*size_t offset;*/ TypeDescriptor* type; void* (*get_ptr)(void*); };

    // construct by passing an init function (macros will use this)
    TypeDescriptor_Struct(void (*init)(TypeDescriptor_Struct*));
    virtual ~TypeDescriptor_Struct();

    // API used by macros to set name/size/members without exposing std::vector
    void SetMembers(const std::initializer_list<Member>& members);
    std::vector<Member> GetMembers() const; // returns a copy (uses std::vector allocated inside dll)

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override;
    virtual void Serialize(const void* obj, std::ostream& os) const override;
    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override;
};

// -------------------------------------------------------------
// std::vector specialization (descriptor). This class contains function pointers and
// a TypeDescriptor* but does not hold std::vector as a direct data member in the header.
// It will call SetName(...) when the item_type is resolved so that no std::string member exists
// in the exported base class layout.
struct TypeDescriptor_StdVector : TypeDescriptor
{
    using ResolverFn = TypeDescriptor * (*)();

    TypeDescriptor* item_type;
    ResolverFn resolver; // function to lazily get the item descriptor
    size_t(*get_size)(const void*);
    const void* (*get_item)(const void*, size_t);
    void* (*set_item)(void*, size_t);

    template <typename ItemType>
    TypeDescriptor_StdVector(ItemType*)
        : TypeDescriptor{ "std::vector<>", sizeof(std::vector<ItemType>) }
        , item_type{ nullptr }
    , resolver{ +[]() -> TypeDescriptor* { return TypeResolver<ItemType>::Get(); } }
    {
        get_size = [](const void* vec_ptr) -> size_t {
            const auto& vec = *(const std::vector<ItemType>*) vec_ptr;
            return vec.size();
            };
        get_item = [](const void* vec_ptr, size_t index) -> const void* {
            const auto& vec = *(const std::vector<ItemType>*) vec_ptr;
            return &vec[index];
            };
        set_item = [](void* vec_ptr, size_t index) -> void* {
            auto& vec = *(std::vector<ItemType>*) vec_ptr;
            if (index >= vec.size()) vec.resize(index + 1);
            return &vec[index];
            };

        // Try eager resolve now (optional). If item_type is resolved we set a proper name.
        item_type = resolver();
        if (item_type)
        {
            // call SetName to record the fully-qualified name into the TypeDescriptor impl
            this->SetName((std::string("std::vector<") + item_type->ToString() + ">").c_str());
            std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
            if (TYPE_DESCRIPTOR_LOOKUP.count(this->ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[this->ToString()] = this;
        }
        else
        {
            // keep a placeholder name; will compute once resolved
            this->SetName("std::vector<unresolved>");
        }
    }

private:
    // call before any use of item_type
    void EnsureResolvedForUse()
    {
        if (item_type) return;
        // call resolver to obtain item_type now
        item_type = resolver();
        if (!item_type)
        {
            throw std::runtime_error("TypeDescriptor_StdVector: failed to resolve item type for vector; ensure the item type is reflected or a primitive.");
        }
        // set the final name and register
        this->SetName((std::string("std::vector<") + item_type->ToString() + ">").c_str());
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(this->ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[this->ToString()] = this;
    }

public:
    virtual std::string ToString() const override
    {
        if (!item_type) const_cast<TypeDescriptor_StdVector*>(this)->EnsureResolvedForUse();
        return std::string("std::vector<") + item_type->ToString() + ">";
    }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override;
    virtual void Serialize(const void* obj, std::ostream& os) const override;
    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override;
};

template <typename T>
struct TypeResolver<std::vector<T>>
{
    static TypeDescriptor* Get()
    {
        static TypeDescriptor_StdVector type_desc{ (T*) nullptr };
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.ToString()] = &type_desc;
        return &type_desc;
    }
};

// -------------------------------------------------------------
// std::unordered_map<KeyType, ValueType> specialization
// (keeps pointer fields only in header)
template <typename KeyType, typename ValueType>
struct TypeDescriptor_StdUnorderedMap : TypeDescriptor
{
    TypeDescriptor* key_type;
    TypeDescriptor* value_type;

    TypeDescriptor_StdUnorderedMap(std::unordered_map<KeyType, ValueType>*)
        : TypeDescriptor{ "std::unordered_map<>", sizeof(std::unordered_map<KeyType, ValueType>) }
        , key_type{ TypeResolver<KeyType>::Get() }
        , value_type{ TypeResolver<ValueType>::Get() }
    {
        this->SetName((std::string("std::unordered_map<") + key_type->ToString() + ", " + value_type->ToString() + ">").c_str());
    }

    virtual std::string ToString() const override { return std::string("std::unordered_map<") + key_type->ToString() + ", " + value_type->ToString() + ">"; }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override;
    virtual void Serialize(const void* obj, std::ostream& os) const override;
    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override;
};

template <typename KeyType, typename ValueType>
struct TypeResolver<std::unordered_map<KeyType, ValueType>>
{
    static TypeDescriptor* Get()
    {
        static TypeDescriptor_StdUnorderedMap<KeyType, ValueType> type_desc{ (std::unordered_map<KeyType, ValueType>*)nullptr };
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.ToString()] = &type_desc;
        return &type_desc;
    }
};

// -------------------------------------------------------------
// std::shared_ptr<T> specialization (keeps only pointers in header)
template <typename T>
struct TypeDescriptor_StdSharedPtr : TypeDescriptor
{
    TypeDescriptor* item_type;

    TypeDescriptor_StdSharedPtr(T*)
        : TypeDescriptor{ "std::shared_ptr<>", sizeof(std::shared_ptr<T>) }
        , item_type{ TypeResolver<T>::Get() }
    {
        this->SetName((std::string("std::shared_ptr<") + item_type->ToString() + ">").c_str());
    }

    virtual std::string ToString() const override { return std::string("std::shared_ptr<") + item_type->ToString() + ">"; }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override;
    virtual void Serialize(const void* obj, std::ostream& os) const override;
    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override;
};

template <>
struct TypeDescriptor_StdSharedPtr<void> : TypeDescriptor
{
    TypeDescriptor* item_type;
    TypeDescriptor_StdSharedPtr(void*)
        : TypeDescriptor{ "std::shared_ptr<>", sizeof(std::shared_ptr<void>) }, item_type{ nullptr }
    {
        this->SetName("std::shared_ptr<void>");
    }

    virtual std::string ToString() const override { return "std::shared_ptr<void>"; }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override;
    virtual void Serialize(const void* obj, std::ostream& os) const override;
    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override;
};

template <typename T>
struct TypeResolver<std::shared_ptr<T>>
{
    static TypeDescriptor* Get()
    {
        static TypeDescriptor_StdSharedPtr type_desc{ (T*) nullptr };
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.ToString()] = &type_desc;
        return &type_desc;
    }
};

// -------------------------------------------------------------
// std::pair specialization
// -------------------------------------------------------------
template <typename FirstType, typename SecondType>
struct TypeDescriptor_StdPair : TypeDescriptor
{
    TypeDescriptor* first_type;
    TypeDescriptor* second_type;

    TypeDescriptor_StdPair(std::pair<FirstType, SecondType>*)
        : TypeDescriptor{ "std::pair<>", sizeof(std::pair<FirstType, SecondType>) }
        , first_type{ TypeResolver<FirstType>::Get() }
        , second_type{ TypeResolver<SecondType>::Get() }
    {
        this->SetName((std::string("std::pair<") + first_type->ToString() + ", " + second_type->ToString() + ">").c_str());
    }

    virtual std::string ToString() const override { return std::string("std::pair<") + first_type->ToString() + ", " + second_type->ToString() + ">"; }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override;
    virtual void Serialize(const void* obj, std::ostream& os) const override;
    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override;
};

template <typename FirstType, typename SecondType>
struct TypeResolver<std::pair<FirstType, SecondType>>
{
    static TypeDescriptor* Get()
    {
        static TypeDescriptor_StdPair<FirstType, SecondType> type_desc{ (std::pair<FirstType, SecondType>*)nullptr };
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.ToString()) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.ToString()] = &type_desc;
        return &type_desc;
    }
};

struct TypeDescriptor_GUID128 : TypeDescriptor {
    TypeDescriptor_GUID128();
    virtual ~TypeDescriptor_GUID128() = default;

    void Dump(const void* obj, std::ostream& os, int) const override;
    void Serialize(const void* obj, std::ostream& os) const override;
    void Deserialize(void* obj, const rapidjson::Value& value) const override;
};

template<> struct TypeResolver<GUID_128> {
    static TypeDescriptor* Get();
};

#pragma endregion

#pragma region Macros
// Keep macros outside namespace to preserve original API shape

#define REFL_SERIALIZABLE \
  friend struct DefaultResolver; \
  static TypeDescriptor_Struct Reflection; \
  static void InitReflection(TypeDescriptor_Struct*);

#define REFL_REGISTER_START(TYPE) \
  TypeDescriptor_Struct TYPE::Reflection{TYPE::InitReflection}; \
  void TYPE::InitReflection(TypeDescriptor_Struct* type_desc) \
  { \
    using T = TYPE; \
    type_desc->SetName(#TYPE); \
    type_desc->SetSize(sizeof(T)); \
    type_desc->SetMembers({

#define REFL_REGISTER_PROPERTY(VARIABLE) \
  TypeDescriptor_Struct::Member{ \
    #VARIABLE, \
    TypeResolver<decltype(T::VARIABLE)>::Get(), \
    +[](void* obj) -> void* { return & (static_cast<T*>(obj)->VARIABLE); } \
  },

#define REFL_REGISTER_END \
    }); \
    { std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex()); \
      TypeDescriptor::type_descriptor_lookup()[std::string(type_desc->GetName())] = type_desc; } \
  }
#pragma endregion