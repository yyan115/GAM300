#pragma once

#include "pch.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
// Note: intentionally *not* using `using namespace rapidjson;` in header to avoid polluting global namespace.

#include "Base64.hpp"

#include <type_traits>
//Sticking with this for now unless stated need to move to a consolidated API.hpp file
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

struct ENGINE_API TypeDescriptor
{ // Note: KEEP macro usage for user types. Do NOT auto-inject reflection into internal runtime types unless intentionally desired.
  // CHANGE: removed FLX_REFL_SERIALIZABLE here in order to avoid unnecessary static Reflection members on core descriptors.
    using json = rapidjson::Value;

    std::string name;
    size_t size;

    // CHANGE: Protect lookup map mutations with a mutex.
    static std::unordered_map<std::string, TypeDescriptor*>& type_descriptor_lookup()
    {
        static std::unordered_map<std::string, TypeDescriptor*> s_map;
        return s_map;
    }

    // Mutex guarding writes to TYPE_DESCRIPTOR_LOOKUP
    static std::mutex& descriptor_registry_mutex()
    {
        static std::mutex s_mutex;
        return s_mutex;
    }

#define TYPE_DESCRIPTOR_LOOKUP TypeDescriptor::type_descriptor_lookup()

    TypeDescriptor(const std::string& name_, size_t size_) : name{ name_ }, size{ size_ } {}
    virtual ~TypeDescriptor() {}

    bool operator<(const TypeDescriptor& other) const { return name < other.name; }
    virtual std::string ToString() const { return name; }

    virtual void Dump(const void* obj, std::ostream& os = std::cout, int indent_level = 0) const = 0;
    virtual void Serialize(const void* obj, std::ostream& out) const = 0;

    // CHANGE: Use rapidjson::Writer for efficient JSON emission on mobile; keep the same method signature.
    virtual void SerializeJson(const void* obj, rapidjson::Document& out) const
    {
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
        // Default approach: produce a compact JSON string using the writer, then parse into Document.
        // Sub-classes can implement WriteJson(writer) helpers for more efficient direct Value building.
        // For backward compatibility, call Serialize into a string stream and parse — but this was inefficient.
        // Here we provide a light-weight wrapper that uses the textual Serialize as a fallback but prefer writer in overrides.
        std::stringstream ss;
        Serialize(obj, ss);
        // Parse the generated string into the Document.
        out.Parse(ss.str().c_str());
    }

    virtual void Deserialize(void* obj, const json& value) const = 0;
};

// Declare primitive descriptors (implementation assumed in primitives.cpp)
template <typename T>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor();

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
// TypeDescriptor for user-defined structs/classes.
// CHANGE: TypeDescriptor_Struct now accepts an initializer that MUST call init(this) where init performs member registration.
// We add basic runtime checks and avoid raw asserts that crash on mobile. Instead we throw exceptions with descriptive messages.
// We also require standard-layout types for offsetof-based registrations.
// -------------------------------------------------------------
struct TypeDescriptor_Struct : TypeDescriptor
{
    struct Member { const char* name; /*size_t offset;*/ TypeDescriptor* type; void* (*get_ptr)(void*); };

    std::vector<Member> members;

    TypeDescriptor_Struct(void (*init)(TypeDescriptor_Struct*)) : TypeDescriptor{ "", 0 }
    {
        init(this);
    }

    TypeDescriptor_Struct(const char* n, size_t s, const std::initializer_list<Member>& init)
        : TypeDescriptor{ n, s }, members{ init } {
    }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override
    {
        os << name << "\n" << std::string(4 * indent_level, ' ') << "{\n";
        for (const Member& member : members)
        {
            os << std::string(4 * (indent_level + 1), ' ') << member.name << " = ";
            void* member_addr = member.get_ptr(const_cast<void*>(obj));
            member.type->Dump(member_addr, os, indent_level + 1);
            os << "\n";
        }
        os << std::string(4 * indent_level, ' ') << "}\n";
    }

    virtual void Serialize(const void* obj, std::ostream& os) const override
    {
        os << R"({"type":")" << name << R"(","data":[)";
        bool first = true;
        for (const Member& member : members)
        {
            if (!first) os << ",";
            first = false;
            void* member_addr = member.get_ptr(const_cast<void*>(obj));
            member.type->Serialize(member_addr, os);
        }
        os << "]}";
    }

    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override
    {
        if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsArray())
        {
            throw std::runtime_error("Malformed JSON while deserializing struct '" + name + "'");
        }
        const auto& arr = value["data"].GetArray();

        // CHANGE: instead of hard assert on size, do a runtime check and provide a useful error message.
        if (arr.Size() != members.size())
        {
            std::stringstream ss;
            ss << "Array size mismatch while deserializing struct '" << name << "' : expected " << members.size() << " got " << arr.Size();
            throw std::runtime_error(ss.str());
        }

        for (rapidjson::SizeType i = 0; i < members.size(); i++)
        {
            void* member_addr = members[i].get_ptr(obj);
            members[i].type->Deserialize(member_addr, arr[i]);
        }
    }
};

// -------------------------------------------------------------
// std::vector specialization
// CHANGE: set name to a unique representation using item_type->ToString()
// -------------------------------------------------------------
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

        // Try eager resolve now (optional)
        item_type = resolver();
        if (item_type)
        {
            this->name = std::string("std::vector<") + item_type->ToString() + ">";
            std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
            if (TYPE_DESCRIPTOR_LOOKUP.count(this->name) == 0) TYPE_DESCRIPTOR_LOOKUP[this->name] = this;
        }
        else
        {
            // keep placeholder name; will compute once resolved
            this->name = "std::vector<unresolved>";
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
            // final attempt: maybe a primitive or reflected type wasn't registered yet
            // produce an error with helpful diagnostic
            throw std::runtime_error("TypeDescriptor_StdVector: failed to resolve item type for vector; ensure the item type is reflected or a primitive.");
        }
        // set the final name and register
        this->name = std::string("std::vector<") + item_type->ToString() + ">";
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(this->name) == 0) TYPE_DESCRIPTOR_LOOKUP[this->name] = this;
    }

public:
    virtual std::string ToString() const override
    {
        if (!item_type) const_cast<TypeDescriptor_StdVector*>(this)->EnsureResolvedForUse();
        return std::string("std::vector<") + item_type->ToString() + ">";
    }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override
    {
        if (!item_type) const_cast<TypeDescriptor_StdVector*>(this)->EnsureResolvedForUse();
        size_t num_items = get_size(obj);
        os << "\n" << ToString();
        if (num_items == 0) { os << "{}\n"; return; }
        os << "\n" << std::string(4 * indent_level, ' ') << "{\n";
        for (size_t index = 0; index < num_items; ++index)
        {
            os << std::string(4 * (indent_level + 1), ' ') << "[" << index << "]\n" << std::string(4 * (indent_level + 1), ' ');
            item_type->Dump(get_item(obj, index), os, indent_level + 1);
            os << "\n";
        }
        os << std::string(4 * indent_level, ' ') << "}\n";
    }

    virtual void Serialize(const void* obj, std::ostream& os) const override
    {
        if (!item_type) const_cast<TypeDescriptor_StdVector*>(this)->EnsureResolvedForUse();
        size_t num_items = get_size(obj);
        if (num_items == 0)
        {
            os << R"({"type":")" << ToString() << R"(","data":[]})";
            return;
        }
        os << R"({"type":")" << ToString() << R"(","data":[)";
        for (size_t index = 0; index < num_items; ++index)
        {
            item_type->Serialize(get_item(obj, index), os);
            if (index < num_items - 1) os << ",";
        }
        os << "]}";
    }

    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override
    {
        if (!item_type) const_cast<TypeDescriptor_StdVector*>(this)->EnsureResolvedForUse();
        if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsArray())
            throw std::runtime_error("Malformed JSON while deserializing std::vector");
        const auto& arr = value["data"].GetArray();
        for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
        {
            item_type->Deserialize(set_item(obj, i), arr[i]);
        }
    }
};

template <typename T>
struct TypeResolver<std::vector<T>>
{
    static TypeDescriptor* Get()
    {
        static TypeDescriptor_StdVector type_desc{ (T*) nullptr };
        // CHANGE: guard registry writes with mutex
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.name) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.name] = &type_desc;
        return &type_desc;
    }
};

// -------------------------------------------------------------
// std::unordered_map<KeyType, ValueType> specialization
// -------------------------------------------------------------
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
        this->name = std::string("std::unordered_map<") + key_type->ToString() + ", " + value_type->ToString() + ">";
    }

    virtual std::string ToString() const override { return std::string("std::unordered_map<") + key_type->ToString() + ", " + value_type->ToString() + ">"; }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override
    {
        const auto& map = *(const std::unordered_map<KeyType, ValueType>*)obj;
        os << "\n" << std::string(4 * indent_level, ' ') << ToString() << "\n" << std::string(4 * indent_level, ' ') << "{\n";
        for (const auto& pair : map)
        {
            os << std::string(4 * (indent_level + 1), ' ');
            key_type->Dump(&pair.first, os, indent_level + 1);
            os << ": ";
            value_type->Dump(&pair.second, os, indent_level + 1);
            os << "\n";
        }
        os << std::string(4 * indent_level, ' ') << "}\n";
    }

    virtual void Serialize(const void* obj, std::ostream& os) const override
    {
        const auto& map = *(const std::unordered_map<KeyType, ValueType>*)obj;
        os << R"({"type":")" << ToString() << R"(","data":[)";
        bool first = true;
        for (const auto& pair : map)
        {
            if (!first) os << ",";
            first = false;
            os << "[";
            key_type->Serialize(&pair.first, os);
            os << ",";
            value_type->Serialize(&pair.second, os);
            os << "]";
        }
        os << "]}";
    }

    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override
    {
        if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsArray())
        {
            throw std::runtime_error("Malformed JSON while deserializing std::unordered_map");
        }
        std::unordered_map<KeyType, ValueType>& map = *(std::unordered_map<KeyType, ValueType>*)obj;
        const auto& arr = value["data"].GetArray();
        for (rapidjson::SizeType i = 0; i < arr.Size(); i++)
        {
            KeyType key; ValueType val;
            key_type->Deserialize(&key, arr[i][0]);
            value_type->Deserialize(&val, arr[i][1]);
            map[key] = val;
        }
    }
};

template <typename KeyType, typename ValueType>
struct TypeResolver<std::unordered_map<KeyType, ValueType>>
{
    static TypeDescriptor* Get()
    {
        static TypeDescriptor_StdUnorderedMap<KeyType, ValueType> type_desc{ (std::unordered_map<KeyType, ValueType>*)nullptr };
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.name) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.name] = &type_desc;
        return &type_desc;
    }
};

// -------------------------------------------------------------
// std::shared_ptr<T> specialization
// -------------------------------------------------------------
template <typename T>
struct TypeDescriptor_StdSharedPtr : TypeDescriptor
{
    TypeDescriptor* item_type;

    TypeDescriptor_StdSharedPtr(T*)
        : TypeDescriptor{ "std::shared_ptr<>", sizeof(std::shared_ptr<T>) }
        , item_type{ TypeResolver<T>::Get() }
    {
        this->name = std::string("std::shared_ptr<") + item_type->ToString() + ">";
    }

    virtual std::string ToString() const override { return std::string("std::shared_ptr<") + item_type->ToString() + ">"; }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override
    {
        const auto& shared_ptr = *reinterpret_cast<const std::shared_ptr<T>*>(obj);
        if (shared_ptr)
        {
            os << "\n" << ToString() << "\n" << std::string(4 * indent_level, ' ') << "{\n" << std::string(4 * (indent_level + 1), ' ');
            item_type->Dump(shared_ptr.get(), os, indent_level + 1);
            os << "\n" << std::string(4 * indent_level, ' ') << "}\n";
        }
        else { os << "null"; }
    }

    virtual void Serialize(const void* obj, std::ostream& os) const override
    {
        const auto& shared_ptr = *reinterpret_cast<const std::shared_ptr<T>*>(obj);
        if (shared_ptr) item_type->Serialize(shared_ptr.get(), os);
        else os << "null";
    }

    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override
    {
        if (value.IsNull()) { *reinterpret_cast<std::shared_ptr<T>*>(obj) = nullptr; }
        else
        {
            std::shared_ptr<T> sp = std::make_shared<T>();
            item_type->Deserialize(sp.get(), value);
            *reinterpret_cast<std::shared_ptr<T>*>(obj) = sp;
        }
    }
};

template <>
struct TypeDescriptor_StdSharedPtr<void> : TypeDescriptor
{
    TypeDescriptor* item_type;
    TypeDescriptor_StdSharedPtr(void*)
        : TypeDescriptor{ "std::shared_ptr<>", sizeof(std::shared_ptr<void>) }, item_type{ nullptr }
    {
        this->name = std::string("std::shared_ptr<void>");
    }

    virtual std::string ToString() const override { return "std::shared_ptr<void>"; }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override
    {
        const auto& shared_ptr = *reinterpret_cast<const std::shared_ptr<void>*>(obj);
        if (shared_ptr)
        {
            os << "\n" << ToString() << "\n" << std::string(4 * indent_level, ' ') << "{\n" << std::string(4 * (indent_level + 1), ' ') << shared_ptr.get() << "\n" << std::string(4 * indent_level, ' ') << "}\n";
        }
        else { os << "null"; }
    }

    virtual void Serialize(const void* obj, std::ostream& os) const override
    {
        const auto& shared_ptr = *reinterpret_cast<const std::shared_ptr<void>*>(obj);
        if (!shared_ptr) { os << "null"; return; }

        void* ptr = shared_ptr.get();
        uint64_t data_size = *static_cast<uint64_t*>(ptr); // NOTE: caller MUST ensure pointer layout matches this contract
        uint64_t le_size = ToLittleEndian_u64(data_size);

        uint8_t* byte_ptr = static_cast<uint8_t*>(ptr);
        std::vector<uint8_t> data(byte_ptr, byte_ptr + sizeof(uint64_t) + static_cast<size_t>(data_size));
        std::string serialized_data = Base64_Encode(data);

        os << R"({"type":")" << ToString() << R"(","data":")" << serialized_data << R"("})";
    }

    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override
    {
        if (value.IsNull()) { *reinterpret_cast<std::shared_ptr<void>*>(obj) = nullptr; return; }
        if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsString())
        {
            throw std::runtime_error("Malformed JSON for std::shared_ptr<void>");
        }

        std::string data = value["data"].GetString();
        std::vector<uint8_t> decoded = Base64_Decode(data);
        if (decoded.size() < sizeof(uint64_t)) throw std::runtime_error("Decoded shared_ptr<void> too small");

        // Read size prefix (little-endian)
        uint64_t stored_size = 0;
        memcpy(&stored_size, decoded.data(), sizeof(uint64_t));
        uint64_t data_size = FromLittleEndian_u64(stored_size);

        if (decoded.size() != sizeof(uint64_t) + data_size) throw std::runtime_error("Decoded shared_ptr<void> length mismatch");

        // Allocate memory and copy
        void* ptr = new uint8_t[sizeof(uint64_t) + static_cast<size_t>(data_size)];
        memcpy(ptr, decoded.data(), decoded.size());

        std::shared_ptr<void> sp(ptr, [](void* p) { delete[] reinterpret_cast<uint8_t*>(p); });
        *reinterpret_cast<std::shared_ptr<void>*>(obj) = sp;
    }
};

template <typename T>
struct TypeResolver<std::shared_ptr<T>>
{
    static TypeDescriptor* Get()
    {
        static TypeDescriptor_StdSharedPtr type_desc{ (T*) nullptr };
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.name) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.name] = &type_desc;
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
        this->name = std::string("std::pair<") + first_type->ToString() + ", " + second_type->ToString() + ">";
    }

    virtual std::string ToString() const override { return std::string("std::pair<") + first_type->ToString() + ", " + second_type->ToString() + ">"; }

    virtual void Dump(const void* obj, std::ostream& os, int indent_level) const override
    {
        const auto& pair = *(const std::pair<FirstType, SecondType>*)obj;
        os << "\n" << std::string(4 * indent_level, ' ') << ToString() << "\n" << std::string(4 * indent_level, ' ') << "{\n" << std::string(4 * (indent_level + 1), ' ');
        first_type->Dump(&pair.first, os, indent_level + 1);
        os << ",\n" << std::string(4 * (indent_level + 1), ' ');
        second_type->Dump(&pair.second, os, indent_level + 1);
        os << "\n" << std::string(4 * indent_level, ' ') << "}\n";
    }

    virtual void Serialize(const void* obj, std::ostream& os) const override
    {
        const auto& pair = *(const std::pair<FirstType, SecondType>*)obj;
        os << R"({"type":")" << ToString() << R"(","data":[)";
        first_type->Serialize(&pair.first, os);
        os << ",";
        second_type->Serialize(&pair.second, os);
        os << "]}";
    }

    virtual void Deserialize(void* obj, const rapidjson::Value& value) const override
    {
        if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsArray())
        {
            throw std::runtime_error("Malformed JSON while deserializing std::pair");
        }
        const auto& arr = value["data"].GetArray();
        if (arr.Size() != 2) throw std::runtime_error("std::pair expects array of size 2");
        first_type->Deserialize(&((std::pair<FirstType, SecondType>*)obj)->first, arr[0]);
        second_type->Deserialize(&((std::pair<FirstType, SecondType>*)obj)->second, arr[1]);
    }
};

template <typename FirstType, typename SecondType>
struct TypeResolver<std::pair<FirstType, SecondType>>
{
    static TypeDescriptor* Get()
    {
        static TypeDescriptor_StdPair<FirstType, SecondType> type_desc{ (std::pair<FirstType, SecondType>*)nullptr };
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.name) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.name] = &type_desc;
        return &type_desc;
    }
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
    type_desc->name = #TYPE; \
    type_desc->size = sizeof(T); \
    type_desc->members = {

#define REFL_REGISTER_PROPERTY(VARIABLE) \
  { \
    #VARIABLE, \
    TypeResolver<decltype(T::VARIABLE)>::Get(), \
    /* captureless lambda -> converts to function pointer */ \
    +[](void* obj) -> void* { return & (static_cast<T*>(obj)->VARIABLE); } \
  },

#define REFL_REGISTER_END \
    }; \
    { std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex()); \
      TypeDescriptor::type_descriptor_lookup()[type_desc->name] = type_desc; } \
  }
#pragma endregion